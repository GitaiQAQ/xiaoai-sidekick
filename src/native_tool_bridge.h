/* Realtime FC -> private, permission-restricted Unix socket. No credentials
   cross this boundary. Network/provider work runs off the audio thread. */
#include <sys/socket.h>
#include <sys/un.h>
#include <fcntl.h>
#include <errno.h>
static struct {
 void *(*parse)(const char *);int (*get)(void *,const char *,void **);int (*type)(void *);int (*integer)(void *);
 const char *(*string)(void *);const char *(*encode)(void *);void (*put)(void *);
 void *(*object)(void);void *(*array)(void);void *(*str)(const char *);
 void (*add)(void *,const char *,void *);int (*append)(void *,void *);int (*length)(void *);int (*string_length)(void *);void *(*index)(void *,int);void (*del)(void *,const char *);
} tj;
static int tb_init(void *lib){
#define T(f,n) *(void **)(&tj.f)=dlsym(lib,n);if(!tj.f)return 1
 T(parse,"json_tokener_parse");T(get,"json_object_object_get_ex");T(type,"json_object_get_type");T(integer,"json_object_get_int");T(string,"json_object_get_string");T(encode,"json_object_to_json_string");T(put,"json_object_put");
 T(object,"json_object_new_object");T(array,"json_object_new_array");T(str,"json_object_new_string");T(add,"json_object_object_add");T(append,"json_object_array_add");T(length,"json_object_array_length");T(string_length,"json_object_get_string_len");T(index,"json_object_array_get_idx");T(del,"json_object_object_del");
#undef T
 return 0;
}
static const char *tb_s(void *o,const char *k){void *v=NULL;if(!o||!tj.get(o,k,&v)||tj.type(v)!=6)return NULL;const char *s=tj.string(v);return s&&(size_t)tj.string_length(v)==strlen(s)?s:NULL;}
static int tb_i(void *o,const char *k){void *v=NULL;return o&&tj.get(o,k,&v)?tj.integer(v):0;}
static int tb_safe_id(const char *s){if(!s||!*s||strlen(s)>128)return 0;for(;*s;s++)if(!((*s>='a'&&*s<='z')||(*s>='A'&&*s<='Z')||(*s>='0'&&*s<='9')||strchr("-_.:",*s)))return 0;return 1;}
/* One POST to the local console. unwrap_result=1 expects the /api/v1/command
   envelope {"result":{...}}; tool endpoints answer with the object itself. */
