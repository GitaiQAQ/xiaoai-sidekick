#define _GNU_SOURCE
#include <dlfcn.h>
#include <gnu/libc-version.h>
#include <stdio.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/prctl.h>
#include <curl/curl.h>
#include <poll.h>
#include <time.h>
#include <stdint.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "native_websocket.h"
#include "native_websocket_test.h"

static int ws_frame_self_test(void) {
    struct lx_ws_header h;
    unsigned char out[32],mask[]={0x37,0xfa,0x21,0x3d};
    const unsigned char expected[]={0x81,0x85,0x37,0xfa,0x21,0x3d,0x7f,0x9f,0x4d,0x51,0x58};
    const unsigned char server[]={0x81,5,'H','e','l','l','o'};
    const unsigned char masked[]={0x81,0x80},rsv[]={0xc1,0},reserved[]={0x83,0};
    const unsigned char fragmented_ping[]={0x09,0},invalid_close[]={0x88,1};
    const unsigned char nonminimal[]={0x81,126,0,125};
    const unsigned char boundary[]={0x82,127,0,0,0,0,0,1,0,0};
    const unsigned char too_big[]={0x82,127,0,0,0,0,0,1,0,1};
    int pass=lx_ws_client_frame(out,sizeof(out),1,(const unsigned char *)"Hello",5,mask)==sizeof(expected)&&
        !memcmp(out,expected,sizeof(expected))&&
        !lx_ws_client_frame(out,10,1,(const unsigned char *)"Hello",5,mask)&&
        lx_ws_server_header(server,1,65536,&h)==0&&
        lx_ws_server_header(server,sizeof(server),65536,&h)==1&&h.payload_bytes==5&&h.opcode==1&&h.final&&
        lx_ws_server_header(masked,sizeof(masked),65536,&h)==-1&&
        lx_ws_server_header(rsv,sizeof(rsv),65536,&h)==-1&&
        lx_ws_server_header(reserved,sizeof(reserved),65536,&h)==-1&&
        lx_ws_server_header(fragmented_ping,sizeof(fragmented_ping),65536,&h)==-1&&
        lx_ws_server_header(invalid_close,sizeof(invalid_close),65536,&h)==-1&&
        lx_ws_server_header(nonminimal,sizeof(nonminimal),65536,&h)==-1&&
        lx_ws_server_header(boundary,9,65536,&h)==0&&
        lx_ws_server_header(boundary,10,65536,&h)==1&&h.payload_bytes==65536&&
        lx_ws_server_header(too_big,10,65536,&h)==-1;
    printf("ws_rfc_frame_self_test=%s network_connections=0\n",pass?"pass":"fail");return !pass;
}

struct curl_api {
    CURLcode (*global_init)(long);
    void (*global_cleanup)(void);
    CURL *(*init)(void);
    CURLcode (*setopt)(CURL *,CURLoption,...);
    CURLcode (*perform)(CURL *);
    CURLcode (*getinfo)(CURL *,CURLINFO,...);
    void (*cleanup)(CURL *);
    CURLcode (*send)(CURL *,const void *,size_t,size_t *);
    CURLcode (*recv)(CURL *,void *,size_t,size_t *);
};

typedef unsigned char *(*lx_sha1)(const unsigned char *,size_t,unsigned char *);
typedef int (*lx_base64)(unsigned char *,const unsigned char *,int);
static int ws_accept(lx_sha1 sha,lx_base64 encode,const char *key,char output[29]) {
    char combined[61];unsigned char digest[20];
    if(strlen(key)!=24)return 1;
    memcpy(combined,key,24);memcpy(combined+24,"258EAFA5-E914-47DA-95CA-C5AB0DC85B11",36);
    if(!sha((const unsigned char *)combined,60,digest)||encode((unsigned char *)output,digest,20)!=28)return 1;
    output[28]=0;return 0;
}
static int ws_crypto_self_test(void) {
    void *crypto=dlopen("libcrypto.so.1.0.0",RTLD_NOW|RTLD_LOCAL);char result[29]={0};int fail=1;
    if(crypto) {
        lx_sha1 sha=(lx_sha1)dlsym(crypto,"SHA1");lx_base64 encode=(lx_base64)dlsym(crypto,"EVP_EncodeBlock");
        if(sha&&encode)fail=ws_accept(sha,encode,"dGhlIHNhbXBsZSBub25jZQ==",result)||
            strcmp(result,"s3pPLMBiTxaQ9kYGzzhZRbK+xOo=");
        dlclose(crypto);
    }
    printf("ws_factory_crypto_rfc_accept=%s network_connections=0\n",fail?"fail":"pass");return !!fail;
}

