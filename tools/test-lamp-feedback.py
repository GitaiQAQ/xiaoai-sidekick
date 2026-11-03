"""Run the exact production feedback_led function with fake LED callbacks."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[1]
s=(root/'src/native_duplex_client.c').read_text()
a=s.index('static void feedback_led(int state)');b=s.index('{',a);depth=1;c=b+1
while depth:
    if s[c]=='{': depth+=1
    elif s[c]=='}': depth-=1
    c+=1
function=s[a:c]
preamble=r'''
#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>
static char lamp_path[200];
#define LX_LAMP_POLICY lamp_path
#include "native_lamp_policy.h"
static int g_led_owned,g_led_requested,calls,layers[16];
static int led_command(int effect,int on){assert(effect>=1&&effect<=3);layers[effect]=on;calls++;return 0;}
static void publish_state(const char *s){(void)s;}
'''
test=r'''
int main(void){char dir[]="/tmp/lx06-lamp-test.XXXXXX";assert(mkdtemp(dir));snprintf(lamp_path,sizeof(lamp_path),"%s/off",dir);layers[9]=1;
feedback_led(1);assert(g_led_owned==1&&g_led_requested==1&&layers[1]&&layers[9]);int before=calls;feedback_led(1);assert(calls==before);
int fd=open(lamp_path,O_CREAT|O_WRONLY,0600);assert(fd>=0);close(fd);feedback_led(3);assert(g_led_owned==0&&g_led_requested==3&&!layers[1]&&!layers[3]&&layers[9]);
unlink(lamp_path);feedback_led(g_led_requested);assert(g_led_owned==3&&layers[3]&&layers[9]);feedback_led(0);assert(!g_led_owned&&!g_led_requested&&!layers[3]&&layers[9]);
rmdir(dir);puts("lamp_feedback_mock=pass owned_layer_only=pass disabled_updates_requested=pass enable_restores_requested=pass factory_layer_preserved=pass device_operations=0");return 0;}
'''
with tempfile.TemporaryDirectory(prefix='lx06-lamp-build-') as temp:
    source=Path(temp)/'test.c';binary=Path(temp)/'test';source.write_text(preamble+function+test)
    subprocess.run(['clang','-std=c11','-Wall','-Wextra','-Werror','-I',str(root/'src'),str(source),'-o',str(binary)],check=True)
    subprocess.run([str(binary)],check=True)
