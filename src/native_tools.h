/* Single source of truth for everything the model or the console may call.
   Voice clients fetch this catalog over the local control socket and forward
   calls back to it, so a new capability is one row plus one action handler. */
#ifndef LX06_NATIVE_TOOLS_H
#define LX06_NATIVE_TOOLS_H
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#define LX_TOOL_VOICE 1u
#define LX_TOOL_WEB   2u

struct lx_tool_def {
 const char *name;
 const char *description;
 const char *params;   /* JSON schema object */
 const char *action;   /* action name executed by action_execute() */
 unsigned scope;
};

static const struct lx_tool_def lx_tool_defs[]={
 {"music_search","Search Netease songs by title, artist or lyrics; returns track IDs.","{\"type\":\"object\",\"properties\":{\"query\":{\"type\":\"string\",\"minLength\":1,\"maxLength\":80}},\"required\":[\"query\"],\"additionalProperties\":false}","netease_search",LX_TOOL_VOICE},
 {"music_charts","List real Netease chart names and IDs.","{\"type\":\"object\",\"properties\":{},\"additionalProperties\":false}","netease_charts",LX_TOOL_VOICE},
 {"music_chart_tracks","Get a page of songs from a chart ID.","{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"string\",\"minLength\":1,\"maxLength\":19,\"pattern\":\"^[0-9]+$\"},\"offset\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":1000000},\"limit\":{\"type\":\"integer\",\"minimum\":1,\"maximum\":20}},\"required\":[\"id\"],\"additionalProperties\":false}","netease_chart_tracks",LX_TOOL_VOICE},
 {"music_playlists","List playlists for the authenticated Netease account.","{\"type\":\"object\",\"properties\":{},\"additionalProperties\":false}","netease_playlists",LX_TOOL_VOICE},
 {"music_playlist_tracks","Get a page of songs from a playlist ID.","{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"string\",\"minLength\":1,\"maxLength\":19,\"pattern\":\"^[0-9]+$\"},\"offset\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":1000000},\"limit\":{\"type\":\"integer\",\"minimum\":1,\"maximum\":20}},\"required\":[\"id\"],\"additionalProperties\":false}","netease_chart_tracks",LX_TOOL_VOICE},
 {"music_play","Play a real song ID. Pass returned title and artist when available. Playback starts after the spoken confirmation.","{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"string\",\"minLength\":1,\"maxLength\":19,\"pattern\":\"^[0-9]+$\"},\"title\":{\"type\":\"string\",\"maxLength\":255},\"artist\":{\"type\":\"string\",\"maxLength\":255},\"quality\":{\"type\":\"string\",\"enum\":[\"standard\",\"exhigh\",\"lossless\"]}},\"required\":[\"id\"],\"additionalProperties\":false}","netease_prepare",LX_TOOL_VOICE},
 {"music_play_chart","Queue songs from a chart ID.","{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"string\",\"minLength\":1,\"maxLength\":19,\"pattern\":\"^[0-9]+$\"},\"quality\":{\"type\":\"string\",\"enum\":[\"standard\",\"exhigh\",\"lossless\"]}},\"required\":[\"id\"],\"additionalProperties\":false}","netease_chart_prepare",LX_TOOL_VOICE},
 {"music_play_playlist","Queue songs from a playlist ID.","{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"string\",\"minLength\":1,\"maxLength\":19,\"pattern\":\"^[0-9]+$\"},\"quality\":{\"type\":\"string\",\"enum\":[\"standard\",\"exhigh\",\"lossless\"]}},\"required\":[\"id\"],\"additionalProperties\":false}","netease_playlist_prepare",LX_TOOL_VOICE},
 {"music_next","Play the next track in the current factory queue.","{\"type\":\"object\",\"properties\":{},\"additionalProperties\":false}","music_next",LX_TOOL_VOICE},
 {"music_previous","Play the previous track in the current factory queue.","{\"type\":\"object\",\"properties\":{},\"additionalProperties\":false}","music_previous",LX_TOOL_VOICE},
 {"music_pause","Pause speaker playback.","{\"type\":\"object\",\"properties\":{},\"additionalProperties\":false}","music_pause",LX_TOOL_VOICE},
 {"music_stop","Stop speaker playback.","{\"type\":\"object\",\"properties\":{},\"additionalProperties\":false}","music_stop",LX_TOOL_VOICE},
 {"speaker_status","Get speaker time, timezone, uptime and available memory without network identifiers.","{\"type\":\"object\",\"properties\":{},\"additionalProperties\":false}","speaker_status",LX_TOOL_VOICE},
 {"speaker_volume_get","Get the current speaker volume.","{\"type\":\"object\",\"properties\":{},\"additionalProperties\":false}","volume_get",LX_TOOL_VOICE},
 {"speaker_volume_set","Set speaker volume from 0 to 100 and verify the result.","{\"type\":\"object\",\"properties\":{\"value\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":100}},\"additionalProperties\":false,\"required\":[\"value\"]}","volume",LX_TOOL_VOICE},
 {"music_status","Get playback state, track, position, volume and queue status.","{\"type\":\"object\",\"properties\":{},\"additionalProperties\":false}","music_status",LX_TOOL_VOICE},
 {"music_resume","Resume the existing factory playback queue.","{\"type\":\"object\",\"properties\":{},\"additionalProperties\":false}","music_resume",LX_TOOL_VOICE},
 {"music_seek","Seek within the current track and verify its position.","{\"type\":\"object\",\"properties\":{\"position_ms\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":2147483647}},\"additionalProperties\":false,\"required\":[\"position_ms\"]}","music_seek",LX_TOOL_VOICE},
 {"music_mode_get","Get the current playback mode.","{\"type\":\"object\",\"properties\":{},\"additionalProperties\":false}","music_mode_get",LX_TOOL_VOICE},
 {"music_mode_set","Select repeating the queue or playing it once in order.","{\"type\":\"object\",\"properties\":{\"mode\":{\"type\":\"string\",\"enum\":[\"repeat_all\",\"sequential\"]}},\"additionalProperties\":false,\"required\":[\"mode\"]}","music_mode_set",LX_TOOL_VOICE},
 {"sleep_timer_get","Get the factory playback shutdown timer state.","{\"type\":\"object\",\"properties\":{},\"additionalProperties\":false}","sleep_timer_get",LX_TOOL_VOICE},
 {"sound_effect_get","Get the factory sound effect mode and effect numbers.","{\"type\":\"object\",\"properties\":{},\"additionalProperties\":false}","sound_effect_get",LX_TOOL_VOICE},
 {"alarm_volume_get","Get the factory alarm volume.","{\"type\":\"object\",\"properties\":{},\"additionalProperties\":false}","alarm_volume_get",LX_TOOL_VOICE},
 {"alarm_volume_set","Set alarm volume without preview playback and verify it.","{\"type\":\"object\",\"properties\":{\"value\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":100}},\"additionalProperties\":false,\"required\":[\"value\"]}","alarm_volume_set",LX_TOOL_VOICE},
 {"alarm_list","List alarm IDs, times, repeat masks, enabled states and volumes.","{\"type\":\"object\",\"properties\":{\"offset\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":1000000},\"limit\":{\"type\":\"integer\",\"minimum\":1,\"maximum\":20}},\"additionalProperties\":false}","alarm_list",LX_TOOL_VOICE},
 {"alarm_create","Create a one-time alarm. Check speaker_status time; use local date/time or minutes from now.","{\"type\":\"object\",\"properties\":{\"local_date\":{\"type\":\"string\",\"pattern\":\"^[0-9]{4}-[0-9]{2}-[0-9]{2}$\"},\"local_time\":{\"type\":\"string\",\"pattern\":\"^[0-9]{2}:[0-9]{2}$\"},\"after_minutes\":{\"type\":\"integer\",\"minimum\":1,\"maximum\":525600},\"timestamp\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":2147483647},\"volume\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":100}},\"additionalProperties\":false,\"oneOf\":[{\"required\":[\"timestamp\"],\"not\":{\"anyOf\":[{\"required\":[\"after_minutes\"]},{\"required\":[\"local_date\"]},{\"required\":[\"local_time\"]}]}},{\"required\":[\"after_minutes\"],\"not\":{\"anyOf\":[{\"required\":[\"timestamp\"]},{\"required\":[\"local_date\"]},{\"required\":[\"local_time\"]}]}},{\"required\":[\"local_date\",\"local_time\"],\"not\":{\"anyOf\":[{\"required\":[\"timestamp\"]},{\"required\":[\"after_minutes\"]}]}}]}","alarm_create",LX_TOOL_VOICE},
 {"alarm_enabled_set","Enable or disable an alarm ID returned by alarm_list.","{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"string\",\"minLength\":1,\"maxLength\":64,\"pattern\":\"^[A-Za-z0-9_-]+$\"},\"enabled\":{\"type\":\"boolean\"}},\"additionalProperties\":false,\"required\":[\"id\",\"enabled\"]}","alarm_enabled_set",LX_TOOL_VOICE},
 {"alarm_delete","Delete only the specified existing alarm ID returned by alarm_list.","{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"string\",\"minLength\":1,\"maxLength\":64,\"pattern\":\"^[A-Za-z0-9_-]+$\"}},\"additionalProperties\":false,\"required\":[\"id\"]}","alarm_delete",LX_TOOL_VOICE},
 {"lamp_status","Get actual LED effects and whether Sidekick feedback lights are enabled.","{\"type\":\"object\",\"properties\":{},\"additionalProperties\":false}","lamp_status",LX_TOOL_VOICE},
 {"lamp_feedback_set","Enable or disable Sidekick feedback lights without clearing factory lighting.","{\"type\":\"object\",\"properties\":{\"enabled\":{\"type\":\"boolean\"}},\"additionalProperties\":false,\"required\":[\"enabled\"]}","lamp_feedback_set",LX_TOOL_VOICE},
 {"bluetooth_status","Get Bluetooth enabled, playing, discovery and paired/connected counts without addresses.","{\"type\":\"object\",\"properties\":{},\"additionalProperties\":false}","bluetooth_status",LX_TOOL_VOICE},
 {"mesh_status","Get Mesh gateway support, enabled and running states; this does not control home devices.","{\"type\":\"object\",\"properties\":{},\"additionalProperties\":false}","mesh_status",LX_TOOL_VOICE},
 {"night_mode_status","Get the factory night mode configuration and schedule.","{\"type\":\"object\",\"properties\":{},\"additionalProperties\":false}","night_mode_status",LX_TOOL_VOICE},
 {"line_in_status","Get wired line-in connection and playback status.","{\"type\":\"object\",\"properties\":{},\"additionalProperties\":false}","line_in_status",LX_TOOL_VOICE},
 {"session_end","End the current voice session when the user is finished or asks to stop. Playback keeps running.","{\"type\":\"object\",\"properties\":{},\"additionalProperties\":false}","session_end",LX_TOOL_VOICE},
};