static int64_t now_ms(void) {
    struct timespec t;
    if(clock_gettime(CLOCK_MONOTONIC,&t))return -1;
    return (int64_t)t.tv_sec*1000+t.tv_nsec/1000000;
}

struct ws_connection {
    struct curl_api *a;CURL *c;curl_socket_t socket;int64_t deadline;
    int (*random_bytes)(unsigned char *,int);unsigned messages,close_code;
};
static int ws_wait(struct ws_connection *w,int sending) {
    int64_t now=now_ms(),remaining=w->deadline-now;
    struct pollfd p={w->socket,sending?POLLOUT:POLLIN,0};
    if(now<0||remaining<=0)return 1;
    return poll(&p,1,remaining>50?50:(int)remaining)<0;
}
static int ws_send_all(struct ws_connection *w,const unsigned char *p,size_t n) {
    while(n) {
        size_t sent=0;CURLcode code;
        int64_t now=now_ms();if(now<0||now>=w->deadline)return 1;
        code=w->a->send(w->c,p,n,&sent);
        if(code==CURLE_AGAIN){if(ws_wait(w,1))return 1;continue;}
        if(code||!sent)return 1;p+=sent;n-=sent;
    }return 0;
}
static int ws_send_control(struct ws_connection *w,unsigned op,const unsigned char *p,size_t n) {
    unsigned char mask[4],frame[131];size_t bytes;
    if(w->random_bytes(mask,4)!=1)return 1;
    bytes=lx_ws_client_frame(frame,sizeof(frame),op,p,n,mask);
    return !bytes||ws_send_all(w,frame,bytes);
}
static int ws_close_event(void *context,unsigned op,const unsigned char *p,size_t n) {
    struct ws_connection *w=context;
    if(op==9)return ws_send_control(w,10,p,n);
    if(op==8)w->close_code=n>=2?((unsigned)p[0]<<8)|p[1]:1005;
    else if(op==1||op==2)w->messages++;
    return 0;
}

static int ws_send_text(struct ws_connection *w,const unsigned char *p,size_t n) {
    unsigned char mask[4],frame[1024];size_t bytes;
    if(n>900||w->random_bytes(mask,4)!=1)return 1;
    bytes=lx_ws_client_frame(frame,sizeof(frame),1,p,n,mask);
    return !bytes||ws_send_all(w,frame,bytes);
}
static int ws_contains(const unsigned char *p,size_t n,const char *needle) {
    size_t m=strlen(needle);
    if(!m||n<m)return 0;
    for(size_t i=0;i+m<=n;i++)if(!memcmp(p+i,needle,m))return 1;
    return 0;
}
struct ws_session_state {
    struct ws_connection *w;int created,closed,error_event,messages;
};
static int ws_session_event(void *context,unsigned op,const unsigned char *p,size_t n) {
    struct ws_session_state *s=context;
    if(op==9)return ws_send_control(s->w,10,p,n);
    if(op==8){s->w->close_code=n>=2?((unsigned)p[0]<<8)|p[1]:1005;return 0;}
    if(op!=1)return 0;
    s->messages++;
    if(ws_contains(p,n,"session.created"))s->created=1;
    if(ws_contains(p,n,"session.closed"))s->closed=1;
    if(ws_contains(p,n,"\"type\":\"error\""))s->error_event=1;
    return 0;
}
/* CONNECT_ONLY=1 is available in factory curl; its newer WS API is not.
   tls=0 is a loopback-only test path used against --ws-test-server.
   session=1 additionally sends session.create/session.close with no audio. */
