/* Half-duplex turn policy. Times are monotonic milliseconds. No I/O. */
#ifndef LX06_SESSION_POLICY_H
#define LX06_SESSION_POLICY_H
#include <stdint.h>
struct lx_turn_policy {
    int uplink,answer_active,answer_ended,turns;
    int64_t last_voice,followup_until,answer_end_at;
};
static void lx_turn_init(struct lx_turn_policy *p,int64_t now) {
    *p=(struct lx_turn_policy){.uplink=1,.last_voice=now};
}
static void lx_turn_voice(struct lx_turn_policy *p,int64_t now) {
    if(p->uplink){p->last_voice=now;p->followup_until=0;}
}
/* Return 1 only when a mute event must be sent. */
static int lx_turn_audio(struct lx_turn_policy *p) {
    int mute=p->uplink;
    p->uplink=0;p->answer_active=1;p->answer_ended=0;p->followup_until=0;
    return mute;
}
static int lx_turn_audio_mode(struct lx_turn_policy *p,int full_duplex) {
    if(!full_duplex)return lx_turn_audio(p);
    p->uplink=1;p->answer_active=1;p->answer_ended=0;p->followup_until=0;return 0;
}
static void lx_turn_done(struct lx_turn_policy *p,int64_t now) {
    if(p->answer_active){p->answer_ended=1;p->answer_end_at=now;}
}
/* 1 = unmute, -1 = single-turn complete, 0 = wait. */
static int lx_turn_played(struct lx_turn_policy *p,int64_t now,int idle,int followup) {
    if(!p->answer_active||!p->answer_ended||!idle||now-p->answer_end_at<150)return 0;
    p->turns++;p->answer_active=p->answer_ended=0;
    if(!followup)return -1;
    p->uplink=1;p->last_voice=now;p->followup_until=now+followup;return 1;
}
static const char *lx_turn_timeout(const struct lx_turn_policy *p,int64_t now,int no_speech) {
    if(p->followup_until&&now>=p->followup_until)return "followup_timeout";
    if(p->uplink&&!p->answer_active&&!p->followup_until&&now-p->last_voice>no_speech)return "no_speech";
    return 0;
}
#endif
