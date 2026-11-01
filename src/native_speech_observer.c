#define _GNU_SOURCE
#include "native_protocol.h"
#include <sys/ptrace.h>
#include <sys/uio.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <elf.h>
#include <errno.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* AArch32 on this AArch64 4.9 kernel: r12=0 enter, r12=1 exit.
 * Verified in upstream arch/arm64/kernel/ptrace.c, tracehook_report_syscall.
 * Never sets registers, memory, syscall results, or PTRACE_O_EXITKILL.
 */
enum { ARM_RECVFROM=292 };
static volatile sig_atomic_t stop;
static void stop_signal(int sig) { (void)sig; stop=1; }

struct remote_input { pid_t tid; uint32_t address; size_t size,reads; };
struct observation { unsigned packets,errors,stream_packets; size_t metadata_reads,audio_length,min_stream,max_stream; };

static int remote_byte(void *context,size_t offset,uint8_t *byte) {
    struct remote_input *r=context;
    struct iovec local={byte,1},remote;
    if(offset>=r->size || offset>UINT32_MAX-r->address) return -1;
    remote.iov_base=(void*)(uintptr_t)(r->address+(uint32_t)offset); remote.iov_len=1;
    if(process_vm_readv(r->tid,&local,1,&remote,1,0)!=1) return -1;
    ++r->reads;
    return 0;
}

static int registers(pid_t tid,uint32_t regs[18]) {
    struct iovec v={regs,18*sizeof(uint32_t)};
    return ptrace(PTRACE_GETREGSET,tid,(void*)(uintptr_t)NT_PRSTATUS,&v) ||
        v.iov_len!=18*sizeof(uint32_t) ? -1 : 0;
}

static void inspect(pid_t tid,const uint32_t regs[18],int fd,struct observation *o) {
    int32_t size=(int32_t)regs[0];
    struct lx06_packet_metadata m;
    struct remote_input input;
    if(regs[7]!=ARM_RECVFROM || regs[12]!=1 || regs[17]!=(uint32_t)fd ||
       size<=0 || (uint32_t)size>regs[2] || size>65536) return;
    input=(struct remote_input){tid,regs[1],(size_t)size,0};
    if(lx06_packet_metadata_reader(remote_byte,&input,(size_t)size,&m)) {
        ++o->errors;
        printf("packet parsed=0 bytes=%d metadata_reads=%zu\n",size,input.reads);
    } else {
        ++o->packets; o->audio_length+=m.audio_bytes;
        if(m.has_stream) {
            ++o->stream_packets;
            if(o->stream_packets==1 || m.audio_bytes<o->min_stream) o->min_stream=m.audio_bytes;
            if(m.audio_bytes>o->max_stream) o->max_stream=m.audio_bytes;
        }
        /* Log control events and first audio packet, aggregate following audio. */
        if(!m.has_stream || o->stream_packets==1)
            printf("packet parsed=1 direction=%u type=%u name=%s bytes=%d stream_type=%u audio_bytes=%zu\n",
                m.direction,m.type,lx06_packet_type_name(m.direction,m.type),size,m.stream_type,m.audio_bytes);
    }
    o->metadata_reads+=input.reads;
}