static int ws_transport_check(struct curl_api *a,const char *api_key,
                              const char *url,const char *path,const char *host,int tls,int session) {
    CURL *c=a->init();void *crypto=NULL;int result=1;
    curl_socket_t socket=CURL_SOCKET_BAD;long verify=-1;
    unsigned char nonce[16];char key[25]={0},request[5400]={0},header[8193]={0},expected[29]={0};
    size_t sent=0,used=0;int length,status=0;int64_t deadline;
    int (*random_bytes)(unsigned char *,int);
    int (*encode)(unsigned char *,const unsigned char *,int);
    lx_sha1 sha;struct ws_connection w={.a=a,.c=c};
    if(!c)return 1;
    crypto=dlopen("libcrypto.so.1.0.0",RTLD_NOW|RTLD_LOCAL);if(!crypto)goto done;
    random_bytes=(int (*)(unsigned char *,int))dlsym(crypto,"RAND_bytes");
    encode=(int (*)(unsigned char *,const unsigned char *,int))dlsym(crypto,"EVP_EncodeBlock");
    sha=(lx_sha1)dlsym(crypto,"SHA1");
    if(!random_bytes||!encode||!sha||random_bytes(nonce,sizeof(nonce))!=1||
       encode((unsigned char *)key,nonce,sizeof(nonce))!=24||ws_accept(sha,encode,key,expected))goto done;
    if(a->setopt(c,CURLOPT_URL,url)||a->setopt(c,CURLOPT_CONNECT_ONLY,1L)||
       a->setopt(c,CURLOPT_PROXY,"")||a->setopt(c,CURLOPT_CONNECTTIMEOUT_MS,5000L)||
       a->setopt(c,CURLOPT_TIMEOUT_MS,10000L)||a->setopt(c,CURLOPT_NOSIGNAL,1L))goto done;
    if(tls&&(a->setopt(c,CURLOPT_SSL_VERIFYPEER,1L)||a->setopt(c,CURLOPT_SSL_VERIFYHOST,2L)))goto done;
    if(a->perform(c)||a->getinfo(c,CURLINFO_ACTIVESOCKET,&socket)||socket==CURL_SOCKET_BAD)goto done;
    if(tls&&(a->getinfo(c,CURLINFO_SSL_VERIFYRESULT,&verify)||verify))goto done;
    length=snprintf(request,sizeof(request),
        "GET %s HTTP/1.1\r\nHost: %s\r\n"
        "Upgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Version: 13\r\n"
        "Sec-WebSocket-Key: %s\r\n%s%s%s\r\n",path,host,key,
        api_key?"X-Api-Key: ":"",api_key?api_key:"",api_key?"\r\n":"");
    if(length<0||(size_t)length>=sizeof(request))goto done;
    deadline=now_ms();if(deadline<0)goto done;deadline+=10000;
    while(sent<(size_t)length || (used<sizeof(header)-1 && !strstr(header,"\r\n\r\n"))) {
        size_t n=0;CURLcode code;
        int sending=sent<(size_t)length;
        int64_t remaining=deadline-now_ms();if(remaining<=0)goto done;
        if(sending)code=a->send(c,request+sent,(size_t)length-sent,&n);
        else code=a->recv(c,header+used,1,&n);
        if(code==CURLE_AGAIN) {
            /* OpenSSL may need either direction; bounded recheck avoids spinning. */
            struct pollfd p={socket,sending?POLLOUT:POLLIN,0};
            int r=poll(&p,1,remaining>50?50:(int)remaining);
            if(r<0)goto done;continue;
        }
        if(code||!n)goto done;
        if(sending)sent+=n;else {used+=n;header[used]=0;}
    }
    if(!strstr(header,"\r\n\r\n")||sscanf(header,"HTTP/1.1 %d",&status)!=1)goto done;
    printf("ws_transport=%s http_status=%d header_bytes=%zu auth_headers_sent=%d\n",
        tls?"tls":"loopback-plain",status,used,api_key!=NULL);
    if(tls)printf("ws_raw_tls_verify=%ld\n",verify);
    /* This probe expects rejection, not a session. No secret or audio is sent. */
    if(!api_key)result=!(status==400||status==401||status==403);
    else {
        static unsigned char message[65536];unsigned char packet[4096],close[]={3,232};
        struct lx_ws_receiver receiver={.message=message,.capacity=sizeof(message)};
        int valid=status==101&&lx_ws_upgrade_valid(header,used,expected);
        printf("ws_authenticated_upgrade_valid=%d\n",valid);if(!valid)goto done;
        w.socket=socket;w.random_bytes=random_bytes;
        if(session) {
            struct ws_session_state st={.w=&w};
            static const char create[]=
                "{\"type\":\"session.create\",\"session\":{\"model\":\"1.2.6.1\",\"audio\":{"
                "\"input\":{\"format\":{\"type\":\"pcm\",\"rate\":16000}},"
                "\"output\":{\"format\":{\"type\":\"pcm_s16le\",\"rate\":24000},"
                "\"voice\":\"zh_female_vv_uranus_bigtts\"}}}}";
            static const char shutdown[]="{\"type\":\"session.close\"}";
            int phase;
            w.deadline=now_ms()+10000;
            if(ws_send_text(&w,(const unsigned char *)create,sizeof(create)-1))goto done;
            for(phase=0;phase<2;phase++) {
                int64_t limit=now_ms()+(phase?5000:10000);
                while(phase?!st.closed:!(st.created||st.error_event)) {
                    size_t n=0;CURLcode code;
                    int64_t now=now_ms();if(now<0||now>=limit||now>=w.deadline)goto done;
                    code=a->recv(c,packet,sizeof(packet),&n);
                    if(code==CURLE_AGAIN){if(ws_wait(&w,0))goto done;continue;}
                    if(code||!n||lx_ws_feed(&receiver,packet,n,ws_session_event,&st))goto done;
                }
                if(phase==0) {
                    printf("session_created=%d error_event=%d\n",st.created,st.error_event);
                    if(!st.created)goto done;
                    if(ws_send_text(&w,(const unsigned char *)shutdown,sizeof(shutdown)-1))goto done;
                }
            }
            printf("session_closed=%d messages=%u\n",st.closed,st.messages);
            if(!st.closed){result=1;goto done;}
            w.deadline=now_ms()+3000;
            if(ws_send_control(&w,8,close,sizeof(close)))goto done;
            while(!receiver.closed) {
                size_t n=0;CURLcode code;
                int64_t now=now_ms();if(now<0||now>=w.deadline)goto done;
                code=a->recv(c,packet,sizeof(packet),&n);
                if(code==CURLE_AGAIN){if(ws_wait(&w,0))goto done;continue;}
                if(code||!n||lx_ws_feed(&receiver,packet,n,ws_close_event,&w))goto done;
            }
            printf("ws_close_received=1 close_code=%u\n",w.close_code);
            result=!(w.close_code==1000||w.close_code==1001);
            goto done;
        }
        w.deadline=now_ms()+3000;
        if(ws_send_control(&w,8,close,sizeof(close)))goto done;
        while(!receiver.closed) {
            size_t n=0;CURLcode code;
            int64_t now=now_ms();if(now<0||now>=w.deadline)goto done;
            code=a->recv(c,packet,sizeof(packet),&n);
            if(code==CURLE_AGAIN){if(ws_wait(&w,0))goto done;continue;}
            if(code||!n||lx_ws_feed(&receiver,packet,n,ws_close_event,&w))goto done;
        }
        printf("ws_close_received=1 close_code=%u discarded_messages=%u session_create_sent=0\n",w.close_code,w.messages);
        result=!(w.close_code==1000||w.close_code==1001);
    }
done:
    {volatile char *wipe=request;for(size_t i=0;i<sizeof(request);i++)wipe[i]=0;}
    a->cleanup(c);if(crypto)dlclose(crypto);
    printf("ws_operation=%s result=%s session_create_sent=%d audio_uploaded=0\n",
        api_key?(session?"session-create-close":"auth-upgrade-close"):"unauth-transport",
        result?"fail":"pass",api_key&&session?1:0);
    return result;
}

