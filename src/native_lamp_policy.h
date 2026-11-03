#ifndef LX06_LAMP_POLICY_H
#define LX06_LAMP_POLICY_H
#include <unistd.h>
#include <errno.h>
#ifndef LX_LAMP_POLICY
#define LX_LAMP_POLICY "/data/lx06-agent/lamp-feedback-off"
#endif
/* Unknown filesystem state disables Sidekick feedback; factory LEDs remain
   owned by their factory service. Only Sidekick's remembered layer is shut. */
static int lx_lamp_feedback_enabled(void){return access(LX_LAMP_POLICY,F_OK)<0&&errno==ENOENT;}
#endif
