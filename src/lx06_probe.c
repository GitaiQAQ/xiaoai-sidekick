#include "native_protocol.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __linux__
#include <sys/stat.h>
#include <sys/utsname.h>
#include <unistd.h>
#endif

struct guarded_reader { const unsigned char *packet; size_t forbidden_start, reads; };
static int guarded_byte(void *context,size_t offset,uint8_t *byte) {
    struct guarded_reader *g=context;
    if(offset>=g->forbidden_start) return -1;
    ++g->reads; *byte=g->packet[offset];
    return 0;
}

static int selftest(void) {
    /* Synthetic protobuf fixtures; never captured microphone data. */
    static const unsigned char stream[] = {8,0,18,11,8,3,34,7,8,0,18,3,0,0,0};
    static const unsigned char finish[] = {8,1,26,2,8,5};
    static const unsigned char wrong_direction[] = {8,1,18,2,8,0};
    static const unsigned char duplicate[] = {8,0,8,0,18,2,8,0};
    static const unsigned char overflow[] = {8,0,18,11,8,255,255,255,255,255,255,255,255,255,2};
    struct lx06_packet_metadata m;
    struct guarded_reader g={stream,sizeof(stream)-3,0};
    size_t n;
    unsigned cases = 0;
    if (lx06_packet_metadata(stream,sizeof(stream),&m) || m.direction || m.type != 3 ||
        !m.has_stream || m.audio_bytes != 3) return 1;
    ++cases;
    if (lx06_packet_metadata(finish,sizeof(finish),&m) || m.direction != 1 || m.type != 5) return 1;
    ++cases;
    for (n=0; n<sizeof(stream); ++n) {
        if (!lx06_packet_metadata(stream,n,&m)) return 1;
        ++cases;
    }
    if (!lx06_packet_metadata(wrong_direction,sizeof(wrong_direction),&m) ||
        !lx06_packet_metadata(duplicate,sizeof(duplicate),&m) ||
        !lx06_packet_metadata(overflow,sizeof(overflow),&m)) return 1;
    cases += 3;
    if(lx06_packet_metadata_reader(guarded_byte,&g,sizeof(stream),&m) ||
       m.audio_bytes!=3 || !g.reads) return 1;
    ++cases;
    printf("payload_skip_test=pass metadata_bytes_read=%zu audio_bytes_read=0\n",g.reads);
    printf("protocol_selftest=pass cases=%u\n", cases);
    return 0;
}

static int status(void) {
#ifdef __linux__
    struct utsname u;
    struct stat s;
    FILE *f;
    char line[256];
    const char *paths[] = {"/tmp/mipns/usock/speech.usock", "/tmp/mico_aivs_lab/usock/speech.usock"};
    size_t i;
    if (uname(&u)) return 1;
    printf("kernel=%s machine=%s pointer_bits=%zu\n",u.release,u.machine,sizeof(void*)*8);
#ifdef __ARM_PCS_VFP
    puts("abi=arm-hardfloat");
#else
    puts("abi=other");
#endif
    for (i=0;i<sizeof(paths)/sizeof(paths[0]);++i)
        printf("socket=%s present=%d\n",paths[i],stat(paths[i],&s)==0 && S_ISSOCK(s.st_mode));
    f=fopen("/proc/meminfo","r");
    if (!f) return 1;
    while (fgets(line,sizeof(line),f))
        if (!strncmp(line,"MemTotal:",9) || !strncmp(line,"MemAvailable:",13)) fputs(line,stdout);
    fclose(f);
    return 0;
#else
    fputs("status requires Linux\n",stderr);
    return 2;
#endif
}

int main(int argc, char **argv) {
    if (argc==2 && !strcmp(argv[1],"--self-test")) return selftest();
    if (argc==2 && !strcmp(argv[1],"--status")) return status();
    fputs("usage: lx06-probe --self-test | --status\nRead-only; no sockets opened or audio captured.\n",stderr);
    return 2;
}
