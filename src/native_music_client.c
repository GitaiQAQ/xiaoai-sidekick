#define _GNU_SOURCE
#include <sys/prctl.h>
#include "native_music.h"
int main(int argc,char **argv){
 char request[2048],out[1024];size_t n=0;int c,rc;void *lib;
 if(prctl(PR_SET_DUMPABLE,0L,0L,0L,0L))return 2;
 lib=dlopen("libjson-c.so.2",RTLD_NOW|RTLD_LOCAL);if(!lib||lx_music_init(lib))return 2;
 if(argc==2&&!strcmp(argv[1],"--self-test")){
  const char *invalid[]={"{}","[]","{\"op\":\"pause;echo unsafe\"}","{\"op\":\"play\",\"url\":\"file:///etc/passwd\"}","{\"op\":\"play\",\"url\":\"http://user:password@127.0.0.1/a\"}","{\"op\":\"play\",\"url\":\"http://127.0.0.1/a b\"}","{\"op\":\"play\",\"url\":\"http://127.0.0.1/a\\u0000b\"}"};
  for(size_t i=0;i<sizeof(invalid)/sizeof(invalid[0]);i++)if(!lx_music_dispatch(invalid[i],out,sizeof(out)))return 1;
  struct rusage usage;getrusage(RUSAGE_SELF,&usage);printf("music_validation checks=7 pass=1 peak_rss_kib=%ld\n",usage.ru_maxrss);dlclose(lib);return 0;
 }
 while((c=getchar())!=EOF){if(n>=sizeof(request)-1)return 2;request[n++]=(char)c;}request[n]=0;
 if(memchr(request,0,n))return 2;
 rc=lx_music_dispatch(request,out,sizeof(out));puts(out);dlclose(lib);return rc;
}
