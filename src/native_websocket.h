#ifndef LX06_NATIVE_WEBSOCKET_H
#define LX06_NATIVE_WEBSOCKET_H
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* No extensions. Header parser returns 1=ready, 0=incomplete, -1=protocol/limit error. */
struct lx_ws_header { size_t header_bytes,payload_bytes; unsigned opcode,final; };
static inline int lx_ws_server_header(const unsigned char *p,size_t n,size_t limit,struct lx_ws_header *h) {
    uint64_t length;size_t header=2;unsigned code;
    if(n<2)return 0;
    code=p[0]&15;
    if((p[0]&0x70)||(p[1]&0x80)||!(code==0||code==1||code==2||code==8||code==9||code==10))return -1;
    length=p[1]&127;
    if(length==126) {
        header=4;if(n<header)return 0;
        length=((unsigned)p[2]<<8)|p[3];if(length<126)return -1;
    }else if(length==127) {
        header=10;if(n<header)return 0;
        if(p[2]&0x80)return -1;
        length=0;for(size_t i=2;i<10;i++)length=(length<<8)|p[i];
        if(length<65536)return -1;
    }
    if(length>limit||length>SIZE_MAX)return -1;
    if(code>=8&&(!(p[0]&0x80)||length>125||(code==8&&length==1)))return -1;
    h->header_bytes=header;h->payload_bytes=(size_t)length;h->opcode=code;h->final=!!(p[0]&0x80);return 1;
}

/* Caller supplies a fresh cryptographically random mask for EVERY frame. */
static inline size_t lx_ws_client_frame(unsigned char *out,size_t capacity,unsigned opcode,
        const unsigned char *data,size_t length,const unsigned char mask[4]) {
    size_t header=length<126?2:length<=65535?4:10;
    if(!(opcode==1||opcode==2||opcode==8||opcode==9||opcode==10)||
       (opcode>=8&&(length>125||(opcode==8&&length==1)))||
       length>65536||capacity<header+4||length>capacity-header-4)return 0;
    out[0]=(unsigned char)(0x80|opcode);
    if(header==2)out[1]=(unsigned char)(0x80|length);
    else if(header==4){out[1]=0xfe;out[2]=(unsigned char)(length>>8);out[3]=(unsigned char)length;}
    else {out[1]=0xff;for(size_t i=0;i<8;i++)out[2+i]=(unsigned char)((uint64_t)length>>(56-8*i));}
    memcpy(out+header,mask,4);
    for(size_t i=0;i<length;i++)out[header+4+i]=data[i]^mask[i%4];
    return header+4+length;
}

struct lx_ws_utf8 { uint32_t value,minimum; unsigned left; };
static inline int lx_ws_utf8_feed(struct lx_ws_utf8 *s,const unsigned char *p,size_t n) {
    for(size_t i=0;i<n;i++) {
        unsigned c=p[i];
        if(s->left) {
            if((c&0xc0)!=0x80)return -1;
            s->value=(s->value<<6)|(c&63);
            if(!--s->left&&(s->value<s->minimum||s->value>0x10ffff||
               (s->value>=0xd800&&s->value<=0xdfff)))return -1;
        }else if(c<0x80)continue;
        else if(c>=0xc2&&c<=0xdf){s->value=c&31;s->minimum=0x80;s->left=1;}
        else if(c>=0xe0&&c<=0xef){s->value=c&15;s->minimum=0x800;s->left=2;}
        else if(c>=0xf0&&c<=0xf4){s->value=c&7;s->minimum=0x10000;s->left=3;}
        else return -1;
    }return 0;
}
static inline int lx_ws_close_valid(const unsigned char *p,size_t n) {
    struct lx_ws_utf8 u={0};unsigned code;
    if(!n)return 1;if(n<2)return 0;
    code=((unsigned)p[0]<<8)|p[1];
    if(!((code>=1000&&code<=1014&&code!=1004&&code!=1005&&code!=1006)||
         (code>=3000&&code<=4999)))return 0;
    return !lx_ws_utf8_feed(&u,p+2,n-2)&&!u.left;
}

/* Allocation-free incremental server stream. Callback payload is transient.
 * Control events may appear during fragmented text; total message <= capacity.
 * A failed or closed receiver must not be reused. Callback must not reenter it. */