static int observe(pid_t tid,int fd,unsigned seconds,int gate,struct observation *o) {
    struct sigaction action;
    uint32_t regs[18];
    int status,attached=0,stopped=0,result=0,next_signal=0;
    pid_t w;
    memset(&action,0,sizeof(action)); action.sa_handler=stop_signal;
    sigemptyset(&action.sa_mask);
    sigaction(SIGTERM,&action,NULL); sigaction(SIGINT,&action,NULL);
    sigaction(SIGHUP,&action,NULL); sigaction(SIGALRM,&action,NULL);
    stop=0;
    if(ptrace(PTRACE_SEIZE,tid,0,(void*)(uintptr_t)PTRACE_O_TRACESYSGOOD)) { perror("seize"); return 1; }
    attached=1;
    alarm(seconds);
    if(ptrace(PTRACE_INTERRUPT,tid,0,0)) { perror("interrupt"); result=1; goto done; }
    w=waitpid(tid,&status,__WALL);
    if(w!=tid || !WIFSTOPPED(status)) { result=1; goto done; }
    stopped=1;
    if(registers(tid,regs)) { perror("ARM32 register set"); result=1; goto done; }
    printf("observer_attached tid=%d fd=%d seconds=%u regset_bytes=72\n",tid,fd,seconds);
    fflush(stdout);
    if(gate>=0) { if(write(gate,"G",1)!=1) { result=1; goto done; } close(gate); gate=-1; }
    while(!stop) {
        if(ptrace(PTRACE_SYSCALL,tid,0,(void*)(uintptr_t)next_signal)) {
            if(errno!=ESRCH) result=1;
            break;
        }
        stopped=0; next_signal=0;
        do { w=waitpid(tid,&status,__WALL); } while(w<0 && errno==EINTR && !stop);
        if(w<0) { if(!stop && errno!=ECHILD) result=1; break; }
        if(WIFEXITED(status) || WIFSIGNALED(status)) {
            attached=0;
            if(!WIFEXITED(status) || WEXITSTATUS(status)!=0) result=1;
            break;
        }
        if(!WIFSTOPPED(status)) continue;
        stopped=1;
        if(WSTOPSIG(status)==(SIGTRAP|0x80)) {
            if(registers(tid,regs)) { result=1; break; }
            inspect(tid,regs,fd,o);
        } else if((unsigned)status>>16==PTRACE_EVENT_STOP) {
            if(WSTOPSIG(status)!=SIGTRAP) { next_signal=WSTOPSIG(status); break; }
        } else next_signal=WSTOPSIG(status);
    }
done:
    if(gate>=0) close(gate);
    alarm(2);
    if(attached) {
        if(!stopped && !ptrace(PTRACE_INTERRUPT,tid,0,0)) {
            do { w=waitpid(tid,&status,__WALL); } while(w<0 && errno==EINTR && !stop);
            if(w==tid && WIFSTOPPED(status)) stopped=1;
        }
        if(stopped && ptrace(PTRACE_DETACH,tid,0,(void*)(uintptr_t)next_signal)) {
            perror("detach"); result=1;
        }
    }
    alarm(0);
    printf("observer_finished parsed_packets=%u parse_errors=%u metadata_bytes_read=%zu audio_length_total=%zu stream_packets=%u min_stream=%zu max_stream=%zu\n",
        o->packets,o->errors,o->metadata_reads,o->audio_length,o->stream_packets,o->min_stream,o->max_stream);
    return result;
}

static int selftest(void) {
    static const uint8_t packet[]={8,0,18,11,8,3,34,7,8,0,18,3,0x55,0x66,0x77};
    int s[2],g[2],result,status;
    pid_t child;
    struct observation o={0};
    if(socketpair(AF_UNIX,SOCK_DGRAM,0,s) || pipe(g)) return 1;
    child=fork();
    if(child<0) return 1;
    if(!child) {
        char gate;
        uint8_t bytes[64];
        close(s[0]); close(g[1]);
        if(read(g[0],&gate,1)!=1) _exit(2);
        close(g[0]);
        if(recvfrom(s[1],bytes,sizeof(bytes),0,NULL,NULL)!=(ssize_t)sizeof(packet) ||
           memcmp(bytes,packet,sizeof(packet))) _exit(3);
        close(s[1]); _exit(0);
    }
    close(s[1]); close(g[0]);
    if(send(s[0],packet,sizeof(packet),0)!=(ssize_t)sizeof(packet)) return 1;
    result=observe(child,s[1],4,g[1],&o);
    close(s[0]);
    /* observe normally reaps the child; ensure a failed test leaves no child. */
    if(waitpid(child,&status,WNOHANG)==0) { kill(child,SIGTERM); waitpid(child,&status,0); }
    if(result || o.packets!=1 || o.errors || o.audio_length!=3 || o.metadata_reads!=12) {
        puts("observer_selftest=fail"); return 1;
    }
    puts("observer_selftest=pass ARM32_syscall_phase=verified payload_unchanged=1");
    return 0;
}

