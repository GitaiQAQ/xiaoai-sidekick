#include <stdio.h>
#include <string.h>
#include "native_session_policy.h"
#define CHECK(x) do {n++;if(!(x)){printf("failed line=%d\n",__LINE__);return 1;}}while(0)
int main(void) {
 int n=0;struct lx_turn_policy p;const char *why;
 lx_turn_init(&p,0);CHECK(p.uplink);CHECK(!lx_turn_timeout(&p,5999,6000));
 why=lx_turn_timeout(&p,6001,6000);CHECK(why&&!strcmp(why,"no_speech"));
 lx_turn_voice(&p,5000);CHECK(!lx_turn_timeout(&p,9000,6000));
 CHECK(lx_turn_audio(&p)==1);CHECK(!p.uplink);CHECK(lx_turn_audio(&p)==0);
 lx_turn_voice(&p,9999);CHECK(p.last_voice==5000);
 lx_turn_done(&p,10000);CHECK(!lx_turn_played(&p,11000,0,8000));
 CHECK(!lx_turn_timeout(&p,20000,6000)); /* long playback cannot end listening */
 CHECK(lx_turn_played(&p,21000,1,8000)==1);CHECK(p.uplink&&p.turns==1);
 CHECK(!lx_turn_timeout(&p,28000,6000)); /* followup has its own 8-second timer */
 lx_turn_done(&p,28000);CHECK(!lx_turn_played(&p,28000,1,8000)); /* duplicate done */
 lx_turn_voice(&p,28500);CHECK(!p.followup_until);
 CHECK(lx_turn_audio(&p)==1);lx_turn_done(&p,30000);
 CHECK(!lx_turn_played(&p,30100,1,8000));CHECK(lx_turn_played(&p,30200,1,8000)==1);
 CHECK(p.turns==2);why=lx_turn_timeout(&p,38200,6000);
 CHECK(why&&!strcmp(why,"followup_timeout"));
 lx_turn_audio(&p);lx_turn_done(&p,40000);CHECK(lx_turn_played(&p,41000,1,0)==-1);
 lx_turn_init(&p,0);CHECK(lx_turn_audio_mode(&p,1)==0);CHECK(p.uplink&&p.answer_active);
 CHECK(!lx_turn_timeout(&p,10000,6000));lx_turn_done(&p,10000);CHECK(lx_turn_played(&p,10200,1,8000)==1);CHECK(p.uplink&&p.turns==1);
 printf("session_policy checks=%d pass=1\n",n);return 0;
}
