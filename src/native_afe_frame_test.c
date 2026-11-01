/* Dynamic check of AFE_DnnProcessPostAgc's frame contract, using only exported
   factory symbols. No audio device is opened, nothing is captured, and the
   vendor processes are untouched: the DNN node object is fabricated locally.

   Layout under test (see research/native-wakeup/native-dnn-frame-layout.md):
     obj[0]=dnn node, obj[4]=queue A (DNN blocks), obj[8]=queue B (plane 1),
     obj[0xc]=256-sample scratch;
     dnn[0x408]=frame length, dnn[0x40c]=fill, dnn+8=sample buffer.
   Each call feeds 640 B: plane 0 at +0 (160 int16), plane 1 at +0x140. */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <string.h>
#include <sys/prctl.h>
#include <stdint.h>
#include <signal.h>
#include <ucontext.h>
#include <unistd.h>

struct lx_queue { unsigned char bytes[16]; };
struct lx_dnn { unsigned char pad[8]; short buffer[512]; unsigned frame_len; unsigned fill; };
struct lx_afe { void *dnn; void *queue_a; void *queue_b; void *scratch; void *spare; short sentinel; };

static void on_fault(int sig,siginfo_t *info,void *context) {
    ucontext_t *uc=(ucontext_t *)context;char line[160];int n;
    Dl_info mod;const char *pname="?";const char *lname="?";
    unsigned long poff=0,loff=0;
    if(dladdr((void *)(uintptr_t)uc->uc_mcontext.arm_pc,&mod)&&mod.dli_fname) {
        pname=mod.dli_fname;poff=(unsigned long)(uintptr_t)uc->uc_mcontext.arm_pc-(unsigned long)(uintptr_t)mod.dli_fbase;
    }
    if(dladdr((void *)(uintptr_t)uc->uc_mcontext.arm_lr,&mod)&&mod.dli_fname) {
        lname=mod.dli_fname;loff=(unsigned long)(uintptr_t)uc->uc_mcontext.arm_lr-(unsigned long)(uintptr_t)mod.dli_fbase;
    }
    n=snprintf(line,sizeof(line),"fault sig=%d addr=%p pc=%s+%lx lr=%s+%lx\n",sig,
        info->si_addr,pname,poff,lname,loff);
    if(n>0)write(2,line,(size_t)n);
    _exit(128+sig);
}

typedef int (*create_fn)(void *, unsigned);
typedef int (*afe_fn)(void *, const short *, unsigned, short *, int *);

int main(int argc,char **argv) {
    void *handle;create_fn create;afe_fn afe;
    struct lx_queue queue_a,queue_b;struct lx_dnn dnn;struct lx_afe obj;
    static short scratch_buf[256];
    short input[320],output[320];int outlen,ok=1,calls,frames=0;
    long counter=0;
    (void)argc;(void)argv;
    setvbuf(stdout,NULL,_IONBF,0);
    {struct sigaction sa;memset(&sa,0,sizeof(sa));sa.sa_sigaction=on_fault;sa.sa_flags=SA_SIGINFO;
     sigaction(SIGSEGV,&sa,NULL);sigaction(SIGBUS,&sa,NULL);}
    if(prctl(PR_SET_DUMPABLE,0L,0L,0L,0L)||prctl(PR_GET_DUMPABLE,0L,0L,0L,0L)!=0) {
        fputs("unable to disable test core dumps\n",stderr);return 1;
    }
    puts("probe_dumpable=0");
    /* The engine is C++ and does not carry its libstdc++ dependency twice. */
    puts("stage=libstdc++");
    if(!dlopen("libstdc++.so.6",RTLD_NOW|RTLD_GLOBAL)) {
        fprintf(stderr,"libstdc++ load failed: %s\n",dlerror());return 1;
    }
    puts("stage=engine");
    handle=dlopen("libxaudio_engine.so",RTLD_NOW|RTLD_LOCAL);
    if(!handle)handle=dlopen("libxaudio_engine.so",RTLD_LAZY|RTLD_LOCAL);
    if(!handle) { fprintf(stderr,"dlopen failed: %s\n",dlerror());return 1; }
    create=(create_fn)dlsym(handle,"Xaudio_CreateElementQueue_s");
    afe=(afe_fn)dlsym(handle,"AFE_DnnProcessPostAgc");
    puts("stage=symbols");
    if(!create||!afe) { fprintf(stderr,"missing engine symbols\n");return 1; }
    memset(&queue_a,0,sizeof(queue_a));memset(&queue_b,0,sizeof(queue_b));
    memset(&dnn,0,sizeof(dnn));memset(&obj,0,sizeof(obj));
    dnn.frame_len=0x100;
    if(create(&queue_a,0x500)||create(&queue_b,0x500)) {
        fputs("element queue create failed\n",stderr);return 1;
    }
    puts("stage=run");
    obj.dnn=&dnn;obj.queue_a=&queue_a;obj.queue_b=&queue_b;
    obj.scratch=scratch_buf;obj.spare=scratch_buf;
    for(calls=0;calls<12;calls++) {
        int j,rc;int expect_dnn,expect_raw;
        for(j=0;j<160;j++)input[j]=(short)counter++;
        for(j=0;j<160;j++)input[160+j]=(short)(5000+calls*1000+j);
        outlen=0x7fffffff;
        rc=afe(&obj,input,640,output,&outlen);
        if(outlen!=640) {
            if(outlen!=-1) { printf("call=%d unexpected_outlen=%d\n",calls,outlen);ok=0; }
            else printf("call=%d outlen=-1 rc=%d\n",calls,rc);
            continue;
        }
        expect_dnn=160*frames;
        expect_raw=5000+frames*1000;
        if(output[0]!=(short)expect_dnn||output[159]!=(short)(expect_dnn+159)||
           output[160]!=(short)expect_raw||output[319]!=(short)(expect_raw+159)) {
            printf("call=%d frame=%d mismatch dnn0=%d raw0=%d\n",
                calls,frames,output[0],output[160]);
            ok=0;
        } else {
            printf("call=%d frame=%d outlen=640 dnn_start=%d raw_start=%d\n",
                calls,frames,expect_dnn,expect_raw);
        }
        frames++;
    }
    printf("afe_frame_test=%s frames=%d dnn_fill=%u\n",ok?"pass":"fail",frames,dnn.fill);
    printf("audio_devices_opened=0 network_connections=0 vendor_processes_touched=0\n");
    dlclose(handle);
    return !ok;
}
