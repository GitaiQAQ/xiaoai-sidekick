/* Typed everyday speaker capabilities. No arbitrary ubus methods or shell. */
#ifndef LX06_CAPABILITIES_H
#define LX06_CAPABILITIES_H
#include <limits.h>
#include "native_lamp_policy.h"
#ifndef LX_LAMP_POLICY
#define LX_LAMP_POLICY "/data/lx06-agent/lamp-feedback-off"
#endif
#ifndef LX_CAP_TIME_NOW
#define LX_CAP_TIME_NOW() time(NULL)
#endif
static const char *cap_string(void *o,const char *key){void *v=NULL;if(!o||!mj.get(o,key,&v)||mj.type(v)!=6)return NULL;const char *s=mj.string(v);return s&&(size_t)mj.length(v)==strlen(s)?s:NULL;}
static int cap_number(void *o,const char *key,int *n){void *v=NULL;if(!o||!mj.get(o,key,&v)||mj.type(v)!=3)return 0;int64_t value=mj.integer64(v);if(value<INT_MIN||value>INT_MAX)return 0;*n=(int)value;return 1;}
static int cap_backend_code,cap_backend_transport,cap_backend_invalid;
static void cap_clear_errors(void){cap_backend_code=cap_backend_transport=cap_backend_invalid=0;}
static void *cap_error(const char *why){void *o=mj.object();add_i(o,"ok",0);add_s(o,"error",why);if(cap_backend_code)add_i(o,"backend_code",cap_backend_code);if(cap_backend_transport)add_i(o,"backend_transport_failed",1);if(cap_backend_invalid)add_i(o,"backend_reply_invalid",1);if(strstr(why,"readback")||!strcmp(why,"operation_not_confirmed")||!strcmp(why,"alarm_create_rejected"))add_i(o,"state_may_have_changed",1);return o;}
static void *cap_call(const char *object,const char *method,void *params){
 char raw[32768];const char *body=params?mj.encode(params):"{}";
 int bad=lx_music_ubus_object(object,method,body,raw,sizeof(raw));if(params)mj.put(params);if(bad){cap_backend_transport=1;return NULL;}
 void *root=mj.parse(raw),*info=NULL;int code=0;
 if(!root||mj.type(root)!=4){cap_backend_invalid=1;if(root)mj.put(root);return NULL;}
 int no_code_read=(!strcmp(object,"sound_effect")&&!strcmp(method,"get_mode"))||(!strcmp(object,"linein")&&(!strcmp(method,"line_status")||!strcmp(method,"play_status")))||(!strcmp(object,"led")&&!strcmp(method,"status"));
 if((!no_code_read&&!mj.get(root,"code",&info))||(mj.get(root,"code",&info)&&(!cap_number(root,"code",&code)||code))){if(cap_number(root,"code",&code)&&code)cap_backend_code=code;else cap_backend_invalid=1;mj.put(root);return NULL;}
 if(mj.get(root,"info",&info)){
  void *copy=NULL;
  if(mj.type(info)==6&&(size_t)mj.length(info)==strlen(mj.string(info))){
   if(*mj.string(info))copy=mj.parse(mj.string(info));
   else if(!strcmp(object,"led")&&!strcmp(method,"status")){copy=mj.object();mj.add(copy,"effect",mj.array());}
  }else if(mj.type(info)==4)copy=mj.parse(mj.encode(info));
  mj.put(root);return copy;
 }
 return root;
}
static int cap_bool(void *o,const char *key,int *out){void *v=NULL;if(!o||!mj.get(o,key,&v))return 0;
 if(mj.type(v)==1){*out=mj.integer(v)!=0;return 1;}
 if(mj.type(v)==6&&(size_t)mj.length(v)==strlen(mj.string(v))&&(!strcmp(mj.string(v),"true")||!strcmp(mj.string(v),"false"))){*out=!strcmp(mj.string(v),"true");return 1;}return 0;}
