/* Microphone capture for the voice client.

   Preferred path: the dsnoop PCM "Capture" carries 8 interleaved S32/48 kHz
   channels. ch0 is microphone 1 and ch6 is the playback reference, recorded in
   the same stream, so they are sample-aligned. Both are decimated 3:1 to
   16 kHz, run through Speex echo cancellation and the Speex preprocessor in
   128-sample frames, and the result feeds both the wake detector and the
   cloud uplink.

   Fallback path: the factory "mico_record" PCM at 16 kHz mono, which still
   contains the speaker echo. Selected when the 8-channel PCM, libspeexdsp or
   the AEC state is unavailable, for local diagnostics and wake detection. Cloud sessions reject this path.

   Nothing here writes to the vendor processes; the 8-channel PCM is a dsnoop,
   so the factory wake stack keeps reading it. */
#ifndef LX06_CAPTURE_H
#define LX06_CAPTURE_H
#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define LX_CAP_RATE 16000
#define LX_CAP_AEC_FRAME 128
#define LX_CAP_DECIMATE 3
#define LX_CAP_BLOCK 480 /* one 10 ms period at 48 kHz */
#define LX_CAP_RING 4096
#define LX_CAP_MIC_CHANNEL 0
#define LX_CAP_MIC_COUNT 6
#define LX_CAP_REF_CHANNEL 6

/* The device's ALSA entry points, passed in because the client loads them
   through dlopen. */
struct lx_capture_alsa {
 int (*open)(void **,const char *,int,int);
 int (*set_params)(void *,int,int,unsigned,unsigned,int,unsigned);
 long (*readi)(void *,void *,unsigned long);
 int (*recover)(void *,int,int);
 int (*close)(void *);
};

struct lx_capture {
 struct lx_capture_alsa ops;
 void *lib;      /* libspeexdsp, NULL in plain mode */
 void *pcm;      /* 8-channel 48 kHz handle, NULL in plain mode */
 void *plain;    /* 16 kHz mono handle, NULL in AEC mode */
 void *echo;     /* SpeexEchoState */
 void *pp;       /* SpeexPreprocessState */
 int (*ecancel)(void *,const short *,const short *,short *);
 int (*prun)(void *,short *);
 int aec;
 int failed;
 long blocks;
 short mask_fill[LX_CAP_AEC_FRAME],ref_fill[LX_CAP_AEC_FRAME];
 int fill,decim;
 long mic_acc,ref_acc;
 short ring[LX_CAP_RING];
 /* Reference samples aligned with the returned AEC samples, for local
    playback-source verification. Never written to a file. */
 short reference_ring[LX_CAP_RING],reference_read[LX_CAP_RING];
 int tail,used;
};

static int lx_capture_init(struct lx_capture *c,const struct lx_capture_alsa *ops,
                           void *pcm8,void *plain16) {
 memset(c,0,sizeof(*c));
 c->ops=*ops;c->pcm=pcm8;c->plain=plain16;
 if(!pcm8)return 0;
 c->lib=dlopen("libspeexdsp.so.1",RTLD_NOW|RTLD_LOCAL);
 if(!c->lib){puts("capture_speex_dlopen_failed");return -1;}
#define LX_CAP_SPEEX_FAIL(msg) do{puts(msg);dlclose(c->lib);c->lib=NULL;c->pcm=NULL;return -1;}while(0)
 void *(*einit)(int,int)=(void *(*)(int,int))dlsym(c->lib,"speex_echo_state_init");
 int (*ectl)(void *,int,void *)=(int (*)(void *,int,void *))dlsym(c->lib,"speex_echo_ctl");
 if(!ectl)ectl=(int (*)(void *,int,void *))dlsym(c->lib,"speex_echo_state_ctl");
 c->ecancel=(int (*)(void *,const short *,const short *,short *))dlsym(c->lib,"speex_echo_cancellation");
 void *(*pinit)(int,int)=(void *(*)(int,int))dlsym(c->lib,"speex_preprocess_state_init");
 int (*pctl)(void *,int,void *)=(int (*)(void *,int,void *))dlsym(c->lib,"speex_preprocess_ctl");
 c->prun=(int (*)(void *,short *))dlsym(c->lib,"speex_preprocess_run");
 if(!einit||!c->ecancel||!pinit||!pctl||!c->prun){
  printf("capture_speex_symbols_missing init=%d cancel=%d pp_init=%d pp_ctl=%d run=%d\n",
   !!einit,!!c->ecancel,!!pinit,!!pctl,!!c->prun);
  LX_CAP_SPEEX_FAIL("capture_speex_unusable");
 }
 c->echo=einit(LX_CAP_AEC_FRAME,4096);
 if(!c->echo)LX_CAP_SPEEX_FAIL("capture_speex_echo_init_failed");
 int rate=LX_CAP_RATE;
 /* SPEEX_ECHO_SET_SAMPLING_RATE (24). This build exports speex_echo_ctl. */
 if(!ectl)puts("capture_speex_rate_ctl_missing");
 else if(ectl(c->echo,24,&rate))puts("capture_speex_rate_ctl_failed");
 else puts("capture_speex_rate=16000");
 c->pp=pinit(LX_CAP_AEC_FRAME,LX_CAP_RATE);
 if(!c->pp||pctl(c->pp,24,c->echo))LX_CAP_SPEEX_FAIL("capture_speex_pp_init_failed");
 c->aec=1;
 return 0;
}

