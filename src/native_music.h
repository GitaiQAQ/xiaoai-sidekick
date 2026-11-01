/* Factory player adapter. No shell; bounded ubus subprocess and numeric state. */
#ifndef LX06_NATIVE_MUSIC_H
#define LX06_NATIVE_MUSIC_H
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <poll.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <fcntl.h>
#include <time.h>
struct lx_music_json {
 void *(*parse)(const char *);
 int (*get)(void *,const char *,void **);
 int (*type)(void *);
 int (*integer)(void *);
 int64_t (*integer64)(void *);
 int (*array_length)(void *);
 void *(*index)(void *,int);
 int (*length)(void *);
 const char *(*string)(void *);
 void (*put)(void *);
 void *(*object)(void);
 void *(*array)(void);
 int (*append)(void *,void *);
 void *(*new_string)(const char *);
 void *(*new_int)(int);
 void (*add)(void *,const char *,void *);
 const char *(*encode)(void *);
};
static struct lx_music_json mj;
#define LX_MUSIC_QUEUE_MAX 600
struct lx_music_track {char audio_id[48],title[256],artist[256];};
static struct lx_music_track lx_music_queue[LX_MUSIC_QUEUE_MAX];
static int lx_music_queue_count;
static char lx_music_playlist[256];
static int lx_music_init(void *lib){
#define M(field,name) *(void **)(&mj.field)=dlsym(lib,name);if(!mj.field)return 1
 M(parse,"json_tokener_parse");M(get,"json_object_object_get_ex");M(type,"json_object_get_type");M(integer,"json_object_get_int");
 M(length,"json_object_get_string_len");M(string,"json_object_get_string");M(put,"json_object_put");M(object,"json_object_new_object");
 M(integer64,"json_object_get_int64");M(array_length,"json_object_array_length");M(index,"json_object_array_get_idx");
 M(array,"json_object_new_array");M(append,"json_object_array_add");
 M(new_string,"json_object_new_string");M(new_int,"json_object_new_int");M(add,"json_object_object_add");M(encode,"json_object_to_json_string");
#undef M
 return 0;
}
static int64_t lx_music_ms(void){struct timespec t;if(clock_gettime(CLOCK_MONOTONIC,&t))return -1;return (int64_t)t.tv_sec*1000+t.tv_nsec/1000000;}
static int lx_music_ubus_object(const char *object,const char *method,const char *args,char *out,size_t cap){
 int fds[2],status=0,failed=0;pid_t pid;size_t used=0;int64_t until=lx_music_ms()+8000;
 if(pipe(fds))return 1;
 pid=fork();
 if(pid==0){
  int null=open("/dev/null",O_RDWR);dup2(fds[1],1);if(null>=0){dup2(null,0);dup2(null,2);}close(fds[0]);close(fds[1]);
  execl("/bin/ubus","ubus","-t","1","call",object,method,args,(char *)NULL);_exit(127);
 }
 close(fds[1]);if(pid<0){close(fds[0]);return 1;}
 while(1){
  struct pollfd p={fds[0],POLLIN,0};
  if(lx_music_ms()>=until){failed=1;break;}
  int got=poll(&p,1,50);if(got<0){if(errno==EINTR)continue;failed=1;break;}if(!got)continue;
  if(!(p.revents&(POLLIN|POLLHUP))){failed=1;break;}
  if(used>=cap-1){failed=1;break;}
  ssize_t n=read(fds[0],out+used,cap-1-used);if(n<0){if(errno==EINTR)continue;failed=1;break;}if(!n)break;used+=(size_t)n;
 }
 close(fds[0]);out[used]=0;
 /* Never wait indefinitely, including the EOF-before-child-exit case. */
 while(waitpid(pid,&status,WNOHANG)==0){if(lx_music_ms()>=until){failed=1;break;}usleep(1000);}
 if(failed){kill(pid,SIGKILL);waitpid(pid,&status,0);return 1;}
 return !WIFEXITED(status)||WEXITSTATUS(status)!=0;
}
static int lx_music_ubus(const char *method,const char *args,char *out,size_t cap){
 return lx_music_ubus_object("mediaplayer",method,args,out,cap);
}
static int lx_music_number(void *obj,const char *key,int *number){void *v=NULL;if(!mj.get(obj,key,&v)||mj.type(v)!=3)return 0;*number=mj.integer(v);return 1;}
/* Only the numbers needed for control leave the factory reply. URL, cookies,
   account information and other metadata are never returned to the model. */