static void cap_int_copy(void *dst,void *src,const char *key){int n;if(src&&cap_number(src,key,&n))add_i(dst,key,n);}
static int cap_input_bool(void *o,const char *key,int *out){void *v=NULL;if(!o||!mj.get(o,key,&v)||mj.type(v)!=1)return 0;*out=mj.integer(v)!=0;return 1;}
static void *cap_music_snapshot(void){
 char raw[8192];if(lx_music_state(raw,sizeof(raw),NULL))return NULL;void *s=mj.parse(raw);int ok=0,status=-1,volume=-1,loop=-1;
 if(!s||!cap_number(s,"ok",&ok)||ok!=1||!cap_number(s,"status",&status)||status<0||status>3||!cap_number(s,"volume",&volume)||volume<0||volume>100||!cap_number(s,"loop_type",&loop)||loop<0){if(s)mj.put(s);return NULL;}
 void *clean=mj.object();add_i(clean,"ok",1);const char *ints[]={"status","volume","loop_type","position_ms","duration_ms","queue_index","queue_total"};for(unsigned i=0;i<7;i++)cap_int_copy(clean,s,ints[i]);
 const char *strings[]={"audio_id","title","artist","playlist"};for(unsigned i=0;i<4;i++){const char *text=cap_string(s,strings[i]);if(text&&strlen(text)<256)add_s(clean,strings[i],text);}mj.put(s);return clean;
}
static int cap_id_valid(const char *id);
static int cap_alarm_id(void *a,char id[65]){void *v=NULL;if(!a||!mj.get(a,"id",&v))return 0;if(mj.type(v)==6){const char *s=cap_string(a,"id");if(!cap_id_valid(s))return 0;snprintf(id,65,"%s",s);return 1;}if(mj.type(v)==3&&mj.integer64(v)>=0){snprintf(id,65,"%lld",(long long)mj.integer64(v));return 1;}return 0;}
static void *cap_alarm_raw(void){void *p=mj.object();add_s(p,"type","alarm");void *s=cap_call("alarm","alarm_query",p),*arr=NULL;if(!s||!mj.get(s,"alarm",&arr)||mj.type(arr)!=5){if(s)mj.put(s);return NULL;}for(int i=0;i<mj.array_length(arr);i++){void *a=mj.index(arr,i);char id[65];int timestamp,circle,status,volume;if(!cap_alarm_id(a,id)||!cap_number(a,"timestamp",&timestamp)||timestamp<0||!cap_number(a,"circle",&circle)||!cap_number(a,"status",&status)||(status!=0&&status!=1)||!cap_number(a,"volume",&volume)||volume<0||volume>100){mj.put(s);return NULL;}}return s;}
static void *cap_alarm_find(void *raw,const char *id){void *arr=NULL;if(!raw||!mj.get(raw,"alarm",&arr)||mj.type(arr)!=5)return NULL;
 for(int i=0;i<mj.array_length(arr);i++){void *a=mj.index(arr,i);char actual[65];if(cap_alarm_id(a,actual)&&!strcmp(actual,id))return a;}return NULL;}
static int cap_id_valid(const char *id){if(!id||!*id||strlen(id)>64)return 0;for(;*id;id++)if(!isalnum((unsigned char)*id)&&*id!='-'&&*id!='_')return 0;return 1;}
static void *cap_alarm_public(void *a){void *o=mj.object();char id[65];if(cap_alarm_id(a,id))add_s(o,"id",id);cap_int_copy(o,a,"timestamp");cap_int_copy(o,a,"circle");cap_int_copy(o,a,"volume");int status;if(cap_number(a,"status",&status))add_i(o,"enabled",status==0);return o;}
/* Exactly one time contract: epoch, local date+time, or relative minutes.
   mktime/localtime normalization rejects impossible dates and DST gaps. */
