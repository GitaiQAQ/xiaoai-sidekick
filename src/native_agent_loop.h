/* Provider-agnostic agent session loop. A provider moves audio and JSON to a
   cloud model; the host owns the device side (microphone, playback, lamp,
   runtime state) and the tool worker. The loop keeps the turn rules: when to
   mute, when a tool round may run, when a follow-up window opens and when the
   session budget is spent. */
#ifndef LX06_AGENT_LOOP_H
#define LX06_AGENT_LOOP_H
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "native_session_policy.h"

struct lx_agent_stats {
 int session_ready,finished,error_seen,imported_audio,peak;
 unsigned created,transcript,text,audio_delta,audio_done,committed,done,error,input_started,canceled;
 char *pending_call; /* provider-owned: one outstanding function-call event */
};

struct lx_agent_provider {
 void *ctx;
 struct lx_agent_stats *(*stats)(void *ctx);
 int (*send_audio)(void *ctx,const short *pcm,int samples);
 int (*send_json)(void *ctx,const char *json);
 int (*pump)(void *ctx,int timeout_ms); /* read + dispatch, 0 ok, <0 transport error */
};

struct lx_agent_host {
 void *ctx;
 int (*stop)(void *ctx);
 long (*capture)(void *ctx,short *pcm,int frames);
 int (*capture_recover)(void *ctx,long error); /* <0 fatal */
 void (*play_delta)(void *ctx,const char *b64);
 int (*playback_idle)(void *ctx);
 void (*led)(void *ctx,int state);
 void (*publish)(void *ctx,const char *phase);
 int (*tool_start)(void *ctx,const char *event_json);
 int (*tool_busy)(void *ctx);
 int (*tool_take)(void *ctx,char *reply,size_t cap); /* 1 ready, 0 running, -1 no reply */
 int (*tool_has_ticket)(void *ctx);
 int (*tool_end)(void *ctx); /* set by a session_end tool call */
 /* Wake-word barge-in while the reply plays. */
 void (*barge_reset)(void *ctx);
 int (*barge_check)(void *ctx,const short *pcm,int samples);
 void (*barge)(void *ctx);
};

struct lx_agent_limits {
 int session_max_ms,followup_ms,no_speech_ms,answer_mode;
 /* Close a session that never produced an answer: a wake with no usable
    speech must not keep uploading until the wall-clock budget. */
 int no_response_ms,voice_gate,full_duplex;
};

struct lx_agent_result {
 int frames,stream_failed,turns;
 const char *reason;
};

#ifndef LX_AGENT_NOW_MS
static int64_t lx_agent_now(void){
 struct timespec t;
 if(clock_gettime(CLOCK_MONOTONIC,&t))return -1;
 return (int64_t)t.tv_sec*1000+t.tv_nsec/1000000;
}
#else
#define lx_agent_now LX_AGENT_NOW_MS
#endif

/* Returns 0 when the session ended normally, 1 when a transport or device
   error ended it. preroll is optional audio captured before the session. */
