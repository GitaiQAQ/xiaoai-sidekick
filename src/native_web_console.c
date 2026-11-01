/* Small LAN console. Static assets + bounded JSON API; no shell or web runtime. */
#define _GNU_SOURCE
#include <sys/prctl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/types.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <ctype.h>
#include <dirent.h>
#include "native_music.h"
#include "native_netease.h"
#include "native_tools.h"

#define AGENT "/data/lx06-agent/lx06-duplex-armhf"
#define PIDFILE "/data/lx06-agent/web.pid"
#define HEADER_MAX 8192
#define BODY_MAX 4096
static volatile sig_atomic_t web_stop;
static char voice_key[4097],csrf[33];
static const char *web_root="/data/lx06-agent/web";
static unsigned short web_port=18080;
static int64_t started;
struct request {char method[8],path[160],host[100],origin[160],token[64];size_t length;};
struct event {char action[32];int ok;int64_t at;};
static struct event events[12];static unsigned event_count;
static char command_id[64];
static int local_tool_request;
#define TOOL_SOCKET "/data/lx06-agent/control.sock"
static void stop_web(int sig){(void)sig;web_stop=1;}
static void add_s(void *o,const char *k,const char *s){mj.add(o,k,mj.new_string(s));}
static void add_i(void *o,const char *k,int n){mj.add(o,k,mj.new_int(n));}
static void audit(const char *action,int ok){struct event *e=&events[event_count++%12];snprintf(e->action,sizeof(e->action),"%s",action);e->ok=ok;e->at=lx_music_ms();}
static int copy_field(char *out,size_t cap,const char *a,size_t n){if(n>=cap)return 1;memcpy(out,a,n);out[n]=0;return 0;}
/* Parse one CRLF-only header. Duplicated lengths/hosts and transfer encoding
   are rejected, so the server and client cannot disagree about body framing. */
