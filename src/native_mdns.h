/* Minimal mDNS responder that advertises one A record for the current IPv4. */
#ifndef LX_NATIVE_MDNS_H
#define LX_NATIVE_MDNS_H
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>

#define LX_MDNS_NAME "xiaoai.local"
#define LX_MDNS_NAME2 "doubaodoubao.local"
#define LX_MDNS_GROUP "224.0.0.251"
#define LX_MDNS_PORT 5353
#define LX_MDNS_TTL 120

static const char *const mdns_names[]={LX_MDNS_NAME,LX_MDNS_NAME2};
#define LX_MDNS_NAME_COUNT (sizeof mdns_names/sizeof mdns_names[0])

static int mdns_fd=-1;
static int64_t mdns_next_announce;
static unsigned char mdns_ip[4];

static int mdns_current_ip(unsigned char out[4]){
 struct ifaddrs *ifa,*p;int found=0;unsigned char fallback[4];int have_fallback=0;
 if(getifaddrs(&ifa))return 0;
 for(p=ifa;p;p=p->ifa_next){
  if(!p->ifa_addr||p->ifa_addr->sa_family!=AF_INET)continue;
  if(!strcmp(p->ifa_name,"lo"))continue;
  const unsigned char *a=(const unsigned char*)&((struct sockaddr_in*)p->ifa_addr)->sin_addr;
  if(!strcmp(p->ifa_name,"wlan0")){memcpy(out,a,4);found=1;break;}
  if(!have_fallback){memcpy(fallback,a,4);have_fallback=1;}
 }
 freeifaddrs(ifa);
 if(!found&&have_fallback){memcpy(out,fallback,4);found=1;}
 return found;
}

static size_t mdns_name_encode(unsigned char *out,size_t cap,const char *name){
 size_t o=0;const char *p=name;
 while(*p){
  const char *dot=strchr(p,'.');size_t n=dot?(size_t)(dot-p):strlen(p);
  if(!n||n>63||o+n+2>cap)return 0;
  out[o++]=(unsigned char)n;memcpy(out+o,p,n);o+=n;p=dot?dot+1:p+n;
 }
 if(o+1>cap)return 0;out[o++]=0;return o;
}

/* Return 1 for a single uncompressed question naming one of our hosts with A/ANY. */
static int mdns_match_query(const unsigned char *q,size_t n,size_t *qend){
 if(n<12||(q[2]&0x80))return 0;
 if(((q[4]<<8)|q[5])!=1)return 0;
 char name[256];size_t ni=0,off=12;int ok=0;
 while(off<n&&q[off]){
  unsigned l=q[off++];
  if((l&0xc0)||l>63||ni+l+1>=sizeof name)return 0;
  if(off+l>n)return 0;
  memcpy(name+ni,q+off,l);ni+=l;name[ni++]='.';off+=l;
 }
 if(!ni||off+5>n)return 0;
 ni--;name[ni]=0;off++;
 unsigned qtype=(q[off]<<8)|q[off+1];
 off+=4;
 if(qtype==1||qtype==255)
  for(unsigned i=0;i<LX_MDNS_NAME_COUNT;i++)if(!strcasecmp(name,mdns_names[i])){ok=1;break;}
 *qend=off;
 return ok;
}

/* Build a response to a single A question. The echoed question has its
   unicast-response bit cleared; cache_flush and ttl follow the transport
   (normal mDNS vs legacy unicast from a non-5353 source port). */
static size_t mdns_answer_packet(unsigned char *out,size_t cap,const unsigned char *q,size_t qend,const unsigned char ip[4],unsigned ttl,int cache_flush){
 if(qend<12||qend>cap-64)return 0;
 size_t o=0;
 out[o++]=q[0];out[o++]=q[1];out[o++]=0x84;out[o++]=0x00;
 out[o++]=0;out[o++]=1;out[o++]=0;out[o++]=1;
 out[o++]=0;out[o++]=0;out[o++]=0;out[o++]=0;
 memcpy(out+o,q+12,qend-12);o+=qend-12;
 out[o-2]&=(unsigned char)0x7f;/* clear QU in the echoed QCLASS */
 out[o++]=0xc0;out[o++]=0x0c;
 out[o++]=0;out[o++]=1;out[o++]=cache_flush?0x80:0;out[o++]=1;
 out[o++]=(unsigned char)(ttl>>24);out[o++]=(unsigned char)(ttl>>16);
 out[o++]=(unsigned char)(ttl>>8);out[o++]=(unsigned char)ttl;
 out[o++]=0;out[o++]=4;
 memcpy(out+o,ip,4);o+=4;
 return o;
}