struct lx_ws_receiver {
    unsigned char head[10],control[125];size_t head_used,frame_used,message_used,capacity;
    unsigned char *message;struct lx_ws_header h;struct lx_ws_utf8 utf8;
    unsigned fragmented,message_opcode,ready,failed,closed;
};
typedef int (*lx_ws_event)(void *,unsigned,const unsigned char *,size_t);
static inline int lx_ws_feed(struct lx_ws_receiver *s,const unsigned char *p,size_t n,
        lx_ws_event event,void *context) {
    if(s->failed||s->closed)return -1;
    while(n) {
        if(!s->ready) {
            int r;if(s->head_used>=sizeof(s->head))goto fail;
            s->head[s->head_used++]=*p++;--n;
            r=lx_ws_server_header(s->head,s->head_used,s->capacity,&s->h);
            if(r<0)goto fail;if(!r)continue;
            if(s->h.opcode<8) {
                if(s->h.opcode==0) {if(!s->fragmented)goto fail;}
                else {
                    if(s->fragmented)goto fail;
                    s->message_opcode=s->h.opcode;s->message_used=0;
                    memset(&s->utf8,0,sizeof(s->utf8));
                }
                if(s->h.payload_bytes>s->capacity-s->message_used)goto fail;
            }
            s->ready=1;s->frame_used=0;
        }
        size_t take=s->h.payload_bytes-s->frame_used;if(take>n)take=n;
        if(s->h.opcode>=8)memcpy(s->control+s->frame_used,p,take);
        else {
            if(s->message_opcode==1&&lx_ws_utf8_feed(&s->utf8,p,take))goto fail;
            memcpy(s->message+s->message_used,p,take);s->message_used+=take;
        }
        p+=take;n-=take;s->frame_used+=take;
        if(s->frame_used<s->h.payload_bytes)continue;
        if(s->h.opcode>=8) {
            if(s->h.opcode==8&&!lx_ws_close_valid(s->control,s->frame_used))goto fail;
            if(event(context,s->h.opcode,s->control,s->frame_used))goto fail;
            if(s->h.opcode==8){s->closed=1;return 0;}
        }else if(s->h.final) {
            if(s->message_opcode==1&&s->utf8.left)goto fail;
            if(event(context,s->message_opcode,s->message,s->message_used))goto fail;
            s->fragmented=0;s->message_used=0;
        }else s->fragmented=1;
        s->ready=0;s->head_used=0;
    }return 0;
fail:s->failed=1;return -1;
}

static inline int lx_ws_equal(const char *a,size_t n,const char *b) {
    size_t i;for(i=0;i<n&&b[i];i++) {
        unsigned c=(unsigned char)a[i];if(c>='A'&&c<='Z')c+=32;
        if(c!=(unsigned char)b[i])return 0;
    }return i==n&&!b[i];
}
static inline int lx_ws_token(const char *a,size_t n,const char *token) {
    size_t start=0;
    while(start<n) {
        size_t end=start;while(end<n&&a[end]!=',')end++;
        size_t right=end;while(start<right&&(a[start]==' '||a[start]=='\t'))start++;
        while(right>start&&(a[right-1]==' '||a[right-1]=='\t'))right--;
        if(lx_ws_equal(a+start,right-start,token))return 1;
        start=end+1;
    }return 0;
}
/* expected_accept = Base64(SHA1(key + RFC6455 GUID)); no negotiated extensions. */
static inline int lx_ws_upgrade_valid(const char *header,size_t n,const char *expected_accept) {
    size_t pos=0;unsigned upgrade=0,connection=0,accept=0;
    if(n<16||n>8192||memcmp(header,"HTTP/1.1 101",12)||
       (header[12]!=' '&&header[12]!='\r'))return 0;
    for(size_t i=0;i<n;i++)if(!header[i]||((unsigned char)header[i]<32&&header[i]!='\r'&&header[i]!='\n'&&header[i]!='\t'))return 0;
    while(pos+1<n&&!(header[pos]=='\r'&&header[pos+1]=='\n'))pos++;
    if(pos+1>=n)return 0;
    for(size_t i=0;i<pos;i++)if(header[i]=='\r'||header[i]=='\n')return 0;
    pos+=2;
    while(pos+1<n) {
        if(header[pos]=='\r'&&header[pos+1]=='\n')return pos+2==n&&upgrade&&connection&&accept==1;
        size_t end=pos;while(end+1<n&&!(header[end]=='\r'&&header[end+1]=='\n'))end++;
        if(end+1>=n)return 0;
        for(size_t i=pos;i<end;i++)if(header[i]=='\r'||header[i]=='\n')return 0;
        size_t colon=pos;while(colon<end&&header[colon]!=':')colon++;
        if(colon==pos||colon==end)return 0;
        for(size_t i=pos;i<colon;i++) {
            unsigned c=(unsigned char)header[i];
            if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||
                 strchr("!#$%&'*+-.^_`|~",c)))return 0;
        }
        size_t start=colon+1,right=end;
        while(start<right&&(header[start]==' '||header[start]=='\t'))start++;
        while(right>start&&(header[right-1]==' '||header[right-1]=='\t'))right--;
        if(lx_ws_equal(header+pos,colon-pos,"upgrade")) {
            if(++upgrade!=1||!lx_ws_equal(header+start,right-start,"websocket"))return 0;
        }
        else if(lx_ws_equal(header+pos,colon-pos,"connection"))connection|=lx_ws_token(header+start,right-start,"upgrade");
        else if(lx_ws_equal(header+pos,colon-pos,"sec-websocket-accept")) {
            if(++accept!=1||right-start!=strlen(expected_accept)||memcmp(header+start,expected_accept,right-start))return 0;
        }else if(lx_ws_equal(header+pos,colon-pos,"sec-websocket-extensions")||
                 lx_ws_equal(header+pos,colon-pos,"sec-websocket-protocol"))return 0;
        pos=end+2;
    }return 0;
}
#endif