static int parse_request(const char *h,size_t n,struct request *r){
 const char *p=h,*end=h+n,*e;int hosts=0,lengths=0,origins=0,tokens=0;
 memset(r,0,sizeof(*r));
 if(n<4||memcmp(end-4,"\r\n\r\n",4)||memchr(h,0,n))return 1;
 e=strstr(p,"\r\n");if(!e)return 1;
 const char *space=memchr(p,' ',(size_t)(e-p));if(!space||copy_field(r->method,sizeof(r->method),p,(size_t)(space-p)))return 1;
 p=space+1;space=memchr(p,' ',(size_t)(e-p));
 if(!space||copy_field(r->path,sizeof(r->path),p,(size_t)(space-p)))return 1;
 if((size_t)(e-space)!=9||memcmp(space+1,"HTTP/1.1",8))return 1;
 for(p=r->method;*p;p++)if(*p<'A'||*p>'Z')return 1;
 for(p=r->path;*p;p++)if((unsigned char)*p<33||(unsigned char)*p>126)return 1;
 p=e+2;
 while(p<end-2){
  e=strstr(p,"\r\n");if(!e||e==p)return 1;
  const char *colon=memchr(p,':',(size_t)(e-p));if(!colon||colon==p)return 1;
  for(const char *q=p;q<colon;q++)if(!isalnum((unsigned char)*q)&&*q!='-')return 1;
  const char *v=colon+1,*ve=e;while(v<ve&&(*v==' '||*v=='\t'))v++;while(ve>v&&(ve[-1]==' '||ve[-1]=='\t'))ve--;
  for(const char *q=v;q<ve;q++)if((unsigned char)*q<32||(unsigned char)*q==127)return 1;
  size_t key=(size_t)(colon-p),len=(size_t)(ve-v);
  if(key==4&&!strncasecmp(p,"Host",4)){if(hosts++||copy_field(r->host,sizeof(r->host),v,len))return 1;}
  else if(key==14&&!strncasecmp(p,"Content-Length",14)){
   if(lengths++||!len||len>5)return 1;
   for(const char *q=v;q<ve;q++){if(*q<'0'||*q>'9')return 1;r->length=r->length*10+(unsigned)(*q-'0');if(r->length>BODY_MAX)return 1;}
  }else if(key==17&&!strncasecmp(p,"Transfer-Encoding",17))return 1;
  else if(key==6&&!strncasecmp(p,"Origin",6)){if(origins++||copy_field(r->origin,sizeof(r->origin),v,len))return 1;}
  else if(key==12&&!strncasecmp(p,"X-LX06-Token",12)){if(tokens++||copy_field(r->token,sizeof(r->token),v,len))return 1;}
  p=e+2;
 }
 return hosts!=1||!r->host[0];
}
static int write_all(int fd,const void *buffer,size_t n){const char *p=buffer;while(n){ssize_t k=send(fd,p,n,MSG_NOSIGNAL);if(k<0&&errno==EINTR)continue;if(k<=0)return 1;p+=k;n-=(size_t)k;}return 0;}
static void respond(int fd,int code,const char *type,const char *body,size_t n){
 char head[1024];int size=snprintf(head,sizeof(head),
 "HTTP/1.1 %d %s\r\nContent-Type: %s\r\nContent-Length: %zu\r\nConnection: close\r\nCache-Control: no-store\r\nX-Content-Type-Options: nosniff\r\nReferrer-Policy: no-referrer\r\nContent-Security-Policy: default-src 'self'; script-src 'self'; style-src 'self'; img-src 'self' data:; connect-src 'self'; frame-ancestors 'none'; base-uri 'none'; form-action 'self'\r\n\r\n",
 code,code==200?"OK":code==400?"Bad Request":code==403?"Forbidden":code==404?"Not Found":code==409?"Conflict":code==503?"Service Unavailable":"Error",type,n);
 if(size>0&&(size_t)size<sizeof(head)&&!write_all(fd,head,(size_t)size))write_all(fd,body,n);
}
static void json_reply(int fd,int code,void *obj){
 if(command_id[0]){void *wrapper=mj.object();add_i(wrapper,"v",1);add_s(wrapper,"id",command_id);mj.add(wrapper,"result",obj);obj=wrapper;command_id[0]=0;}
 const char *s=mj.encode(obj);respond(fd,code,"application/json; charset=utf-8",s,strlen(s));mj.put(obj);
}
static void error_reply(int fd,int code,const char *name){void *o=mj.object();add_i(o,"ok",0);add_s(o,"error",name);json_reply(fd,code,o);}
static int read_file(const char *path,char *out,size_t cap){FILE *f=fopen(path,"r");if(!f)return -1;size_t n=fread(out,1,cap-1,f);int bad=ferror(f)||(!feof(f)&&n==cap-1);fclose(f);out[n]=0;return bad?-1:(int)n;}
static int agent_pid(void){
 FILE *f=fopen("/data/lx06-agent/lx06.pid","r");long pid=0;char path[96],cmd[512];
 if(!f)return 0;int ok=fscanf(f,"%ld",&pid)==1;fclose(f);if(!ok||pid<=1||pid>4194304)return 0;
 snprintf(path,sizeof(path),"/proc/%ld/cmdline",pid);int n=read_file(path,cmd,sizeof(cmd));if(n<=0)return 0;
 size_t a=strnlen(cmd,(size_t)n);if(a+1>=(size_t)n)return 0;
 if(strcmp(cmd,"/lib/ld-linux-armhf.so.3")||strcmp(cmd+a+1,AGENT))return 0;
 return kill((pid_t)pid,0)==0?(int)pid:0;
}
static int start_agent(int once){
 if(agent_pid())return once?1:0;if(!voice_key[0]||access(AGENT,X_OK))return 1;
 int fds[2];if(pipe(fds))return 1;pid_t p=fork();
 if(p==0){
  int log=open("/data/lx06-agent/client.log",O_WRONLY|O_CREAT|O_APPEND,0600);
  dup2(fds[0],0);if(log>=0){dup2(log,1);dup2(log,2);if(log>2)close(log);}close(fds[0]);close(fds[1]);
  if(once)execl("/lib/ld-linux-armhf.so.3","/lib/ld-linux-armhf.so.3",AGENT,"--kws","300","--listen-once","--daemon","/data/lx06-agent/lx06.pid",(char *)NULL);
  else execl("/lib/ld-linux-armhf.so.3","/lib/ld-linux-armhf.so.3",AGENT,"--kws","300","--daemon","/data/lx06-agent/lx06.pid",(char *)NULL);_exit(127);
 }
 close(fds[0]);if(p<0){close(fds[1]);return 1;}
 size_t n=strlen(voice_key),off=0;while(off<n){ssize_t k=write(fds[1],voice_key+off,n-off);if(k<0&&errno==EINTR)continue;if(k<=0)break;off+=(size_t)k;}if(off==n){ssize_t k=write(fds[1],"\n",1);(void)k;}close(fds[1]);
 int status=0;int64_t until=lx_music_ms()+2500;
 while(waitpid(p,&status,WNOHANG)==0){if(lx_music_ms()>until){kill(p,SIGKILL);waitpid(p,&status,0);return 1;}usleep(10000);}
 for(int i=0;i<20;i++){if(agent_pid())return 0;usleep(50000);}return 1;
}
static int stop_agent(void){int pid=agent_pid();if(!pid)return 0;if(kill(pid,SIGTERM))return 1;for(int i=0;i<30;i++){if(!agent_pid())return 0;usleep(100000);}return 1;}
static void *ubus_info(const char *object,const char *method){
 char raw[8192];void *root,*info=NULL,*copy=NULL;
 if(lx_music_ubus_object(object,method,"{}",raw,sizeof(raw)))return NULL;
 root=mj.parse(raw);if(!root)return NULL;
 if(mj.get(root,"info",&info)){
  if(mj.type(info)==6)copy=mj.parse(mj.string(info));else copy=mj.parse(mj.encode(info));
 }
 mj.put(root);return copy;
}
static void *build_state(void){
 void *root=mj.object(),*device=mj.object(),*voice=mj.object(),*music=mj.object(),*lamp=mj.object(),*account=mj.object(),*recent=mj.array();
 int pid=agent_pid();char raw[8192];
 add_i(root,"ok",1);add_s(root,"csrf",csrf);add_i(root,"version",1);add_i(root,"v",1);add_s(root,"type","state.snapshot");
 add_s(device,"name","小爱音箱 Pro");add_s(device,"model","LX06");add_i(device,"web_pid",(int)getpid());
 add_i(device,"uptime_seconds",(int)(lx_music_ms()/1000));add_i(device,"web_uptime_seconds",(int)((lx_music_ms()-started)/1000));
 FILE *mem=fopen("/proc/meminfo","r");int total=0,available=0;if(mem){while(fgets(raw,sizeof(raw),mem)){sscanf(raw,"MemTotal: %d",&total);sscanf(raw,"MemAvailable: %d",&available);}fclose(mem);}
 add_i(device,"memory_total_kib",total);add_i(device,"memory_available_kib",available);
 struct statvfs fs;if(!statvfs("/data",&fs)){add_i(device,"storage_total_kib",(int)((uint64_t)fs.f_blocks*fs.f_frsize/1024));add_i(device,"storage_free_kib",(int)((uint64_t)fs.f_bavail*fs.f_frsize/1024));}
 int rss=0;FILE *self=fopen("/proc/self/status","r");if(self){while(fgets(raw,sizeof(raw),self))sscanf(raw,"VmRSS: %d",&rss);fclose(self);}add_i(device,"web_rss_kib",rss);
 void *mesh=ubus_info("mibt_mesh","mesh_status"),*running=NULL;
 add_i(device,"mesh_known",mesh&&mj.get(mesh,"mesh_is_running",&running)&&mj.type(running)==1);
 add_i(device,"mesh_running",running?mj.integer(running):0);if(mesh)mj.put(mesh);
 add_i(voice,"running",pid!=0);add_i(voice,"pid",pid);add_i(voice,"credentials_loaded",voice_key[0]!=0);
 const char *phase=pid?"running":"stopped";
 if(pid&&read_file("/tmp/lx06-agent-state.json",raw,sizeof(raw))>0){
  void *st=mj.parse(raw),*v=NULL;int state_pid=0;
  if(st&&lx_music_number(st,"pid",&state_pid)&&state_pid==pid&&mj.get(st,"state",&v)&&mj.type(v)==6){
   const char *s=mj.string(v);const char *allowed[]={"standby","connecting","listening","thinking","speaking","followup","stopping","error"};
   for(unsigned i=0;i<sizeof(allowed)/sizeof(allowed[0]);i++)if(!strcmp(s,allowed[i]))phase=allowed[i];
  }
  if(st)mj.put(st);
 }
 add_s(voice,"state",phase);
 if(!lx_music_state(raw,sizeof(raw),NULL)){mj.put(music);music=mj.parse(raw);}else{add_i(music,"ok",0);add_s(music,"error","state_unavailable");}
 void *led=ubus_info("led","status"),*effects=NULL;int effect=0;
 if(led&&mj.get(led,"effect",&effects)&&mj.type(effects)==5){
  /* The factory reply is bounded; only numeric effect IDs leave this API. */
  if(mj.array_length(effects)>0)lx_music_number(mj.index(effects,0),"type",&effect);
 }
 add_i(lamp,"effect",effect);add_i(lamp,"on",effect!=0);if(led)mj.put(led);
 mj.put(account);account=nc_status();add_s(account,"name","网易云音乐");
 unsigned count=event_count>12?12:event_count;
 for(unsigned i=0;i<count;i++){struct event *e=&events[(event_count-1-i)%12];void *o=mj.object();add_s(o,"action",e->action);add_i(o,"ok",e->ok);add_i(o,"age_seconds",(int)((lx_music_ms()-e->at)/1000));mj.append(recent,o);}
 mj.add(root,"device",device);mj.add(root,"voice",voice);mj.add(root,"music",music);mj.add(root,"lamp",lamp);mj.add(root,"netease",account);mj.add(root,"events",recent);
 return root;
}
static void state_reply(int fd){json_reply(fd,200,build_state());}
static void event_reply(int fd){
 void *o=build_state();const char *json=mj.encode(o);size_t n=strlen(json)+100;char *frame=malloc(n);if(!frame){mj.put(o);error_reply(fd,503,"allocation_failed");return;}
 int len=snprintf(frame,n,"retry: 4000\nevent: state\ndata: %s\n\n",json);respond(fd,200,"text/event-stream; charset=utf-8",frame,(size_t)len);free(frame);mj.put(o);
}
/* Every capability runs through this one dispatcher; the HTTP action endpoint,
   the model-tool endpoint and the RPC wrapper all share it. */