static int lx_music_state(char *out,size_t cap,int *status_out){
 char raw[32768];void *root=NULL,*info=NULL,*nested=NULL,*detail=NULL,*audio=NULL;int status=-1,volume=-1,loop=-1,code=-1,position=-1,duration=-1,current=-1;
 if(lx_music_ubus("player_get_play_status","{}",raw,sizeof(raw)))return 1;
 root=mj.parse(raw);if(!root||mj.type(root)!=4)goto fail;
 if(!lx_music_number(root,"code",&code)||code!=0||!mj.get(root,"info",&info))goto fail;
 if(mj.type(info)==6){nested=mj.parse(mj.string(info));info=nested;}
 if(!info||mj.type(info)!=4||!lx_music_number(info,"status",&status))goto fail;
 lx_music_number(info,"volume",&volume);lx_music_number(info,"loop_type",&loop);
 if(mj.get(info,"play_song_detail",&detail)&&mj.type(detail)==4){lx_music_number(detail,"position",&position);lx_music_number(detail,"duration",&duration);audio=mj.get(detail,"audio_id",&audio)?audio:NULL;}
 const char *audio_id=audio&&mj.type(audio)==6?mj.string(audio):NULL;const char *title="",*artist="";
 if(audio_id)for(int i=0;i<lx_music_queue_count;i++)if(!strcmp(audio_id,lx_music_queue[i].audio_id)){current=i;title=lx_music_queue[i].title;artist=lx_music_queue[i].artist;break;}
 void *result=mj.object();mj.add(result,"ok",mj.new_int(1));mj.add(result,"status",mj.new_int(status));mj.add(result,"volume",mj.new_int(volume));mj.add(result,"loop_type",mj.new_int(loop));mj.add(result,"position_ms",mj.new_int(position));mj.add(result,"duration_ms",mj.new_int(duration));
 if(audio_id)mj.add(result,"audio_id",mj.new_string(audio_id));if(current>=0){mj.add(result,"title",mj.new_string(title));mj.add(result,"artist",mj.new_string(artist));mj.add(result,"playlist",mj.new_string(lx_music_playlist));mj.add(result,"queue_index",mj.new_int(current+1));mj.add(result,"queue_total",mj.new_int(lx_music_queue_count));}
 const char *encoded=mj.encode(result);snprintf(out,cap,"%s",encoded?encoded:"{}");mj.put(result);
 if(status_out)*status_out=status;
 if(nested)mj.put(nested);mj.put(root);return 0;
fail:
 if(nested)mj.put(nested);if(root)mj.put(root);return 1;
}
static int lx_music_reply_ok(const char *raw){void *obj=mj.parse(raw);int code=-1;int ok=obj&&mj.type(obj)==4&&lx_music_number(obj,"code",&code)&&!code;if(obj)mj.put(obj);return ok;}
static int lx_music_control(const char *action,char *out,size_t cap){
 const char *factory;int expected;char raw[32768],args[128],state[512];int status=-1;
 if(!strcmp(action,"pause")){factory="pause";expected=2;}
 else if(!strcmp(action,"resume")){factory="play";expected=1;}
 else if(!strcmp(action,"stop")){factory="stop";expected=3;}
 else return 1;
 snprintf(args,sizeof(args),"{\"media\":\"common\",\"action\":\"%s\"}",factory);
 if(lx_music_ubus("player_play_operation",args,raw,sizeof(raw))||!lx_music_reply_ok(raw))return 1;
 for(int i=0;i<10;i++){if(!lx_music_state(state,sizeof(state),&status)&&(status==expected||(!strcmp(action,"stop")&&status==2))){snprintf(out,cap,"{\"ok\":true,\"status\":%d}",status);return 0;}usleep(100000);}
 snprintf(out,cap,"{\"ok\":false,\"error\":\"state_not_confirmed\",\"status\":%d}",status);return 1;
}
/* User explicitly accepts playback URLs in factory logs. Direct HTTP(S)
   avoids an unnecessary proxy; account cookies are never added to a URL. */