static int resume_test(int kill_observer) {
    static const uint8_t packet[]={8,0,18,2,8,2};
    int s[2],ready[2],status,result=1;
    pid_t target,tracer=-1,w;
    struct observation o={0};
    struct sigaction default_action;
    if(socketpair(AF_UNIX,SOCK_DGRAM,0,s) || pipe(ready)) return 1;
    target=fork();
    if(target<0) return 1;
    if(!target) {
        uint8_t b[32];
        memset(&default_action,0,sizeof(default_action)); default_action.sa_handler=SIG_DFL;
        sigaction(SIGTERM,&default_action,NULL);
        close(s[0]); close(ready[0]); close(ready[1]);
        if(recvfrom(s[1],b,sizeof(b),0,NULL,NULL)!=(ssize_t)sizeof(packet) ||
           memcmp(b,packet,sizeof(packet))) _exit(3);
        close(s[1]); _exit(0);
    }
    close(s[1]);
    if(kill_observer) {
        char r;
        tracer=fork();
        if(tracer<0) goto done;
        if(!tracer) {
            close(ready[0]); close(s[0]);
            _exit(observe(target,s[1],4,ready[1],&o));
        }
        close(ready[1]);
        alarm(3);
        if(read(ready[0],&r,1)!=1) goto done;
        alarm(0);
        if(kill(tracer,SIGKILL)) goto done;
        if(waitpid(tracer,&status,0)!=tracer || !WIFSIGNALED(status)) goto done;
        tracer=-1;
    } else {
        close(ready[0]); close(ready[1]);
        if(observe(target,s[1],1,-1,&o) || o.packets || o.errors) goto done;
    }
    if(send(s[0],packet,sizeof(packet),0)!=(ssize_t)sizeof(packet)) goto done;
    alarm(2);
    w=waitpid(target,&status,0);
    alarm(0);
    if(w==target && WIFEXITED(status) && !WEXITSTATUS(status)) {
        target=-1; result=0;
    }
done:
    alarm(0);
    if(tracer>0) { kill(tracer,SIGKILL); waitpid(tracer,&status,0); }
    if(target>0) { kill(target,SIGTERM); waitpid(target,&status,0); }
    close(s[0]); close(ready[0]);
    printf("observer_%s_selftest=%s target_resumed=%d\n",
        kill_observer?"tracer_death":"detach",result?"fail":"pass",!result);
    return result;
}

static int validate_native_target(pid_t tid,int fd) {
    char name[128],line[256],link[128],target[128],expected[128];
    unsigned long long inode,found;
    ssize_t n;
    FILE *f;
    snprintf(name,sizeof(name),"/proc/%d/exe",tid);
    n=readlink(name,target,sizeof(target)-1);
    if(n<0) return -1;
    target[n]=0;
    if(!strcmp(target,"/usr/bin/mico_aivs_lab")) strcpy(expected,"/tmp/mico_aivs_lab/usock/speech.usock");
    else if(!strcmp(target,"/usr/bin/mipns-xiaomi")) strcpy(expected,"/tmp/mipns/usock/speech.usock");
    else return -1;
    snprintf(name,sizeof(name),"/proc/%d/fd/%d",tid,fd);
    n=readlink(name,link,sizeof(link)-1);
    if(n<0) return -1;
    link[n]=0;
    if(sscanf(link,"socket:[%llu]",&inode)!=1) return -1;
    f=fopen("/proc/net/unix","r");
    if(!f) return -1;
    while(fgets(line,sizeof(line),f)) {
        if(sscanf(line,"%*s %*s %*s %*s %*s %*s %llu %127s",&found,target)==2 &&
           found==inode && !strcmp(target,expected)) { fclose(f); return 0; }
    }
    fclose(f); return -1;
}

int main(int argc,char **argv) {
    struct observation o={0};
    char *end;
    long tid,fd,seconds;
    setvbuf(stdout,NULL,_IOLBF,0);
    if(argc==2 && !strcmp(argv[1],"--self-test"))
        return selftest() || resume_test(0) || resume_test(1);
    if(argc!=5 || strcmp(argv[1],"--observe")) {
        fputs("usage: native-speech-observer --self-test | --observe TID FD SECONDS\n",stderr); return 2;
    }
    tid=strtol(argv[2],&end,10); if(*end || tid<=1 || tid>4194304) return 2;
    fd=strtol(argv[3],&end,10); if(*end || fd<0 || fd>1024) return 2;
    seconds=strtol(argv[4],&end,10); if(*end || seconds<1 || seconds>60) return 2;
    if(validate_native_target((pid_t)tid,(int)fd)) {
        fputs("refusing: target must be native speaker speech socket\n",stderr); return 2;
    }
    return observe((pid_t)tid,(int)fd,(unsigned)seconds,-1,&o);
}
