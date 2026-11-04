/* Host-side checks for the embedded mDNS responder's packet handling. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../src/native_mdns.h"

static size_t build_query(unsigned char *out,const char *name,unsigned qtype){
 size_t o=0;memset(out,0,12);
 out[0]=out[1]=0;out[2]=0;out[3]=0;
 out[4]=0;out[5]=1;out[6]=0;out[7]=0;out[8]=0;out[9]=0;out[10]=0;out[11]=0;o=12;
 size_t n=mdns_name_encode(out+o,256,name);assert(n);o+=n;
 out[o++]=(unsigned char)(qtype>>8);out[o++]=(unsigned char)qtype;
 out[o++]=0;out[o++]=1;/* IN, no QU bit */
 return o;
}

int main(void){
 unsigned checks=0;
 unsigned char name[64];
 size_t n=mdns_name_encode(name,sizeof name,LX_MDNS_NAME);
 assert(n==14);assert(name[0]==6&&!memcmp(name+1,"xiaoai",6));assert(name[7]==5&&!memcmp(name+8,"local",5));assert(name[13]==0);checks+=2;
 n=mdns_name_encode(name,sizeof name,LX_MDNS_NAME2);
 assert(n==20);assert(name[0]==12&&!memcmp(name+1,"doubaodoubao",12));assert(name[13]==5&&!memcmp(name+14,"local",5));assert(name[19]==0);checks+=2;

 unsigned char q[512];size_t qn=build_query(q,LX_MDNS_NAME,1),qend=0;
 assert(mdns_match_query(q,qn,&qend));assert(qend==qn);checks+=2;

 qn=build_query(q,"XiaoAI.Local",255);assert(mdns_match_query(q,qn,&qend));checks++;
 qn=build_query(q,LX_MDNS_NAME2,1);assert(mdns_match_query(q,qn,&qend));checks++;
 qn=build_query(q,"DouBaoDouBao.LOCAL",255);assert(mdns_match_query(q,qn,&qend));checks++;

 qn=build_query(q,LX_MDNS_NAME,28);assert(!mdns_match_query(q,qn,&qend));checks++;
 qn=build_query(q,"other.local",1);assert(!mdns_match_query(q,qn,&qend));checks++;
 qn=build_query(q,"doubaodoubao.local.evil.com",1);assert(!mdns_match_query(q,qn,&qend));checks++;
 qn=build_query(q,"doubaodoubao",1);assert(!mdns_match_query(q,qn,&qend));checks++;
 qn=build_query(q,LX_MDNS_NAME,1);q[2]=0x80;assert(!mdns_match_query(q,qn,&qend));checks++;/* response, not query */
 qn=build_query(q,LX_MDNS_NAME,1);q[5]=2;assert(!mdns_match_query(q,qn,&qend));checks++;/* QDCOUNT != 1 */

 qn=build_query(q,LX_MDNS_NAME,1);assert(mdns_match_query(q,qn,&qend));
 const unsigned char ip[4]={192,0,2,10}; /* TEST-NET-1; never a device address */
 unsigned char out[512];size_t o=mdns_answer_packet(out,sizeof out,q,qend,ip,LX_MDNS_TTL,1);
 assert(o==qend+16);checks++;
 assert(out[2]==0x84&&out[3]==0x00);assert(out[5]==1&&out[7]==1);/* QR+AA, QD=1 AN=1 */
 assert(!memcmp(out+o-16,"\xc0\x0c\x00\x01\x80\x01",6));
 assert(out[o-10]==0&&out[o-9]==0&&out[o-8]==(unsigned char)(LX_MDNS_TTL>>8)&&out[o-7]==(unsigned char)LX_MDNS_TTL);
 assert(out[o-6]==0&&out[o-5]==4&&!memcmp(out+o-4,ip,4));checks+=4;

 /* A QU query's echoed question must have the top qclass bit cleared. */
 q[qend-2]|=0x80;o=mdns_answer_packet(out,sizeof out,q,qend,ip,LX_MDNS_TTL,1);
 assert(out[qend-2]==0x00&&out[qend-1]==0x01);checks++;
 /* Legacy unicast (non-5353 source) uses a 10 s TTL and no cache-flush bit. */
 o=mdns_answer_packet(out,sizeof out,q,qend,ip,10,0);
 assert(out[qend]==0xc0&&out[qend+1]==0x0c&&out[qend+2]==0&&out[qend+3]==1&&out[qend+4]==0&&out[qend+5]==1&&out[qend+6]==0&&out[qend+7]==0&&out[qend+8]==0&&out[qend+9]==10);checks++;

 unsigned short ttl=0;/* goodbye clears both records */
 o=mdns_announce_packet(out,sizeof out,ttl,ip);assert(o==12+(14+14)+(20+14));checks++;
 assert(out[2]==0x84&&out[7]==LX_MDNS_NAME_COUNT);
 assert(out[o-10]==0&&out[o-9]==0&&out[o-8]==0&&out[o-7]==0&&out[o-6]==0&&out[o-5]==4&&!memcmp(out+o-4,ip,4));checks++;
 /* First record name starts at offset 12; type/class are 2+2 bytes, then the cleared TTL. */
 assert(out[12]==6&&!memcmp(out+13,"xiaoai",6));assert(out[12+14+4]==0&&out[12+14+5]==0&&out[12+14+6]==0&&out[12+14+7]==0);checks++;
 o=mdns_announce_packet(out,sizeof out,LX_MDNS_TTL,ip);assert(o>12&&out[7]==2&&out[o-6]==0&&out[o-5]==4&&!memcmp(out+o-4,ip,4));checks+=2;

 /* Oversized names must not be accepted by the encoder. */
 char label[70];memset(label,'b',69);label[69]=0;assert(!mdns_name_encode(name,sizeof name,label));checks++;

 printf("mdns_responder=pass checks=%u network_connections=0\n",checks);
 return 0;
}
