/* Loopback-only bounded HTTP audio fixture. Generated PCM, no media files. */
#define _GNU_SOURCE
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/prctl.h>
#include <sys/time.h>
#include <unistd.h>
#include <poll.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
static volatile sig_atomic_t stopped;
static void stop(int s){(void)s;stopped=1;}
static int64_t ms(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return (int64_t)t.tv_sec*1000+t.tv_nsec/1000000;}
static void le16(unsigned char *p,unsigned n){p[0]=n;p[1]=n>>8;}
static void le32(unsigned char *p,unsigned n){le16(p,n);le16(p+2,n>>16);}
static int send_all(int fd,const void *v,size_t n){const unsigned char *p=v;while(n&&!stopped){ssize_t k=send(fd,p,n,MSG_NOSIGNAL);if(k<=0)return 1;p+=k;n-=(size_t)k;}return n!=0;}
int main(void){
 int sock,client,one=1,requests=0;struct sockaddr_in addr={0};int64_t end=ms()+55000;
 const unsigned bytes=24000*2*20,total=bytes+44;unsigned char wav[44]={0};
 signal(SIGINT,stop);signal(SIGTERM,stop);signal(SIGPIPE,SIG_IGN);prctl(PR_SET_DUMPABLE,0);
 memcpy(wav,"RIFF",4);le32(wav+4,bytes+36);memcpy(wav+8,"WAVEfmt ",8);le32(wav+16,16);
 le16(wav+20,1);le16(wav+22,1);le32(wav+24,24000);le32(wav+28,48000);le16(wav+32,2);le16(wav+34,16);memcpy(wav+36,"data",4);le32(wav+40,bytes);
 sock=socket(AF_INET,SOCK_STREAM,0);setsockopt(sock,SOL_SOCKET,SO_REUSEADDR,&one,sizeof(one));
 addr.sin_family=AF_INET;addr.sin_port=htons(18087);addr.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
 if(bind(sock,(struct sockaddr *)&addr,sizeof(addr))||listen(sock,4))return 2;
 puts("fixture_ready loopback=1 seconds=55");fflush(stdout);
 while(!stopped&&ms()<end){
  struct pollfd poller={sock,POLLIN,0};char req[4096]={0},header[512];size_t used=0;unsigned from=0,to=total-1;int partial=0;
  if(poll(&poller,1,100)<=0)continue;
  client=accept(sock,NULL,NULL);if(client<0)continue;
  struct timeval timeout={2,0};setsockopt(client,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout));setsockopt(client,SOL_SOCKET,SO_SNDTIMEO,&timeout,sizeof(timeout));
  while(used<sizeof(req)-1&&!strstr(req,"\r\n\r\n")){ssize_t k=recv(client,req+used,sizeof(req)-1-used,0);if(k<=0)break;used+=(size_t)k;req[used]=0;}
  if(strncmp(req,"GET /tone",9)&&strncmp(req,"HEAD /tone",10)){send_all(client,"HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n",45);close(client);continue;}
  char *range=strcasestr(req,"\r\nRange: bytes=");
  if(range){unsigned a=0,b=total-1;int got=sscanf(range+15,"%u-%u",&a,&b);if(got>0&&a<total){from=a;to=got==2&&b<total?b:total-1;partial=1;}}
  if(to<from){close(client);continue;}
  int len=snprintf(header,sizeof(header),"HTTP/1.1 %s\r\nContent-Type: audio/wav\r\nAccept-Ranges: bytes\r\nContent-Length: %u\r\nConnection: close\r\n",partial?"206 Partial Content":"200 OK",to-from+1);
  if(partial)len+=snprintf(header+len,sizeof(header)-(size_t)len,"Content-Range: bytes %u-%u/%u\r\n",from,to,total);
  len+=snprintf(header+len,sizeof(header)-(size_t)len,"\r\n");send_all(client,header,(size_t)len);
  requests++;printf("request=%d range_start=%u range_end=%u\n",requests,from,to);fflush(stdout);
  if(!strncmp(req,"HEAD",4)){close(client);continue;}
  unsigned pos=from;int64_t next=ms();
  while(pos<=to&&!stopped&&ms()<end){unsigned char block[960];unsigned take=to-pos+1;if(take>sizeof(block))take=sizeof(block);
   for(unsigned i=0;i<take;i++){unsigned at=pos+i;if(at<44)block[i]=wav[at];else{unsigned sample=(at-44)/2;int16_t v=(sample%240<120)?160:-160;block[i]=(unsigned char)((uint16_t)v>>(((at-44)%2)*8));}}
   if(send_all(client,block,take))break;pos+=take;next+=20;int64_t wait=next-ms();if(wait>0)usleep((useconds_t)wait*1000);
  }
  printf("sent_bytes=%u\n",pos-from);fflush(stdout);close(client);
 }
 close(sock);puts("fixture_stopped");return 0;
}

