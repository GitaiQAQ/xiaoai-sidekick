/* Read-only, firmware-specific control-field probe. No ptrace, audio reads,
   model replacement, remote calls or process writes. LX06 1.94.13 only. */
#define _GNU_SOURCE
#define _FILE_OFFSET_BITS 64
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

struct mapping {uint32_t start,end;};
static struct mapping maps[256];
static int count,fd;
static unsigned bytes_read;
static int readable(uint32_t p,unsigned n) {
 if(!p||p%4||n>4||p>UINT32_MAX-n)return 0;
 for(int i=0;i<count;i++)if(p>=maps[i].start&&p+n<=maps[i].end)return 1;
 return 0;
}
static uint32_t word(uint32_t p) {
 uint32_t v;
 if(!readable(p,4)||pread(fd,&v,4,(off_t)p)!=4){fputs("probe_read_failed\n",stderr);exit(2);}
 bytes_read+=4;return v;
}
static float real(uint32_t p){uint32_t u=word(p);float v;memcpy(&v,&u,4);return v;}
int main(int argc,char **argv) {
 char path[96],exe[160],line[512],perms[8];uint32_t base=0;
 if(argc!=2)return 2;
 char *end;long pid=strtol(argv[1],&end,10);if(*end||pid<2||pid>4194304)return 2;
 snprintf(path,sizeof(path),"/proc/%ld/exe",pid);
 ssize_t n=readlink(path,exe,sizeof(exe)-1);if(n<0)return 2;exe[n]=0;
 if(strcmp(exe,"/usr/bin/mipns-xiaomi")){fputs("probe_wrong_target\n",stderr);return 2;}
 snprintf(path,sizeof(path),"/proc/%ld/maps",pid);FILE *f=fopen(path,"r");if(!f)return 2;
 while(fgets(line,sizeof(line),f)) {
  unsigned a,b,offset;
  if(sscanf(line,"%x-%x %7s %x",&a,&b,perms,&offset)!=4)continue;
  if(perms[0]=='r'){
   if(count==256){fclose(f);return 2;}
   maps[count++]=(struct mapping){a,b};
  }
  if(strstr(line,"/usr/lib/libxaudio_engine.so")&&offset==0)base=a;
 }
 fclose(f);if(!base){fputs("probe_library_not_mapped\n",stderr);return 2;}
 snprintf(path,sizeof(path),"/proc/%ld/mem",pid);fd=open(path,O_RDONLY);if(fd<0){perror("probe_mem");return 2;}
 /* Fixed chain verified from g_handle, create_xaudio, xaudio_create_wakeup,
    xaudio_wakeup_init, wrapper init and PaddyEngine::Init disassembly. */
 uint32_t engine=word(base+0x4254d4);
 uint32_t wake=word(engine+0x10),wrapper=word(wake+0x40);
 uint32_t paddy=word(wrapper+4),impl=word(paddy);
 uint32_t state[2]={word(impl),word(impl+4)};
 if(word(state[0]+0x3b8)!=1||word(state[1]+0x3b8)!=2){
  fputs("probe_layout_mismatch\n",stderr);close(fd);return 2;
 }
 puts("probe=read_only fields=control_scalars audio=not_read");
 for(int i=0;i<2;i++) {
  uint32_t s=state[i];
  printf("level=%d chunk=%u min_self_loop=%u max_self_loop=%u "
         "acoustic_scale=%.6f min_trans_loglikelihood=%.6f score_threshold=%.6f "
         "wait_frames=%u frames_forward=%u keyword_seq_items=%u\n",
    i+1,word(s+0x39c),word(s+0x73c),word(s+0x740),
    (double)real(s+0x3bc),(double)real(s+0x744),(double)real(s+0x3e8),
    word(s+0x364),word(s+0x300),word(s+0x738));
 }
 close(fd);printf("probe_bytes_read=%u\n",bytes_read);return 0;
}
