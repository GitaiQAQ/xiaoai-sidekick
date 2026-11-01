/* Isolated native model probe: init/destroy, optional synthetic silence or
   an explicit offline S16LE fixture. No init_system, capture or callbacks. */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/prctl.h>
#include <sys/resource.h>
static long milliseconds(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec*1000L+t.tv_nsec/1000000L;}
int main(int argc,char **argv) {
 if(argc!=1&&!(argc==2&&!strcmp(argv[1],"--silence"))&&
    !(argc==3&&!strcmp(argv[1],"--pcm16le")))return 2;
 if(prctl(PR_SET_DUMPABLE,0L,0L,0L,0L))return 2;
 if(!dlopen("libpthread.so.0",RTLD_NOW|RTLD_GLOBAL)){
  printf("engine_probe_pthread_failed=%s\n",dlerror());return 2;
 }
 /* As in the existing AFE probe: vendor engine omits libstdc++ in NEEDED. */
 if(!dlopen("libstdc++.so.6",RTLD_NOW|RTLD_GLOBAL)){
  printf("engine_probe_cpp_failed=%s\n",dlerror());return 2;
 }
 /* Factory ALSA also owns the two WebRTC resampler imports. Loading the
    library is distinct from opening a PCM or mixer. */
 if(!dlopen("libasound.so.2",RTLD_NOW|RTLD_GLOBAL)){
  printf("engine_probe_alsa_failed=%s\n",dlerror());return 2;
 }
 void *lib=dlopen("libxaudio_engine.so",RTLD_NOW|RTLD_LOCAL);
 if(!lib){printf("engine_probe_dlopen_failed=%s\n",dlerror());return 2;}
 void *(*init)(const char *)=(void *(*)(const char *))dlsym(lib,"WakeupInit");
 void (*destroy)(void *)=(void (*)(void *))dlsym(lib,"WakeupDestroy");
 if(!init||!destroy){puts("engine_probe_symbols_missing");return 2;}
 long start=milliseconds();
 void *engine=init("/tmp/lx06-probe-deliberately-absent-model.bin");
 long elapsed=milliseconds()-start;
 if(!engine){puts("engine_probe_init_failed");return 2;}
 uint32_t *impl=*(uint32_t **)engine;
 uint32_t *level1=(uint32_t *)(uintptr_t)impl[0];
 uint32_t *level2=(uint32_t *)(uintptr_t)impl[1];
 if(level1[0x3b8/4]!=1||level2[0x3b8/4]!=2){puts("engine_probe_layout_mismatch");return 2;}
 struct rusage usage;getrusage(RUSAGE_SELF,&usage);
 printf("engine_probe_init=pass model_types=1,2 elapsed_ms=%ld maxrss_kb=%ld\n",elapsed,usage.ru_maxrss);
 if(argc>1) {
  /* WakeupProcess has a hidden large-result pointer (r0), then engine (r1),
     task (r2), PCM (r3), byte count (stack). Derived from the factory wrapper;
     output stores end at +0x23c. CreateTask legitimately returns NULL here. */
  void *(*create_task)(void *)=(void *(*)(void *))dlsym(lib,"WakeupCreateTask");
  void (*process)(void *,void *,void *,const short *,unsigned)=
    (void (*)(void *,void *,void *,const short *,unsigned))dlsym(lib,"WakeupProcess");
  if(!create_task||!process){puts("engine_probe_process_symbols_missing");return 2;}
  void *task=create_task(engine);short zero[160]={0};unsigned result[256];
  unsigned flags=0,confirmed=0,frames=0;
  FILE *fixture=argc==3?fopen(argv[2],"rb"):NULL;
  if(argc==3&&!fixture){puts("engine_probe_fixture_unreadable");return 2;}
  int trailing=100;start=milliseconds();
  for(int i=0;i<2000;i++) {
   if(fixture){
    memset(zero,0,sizeof(zero));
    size_t got=fread(zero,sizeof(short),160,fixture);
    if(got<160&&ferror(fixture)){fclose(fixture);return 2;}
    if(!got&&--trailing<0)break;
   } else if(i>=1000)break;
   frames++;
   memset(result,0,sizeof(result));
   process(result,engine,task,zero,sizeof(zero));
   if(result[0]){flags++;printf("engine_probe_result frame=%u flag=%u\n",frames,result[0]);}
   if(result[0]==1)confirmed++;
  }
  if(fixture)fclose(fixture);
  elapsed=milliseconds()-start;getrusage(RUSAGE_SELF,&usage);
  printf("engine_probe_input=%s frames=%u samples_per_frame=160 input_ms=%u elapsed_ms=%ld nonzero_result_flags=%u confirmed_flags=%u maxrss_kb=%ld\n",
    argc==3?"offline_fixture":"silence",frames,frames*10,elapsed,flags,confirmed,usage.ru_maxrss);
  if(argc==2&&flags){destroy(engine);dlclose(lib);return 3;}
 }
 destroy(engine);dlclose(lib);
 puts("engine_probe_destroy=pass audio_devices=not_opened_by_probe");
 return 0;
}