static int tls_check(struct curl_api *a,const char *name,const char *url) {
    CURL *c=a->init();
    CURLcode code;
    long http=0,verify=-1;
    double handshake=0;
    char error[CURL_ERROR_SIZE]={0};
    if(!c) return 1;
    if(a->setopt(c,CURLOPT_URL,url) || a->setopt(c,CURLOPT_NOBODY,1L) ||
       a->setopt(c,CURLOPT_SSL_VERIFYPEER,1L) || a->setopt(c,CURLOPT_SSL_VERIFYHOST,2L) ||
       a->setopt(c,CURLOPT_CONNECTTIMEOUT_MS,5000L) || a->setopt(c,CURLOPT_TIMEOUT_MS,10000L) ||
       a->setopt(c,CURLOPT_NOSIGNAL,1L) || a->setopt(c,CURLOPT_FOLLOWLOCATION,0L) ||
       a->setopt(c,CURLOPT_PROXY,"") || a->setopt(c,CURLOPT_ERRORBUFFER,error)) {
        a->cleanup(c); return 1;
    }
    code=a->perform(c);
    if(a->getinfo(c,CURLINFO_RESPONSE_CODE,&http) ||
       a->getinfo(c,CURLINFO_SSL_VERIFYRESULT,&verify) ||
       a->getinfo(c,CURLINFO_APPCONNECT_TIME,&handshake)) {
        a->cleanup(c); return 1;
    }
    printf("tls_probe domain=%s curl_code=%d http_status=%ld verify_result=%ld handshake_ms=%.0f\n",
        name,(int)code,http,verify,handshake*1000);
    if(code) fprintf(stderr,"TLS error: %s\n",error);
    a->cleanup(c);
    return code!=CURLE_OK || verify!=0 || http==0;
}

