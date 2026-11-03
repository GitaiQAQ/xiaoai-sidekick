/* LX06 1.94.13 two-level factory decoder with private BINN models.
 * Default production paths may be overridden by environment. No DTW fallback.
 * A process initializes once; standby windows reuse the engine. */
#ifndef LX_NATIVE_FACTORY_KWS_H
#define LX_NATIVE_FACTORY_KWS_H
struct lx_factory_kws {
 void *lib,*engine,*task;
 void (*destroy)(void *);
 void (*process)(void *,void *,void *,const short *,unsigned);
 void *old[2];
 short pending[160];unsigned pending_n;
};
static struct lx_factory_kws lx_factory;
static unsigned char *lx_factory_blob(const char *path) {
 FILE *f=path?fopen(path,"rb"):NULL;if(!f)return NULL;
 if(fseek(f,0,SEEK_END)){fclose(f);return NULL;}long n=ftell(f);
 if(n<16||n>2097152||fseek(f,0,SEEK_SET)){fclose(f);return NULL;}
 unsigned char *b=malloc((size_t)n);
 if(!b){fclose(f);return NULL;}
 if(fread(b,1,(size_t)n,f)!=(size_t)n||b[0]!=0xe2){free(b);b=NULL;}
 fclose(f);return b;
}
static void lx_factory_close(void) {
 if(lx_factory.engine&&lx_factory.destroy)lx_factory.destroy(lx_factory.engine);
 for(int i=0;i<2;i++)free(lx_factory.old[i]);
 if(lx_factory.lib)dlclose(lx_factory.lib);
 memset(&lx_factory,0,sizeof(lx_factory));
}
static int lx_factory_init(void) {
 if(lx_factory.engine)return 0;
 const char *paths[2]={getenv("LX06_WAKE_MODEL1"),getenv("LX06_WAKE_MODEL2")};
 if(!paths[0])paths[0]="/data/lx06-agent/models/d96-v5.binn";
 if(!paths[1])paths[1]="/data/lx06-agent/models/l2-d160.binn";
 unsigned char *blobs[2]={lx_factory_blob(paths[0]),lx_factory_blob(paths[1])};
 if(!blobs[0]||!blobs[1]){free(blobs[0]);free(blobs[1]);puts("factory_kws_models_unreadable");return -1;}
 if(!dlopen("libpthread.so.0",RTLD_NOW|RTLD_GLOBAL)||
    !dlopen("libstdc++.so.6",RTLD_NOW|RTLD_GLOBAL)||
    !dlopen("libasound.so.2",RTLD_NOW|RTLD_GLOBAL))goto fail;
 lx_factory.lib=dlopen("libxaudio_engine.so",RTLD_NOW|RTLD_LOCAL);
 if(!lx_factory.lib)goto fail;
 void *(*init)(const char *)=(void *(*)(const char *))dlsym(lx_factory.lib,"WakeupInit");
 void *(*task)(void *)=(void *(*)(void *))dlsym(lx_factory.lib,"WakeupCreateTask");
 int (*reinit)(const char *,void *)=(int (*)(const char *,void *))dlsym(lx_factory.lib,"RiceEngineInit");
 lx_factory.destroy=(void (*)(void *))dlsym(lx_factory.lib,"WakeupDestroy");
 lx_factory.process=(void (*)(void *,void *,void *,const short *,unsigned))dlsym(lx_factory.lib,"WakeupProcess");
 if(!init||!task||!reinit||!lx_factory.destroy||!lx_factory.process)goto fail;
 lx_factory.engine=init("/tmp/lx06-unused-model-path");if(!lx_factory.engine)goto fail;
 uint32_t *impl=*(uint32_t **)lx_factory.engine;
 uint32_t *states[2]={(uint32_t *)(uintptr_t)impl[0],(uint32_t *)(uintptr_t)impl[1]};
 if(states[0][0x3b8/4]!=1||states[1][0x3b8/4]!=2)goto fail;
 for(int i=0;i<2;i++){
  uint32_t old=states[i][0x68/4];states[i][0x68/4]=(uint32_t)(uintptr_t)blobs[i];
  int rc=reinit(NULL,states[i]);
  if(rc){states[i][0x68/4]=old;goto fail;}
  lx_factory.old[i]=(void *)(uintptr_t)old;blobs[i]=NULL;
 }
 lx_factory.task=task(lx_factory.engine);
 puts("factory_kws_ready levels=2 trigger_flag=1");fflush(stdout);return 0;
fail:
 free(blobs[0]);free(blobs[1]);lx_factory_close();puts("factory_kws_init_failed");return -1;
}
static int lx_factory_push(const short *pcm,int n) {
 int confirmed=0;
 for(int i=0;i<n;i++){
  lx_factory.pending[lx_factory.pending_n++]=pcm[i];
  if(lx_factory.pending_n==160){
   _Alignas(8) uint32_t output[256]={0};
   lx_factory.process(output,lx_factory.engine,lx_factory.task,lx_factory.pending,320);
   lx_factory.pending_n=0;
   if(output[0]){printf("factory_kws_event flag=%u start=%u end=%u\n",output[0],output[1],output[2]);fflush(stdout);}
   if(output[0]==1)confirmed=1;
  }
 }
 return confirmed;
}
#endif
