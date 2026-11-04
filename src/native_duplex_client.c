/* Minimal in-device duplex client: mic -> Doubao realtime -> speaker.
   Reuses factory libcurl/OpenSSL/json-c and libasound via dlopen.
   Capture uses the dsnoop PCM "mico_record", so the vendor stack keeps running.

   Modes:
     ld-linux lib.so /tmp/client <seconds>          direct duplex session
     ld-linux lib.so /tmp/client --tone             playback only
     ld-linux lib.so /tmp/client --kws-score <wav>  offline wake word scoring
     ld-linux lib.so /tmp/client --kws-dump <wav>   same, plus raw features
     ld-linux lib.so /tmp/client --kws-live <sec>   local detection only, no network
     ld-linux lib.so /tmp/client --kws-capture <sec> in-memory negative sample
     ld-linux lib.so /tmp/client --kws-wav <wav>    replay a WAV through the detector
     ld-linux lib.so /tmp/client --kws <sec>        standby, session after wake,
                                                    then back to standby
   The API key is read from stdin for the network modes only, after a wake. */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <pthread.h>
#include <poll.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <curl/curl.h>
#include "native_websocket.h"
#include "native_kws.h"
#include "native_session_policy.h"
#include "native_agent_loop.h"
#include "native_capture.h"
#include "native_lamp_policy.h"
#include "native_wake_handoff.h"
#include "native_factory_kws.h"

#define RATE_IN 16000
#define RATE_OUT 24000
#define FRAME_IN 320
#define MAX_MESSAGE 65536

enum { MODE_DUPLEX, MODE_TONE, MODE_SCORE, MODE_LIVE, MODE_KWS, MODE_CAPTURE, MODE_KWS_WAV, MODE_WAKE_AUDIT };

struct curl_api {
    CURLcode (*global_init)(long);
    void (*global_cleanup)(void);
    CURL *(*init)(void);
    CURLcode (*setopt)(CURL *,CURLoption,...);
    CURLcode (*perform)(CURL *);
    CURLcode (*getinfo)(CURL *,CURLINFO,...);
    void (*cleanup)(CURL *);
    CURLcode (*send)(CURL *,const void *,size_t,size_t *);
    CURLcode (*recv)(CURL *,void *,size_t,size_t *);
};
struct alsa {
    void *lib;
    int (*open)(void **,const char *,int,int);
    int (*set_params)(void *,int,int,unsigned,unsigned,int,unsigned);
    long (*readi)(void *,void *,unsigned long);
    long (*writei)(void *,const void *,unsigned long);
    int (*recover)(void *,int,int);
    int (*drain)(void *);
    int (*delay)(void *,long *);
    int (*drop)(void *);
    int (*prepare)(void *);
    int (*close)(void *);
    const char *(*strerror)(int);
};
struct ring {
    unsigned char *data;size_t cap,head,tail,used;
    pthread_mutex_t lock;pthread_cond_t ready;int stop,inflight;
};
struct client {
    struct curl_api *a;CURL *c;int socket;int64_t deadline,setup_ms,connect_done_ms,upgrade_ms,ready_ms,input_ms;
    int (*random_bytes)(unsigned char *,int);
    int (*b64_encode)(unsigned char *,const unsigned char *,int);
    int (*b64_decode)(unsigned char *,const unsigned char *,int);
    void *(*json_parse)(const char *);
    int (*json_get)(void *,const char *,void **);
    const char *(*json_string)(void *);
    int (*json_type)(void *);
    void (*json_put)(void *);
    struct ring *ring;
    struct lx_agent_stats st;
};

static struct alsa g_alsa;
static void *g_play;
static long g_written;static int g_werrs;static int g_apeak;static long g_asamples;
static int g_answer_gain=250; /* percent applied to streamed answer PCM */
static int g_suppress_audio;  /* drop the rest of an interrupted reply */
/* Every session is bounded, and a signal closes the current one instead of
   leaving the socket streaming. */
static volatile sig_atomic_t g_stop;
static int g_session_max_ms=1800000;
static int g_followup_ms=20000;
static int g_feedback_test;
static int g_led_owned,g_led_requested;
static int g_no_speech_ms=20000;
static int g_no_response_ms=20000;
static int g_voice_gate=2500;
static int64_t g_wake_started_ms;
#ifndef LX06_DEFAULT_FULL_DUPLEX
#define LX06_DEFAULT_FULL_DUPLEX 1
#endif
static int g_full_duplex=LX06_DEFAULT_FULL_DUPLEX;
/* Explicit synthetic fixture mode only: never opens microphone capture. */
static short *g_session_fixture;
static int g_fixture_live_tail;
static char g_fixture_reply[1024];static size_t g_fixture_reply_n;
static int g_listen_once;
static void on_signal(int sig){(void)sig;g_stop=1;}

static int64_t now_ms(void) {
    struct timespec t;
    if(clock_gettime(CLOCK_MONOTONIC,&t))return -1;
    return (int64_t)t.tv_sec*1000+t.tv_nsec/1000000;
}
#include "native_tool_bridge.h"
static const char *g_daemon_pidfile;
/* Small, atomic runtime snapshot for the local console. No speech content. */
static void publish_state(const char *state){
    if(!g_daemon_pidfile)return;
    FILE *f=fopen("/tmp/lx06-agent-state.json.tmp","w");if(!f)return;
    fprintf(f,"{\"pid\":%d,\"state\":\"%s\"}\n",(int)getpid(),state);
    fclose(f);chmod("/tmp/lx06-agent-state.json.tmp",0600);
    rename("/tmp/lx06-agent-state.json.tmp","/tmp/lx06-agent-state.json");
}
/* Detach into a new session so closing the SSH channel cannot deliver SIGHUP.
   The parent returns to the caller, which exits; the child keeps stdin (the
   FIFO that will deliver the API key on first wake). */
static int daemonize(void) {
    pid_t pid;
    FILE *f;
    fflush(NULL);
    pid=fork();
    if(pid<0)return -1;
    if(pid>0)return 1;
    if(setsid()<0)return -1;
    if(chdir("/")){}
    umask(077);
    f=fopen(g_daemon_pidfile,"w");
    if(!f)return -1;
    fprintf(f,"%d\n",(int)getpid());
    fclose(f);
    chmod(g_daemon_pidfile,0600);
    return 0;
}
static int ws_wait(struct client *w,int sending) {
    int64_t now=now_ms(),remaining=w->deadline-now;
    struct pollfd p={w->socket,sending?POLLOUT:POLLIN,0};
    if(now<0||remaining<=0)return 1;
    return poll(&p,1,remaining>50?50:(int)remaining)<0;
}
static int ws_send_all(struct client *w,const unsigned char *p,size_t n) {
    while(n) {
        size_t sent=0;CURLcode code;
        int64_t now=now_ms();if(now<0||now>=w->deadline)return 1;
        code=w->a->send(w->c,p,n,&sent);
        if(code==CURLE_AGAIN){if(ws_wait(w,1))return 1;continue;}
        if(code||!sent)return 1;p+=sent;n-=sent;
    }return 0;
}
static int ws_send_frame(struct client *w,unsigned op,const unsigned char *p,size_t n) {
    unsigned char mask[4],small[2048],*frame=small;size_t bytes,cap=sizeof(small);
    if(n>MAX_MESSAGE||w->random_bytes(mask,4)!=1)return 1;
    if(n+14>cap){cap=n+14;frame=malloc(cap);if(!frame)return 1;}
    bytes=lx_ws_client_frame(frame,cap,op,p,n,mask);
    int bad=!bytes||ws_send_all(w,frame,bytes);if(frame!=small)free(frame);return bad;
}
struct frame_test_sink{unsigned char *data;size_t used;};
static CURLcode frame_test_send(CURL *context,const void *p,size_t n,size_t *sent){
    struct frame_test_sink *s=(void *)context;if(n>83)n=83;if(s->used+n>MAX_MESSAGE+14)return CURLE_SEND_ERROR;memcpy(s->data+s->used,p,n);s->used+=n;*sent=n;return CURLE_OK;
}
static int frame_test_random(unsigned char *p,int n){for(int i=0;i<n;i++)p[i]=(unsigned char)(17+i);return 1;}
static int frame_send_test(void){
    unsigned char *payload=malloc(MAX_MESSAGE+1),*encoded=malloc(MAX_MESSAGE+14);if(!payload||!encoded){free(payload);free(encoded);return 1;}
    for(size_t i=0;i<MAX_MESSAGE+1;i++)payload[i]=(unsigned char)(i%251);
    const size_t sizes[]={0,125,126,2040,2041,4096,65535,65536};int bad=0;struct curl_api a={.send=frame_test_send};
    for(unsigned i=0;i<sizeof(sizes)/sizeof(sizes[0]);i++){size_t n=sizes[i],h=n<126?2:n<=65535?4:10;struct frame_test_sink sink={encoded,0};struct client w={.a=&a,.c=(void *)&sink,.random_bytes=frame_test_random,.deadline=now_ms()+2000};
        if(ws_send_frame(&w,1,payload,n)||sink.used!=n+h+4||encoded[0]!=0x81||!(encoded[1]&0x80))bad=1;
        size_t decoded=encoded[1]&127;if(decoded==126)decoded=(size_t)encoded[2]*256+encoded[3];else if(decoded==127){decoded=0;for(int k=0;k<8;k++)decoded=decoded*256+encoded[2+k];}if(decoded!=n)bad=1;
        for(size_t k=0;k<n;k++)if((encoded[h+4+k]^encoded[h+k%4])!=payload[k])bad=1;
    }
    struct frame_test_sink sink={encoded,0};struct client w={.a=&a,.c=(void *)&sink,.random_bytes=frame_test_random,.deadline=now_ms()+2000};if(!ws_send_frame(&w,1,payload,MAX_MESSAGE+1)||sink.used)bad=1;
    free(payload);free(encoded);printf("ws_outbound_boundary_test=%s checks=9 partial_writes=1 network_connections=0\n",bad?"fail":"pass");return bad;
}

static int playback_write(const unsigned char *p,size_t n) {
    long frames=(long)(n/2),done=0;
    while(done<frames) {
        long r=g_alsa.writei(g_play,p+done*2,(unsigned long)(frames-done));
        if(r<0) { g_werrs++; if(g_alsa.recover(g_play,(int)r,1)<0)return 1; continue; }
        if(!r)continue;
        done+=r;g_written+=r;
    }return 0;
}
static void *playback_main(void *arg) {
    struct ring *r=arg;
    static unsigned char block[4096];
    while(1) {
        size_t take;
        pthread_mutex_lock(&r->lock);
        while(!r->used&&!r->stop)pthread_cond_wait(&r->ready,&r->lock);
        if(r->stop){pthread_mutex_unlock(&r->lock);break;}
        take=r->used>sizeof(block)?sizeof(block):r->used;
        {
            size_t first=r->cap-r->head;
            if(first>take)first=take;
            memcpy(block,r->data+r->head,first);
            if(take>first)memcpy(block+first,r->data,take-first);
            r->head=(r->head+take)%r->cap;r->used-=take;r->inflight=1;
        }
        pthread_mutex_unlock(&r->lock);
        {int failed=playback_write(block,take);
         pthread_mutex_lock(&r->lock);r->inflight=0;if(failed)r->stop=1;
         pthread_mutex_unlock(&r->lock);if(failed)break;}
    }return NULL;
}
/* Query both the software queue and ALSA: follow-up time starts after sound,
   not after the last network packet. */
static int playback_idle(struct ring *r) {
    int idle;
    pthread_mutex_lock(&r->lock);idle=!r->used&&!r->inflight;
    if(idle) {
        /* The factory default PCM can retain a positive snd_pcm_delay after
           all writes complete. Drain is its completion barrier; prepare
           enables the same handle for the next turn. No producer can enqueue
           more audio while this function executes in the receiver thread. */
        int rc=g_alsa.drain(g_play);
        idle=rc>=0;
        if(g_alsa.prepare(g_play)<0)idle=0;
    }
    pthread_mutex_unlock(&r->lock);
    return idle;
}
/* Factory wakeup.sh maps 1=listening, 2=thinking, 3=speaking.
   Use the existing effects without invoking its player/session side effects.
   Only withdraw the effect this client actually enabled. */