static void lx_capture_close(struct lx_capture *c) {
 if(c->pcm&&c->ops.close)c->ops.close(c->pcm);
 if(c->plain&&c->ops.close)c->ops.close(c->plain);
 c->pcm=NULL;c->plain=NULL;
 /* The Speex state is deliberately not destroyed: the library keeps its own
    pools and the process exits right after this. */
 if(c->lib){dlclose(c->lib);c->lib=NULL;}
 c->aec=0;
}

/* Decimate one 48 kHz frame into the 16 kHz Speex accumulator. Each completed
   128-sample frame is echo-cancelled and appended to the output ring, so any
   ALSA read size keeps the sample rate exact. */
static void lx_capture_frame(struct lx_capture *c,const int32_t *frame) {
 int ch;
 /* Average all six microphones. A single microphone loses too much far-field
    speech under music; the sum also feeds one echo filter, which measured
    better than ch0 alone in --mic-compare. */
 for(ch=0;ch<LX_CAP_MIC_COUNT;ch++)c->mic_acc+=frame[ch];
 c->ref_acc+=frame[LX_CAP_REF_CHANNEL];
 if(++c->decim<LX_CAP_DECIMATE)return;
 c->mask_fill[c->fill]=(short)((c->mic_acc/(LX_CAP_DECIMATE*LX_CAP_MIC_COUNT))>>16);
 c->ref_fill[c->fill]=(short)((c->ref_acc/LX_CAP_DECIMATE)>>16);
 c->mic_acc=0;c->ref_acc=0;c->decim=0;
 if(++c->fill<LX_CAP_AEC_FRAME)return;
 c->fill=0;
 {
  short out[LX_CAP_AEC_FRAME];
  int i;
  c->ecancel(c->echo,c->mask_fill,c->ref_fill,out);
  c->prun(c->pp,out);
  for(i=0;i<LX_CAP_AEC_FRAME;i++) {
   if(c->used>=LX_CAP_RING)return;
   c->ring[(c->tail+c->used)%LX_CAP_RING]=out[i];
   c->reference_ring[(c->tail+c->used)%LX_CAP_RING]=c->ref_fill[i];
   c->used++;
  }
  c->blocks++;
 }
}

static int lx_capture_fill(struct lx_capture *c) {
 int32_t raw[LX_CAP_BLOCK*8];
 long got=0;
 while(got<LX_CAP_BLOCK) {
  long r=c->ops.readi(c->pcm,raw+got*8,(unsigned long)(LX_CAP_BLOCK-got));
  if(r<0){if(c->ops.recover(c->pcm,(int)r,1)<0)return -1;continue;}
  if(!r)continue;
  got+=r;
 }
 for(long f=0;f<got;f++)lx_capture_frame(c,raw+f*8);
 return 0;
}

/* Returns the requested number of 16 kHz samples, or <0 on a fatal device error. */
static long lx_capture_read(struct lx_capture *c,short *pcm,int frames) {
 int i;
 if(!c->aec){memset(c->reference_read,0,sizeof(c->reference_read));return c->ops.readi(c->plain,pcm,(unsigned long)frames);}
 if(c->failed)return -1;
 if(frames<=0||frames>LX_CAP_RING-LX_CAP_BLOCK){puts("capture_bad_read_size");return -1;}
 while(c->used<frames) {
  if(lx_capture_fill(c)<0){c->failed=1;puts("capture_aec_read_failed");return -1;}
 }
 for(i=0;i<frames;i++) {
  pcm[i]=c->ring[c->tail];
  c->reference_read[i]=c->reference_ring[c->tail];
  c->tail=(c->tail+1)%LX_CAP_RING;
  c->used--;
 }
 return frames;
}

static int lx_capture_recover(struct lx_capture *c,long error) {
 if(!c->aec)return c->ops.recover(c->plain,(int)error,1);
 c->used=0;c->tail=0;
 c->fill=0;c->decim=0;c->mic_acc=0;c->ref_acc=0;
 if(c->ops.recover(c->pcm,(int)error,1)<0){c->failed=1;return -1;}
 return 0;
}

/* Opens the 8-channel capture when requested, otherwise the factory mono PCM.
   Returns 0 on success and leaves the chosen mode in c->aec. */
static int lx_capture_open(struct lx_capture *c,const struct lx_capture_alsa *ops,
                           int want_aec,unsigned rate_in) {
 void *pcm=NULL,*plain=NULL;
 if(want_aec) {
  if(!ops->open(&pcm,"Capture",1,0)) {
   if(ops->set_params(pcm,10,3,8,48000,0,200000)) {
    puts("capture_aec_params_failed");ops->close(pcm);pcm=NULL;
   } else if(lx_capture_init(c,ops,pcm,NULL)) {
    ops->close(pcm);pcm=NULL;
   }
  } else {
   puts("capture_aec_open_failed");
  }
  if(pcm) {
   printf("capture=aec pcm=Capture channels=8 mic=mix6 rate=48000 frame=%d\n",LX_CAP_AEC_FRAME);
   fflush(stdout);
   return 0;
  }
  puts("capture_aec_unavailable fallback=mico_record");
 }
 if(ops->open(&plain,"mico_record",1,0))return -1;
 if(ops->set_params(plain,2,3,1,rate_in,1,100000)){ops->close(plain);return -1;}
 if(lx_capture_init(c,ops,NULL,plain)){ops->close(plain);return -1;}
 puts("capture=plain pcm=mico_record");
 return 0;
}
/* Plain capture remains available only for local diagnostics and wake detection.
   Every cloud upload checks this predicate, including buffered preroll. */
static int lx_capture_cloud_ready(const struct lx_capture *c) {
 return c&&c->aec&&!c->failed&&c->echo&&c->pp&&c->ecancel&&c->prun;
}
#endif