/* Loopback-only scripted peer used to exercise the upgrade/close control flow
   without any cloud session. Scenarios: good, badaccept, status400, silent. */
static int ws_test_server(int port,const char *scenario) {
    void *crypto=dlopen("libcrypto.so.1.0.0",RTLD_NOW|RTLD_LOCAL);
    lx_sha1 sha;lx_base64 encode;
    char key[25]={0},accept_value[29]={0},request[4097]={0},response[512]={0};
    unsigned char packet[1024];
    ssize_t n;size_t used=0;int fd=-1,client=-1,result=0,send_close=0;
    struct sockaddr_in addr;struct timeval tv={12,0};
    if(!crypto)return 1;
    sha=(lx_sha1)dlsym(crypto,"SHA1");
    encode=(lx_base64)dlsym(crypto,"EVP_EncodeBlock");
    if(!sha||!encode)goto done;
    fd=socket(AF_INET,SOCK_STREAM,0);if(fd<0)goto done;
    {int on=1;setsockopt(fd,SOL_SOCKET,SO_REUSEADDR,&on,sizeof(on));}
    memset(&addr,0,sizeof(addr));
    addr.sin_family=AF_INET;addr.sin_port=htons((unsigned short)port);
    addr.sin_addr.s_addr=htonl(0x7f000001);
    if(bind(fd,(struct sockaddr *)&addr,sizeof(addr))||listen(fd,1))goto done;
    printf("ws_test_server listening=127.0.0.1:%d scenario=%s\n",port,scenario);
    fflush(stdout);
    client=accept(fd,NULL,NULL);if(client<0)goto done;
    setsockopt(client,SOL_SOCKET,SO_RCVTIMEO,&tv,sizeof(tv));
    while(used<sizeof(request)-1&&!strstr(request,"\r\n\r\n")) {
        n=recv(client,request+used,sizeof(request)-1-used,0);
        if(n<=0)goto done;
        used+=(size_t)n;request[used]=0;
    }
    {const char *p=strstr(request,"Sec-WebSocket-Key: ");
     if(!p)goto done;
     p+=19;
     if(strlen(p)<24)goto done;
     memcpy(key,p,24);key[24]=0;
     if(ws_accept(sha,encode,key,accept_value))goto done;}
    if(!strcmp(scenario,"status400"))
        snprintf(response,sizeof(response),"HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\n\r\n");
    else if(!strcmp(scenario,"badaccept"))
        snprintf(response,sizeof(response),"HTTP/1.1 101 Switching Protocols\r\n"
            "Upgrade: websocket\r\nConnection: Upgrade\r\n"
            "Sec-WebSocket-Accept: AAAAAAAAAAAAAAAAAAAAAAAAAAAA\r\n\r\n");
    else {
        snprintf(response,sizeof(response),"HTTP/1.1 101 Switching Protocols\r\n"
            "Upgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: %s\r\n\r\n",accept_value);
        send_close=!strcmp(scenario,"good");
    }
    if(send(client,response,strlen(response),0)!=(ssize_t)strlen(response))goto done;
    printf("ws_test_server response_sent=1 bytes=%zu\n",strlen(response));
    if(!strcmp(scenario,"silent")) { sleep(5); goto done; }
    if(!send_close)goto done;
    /* Wait for the client Close, then finish the closing handshake with 1000. */
    n=recv(client,packet,sizeof(packet),0);
    if(n<2||(packet[0]&0x0f)!=8){printf("ws_test_server client_close_valid=0\n");result=1;goto done;}
    printf("ws_test_server client_close_valid=1\n");
    {unsigned char close_frame[]={0x88,0x02,0x03,0xe8};
     if(send(client,close_frame,sizeof(close_frame),0)!=(ssize_t)sizeof(close_frame))result=1;}
done:
    if(client>=0)close(client);
    if(fd>=0)close(fd);
    if(crypto)dlclose(crypto);
    printf("ws_test_server result=%s\n",result?"fail":"pass");
    return result;
}