static int cap_alarm_time(void *obj,int *timestamp){
 void *v=NULL;int epoch=mj.get(obj,"timestamp",&v),relative=mj.get(obj,"after_minutes",&v),date=mj.get(obj,"local_date",&v),clock=mj.get(obj,"local_time",&v);
 if(epoch+relative+(date||clock)!=1||date!=clock)return 0;time_t now=LX_CAP_TIME_NOW();int64_t at=0;
 if(epoch){int value;if(!cap_number(obj,"timestamp",&value))return 0;at=value;}
 else if(relative){int minutes;if(!cap_number(obj,"after_minutes",&minutes)||minutes<1||minutes>525600)return 0;at=(int64_t)now+(int64_t)minutes*60;}
 else{
  const char *d=cap_string(obj,"local_date"),*t=cap_string(obj,"local_time");int y,m,day,h,minute;char extra;
  if(!d||!t||strlen(d)!=10||strlen(t)!=5||sscanf(d,"%4d-%2d-%2d%c",&y,&m,&day,&extra)!=3||sscanf(t,"%2d:%2d%c",&h,&minute,&extra)!=2||y<1970||m<1||m>12||day<1||day>31||h<0||h>23||minute<0||minute>59)return 0;
  for(int i=0;i<10;i++)if((i==4||i==7)?d[i]!='-':!isdigit((unsigned char)d[i]))return 0;
  if(!isdigit((unsigned char)t[0])||!isdigit((unsigned char)t[1])||t[2]!=':'||!isdigit((unsigned char)t[3])||!isdigit((unsigned char)t[4]))return 0;
  struct tm desired={0};desired.tm_year=y-1900;desired.tm_mon=m-1;desired.tm_mday=day;desired.tm_hour=h;desired.tm_min=minute;desired.tm_isdst=-1;
  time_t resolved=mktime(&desired);struct tm check;if(resolved<0||!localtime_r(&resolved,&check)||check.tm_year!=y-1900||check.tm_mon!=m-1||check.tm_mday!=day||check.tm_hour!=h||check.tm_min!=minute)return 0;at=(int64_t)resolved;
 }
 if(now<0||at<(int64_t)now+60||at>(int64_t)now+31536000||at>INT_MAX)return 0;*timestamp=(int)at;return 1;
}
/* Check model arguments before any action, including unexpected keys. The
   finite property set is shared by all reviewed tools, never a raw passthrough. */
static int cap_utf8_length(const char *s){int count=0;const unsigned char *p=(const unsigned char *)s;while(*p){int n=*p<0x80?1:(*p&0xe0)==0xc0?2:(*p&0xf0)==0xe0?3:(*p&0xf8)==0xf0?4:0;if(!n)return -1;unsigned cp=*p&((1u<<(7-n))-1);if(n==1)cp=*p;for(int k=1;k<n;k++){if((p[k]&0xc0)!=0x80)return -1;cp=(cp<<6)|(p[k]&0x3f);}if((n==2&&cp<0x80)||(n==3&&cp<0x800)||(n==4&&cp<0x10000)||cp>0x10ffff||(cp>=0xd800&&cp<=0xdfff))return -1;p+=n;count++;}return count;}
static int cap_tool_arguments_valid(const struct lx_tool_def *tool,void *args){
 if(!tool||!args||mj.type(args)!=4)return 0;void *schema=mj.parse(tool->params),*props=NULL,*required=NULL;
 if(!schema||!mj.get(schema,"properties",&props)||mj.type(props)!=4){if(schema)mj.put(schema);return 0;}
 int valid=1,seen=0;const char *keys[]={"query","id","title","artist","quality","value","position_ms","mode","offset","limit","local_date","local_time","after_minutes","timestamp","volume","enabled"};
 for(unsigned i=0;i<sizeof(keys)/sizeof(keys[0])&&valid;i++){
  void *value=NULL,*property=NULL,*field=NULL;if(!mj.get(args,keys[i],&value))continue;seen++;
  if(!mj.get(props,keys[i],&property)||mj.type(property)!=4){valid=0;break;}const char *type=cap_string(property,"type");
  if(!type){valid=0;break;}
  if(!strcmp(type,"integer")){int n;if(!cap_number(args,keys[i],&n)){valid=0;break;}int bound;if(cap_number(property,"minimum",&bound)&&n<bound)valid=0;if(cap_number(property,"maximum",&bound)&&n>bound)valid=0;}
  else if(!strcmp(type,"boolean")){int enabled;if(!cap_input_bool(args,keys[i],&enabled))valid=0;}
  else if(!strcmp(type,"string")){const char *text=cap_string(args,keys[i]);if(!text){valid=0;break;}int length=cap_utf8_length(text),bound;if(length<0)valid=0;if(cap_number(property,"minLength",&bound)&&length<bound)valid=0;if(cap_number(property,"maxLength",&bound)&&length>bound)valid=0;
   if(mj.get(property,"enum",&field)&&mj.type(field)==5){int matches=0;for(int j=0;j<mj.array_length(field);j++){void *candidate=mj.index(field,j);if(mj.type(candidate)==6&&!strcmp(mj.string(candidate),text))matches++;}if(!matches)valid=0;}
  }else valid=0;
 }
 if(seen!=mj.object_length(args))valid=0;
 if(mj.get(schema,"required",&required)&&mj.type(required)==5)for(int i=0;i<mj.array_length(required);i++){void *key=mj.index(required,i),*value=NULL;if(mj.type(key)!=6||!mj.get(args,mj.string(key),&value))valid=0;}
 mj.put(schema);return valid;
}