static int led_command(int effect,int on) {
    pid_t child;int status=0;char data[48];
    snprintf(data,sizeof(data),"{\"L\":%d,\"pos\":0}",effect);
    child=fork();
    if(child==0) {
        execl("/bin/ubus","ubus","-t","1","call","led",on?"show":"shut",data,(char *)NULL);
        _exit(127);
    }
    if(child<0)return 1;
    while(waitpid(child,&status,0)<0){if(g_stop)return 1;}
    return !WIFEXITED(status)||WEXITSTATUS(status)!=0;
}
static void feedback_led(int state) {
    g_led_requested=state;if(!lx_lamp_feedback_enabled())state=0;
    int old=g_led_owned;
    if(state==old)return;
    /* Show the next effect before withdrawing the previous one: no dark gap. */
    if(state&&led_command(state,1)){puts("wake_led=failed");return;}
    if(old)led_command(old,0);
    g_led_owned=state;
    if(state)publish_state(state==1?"listening":state==2?"thinking":"speaking");
    printf("wake_led=%s effect=%d\n",state==1?"listening":state==2?"thinking":state==3?"speaking":"off",state);
    fflush(stdout);
}
/* Existing factory recording, played locally: no TTS request and no key.
   The file is mono S16/16k. ALSA handles the rate conversion. */
static int feedback_clip(const char *path,int fallback) {
    int n=0,rc=1;void *pcm=NULL;
    short *audio=lx_kws_read_wav(path,&n);
    if(!audio&&fallback)audio=lx_kws_read_wav("/usr/share/sound/wakeup_wozai.wav",&n);
    if(!audio||n<=0||n>RATE_IN*8)goto done;
    if(g_alsa.open(&pcm,"default",0,0))goto done;
    if(g_alsa.set_params(pcm,2,3,1,RATE_IN,1,100000))goto done;
    for(int off=0;off<n&&!g_stop;) {
        long got=g_alsa.writei(pcm,audio+off,(unsigned long)(n-off));
        if(got<0){if(g_alsa.recover(pcm,(int)got,1)<0)goto done;continue;}
        if(!got)goto done;
        off+=(int)got;
    }
    rc=g_stop?1:0;
done:
    if(pcm){if(g_stop)g_alsa.drop(pcm);else g_alsa.drain(pcm);g_alsa.close(pcm);}
    free(audio);printf("wake_ack=%s\n",rc?"fail":"played");fflush(stdout);return rc;
}
static int feedback_ack(void) {return feedback_clip("/data/lx06-agent/wake_ack.wav",1);}
static int ring_push(struct ring *r,const unsigned char *p,size_t n) {
    pthread_mutex_lock(&r->lock);
    if(n>r->cap-r->used){pthread_mutex_unlock(&r->lock);return 1;}
    while(n) {
        size_t space=r->cap-r->used,take,first;
        if(!space)break;
        take=n>space?space:n;first=r->cap-r->tail;
        if(first>take)first=take;
        memcpy(r->data+r->tail,p,first);
        if(take>first)memcpy(r->data,p+first,take-first);
        r->tail=(r->tail+take)%r->cap;r->used+=take;
        p+=take;n-=take;
    }
    pthread_cond_signal(&r->ready);
    pthread_mutex_unlock(&r->lock);return 0;
}

static void wake_ding(struct ring *r) {
    static const short wave[60]={0,125,249,371,488,600,705,803,892,971,1039,1096,1141,1174,1193,1200,1193,1174,1141,1096,1039,971,892,803,705,600,488,371,249,125,0,-125,-249,-371,-488,-600,-705,-803,-892,-971,-1039,-1096,-1141,-1174,-1193,-1200,-1193,-1174,-1141,-1096,-1039,-971,-892,-803,-705,-600,-488,-371,-249,-125};short pcm[2400];
    for(int i=0;i<2400;i++){int ramp=i<288?i:(2399-i<288?2399-i:288);pcm[i]=(short)(wave[i%60]*ramp/288);}
    int rc=ring_push(r,(const unsigned char *)pcm,sizeof(pcm));
    printf("wake_ding=%s duration_ms=100 peak=1200\n",rc?"failed":"queued");fflush(stdout);
}

static void ring_clear(struct ring *r) {
    pthread_mutex_lock(&r->lock);
    r->head=r->tail=0;r->used=0;
    pthread_mutex_unlock(&r->lock);
}
static void play_delta(struct client *w,const char *b64) {
    if(g_suppress_audio)return;
    size_t len=strlen(b64);
    unsigned char *raw;
    int got;
    if(!len||len>MAX_MESSAGE)return;
    raw=malloc(len);
    if(!raw)return;
    got=w->b64_decode(raw,(const unsigned char *)b64,(int)len);
    while(got>0&&len>0&&b64[len-1]=='='){got--;len--;}
    if(got>0){
        short *s=(short *)raw;int i,n=got/2;
        /* The realtime voice peaks around -8 dBFS, clearly below the factory
           prompts; lift it here rather than changing the shared volume. */
        for(i=0;i<n;i++){
            long v=(long)s[i]*g_answer_gain/100;
            if(v>32767)v=32767;else if(v<-32768)v=-32768;
            s[i]=(short)v;
            int a=v<0?-(int)v:(int)v;if(a>g_apeak)g_apeak=a;
        }
        g_asamples+=n;
        w->st.imported_audio+=got;if(ring_push(w->ring,raw,(size_t)got)){w->st.error_seen=1;w->st.finished=1;}
    }
    free(raw);
}
static void handle_message(struct client *w,const unsigned char *p,size_t n) {
    static char text[MAX_MESSAGE];
    void *obj,*value=NULL;
    size_t copy=n<sizeof(text)-1?n:sizeof(text)-1;
    memcpy(text,p,copy);text[copy]=0;
    obj=w->json_parse(text);
    if(!obj)return;
    if(w->json_get(obj,"type",&value)&&w->json_type(value)==6) {
        const char *type=w->json_string(value);
        if(!strcmp(type,"session.created")){w->st.session_ready=1;w->st.created++;}
        else if(!strcmp(type,"response.function_call_arguments.done")){
            if(w->st.pending_call){w->st.error_seen=1;w->st.finished=1;}else w->st.pending_call=strdup(text);
        }
        else if(strstr(type,"input_audio_transcription")) {
            w->st.transcript++;
            if(strstr(type,".started")){w->st.input_started++;if(!w->input_ms){w->input_ms=now_ms();printf("session_ready_to_first_asr_ms=%lld\n",(long long)(w->ready_ms?w->input_ms-w->ready_ms:0));fflush(stdout);}}
            if(strstr(type,".completed")){w->st.committed++;if(g_led_requested)feedback_led(2);}
        }
        else if(!strcmp(type,"input_audio_buffer.committed")){w->st.committed++;if(g_led_requested)feedback_led(2);}
        else if(strstr(type,"output_text")){
            w->st.text++;
            if(g_session_fixture&&!g_fixture_live_tail&&!strcmp(type,"response.output_text.delta")){
                void *delta=NULL;
                if(w->json_get(obj,"delta",&delta)&&w->json_type(delta)==6){const char *s=w->json_string(delta);size_t len=strlen(s);if(len<sizeof(g_fixture_reply)-g_fixture_reply_n){memcpy(g_fixture_reply+g_fixture_reply_n,s,len);g_fixture_reply_n+=len;g_fixture_reply[g_fixture_reply_n]=0;}}
            }
        }
        else if(!strcmp(type,"response.output_audio.done"))w->st.audio_done++;
        else if(!strcmp(type,"response.done")){w->st.done++;g_suppress_audio=0;}
        else if(!strcmp(type,"response.canceled")){w->st.canceled++;if(w->ring)ring_clear(w->ring);g_suppress_audio=0;puts("response_canceled=1");fflush(stdout);}
        else if(!strcmp(type,"session.closed"))w->st.finished=1;
        else if(!strcmp(type,"error")){w->st.error_seen=1;w->st.error++;w->st.finished=1;}
        else if(!strcmp(type,"response.output_audio.delta")) {
            void *audio=NULL;
            if(!w->st.audio_delta&&w->input_ms){printf("first_asr_to_first_audio_ms=%lld\n",(long long)(now_ms()-w->input_ms));fflush(stdout);}
            if(!w->st.audio_delta&&w->ready_ms){printf("session_ready_to_first_audio_ms=%lld\n",(long long)(now_ms()-w->ready_ms));fflush(stdout);}
            if(!w->st.audio_delta&&g_wake_started_ms){printf("wake_to_first_audio_ms=%lld\n",(long long)(now_ms()-g_wake_started_ms));fflush(stdout);}
            w->st.audio_delta++;if(g_led_requested)feedback_led(3);
            if(w->json_get(obj,"delta",&audio)||w->json_get(obj,"audio",&audio))
                if(w->json_type(audio)==6)play_delta(w,w->json_string(audio));
        }
    }
    w->json_put(obj);
}
static int ws_event(void *context,unsigned op,const unsigned char *p,size_t n) {
    struct client *w=context;
    if(op==9)return ws_send_frame(w,10,p,n);
    if(op==8){w->st.finished=1;return 0;}
    if(op==1)handle_message(w,p,n);
    return 0;
}
static int send_pcm(struct client *w,const short *pcm,int samples) {
    unsigned char b64[1024];char event[1200];int len;
    len=w->b64_encode(b64,(const unsigned char *)pcm,samples*2);
    if(len<=0)return 1;
    b64[len]=0;
    len=snprintf(event,sizeof(event),
        "{\"type\":\"input_audio_buffer.append\",\"audio\":\"%s\"}",(char *)b64);
    if(len<=0||(size_t)len>=sizeof(event))return 1;
    w->deadline=now_ms()+5000;
    return ws_send_frame(w,1,(const unsigned char *)event,(size_t)len);
}
static void drain(struct client *w,struct lx_ws_receiver *r) {
    unsigned char packet[4096];
    while(1) {
        size_t n=0;CURLcode code=w->a->recv(w->c,packet,sizeof(packet),&n);
        if(code==CURLE_AGAIN)return;
        if(code||!n){w->st.finished=1;return;}
        if(lx_ws_feed(r,packet,n,ws_event,w)){w->st.finished=1;return;}
    }
}

/* ---------- provider/host adapters for the shared agent loop ---------- */

struct agent_provider_ctx {struct client *w;struct lx_ws_receiver *rx;struct lx_capture *capture;};
struct agent_host_ctx {struct client *w;struct lx_capture *capture;struct tb_job *tools;int led_owned;struct lx_kws *kws;};

static struct lx_agent_stats *agent_stats(void *c){return &((struct agent_provider_ctx *)c)->w->st;}
static int agent_send_audio(void *c,const short *pcm,int samples){struct agent_provider_ctx *p=c;if(!lx_capture_cloud_ready(p->capture)||g_session_fixture)return -1;return send_pcm(p->w,pcm,samples);}
static int agent_send_json(void *c,const char *json){struct client *w=((struct agent_provider_ctx *)c)->w;w->deadline=now_ms()+2000;return ws_send_frame(w,1,(const unsigned char *)json,strlen(json));}
static int agent_pump(void *c,int timeout_ms){
    struct agent_provider_ctx *p=c;struct client *w=p->w;struct pollfd f={w->socket,POLLIN,0};
    w->deadline=now_ms()+1000;
    if(poll(&f,1,(lx_handoff.active||g_session_fixture)?0:timeout_ms)<=0)return 0;
    drain(w,p->rx);
    return 0;
}
static int agent_stop(void *c){(void)c;feedback_led(g_led_requested);return g_stop;}
static long agent_capture(void *c,short *pcm,int frames){
    struct agent_host_ctx *h=c;
    if(!lx_capture_cloud_ready(h->capture))return -1;
    return lx_handoff.active?lx_handoff_read(pcm,frames):lx_capture_read(h->capture,pcm,frames);
}
static int agent_recover(void *c,long error){
    struct agent_host_ctx *h=c;
    /* The handoff worker owns the PCM and retries recovery itself. A transient
       read error here must not end the session. */
    if(lx_handoff.active)return 0;
    return lx_capture_recover(h->capture,error);
}
static void agent_play(void *c,const char *b64){struct agent_host_ctx *h=c;play_delta(h->w,b64);}
static int agent_idle(void *c){struct agent_host_ctx *h=c;return playback_idle(h->w->ring);}
static void agent_led(void *c,int state){struct agent_host_ctx *h=c;if(h->led_owned)feedback_led(state);}
static void agent_publish(void *c,const char *phase){(void)c;publish_state(phase);}
static int agent_tool_start(void *c,const char *event){
    struct tb_job *j=((struct agent_host_ctx *)c)->tools;
    j->event=strdup(event);
    if(!j->event)return 1;
    j->done=0;j->active=1;
    if(pthread_create(&j->thread,NULL,tb_worker,j)){j->active=0;free(j->event);j->event=NULL;return 1;}
    return 0;
}
static int agent_tool_busy(void *c){return ((struct agent_host_ctx *)c)->tools->active;}
static int agent_tool_take(void *c,char *reply,size_t cap){
    struct tb_job *j=((struct agent_host_ctx *)c)->tools;
    pthread_mutex_lock(&j->lock);int done=j->done;char *text=j->reply;j->reply=NULL;pthread_mutex_unlock(&j->lock);
    if(!done)return 0;
    pthread_join(j->thread,NULL);j->active=0;free(j->event);j->event=NULL;
    if(!text)return -1;
    snprintf(reply,cap,"%s",text);free(text);
    return 1;
}
static int agent_tool_ticket(void *c){return ((struct agent_host_ctx *)c)->tools->ticket[0]!=0;}
static int agent_tool_end(void *c){return ((struct agent_host_ctx *)c)->tools->end_session;}
/* Wake-word barge-in: keep matching while the reply plays. */
static void agent_barge_reset(void *c){struct agent_host_ctx *h=c;if(h->kws)lx_kws_init(h->kws);}
static int agent_barge_check(void *c,const short *pcm,int n){
    struct agent_host_ctx *h=c;
    if(!h->kws||n<=0)return 0;
    lx_kws_push(h->kws,pcm,n,NULL,NULL);
    if(h->kws->woke){h->kws->woke=0;h->kws->hits=0;h->kws->best_dist=1e30f;return 1;}
    return 0;
}
static void agent_barge(void *c){
    struct agent_host_ctx *h=c;
    if(h->w->ring)ring_clear(h->w->ring);
    g_suppress_audio=1;
    if(h->led_owned)feedback_led(1);
}