static void *action_execute(void *obj,const char **action_out,int *code_out,const char **err_out){
 void *value=NULL,*out=NULL;const char *action=NULL;char reply[1024]="{\"ok\":false}";int rc=1,code=200;
 *code_out=200;*err_out="invalid_action";*action_out=NULL;
 if(!obj||mj.type(obj)!=4||!mj.get(obj,"action",&value)||mj.type(value)!=6||(size_t)mj.length(value)!=strlen(mj.string(value)))return NULL;
 action=mj.string(value);*action_out=action;
 if(!strncmp(action,"netease_",8)) {
  if(!strcmp(action,"netease_login"))out=nc_qr_start();
  else if(!strcmp(action,"netease_check"))out=nc_qr_check();
  else if(!strcmp(action,"netease_refresh")){int bad=nc_refresh();out=nc_status();add_i(out,"ok",!bad);}
  else if(!strcmp(action,"netease_logout"))out=nc_logout();
  else if(!strcmp(action,"netease_search")) {
   const char *q=nc_string(obj,"query");if(q&&*q&&strlen(q)<=256)out=nc_search(q);
  }else if(!strcmp(action,"netease_charts"))out=nc_charts();
  else if(!strcmp(action,"netease_playlists")){if(!nc.authenticated&&nc.music_u[0])nc_refresh();out=nc_user_playlists();}
  else if(!strcmp(action,"netease_chart_tracks")){char id[32];if(nc_id(obj,"id",id))out=nc_chart_tracks(id);}
  else if(!strcmp(action,"netease_chart_play")||(!strcmp(action,"netease_chart_prepare")&&local_tool_request)||!strcmp(action,"netease_playlist_play")||(!strcmp(action,"netease_playlist_prepare")&&local_tool_request)){char id[32];const char *q=nc_string(obj,"quality");if(!q)q="exhigh";if(nc_id(obj,"id",id)&&(!strcmp(q,"standard")||!strcmp(q,"exhigh")||!strcmp(q,"lossless"))){if(!nc.authenticated&&nc.music_u[0])nc_refresh();nc_defer_play=!strcmp(action,"netease_chart_prepare")||!strcmp(action,"netease_playlist_prepare");out=nc_play_chart(id,q);nc_defer_play=0;}}
  else if(!strcmp(action,"netease_commit")&&local_tool_request){
   const char *ticket=nc_string(obj,"ticket");int ok=0;char result[1024];
   if(ticket&&*ticket&&!strcmp(ticket,nc_pending_ticket)&&lx_music_ms()<nc_pending_until){if(nc_pending_count>0)ok=!lx_music_play_queue(nc_pending_urlptr,nc_pending_queue,nc_pending_count,nc_pending_playlist,result,sizeof(result));else{const char *urls[1]={nc_pending_url};ok=!lx_music_play_queue(urls,&nc_pending_track,1,nc_pending_track.title[0]?nc_pending_track.title:"网易云音乐",result,sizeof(result));}nc_pending_ticket[0]=nc_pending_url[0]=0;nc_pending_count=0;}
   out=mj.object();add_i(out,"ok",ok);if(!ok)add_s(out,"error","pending_playback_not_confirmed");
  }else if(!strcmp(action,"netease_play")||(!strcmp(action,"netease_prepare")&&local_tool_request)) {
   char id[32];const char *q=nc_string(obj,"quality");if(!q)q="exhigh";
   if(nc_id(obj,"id",id)&&(!strcmp(q,"standard")||!strcmp(q,"exhigh")||!strcmp(q,"lossless"))){if(!nc.authenticated&&nc.music_u[0])nc_refresh();nc_defer_play=!strcmp(action,"netease_prepare");out=nc_play(id,q,nc_string(obj,"title"),nc_string(obj,"artist"));nc_defer_play=0;}
  }
  if(!out){*code_out=400;*err_out="invalid_netease_request";return NULL;}
  int ok=0;lx_music_number(out,"ok",&ok);if(strcmp(action,"netease_check")||nc.authenticated)audit(action,ok);add_s(out,"action",action);return out;
 }
 if(!strcmp(action,"voice_start")||!strcmp(action,"voice_listen")){rc=start_agent(!strcmp(action,"voice_listen"));if(rc)code=409;}
 else if(!strcmp(action,"voice_stop")){rc=stop_agent();if(rc)code=409;}
 else if(!strcmp(action,"music_pause")){nc_pending_ticket[0]=nc_pending_url[0]=0;nc_pending_count=0;rc=lx_music_dispatch("{\"op\":\"pause\"}",reply,sizeof(reply));}
 else if(!strcmp(action,"music_status")){
  char raw[8192];int status=-1;if(!lx_music_state(raw,sizeof(raw),NULL)){void *state=mj.parse(raw);if(state){lx_music_number(state,"status",&status);mj.put(state);}}
  out=mj.object();add_i(out,"ok",status>=0);add_i(out,"status",status);add_s(out,"action",action);if(status<0)add_s(out,"error","state_unavailable");
  audit(action,status>=0);return out;
 }
 else if(!strcmp(action,"music_next"))rc=lx_music_dispatch("{\"op\":\"next\"}",reply,sizeof(reply));
 else if(!strcmp(action,"music_previous"))rc=lx_music_dispatch("{\"op\":\"previous\"}",reply,sizeof(reply));
 else if(!strcmp(action,"music_resume"))rc=lx_music_dispatch("{\"op\":\"resume\"}",reply,sizeof(reply));
 else if(!strcmp(action,"music_stop")){
  nc_pending_ticket[0]=nc_pending_url[0]=0;
  nc_pending_count=0;
  rc=lx_music_dispatch("{\"op\":\"stop\"}",reply,sizeof(reply));
  if(!rc&&!agent_pid()){char raw[8192];lx_music_ubus_object("led","shut","{\"L\":3}",raw,sizeof(raw));}
 }else if(!strcmp(action,"music_play")){
  void *url=NULL;if(mj.get(obj,"url",&url)&&mj.type(url)==6&&(size_t)mj.length(url)==strlen(mj.string(url)))rc=lx_music_play_url(mj.string(url),reply,sizeof(reply));else code=400;
 }else if(!strcmp(action,"volume")){
  int volume=-1;char args[160],raw[8192];
  if(!lx_music_number(obj,"value",&volume)||volume<0||volume>100)code=400;
  else{
   snprintf(args,sizeof(args),"{\"volume\":%d,\"beep\":0,\"tts_silence\":true}",volume);rc=lx_music_ubus("player_set_volume",args,raw,sizeof(raw))||!lx_music_reply_ok(raw);
   int confirmed=-1;if(!rc){for(int i=0;i<8;i++){if(!lx_music_state(raw,sizeof(raw),NULL)){void *state=mj.parse(raw);if(state){lx_music_number(state,"volume",&confirmed);mj.put(state);}}if(confirmed==volume)break;usleep(100000);}rc=confirmed!=volume;}
  }
 }else if(!strcmp(action,"lamp_clear")){
  if(agent_pid()){code=409;rc=1;}else{char raw[8192];rc=0;const int ids[]={1,2,3,8};for(unsigned i=0;i<4;i++){char args[32];snprintf(args,sizeof(args),"{\"L\":%d}",ids[i]);if(lx_music_ubus_object("led","shut",args,raw,sizeof(raw)))rc=1;}}
 }else if(!strcmp(action,"session_end")){
  /* The voice client turns this result into "close after the current reply". */
  rc=0;
 }else{*code_out=400;*err_out="unknown_action";return NULL;}
 audit(action,!rc);if(rc&&code==200)code=409;
 out=mj.object();add_i(out,"ok",!rc);add_s(out,"action",action);if(rc)add_s(out,"error","operation_not_confirmed");
 *code_out=code;return out;
}
static void action_reply(int fd,const char *body){
 void *obj=mj.parse(body),*out=NULL;const char *action=NULL,*err=NULL;int code=200;
 if(obj){out=action_execute(obj,&action,&code,&err);mj.put(obj);}else err="invalid_action";
 if(!out){error_reply(fd,code,err?err:"invalid_action");return;}
 json_reply(fd,200,out);
}
static void tools_reply(int fd){
 char catalog[8192];int n=lx_tools_json(LX_TOOL_VOICE,catalog,sizeof(catalog));
 if(n<0){error_reply(fd,503,"tool_catalog_too_large");return;}
 void *arr=mj.parse(catalog);if(!arr){error_reply(fd,503,"tool_catalog_invalid");return;}
 void *o=mj.object();add_i(o,"ok",1);add_i(o,"version",1);mj.add(o,"tools",arr);json_reply(fd,200,o);
}
/* Local-only: the voice client forwards model tool calls here. Arguments are
   the provider JSON string; unknown tools and oversized arguments are refused. */