static void *tb_exchange(const char *path,void *payload,int unwrap_result,int64_t until){
 const char *body=tj.encode(payload);size_t n=strlen(body);char packet[6000];
 int size=snprintf(packet,sizeof(packet),"POST %s HTTP/1.1\r\nHost: localhost\r\nContent-Type: application/json\r\nContent-Length: %zu\r\n\r\n%s",path,n,body);
 if(size<=0||(size_t)size>=sizeof(packet))return NULL;
 int fd=socket(AF_UNIX,SOCK_STREAM,0);if(fd<0)return NULL;fcntl(fd,F_SETFD,FD_CLOEXEC);struct sockaddr_un un={0};un.sun_family=AF_UNIX;snprintf(un.sun_path,sizeof(un.sun_path),"/data/lx06-agent/control.sock");
 fcntl(fd,F_SETFL,O_NONBLOCK);if(connect(fd,(struct sockaddr *)&un,sizeof(un))){close(fd);return NULL;}
 size_t sent=0,used=0;char *raw=malloc(32769);if(!raw){close(fd);return NULL;}
 int bad=0;while(!g_stop&&now_ms()<until){struct pollfd p={fd,sent<(size_t)size?POLLOUT:POLLIN,0};int rc=poll(&p,1,100);if(rc<0){if(errno==EINTR)continue;bad=1;break;}if(!rc)continue;
  if(sent<(size_t)size){ssize_t k=send(fd,packet+sent,(size_t)size-sent,MSG_NOSIGNAL);if(k>0)sent+=(size_t)k;else if(errno!=EAGAIN&&errno!=EINTR){bad=1;break;}}
  else{if(used==32768){bad=1;break;}ssize_t k=recv(fd,raw+used,32768-used,0);if(k>0)used+=(size_t)k;else if(!k)break;else if(errno!=EAGAIN&&errno!=EINTR){bad=1;break;}}
 }
 if(now_ms()>=until||g_stop)bad=1;close(fd);raw[used]=0;void *out=NULL,*root=NULL,*result=NULL;char *start=strstr(raw,"\r\n\r\n");
 if(!bad&&start&&(root=tj.parse(start+4))){void *pick=root;if(unwrap_result){if(tj.get(root,"result",&result)&&tj.type(result)==4)pick=result;else pick=NULL;}if(pick&&tj.type(pick)==4)out=tj.parse(tj.encode(pick));}
 if(root)tj.put(root);free(raw);return out;
}
static void *tb_request(const char *method,void *params,int64_t until){
 void *req=tj.object();tj.add(req,"v",tj.parse("1"));tj.add(req,"id",tj.str("voice-tool"));tj.add(req,"method",tj.str(method));tj.add(req,"params",params);
 void *out=tb_exchange("/api/v1/command",req,1,until);tj.put(req);return out;
}
/* The console owns the tool catalog; the voice client only fetches and forwards. */
static void *tb_catalog(int64_t until){
 void *req=tj.object();void *resp=tb_exchange("/api/v1/tools",req,0,until),*tools=NULL,*copy=NULL;
 if(resp&&tj.get(resp,"tools",&tools)&&tj.type(tools)==5)copy=tj.parse(tj.encode(tools));
 if(resp)tj.put(resp);return copy;
}
static int tb_arguments_valid(const char *arguments){
 if(!arguments||strlen(arguments)>3000)return 0;
 /* json-c may accept a parsed prefix; require one complete object with only
    JSON whitespace after it, and reject comment/single-quote extensions. */
 size_t i=0;while(strchr(" \t\r\n",arguments[i])&&arguments[i])i++;if(arguments[i]!='{')return 0;
 int depth=0,quoted=0,escaped=0,complete=0;
 for(;arguments[i];i++){char c=arguments[i];if(quoted){if(escaped)escaped=0;else if(c=='\\')escaped=1;else if(c=='"')quoted=0;continue;}
  if(c=='"'){quoted=1;continue;}if(c=='/'||c=='\'')return 0;
  if(c=='{'||c=='[')depth++;else if(c=='}'||c==']'){if(--depth<0)return 0;if(!depth){complete=1;i++;break;}}
 }
 if(!complete||quoted)return 0;for(;arguments[i];i++)if(!strchr(" \t\r\n",arguments[i]))return 0;
 void *parsed=tj.parse(arguments);int valid=parsed&&tj.type(parsed)==4;if(parsed)tj.put(parsed);return valid;
}
static void *tb_call(const char *name,const char *arguments,int64_t until){
 if(!tb_arguments_valid(arguments))return tj.parse("{\"ok\":0,\"error\":\"invalid_tool_arguments\"}");
 void *req=tj.object();tj.add(req,"name",tj.str(name));tj.add(req,"arguments",tj.str(arguments));
 void *out=tb_exchange("/api/v1/tool",req,0,until);tj.put(req);return out;
}
#define LX_TB_CALL_LIMIT 64
#define LX_TB_BATCH_LIMIT 8
#define LX_TB_RESULT_LIMIT 8192
#define LX_TB_CACHE_LIMIT 98304
#ifndef LX_TB_CALL
#define LX_TB_CALL tb_call
#endif
struct tb_job {
 pthread_t thread;pthread_mutex_t lock;int active,done;char *event,*reply;char ticket[33];
 int end_session;int64_t ticket_ms;
 int music_touched;
 unsigned count;size_t cache_bytes;int cache_failed;char ids[LX_TB_CALL_LIMIT][129];char *cached[LX_TB_CALL_LIMIT];
};
static void *tb_worker(void *context){
 struct tb_job *j=context;void *e=tj.parse(j->event),*calls=NULL,*items=tj.array(),*response=tj.object();int64_t until=now_ms()+20000;int fatal_batch=0;size_t reply_budget=0;
 if(e&&tj.get(e,"items",&calls)&&tj.type(calls)==5){int n=tj.length(calls);if(n>64){fatal_batch=1;printf("tool_batch_rejected count=%d\n",n);fflush(stdout);n=0;}for(int i=0;i<n&&!g_stop;i++){
  void *call=tj.index(calls,i);const char *id=tb_s(call,"call_id"),*name=tb_s(call,"name"),*args=tb_s(call,"arguments");if(!tb_safe_id(id))continue;
  void *argument_value=NULL;int arguments_valid=tj.get(call,"arguments",&argument_value)&&tj.type(argument_value)==6&&args&&(size_t)tj.string_length(argument_value)==strlen(args)&&tb_arguments_valid(args);
  void *result=NULL;int did_execute=0;unsigned k;for(k=0;k<j->count;k++)if(!strcmp(id,j->ids[k]))break;
  if(!arguments_valid)result=tj.parse("{\"ok\":0,\"error\":\"invalid_tool_arguments\"}");
  else if(n>LX_TB_BATCH_LIMIT)result=tj.parse("{\"ok\":0,\"error\":\"tool_batch_too_large\"}");
  else if(k<j->count)result=tj.parse(j->cached[k]);
  else if(j->cache_failed)result=tj.parse("{\"ok\":0,\"error\":\"tool_cache_unavailable\"}");
  else if(j->count>=LX_TB_CALL_LIMIT||j->cache_bytes>LX_TB_CACHE_LIMIT-LX_TB_RESULT_LIMIT)result=tj.parse("{\"ok\":0,\"error\":\"tool_budget_exceeded\"}");
  else{
   int known=name&&tb_safe_id(name);if(known){did_execute=1;result=LX_TB_CALL(name,args,until);}
   if(!result)result=tj.parse("{\"ok\":0,\"error\":\"local_control_unavailable_or_timeout\"}");
   const char *ticket=tb_s(result,"play_ticket");if(ticket&&strlen(ticket)==32){snprintf(j->ticket,sizeof(j->ticket),"%s",ticket);j->ticket_ms=now_ms();tj.del(result,"play_ticket");}
   const char *act=tb_s(result,"action");if(act&&!strcmp(act,"session_end"))j->end_session=1;
   if(act&&(!strncmp(act,"music_",6)||!strncmp(act,"netease_",7)))j->music_touched=1;
   if(act&&(!strcmp(act,"music_pause")||!strcmp(act,"music_stop")||!strcmp(act,"music_next")||!strcmp(act,"music_previous"))){j->ticket[0]=0;j->ticket_ms=0;}
   if(strlen(tj.encode(result))>LX_TB_RESULT_LIMIT){tj.put(result);result=tj.parse("{\"ok\":0,\"error\":\"tool_result_too_large\",\"state_may_have_changed\":1}");}
   snprintf(j->ids[j->count],129,"%s",id);j->cached[j->count]=strdup(tj.encode(result));if(j->cached[j->count]){j->cache_bytes+=strlen(j->cached[j->count]);j->count++;}else j->cache_failed=1;
  }
  int recognized=tb_s(result,"action")!=NULL;
  size_t result_size=strlen(tj.encode(result));
  if(reply_budget+result_size*2+512>60000){void *summary=tj.object();tj.add(summary,"ok",tj.parse(tb_i(result,"ok")==1?"1":"0"));tj.add(summary,"result_truncated",tj.parse("1"));const char *original_error=tb_s(result,"error");if(original_error)tj.add(summary,"error",tj.str(original_error));tj.put(result);result=summary;result_size=strlen(tj.encode(result));}
  reply_budget+=result_size*2+512;
  void *item=tj.object(),*content=tj.array(),*text=tj.object();tj.add(item,"call_id",tj.str(id));tj.add(item,"role",tj.str("tool"));tj.add(text,"type",tj.str("input_text"));tj.add(text,"text",tj.str(tj.encode(result)));tj.append(content,text);tj.add(item,"content",content);tj.append(items,item);tj.put(result);
  /* Log only the allowlisted name and success, never model arguments. */
  printf(did_execute&&recognized?"tool_executed name=%s\n":"tool_result_without_execution name=%s\n",recognized&&name&&tb_safe_id(name)?name:"rejected");fflush(stdout);
 }}
 tj.add(response,"type",tj.str("conversation.item.create"));tj.add(response,"items",items);char *reply=fatal_batch?NULL:strdup(tj.encode(response));tj.put(response);if(e)tj.put(e);
 pthread_mutex_lock(&j->lock);j->reply=reply;j->done=1;pthread_mutex_unlock(&j->lock);return NULL;
}

