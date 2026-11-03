/* Accelerated clock: no audio files, network, or device are involved. */
#include <stdint.h>
static int64_t fake_now;
static int64_t test_now(void){return fake_now;}
#define LX_AGENT_NOW_MS test_now
#include "../src/native_agent_loop.h"
#include <assert.h>
struct mock {struct lx_agent_stats st;int step,scenario,busy;};
static struct lx_agent_stats *stats(void *c){return &((struct mock *)c)->st;}
static int audio(void *c,const short *p,int n){(void)c;(void)p;(void)n;return 0;}
static int json(void *c,const char *s){(void)c;(void)s;return 0;}
static int pump(void *c,int ms){(void)ms;struct mock *m=c;m->step++;fake_now+=1000;
 if(m->scenario==1||m->scenario==6){if(m->step==1)m->st.input_started++;if(m->step<=70)m->st.audio_delta++;if(m->step==71){m->st.done++;m->st.audio_done++;}}
 if(m->scenario==2||m->scenario==3){if(m->step==1)m->st.input_started++;if(m->scenario==3&&m->step<=70)m->st.transcript++;}
 if(m->scenario==4){if(m->step==1)m->st.audio_delta++;if(m->step==2)m->st.input_started++;if(m->step==3)m->st.canceled++;if(m->step==4)m->st.done++;}
 if(m->scenario==5){if(m->step==1)m->st.pending_call=malloc(3),memcpy(m->st.pending_call,"{}",3);if(m->step==17)m->st.audio_delta++;if(m->step==18)m->st.done++;}
 return 0;}
static int stop(void *c){(void)c;return 0;}
static long capture(void *c,short *p,int n){(void)c;memset(p,0,(size_t)n*sizeof(short));return n;}
static int recover(void *c,long e){(void)c;(void)e;return -1;}
static void play(void *c,const char *s){(void)c;(void)s;}
static int idle(void *c){struct mock *m=c;return m->scenario!=1||m->step>71;}
static void led(void *c,int s){(void)c;(void)s;}
static void publish(void *c,const char *s){(void)c;(void)s;}
static int start_tool(void *c,const char *s){(void)s;((struct mock *)c)->busy=1;return 0;}
static int busy(void *c){return ((struct mock *)c)->busy;}
static int take_tool(void *c,char *s,size_t n){struct mock *m=c;if(m->step<16)return 0;m->busy=0;snprintf(s,n,"{}");return 1;}
static int has_ticket(void *c){struct mock *m=c;return m->scenario==6&&m->step>0;}
static int no(void *c){(void)c;return 0;}
static void reset(void *c){(void)c;}
static int barge_check(void *c,const short *p,int n){(void)c;(void)p;(void)n;return 0;}
static struct lx_agent_result run(int scenario){fake_now=0;struct mock m={.scenario=scenario};struct lx_agent_provider p={&m,stats,audio,json,pump};struct lx_agent_host h={&m,stop,capture,recover,play,idle,led,publish,start_tool,busy,take_tool,has_ticket,no,reset,barge_check,reset};struct lx_agent_limits l={1800000,20000,20000,1,20000,2500,1};struct lx_agent_result r;assert(!lx_agent_run(&p,&h,&l,NULL,0,320,&r));return r;}
int main(void){struct lx_agent_result r=run(0);assert(!strcmp(r.reason,"no_speech")&&fake_now==21000);
 r=run(1);assert(!strcmp(r.reason,"followup_timeout")&&fake_now==92000&&r.turns==1);
 r=run(2);assert(!strcmp(r.reason,"no_response")&&fake_now==21000);
 r=run(3);assert(!strcmp(r.reason,"no_response")&&fake_now==90000);
 r=run(4);assert(!strcmp(r.reason,"no_response")&&fake_now==24000&&r.turns==0);
 r=run(5);assert(!strcmp(r.reason,"followup_timeout")&&fake_now==39000&&r.turns==1);
 r=run(6);assert(!strcmp(r.reason,"music_queued")&&fake_now==16000);
 puts("timeout_mock=pass active_output_over_60s=pass active_input_over_60s=pass no_progress_20s=pass idle_20s=pass late_canceled_done=pass tool_progress=pass music_confirmation_bounded=pass");return 0;}