static int lx_music_play_queue(const char **urls,const struct lx_music_track *tracks,int count,const char *playlist,char *out,size_t cap){
 char raw[32768],state[512];int status=-1;
 if(!urls||!tracks||count<1||count>LX_MUSIC_QUEUE_MAX||!playlist||strlen(playlist)>255)return 1;
 for(int i=0;i<count;i++){
  const char *url=urls[i];if(!url||(strncmp(url,"http://",7)&&strncmp(url,"https://",8))||strlen(url)>=160||!tracks[i].audio_id[0])return 1;
  const char *authority=strstr(url,"://")+3,*slash=strchr(authority,'/');if(!*authority||strchr(authority,'@')||(!slash&&strpbrk(authority,"?#")))return 1;
  for(const unsigned char *p=(const unsigned char *)url;*p;p++)if(*p<33||*p>126||*p=='"'||*p=='\\'||*p=='#')return 1;
 }
 /* A single ubus argv is limited; send 300-item chunks. APPEND starts at
    the appended chunk on this firmware, so reset to index 0 afterwards. */
 const int chunk=300;
 for(int start=0;start<count;start+=chunk){  int end=start+chunk;if(end>count)end=count;
  void *message=mj.object(),*payload=mj.object(),*items=mj.array();
  for(int i=start;i<end;i++){
   void *item=mj.object(),*id=mj.object(),*cp=mj.object(),*stream=mj.object();mj.add(cp,"id",mj.new_string("lx06-agent"));mj.add(cp,"name",mj.new_string("lx06-agent"));
   mj.add(id,"audio_id",mj.new_string(tracks[i].audio_id));mj.add(id,"cp",cp);mj.add(stream,"url",mj.new_string(urls[i]));mj.add(stream,"offset_in_ms",mj.new_int(0));mj.add(stream,"duration_in_ms",mj.new_int(0));mj.add(item,"item_id",id);mj.add(item,"stream",stream);mj.append(items,item);
  }
  mj.add(payload,"audio_type",mj.new_string("MUSIC"));mj.add(payload,"play_behavior",mj.new_string(start?"APPEND":"REPLACE_ALL"));mj.add(payload,"audio_items",items);
  /* Factory naming is inverted: "SEQUENCE" loops the list, while
     "LoopSequence" stops after the last item. */
  mj.add(message,"payload",payload);mj.add(message,"play_mode",mj.new_string("SEQUENCE"));
  void *args=mj.object();mj.add(args,"music",mj.new_string(mj.encode(message)));mj.add(args,"media",mj.new_string("common"));
  int bad=lx_music_ubus("player_play_music",mj.encode(args),raw,sizeof(raw));mj.put(args);mj.put(message);
  if(bad||!lx_music_reply_ok(raw))return 1;
 }
 if(count>chunk){char idx[64];snprintf(idx,sizeof(idx),"{\"index\":0,\"media\":\"common\"}");if(lx_music_ubus("player_play_index",idx,raw,sizeof(raw))||!lx_music_reply_ok(raw))return 1;}
 lx_music_queue_count=count;snprintf(lx_music_playlist,sizeof(lx_music_playlist),"%s",playlist);for(int i=0;i<count;i++)lx_music_queue[i]=tracks[i];
 for(int i=0;i<50;i++){if(!lx_music_state(state,sizeof(state),&status)&&status==1){snprintf(out,cap,"{\"ok\":true,\"status\":1,\"queue_total\":%d}",count);return 0;}usleep(100000);}
 snprintf(out,cap,"{\"ok\":false,\"error\":\"playback_not_confirmed\",\"status\":%d}",status);return 1;
}
static int lx_music_play_url(const char *url,char *out,size_t cap){const char *urls[1]={url};struct lx_music_track track={0};snprintf(track.audio_id,sizeof(track.audio_id),"lx06-direct-stream");snprintf(track.title,sizeof(track.title),"网络音频流");return lx_music_play_queue(urls,&track,1,"网络音频流",out,cap);}
static int lx_music_skip(int delta,char *out,size_t cap){char state[2048],args[128],raw[32768];if(!lx_music_queue_count||lx_music_state(state,sizeof(state),NULL))return 1;void *o=mj.parse(state),*v=NULL;int index=0;if(!o||!mj.get(o,"queue_index",&v)){if(o)mj.put(o);return 1;}index=mj.integer(v)-1;mj.put(o);index=(index+delta+lx_music_queue_count)%lx_music_queue_count;snprintf(args,sizeof(args),"{\"index\":%d,\"media\":\"common\"}",index);if(lx_music_ubus("player_play_index",args,raw,sizeof(raw))||!lx_music_reply_ok(raw))return 1;snprintf(out,cap,"{\"ok\":true,\"index\":%d}",index+1);return 0;}
static int lx_music_dispatch(const char *request,char *out,size_t cap){
 void *obj=mj.parse(request),*v=NULL;int rc=1;const char *op;
 snprintf(out,cap,"{\"ok\":false,\"error\":\"invalid_request_or_execution\"}");
 if(!obj||mj.type(obj)!=4||!mj.get(obj,"op",&v)||mj.type(v)!=6)goto done;
 op=mj.string(v);if((size_t)mj.length(v)!=strlen(op))goto done;
 if(!strcmp(op,"state"))rc=lx_music_state(out,cap,NULL);
 else if(!strcmp(op,"pause")||!strcmp(op,"resume")||!strcmp(op,"stop"))rc=lx_music_control(op,out,cap);
 else if(!strcmp(op,"play")&&mj.get(obj,"url",&v)&&mj.type(v)==6&&(size_t)mj.length(v)==strlen(mj.string(v)))rc=lx_music_play_url(mj.string(v),out,cap);
 else if(!strcmp(op,"next"))rc=lx_music_skip(1,out,cap);
 else if(!strcmp(op,"previous"))rc=lx_music_skip(-1,out,cap);
done:
 if(obj)mj.put(obj);return rc;
}
#endif
