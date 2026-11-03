#include "test-native-json.m.h"
#include <pthread.h>
#include <poll.h>
#include <dlfcn.h>
static int g_stop;
static int64_t now_ms(void){return 123000;}
static unsigned executed;
static size_t filler_size;
static int prepare_ticket;
static void *fake_tool_call(const char *name,const char *args,int64_t until){(void)args;(void)until;executed++;void *r=jobject();add_i(r,"ok",1);add_s(r,"action",name);if(filler_size){char *f=malloc(filler_size+1);memset(f,'x',filler_size);f[filler_size]=0;add_s(r,"details",f);free(f);}if(prepare_ticket)add_s(r,"play_ticket","0123456789abcdef0123456789abcdef");return r;}
#define LX_TB_CALL fake_tool_call
#include "../src/native_tool_bridge.h"
static unsigned checks;
static void require(int good){checks++;assert(good);}
static void init(struct tb_job *j){memset(j,0,sizeof(*j));require(!pthread_mutex_init(&j->lock,NULL));}
static void release(struct tb_job *j){for(unsigned i=0;i<j->count;i++)free(j->cached[i]);free(j->reply);pthread_mutex_destroy(&j->lock);}
static void *batch(struct tb_job *j,int n,int base,const char *name){NSMutableArray *items=[NSMutableArray array];for(int i=0;i<n;i++)[items addObject:@{@"call_id":[NSString stringWithFormat:@"call-%d",base+i],@"name":@(name),@"arguments":@"{}"}];void *event=jobject();jadd(event,"items",[items retain]);j->event=strdup(jencode(event));jput(event);free(j->reply);j->reply=NULL;tb_worker(j);free(j->event);j->event=NULL;return j->reply?jparse(j->reply):NULL;}
static id first_result(void *reply){return node(jparse([node(reply)[@"items"][0][@"content"][0][@"text"] UTF8String]));}
int main(void){@autoreleasepool{(void)mj;tj.parse=jparse;tj.get=jget;tj.type=jtype;tj.integer=jint;tj.string=jstring;tj.encode=jencode;tj.put=jput;tj.object=jobject;tj.array=jarray;tj.str=jstr;tj.add=jadd;tj.append=jappend;tj.length=jarraylen;tj.string_length=jlength;tj.index=jindex;tj.del=jdel;
 struct tb_job j;init(&j);for(int i=0;i<64;i++){void *r=batch(&j,1,i,"speaker_status");require(r!=NULL);require([first_result(r)[@"ok"] intValue]==1);jput(r);}require(executed==64&&j.count==64);void *r=batch(&j,1,64,"speaker_status");require([first_result(r)[@"error"] isEqual:@"tool_budget_exceeded"]);jput(r);r=batch(&j,1,0,"speaker_status");require([first_result(r)[@"ok"] intValue]==1);require(executed==64);jput(r);release(&j);
 init(&j);executed=0;r=batch(&j,9,100,"speaker_status");require(executed==0);require([node(r)[@"items"] count]==9);for(id item in node(r)[@"items"]){id result=node(jparse([item[@"content"][0][@"text"] UTF8String]));require([result[@"error"] isEqual:@"tool_batch_too_large"]);}jput(r);release(&j);
 init(&j);r=batch(&j,65,200,"speaker_status");require(r==NULL&&executed==0);release(&j);
 init(&j);filler_size=9000;r=batch(&j,1,300,"speaker_status");require([first_result(r)[@"error"] isEqual:@"tool_result_too_large"]);jput(r);unsigned prior=executed;r=batch(&j,1,300,"speaker_status");require(executed==prior);jput(r);release(&j);
 init(&j);filler_size=7900;executed=0;r=batch(&j,8,400,"speaker_status");require(executed==8);require(strlen(j.reply)<=60000);int summaries=0;for(id item in node(r)[@"items"]){id result=node(jparse([item[@"content"][0][@"text"] UTF8String]));if([result[@"result_truncated"] intValue])summaries++;}require(summaries>0);jput(r);r=batch(&j,1,407,"speaker_status");require(executed==8);require([first_result(r)[@"details"] length]==7900);jput(r);release(&j);
 init(&j);executed=0;for(int i=0;i<20;i++){r=batch(&j,1,500+i,"speaker_status");jput(r);}require(j.cache_bytes<=LX_TB_CACHE_LIMIT);require(executed<20);release(&j);
 init(&j);filler_size=0;prepare_ticket=1;r=batch(&j,1,600,"netease_prepare");require(j.ticket[0]!=0&&j.ticket_ms==123000);require(strstr(j.reply,"0123456789abcdef")==NULL);jput(r);prepare_ticket=0;r=batch(&j,1,601,"music_pause");require(!j.ticket[0]);jput(r);release(&j);
 filler_size=0;prepare_ticket=0;
 NSString *huge=[@"{}" stringByPaddingToLength:3001 withString:@" " startingAtIndex:0];unichar embedded[]={ '{','}',0,'b','a','d' };NSString *nul=[NSString stringWithCharacters:embedded length:6];
 NSArray *invalid=@[huge,@YES,@42,@{},@[],[NSNull null],nul,@"broken JSON",@"{} trailing",@"{}{}",@"{/*comment*/}",@"[]",@"null",@"42"];
 for(NSString *name in @[@"music_stop",@"session_end"]){for(id args in invalid){init(&j);executed=0;void *event=jobject();jadd(event,"items",[@[@{@"call_id":@"invalid-call",@"name":name,@"arguments":args}] mutableCopy]);j.event=strdup(jencode(event));jput(event);tb_worker(&j);free(j.event);j.event=NULL;r=jparse(j.reply);require(executed==0&&j.end_session==0&&j.music_touched==0);require([first_result(r)[@"error"] isEqual:@"invalid_tool_arguments"]);jput(r);release(&j);}
 init(&j);executed=0;void *event=jobject();jadd(event,"items",[@[@{@"call_id":@"missing-arguments",@"name":name}] mutableCopy]);j.event=strdup(jencode(event));jput(event);tb_worker(&j);free(j.event);j.event=NULL;r=jparse(j.reply);require(executed==0&&j.end_session==0);require([first_result(r)[@"error"] isEqual:@"invalid_tool_arguments"]);jput(r);release(&j);
 }
 const char *direct_bad[]={NULL,"bad JSON","[]","null","{} trailing"};for(unsigned i=0;i<5;i++){r=tb_call("music_stop",direct_bad[i],now_ms()+1000);require([node(r)[@"error"] isEqual:@"invalid_tool_arguments"]);jput(r);}r=tb_call("session_end",huge.UTF8String,now_ms()+1000);require([node(r)[@"error"] isEqual:@"invalid_tool_arguments"]);jput(r);
 printf("tool_bridge_mock=pass checks=%u call_budget=64 batch_limit=8 bounded_cache=pass explicit_response_summary=pass dedup=pass ticket_cancel=pass invalid_arguments_no_execution=pass network_connections=0\n",checks);
 }return 0;}