/* Default: versions/symbols only. --tls: two fixed, unauthenticated HEAD checks. */
int main(int argc,char **argv) {    const char *symbols[]={"curl_easy_init","curl_easy_setopt","curl_easy_perform",
        "curl_easy_send","curl_easy_recv","curl_easy_getinfo","curl_easy_cleanup"};
    void *curl;
    const char *(*version)(void);
    struct rusage usage;
    struct curl_api a;
    int ws=argc==2&&!strcmp(argv[1],"--ws-probe");
    int auth=argc==2&&!strcmp(argv[1],"--ws-stdin");
    int session=argc==2&&!strcmp(argv[1],"--ws-session");
    int local=argc==3&&!strcmp(argv[1],"--ws-local");
    int server=argc==4&&!strcmp(argv[1],"--ws-test-server");
    int check=argc==2&&!strcmp(argv[1],"--ws-check-key");char voice_key[4097]={0};
    int network=(argc==2 && (!strcmp(argv[1],"--tls")||ws||auth||session))||local,result=0;
    size_t i;
    if(prctl(PR_SET_DUMPABLE,0L,0L,0L,0L) || prctl(PR_GET_DUMPABLE,0L,0L,0L,0L)!=0) {
        fputs("unable to disable probe core dumps\n",stderr); return 1;
    }
    puts("probe_dumpable=0");
    if(argc==2&&!strcmp(argv[1],"--ws-self-test")) {
        int failed=ws_frame_self_test();failed|=lx_ws_stream_self_test();failed|=ws_crypto_self_test();return failed;
    }
    if(server) {
        int port=atoi(argv[2]);
        if(port<1||port>65535){fputs("bad port\n",stderr);return 2;}
        signal(SIGPIPE,SIG_IGN);
        return ws_test_server(port,argv[3]);
    }
    if(auth||session||check) {
        size_t n=fread(voice_key,1,sizeof(voice_key),stdin);
        if(!n||n>4096||ferror(stdin))return 2;
        for(size_t k=0;k<n;k++)if((unsigned char)voice_key[k]<33||(unsigned char)voice_key[k]>126)return 2;
        if(check){puts("ws_stdin_key_valid=1 network_connections=0");return 0;}
    }
    if(argc!=1 && !network) { fputs("usage: native-library-probe [--tls|--ws-probe|--ws-stdin|--ws-session|--ws-check-key|--ws-self-test|--ws-local <port>|--ws-test-server <port> <scenario>]\n",stderr); return 2; }
    printf("libc=%s pointer_bits=%zu\n",gnu_get_libc_version(),sizeof(void*)*8);
    curl=dlopen("libcurl.so.4",RTLD_NOW|RTLD_LOCAL);
    if(!curl) { fprintf(stderr,"libcurl load failed: %s\n",dlerror()); return 1; }
    version=(const char *(*)(void))dlsym(curl,"curl_version");
    if(!version) { dlclose(curl); return 1; }
    printf("curl=%s\n",version());
    for(i=0;i<sizeof(symbols)/sizeof(symbols[0]);++i) {
        int available=dlsym(curl,symbols[i])!=NULL;
        printf("symbol=%s available=%d\n",symbols[i],available);
        if(!available) { dlclose(curl); return 1; }
    }
    if(network) {
        a.global_init=(CURLcode (*)(long))dlsym(curl,"curl_global_init");
        a.global_cleanup=(void (*)(void))dlsym(curl,"curl_global_cleanup");
        a.init=(CURL *(*)(void))dlsym(curl,"curl_easy_init");
        a.setopt=(CURLcode (*)(CURL *,CURLoption,...))dlsym(curl,"curl_easy_setopt");
        a.perform=(CURLcode (*)(CURL *))dlsym(curl,"curl_easy_perform");
        a.getinfo=(CURLcode (*)(CURL *,CURLINFO,...))dlsym(curl,"curl_easy_getinfo");
        a.cleanup=(void (*)(CURL *))dlsym(curl,"curl_easy_cleanup");
        a.send=(CURLcode (*)(CURL *,const void *,size_t,size_t *))dlsym(curl,"curl_easy_send");
        a.recv=(CURLcode (*)(CURL *,void *,size_t,size_t *))dlsym(curl,"curl_easy_recv");
        if(!a.global_init || !a.global_cleanup || !a.init || !a.setopt ||
           !a.perform || !a.getinfo || !a.cleanup || !a.send || !a.recv || a.global_init(CURL_GLOBAL_DEFAULT)) {
            dlclose(curl); return 1;
        }
        /* Fixed public HEAD endpoints: no key, POST body, model request, or audio.
           --ws-local talks only to the loopback test peer and sends a dummy key. */
        if(ws||auth||session)result=ws_transport_check(&a,(auth||session)?voice_key:NULL,
            "https://openspeech.bytedance.com/api/v3/duplex/realtime/dialogue",
            "/api/v3/duplex/realtime/dialogue","openspeech.bytedance.com",1,session);
        else if(local) {
            char url[64],host[32];
            snprintf(url,sizeof(url),"http://127.0.0.1:%d/api/v3/duplex/realtime/dialogue",atoi(argv[2]));
            snprintf(host,sizeof(host),"127.0.0.1:%d",atoi(argv[2]));
            result=ws_transport_check(&a,"loopback-test-key",url,
                "/api/v3/duplex/realtime/dialogue",host,0,0);
        }
        else {
            result=tls_check(&a,"ark","https://ark.cn-beijing.volces.com/api/v3/chat/completions");
            result|=tls_check(&a,"voice","https://openspeech.bytedance.com/");
        }
        a.global_cleanup();
    }
    {volatile char *wipe=voice_key;for(size_t k=0;k<sizeof(voice_key);k++)wipe[k]=0;}
    if(!getrusage(RUSAGE_SELF,&usage)) printf("probe_peak_rss_kib=%ld\n",usage.ru_maxrss);
    if(dlclose(curl)) return 1;
    if(network) puts(result?"tls_transport_probe=fail":"tls_transport_probe=pass authenticated_model_calls=0 audio_uploaded=0");
    else puts("library_probe=pass network_connections=0 audio_devices_opened=0");
    return result;
}