/* ---------- local wake word detection ---------- */

struct kws_ctx {
    int quiet;
    int wakes;
    int near;
    int candidate_hits;
    int64_t start_ms;
    float best;
};

/* Music raises the noise floor enough that the third confirming window often
   lands just above the threshold. Two consecutive hits pause playback briefly
   so the confirmation runs on clean audio; no wake means quick resume. */
static int g_tools_ready,g_candidate_paused;
static int64_t g_candidate_last;
/* Threshold passed on the command line, if any. Music mode may widen it. */
static float g_threshold_cli=-1.0f;
static int64_t g_candidate_until;
static int candidate_music_pause(void){
    if(!g_tools_ready||g_candidate_paused)return 0;
    /* A false candidate should not keep dipping the music. */
    if(now_ms()-g_candidate_last<6000)return 0;
    void *st=tb_request("music.status",tj.object(),now_ms()+800);
    int playing=st&&tb_i(st,"status")==1;if(st)tj.put(st);
    if(!playing)return 0;
    void *r=tb_request("music.pause",tj.object(),now_ms()+800);
    if(!r)return 0;
    tj.put(r);g_candidate_paused=1;g_candidate_until=now_ms()+2200;g_candidate_last=now_ms();
    puts("music_paused_for_candidate");fflush(stdout);
    return 1;
}
static void candidate_music_resume(void){
    if(!g_candidate_paused)return;
    void *r=tb_request("music.resume",tj.object(),now_ms()+800);
    if(r)tj.put(r);
    g_candidate_paused=0;
    puts("music_resumed_no_wake");fflush(stdout);
}

static void kws_log(void *context,struct lx_kws *k,int span_ms,float dist,int hit,int woke) {
    struct kws_ctx *c=context;
    int64_t ms=now_ms();
    if(c->best>k->best_dist)c->best=k->best_dist;
    if(!c->quiet)
        printf("win=%d t=%dms span=%dms peak=%.1fdB floor=%.1fdB flux=%.2f dist=%.4f hits=%d hit=%d wake=%d\n",
            k->evals,(int)(ms-c->start_ms),span_ms,(double)k->peak_db,(double)k->floor_db,
            (double)k->flux,(double)dist,k->hits,hit,woke);
    if(woke)c->wakes++;
    if(!woke&&k->hits>=(c->candidate_hits>0?c->candidate_hits:3))c->near=1;
    fflush(stdout);
}

/* Read the capture stream until a wake word is detected or the standby time
   ends. The last 300 ms of audio is copied to preroll_out on success. */
static int kws_listen(struct lx_capture *capture,struct lx_kws *k,int seconds,int quiet,
                      short *preroll_out) {
    const char *engine_mode=getenv("LX06_WAKE_ENGINE");
    if(!engine_mode||!strcmp(engine_mode,"factory")) {
        if(lx_factory_init()) {g_stop=1;return 0;}
        short mic[FRAME_IN];int wakes=0;k->woke=0;
        int64_t stop=now_ms()+(int64_t)seconds*1000,next_report=now_ms()+10000;
        unsigned samples=0,reads=0;int peak=0;double energy=0;
        short silence[160]={0};
        for(int i=0;i<100;i++)lx_factory_push(silence,160);
        while(!g_stop&&now_ms()<stop) {
            long got=lx_capture_read(capture,mic,(unsigned long)FRAME_IN);
            if(got<0){if(lx_capture_recover(capture,got)<0)break;continue;}
            for(int i=0;i<(int)got;i++) {
                int v=mic[i],abs_v=v<0?-v:v;if(abs_v>peak)peak=abs_v;energy+=(double)v*v;samples++;
                k->preroll[k->preroll_pos]=mic[i];
                k->preroll_pos=(k->preroll_pos+1)%LX_KWS_PREROLL;
            }
            if(got>0)reads++;
            if(now_ms()>=next_report){printf("factory_capture reads=%u samples=%u peak=%d mean_square=%.1f\n",reads,samples,peak,samples?energy/samples:0);fflush(stdout);reads=samples=0;peak=0;energy=0;next_report=now_ms()+10000;}
            if(got>0&&lx_factory_push(mic,(int)got)) {
                wakes++;k->woke=1;
                if(preroll_out)break;
            }
        }
        if(preroll_out&&k->woke)lx_kws_preroll(k,preroll_out);
        printf("factory_kws_listen wakes=%d woke=%d\n",wakes,k->woke);fflush(stdout);
        return preroll_out?k->woke:(wakes>0);
    }
    struct kws_ctx ctx;
    short mic[FRAME_IN];
    int64_t stop_at,frames=0;
    memset(&ctx,0,sizeof(ctx));
    ctx.quiet=quiet;
    ctx.best=1e30f;
    ctx.start_ms=now_ms();
    int64_t next_music_poll=0;
    if(g_threshold_cli<0.0f)g_threshold_cli=lx_kws_threshold_override;
    stop_at=ctx.start_ms+(int64_t)seconds*1000;
    while((preroll_out? !k->woke : 1)&&!g_stop&&now_ms()<stop_at) {
        /* Music raises the false-hit rate and masks the word. While the factory
           player runs, require one extra consecutive window; the confirmation
           window then lands after candidate_music_pause() has silenced music. */
        if(preroll_out&&g_tools_ready&&now_ms()>=next_music_poll) {
            next_music_poll=now_ms()+5000;
            void *st=tb_request("music.status",tj.object(),now_ms()+300);
            int playing=st&&tb_i(st,"status")==1;if(st)tj.put(st);
            int want=playing&&!capture->aec?4:3;
            if(k->min_hits!=want){k->min_hits=want;printf("kws_min_hits=%d music=%d aec=%d\n",want,playing,capture->aec);fflush(stdout);}
            /* Music plus AEC leaves speech around distance 14 while loud music
               sits near 13. Widen the candidate threshold and pause playback
               after two consecutive hits; the pause supplies clean audio for
               the third confirmation window. */
            int aec_music=playing&&capture->aec;
            float limit=aec_music?(g_threshold_cli>0.0f?g_threshold_cli:15.0f):g_threshold_cli;
            if(lx_kws_threshold_override!=limit){
                lx_kws_threshold_override=limit;
                printf("kws_threshold=%.2f music=%d\n",(double)lx_kws_threshold(),playing);fflush(stdout);
            }
            ctx.candidate_hits=aec_music?2:3;
        }
        long got=lx_capture_read(capture,mic,(unsigned long)FRAME_IN);
        if(got<0){if(lx_capture_recover(capture,got)<0)break;continue;}
        if(!got)continue;
        lx_kws_push(k,mic,(int)got,kws_log,&ctx);
        frames++;
        if(ctx.near){ctx.near=0;candidate_music_pause();}
        if(g_candidate_paused&&now_ms()>=g_candidate_until)candidate_music_resume();
        /* Standby-only runs keep listening and count every wake event. */
        if(!preroll_out&&k->woke){k->woke=0;k->hits=0;}
    }
    if(!k->woke)candidate_music_resume();
    if(preroll_out&&k->woke)lx_kws_preroll(k,preroll_out);
    printf("kws_listen frames=%d windows=%d best_dist=%.4f wakes=%d woke=%d elapsed=%dms\n",
        (int)frames,k->evals,(double)ctx.best,ctx.wakes,k->woke,
        (int)(now_ms()-ctx.start_ms));
    fflush(stdout);
    return preroll_out?k->woke:(ctx.wakes>0);
}

static void print_kws_verdict(void) {
    printf("kws_templates=%d threshold=%.4f intra_max=%.4f\n",
        LX_KWS_TEMPLATE_COUNT,(double)lx_kws_threshold(),
        (double)lx_kws_template_intra_max);
}

/* Score a captured buffer with the same detector code and print every window.
   Used for negative tests: nothing is written to disk and no network is used. */
static int kws_scan_buffer(const short *pcm,int n) {
    struct lx_kws *k=calloc(1,sizeof(*k));
    int i,step=320,scored=0,wakes=0;
    float best=1e30f,peak_min=1e30f,peak_max=-1e30f;
    if(!k)return 1;
    lx_kws_init(k);
    for(i=0;i+step<=n;i+=step) {
        int before=k->evals;
        lx_kws_push(k,pcm+i,step,NULL,NULL);
        if(k->evals==before)continue;
        if(k->peak_db<peak_min)peak_min=k->peak_db;
        if(k->peak_db>peak_max)peak_max=k->peak_db;
        if(k->last_dist>=0.0f){
            int woke=k->hits==LX_KWS_MIN_HITS;
            if(k->last_dist<best)best=k->last_dist;
            scored++;
            if(woke)wakes++;
            printf("cap t=%dms span=%dms peak=%.1fdB floor=%.1fdB noise=%.1fdB dist=%.4f hits=%d wake=%d\n",
                (i+step)*1000/LX_KWS_RATE,k->span_ms,(double)k->peak_db,
                (double)k->floor_db,(double)k->noise_db,(double)k->last_dist,k->hits,woke);
        }
    }
    printf("cap_result windows=%d scored=%d wakes=%d min_dist=%.4f peak_min=%.1f peak_max=%.1f\n",
        k->evals,scored,wakes,(double)best,(double)peak_min,(double)peak_max);
    fflush(stdout);
    free(k);
    return 0;
}

/* Score one buffer without printing every window, for A/B capture comparisons. */
struct kws_measure {float best;int wakes,scored;};
static void kws_measure(const short *pcm,int n,struct kws_measure *out) {
    struct lx_kws *k=calloc(1,sizeof(*k));
    memset(out,0,sizeof(*out));out->best=1e30f;
    if(!k)return;
    lx_kws_init(k);
    for(int i=0;i+320<=n;i+=320) {
        int before=k->evals;
        lx_kws_push(k,pcm+i,320,NULL,NULL);
        if(k->evals==before)continue;
        if(k->last_dist>=0.0f){out->scored++;if(k->last_dist<out->best)out->best=k->last_dist;}
        if(k->woke){out->wakes++;k->woke=0;k->hits=0;}
    }
    free(k);
}

/* ---------- one realtime session ---------- */

/* Diagnostic playback-source comparison. Unlike adjacent overlapping wake
   windows, the hardware playback reference is independent of near-end speech.
   This is an experiment, not yet an activation gate. */
struct wake_audit_ctx {struct lx_kws *reference;int candidates;};
static void wake_audit_report(void *context,struct lx_kws *k,int span,float dist,int hit,int woke) {
    struct wake_audit_ctx *a=context;
    struct lx_kws *r=a->reference;
    (void)span;(void)woke;
    if(!hit)return;
    int first,last,n=lx_kws_trim(k->db,k->nframes,&first,&last);
    static struct lx_kws_utt mic,ref;
    float echo=1e30f,ref_peak=-200.0f;
    if(n<=0||r->nframes!=k->nframes)return;
    mic.n=n;lx_kws_finalize(&mic,(const float (*)[LX_KWS_COEF])&k->cep[first][0]);
    for(int lag=0;lag<=30;lag+=5) {
        int start=first-lag;
        if(start<0)continue;
        ref.n=n;lx_kws_finalize(&ref,(const float (*)[LX_KWS_COEF])&r->cep[start][0]);
        float d=lx_kws_dtw(mic.f[0],n,ref.f[0],n);
        if(d<echo)echo=d;
    }
    for(int i=0;i<r->nframes;i++)if(r->db[i]>ref_peak)ref_peak=r->db[i];
    a->candidates++;
    printf("wake_audit win=%d wake_dist=%.4f echo_dist=%.4f reference_wake=%.4f reference_peak=%.1f hits=%d\n",
        k->evals,(double)dist,(double)echo,(double)r->last_dist,(double)ref_peak,k->hits);
    fflush(stdout);
}

