/* Isolated probe that makes the factory Rice/Paddy engine parse our own BINN
   containers instead of the built-in ones. Never touches the factory process,
   capture, callbacks or persistent storage.

   WakeupInit is called first only to obtain the engine wrapper and its two
   level states; the wrapper ignores the path argument and copies the built-in
   models. Each state's +0x68 buffer pointer is then replaced with our blob and
   RiceEngineInit is called again, which is the non-empty branch of
   RiceLoadModelBin (0x589d8). Task creation and processing use the documented
   Wakeup wrappers.

   Ownership (factory 1.94.13 static evidence):
   - RiceLoadModelBin's empty branch mallocs the built-in container and stores
     it at state+0x68 (0x5a118 / 0x5a144).
   - RiceEngineEnd frees state+0x68 when non-null (0x57680).
   - WakeupDestroy calls PaddyEngineImpl::Destroy (frees both level states and
     their current +0x68 buffers) and then deletes the wrapper (0x29444).
   Therefore a blob installed at +0x68 is owned by the engine after a successful
   RiceEngineInit; freeing it in this probe would double-free. A failed init
   restores the old built-in pointer and the probe frees its own blob. The
   replaced built-in container is orphaned until after WakeupDestroy, then the
   probe frees it.

   The result buffer is 1024 bytes; the factory wrapper copies an output struct
   ending at +0x240 (VSTR d16/d17 at +0x230/+0x238). The buffer is explicitly
   8-byte aligned because the wrapper uses 64-bit VFP stores; doubles are read
   with memcpy to avoid alignment and strict-aliasing assumptions.

   Usage: lx06-wake-model-probe [--model1 f] [--model2 f] [--pcm16le f]
   --pcm16le is raw 16 kHz mono S16LE, 160 samples (10 ms) per process call. */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/prctl.h>