static size_t mdns_announce_packet(unsigned char *out,size_t cap,unsigned ttl,const unsigned char ip[4]){
 size_t o=0;unsigned char name[64];
 out[o++]=0;out[o++]=0;out[o++]=0x84;out[o++]=0x00;
 out[o++]=0;out[o++]=0;out[o++]=0;out[o++]=LX_MDNS_NAME_COUNT;
 out[o++]=0;out[o++]=0;out[o++]=0;out[o++]=0;
 for(unsigned i=0;i<LX_MDNS_NAME_COUNT;i++){
  size_t nl=mdns_name_encode(name,sizeof name,mdns_names[i]);if(!nl||o+nl+16>cap)return 0;
  memcpy(out+o,name,nl);o+=nl;
  out[o++]=0;out[o++]=1;out[o++]=0x80;out[o++]=1;
  out[o++]=(unsigned char)(ttl>>24);out[o++]=(unsigned char)(ttl>>16);
  out[o++]=(unsigned char)(ttl>>8);out[o++]=(unsigned char)ttl;
  out[o++]=0;out[o++]=4;
  memcpy(out+o,ip,4);o+=4;
 }
 return o;
}

static void mdns_multicast(int fd,const unsigned char *buf,size_t n){
 struct sockaddr_in dst;memset(&dst,0,sizeof dst);
 dst.sin_family=AF_INET;dst.sin_port=htons(LX_MDNS_PORT);
 inet_pton(AF_INET,LX_MDNS_GROUP,&dst.sin_addr);
 sendto(fd,buf,n,0,(struct sockaddr*)&dst,sizeof dst);
}

static void mdns_announce(int fd,unsigned ttl){
 unsigned char out[128],ip[4];
 if(!mdns_current_ip(ip))return;
 size_t n=mdns_announce_packet(out,sizeof out,ttl,ip);
 if(n)mdns_multicast(fd,out,n);
}

static int mdns_open(void){
 unsigned char ip[4];
 int s=socket(AF_INET,SOCK_DGRAM,0);if(s<0)return -1;
 int one=1;
 setsockopt(s,SOL_SOCKET,SO_REUSEADDR,&one,sizeof one);
#ifdef SO_REUSEPORT
 setsockopt(s,SOL_SOCKET,SO_REUSEPORT,&one,sizeof one);
#endif
 struct sockaddr_in a;memset(&a,0,sizeof a);
 a.sin_family=AF_INET;a.sin_port=htons(LX_MDNS_PORT);a.sin_addr.s_addr=htonl(INADDR_ANY);
 if(bind(s,(struct sockaddr*)&a,sizeof a)){close(s);return -1;}
 struct ip_mreq m;memset(&m,0,sizeof m);
 inet_pton(AF_INET,LX_MDNS_GROUP,&m.imr_multiaddr);
 if(mdns_current_ip(ip))memcpy(&m.imr_interface.s_addr,ip,4);
 else m.imr_interface.s_addr=htonl(INADDR_ANY);
 if(setsockopt(s,IPPROTO_IP,IP_ADD_MEMBERSHIP,&m,sizeof m)){
  m.imr_interface.s_addr=htonl(INADDR_ANY);
  if(setsockopt(s,IPPROTO_IP,IP_ADD_MEMBERSHIP,&m,sizeof m)){close(s);return -1;}
 }
 int fl=fcntl(s,F_GETFL,0);if(fl>=0)fcntl(s,F_SETFL,fl|O_NONBLOCK);
 return s;
}

static void mdns_handle(int fd){
 unsigned char q[1500];
 for(int k=0;k<8;k++){
  struct sockaddr_in from;socklen_t fl=sizeof from;
  ssize_t n=recvfrom(fd,q,sizeof q,0,(struct sockaddr*)&from,&fl);
  if(n<12){if(n<0)return;continue;}
  size_t qend=0;
  if(!mdns_match_query(q,(size_t)n,&qend))continue;
  unsigned char ip[4];if(!mdns_current_ip(ip))continue;
  unsigned qclass=(q[qend-2]<<8)|q[qend-1];
  int legacy=ntohs(from.sin_port)!=LX_MDNS_PORT;
  unsigned char out[512];
  size_t o=mdns_answer_packet(out,sizeof out,q,qend,ip,legacy?10:LX_MDNS_TTL,legacy?0:1);
  if(!o)continue;
  if(legacy||(qclass&0x8000)){
   sendto(fd,out,o,0,(struct sockaddr*)&from,sizeof from);
  }else mdns_multicast(fd,out,o);
 }
}

static void mdns_tick(int64_t now){
 if(mdns_fd<0)return;
 unsigned char ip[4];
 if(mdns_current_ip(ip)&&memcmp(ip,mdns_ip,4)){
  memcpy(mdns_ip,ip,4);
  mdns_announce(mdns_fd,LX_MDNS_TTL);
  mdns_next_announce=now+60000;
 }else if(now>=mdns_next_announce){
  mdns_announce(mdns_fd,LX_MDNS_TTL);
  mdns_next_announce=now+60000;
 }
}
#endif
