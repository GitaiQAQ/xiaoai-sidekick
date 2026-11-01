/* Single source of truth for everything the model or the console may call.
   Voice clients fetch this catalog over the local control socket and forward
   calls back to it, so a new capability is one row plus one action handler. */
#ifndef LX06_NATIVE_TOOLS_H
#define LX06_NATIVE_TOOLS_H
#include <stddef.h>
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
 {"music_search","Search Netease and return real track IDs.","{\"type\":\"object\",\"properties\":{\"query\":{\"type\":\"string\"}},\"required\":[\"query\"]}","netease_search",LX_TOOL_VOICE},
 {"music_charts","List real Netease chart names and IDs.","{\"type\":\"object\",\"properties\":{}}","netease_charts",LX_TOOL_VOICE},
 {"music_chart_tracks","Get the first 20 songs in a chart ID from music_charts.","{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"string\"}},\"required\":[\"id\"]}","netease_chart_tracks",LX_TOOL_VOICE},
 {"music_playlists","List playlists for the authenticated Netease account.","{\"type\":\"object\",\"properties\":{}}","netease_playlists",LX_TOOL_VOICE},
 {"music_playlist_tracks","Get the first 20 songs from a playlist ID returned by music_playlists.","{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"string\"}},\"required\":[\"id\"]}","netease_chart_tracks",LX_TOOL_VOICE},
 {"music_play","Play a real song ID. Pass returned title and artist when available. Playback starts after the spoken confirmation.","{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"string\"},\"title\":{\"type\":\"string\"},\"artist\":{\"type\":\"string\"},\"quality\":{\"type\":\"string\",\"enum\":[\"standard\",\"exhigh\",\"lossless\"]}},\"required\":[\"id\"]}","netease_prepare",LX_TOOL_VOICE},
 {"music_play_chart","Queue the first 20 tracks from a chart ID returned by music_charts.","{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"string\"},\"quality\":{\"type\":\"string\",\"enum\":[\"standard\",\"exhigh\",\"lossless\"]}},\"required\":[\"id\"]}","netease_chart_prepare",LX_TOOL_VOICE},
 {"music_play_playlist","Queue the first 20 songs from a playlist ID returned by music_playlists.","{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"string\"},\"quality\":{\"type\":\"string\",\"enum\":[\"standard\",\"exhigh\",\"lossless\"]}},\"required\":[\"id\"]}","netease_playlist_prepare",LX_TOOL_VOICE},
 {"music_next","Play the next track in the current factory queue.","{\"type\":\"object\",\"properties\":{}}","music_next",LX_TOOL_VOICE},
 {"music_previous","Play the previous track in the current factory queue.","{\"type\":\"object\",\"properties\":{}}","music_previous",LX_TOOL_VOICE},
 {"music_pause","Pause speaker playback.","{\"type\":\"object\",\"properties\":{}}","music_pause",LX_TOOL_VOICE},
 {"music_stop","Stop speaker playback.","{\"type\":\"object\",\"properties\":{}}","music_stop",LX_TOOL_VOICE},
 {"session_end","End the current voice session when the user is finished or asks to stop. Playback keeps running.","{\"type\":\"object\",\"properties\":{}}","session_end",LX_TOOL_VOICE}
};

static const struct lx_tool_def *lx_tool_find(const char *name,unsigned scope){
 if(!name||!*name)return NULL;
 for(unsigned i=0;i<sizeof(lx_tool_defs)/sizeof(lx_tool_defs[0]);i++)
  if(!strcmp(name,lx_tool_defs[i].name)&&(lx_tool_defs[i].scope&scope))return &lx_tool_defs[i];
 return NULL;
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