static int lx_agent_run(struct lx_agent_provider *p,struct lx_agent_host *h,
 const struct lx_agent_limits *lim,const short *preroll,int preroll_n,int frame_in,
 struct lx_agent_result *out){
 struct lx_agent_stats *s=p->stats(p->ctx);
 struct lx_turn_policy turn;lx_turn_init(&turn,lx_agent_now());
 unsigned seen_audio=s->audio_delta,seen_done=s->done,seen_audio_done=s->audio_done,seen_transcript=s->transcript;
 unsigned seen_input=s->input_started,seen_canceled=s->canceled;
 int frames=0,sent_during_answer=0,sent_during_tool=0;const char *reason="remote_closed";int failed=0;
 int end_requested=0;int64_t end_deadline=0;
 int64_t started=lx_agent_now();
 int awaiting_reply=0;int64_t progress_at=started,music_queued_at=0;
 unsigned seen_committed=s->committed,seen_text=s->text;
 int64_t stop_at=started+lim->session_max_ms;
 short *mic=malloc(sizeof(short)*(size_t)frame_in);
 out->frames=0;out->stream_failed=0;out->turns=0;out->reason=reason;
 if(!mic)return 1;
 if(preroll_n>0&&preroll){
  int off=0;
  while(off<preroll_n){
   int take=preroll_n-off;if(take>frame_in)take=frame_in;
   if(p->send_audio(p->ctx,preroll+off,take)){reason="preroll_error";failed=1;break;}
   off+=take;
  }
  if(!failed){printf("preroll_uploaded=%d\n",off);fflush(stdout);}
 }
 while(!failed&&!s->finished){
  int64_t now=lx_agent_now();
  if(now<0){reason="clock_error";break;}
  if(now>=stop_at)break;
  long got;
  if(h->stop(h->ctx)){reason="signal";break;}
  /* Keep draining dsnoop during playback. Otherwise the next turn begins with
     a stale capture backlog containing our own answer. */
  got=h->capture(h->ctx,mic,frame_in);
  if(got<0){if(h->capture_recover(h->ctx,got)<0){reason="capture_error";failed=1;break;}continue;}
  if(got&&(lim->full_duplex||turn.uplink)&&(lim->full_duplex||!h->tool_busy(h->ctx))){
   int frame_peak=0;double frame_energy=0;
   for(int k=0;k<got;k++){int v=mic[k];frame_energy+=(double)v*v;if(v<0)v=-v;if(v>frame_peak)frame_peak=v;}
   if(frame_peak>s->peak)s->peak=frame_peak;
   /* Local capture includes quiet far-field speech (measured peak 324).
      Use short-time energy rather than an absolute peak of 1200. */
   if((!turn.turns||lim->full_duplex)&&frame_energy/(double)got>(double)lim->voice_gate){lx_turn_voice(&turn,lx_agent_now());if(awaiting_reply)progress_at=lx_agent_now();}
   if(p->send_audio(p->ctx,mic,(int)got)){reason="send_error";break;}
   if(turn.answer_active)sent_during_answer++;
   if(h->tool_busy(h->ctx))sent_during_tool++;
   frames++;
   if(frames%100==0){printf("tick frames=%d peak=%d audio_bytes=%d\n",frames,s->peak,s->imported_audio);fflush(stdout);}
  }else if(got&&!turn.uplink&&turn.answer_active&&h->barge_check&&h->barge_check(h->ctx,mic,(int)got)){
   static const char unmute[]="{\"type\":\"input_audio_unmute.commit\"}";
   if(p->send_json(p->ctx,unmute)){reason="barge_unmute_error";break;}
   h->barge(h->ctx);
   turn.uplink=1;turn.answer_active=0;turn.answer_ended=0;turn.followup_until=0;turn.last_voice=lx_agent_now();
   printf("barge_in=1\n");fflush(stdout);
  }
  if(p->pump(p->ctx,20)<0){reason="receive_error";break;}
  if(s->finished)break;
  if(s->input_started!=seen_input){
   seen_input=s->input_started;
   awaiting_reply=1;progress_at=lx_agent_now();
   if(lim->full_duplex&&turn.answer_active){
    if(p->send_json(p->ctx,"{\"type\":\"response.cancel\"}")){reason="barge_cancel_error";break;}
    if(h->barge)h->barge(h->ctx);
    turn.answer_active=turn.answer_ended=0;turn.followup_until=0;turn.uplink=1;
    lx_turn_voice(&turn,lx_agent_now());seen_audio=s->audio_delta;seen_done=s->done;seen_audio_done=s->audio_done;
    puts("barge_in=server_asr");fflush(stdout);
   }
  }
  if(lim->full_duplex&&s->canceled!=seen_canceled){seen_canceled=s->canceled;turn.answer_active=turn.answer_ended=0;turn.followup_until=0;turn.uplink=1;lx_turn_voice(&turn,lx_agent_now());}
  if(s->pending_call&&!h->tool_busy(h->ctx)){
   static const char mute[]="{\"type\":\"input_audio_mute.commit\"}";
   if(!lim->full_duplex&&p->send_json(p->ctx,mute)){reason="tool_mute_error";break;}
   const char *event=s->pending_call;s->pending_call=NULL;
   int started=h->tool_start(h->ctx,event);
   free((void *)event);
   if(started){reason="tool_thread_error";break;}
   awaiting_reply=1;progress_at=lx_agent_now();
   lx_turn_voice(&turn,lx_agent_now());h->publish(h->ctx,"thinking");
  }
  if(h->tool_busy(h->ctx)){
   char reply[32768];int done=h->tool_take(h->ctx,reply,sizeof(reply));
   if(done==1){
    progress_at=lx_agent_now();
    if(p->send_json(p->ctx,reply)){reason="tool_result_error";break;}
    static const char unmute[]="{\"type\":\"input_audio_unmute.commit\"}";
    if(!lim->full_duplex&&!turn.answer_active&&p->send_json(p->ctx,unmute)){reason="tool_unmute_error";break;}
    lx_turn_voice(&turn,lx_agent_now());
    if(h->tool_end(h->ctx)){end_requested=1;end_deadline=lx_agent_now()+5000;}
   }else if(done<0){reason="tool_result_error";break;}
  }
  /* After a reply, server ASR events decide whether the user began a new turn.
     Ambient fan/TV energy must not cancel the idle timer. */
  if(s->transcript!=seen_transcript){seen_transcript=s->transcript;lx_turn_voice(&turn,lx_agent_now());if(awaiting_reply)progress_at=lx_agent_now();}
  if(s->committed!=seen_committed){seen_committed=s->committed;progress_at=lx_agent_now();if(!s->input_started&&!turn.turns)awaiting_reply=1;}
  if(s->text!=seen_text){seen_text=s->text;progress_at=lx_agent_now();awaiting_reply=1;}
  if(s->audio_delta!=seen_audio){
   progress_at=lx_agent_now();awaiting_reply=1;
   seen_audio=s->audio_delta;
   if(lim->answer_mode&&lx_turn_audio_mode(&turn,lim->full_duplex)){
    static const char mute[]="{\"type\":\"input_audio_mute.commit\"}";
    if(p->send_json(p->ctx,mute)){reason="mute_error";break;}
    if(h->barge_reset)h->barge_reset(h->ctx);
    puts("uplink_paused_for_playback");fflush(stdout);
   }
  }
  if(s->done!=seen_done||s->audio_done!=seen_audio_done){
   seen_done=s->done;seen_audio_done=s->audio_done;
   progress_at=lx_agent_now();if(turn.answer_active)awaiting_reply=0;
   lx_turn_done(&turn,lx_agent_now());
  }
  int action=(lim->answer_mode&&!h->tool_busy(h->ctx)&&!s->pending_call&&turn.answer_active&&turn.answer_ended)?
   lx_turn_played(&turn,lx_agent_now(),h->playback_idle(h->ctx),end_requested?0:lim->followup_ms):0;
  if(action<0){reason=end_requested?"end_requested":"answer_complete";break;}
  if(action>0){
   if(h->tool_has_ticket(h->ctx)){reason="music_queued";break;}
   static const char unmute[]="{\"type\":\"input_audio_unmute.commit\"}";
   if(!lim->full_duplex&&p->send_json(p->ctx,unmute)){reason="unmute_error";break;}
   printf("stage=followup turn=%d timeout_ms=%d\n",turn.turns,lim->followup_ms);fflush(stdout);
   h->led(h->ctx,1);h->publish(h->ctx,"followup");
  }
  /* A queued real track must not wait through repeated interruptions until
     its private playback ticket expires. Allow a bounded spoken confirmation. */
  if(h->tool_has_ticket(h->ctx)){
   if(!music_queued_at)music_queued_at=lx_agent_now();
   if(!h->tool_busy(h->ctx)&&lx_agent_now()-music_queued_at>=15000){reason="music_queued";break;}
  }else music_queued_at=0;
  if(end_requested&&!turn.answer_active&&lx_agent_now()>=end_deadline){reason="end_requested";break;}
  if(lim->answer_mode&&!h->tool_busy(h->ctx)&&awaiting_reply&&lim->no_response_ms>0&&
     (!turn.answer_active||h->playback_idle(h->ctx))&&lx_agent_now()-progress_at>=lim->no_response_ms){reason="no_response";break;}
  const char *timed=lx_turn_timeout(&turn,lx_agent_now(),lim->no_speech_ms);
  if(timed&&!h->tool_busy(h->ctx)&&!awaiting_reply){reason=timed;break;}
 }
 if(lx_agent_now()>=stop_at&&!s->finished)reason="max_duration";
 if(strstr(reason,"error"))out->stream_failed=1;
 out->frames=frames;out->turns=turn.turns;out->reason=reason;
 printf("session_upload mode=%s frames=%d during_answer=%d during_tool=%d\n",lim->full_duplex?"full":"half",frames,sent_during_answer,sent_during_tool);fflush(stdout);
 free(mic);
 return out->stream_failed?1:0;
}
#endif