static void *cap_execute(const char *action,void *obj){
 cap_clear_errors();
 if(!strcmp(action,"speaker_status")){
  time_t now=LX_CAP_TIME_NOW();struct tm tm;char local[48],zone[40],offset[16];if(now<0||now>INT_MAX||!localtime_r(&now,&tm)||!strftime(local,sizeof(local),"%Y-%m-%dT%H:%M:%S%z",&tm)||!strftime(zone,sizeof(zone),"%Z",&tm)||!strftime(offset,sizeof(offset),"%z",&tm))return cap_error("device_time_unavailable");
  void *o=mj.object();add_i(o,"ok",1);add_s(o,"model","LX06");add_i(o,"unix_seconds",(int)now);add_s(o,"local_datetime",local);add_s(o,"timezone",zone);add_s(o,"utc_offset",offset);add_s(o,"timezone_source","system_localtime");add_i(o,"voice_running",agent_pid()!=0);
  FILE *f=fopen("/proc/uptime","r");double up=0;if(f){if(fscanf(f,"%lf",&up)==1)add_i(o,"uptime_seconds",up>2147483647?2147483647:(int)up);fclose(f);}
  f=fopen("/proc/meminfo","r");char line[160];if(f){while(fgets(line,sizeof(line),f)){unsigned n;if(sscanf(line,"MemAvailable: %u kB",&n)==1)add_i(o,"memory_available_kib",(int)n);}fclose(f);}return o;
 }
 if(!strcmp(action,"volume_get")||!strcmp(action,"music_mode_get")){
  void *state=cap_music_snapshot();int v=-1;const char *field=!strcmp(action,"volume_get")?"volume":"loop_type";
  if(!state||!cap_number(state,field,&v)){if(state)mj.put(state);return cap_error("state_unavailable");}
  void *o=mj.object();add_i(o,"ok",1);add_i(o,field,v);if(!strcmp(field,"loop_type"))add_s(o,"mode",v==1?"repeat_all":v==2?"sequential":"other");mj.put(state);return o;
 }
 if(!strcmp(action,"music_mode_set")){
  const char *mode=cap_string(obj,"mode");int v=mode&&!strcmp(mode,"repeat_all")?1:mode&&!strcmp(mode,"sequential")?2:-1;
  if(v<0)return cap_error("invalid_playback_mode");void *p=mj.object();add_i(p,"type",v);add_s(p,"media","common");void *ack=cap_call("mediaplayer","player_set_loop",p);if(!ack)return cap_error("operation_not_confirmed");mj.put(ack);
  void *state=cap_music_snapshot();int actual=-1;int ok=state&&cap_number(state,"loop_type",&actual)&&actual==v;if(state)mj.put(state);void *o=ok?mj.object():cap_error("mode_readback_mismatch");if(ok)add_i(o,"ok",1);add_i(o,"loop_type",actual);return o;
 }
 if(!strcmp(action,"music_seek")){
  int position=-1;void *state=cap_music_snapshot();int duration=-1,status=-1;
  if(!cap_number(obj,"position_ms",&position)||position<0||!state||!cap_number(state,"duration_ms",&duration)||duration<=0||position>=duration||!cap_number(state,"status",&status)||(status!=1&&status!=2)){if(state)mj.put(state);return cap_error("invalid_position_or_no_track");}
  const char *id=cap_string(state,"audio_id");char expected[80];if(!id||strlen(id)>=sizeof(expected)){mj.put(state);return cap_error("track_unavailable");}snprintf(expected,sizeof(expected),"%s",id);mj.put(state);
  void *p=mj.object();add_i(p,"position",position);add_s(p,"media","common");void *ack=cap_call("mediaplayer","player_set_positon",p);if(!ack)return cap_error("operation_not_confirmed");mj.put(ack);
  int got=-1,ok=0;for(int i=0;i<8;i++){state=cap_music_snapshot();id=state?cap_string(state,"audio_id"):NULL;if(state&&id&&!strcmp(id,expected)&&cap_number(state,"position_ms",&got)&&got>=0&&llabs((long long)got-position)<=2000)ok=1;if(state)mj.put(state);if(ok)break;usleep(100000);}
  void *o=ok?mj.object():cap_error("seek_readback_mismatch");if(ok)add_i(o,"ok",1);add_i(o,"position_ms",got);return o;
 }
 if(!strcmp(action,"sleep_timer_get")){
  void *s=cap_call("mediaplayer","get_shutdown_timer",NULL);int t=-1;if(!s||!cap_number(s,"type",&t)||t<0){if(s)mj.put(s);return cap_error("timer_state_unavailable");}void *o=mj.object();add_i(o,"ok",1);cap_int_copy(o,s,"type");cap_int_copy(o,s,"hour");cap_int_copy(o,s,"minute");cap_int_copy(o,s,"second");mj.put(s);return o;
 }
 if(!strcmp(action,"sound_effect_get")){
  void *s=cap_call("sound_effect","get_mode",NULL);int mode=-1,effect=-1;if(!s||!cap_number(s,"mode",&mode)||!cap_number(s,"effect",&effect)||mode<0||effect<0){if(s)mj.put(s);return cap_error("sound_effect_unavailable");}void *o=mj.object();add_i(o,"ok",1);add_i(o,"mode",mode);add_i(o,"effect",effect);mj.put(s);return o;
 }
 if(!strcmp(action,"alarm_volume_get")||!strcmp(action,"alarm_volume_set")){
  int value=-1;if(!strcmp(action,"alarm_volume_set")){if(!cap_number(obj,"value",&value)||value<0||value>100)return cap_error("invalid_volume");void *p=mj.object();add_i(p,"volume",value);add_i(p,"isPreview",0);void *ack=cap_call("alarm","alarm_set_volume",p);if(!ack)return cap_error("operation_not_confirmed");mj.put(ack);}
  void *s=cap_call("alarm","alarm_get_volume",NULL);int actual=-1;if(!s||!cap_number(s,"volume",&actual)){if(s)mj.put(s);return cap_error("alarm_volume_unavailable");}mj.put(s);void *o=value<0||value==actual?mj.object():cap_error("volume_readback_mismatch");if(value<0||value==actual)add_i(o,"ok",1);add_i(o,"volume",actual);return o;
 }
 if(!strcmp(action,"mesh_status")){
  void *s=cap_call("mibt_mesh","mesh_status",NULL);int support=0,enabled=0,running=0;
  if(!s||!cap_bool(s,"mesh_is_support",&support)||!cap_bool(s,"mesh_is_enable",&enabled)||!cap_bool(s,"mesh_is_running",&running)){if(s)mj.put(s);return cap_error("mesh_status_unavailable");}void *o=mj.object();add_i(o,"ok",1);add_i(o,"supported",support);add_i(o,"enabled",enabled);add_i(o,"running",running);mj.put(s);return o;
 }
 if(!strcmp(action,"bluetooth_status")){
  void *s=cap_call("mibt","status",NULL),*devices=NULL;int enabled=0,playing=-1,discover=-1;
  if(!s||!cap_bool(s,"enable",&enabled)||!cap_number(s,"playing",&playing)||!cap_number(s,"discover",&discover)||!mj.get(s,"devices_list",&devices)||mj.type(devices)!=5){if(s)mj.put(s);return cap_error("bluetooth_status_unavailable");}
  int total=mj.array_length(devices),count=total>64?64:total,connected=0;
  for(int i=0;i<count;i++){int yes=0;if(!cap_bool(mj.index(devices,i),"connect",&yes)){mj.put(s);return cap_error("bluetooth_status_unavailable");}connected+=yes;}
  void *o=mj.object();add_i(o,"ok",1);add_i(o,"enabled",enabled);add_i(o,"playing",playing);add_i(o,"discover",discover);add_i(o,"paired_count",total);add_i(o,"connected_count",connected);add_i(o,"connected_count_complete",count==total);add_i(o,"devices_checked",count);mj.put(s);return o;
 }
 if(!strcmp(action,"night_mode_status")){
  void *s=cap_call("nightmode","status",NULL);if(!s||!cap_string(s,"sleep_mode")||!cap_string(s,"total")||!cap_string(s,"light")||!cap_string(s,"volume")||!cap_string(s,"start")||!cap_string(s,"stop")){if(s)mj.put(s);return cap_error("night_mode_unavailable");}void *o=mj.object();add_i(o,"ok",1);const char *keys[]={"sleep_mode","total","light","volume","start","stop"};for(unsigned i=0;i<6;i++){const char *v=cap_string(s,keys[i]);if(!v||strlen(v)>=64){mj.put(o);mj.put(s);return cap_error("night_mode_unavailable");}add_s(o,keys[i],v);}mj.put(s);return o;
 }
 if(!strcmp(action,"line_in_status")){
  void *s=cap_call("linein","line_status",NULL),*v=NULL;int connected=-1;if(s&&mj.get(s,"line_status",&v)){if(mj.type(v)==3)connected=mj.integer(v);else if(mj.type(v)==6&&strlen(mj.string(v))==1&&(*mj.string(v)=='0'||*mj.string(v)=='1'))connected=*mj.string(v)-'0';}if(s)mj.put(s);s=cap_call("linein","play_status",NULL);int playing=-1;if(s)cap_number(s,"play_status",&playing);if(s)mj.put(s);if(connected<0||connected>1||playing<0)return cap_error("line_in_unavailable");void *o=mj.object();add_i(o,"ok",1);add_i(o,"connected",connected);add_i(o,"play_status",playing);return o;
 }
 if(!strcmp(action,"lamp_status")||!strcmp(action,"lamp_feedback_set")){
  int set=!strcmp(action,"lamp_feedback_set"),enabled=0;if(set&&!cap_input_bool(obj,"enabled",&enabled))return cap_error("invalid_enabled");
  void *s=cap_call("led","status",NULL),*effects=NULL;if(!s||!mj.get(s,"effect",&effects)||mj.type(effects)!=5){if(s)mj.put(s);return cap_error("lamp_status_unavailable");}
  int total=mj.array_length(effects);for(int i=0;i<total;i++){int type;if(!cap_number(mj.index(effects,i),"type",&type)){mj.put(s);return cap_error("lamp_status_unavailable");}}
  if(set){
   if(enabled){if(unlink(LX_LAMP_POLICY)&&errno!=ENOENT){mj.put(s);return cap_error("lamp_policy_write_failed");}}
   else{int fd=open(LX_LAMP_POLICY,O_WRONLY|O_CREAT|O_TRUNC|O_NOFOLLOW,0600);if(fd<0){mj.put(s);return cap_error("lamp_policy_write_failed");}int bad=write(fd,"0\n",2)!=2;close(fd);if(bad){mj.put(s);return cap_error("lamp_policy_write_failed");}}
   if(lx_lamp_feedback_enabled()!=enabled){mj.put(s);return cap_error("lamp_policy_readback_mismatch");}
  }
  void *o=mj.object(),*ids=mj.array();add_i(o,"ok",1);add_i(o,"feedback_enabled",lx_lamp_feedback_enabled());
  for(int i=0;i<total&&i<16;i++){int type;cap_number(mj.index(effects,i),"type",&type);mj.append(ids,mj.new_int(type));}
  add_i(o,"active_effects_total",total);add_i(o,"active_effects_complete",total<=16);mj.add(o,"active_effects",ids);mj.put(s);return o;
 }
 if(!strcmp(action,"alarm_list")){
  void *s=cap_alarm_raw(),*arr=NULL;if(!s||!mj.get(s,"alarm",&arr)||mj.type(arr)!=5){if(s)mj.put(s);return cap_error("alarm_state_unavailable");}int offset=0,limit=20;if(mj.get(obj,"offset",&arr)&&!cap_number(obj,"offset",&offset)){mj.put(s);return cap_error("invalid_offset");}if(mj.get(obj,"limit",&arr)&&!cap_number(obj,"limit",&limit)){mj.put(s);return cap_error("invalid_limit");}if(offset<0||offset>1000000||limit<1||limit>20){mj.put(s);return cap_error("invalid_pagination");}mj.get(s,"alarm",&arr);int total=mj.array_length(arr);void *o=mj.object(),*items=mj.array();for(int i=offset;i<total&&i<offset+limit;i++)mj.append(items,cap_alarm_public(mj.index(arr,i)));add_i(o,"ok",1);add_i(o,"total",total);add_i(o,"offset",offset);mj.add(o,"items",items);mj.put(s);return o;
 }
 if(!strcmp(action,"alarm_create")){
  int timestamp=0,volume=18;if(!cap_alarm_time(obj,&timestamp))return cap_error("invalid_alarm_time");void *v=NULL;if(mj.get(obj,"volume",&v)&&!cap_number(obj,"volume",&volume))return cap_error("invalid_volume");if(volume<0||volume>100)return cap_error("invalid_volume");
  void *before=cap_alarm_raw();if(!before)return cap_error("alarm_state_unavailable");
  void *p=mj.object();add_s(p,"type","alarm");add_i(p,"timestamp",timestamp);add_i(p,"circle",0);add_i(p,"volume",volume);add_s(p,"event","Sidekick alarm");add_s(p,"extra","");void *ack=cap_call("alarm","alarm_create",p);if(!ack){mj.put(before);return cap_error("alarm_create_rejected");}mj.put(ack);
  void *after=cap_alarm_raw(),*arr=NULL,*found=NULL;int matches=0;if(after&&mj.get(after,"alarm",&arr)&&mj.type(arr)==5)for(int i=0;i<mj.array_length(arr);i++){void *a=mj.index(arr,i);int at=-1;char id[65];if(cap_alarm_id(a,id)&&!cap_alarm_find(before,id)&&cap_number(a,"timestamp",&at)&&at==timestamp){found=a;matches++;}}
  void *o=matches==1?cap_alarm_public(found):cap_error("alarm_create_readback_unconfirmed");if(matches==1)add_i(o,"ok",1);if(after)mj.put(after);mj.put(before);return o;
 }
 if(!strcmp(action,"alarm_delete")||!strcmp(action,"alarm_enabled_set")){
  const char *id=cap_string(obj,"id");int enabled=0;if(!cap_id_valid(id))return cap_error("invalid_alarm_id");if(!strcmp(action,"alarm_enabled_set")&&!cap_input_bool(obj,"enabled",&enabled))return cap_error("invalid_enabled");void *before=cap_alarm_raw();if(!before)return cap_error("alarm_state_unavailable");if(!cap_alarm_find(before,id)){mj.put(before);return cap_error("alarm_not_found");}mj.put(before);
  void *p=mj.object();add_s(p,"type","alarm");add_s(p,"id",id);void *ack=cap_call("alarm",!strcmp(action,"alarm_delete")?"alarm_delete":enabled?"alarm_open":"alarm_close",p);if(!ack)return cap_error("operation_not_confirmed");mj.put(ack);
  void *after=cap_alarm_raw();if(!after)return cap_error("alarm_readback_unavailable");void *a=cap_alarm_find(after,id);int status=-1;int ok=!strcmp(action,"alarm_delete")?!a:a&&cap_number(a,"status",&status)&&status==(enabled?0:1);void *o=ok?mj.object():cap_error("alarm_readback_mismatch");if(ok){add_i(o,"ok",1);add_s(o,"id",id);if(strcmp(action,"alarm_delete"))add_i(o,"enabled",enabled);}mj.put(after);return o;
 }
 return NULL;
}
#endif