/* A bounded realtime session, with half-duplex follow-up turns after playback.
   All cloud events stay in the same session so conversation history is retained. */
static int run_session(struct curl_api *a,void *crypto,void *json,const char *key,
                       struct lx_capture *capture,int mode,int seconds,const short *preroll,
                       int preroll_n,int *resume_music,struct lx_kws *kws) {
    struct client w;struct ring ring;struct lx_ws_receiver receiver;
    struct tb_job tools={0};int tools_ready=0,play_after_reply=0;
    static unsigned char message[MAX_MESSAGE];
    char request[4608],header[8193]={0},expected[29],nonce_key[25]={0};
    unsigned char nonce[16];
    size_t used=0,sent=0;
    int length,status=0,frames=0,result=1,started=0,stage=0,stream_failed=0;
    pthread_t player;
    void *playback=NULL;
    if(!lx_capture_cloud_ready(capture)||g_session_fixture||mode==MODE_KWS_WAV){puts("cloud_input_blocked=aec_unavailable_or_unfiltered_source");return 1;}
    memset(&w,0,sizeof(w));
    w.setup_ms=now_ms();
    memset(&ring,0,sizeof(ring));
    if(tb_init(json)||pthread_mutex_init(&tools.lock,NULL))return 1;tools_ready=1;
    g_play=NULL;g_written=0;g_werrs=0;g_apeak=0;g_asamples=0;
    w.random_bytes=(int (*)(unsigned char *,int))dlsym(crypto,"RAND_bytes");
    w.b64_encode=(int (*)(unsigned char *,const unsigned char *,int))dlsym(crypto,"EVP_EncodeBlock");
    w.b64_decode=(int (*)(unsigned char *,const unsigned char *,int))dlsym(crypto,"EVP_DecodeBlock");
    w.json_parse=(void *(*)(const char *))dlsym(json,"json_tokener_parse");
    w.json_get=(int (*)(void *,const char *,void **))dlsym(json,"json_object_object_get_ex");
    w.json_string=(const char *(*)(void *))dlsym(json,"json_object_get_string");
    w.json_type=(int (*)(void *))dlsym(json,"json_object_get_type");
    w.json_put=(void (*)(void *))dlsym(json,"json_object_put");
    if(!w.random_bytes||!w.b64_encode||!w.b64_decode||!w.json_parse||!w.json_get||
       !w.json_string||!w.json_type||!w.json_put){puts("symbol lookup failed");return 1;}

    int64_t open_started=now_ms();
    if(g_alsa.open(&playback,"default",0,0)){puts("playback open failed");return 1;}
    int64_t open_done=now_ms();
    if(g_alsa.set_params(playback,2,3,1,RATE_OUT,1,100000)) {
        puts("alsa playback set_params failed");
        g_alsa.close(playback);
        return 1;
    }
    printf("playback_open_ms=%lld playback_params_ms=%lld\n",
        (long long)(open_done-open_started),(long long)(now_ms()-open_done));fflush(stdout);
    g_play=playback;
    ring.cap=RATE_OUT*2*60;ring.data=malloc(ring.cap);
    if(!ring.data){g_alsa.drain(playback);g_alsa.close(playback);return 1;}
    pthread_mutex_init(&ring.lock,NULL);pthread_cond_init(&ring.ready,NULL);
    w.ring=&ring;
    if(pthread_create(&player,NULL,playback_main,&ring)) {
        free(ring.data);g_alsa.drain(playback);g_alsa.close(playback);
        return 1;
    }
    started=1;
    if(mode==MODE_KWS)wake_ding(&ring);
    /* Everything above is local device setup; the remainder of setup_to_tls
       is curl/OpenSSL initialization and the handshake. */
    printf("session_prep_ms=%lld\n",(long long)(now_ms()-w.setup_ms));fflush(stdout);
    publish_state("connecting");
    stage=1;
    w.a=a;w.c=a->init();w.socket=-1;
    if(!w.c||a->global_init(CURL_GLOBAL_DEFAULT))goto cleanup;
    stage=2;
    if(w.random_bytes(nonce,sizeof(nonce))!=1||
       w.b64_encode((unsigned char *)nonce_key,nonce,sizeof(nonce))!=24)goto cleanup;
    {
        int (*sha1)(const unsigned char *,size_t,unsigned char *)=
            (int (*)(const unsigned char *,size_t,unsigned char *))dlsym(crypto,"SHA1");
        char combined[61];unsigned char digest[20];
        if(!sha1)goto cleanup;
        stage=3;
        memcpy(combined,nonce_key,24);memcpy(combined+24,"258EAFA5-E914-47DA-95CA-C5AB0DC85B11",36);
        if(!sha1((const unsigned char *)combined,60,digest)||
           w.b64_encode((unsigned char *)expected,digest,20)!=28)goto cleanup;
    }
    stage=4;
    if(a->setopt(w.c,CURLOPT_URL,"https://openspeech.bytedance.com/api/v3/duplex/realtime/dialogue")||
       a->setopt(w.c,CURLOPT_CONNECT_ONLY,1L)||a->setopt(w.c,CURLOPT_PROXY,"")||
       a->setopt(w.c,CURLOPT_CONNECTTIMEOUT_MS,5000L)||a->setopt(w.c,CURLOPT_TIMEOUT_MS,10000L)||
       a->setopt(w.c,CURLOPT_NOSIGNAL,1L)||a->perform(w.c)) {
        puts("connect failed");goto cleanup;
    }
    w.connect_done_ms=now_ms();
    printf("setup_to_tls_ms=%lld wake_to_tls_ms=%lld\n",(long long)(w.connect_done_ms-w.setup_ms),(long long)(g_wake_started_ms?w.connect_done_ms-g_wake_started_ms:0));fflush(stdout);
    {
        double dns=0,tcp=0,tls=0;
        if(!a->getinfo(w.c,CURLINFO_NAMELOOKUP_TIME,&dns)&&!a->getinfo(w.c,CURLINFO_CONNECT_TIME,&tcp)&&!a->getinfo(w.c,CURLINFO_APPCONNECT_TIME,&tls))
            printf("connect_dns_ms=%.0f connect_tcp_ms=%.0f connect_tls_ms=%.0f\n",dns*1000,(tcp-dns)*1000,(tls-tcp)*1000);
        fflush(stdout);
    }
    {
        long sock=-1,verify=-1;
        if(a->getinfo(w.c,CURLINFO_ACTIVESOCKET,&sock)||
           a->getinfo(w.c,CURLINFO_SSL_VERIFYRESULT,&verify)||verify||sock<0)goto cleanup;
        w.socket=(int)sock;
        stage=5;
    }
    length=snprintf(request,sizeof(request),
        "GET /api/v3/duplex/realtime/dialogue HTTP/1.1\r\nHost: openspeech.bytedance.com\r\n"
        "Upgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Version: 13\r\n"
        "Sec-WebSocket-Key: %s\r\nX-Api-Key: %s\r\n\r\n",nonce_key,key);
    if(length<=0||(size_t)length>=sizeof(request))goto cleanup;
    stage=6;
    w.deadline=now_ms()+10000;
    while(sent<(size_t)length||(used<sizeof(header)-1&&!strstr(header,"\r\n\r\n"))) {
        size_t n=0;CURLcode code;
        int sending=sent<(size_t)length;
        int64_t remaining=w.deadline-now_ms();
        if(remaining<=0)goto cleanup;
        if(sending)code=a->send(w.c,request+sent,(size_t)length-sent,&n);
        else code=a->recv(w.c,header+used,1,&n);
        if(code==CURLE_AGAIN) {
            struct pollfd p={w.socket,sending?POLLOUT:POLLIN,0};
            if(poll(&p,1,remaining>50?50:(int)remaining)<0)goto cleanup;
            continue;
        }
        if(code||!n)goto cleanup;
        if(sending)sent+=n;else {used+=n;header[used]=0;}
    }
    if(sscanf(header,"HTTP/1.1 %d",&status)!=1||status!=101||
       !lx_ws_upgrade_valid(header,used,expected)) {
        fprintf(stderr,"upgrade failed http=%d\n",status);goto cleanup;
    }
    stage=7;
    memset(&receiver,0,sizeof(receiver));
    receiver.message=message;receiver.capacity=sizeof(message);
    puts("stage=connected");fflush(stdout);
    w.upgrade_ms=now_ms();printf("connect_to_upgrade_ms=%lld wake_to_upgrade_ms=%lld\n",(long long)(w.upgrade_ms-w.connect_done_ms),(long long)(g_wake_started_ms?w.upgrade_ms-g_wake_started_ms:0));fflush(stdout);
    printf("upgrade=101 session_starting\n");fflush(stdout);

    {
        static const char create_plain[]=
            "{\"type\":\"session.create\",\"session\":{\"model\":\"1.2.6.1\","
            "\"instructions\":\"只有用户明确叫你豆包、或说你好豆包时才回应；其他对话保持沉默。\","
            "\"audio\":{"
            "\"input\":{\"format\":{\"type\":\"pcm\",\"rate\":16000}},"
            "\"output\":{\"format\":{\"type\":\"pcm_s16le\",\"rate\":24000},"
            "\"voice\":\"zh_female_vv_uranus_bigtts\"}}}}";
        static const char create_wake[]=
            "{\"type\":\"session.create\",\"session\":{\"model\":\"1.2.6.1\","
            "\"instructions\":\"用户刚叫了你的名字豆包，请直接回答随后说的话；听不清时简短询问。\","
            "\"audio\":{"
            "\"input\":{\"format\":{\"type\":\"pcm\",\"rate\":16000}},"
            "\"output\":{\"format\":{\"type\":\"pcm_s16le\",\"rate\":24000},"
            "\"voice\":\"zh_female_vv_uranus_bigtts\"}}}}";
        int wake=mode==MODE_KWS||mode==MODE_KWS_WAV;
        void *config=tj.parse(wake?create_wake:create_plain),*session=NULL;
        if(!config||!tj.get(config,"session",&session)){if(config)tj.put(config);goto cleanup;}
        {
            /* The console owns the tool catalog. Without it the session still
               works for conversation, with tools reported as unavailable. */
            void *catalog=tb_catalog(now_ms()+3000);
            if(catalog){tj.add(session,"tools",catalog);printf("tool_catalog=loaded\n");}
            else printf("tool_catalog=unavailable\n");
            fflush(stdout);
        }
        {
            /* Compose, never replace, the mode-specific instruction. The plain
               session must keep its no-unsolicited-response rule. */
            const char *base=wake
                ?"用户刚叫了你的名字豆包，请直接回答随后说的话；听不清时简短询问。"
                :"只有用户明确叫你豆包、或说你好豆包时才回应；其他对话保持沉默。";
            char instructions[1024];
            snprintf(instructions,sizeof(instructions),
                "%s你是豆包，用用户的语言简短回答。需要操作音箱时调用对应工具；工具未成功，不要声称成功。"
                "当用户说闭嘴、停止、别说了、结束、不用了或表示不想继续时，立即调用 session_end 工具结束会话，不要用语音回复。",
                base);
            tj.add(session,"instructions",tj.str(instructions));
        }
        const char *create=tj.encode(config);size_t len=strlen(create);
        w.deadline=now_ms()+5000;
        int bad=ws_send_frame(&w,1,(const unsigned char *)create,len);tj.put(config);if(bad)goto cleanup;
    }
    stage=8;
    w.deadline=now_ms()+10000;
    while(!w.st.session_ready&&!w.st.error_seen&&!w.st.finished) {
        struct pollfd p={w.socket,POLLIN,0};
        int64_t now=now_ms();
        if(now<0||now>=w.deadline)goto cleanup;
        if(poll(&p,1,20)<=0)continue;
        drain(&w,&receiver);
    }
    if(!w.st.session_ready){fputs("session.create rejected\n",stderr);goto cleanup;}
    stage=9;
    printf("session_ready=1\n");fflush(stdout);if(g_led_requested)feedback_led(1);
    w.ready_ms=now_ms();printf("upgrade_to_session_ready_ms=%lld\n",(long long)(w.ready_ms-w.upgrade_ms));fflush(stdout);

    stage=10;
    {
        int answer_mode=mode==MODE_KWS||mode==MODE_KWS_WAV;
        int limit=answer_mode?g_session_max_ms:seconds*1000;
        if(limit>g_session_max_ms)limit=g_session_max_ms;
        struct agent_provider_ctx pctx={&w,&receiver,capture};
        struct agent_host_ctx hctx={&w,capture,&tools,g_led_requested,kws};
        struct lx_agent_provider provider={&pctx,agent_stats,agent_send_audio,agent_send_json,agent_pump};
        struct lx_agent_host host={&hctx,agent_stop,agent_capture,agent_recover,agent_play,agent_idle,agent_led,agent_publish,agent_tool_start,agent_tool_busy,agent_tool_take,agent_tool_ticket,agent_tool_end,agent_barge_reset,agent_barge_check,agent_barge};
        struct lx_agent_limits limits={limit,g_followup_ms,g_no_speech_ms,answer_mode,g_no_response_ms,g_voice_gate,g_full_duplex};
        printf("session_duplex=%s\n",g_full_duplex?"full":"half");fflush(stdout);
        printf("session_policy safety_max_ms=%d silence_ms=%d followup_ms=%d no_progress_ms=%d\n",limit,g_no_speech_ms,g_followup_ms,g_no_response_ms);fflush(stdout);
        struct lx_agent_result run;
        result=lx_agent_run(&provider,&host,&limits,lx_handoff.active?NULL:preroll,lx_handoff.active?0:preroll_n,FRAME_IN,&run);
        lx_handoff_discard();
        frames=run.frames;stream_failed=run.stream_failed;
        if(!strcmp(run.reason,"music_queued"))play_after_reply=1;
        printf("session_stop=%s turns=%d\n",run.reason,run.turns);fflush(stdout);
    }
    stage=11;
    printf("stream_done frames=%d imported_audio_bytes=%d error_event=%d peak=%d\n",
        frames,w.st.imported_audio,w.st.error_seen,w.st.peak);
    printf("events created=%u transcript=%u input_started=%u text=%u audio_delta=%u audio_done=%u canceled=%u done=%u error=%u\n",
        w.st.created,w.st.transcript,w.st.input_started,w.st.text,w.st.audio_delta,w.st.audio_done,w.st.canceled,w.st.done,w.st.error);
    fflush(stdout);
    w.deadline=now_ms()+3000;
    if(ws_send_frame(&w,1,(const unsigned char *)"{\"type\":\"session.close\"}",24)){}
    for(int i=0;i<100&&!w.st.finished;i++) {
        struct pollfd p={w.socket,POLLIN,0};
        if(now_ms()>=w.deadline)break;
        if(poll(&p,1,20)<=0)continue;
        drain(&w,&receiver);
    }
    { unsigned char close_frame[]={3,232}; w.deadline=now_ms()+1000; ws_send_frame(&w,8,close_frame,2); }
    result=(w.st.error_seen||stream_failed)?1:0;
cleanup:
    lx_handoff_discard();
    if(tools.active){pthread_join(tools.thread,NULL);tools.active=0;}
    free(w.st.pending_call);free(tools.event);free(tools.reply);
    for(unsigned i=0;i<tools.count;i++)free(tools.cached[i]);
    if(tools_ready)pthread_mutex_destroy(&tools.lock);
    if(result&&stage>=7&&w.c) {
        static const char close[]="{\"type\":\"session.close\"}";
        w.deadline=now_ms()+1000;ws_send_frame(&w,1,(const unsigned char *)close,sizeof(close)-1);
        {unsigned char code[]={3,232};ws_send_frame(&w,8,code,2);}
    }
    /* Let the queued answer finish playing before the devices are closed. */
    if(started) {
        int64_t until=now_ms()+30000;
        while(!g_stop&&!playback_idle(&ring)&&now_ms()<until)usleep(10000);
        pthread_mutex_lock(&ring.lock);ring.stop=1;pthread_cond_signal(&ring.ready);pthread_mutex_unlock(&ring.lock);
        pthread_join(player,NULL);
    }
    if(playback){if(g_stop)g_alsa.drop(playback);else g_alsa.drain(playback);g_alsa.close(playback);}
    if(started){pthread_mutex_destroy(&ring.lock);pthread_cond_destroy(&ring.ready);}
    if(w.c){a->cleanup(w.c);a->global_cleanup();}
    free(ring.data);
    /* Voice playback and cloud socket are closed before factory music starts. */
    if(play_after_reply&&!g_stop&&tools.ticket[0]){
        void *params=tj.object();tj.add(params,"ticket",tj.str(tools.ticket));void *out=tb_request("music.netease.commit",params,now_ms()+5000);
        int confirmed=tb_i(out,"ok")==1;const char *err=tb_s(out,"error");printf("music_deferred_commit=%s ticket_age_ms=%lld response=%s error=%s\n",confirmed?"pass":"fail",(long long)(now_ms()-tools.ticket_ms),out?"received":"timeout_or_transport",err&&tb_safe_id(err)?err:"none");fflush(stdout);if(out)tj.put(out);
    }
    memset(request,0,sizeof(request));
    /* Caller paused factory music for the session; resume only when the model
       did not touch the player and no deferred queue is about to start. */
    if(resume_music)*resume_music=!tools.music_touched&&!play_after_reply;
    if(result)printf("session_failure_stage=%d\n",stage);
    printf("session=%s frames=%d audio_bytes=%d written_frames=%ld write_errors=%d peak=%d samples=%ld\n",
        result?"fail":"pass",frames,w.st.imported_audio,g_written,g_werrs,g_apeak,g_asamples);
    fflush(stdout);
    lx_handoff_end();
    return result;
}

