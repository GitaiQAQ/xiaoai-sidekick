#include "test-native-json.m.h"
static NSMutableDictionary *fixtures,*player;
static NSMutableArray *alarms;
static int calls,mutations,transport_fail,suppress_update;
static char mock_policy_path[256];
static time_t mock_time=1735689600;
#define LX_LAMP_POLICY mock_policy_path
#define LX_CAP_TIME_NOW() mock_time
static int lx_music_state(char *out,size_t cap,int *status){snprintf(out,cap,"%s",jencode(player));if(status)*status=[player[@"status"] intValue];return 0;}
static int lx_music_ubus_object(const char *object,const char *method,const char *body,char *out,size_t cap){
 calls++;if(transport_fail)return 1;void *params=jparse(body);NSString *key=[NSString stringWithFormat:@"%s.%s",object,method];id result=fixtures[key];
 if(!strcmp(method,"alarm_query"))result=@{@"code":@0,@"info":@{@"alarm":alarms}};
 else if(!strcmp(method,"player_set_loop")){mutations++;if(!suppress_update)player[@"loop_type"]=node(params)[@"type"];result=@{@"code":@0};}
 else if(!strcmp(method,"player_set_positon")){mutations++;if(!suppress_update)player[@"position_ms"]=node(params)[@"position"];result=@{@"code":@0};}
 else if(!strcmp(method,"alarm_set_volume")){mutations++;if(!suppress_update)fixtures[@"alarm.alarm_get_volume"]=@{@"code":@0,@"info":@{@"volume":node(params)[@"volume"]}};result=@{@"code":@0};}
 else if(!strcmp(method,"alarm_create")){mutations++;if(!suppress_update)[alarms addObject:[@{@"id":@"mock-new",@"timestamp":node(params)[@"timestamp"],@"circle":@0,@"status":@0,@"volume":node(params)[@"volume"]}mutableCopy]];result=@{@"code":@0,@"info":@{@"baby_course":@[]}};}
 else if(!strcmp(method,"alarm_delete")||!strcmp(method,"alarm_open")||!strcmp(method,"alarm_close")){mutations++;if(!suppress_update){NSString *ident=node(params)[@"id"];for(NSMutableDictionary *a in [NSArray arrayWithArray:alarms])if([a[@"id"] isEqual:ident]){if(!strcmp(method,"alarm_delete"))[alarms removeObject:a];else a[@"status"]=!strcmp(method,"alarm_open")?@0:@1;}}result=@{@"code":@0};}
 if(!result)result=@{};snprintf(out,cap,"%s",jencode(result));jput(params);return 0;
}
#include "../src/native_capabilities.h"
static unsigned checks;
static void require(int condition){checks++;assert(condition);}
static void *run(const char *action,const char *args){void *p=jparse(args);require(p!=NULL);void *out=cap_execute(action,p);jput(p);return out;}
static int ok(void *o){void *v=NULL;return o&&jget(o,"ok",&v)&&jint(v)==1;}
static void expect(const char *action,const char *args,int good){void *o=run(action,args);require(ok(o)==good);jput(o);}
static void reset(void){calls=mutations=transport_fail=suppress_update=0;fixtures=[[NSMutableDictionary alloc]init];player=[@{@"ok":@1,@"status":@1,@"volume":@70,@"loop_type":@1,@"position_ms":@10000,@"duration_ms":@120000,@"audio_id":@"mock-track"}mutableCopy];alarms=[[NSMutableArray alloc]initWithObjects:[@{@"id":@"mock-original",@"timestamp":@1735693200,@"circle":@0,@"status":@0,@"volume":@18,@"event":@"private label must not be returned"}mutableCopy],nil];fixtures[@"mediaplayer.get_shutdown_timer"]=@{@"code":@0,@"info":@"{\"type\":0}"};fixtures[@"sound_effect.get_mode"]=@{@"mode":@0,@"effect":@1};fixtures[@"alarm.alarm_get_volume"]=@{@"code":@0,@"info":@{@"volume":@18}};fixtures[@"nightmode.status"]=@{@"code":@0,@"info":@{@"sleep_mode":@"night",@"total":@"night",@"light":@"night",@"volume":@"night",@"start":@"22:00",@"stop":@"07:00"}};fixtures[@"mibt_mesh.mesh_status"]=@{@"code":@0,@"info":@{@"mesh_is_support":@YES,@"mesh_is_enable":@YES,@"mesh_is_running":@YES}};fixtures[@"mibt.status"]=@{@"code":@0,@"info":@{@"enable":@"true",@"playing":@0,@"discover":@0,@"devices_list":@[@{@"connect":@"true",@"bdaddr":@"must-not-leak"}]}};fixtures[@"linein.line_status"]=@{@"line_status":@"0"};fixtures[@"linein.play_status"]=@{@"play_status":@0};fixtures[@"led.status"]=@{@"info":@""};}
int main(void){@autoreleasepool{
 char dir[]="/tmp/lx06-capability-tests.XXXXXX";require(mkdtemp(dir)!=NULL);snprintf(mock_policy_path,sizeof(mock_policy_path),"%s/off",dir);setenv("TZ","Asia/Shanghai",1);tzset();reset();
 const char *reads[]={"speaker_status","volume_get","music_mode_get","sleep_timer_get","sound_effect_get","alarm_volume_get","mesh_status","bluetooth_status","night_mode_status","line_in_status","lamp_status","alarm_list"};for(unsigned i=0;i<sizeof(reads)/sizeof(reads[0]);i++)expect(reads[i],"{}",1);
 player[@"url"]=@"must-not-leak";void *snapshot=cap_music_snapshot();require(strstr(jencode(snapshot),"must-not-leak")==NULL);jput(snapshot);
 void *o=run("speaker_status","{}");require(strstr(jencode(o),"+0800")!=NULL);require(strstr(jencode(o),"unix_seconds")!=NULL);jput(o);
 o=run("bluetooth_status","{}");require(strstr(jencode(o),"must-not-leak")==NULL);jput(o);o=run("alarm_list","{}");require(strstr(jencode(o),"private label")==NULL);jput(o);
 const char *missing_keys[]={"nightmode.status","sound_effect.get_mode","mibt.status","mibt_mesh.mesh_status","linein.line_status","led.status","mediaplayer.get_shutdown_timer","alarm.alarm_get_volume"};const char *missing_actions[]={"night_mode_status","sound_effect_get","bluetooth_status","mesh_status","line_in_status","lamp_status","sleep_timer_get","alarm_volume_get"};for(unsigned i=0;i<8;i++){id previous=fixtures[@(missing_keys[i])];fixtures[@(missing_keys[i])]=@{};expect(missing_actions[i],"{}",0);fixtures[@(missing_keys[i])]=previous;}
 fixtures[@"nightmode.status"]=@{@"code":@1002};void *failed=run("night_mode_status","{}");require([node(failed)[@"backend_code"] intValue]==1002);jput(failed);
 fixtures[@"nightmode.status"]=@{@"code":@"0",@"info":@{}};expect("night_mode_status","{}",0);fixtures[@"nightmode.status"]=@{@"code":@0,@"info":@{}};expect("night_mode_status","{}",0);
 fixtures[@"mibt_mesh.mesh_status"]=@{@"mesh_is_support":@YES,@"mesh_is_enable":@YES,@"mesh_is_running":@YES};expect("mesh_status","{}",0);fixtures[@"mibt_mesh.mesh_status"]=@{@"code":@0,@"info":@{@"mesh_is_support":@YES,@"mesh_is_enable":@YES,@"mesh_is_running":@YES}};
 fixtures[@"led.status"]=@{};expect("lamp_feedback_set","{\"enabled\":false}",0);require(lx_lamp_feedback_enabled());fixtures[@"led.status"]=@{@"info":@""};
 transport_fail=1;expect("mesh_status","{}",0);expect("alarm_volume_set","{\"value\":20}",0);transport_fail=0;
 expect("music_mode_set","{\"mode\":\"sequential\"}",1);suppress_update=1;expect("music_mode_set","{\"mode\":\"repeat_all\"}",0);suppress_update=0;
 expect("music_seek","{\"position_ms\":30000}",1);suppress_update=1;expect("music_seek","{\"position_ms\":40000}",0);suppress_update=0;
 expect("music_seek","{\"position_ms\":120000}",0);expect("music_seek","{\"position_ms\":-1}",0);expect("alarm_volume_set","{\"value\":101}",0);expect("alarm_volume_set","{\"value\":2147483648}",0);expect("alarm_volume_set","{\"value\":true}",0);expect("alarm_volume_set","{\"value\":22}",1);
 expect("alarm_create","{\"after_minutes\":120}",1);expect("alarm_enabled_set","{\"id\":\"mock-new\",\"enabled\":false}",1);expect("alarm_enabled_set","{\"id\":\"mock-new\",\"enabled\":true}",1);expect("alarm_delete","{\"id\":\"mock-new\"}",1);require(alarms.count==1);
 suppress_update=1;expect("alarm_create","{\"after_minutes\":120}",0);expect("alarm_enabled_set","{\"id\":\"mock-original\",\"enabled\":false}",0);expect("alarm_delete","{\"id\":\"mock-original\"}",0);suppress_update=0;require(alarms.count==1);
 expect("alarm_delete","{\"id\":\"absent\"}",0);expect("alarm_enabled_set","{\"id\":\"mock-original\",\"enabled\":\"true\"}",0);expect("alarm_delete","{\"id\":\"mock-original\\u0000other\"}",0);
 expect("alarm_list","{\"offset\":2147483647}",0);expect("alarm_list","{\"limit\":21}",0);
 int at=0;void *p=jparse("{\"local_date\":\"2025-01-02\",\"local_time\":\"07:00\"}");require(cap_alarm_time(p,&at));require(at==1735772400);jput(p);
 const char *bad_times[]={"{}","{\"after_minutes\":0}","{\"after_minutes\":1,\"timestamp\":1735772400}","{\"timestamp\":1735689600}","{\"local_date\":\"2025-01-02\"}","{\"local_date\":\"2025-02-30\",\"local_time\":\"07:00\"}","{\"local_date\":\"2025-01-02\",\"local_time\":\"24:00\"}","{\"after_minutes\":525601}"};for(unsigned i=0;i<8;i++){p=jparse(bad_times[i]);require(!cap_alarm_time(p,&at));jput(p);}
 setenv("TZ","America/New_York",1);tzset();p=jparse("{\"local_date\":\"2025-03-09\",\"local_time\":\"02:30\"}");require(!cap_alarm_time(p,&at));jput(p);setenv("TZ","Asia/Shanghai",1);tzset();
 expect("lamp_feedback_set","{\"enabled\":false}",1);require(!lx_lamp_feedback_enabled());expect("lamp_feedback_set","{\"enabled\":true}",1);require(lx_lamp_feedback_enabled());expect("lamp_feedback_set","{\"enabled\":\"true\"}",0);
 NSMutableArray *many=[NSMutableArray array];for(int i=0;i<70;i++)[many addObject:@{@"connect":@YES}];fixtures[@"mibt.status"]=@{@"code":@0,@"info":@{@"enable":@YES,@"playing":@0,@"discover":@0,@"devices_list":many}};o=run("bluetooth_status","{}");require([node(o)[@"paired_count"] intValue]==70);require([node(o)[@"connected_count_complete"] intValue]==0);require([node(o)[@"devices_checked"] intValue]==64);jput(o);
 char catalog[32768];require(lx_tools_json(LX_TOOL_VOICE,catalog,sizeof(catalog))>0);require(lx_tools_json(LX_TOOL_VOICE,catalog,128)<0);require(lx_tool_action_allowed("alarm_delete",LX_TOOL_VOICE));require(!lx_tool_action_allowed("voice_start",LX_TOOL_VOICE));
 const struct lx_tool_def *tool=lx_tool_find("speaker_volume_set",LX_TOOL_VOICE);p=jparse("{\"value\":40}");require(cap_tool_arguments_valid(tool,p));jput(p);const char *bad_args[]={"{}","{\"value\":101}","{\"value\":true}","{\"value\":3.5}","{\"value\":40,\"unknown\":1}"};for(unsigned i=0;i<5;i++){p=jparse(bad_args[i]);require(!cap_tool_arguments_valid(tool,p));jput(p);}
 p=jparse("{\"mode\":\"random\"}");require(!cap_tool_arguments_valid(lx_tool_find("music_mode_set",LX_TOOL_VOICE),p));jput(p);p=jparse("{\"query\":\"test\\u0000hidden\"}");require(!cap_tool_arguments_valid(lx_tool_find("music_search",LX_TOOL_VOICE),p));jput(p);
 p=jparse("{\"query\":\"你好世界你好世界你好世界你好世界你好世界你好世界你好世界你好世界你好世界你好世界\"}");require(cap_tool_arguments_valid(lx_tool_find("music_search",LX_TOOL_VOICE),p));jput(p);
 /* Models often send track IDs as JSON integers; digit-pattern string fields
    normalize to strings so dispatch sees the same shape as string calls. */
 const struct lx_tool_def *play=lx_tool_find("music_play",LX_TOOL_VOICE);
 p=jparse("{\"id\":1412022124}");require(cap_tool_arguments_valid(play,p));require([node(p)[@"id"] isEqual:@"1412022124"]);jput(p);
 p=jparse("{\"id\":\"1412022124\"}");require(cap_tool_arguments_valid(play,p));jput(p);
 p=jparse("{\"id\":1412022124,\"quality\":\"standard\"}");require(cap_tool_arguments_valid(play,p));jput(p);
 const char *bad_ids[]={"{\"id\":0}","{\"id\":-1}","{\"id\":1.5}","{\"id\":true}","{\"id\":1412022124,\"album\":\"x\"}","{\"id\":1412022124,\"quality\":\"bogus\"}","{\"quality\":\"standard\"}"};for(unsigned i=0;i<7;i++){p=jparse(bad_ids[i]);require(!cap_tool_arguments_valid(play,p));jput(p);}
 p=jparse("{\"query\":123}");require(!cap_tool_arguments_valid(lx_tool_find("music_search",LX_TOOL_VOICE),p));jput(p);
 unlink(mock_policy_path);rmdir(dir);printf("speaker_capabilities_mock=pass checks=%u network_connections=0 device_operations=0\n",checks);
 }return 0;}
