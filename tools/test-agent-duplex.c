/* Synthetic session events: full/half, tools, barge-in and budgets. */
#include <unistd.h>
#include "../src/native_agent_loop.h"
#include <assert.h>
struct mock {struct lx_agent_stats st;int step,full,tools,barge,stop_at,busy,sent,answer_sent,tool_sent,mute,unmute,cancel,barge_calls;};
static struct lx_agent_stats *stats(void *c){return &((struct mock *)c)->st;}
static int send_audio(void *c,const short *p,int n){(void)p;(void)n;struct mock *m=c;m->sent++;if(m->step>=2&&m->step<6)m->answer_sent++;if(m->busy)m->tool_sent++;return 0;}
static int send_json(void *c,const char *s){struct mock *m=c;if(strstr(s,"\"input_audio_mute.commit\""))m->mute++;if(strstr(s,"input_audio_unmute"))m->unmute++;if(strstr(s,"response.cancel"))m->cancel++;return 0;}
static int pump(void *c,int ms){(void)ms;struct mock *m=c;usleep(2000);m->step++;if(m->step==2)m->st.audio_delta++;if(m->tools&&m->step==3)m->st.pending_call=strdup("{}");if(m->barge&&m->step==3){m->st.input_started++;m->st.transcript++;}if(m->barge&&m->step==4)m->st.canceled++;if(m->barge&&m->step==5)m->st.audio_delta++;if(m->step==6){m->st.audio_done++;m->st.done++;}return 0;}
static int stop(void *c){struct mock *m=c;return m->stop_at&&m->step>=m->stop_at;}
static long capture(void *c,short *p,int n){(void)c;usleep(1000);memset(p,0,(size_t)n*sizeof(short));return n;}
static int recover(void *c,long e){(void)c;(void)e;return -1;}
static void play(void *c,const char *s){(void)c;(void)s;}
static int idle(void *c){return ((struct mock *)c)->step>=6;}
static void led(void *c,int s){(void)c;(void)s;}
static void publish(void *c,const char *s){(void)c;(void)s;}
static int tool_start(void *c,const char *s){(void)s;((struct mock *)c)->busy=1;return 0;}
static int tool_busy(void *c){return ((struct mock *)c)->busy;}
static int tool_take(void *c,char *s,size_t cap){struct mock *m=c;if(m->step<5)return 0;m->busy=0;snprintf(s,cap,"{}");return 1;}
static int no(void *c){(void)c;return 0;}
static void reset(void *c){(void)c;}
static int barge_check(void *c,const short *p,int n){(void)c;(void)p;(void)n;return 0;}
static void barge(void *c){((struct mock *)c)->barge_calls++;}
static struct lx_agent_result run(struct mock *m,int budget,int followup){
 struct lx_agent_provider p={m,stats,send_audio,send_json,pump};struct lx_agent_host h={m,stop,capture,recover,play,idle,led,publish,tool_start,tool_busy,tool_take,no,no,reset,barge_check,barge};struct lx_agent_limits l={budget,followup,1000,1,0,2500,m->full};struct lx_agent_result r;
 assert(lx_agent_run(&p,&h,&l,NULL,0,320,&r)==0);return r;
}
int main(void){
 struct mock f={.full=1,.tools=1,.stop_at=12};run(&f,1000,1000);assert(f.answer_sent>0&&f.tool_sent>0&&f.mute==0&&f.unmute==0);
 struct mock h={.tools=1,.stop_at=12};run(&h,1000,1000);assert(h.answer_sent==0&&h.tool_sent==0&&h.mute>0);
 struct mock b={.full=1,.barge=1,.stop_at=12};run(&b,1000,1000);assert(b.cancel==1&&b.barge_calls==1&&b.mute==0);
 struct mock t={.full=1};struct lx_agent_result r=run(&t,15,1000);assert(!strcmp(r.reason,"max_duration"));
 struct mock e={.full=1};r=run(&e,1000,0);assert(!strcmp(r.reason,"answer_complete")&&r.turns==1);
 puts("duplex_mock=pass full_answer_upload=pass tool_upload=pass no_mute=pass half_regression=pass barge=pass max_duration=pass answer_complete=pass");return 0;
}