static int read_key(char *key,size_t size) {
    if(!fgets(key,size,stdin))return 0;
    key[strcspn(key,"\r\n")]=0;
    if(!key[0]||strlen(key)>4096)return 0;
    return 1;
}

/* ---------- one-off acknowledgement clip in the Doubao voice ---------- */

struct ack_ctx {
    struct client *w;
    unsigned char *pcm;size_t used,cap;
    int created,done,error;
};
static int ack_event(void *context,unsigned op,const unsigned char *p,size_t n){
    struct ack_ctx *c=context;struct client *w=c->w;
    if(op==9)return ws_send_frame(w,10,p,n);
    if(op==8){c->done=1;return 0;}
    if(op!=1)return 0;
    static char text[MAX_MESSAGE];
    size_t copy=n<sizeof(text)-1?n:sizeof(text)-1;
    memcpy(text,p,copy);text[copy]=0;
    void *o=w->json_parse(text),*v=NULL;
    if(!o)return 0;
    if(w->json_get(o,"type",&v)&&w->json_type(v)==6){
        const char *t=w->json_string(v);
        printf("ack_event=%s\n",t);fflush(stdout);
        if(!strcmp(t,"session.created"))c->created=1;
        else if(!strcmp(t,"response.output_text.delta")){
            void *txt=NULL;
            if(w->json_get(o,"delta",&txt)&&w->json_type(txt)==6)printf("ack_text=%s\n",w->json_string(txt));
            fflush(stdout);
        }
        else if(!strcmp(t,"response.output_audio.delta")){
            void *audio=NULL;
            if((w->json_get(o,"delta",&audio)||w->json_get(o,"audio",&audio))&&w->json_type(audio)==6){
                const char *b=w->json_string(audio);size_t bl=strlen(b);
                if(bl&&bl%4==0&&c->used+bl/4*3+8<=c->cap){
                    int k=w->b64_decode(c->pcm+c->used,(const unsigned char *)b,(int)bl);
                    if(k>0){int pad=(bl&&b[bl-1]=='=')?(bl>1&&b[bl-2]=='='?2:1):0;c->used+=(size_t)(k-pad);}
                }
            }
        }
        else if(!strcmp(t,"response.done")||!strcmp(t,"response.output_audio.done"))c->done=1;
        else if(!strcmp(t,"error")){c->error=1;c->done=1;puts("ack_error_event=1");fflush(stdout);}
    }
    w->json_put(o);
    return 0;
}
static int write_wav16(const char *path,const unsigned char *data,size_t bytes,int rate){
    unsigned char hdr[44]={0};unsigned int total=(unsigned int)(bytes+36);
    unsigned int br=(unsigned int)rate*2;
    memcpy(hdr,"RIFF",4);hdr[4]=(unsigned char)total;hdr[5]=(unsigned char)(total>>8);hdr[6]=(unsigned char)(total>>16);hdr[7]=(unsigned char)(total>>24);
    memcpy(hdr+8,"WAVEfmt ",8);hdr[16]=16;hdr[20]=1;hdr[22]=1;
    hdr[24]=(unsigned char)rate;hdr[25]=(unsigned char)(rate>>8);hdr[26]=(unsigned char)(rate>>16);hdr[27]=(unsigned char)(rate>>24);
    hdr[28]=(unsigned char)br;hdr[29]=(unsigned char)(br>>8);hdr[30]=(unsigned char)(br>>16);hdr[31]=(unsigned char)(br>>24);
    hdr[32]=2;hdr[34]=16;memcpy(hdr+36,"data",4);
    hdr[40]=(unsigned char)bytes;hdr[41]=(unsigned char)(bytes>>8);hdr[42]=(unsigned char)(bytes>>16);hdr[43]=(unsigned char)(bytes>>24);
    FILE *f=fopen(path,"wb");
    if(!f)return 1;
    int bad=fwrite(hdr,1,sizeof(hdr),f)!=sizeof(hdr)||fwrite(data,1,bytes,f)!=bytes;
    fclose(f);
    return bad;
}
/* Ask the realtime model for one short phrase and save it at 16 kHz mono. */
static int make_ack(struct curl_api *a,void *crypto,void *json,const char *key,const char *path,int missing){
    struct client w;struct lx_ws_receiver receiver;struct ack_ctx ctx={0};
    static unsigned char message[MAX_MESSAGE];
    char request[2048],header[8193]={0},expected[29],nonce_key[25]={0};
    unsigned char nonce[16];
    size_t used=0,sent=0;int length,status=0,result=1;int64_t until;
    memset(&w,0,sizeof(w));
    w.a=a;w.random_bytes=(int (*)(unsigned char *,int))dlsym(crypto,"RAND_bytes");
    w.b64_encode=(int (*)(unsigned char *,const unsigned char *,int))dlsym(crypto,"EVP_EncodeBlock");
    w.b64_decode=(int (*)(unsigned char *,const unsigned char *,int))dlsym(crypto,"EVP_DecodeBlock");
    w.json_parse=(void *(*)(const char *))dlsym(json,"json_tokener_parse");
    w.json_get=(int (*)(void *,const char *,void **))dlsym(json,"json_object_object_get_ex");
    w.json_string=(const char *(*)(void *))dlsym(json,"json_object_get_string");
    w.json_type=(int (*)(void *))dlsym(json,"json_object_get_type");
    w.json_put=(void (*)(void *))dlsym(json,"json_object_put");
    if(!w.random_bytes||!w.b64_encode||!w.b64_decode||!w.json_parse||!w.json_get||
       !w.json_string||!w.json_type||!w.json_put){puts("symbol lookup failed");return 1;}
    if(w.random_bytes(nonce,sizeof(nonce))!=1||
       w.b64_encode((unsigned char *)nonce_key,nonce,sizeof(nonce))!=24){puts("nonce failed");return 1;}
    {
        int (*sha1)(const unsigned char *,size_t,unsigned char *)=
            (int (*)(const unsigned char *,size_t,unsigned char *))dlsym(crypto,"SHA1");
        char combined[61];unsigned char digest[20];
        if(!sha1){puts("sha1 missing");return 1;}
        memcpy(combined,nonce_key,24);memcpy(combined+24,"258EAFA5-E914-47DA-95CA-C5AB0DC85B11",36);
        if(!sha1((const unsigned char *)combined,60,digest)||
           w.b64_encode((unsigned char *)expected,digest,20)!=28){puts("accept failed");return 1;}
    }
    ctx.pcm=malloc(RATE_IN*2*8);ctx.cap=RATE_IN*2*8;ctx.w=&w;
    if(!ctx.pcm)return 1;
    w.c=a->init();w.socket=-1;
    if(!w.c||a->global_init(CURL_GLOBAL_DEFAULT))goto done;
    if(a->setopt(w.c,CURLOPT_URL,"https://openspeech.bytedance.com/api/v3/duplex/realtime/dialogue")||
       a->setopt(w.c,CURLOPT_CONNECT_ONLY,1L)||a->setopt(w.c,CURLOPT_PROXY,"")||
       a->setopt(w.c,CURLOPT_CONNECTTIMEOUT_MS,5000L)||a->setopt(w.c,CURLOPT_TIMEOUT_MS,10000L)||
       a->setopt(w.c,CURLOPT_NOSIGNAL,1L)||a->perform(w.c)){puts("connect failed");goto done;}
    {
        long sock=-1,verify=-1;
        if(a->getinfo(w.c,CURLINFO_ACTIVESOCKET,&sock)||
           a->getinfo(w.c,CURLINFO_SSL_VERIFYRESULT,&verify)||verify||sock<0){puts("socket failed");goto done;}
        w.socket=(int)sock;
    }
    length=snprintf(request,sizeof(request),
        "GET /api/v3/duplex/realtime/dialogue HTTP/1.1\r\nHost: openspeech.bytedance.com\r\n"
        "Upgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Version: 13\r\n"
        "Sec-WebSocket-Key: %s\r\nX-Api-Key: %s\r\n\r\n",nonce_key,key);
    if(length<=0||(size_t)length>=sizeof(request))goto done;
    w.deadline=now_ms()+10000;
    while(sent<(size_t)length||(used<sizeof(header)-1&&!strstr(header,"\r\n\r\n"))) {
        size_t n=0;CURLcode code;
        int sending=sent<(size_t)length;
        int64_t remaining=w.deadline-now_ms();
        if(remaining<=0)goto done;
        if(sending)code=a->send(w.c,request+sent,(size_t)length-sent,&n);
        else code=a->recv(w.c,header+used,1,&n);
        if(code==CURLE_AGAIN) {
            struct pollfd p={w.socket,sending?POLLOUT:POLLIN,0};
            if(poll(&p,1,remaining>50?50:(int)remaining)<0)goto done;
            continue;
        }
        if(code||!n)goto done;
        if(sending)sent+=n;else {used+=n;header[used]=0;}
    }
    if(sscanf(header,"HTTP/1.1 %d",&status)!=1||status!=101||
       !lx_ws_upgrade_valid(header,used,expected)){fprintf(stderr,"upgrade failed http=%d\n",status);goto done;}
    memset(&receiver,0,sizeof(receiver));
    receiver.message=message;receiver.capacity=sizeof(message);
    {
        static const char create[]=
            "{\"type\":\"session.create\",\"session\":{\"model\":\"1.2.6.1\","
            "\"instructions\":\"你是豆包。用户刚叫了你的名字，请只回答：我在。不要多说。\","
            "\"audio\":{\"input\":{\"format\":{\"type\":\"pcm\",\"rate\":16000}},"
            "\"output\":{\"format\":{\"type\":\"pcm_s16le\",\"rate\":16000},"
            "\"voice\":\"zh_female_vv_uranus_bigtts\"}}}}";
        w.deadline=now_ms()+3000;
        if(ws_send_frame(&w,1,(const unsigned char *)create,strlen(create))){puts("create failed");goto done;}
    }
    until=now_ms()+5000;
    while(!ctx.created&&now_ms()<until) {
        struct pollfd p={w.socket,POLLIN,0};
        if(poll(&p,1,100)<=0)continue;
        unsigned char packet[4096];size_t n=0;CURLcode code=a->recv(w.c,packet,sizeof(packet),&n);
        if(code==CURLE_AGAIN)continue;
        if(code||!n)goto done;
        if(lx_ws_feed(&receiver,packet,n,ack_event,&ctx))goto done;
    }
    if(!ctx.created){puts("session not created");goto done;}
    {
        const char *mute="{\"type\":\"input_audio_mute.commit\"}";
        w.deadline=now_ms()+3000;
        if(ws_send_frame(&w,1,(const unsigned char *)mute,strlen(mute)))goto done;
        const char *text=missing?
          "{\"type\":\"speech_text_buffer.commit\",\"text\":\"还没有配置语音凭据，请先配置。\"}":
          "{\"type\":\"speech_text_buffer.commit\",\"text\":\"收到。\"}";
        w.deadline=now_ms()+3000;
        if(ws_send_frame(&w,1,(const unsigned char *)text,strlen(text))){puts("ack text failed");goto done;}
    }
    until=now_ms()+8000;
    w.deadline=until;
    while(!ctx.done&&now_ms()<until) {
        struct pollfd p={w.socket,POLLIN,0};
        if(poll(&p,1,100)<=0)continue;
        unsigned char packet[4096];size_t n=0;CURLcode code=a->recv(w.c,packet,sizeof(packet),&n);
        if(code==CURLE_AGAIN)continue;
        if(code||!n)break;
        if(lx_ws_feed(&receiver,packet,n,ack_event,&ctx))break;
    }
    if(ctx.used>0&&ctx.done&&!ctx.error){
        /* The model's PCM sits well below the factory prompt level; normalise so
           the acknowledgement matches the speaker's other cues. */
        short *s=(short *)ctx.pcm;size_t n=ctx.used/2;int peak=0;
        for(size_t i=0;i<n;i++){int v=s[i];if(v<0)v=-v;if(v>peak)peak=v;}
        if(peak>0&&peak<30000){
            for(size_t i=0;i<n;i++){long v=(long)s[i]*30000/peak;if(v>32767)v=32767;if(v<-32768)v=-32768;s[i]=(short)v;}
            printf("ack_normalized_from_peak=%d\n",peak);
        }
        if(write_wav16(path,ctx.pcm,ctx.used,RATE_IN)){puts("write failed");goto done;}
        printf("ack_bytes=%u duration_ms=%u peak=%d path=%s\n",(unsigned)ctx.used,(unsigned)(ctx.used/2*1000/RATE_IN),
            (int)(peak?30000:0),path);
        result=0;
    }else puts("ack_empty_or_error");
done:
    if(w.c){a->cleanup(w.c);a->global_cleanup();}
    free(ctx.pcm);
    fflush(stdout);
    return result;
}