static const struct lx_tool_def *lx_tool_find(const char *name,unsigned scope){
 if(!name||!*name)return NULL;
 for(unsigned i=0;i<sizeof(lx_tool_defs)/sizeof(lx_tool_defs[0]);i++)
  if(!strcmp(name,lx_tool_defs[i].name)&&(lx_tool_defs[i].scope&scope))return &lx_tool_defs[i];
 return NULL;
}

static int lx_tool_action_allowed(const char *action,unsigned scope){
 if(!action)return 0;for(unsigned i=0;i<sizeof(lx_tool_defs)/sizeof(lx_tool_defs[0]);i++)if(!strcmp(action,lx_tool_defs[i].action)&&(lx_tool_defs[i].scope&scope))return 1;return 0;
}

/* Model-facing catalog: [{"type":"function","name":...,"description":...,
   "parameters":{...}}, ...]. Returns the length or -1 if it does not fit. */
static int lx_tools_json(unsigned scope,char *out,size_t cap){
 size_t used=0;int n=snprintf(out,cap,"[");
 if(n<0||(size_t)n>=cap)return -1;
 used=(size_t)n;
 unsigned emitted=0;
 for(unsigned i=0;i<sizeof(lx_tool_defs)/sizeof(lx_tool_defs[0]);i++){
  const struct lx_tool_def *t=&lx_tool_defs[i];
  if(!(t->scope&scope))continue;
  n=snprintf(out+used,cap-used,"%s{\"type\":\"function\",\"name\":\"%s\",\"description\":\"%s\",\"parameters\":%s}",
   emitted?",":"",t->name,t->description,t->params);
  if(n<0||(size_t)n>=cap-used)return -1;
  used+=(size_t)n;emitted++;
 }
 n=snprintf(out+used,cap-used,"]");
 if(n<0||(size_t)n>=cap-used)return -1;
 return (int)(used+(size_t)n);
}
#endif
