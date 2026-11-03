/* Offline replay of the device capture path (src/native_capture.h) on a host.

   Feeds a raw 8-channel 48 kHz S32_LE recording (tools/lx06-record.py) through
   the same decimation, 6-mic mix, Speex AEC and preprocessor the voice client
   uses, and writes 16 kHz mono s16 raw to the output file (stdout carries the
   capture_* status lines from the header).

   Build (macOS, speexdsp from Homebrew):
     clang -O2 -I src -o build/host-aec tools/lx06-aec-offline.c
     mkdir -p build/speex && ln -sf /opt/homebrew/lib/libspeexdsp.1.dylib build/speex/libspeexdsp.so.1
   Run:
     DYLD_LIBRARY_PATH=build/speex build/host-aec in.raw out.s16

   The device library may be a different speexdsp build (fixed vs float), so
   output approximates rather than reproduces the on-device signal. */
#include <stdlib.h>
#include "native_capture.h"

static FILE *g_in,*g_out;

static long file_readi(void *pcm,void *buf,unsigned long frames) {
 (void)pcm;
 size_t got=fread(buf,32,frames,g_in);
 if(!got){fclose(g_out);exit(0);}
 return (long)got;
}
static int file_recover(void *pcm,int err,int silent){(void)pcm;(void)err;(void)silent;return -1;}
static int file_close(void *pcm){(void)pcm;return 0;}

int main(int argc,char **argv) {
 struct lx_capture_alsa ops={0};
 static struct lx_capture c;
 short out[1600];
 if(argc!=3){fputs("usage: host-aec <8ch48k.raw> <out.s16>\n",stderr);return 2;}
 if(!(g_in=fopen(argv[1],"rb"))){perror(argv[1]);return 1;}
 if(!(g_out=fopen(argv[2],"wb"))){perror(argv[2]);return 1;}
 ops.readi=file_readi;ops.recover=file_recover;ops.close=file_close;
 if(lx_capture_init(&c,&ops,(void *)1,NULL)||!c.aec){fputs("aec init failed\n",stderr);return 1;}
 for(;;) {
  if(lx_capture_read(&c,out,1600)!=1600)return 1;
  fwrite(out,2,1600,g_out);
 }
}
