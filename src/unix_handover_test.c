/* Synthetic socket handover experiment. Never touches native speaker sockets. */
#define _DEFAULT_SOURCE
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/time.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int endpoint(const char *path) {
    struct sockaddr_un a;
    struct timeval t = {1,0};
    int fd = socket(AF_UNIX,SOCK_DGRAM,0);
    if (fd < 0) return -1;
    memset(&a,0,sizeof(a)); a.sun_family=AF_UNIX;
    if (strlen(path)>=sizeof(a.sun_path)) { close(fd); return -1; }
    strcpy(a.sun_path,path);
    if (setsockopt(fd,SOL_SOCKET,SO_RCVTIMEO,&t,sizeof(t)) ||
        bind(fd,(struct sockaddr*)&a,(socklen_t)(offsetof(struct sockaddr_un,sun_path)+strlen(path)+1))) {
        close(fd); return -1;
    }
    return fd;
}

static int send_packet(int fd,const char *path,const unsigned char *b,size_t n) {
    struct sockaddr_un a;
    memset(&a,0,sizeof(a)); a.sun_family=AF_UNIX;
    if (strlen(path)>=sizeof(a.sun_path)) return -1;
    strcpy(a.sun_path,path);
    return sendto(fd,b,n,0,(struct sockaddr*)&a,
        (socklen_t)(offsetof(struct sockaddr_un,sun_path)+strlen(path)+1)) == (ssize_t)n ? 0 : -1;
}

static int receive_packet(int fd,const unsigned char *b,size_t n,const char *sender) {
    unsigned char got[64];
    struct sockaddr_un a;
    socklen_t len=sizeof(a);
    ssize_t count;
    memset(&a,0,sizeof(a));
    count=recvfrom(fd,got,sizeof(got),0,(struct sockaddr*)&a,&len);
    if(count!=(ssize_t)n || memcmp(got,b,n)) return -1;
    /* Also verifies the sender name that native reply logic may depend on. */
    return strcmp(a.sun_path,sender) ? -1 : 0;
}

int main(void) {
    char dir[]="/tmp/lx06-socket-test-XXXXXX";
    char pns[108],aivs[108],real_pns[108],real_aivs[108];
    int rp=-1,ra=-1,pp=-1,pa=-1,result=1;
    static const unsigned char up[]={8,0,18,2,8,2};
    static const unsigned char down[]={8,1,26,2,8,10};
    if(!mkdtemp(dir)) { perror("mkdtemp"); return 1; }
    snprintf(pns,sizeof(pns),"%s/pns",dir);
    snprintf(aivs,sizeof(aivs),"%s/aivs",dir);
    snprintf(real_pns,sizeof(real_pns),"%s/pns.real",dir);
    snprintf(real_aivs,sizeof(real_aivs),"%s/aivs.real",dir);
    rp=endpoint(pns); ra=endpoint(aivs);
    if(rp<0 || ra<0) goto done;
    if(rename(pns,real_pns) || rename(aivs,real_aivs)) goto done;
    pp=endpoint(pns); pa=endpoint(aivs);
    if(pp<0 || pa<0) goto done;
    if(send_packet(rp,aivs,up,sizeof(up)) || receive_packet(pa,up,sizeof(up),pns) ||
       send_packet(pp,real_aivs,up,sizeof(up)) || receive_packet(ra,up,sizeof(up),pns) ||
       send_packet(ra,pns,down,sizeof(down)) || receive_packet(pp,down,sizeof(down),aivs) ||
       send_packet(pa,real_pns,down,sizeof(down)) || receive_packet(rp,down,sizeof(down),aivs)) goto done;
    close(pp); pp=-1; close(pa); pa=-1;
    if(unlink(pns) || unlink(aivs) || rename(real_pns,pns) || rename(real_aivs,aivs)) goto done;
    if(send_packet(rp,aivs,up,sizeof(up)) || receive_packet(ra,up,sizeof(up),pns) ||
       send_packet(ra,pns,down,sizeof(down)) || receive_packet(rp,down,sizeof(down),aivs)) goto done;
    puts("socket_handover_selftest=pass relay=both_directions sender_names=preserved restore=pass");
    result=0;
done:
    if(result) fputs("socket_handover_selftest=fail\n",stderr);
    if(rp>=0) close(rp);
    if(ra>=0) close(ra);
    if(pp>=0) close(pp);
    if(pa>=0) close(pa);
    unlink(pns); unlink(aivs); unlink(real_pns); unlink(real_aivs);
    if(rmdir(dir)) { perror("cleanup"); result=1; }
    return result;
}