#include <sys/resource.h>
static long milliseconds(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec*1000L+t.tv_nsec/1000000L;}
static unsigned char *load(const char *path,long *size){
 FILE *f=fopen(path,"rb");if(!f)return NULL;
 long n=0;if(fseek(f,0,SEEK_END)||(n=ftell(f))<=0||fseek(f,0,SEEK_SET)){fclose(f);return NULL;}
 unsigned char *p=malloc((size_t)n);if(!p){fclose(f);return NULL;}
 if(fread(p,1,(size_t)n,f)!=(size_t)n){free(p);fclose(f);return NULL;}
 fclose(f);*size=n;return p;
}
int main(int argc,char **argv) {
 setbuf(stdout,NULL);puts("probe_start");
 const char *m1=NULL,*m2=NULL,*pcm=NULL;
 for(int i=1;i+1<argc;i+=2){
  if(!strcmp(argv[i],"--model1"))m1=argv[i+1];
  else if(!strcmp(argv[i],"--model2"))m2=argv[i+1];
  else if(!strcmp(argv[i],"--pcm16le"))pcm=argv[i+1];
  else return 2;
 }
 if(argc%2==0)return 2;
 if(prctl(PR_SET_DUMPABLE,0L,0L,0L,0L))return 2;
 if(!dlopen("libpthread.so.0",RTLD_NOW|RTLD_GLOBAL)){printf("probe_pthread_failed=%s\n",dlerror());return 2;}
 if(!dlopen("libstdc++.so.6",RTLD_NOW|RTLD_GLOBAL)){printf("probe_cpp_failed=%s\n",dlerror());return 2;}
 if(!dlopen("libasound.so.2",RTLD_NOW|RTLD_GLOBAL)){printf("probe_alsa_failed=%s\n",dlerror());return 2;}
 void *lib=dlopen("libxaudio_engine.so",RTLD_NOW|RTLD_LOCAL);
 if(!lib){printf("probe_dlopen_failed=%s\n",dlerror());return 2;}
 void *(*winit)(const char *)=(void *(*)(const char *))dlsym(lib,"WakeupInit");
 void (*wdestroy)(void *)=(void (*)(void *))dlsym(lib,"WakeupDestroy");
 void *(*ctask)(void *)=(void *(*)(void *))dlsym(lib,"WakeupCreateTask");
 void (*process)(void *,void *,void *,const short *,unsigned)=
   (void (*)(void *,void *,void *,const short *,unsigned))dlsym(lib,"WakeupProcess");
 int (*reinit)(const char *,void *)=(int (*)(const char *,void *))dlsym(lib,"RiceEngineInit");
 if(!winit||!wdestroy||!ctask||!process||!reinit){puts("probe_symbols_missing");return 2;}
 long start=milliseconds();
 puts("probe_libraries_loaded");
 void *engine=winit("/tmp/lx06-probe-deliberately-absent-model.bin");
 if(!engine){puts("probe_init_failed");return 2;}
 uint32_t *impl=*(uint32_t **)engine;
 uint32_t *level1=(uint32_t *)(uintptr_t)impl[0];
 uint32_t *level2=(uint32_t *)(uintptr_t)impl[1];
 if(level1[0x3b8/4]!=1||level2[0x3b8/4]!=2){puts("probe_layout_mismatch");wdestroy(engine);dlclose(lib);return 2;}
 printf("probe_init=pass builtin_l1=%p builtin_l2=%p elapsed_ms=%ld\n",
   (void *)(uintptr_t)level1[0x68/4],(void *)(uintptr_t)level2[0x68/4],milliseconds()-start);
 struct { uint32_t *state; unsigned char *blob; long size; uint32_t old; const char *path; int owned; int orphaned; } lv[2]={
   {level1,NULL,0,level1[0x68/4],m1,0,0},{level2,NULL,0,level2[0x68/4],m2,0,0}};
 int failed=0;
 for(int i=0;i<2;i++){
  if(!lv[i].path)continue;
  lv[i].blob=load(lv[i].path,&lv[i].size);
  if(!lv[i].blob){printf("probe_model%d_unreadable=%s\n",i+1,lv[i].path);failed=1;continue;}
  lv[i].state[0x68/4]=(uint32_t)(uintptr_t)lv[i].blob;
  start=milliseconds();int r=reinit(NULL,lv[i].state);
  if(r==0){lv[i].owned=1;lv[i].orphaned=1;}
  else {lv[i].state[0x68/4]=lv[i].old;free(lv[i].blob);lv[i].blob=NULL;}
  printf("probe_model%d path=%s bytes=%ld parse_rc=%d ownership=%s elapsed_ms=%ld\n",
    i+1,lv[i].path,lv[i].size,r,lv[i].owned?"engine":"probe",milliseconds()-start);
  if(r!=0)failed=1;
  float threshold=0;memcpy(&threshold,(char *)lv[i].state+0x3e8,4);
  printf("probe_control level=%d score_threshold=%.4f min_loop=%u max_loop=%u wait_frames=%u\n",i+1,(double)threshold,lv[i].state[0x73c/4],lv[i].state[0x740/4],lv[i].state[0x364/4]);
 }
 if(failed){wdestroy(engine);dlclose(lib);return 3;}
 void *task=ctask(engine);
 _Alignas(8) unsigned result[256];
 _Static_assert(sizeof(result)>=0x240,"result buffer smaller than factory output");
 unsigned flags=0,confirmed=0,frames=0,prev=0;
 short buf[160];FILE *f=pcm?(!strcmp(pcm,"-")?stdin:fopen(pcm,"rb")):NULL;
 if(pcm&&!f){puts("probe_pcm_unreadable");wdestroy(engine);dlclose(lib);return 2;}
 memset(buf,0,sizeof(buf));start=milliseconds();
 while(frames<200000){
  if(f){
   size_t got=fread(buf,sizeof(short),160,f);
   if(!got)break;
   if(got<160)memset(buf+got,0,(160-got)*sizeof(short));
  }else if(frames>=1000)break;
  frames++;memset(result,0,sizeof(result));
  process(result,engine,task,buf,sizeof(buf));
  if(result[0]){
   flags++;if(result[0]==1)confirmed++;
   if(result[0]!=prev){
    double score0=0.0,score1=0.0;
    memcpy(&score0,(char *)result+0x230,sizeof(score0));
    memcpy(&score1,(char *)result+0x238,sizeof(score1));
    printf("probe_event frame=%u t=%.2fs flag=%u a1=%u a2=%u a3=%u score0=%.4f score1=%.4f\n",
      frames,(double)frames/100.0,result[0],result[1],result[2],result[3],score0,score1);
   }
  }
  prev=result[0];
 }
 if(f&&f!=stdin)fclose(f);
 struct rusage usage;getrusage(RUSAGE_SELF,&usage);
 printf("probe_input=%s frames=%u input_ms=%.0f elapsed_ms=%ld flags=%u confirmed=%u maxrss_kb=%ld\n",
   pcm?"offline_fixture":"silence",frames,(double)frames*10.0,milliseconds()-start,flags,confirmed,usage.ru_maxrss);
 wdestroy(engine);dlclose(lib);
 for(int i=0;i<2;i++)if(lv[i].orphaned&&lv[i].old)free((void *)(uintptr_t)lv[i].old);
 printf("probe_destroy=pass model_buffers_freed_by=engine orphaned_builtin_freed=%d audio_devices=not_opened_by_probe\n",
   (lv[0].orphaned?1:0)+(lv[1].orphaned?1:0));
 return 0;
}