static void tool_reply(int fd,const char *body){
 void *o=mj.parse(body),*nv=NULL,*av=NULL,*args=NULL,*result=NULL;const char *name=NULL,*err=NULL;int code=200;
 if(!o||mj.type(o)!=4||!mj.get(o,"name",&nv)||mj.type(nv)!=6||strlen(mj.string(nv))>=64){if(o)mj.put(o);error_reply(fd,400,"invalid_tool_request");return;}
 name=mj.string(nv);
 char tool_name[64];snprintf(tool_name,sizeof(tool_name),"%s",name);name=tool_name;
 if(mj.get(o,"arguments",&av)){
  if(mj.type(av)==6){const char *s=mj.string(av);if((size_t)mj.length(av)!=strlen(s)||strlen(s)>3000){mj.put(o);error_reply(fd,400,"invalid_tool_arguments");return;}args=mj.parse(s);}
  else if(mj.type(av)==4)args=mj.parse(mj.encode(av));
 }
 mj.put(o);
 if(!args)args=mj.object();
 if(mj.type(args)!=4){mj.put(args);error_reply(fd,400,"invalid_tool_arguments");return;}
 const struct lx_tool_def *t=lx_tool_find(name,LX_TOOL_VOICE);
 if(!t){mj.put(args);void *r=mj.object();add_i(r,"ok",0);add_s(r,"error","unknown_tool");json_reply(fd,200,r);return;}
 add_s(args,"action",t->action);
 const char *used=NULL;result=action_execute(args,&used,&code,&err);mj.put(args);
 if(!result){void *r=mj.object();add_i(r,"ok",0);add_s(r,"error",err?err:"tool_failed");json_reply(fd,200,r);return;}
 json_reply(fd,200,result);
}
static void command_reply(int fd,const char *body){
 static const char *map[][2]={
 {"voice.start","voice_start"},{"voice.listen","voice_listen"},{"voice.stop","voice_stop"},{"music.pause","music_pause"},{"music.resume","music_resume"},{"music.stop","music_stop"},{"music.status","music_status"},{"music.next","music_next"},{"music.previous","music_previous"},{"music.stream.play","music_play"},{"device.volume.set","volume"},{"device.lamp.clear","lamp_clear"},
  {"accounts.netease.login.start","netease_login"},{"accounts.netease.login.check","netease_check"},{"accounts.netease.refresh","netease_refresh"},{"accounts.netease.logout","netease_logout"},{"music.netease.search","netease_search"},{"music.netease.play","netease_play"},{"music.netease.charts","netease_charts"},{"music.netease.playlists","netease_playlists"},{"music.netease.chart.tracks","netease_chart_tracks"},{"music.netease.playlist.tracks","netease_chart_tracks"},{"music.netease.chart.play","netease_chart_play"},{"music.netease.playlist.play","netease_playlist_play"},{"music.netease.chart.prepare","netease_chart_prepare"},{"music.netease.playlist.prepare","netease_playlist_prepare"},{"music.netease.prepare","netease_prepare"},{"music.netease.commit","netease_commit"}};
 void *o=mj.parse(body),*params=NULL;int version=0;const char *method=o?nc_string(o,"method"):NULL,*id=o?nc_string(o,"id"):NULL,*action=NULL;
 if(!o||mj.type(o)!=4||!lx_music_number(o,"v",&version)||version!=1||!method||!id||!id[0]||strlen(id)>=sizeof(command_id)){if(o)mj.put(o);error_reply(fd,400,"invalid_envelope");return;}
 for(const char *p=id;*p;p++)if(!isalnum((unsigned char)*p)&&*p!='-'&&*p!='_'&&*p!='.'&&*p!=':'){mj.put(o);error_reply(fd,400,"invalid_request_id");return;}
 snprintf(command_id,sizeof(command_id),"%s",id);
 for(unsigned i=0;i<sizeof(map)/sizeof(map[0]);i++)if(!strcmp(method,map[i][0]))action=map[i][1];
 if(local_tool_request&&strncmp(method,"music.",6)){mj.put(o);error_reply(fd,403,"tool_method_rejected");return;}
 if(!action||!mj.get(o,"params",&params)||mj.type(params)!=4){mj.put(o);error_reply(fd,400,"unknown_method_or_params");return;}
 void *request=mj.parse(mj.encode(params));add_s(request,"action",action);action_reply(fd,mj.encode(request));mj.put(request);mj.put(o);
}
static void static_reply(int fd,const char *path){
 const char *name,*type;if(!strcmp(path,"/")){name="index.html";type="text/html; charset=utf-8";}else if(!strcmp(path,"/app.css")){name="app.css";type="text/css; charset=utf-8";}else if(!strcmp(path,"/app.js")){name="app.js";type="text/javascript; charset=utf-8";}else if(!strcmp(path,"/qrcode.js")){name="qrcode.js";type="text/javascript; charset=utf-8";}else{error_reply(fd,404,"not_found");return;}
 char file[512];snprintf(file,sizeof(file),"%s/%s",web_root,name);int f=open(file,O_RDONLY|O_NOFOLLOW);struct stat64 st;
 if(f<0){error_reply(fd,404,"asset_missing");return;}if(__fxstat64(0,f,&st)||!S_ISREG(st.st_mode)||st.st_size<0||st.st_size>65536){close(f);error_reply(fd,503,"invalid_asset");return;}
 size_t n=(size_t)st.st_size;char *data=malloc(n?n:1);if(!data){close(f);error_reply(fd,503,"allocation_failed");return;}
 size_t used=0;while(used<n){ssize_t k=read(f,data+used,n-used);if(k<=0)break;used+=(size_t)k;}close(f);if(used==n)respond(fd,200,type,data,n);else error_reply(fd,503,"asset_read_failed");free(data);
}
static void handle_connection(int fd){
 char buffer[HEADER_MAX+BODY_MAX+1];size_t used=0,head=0;int64_t until=lx_music_ms()+3000;
 struct timeval timeout={1,0};setsockopt(fd,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout));setsockopt(fd,SOL_SOCKET,SO_SNDTIMEO,&timeout,sizeof(timeout));
 while(!head&&used<HEADER_MAX&&!web_stop&&lx_music_ms()<until){ssize_t n=recv(fd,buffer+used,HEADER_MAX-used,0);if(n<=0)break;used+=(size_t)n;buffer[used]=0;char *end=strstr(buffer,"\r\n\r\n");if(end)head=(size_t)(end-buffer)+4;}
 if(!head){error_reply(fd,400,"incomplete_headers");return;}
 char saved=buffer[head];buffer[head]=0;struct request r;int bad=parse_request(buffer,head,&r);buffer[head]=saved;
 if(bad||used>head+r.length){error_reply(fd,400,"invalid_request");return;}
 struct sockaddr_in local;socklen_t size=sizeof(local);char host[100]="",origin[160]="";if(!local_tool_request&&getsockname(fd,(struct sockaddr *)&local,&size)){error_reply(fd,403,"invalid_host");return;}
 if(!local_tool_request){
 snprintf(host,sizeof(host),"%s:%u",inet_ntoa(local.sin_addr),web_port);if(strcmp(r.host,host)){error_reply(fd,403,"invalid_host");return;}
 snprintf(origin,sizeof(origin),"http://%s",host);
 }
 while(used<head+r.length&&lx_music_ms()<until){ssize_t n=recv(fd,buffer+used,head+r.length-used,0);if(n<=0)break;used+=(size_t)n;}if(used!=head+r.length){error_reply(fd,400,"incomplete_body");return;}buffer[used]=0;
 if(!local_tool_request&&!strcmp(r.method,"GET")&&!r.length){if(!strcmp(r.path,"/api/state")||!strcmp(r.path,"/api/v1/state"))state_reply(fd);else if(!strcmp(r.path,"/api/v1/events"))event_reply(fd);else static_reply(fd,r.path);return;}
 if(!strcmp(r.method,"POST")&&(!strcmp(r.path,"/api/action")||!strcmp(r.path,"/api/v1/command"))){
  if(!local_tool_request&&(strcmp(r.origin,origin)||strcmp(r.token,csrf))){error_reply(fd,403,"origin_or_token_rejected");return;}
  if(local_tool_request&&strcmp(r.path,"/api/v1/command")){error_reply(fd,403,"tool_path_rejected");return;}
  if(!strcmp(r.path,"/api/v1/command"))command_reply(fd,buffer+head);else action_reply(fd,buffer+head);return;
 }
 if(!strcmp(r.method,"POST")&&(!strcmp(r.path,"/api/v1/tool")||!strcmp(r.path,"/api/v1/tools"))){
  if(!local_tool_request){error_reply(fd,403,"tool_path_rejected");return;}
  if(!strcmp(r.path,"/api/v1/tools"))tools_reply(fd);else tool_reply(fd,buffer+head);
  return;
 }
 error_reply(fd,404,"not_found");
}
static int parser_test(void){
 struct request r;int checks=0;
 const char *good="POST /api/action HTTP/1.1\r\nHost: 127.0.0.1:18080\r\nContent-Length: 2\r\n\r\n";
 if(parse_request(good,strlen(good),&r)||r.length!=2)return 1;checks++;
 const char *bad[]={"GET / HTTP/1.1\r\n\r\n","POST / HTTP/1.1\r\nHost: x\r\nContent-Length: 1\r\nContent-Length: 2\r\n\r\n","GET / HTTP/1.1\r\nHost: x\r\nHost: y\r\n\r\n","POST / HTTP/1.1\r\nHost: x\r\nTransfer-Encoding: chunked\r\n\r\n","POST / HTTP/1.1\r\nHost: x\r\nContent-Length: 99999\r\n\r\n","GET / HTTP/1.1\nHost: x\n\n"};
 for(unsigned i=0;i<sizeof(bad)/sizeof(bad[0]);i++){if(!parse_request(bad[i],strlen(bad[i]),&r))return 1;checks++;}
 printf("web_parser checks=%d pass=1\n",checks);return 0;
}
int main(int argc,char **argv){
 if(prctl(PR_SET_DUMPABLE,0L,0L,0L,0L)||prctl(PR_GET_DUMPABLE,0L,0L,0L,0L)!=0)return 1;
 if(argc==2&&!strcmp(argv[1],"--self-test"))return parser_test();
 if(argc==2&&!strcmp(argv[1],"--ncm-probe")) {
  void *j=dlopen("libjson-c.so.2",RTLD_NOW|RTLD_LOCAL);if(!j||lx_music_init(j)||nc_init())return 1;
  void *o=nc_qr_start();int ok=0;lx_music_number(o,"ok",&ok);printf("netease_qr_probe ok=%d api_code=%d error=%s\n",ok,nc.last_api_code,nc.error);mj.put(o);return !ok;
 }
 int daemon=0;for(int i=1;i<argc;i++){if(!strcmp(argv[i],"--daemon"))daemon=1;else if(!strcmp(argv[i],"--root")&&i+1<argc)web_root=argv[++i];else return 2;}
 signal(SIGTERM,stop_web);signal(SIGINT,stop_web);signal(SIGPIPE,SIG_IGN);
 if(fgets(voice_key,sizeof(voice_key),stdin))voice_key[strcspn(voice_key,"\r\n")]=0;
 for(size_t i=0;voice_key[i];i++)if((unsigned char)voice_key[i]<33||(unsigned char)voice_key[i]>126)return 2;
 /* The key is read once. A daemon child that keeps the SSH stdin pipe open
    prevents the launching session from closing, so detach it here. */
 if(daemon){int devnull=open("/dev/null",O_RDONLY);if(devnull>=0){dup2(devnull,0);if(devnull>2)close(devnull);}}
 int random=open("/dev/urandom",O_RDONLY);unsigned char nonce[16];if(random<0||read(random,nonce,sizeof(nonce))!=(ssize_t)sizeof(nonce))return 1;close(random);for(unsigned i=0;i<16;i++)snprintf(csrf+i*2,3,"%02x",nonce[i]);
 void *json=dlopen("libjson-c.so.2",RTLD_NOW|RTLD_LOCAL);if(!json||lx_music_init(json)||nc_init())return 1;
 int sock=socket(AF_INET,SOCK_STREAM,0),one=1;struct sockaddr_in addr={0};if(sock<0)return 1;fcntl(sock,F_SETFD,FD_CLOEXEC);setsockopt(sock,SOL_SOCKET,SO_REUSEADDR,&one,sizeof(one));
 addr.sin_family=AF_INET;addr.sin_port=htons(web_port);addr.sin_addr.s_addr=htonl(INADDR_ANY);if(bind(sock,(struct sockaddr *)&addr,sizeof(addr))||listen(sock,8)){close(sock);return 1;}
 int local_sock=socket(AF_UNIX,SOCK_STREAM,0);struct sockaddr_un un={0};un.sun_family=AF_UNIX;snprintf(un.sun_path,sizeof(un.sun_path),"%s",TOOL_SOCKET);
 /* TCP bind above prevents a second instance from removing a live socket. */
 if(local_sock<0)return 1;fcntl(local_sock,F_SETFD,FD_CLOEXEC);unlink(TOOL_SOCKET);
 mode_t oldmask=umask(077);int local_bad=bind(local_sock,(struct sockaddr *)&un,sizeof(un))||chmod(TOOL_SOCKET,0600)||listen(local_sock,4);umask(oldmask);if(local_bad){close(local_sock);close(sock);return 1;}
 if(daemon){pid_t p=fork();if(p<0)return 1;if(p>0){close(sock);return 0;}if(setsid()<0)return 1;umask(077);FILE *f=fopen(PIDFILE,"w");if(!f)return 1;fprintf(f,"%d\n",(int)getpid());fclose(f);}
 started=lx_music_ms();printf("web_ready port=%u credentials_loaded=%d\n",web_port,voice_key[0]!=0);fflush(stdout);
 while(!web_stop){struct pollfd p[2]={{sock,POLLIN,0},{local_sock,POLLIN,0}};if(poll(p,2,250)<=0)continue;for(int i=0;i<2&&!web_stop;i++){if(!(p[i].revents&POLLIN))continue;int fd=accept(p[i].fd,NULL,NULL);if(fd<0)continue;fcntl(fd,F_SETFD,FD_CLOEXEC);local_tool_request=i==1;handle_connection(fd);local_tool_request=0;close(fd);}}
 close(local_sock);unlink(TOOL_SOCKET);close(sock);if(daemon)unlink(PIDFILE);volatile char *wipe=voice_key;for(size_t i=0;i<sizeof(voice_key);i++)wipe[i]=0;dlclose(json);return 0;
}
