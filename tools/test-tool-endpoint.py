"""Execute the actual Web tool_reply with JSON and dispatch mocks on macOS."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[1]
s=(root/'src/native_web_console.c').read_text();a=s.index('static void tool_reply(int fd');b=s.index('{',a);depth=1;c=b+1
while depth:
    if s[c]=='{':depth+=1
    elif s[c]=='}':depth-=1
    c+=1
body=s[a:c]
preamble=r'''
#include "test-native-json.m.h"
static int lx_music_state(char *a,size_t b,int *c){(void)a;(void)b;(void)c;return 1;}
static int lx_music_ubus_object(const char *a,const char *b,const char *c,char *d,size_t e){(void)a;(void)b;(void)c;(void)d;(void)e;assert(0);return 1;}
#define LX_LAMP_POLICY "/nonexistent/lx06-local-test-only"
#include "native_capabilities.h"
static id reply;
static int executed;
static void json_reply(int fd,int code,void *o){(void)fd;(void)code;[reply release];reply=[node(o) retain];jput(o);}
static void error_reply(int fd,int code,const char *e){json_reply(fd,code,cap_error(e));}
static void *action_execute(void *o,const char **action,int *code,const char **err){(void)action;(void)code;(void)err;executed++;void *r=jobject();add_i(r,"ok",1);add_s(r,"action",cap_string(o,"action"));return r;}
'''
test=r'''
static unsigned checks;
static void check(const char *input,int expected,const char *error){executed=0;tool_reply(0,input);assert(executed==expected);checks++;if(error){assert([reply[@"error"] isEqual:@(error)]);checks++;}else{assert([reply[@"ok"] intValue]==1);checks++;}}
int main(void){@autoreleasepool{
check("{\"name\":\"music_stop\",\"arguments\":\"{}\"}",1,NULL);
check("{\"name\":\"music_stop\"}",1,NULL);
check("{\"name\":\"music_stop\",\"arguments\":\"broken JSON\"}",0,"invalid_tool_arguments");
check("{\"name\":\"music_stop\",\"arguments\":true}",0,"invalid_tool_arguments");
check("{\"name\":\"music_stop\",\"arguments\":null}",0,"invalid_tool_arguments");
check("{\"name\":\"music_stop\",\"arguments\":[]}",0,"invalid_tool_arguments");
check("{\"name\":\"music_stop\\u0000unexpected\",\"arguments\":\"{}\"}",0,"invalid_tool_request");
check("{\"name\":\"shell\",\"arguments\":\"{}\"}",0,"unknown_tool");
check("{\"name\":\"speaker_volume_set\",\"arguments\":{\"value\":40,\"unknown\":1}}",0,"invalid_tool_arguments");
check("{\"name\":\"speaker_volume_set\",\"arguments\":{\"value\":40}}",1,NULL);
check("{\"name\":\"music_status\",\"arguments\":{\"action\":\"reboot\"}}",0,"invalid_tool_arguments");
[reply release];printf("tool_endpoint_mock=pass checks=%u malformed_arguments_no_execution=pass embedded_nul_no_execution=pass unknown_tool_no_execution=pass network_connections=0\n",checks);
}return 0;}
'''
with tempfile.TemporaryDirectory(prefix='lx06-tool-endpoint-') as temp:
    source=Path(temp)/'test.m';exe=Path(temp)/'test';source.write_text(preamble+body+test)
    subprocess.run(['clang','-D_DEFAULT_SOURCE','-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-function','-framework','Foundation','-framework','CoreFoundation','-I',str(root/'src'),'-I',str(root/'tools'),str(source),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
