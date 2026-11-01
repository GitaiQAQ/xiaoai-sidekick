#ifndef LX06_NATIVE_WEBSOCKET_TEST_H
#define LX06_NATIVE_WEBSOCKET_TEST_H
#include "native_websocket.h"
struct lx_ws_test_events {unsigned count,op[8];size_t length[8];unsigned char data[8][32];};
static int lx_ws_test_event(void *ctx,unsigned op,const unsigned char *p,size_t n) {
    struct lx_ws_test_events *e=ctx;
    if(e->count>=8||n>32)return -1;
    e->op[e->count]=op;e->length[e->count]=n;memcpy(e->data[e->count],p,n);e->count++;return 0;
}
static int lx_ws_stream_self_test(void) {
    /* UTF8 Chinese character split over WS frames, ping interleaved, then text+close. */
    const unsigned char stream[]={0x01,1,0xe4,0x89,1,'p',0x80,2,0xbd,0xa0,
        0x81,2,'O','K',0x88,2,0x03,0xe8};
    unsigned checks=0;int pass=1;
    for(size_t split=0;split<=sizeof(stream);split++) {
        unsigned char buf[32];struct lx_ws_receiver s={.message=buf,.capacity=sizeof(buf)};
        struct lx_ws_test_events e={0};
        int r=lx_ws_feed(&s,stream,split,lx_ws_test_event,&e);
        if(!s.closed)r|=lx_ws_feed(&s,stream+split,sizeof(stream)-split,lx_ws_test_event,&e);
        pass&=!r&&s.closed&&e.count==4&&e.op[0]==9&&e.length[0]==1&&e.data[0][0]=='p'&&
            e.op[1]==1&&e.length[1]==3&&!memcmp(e.data[1],"\xe4\xbd\xa0",3)&&
            e.op[2]==1&&e.length[2]==2&&!memcmp(e.data[2],"OK",2)&&e.op[3]==8;
        checks++;
    }
    const unsigned char invalid[][10]={
        {0x80,0}, /* continuation without start */
        {0x01,0,0x81,0}, /* interleaved data */
        {0x81,2,0xc0,0x80}, /* overlong UTF8 */
        {0x81,3,0xed,0xa0,0x80}, /* surrogate */
        {0x81,4,0xf4,0x90,0x80,0x80}, /* above Unicode range */
        {0x81,1,0xe4}, /* truncated UTF8 */
        {0x88,2,0x03,0xed}, /* forbidden close1005 */
        {0x88,3,0x03,0xe8,0xff}, /* invalid close reason */
        {0x01,2,'a','b',0x80,3,'c','d','e'} /* cumulative limit */
    };
    const size_t sizes[]={2,4,4,5,6,3,4,5,9};
    for(size_t i=0;i<sizeof(sizes)/sizeof(sizes[0]);i++) {
        unsigned char buf[32];struct lx_ws_receiver s={.message=buf,.capacity=i==8?4:sizeof(buf)};
        struct lx_ws_test_events e={0};int r=0;
        /* one-byte reads exercise header AND payload fragmentation */
        for(size_t k=0;k<sizes[i]&&!r;k++)r=lx_ws_feed(&s,invalid[i]+k,1,lx_ws_test_event,&e);
        pass&=r==-1&&s.failed&&e.count==0;checks++;
    }
    const char *accept="s3pPLMBiTxaQ9kYGzzhZRbK+xOo=";
    const char good[]="HTTP/1.1 101 Switching Protocols\r\nuPgRaDe: WebSocket\r\nConnection: keep-alive, Upgrade\r\nSec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\n\r\n";
    const char duplicate[]="HTTP/1.1 101 X\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\nSec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\n\r\n";
    const char extension[]="HTTP/1.1 101 X\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\nSec-WebSocket-Extensions: permessage-deflate\r\n\r\n";
    pass&=lx_ws_upgrade_valid(good,sizeof(good)-1,accept)&&
        !lx_ws_upgrade_valid(good,sizeof(good)-2,accept)&&
        !lx_ws_upgrade_valid(good,sizeof(good)-1,"wrong")&&
        !lx_ws_upgrade_valid(duplicate,sizeof(duplicate)-1,accept)&&
        !lx_ws_upgrade_valid(extension,sizeof(extension)-1,accept);
    checks+=5;
    printf("ws_stream_handshake_self_test=%s checks=%u network_connections=0\n",pass?"pass":"fail",checks);
    return !pass;
}
#endif