int main(int argc,char **argv) {
    static char key[4097];
    static short preroll[LX_KWS_PREROLL];
    int mode=MODE_DUPLEX,seconds=30,result=1,dump_wav=0;
    const char *wav=NULL;
    void *curl=NULL,*crypto=NULL,*json=NULL;
    struct curl_api a;
    struct lx_kws *kws=NULL;
    static struct lx_capture capture;
    int capture_ready=0,disable_aec=0;

    signal(SIGINT,on_signal);signal(SIGTERM,on_signal);signal(SIGHUP,on_signal);
    if(argc==2&&!strcmp(argv[1],"--factory-pcm")) {
        if(lx_factory_init())return 2;
        short pcm[160];unsigned frames=0,wakes=0;
        while(!g_stop&&frames<200000) {
            size_t got=fread(pcm,sizeof(short),160,stdin);if(!got)break;
            if(got<160)memset(pcm+got,0,(160-got)*sizeof(short));
            wakes+=(unsigned)lx_factory_push(pcm,160);frames++;
        }
        int error=ferror(stdin);
        printf("factory_pcm frames=%u confirmed=%u input_error=%d\n",frames,wakes,error);
        lx_factory_close();return error?3:0;
    }
    if(argc==2&&!strcmp(argv[1],"--transport-self-test"))return frame_send_test();
    if(argc==2&&!strcmp(argv[1],"--tool-catalog")){
        void *lib=dlopen("libjson-c.so.2",RTLD_NOW|RTLD_LOCAL);if(!lib||tb_init(lib))return 1;
        void *tools=tb_catalog(now_ms()+3000);int rc=tools?0:1;
        printf("tool_catalog=%s\n",tools?tj.encode(tools):"unavailable");
        if(tools)tj.put(tools);dlclose(lib);return rc;
    }
    if(argc==2&&(!strcmp(argv[1],"--tool-test")||!strcmp(argv[1],"--music-tool-smoke"))){
        char event[8192];void *lib=dlopen("libjson-c.so.2",RTLD_NOW|RTLD_LOCAL);if(!lib||tb_init(lib)||!fgets(event,sizeof(event),stdin))return 1;
        int smoke_rc=0;struct tb_job j={0};if(pthread_mutex_init(&j.lock,NULL))return 1;j.event=event;tb_worker(&j);if(!strcmp(argv[1],"--music-tool-smoke")){
            smoke_rc=1;printf("music_smoke_prepared=%d\n",j.ticket[0]!=0);
            if(j.ticket[0]){void *params=tj.object();tj.add(params,"ticket",tj.str(j.ticket));void *out=tb_request("music.netease.commit",params,now_ms()+5000);smoke_rc=tb_i(out,"ok")!=1;const char *err=tb_s(out,"error");printf("music_smoke_commit=%d response=%s error=%s ticket_age_ms=%lld\n",tb_i(out,"ok")==1,out?"received":"timeout_or_transport",err&&tb_safe_id(err)?err:"none",(long long)(now_ms()-j.ticket_ms));if(out)tj.put(out);}
        }else if(j.reply)puts(j.reply);
        printf("tool_end_session=%d\n",j.end_session);
        for(unsigned i=0;i<j.count;i++)free(j.cached[i]);free(j.reply);pthread_mutex_destroy(&j.lock);dlclose(lib);return smoke_rc;
    }
    {
        const char *e=getenv("LX_SESSION_MAX");
        if(e){char *end;long v=strtol(e,&end,10);if(*end||v<1||v>1800){fputs("invalid session safety limit\n",stderr);return 2;}g_session_max_ms=(int)v*1000;}
        e=getenv("LX_NO_SPEECH");
        if(e&&atoi(e)>0&&atoi(e)<=30)g_no_speech_ms=atoi(e)*1000;
        e=getenv("LX_FOLLOWUP");
        if(e&&atoi(e)>=0&&atoi(e)<=30)g_followup_ms=atoi(e)*1000;
        e=getenv("LX_NO_RESPONSE");
        if(e&&atoi(e)>=1&&atoi(e)<=60)g_no_response_ms=atoi(e)*1000;
        e=getenv("LX_VOICE_GATE");
        if(e&&atoi(e)>=100&&atoi(e)<=100000)g_voice_gate=atoi(e);
        e=getenv("LX_ANSWER_GAIN");
        if(e&&atoi(e)>=100&&atoi(e)<=400)g_answer_gain=atoi(e);
        e=getenv("LX06_DUPLEX_MODE");
        if(e&&!strcmp(e,"half"))g_full_duplex=0;
        else if(e&&strcmp(e,"full")){fputs("invalid duplex mode\n",stderr);return 2;}
        e=getenv("LX06_AEC");
        if(e&&!strcmp(e,"0"))disable_aec=1;
    }

    if(argc>1&&!strcmp(argv[1],"--kws-score")) {
        if(argc<3){fputs("usage: --kws-score <wav>\n",stderr);return 2;}
        mode=MODE_SCORE;wav=argv[2];
    } else if(argc>1&&!strcmp(argv[1],"--kws-dump")) {
        if(argc<3){fputs("usage: --kws-dump <wav>\n",stderr);return 2;}
        mode=MODE_SCORE;wav=argv[2];dump_wav=1;
    } else if(argc>1&&!strcmp(argv[1],"--wake-audit")) {
        mode=MODE_WAKE_AUDIT;seconds=argc>2?atoi(argv[2]):60;
    } else if(argc>1&&!strcmp(argv[1],"--kws-live")) {
        mode=MODE_LIVE;seconds=argc>2?atoi(argv[2]):60;
    } else if(argc>1&&!strcmp(argv[1],"--kws-capture")) {
        mode=MODE_CAPTURE;seconds=argc>2?atoi(argv[2]):20;
    } else if(argc>1&&!strcmp(argv[1],"--kws-wav")) {
        if(argc<3){fputs("usage: --kws-wav <wav> [threshold]\n",stderr);return 2;}
        mode=MODE_KWS_WAV;wav=argv[2];
        if(argc>3)lx_kws_threshold_override=(float)atof(argv[3]);
    } else if(argc>1&&!strcmp(argv[1],"--kws")) {
        mode=MODE_KWS;seconds=argc>2?atoi(argv[2]):60;
        if(argc>3)lx_kws_threshold_override=(float)atof(argv[3]);
    } else if(argc>1&&!strcmp(argv[1],"--feedback-test")) {
        mode=MODE_TONE;g_feedback_test=1;
    } else if(argc>1&&!strcmp(argv[1],"--tone")) {
        mode=MODE_TONE;
    } else if(argc>1&&argv[1][0]!='-') {
        seconds=atoi(argv[1]);
    }
    {
        int i;
        for(i=1;i<argc;i++)if(!strcmp(argv[i],"--no-aec"))disable_aec=1;
        for(i=1;i+1<argc;i++) {
            if(!strcmp(argv[i],"--listen-once"))g_listen_once=1;
            if(!strcmp(argv[i],"--daemon"))g_daemon_pidfile=argv[i+1];
            if(!strcmp(argv[i],"--followup")) {
                char *end;long value=strtol(argv[i+1],&end,10);
                if(*end||value<0||value>30)return 2;
                g_followup_ms=(int)value*1000;
            }
        }
    }
    if(mode==MODE_SCORE)return lx_kws_score_file(wav,stdout,dump_wav);
    if(g_daemon_pidfile) {
        int d;
        if(mode!=MODE_KWS){fputs("--daemon requires --kws\n",stderr);return 2;}
        d=daemonize();
        if(d!=0)return d>0?0:1;
    }
    if(prctl(PR_SET_DUMPABLE,0L,0L,0L,0L)||prctl(PR_GET_DUMPABLE,0L,0L,0L,0L)!=0)return 1;
    if(seconds<5)seconds=5;

    curl=dlopen("libcurl.so.4",RTLD_NOW|RTLD_LOCAL);
    crypto=dlopen("libcrypto.so.1.0.0",RTLD_NOW|RTLD_LOCAL);
    json=dlopen("libjson-c.so.2",RTLD_NOW|RTLD_LOCAL);
    g_alsa.lib=dlopen("libasound.so.2",RTLD_NOW|RTLD_LOCAL);
    if(!curl||!crypto||!json||!g_alsa.lib){fputs("library load failed\n",stderr);return 1;}
#define S(ptr,target,sym) *(void **)(&ptr)=dlsym(target,sym); if(!ptr)return 1
    S(a.global_init,curl,"curl_global_init");S(a.global_cleanup,curl,"curl_global_cleanup");
    S(a.init,curl,"curl_easy_init");S(a.setopt,curl,"curl_easy_setopt");
    S(a.perform,curl,"curl_easy_perform");S(a.getinfo,curl,"curl_easy_getinfo");
    S(a.cleanup,curl,"curl_easy_cleanup");S(a.send,curl,"curl_easy_send");
    S(a.recv,curl,"curl_easy_recv");
    S(g_alsa.open,g_alsa.lib,"snd_pcm_open");
    S(g_alsa.set_params,g_alsa.lib,"snd_pcm_set_params");
    S(g_alsa.readi,g_alsa.lib,"snd_pcm_readi");
    S(g_alsa.writei,g_alsa.lib,"snd_pcm_writei");
    S(g_alsa.recover,g_alsa.lib,"snd_pcm_recover");
    S(g_alsa.drain,g_alsa.lib,"snd_pcm_drain");
    S(g_alsa.delay,g_alsa.lib,"snd_pcm_delay");
    S(g_alsa.drop,g_alsa.lib,"snd_pcm_drop");
    S(g_alsa.prepare,g_alsa.lib,"snd_pcm_prepare");
    S(g_alsa.close,g_alsa.lib,"snd_pcm_close");
    S(g_alsa.strerror,g_alsa.lib,"snd_strerror");
#undef S

    puts("stage=libs_ok");fflush(stdout);
    print_kws_verdict();
    if(argc==3&&(!strcmp(argv[1],"--voice-fixture")||!strcmp(argv[1],"--voice-aec-fixture"))) {
        puts("cloud_fixture_disabled=unfiltered_source local_wav_diagnostics_only");return 2;
    }
    if((argc==3||argc==4)&&!strcmp(argv[1],"--make-ack")) {
        if(!read_key(key,sizeof(key))){fputs("missing api key\n",stderr);return 2;}
        if(argc==4&&strcmp(argv[3],"--missing-token"))return 2;
        return make_ack(&a,crypto,json,key,argv[2],argc==4);
    }
    if(argc==3&&!strcmp(argv[1],"--aec-test")) {
        /* Same chain the factory runs: Speex AEC + preprocessor on 128-sample
           frames at 16 kHz, using mic ch0 and the playback reference ch6. */
        int secs=atoi(argv[2]);if(secs<1||secs>10)secs=2;
        void *sp=dlopen("libspeexdsp.so.1",RTLD_NOW|RTLD_LOCAL);
        if(!sp){printf("speex_dlopen_failed=%s\n",dlerror());return 1;}
        void *(*einit)(int,int)=(void *(*)(int,int))dlsym(sp,"speex_echo_state_init");
        int (*ectl)(void *,int,void *)=(int (*)(void *,int,void *))dlsym(sp,"speex_echo_ctl");
        if(!ectl)ectl=(int (*)(void *,int,void *))dlsym(sp,"speex_echo_state_ctl");
        int (*ecancel)(void *,const short *,const short *,short *)=(int (*)(void *,const short *,const short *,short *))dlsym(sp,"speex_echo_cancellation");
        void *(*pinit)(int,int)=(void *(*)(int,int))dlsym(sp,"speex_preprocess_state_init");
        int (*pctl)(void *,int,void *)=(int (*)(void *,int,void *))dlsym(sp,"speex_preprocess_ctl");
        int (*prun)(void *,short *)=(int (*)(void *,short *))dlsym(sp,"speex_preprocess_run");
        if(!einit||!ecancel||!pinit||!pctl||!prun){printf("speex_symbols_missing einit=%d cancel=%d pinit=%d pctl=%d prun=%d\n",
            !!einit,!!ecancel,!!pinit,!!pctl,!!prun);return 1;}
        void *st=einit(128,4096);int rate=16000;
        if(!st){puts("speex_init_failed");return 1;}
        if(ectl){if(ectl(st,24,&rate))puts("speex_rate_ctl_failed");else puts("speex_rate=16000");}
        else puts("speex_ctl_missing_default_rate=8000");
        void *pp=pinit(128,16000);
        if(!pp||pctl(pp,24,st)){puts("speex_pp_failed");return 1;}
        void *pcm=NULL;static short raw[384*8*2];int32_t *buf=(int32_t *)raw;
        if(g_alsa.open(&pcm,"Capture",1,0)){puts("aec_open_failed");return 1;}
        if(g_alsa.set_params(pcm,10,3,8,48000,0,200000)){puts("aec_params_failed");g_alsa.close(pcm);return 1;}
        short mic[128],ref[128],out[128];double sm=0,so=0,sr=0;long blocks=0;
        while(blocks<(long)secs*125) {
            long got=g_alsa.readi(pcm,raw,384);
            if(got<0){if(g_alsa.recover(pcm,(int)got,1)<0)break;continue;}
            if(got<384)continue;
            for(int i=0;i<128;i++){
                long vm=0,vr=0;
                for(int k=0;k<3;k++){vm+=buf[(i*3+k)*8+0];vr+=buf[(i*3+k)*8+6];}
                mic[i]=(short)((vm/3)>>16);ref[i]=(short)((vr/3)>>16);
            }
            ecancel(st,mic,ref,out);
            prun(pp,out);
            for(int i=0;i<128;i++){sm+=(double)mic[i]*mic[i];so+=(double)out[i]*out[i];sr+=(double)ref[i]*ref[i];}
            blocks++;
        }
        g_alsa.close(pcm);
        if(blocks<=0){puts("aec_no_frames");return 1;}
        printf("aec frames=%ld mic_rms=%.0f out_rms=%.0f ref_rms=%.0f ratio=%.2f\n",
            blocks*128,sm/(double)(blocks*128),so/(double)(blocks*128),sr/(double)(blocks*128),
            so>0?sm/so:0.0);
        return 0;
    }
    if(argc==3&&!strcmp(argv[1],"--mic-compare")) {
        /* Score the same captured speech through several capture variants so a
           missed wake can be attributed to the microphone mix, the echo
           canceller or the preprocessor. Audio stays in memory. */
        int secs=atoi(argv[2]);if(secs<5||secs>30)secs=15;
        void *sp=dlopen("libspeexdsp.so.1",RTLD_NOW|RTLD_LOCAL);
        if(!sp){printf("speex_dlopen_failed=%s\n",dlerror());return 1;}
        void *(*einit)(int,int)=(void *(*)(int,int))dlsym(sp,"speex_echo_state_init");
        int (*ecancel)(void *,const short *,const short *,short *)=(int (*)(void *,const short *,const short *,short *))dlsym(sp,"speex_echo_cancellation");
        void *(*pinit)(int,int)=(void *(*)(int,int))dlsym(sp,"speex_preprocess_state_init");
        int (*pctl)(void *,int,void *)=(int (*)(void *,int,void *))dlsym(sp,"speex_preprocess_ctl");
        int (*prun)(void *,short *)=(int (*)(void *,short *))dlsym(sp,"speex_preprocess_run");
        int (*ectl)(void *,int,void *)=(int (*)(void *,int,void *))dlsym(sp,"speex_echo_ctl");
        if(!ectl)ectl=(int (*)(void *,int,void *))dlsym(sp,"speex_echo_state_ctl");
        if(!einit||!ecancel||!pinit||!pctl||!prun){puts("speex_symbols_missing");return 1;}
        void *st=einit(128,4096),*st2=einit(128,4096),*st3=einit(128,4096),*st4=einit(128,4096);
        void *pp=pinit(128,16000),*pp2=pinit(128,16000);
        if(!st||!st2||!st3||!st4||!pp||!pp2||pctl(pp,24,st)||pctl(pp2,24,st3)){puts("speex_init_failed");return 1;}
        if(ectl){int rate=16000;ectl(st,24,&rate);ectl(st2,24,&rate);ectl(st3,24,&rate);ectl(st4,24,&rate);}
        static int32_t raw[480*8];
        void *pcm=NULL;
        if(g_alsa.open(&pcm,"Capture",1,0)){puts("compare_open_failed");return 1;}
        if(g_alsa.set_params(pcm,10,3,8,48000,0,200000)){puts("compare_params_failed");g_alsa.close(pcm);return 1;}
        int total=secs*RATE_IN,i,c,f;
        short *buf[11];double sums[11]={0};
        const char *names[11]={"ch0","ch1","ch2","ch3","ch4","ch5","mix6",
            "ch0_aec_pp","ch0_aec","mix6_aec_pp","mix6_aec"};
        for(i=0;i<11;i++){buf[i]=calloc((size_t)total,sizeof(short));if(!buf[i]){puts("compare_alloc_failed");return 1;}}
        short f8[9][128],ref[128],out[128],out2[128],out3[128],out4[128];
        long acc[6]={0,0,0,0,0,0},accmix=0,accref=0;
        int cnt=0,fill=0,outn=0;
        while(outn<total) {
            long got=0;
            while(got<480) {
                long r=g_alsa.readi(pcm,raw+got*8,(unsigned long)(480-got));
                if(r<0){if(g_alsa.recover(pcm,(int)r,1)<0)break;continue;}
                if(!r)continue;
                got+=r;
            }
            if(got<480)break;
            for(f=0;f<480;f++) {
                int32_t *fr=raw+f*8;
                for(c=0;c<6;c++){acc[c]+=fr[c];accmix+=fr[c];}
                accref+=fr[6];
                if(++cnt<3)continue;
                for(c=0;c<6;c++)f8[c][fill]=(short)((acc[c]/3)>>16);
                f8[6][fill]=(short)((accmix/18)>>16);
                f8[7][fill]=(short)((acc[0]/3)>>16);
                ref[fill]=(short)((accref/3)>>16);
                for(c=0;c<6;c++)acc[c]=0;
                accmix=0;accref=0;cnt=0;
                if(++fill<128)continue;
                for(c=0;c<7;c++)memcpy(buf[c]+outn,f8[c],sizeof(f8[c]));
                ecancel(st,f8[7],ref,out);prun(pp,out);memcpy(buf[7]+outn,out,sizeof(out));
                ecancel(st2,f8[7],ref,out2);memcpy(buf[8]+outn,out2,sizeof(out2));
                ecancel(st3,f8[6],ref,out3);prun(pp2,out3);memcpy(buf[9]+outn,out3,sizeof(out3));
                ecancel(st4,f8[6],ref,out4);memcpy(buf[10]+outn,out4,sizeof(out4));
                outn+=128;fill=0;
            }
        }
        g_alsa.close(pcm);
        if(outn<=0){puts("compare_no_frames");return 1;}
        for(i=0;i<11;i++)for(f=0;f<outn;f++)sums[i]+=(double)buf[i][f]*buf[i][f];
        for(i=0;i<11;i++) {
            struct kws_measure m;
            kws_measure(buf[i],outn,&m);
            printf("compare stream=%s best=%.4f wakes=%d scored=%d ms=%.0f\n",
                names[i],(double)m.best,m.wakes,m.scored,sums[i]/(double)outn);
            free(buf[i]);
        }
        fflush(stdout);
        return 0;
    }
    if(argc==3&&!strcmp(argv[1],"--afe-channels")) {
        int secs=atoi(argv[2]);if(secs<1||secs>10)secs=2;
        void *pcm=NULL;static short raw[480*8*2];int32_t *buf=(int32_t *)raw;double sum[8]={0},peak[8]={0};long frames=0;
        if(g_alsa.open(&pcm,"Capture",1,0)){puts("afe_open_failed");return 1;}
        if(g_alsa.set_params(pcm,10,3,8,48000,0,200000)){puts("afe_params_failed");g_alsa.close(pcm);return 1;}
        while(frames<(long)secs*50) {
            long got=g_alsa.readi(pcm,raw,480);
            if(got<0){if(g_alsa.recover(pcm,(int)got,1)<0)break;continue;}
            for(long f=0;f<got;f++)for(int c=0;c<8;c++){
                double v=buf[f*8+c];
                sum[c]+=v*v;if(v<0)v=-v;if(v>peak[c])peak[c]=v;
            }
            frames++;
        }
        g_alsa.close(pcm);
        if(frames<=0){puts("afe_no_frames");return 1;}
        for(int c=0;c<8;c++){
            double rms=sum[c]/(double)(frames*480);
            printf("afe_ch=%d rms=%.0f peak=%.0f\n",c,rms,peak[c]);
        }
        return 0;
    }
    if(argc==3&&!strcmp(argv[1],"--wav-peak")) {
        int n=0;short *pcm=lx_kws_read_wav(argv[2],&n);
        if(!pcm){printf("wav_unreadable=%s\n",argv[2]);return 2;}
        int peak=0;for(int i=0;i<n;i++){int v=pcm[i];if(v<0)v=-v;if(v>peak)peak=v;}
        printf("wav=%s samples=%d duration_ms=%d peak=%d\n",argv[2],n,n*1000/RATE_IN,peak);
        free(pcm);
        return 0;
    }
    {
        struct lx_capture_alsa cops={g_alsa.open,g_alsa.set_params,g_alsa.readi,g_alsa.recover,g_alsa.close};
        if(lx_capture_open(&capture,&cops,!disable_aec,RATE_IN)) {
            puts("capture open failed");goto cleanup;
        }
        capture_ready=1;
    }

    if(mode==MODE_WAKE_AUDIT) {
        struct lx_kws *ref=calloc(1,sizeof(*ref));
        kws=calloc(1,sizeof(*kws));
        if(!ref||!kws){free(ref);goto cleanup;}
        if(!capture.aec){puts("wake_audit_requires_reference");free(ref);goto cleanup;}
        lx_kws_init(kws);lx_kws_init(ref);
        lx_kws_threshold_override=15.0f;
        struct wake_audit_ctx audit={ref,0};short mic[FRAME_IN];
        int64_t until=now_ms()+(int64_t)seconds*1000;
        puts("wake_audit=no_cloud_no_playback_control_no_audio_files");fflush(stdout);
        while(!g_stop&&now_ms()<until) {
            long got=lx_capture_read(&capture,mic,FRAME_IN);
            if(got<0){if(lx_capture_recover(&capture,got)<0)break;continue;}
            if(!got)continue;
            lx_kws_push(ref,capture.reference_read,(int)got,NULL,NULL);
            lx_kws_push(kws,mic,(int)got,wake_audit_report,&audit);
            kws->woke=0;ref->woke=0;
        }
        printf("wake_audit_done windows=%d candidates=%d\n",kws->evals,audit.candidates);
        free(ref);result=0;goto cleanup;
    }

    if(mode==MODE_CAPTURE) {
        short *buffer;
        int want,got=0;
        int64_t stop;
        if(seconds>60)seconds=60;
        want=seconds*RATE_IN;
        buffer=malloc((size_t)want*sizeof(short));
        if(!buffer){puts("capture alloc failed");goto cleanup;}
        printf("stage=capture seconds=%d\n",seconds);fflush(stdout);
        stop=now_ms()+(int64_t)(seconds+3)*1000;
        while(got<want&&now_ms()<stop) {
            int take=want-got;
            long r;
            if(take>FRAME_IN)take=FRAME_IN;
            r=lx_capture_read(&capture,buffer+got,take);
            if(r<0){if(lx_capture_recover(&capture,r)<0)break;continue;}
            if(!r)continue;
            got+=(int)r;
        }
        printf("captured_samples=%d\n",got);fflush(stdout);
        kws_scan_buffer(buffer,got);
        free(buffer);
        result=0;
        goto cleanup;
    }

    if(mode==MODE_LIVE) {
        kws=calloc(1,sizeof(*kws));
        if(!kws){puts("kws alloc failed");goto cleanup;}
        lx_kws_init(kws);
        kws_listen(&capture,kws,seconds,0,NULL);
        result=0;
        goto cleanup;
    }

    if(g_feedback_test) {
        feedback_led(1);result=feedback_ack();sleep(1);
        feedback_led(2);sleep(2);feedback_led(3);sleep(2);
        feedback_led(1);sleep(1);feedback_led(0);goto cleanup;
    }

    if(mode==MODE_TONE) {
        void *playback=NULL;
        if(g_alsa.open(&playback,"default",0,0)||
           g_alsa.set_params(playback,2,3,1,RATE_OUT,1,100000)) {
            puts("playback open failed");goto cleanup;
        }
        g_play=playback;
        {
            static short tone[960];
            int chunk,k;
            for(chunk=0;chunk<50;chunk++) {
                for(k=0;k<960;k++){int idx=chunk*960+k;tone[k]=(short)(((idx/60)%2)?3000:-3000);}
                if(playback_write((const unsigned char *)tone,sizeof(tone)))break;
            }
            printf("tone_done written_frames=%ld errors=%d\n",g_written,g_werrs);
        }
        g_alsa.drain(playback);g_alsa.close(playback);
        result=0;
        goto cleanup;
    }

    if(mode==MODE_KWS_WAV) {
        int nsamp=0,frames=0;
        short *pcm=lx_kws_read_wav(wav,&nsamp);
        struct kws_ctx ctx;
        if(!pcm){printf("wav_open_failed path=%s\n",wav);result=2;goto cleanup;}
        kws=calloc(1,sizeof(*kws));
        if(!kws){free(pcm);result=1;goto cleanup;}
        lx_kws_init(kws);
        memset(&ctx,0,sizeof(ctx));
        ctx.best=1e30f;
        ctx.start_ms=now_ms();
        printf("stage=replay wav=%s samples=%d threshold=%.4f\n",
            wav,nsamp,(double)lx_kws_threshold());fflush(stdout);
        for(frames=0;frames*FRAME_IN<nsamp&&!kws->woke;frames++) {
            int take=nsamp-frames*FRAME_IN;
            if(take>FRAME_IN)take=FRAME_IN;
            lx_kws_push(kws,pcm+frames*FRAME_IN,take,kws_log,&ctx);
            usleep(20000);
        }
        free(pcm);
        printf("replay_done frames=%d windows=%d best_dist=%.4f woke=%d\n",
            frames,kws->evals,(double)ctx.best,kws->woke);fflush(stdout);
        if(g_stop){puts("replay_stopped=signal");result=0;goto cleanup;}
        if(!kws->woke){puts("replay_timeout no_wake");result=3;goto cleanup;}
        lx_kws_preroll(kws,preroll);
        printf("stage=wake_detected preroll_samples=%d best_dist=%.4f\n",
            LX_KWS_PREROLL,(double)kws->best_dist);fflush(stdout);
        if(!read_key(key,sizeof(key))) {
            puts("wake_detected no_key_stdin detection_only=1");fflush(stdout);
            result=0;
            goto cleanup;
        }
        result=run_session(&a,crypto,json,key,&capture,mode,seconds,preroll,LX_KWS_PREROLL,NULL,NULL);
        goto cleanup;
    }

    if(mode==MODE_KWS) {
        int have_key=0;
        kws=calloc(1,sizeof(*kws));
        if(!kws){puts("kws alloc failed");goto cleanup;}
        lx_kws_init(kws);
        /* A daemon gets the key from the FIFO at startup, while the SSH
           session that created it is still open; waiting until the first wake
           loses the key when that session closes. */
        {
            struct pollfd kp={0,POLLIN,0};
            if(poll(&kp,1,1500)>0&&read_key(key,sizeof(key))) {
                have_key=1;puts("key_loaded=1");
            } else {
                puts("key_loaded=0 local_wake_only=1");
            }
            fflush(stdout);
        }
        if(!tb_init(json))g_tools_ready=1;
        while(!g_stop) {
            publish_state("standby");
            printf("stage=standby seconds=%d\n",seconds);fflush(stdout);
            if(!g_listen_once&&!kws_listen(&capture,kws,seconds,0,preroll)) {
                if(g_stop){puts("standby_stopped=signal");result=0;break;}
                if(g_daemon_pidfile){puts("standby_window_renewed");continue;}
                puts("standby_timeout no_wake");result=3;break;
            }
            if(g_stop){puts("standby_stopped=signal");result=0;break;}
            printf("stage=wake_detected preroll_samples=%d best_dist=%.4f\n",
                LX_KWS_PREROLL,(double)kws->best_dist);fflush(stdout);
            g_wake_started_ms=now_ms();
            if(have_key&&lx_handoff_begin(&capture,preroll,g_listen_once?0:LX_KWS_PREROLL)){
                puts("wake_handoff_begin_failed");result=1;break;
            }
            /* Music masks both the wake word and the request. Pause the factory
               player before the acknowledgement, then resume if the model did
               not touch playback during the session. */
            int music_paused=0;
            if(g_candidate_paused){music_paused=1;g_candidate_paused=0;puts("music_paused_for_candidate_confirmed");fflush(stdout);}
            else if(g_tools_ready) {
                void *st=tb_request("music.status",tj.object(),now_ms()+1200);
                if(st) {
                    if(tb_i(st,"status")==1) {
                        void *r=tb_request("music.pause",tj.object(),now_ms()+1500);
                        if(r) {music_paused=1;tj.put(r);}
                    }
                    tj.put(st);
                }
            }
            if(music_paused){puts("music_paused_for_listening=1");fflush(stdout);}
            feedback_led(1);
            if(!have_key) {
                feedback_clip("/data/lx06-agent/missing_token.wav",0);
                puts("wake_local_ack no_key=1 cloud_connected=0");fflush(stdout);
                feedback_led(0);
                if(music_paused&&g_tools_ready){void *r=tb_request("music.resume",tj.object(),now_ms()+1500);if(r)tj.put(r);}
                kws->woke=0;kws->hits=0;kws->best_dist=1e30f;
                if(g_listen_once){result=0;break;}
                continue;
            }
            if(!g_stop)feedback_led(2);
            int resume_music=0;
            result=g_stop?0:run_session(&a,crypto,json,key,&capture,mode,seconds,preroll,LX_KWS_PREROLL,&resume_music,kws);
            lx_handoff_end();
            if(music_paused&&resume_music&&!g_stop) {
                void *r=tb_request("music.resume",tj.object(),now_ms()+1500);
                if(r){puts("music_resumed_after_session=1");fflush(stdout);tj.put(r);}
            }
            if(g_listen_once)break;
            feedback_led(0);
            /* Back to local standby: keep the learned noise floor, drop the hits. */
            kws->woke=0;kws->hits=0;kws->best_dist=1e30f;kws->nframes=0;kws->fill=0;kws->hops=0;
            puts("stage=standby_resumed");fflush(stdout);
        }
        goto cleanup;
    }

    if(!read_key(key,sizeof(key))){fputs("missing api key\n",stderr);result=2;goto cleanup;}
    result=run_session(&a,crypto,json,key,&capture,mode,seconds,NULL,0,NULL,NULL);

cleanup:
    publish_state("stopping");
    feedback_led(0);
    lx_handoff_end();
    if(capture_ready)lx_capture_close(&capture);
    lx_factory_close();
    free(kws);
    { volatile char *wipe=key;for(size_t k=0;k<sizeof(key);k++)wipe[k]=0; }
    if(curl)dlclose(curl);if(crypto)dlclose(crypto);if(json)dlclose(json);if(g_alsa.lib)dlclose(g_alsa.lib);
    publish_state(result?"error":"stopped");
    printf("duplex_client=%s\n",result?"fail":"pass");
    return result;
}
