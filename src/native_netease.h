/* Native account provider. Protocol reference: api-enhanced util/crypto.js
   eapi (AES-128-ECB + MD5), independently implemented with factory OpenSSL.
   Only account-entitled streams are requested; no alternate/unlock sources. */
#ifndef LX06_NETEASE_H
#define LX06_NETEASE_H
#include <stdint.h>
#include <curl/curl.h>
#include <sys/stat.h>
extern int __fxstat64(int,int,struct stat64 *);
static struct {
 void *curl_lib,*crypto;
 CURL *(*init)(void);CURLcode (*set)(CURL *,CURLoption,...);CURLcode (*perform)(CURL *);void (*cleanup)(CURL *);
 CURLcode (*getinfo)(CURL *,CURLINFO,...);struct curl_slist *(*append)(struct curl_slist *,const char *);void (*free_headers)(struct curl_slist *);
 void *(*ctx_new)(void);void (*ctx_free)(void *);const void *(*aes)(void);
 int (*enc_init)(void *,const void *,void *,const unsigned char *,const unsigned char *);
 int (*enc_update)(void *,unsigned char *,int *,const unsigned char *,int);int (*enc_final)(void *,unsigned char *,int *);
 unsigned char *(*md5)(const unsigned char *,size_t,unsigned char *);
 int (*random)(unsigned char *,int);
 char music_u[4097],csrf[257],nmtid[513],qr[257],nickname[257],uid[40],error[64];
 int qr_code,authenticated,vip_type,last_api_code;int64_t qr_until,last_poll;
} nc;
#define NC_SESSION "/data/lx06-agent/netease.session"
static char nc_pending_url[901],nc_pending_ticket[33];static int64_t nc_pending_until;
static struct lx_music_track nc_pending_track;
static char nc_pending_urls[20][901],nc_pending_playlist[256];static const char *nc_pending_urlptr[20];static struct lx_music_track nc_pending_queue[20];static int nc_pending_count;
static int nc_defer_play;
static const char *nc_string(void *o,const char *k){void *v=NULL;if(!o||!mj.get(o,k,&v)||mj.type(v)!=6)return NULL;return mj.string(v);}
static void nc_add_s(void *o,const char *k,const char *v){mj.add(o,k,mj.new_string(v));}
static void nc_add_i(void *o,const char *k,int v){mj.add(o,k,mj.new_int(v));}
static int nc_value_valid(const char *s,size_t cap){if(!s||strlen(s)>=cap)return 0;for(;*s;s++)if((unsigned char)*s<33||(unsigned char)*s>126||*s==';')return 0;return 1;}
static void nc_load(void){
 char buf[6000];int fd=open(NC_SESSION,O_RDONLY|O_NOFOLLOW);if(fd<0)return;struct stat64 st;
 if(__fxstat64(0,fd,&st)||!S_ISREG(st.st_mode)||st.st_size>=(off_t)sizeof(buf)||(st.st_mode&077)){close(fd);return;}
 ssize_t n=read(fd,buf,sizeof(buf)-1);close(fd);if(n<=0)return;buf[n]=0;void *o=mj.parse(buf);if(!o)return;
 const char *u=nc_string(o,"music_u"),*c=nc_string(o,"csrf"),*m=nc_string(o,"nmtid");
 if(nc_value_valid(u,sizeof(nc.music_u))&&*u){snprintf(nc.music_u,sizeof(nc.music_u),"%s",u);if(nc_value_valid(c,sizeof(nc.csrf)))snprintf(nc.csrf,sizeof(nc.csrf),"%s",c);if(nc_value_valid(m,sizeof(nc.nmtid)))snprintf(nc.nmtid,sizeof(nc.nmtid),"%s",m);}
 mj.put(o);memset(buf,0,sizeof(buf));
}
static int nc_save(void){
 void *o=mj.object();nc_add_s(o,"music_u",nc.music_u);nc_add_s(o,"csrf",nc.csrf);nc_add_s(o,"nmtid",nc.nmtid);
 const char *s=mj.encode(o);int fd=open(NC_SESSION ".tmp",O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW,0600);int bad=fd<0;
 if(fd>=0){size_t n=strlen(s),off=0;while(off<n){ssize_t k=write(fd,s+off,n-off);if(k<=0){bad=1;break;}off+=(size_t)k;}if(fsync(fd))bad=1;close(fd);if(!bad&&rename(NC_SESSION ".tmp",NC_SESSION))bad=1;if(bad)unlink(NC_SESSION ".tmp");}
 mj.put(o);return bad;
}
static int nc_init(void){
 nc.curl_lib=dlopen("libcurl.so.4",RTLD_NOW|RTLD_LOCAL);nc.crypto=dlopen("libcrypto.so.1.0.0",RTLD_NOW|RTLD_LOCAL);if(!nc.curl_lib||!nc.crypto)return 1;
#define N(f,l,s) *(void **)(&nc.f)=dlsym(l,s);if(!nc.f)return 1
 N(init,nc.curl_lib,"curl_easy_init");N(set,nc.curl_lib,"curl_easy_setopt");N(perform,nc.curl_lib,"curl_easy_perform");N(getinfo,nc.curl_lib,"curl_easy_getinfo");N(cleanup,nc.curl_lib,"curl_easy_cleanup");N(append,nc.curl_lib,"curl_slist_append");N(free_headers,nc.curl_lib,"curl_slist_free_all");
 N(ctx_new,nc.crypto,"EVP_CIPHER_CTX_new");N(ctx_free,nc.crypto,"EVP_CIPHER_CTX_free");N(aes,nc.crypto,"EVP_aes_128_ecb");N(enc_init,nc.crypto,"EVP_EncryptInit_ex");N(enc_update,nc.crypto,"EVP_EncryptUpdate");N(enc_final,nc.crypto,"EVP_EncryptFinal_ex");N(md5,nc.crypto,"MD5");N(random,nc.crypto,"RAND_bytes");
#undef N
 CURLcode (*global)(long)=(CURLcode (*)(long))dlsym(nc.curl_lib,"curl_global_init");if(!global||global(CURL_GLOBAL_DEFAULT))return 1;
 nc_load();return 0;
}
static int nc_encrypt(const char *path,const char *json,char *out,size_t cap){
 size_t a=strlen(path),b=strlen(json);if(a+b>12000)return 1;
 char *plain=malloc(a+b+100),*sign=malloc(a+b+40);unsigned char digest[16],*cipher=malloc(a+b+128);if(!plain||!sign||!cipher){free(plain);free(sign);free(cipher);return 1;}
 int n=snprintf(sign,a+b+40,"nobody%suse%smd5forencrypt",path,json);nc.md5((const unsigned char *)sign,(size_t)n,digest);
 char hex[33];for(unsigned i=0;i<16;i++)snprintf(hex+i*2,3,"%02x",digest[i]);
 n=snprintf(plain,a+b+100,"%s-36cd479b6b5-%s-36cd479b6b5-%s",path,json,hex);
 void *ctx=nc.ctx_new();int count=0,tail=0,bad=!ctx;
 if(!bad)bad=nc.enc_init(ctx,nc.aes(),NULL,(const unsigned char *)"e82ckenh8dichen8",NULL)!=1||nc.enc_update(ctx,cipher,&count,(const unsigned char *)plain,n)!=1||nc.enc_final(ctx,cipher+count,&tail)!=1;
 if(!bad){size_t len=(size_t)(count+tail);if(7+len*2+1>cap)bad=1;else{memcpy(out,"params=",7);for(size_t i=0;i<len;i++)snprintf(out+7+i*2,3,"%02X",cipher[i]);}}
 if(ctx)nc.ctx_free(ctx);memset(plain,0,a+b+100);memset(sign,0,a+b+40);memset(cipher,0,a+b+128);free(plain);free(sign);free(cipher);return bad;
}
struct nc_response {char *data;size_t used,cap;int overflow;};
/* Account playlist listings arrive in a provider variant that inlines each
   playlist's tracks and reaches several hundred KiB; keep a hard 1 MiB ceiling. */
#define NC_RESPONSE_CAP (1024*1024)
static size_t nc_receive(char *p,size_t a,size_t b,void *context){struct nc_response *r=context;if(b&&a>SIZE_MAX/b)return 0;size_t n=a*b;if(n>r->cap-1-r->used){r->overflow=1;return 0;}memcpy(r->data+r->used,p,n);r->used+=n;r->data[r->used]=0;return n;}
static size_t nc_header(char *p,size_t a,size_t b,void *context){
 (void)context;if(b&&a>SIZE_MAX/b)return 0;size_t n=a*b;if(n>8192)return 0;
 if(n>12&&!strncasecmp(p,"Set-Cookie:",11)){
  const char *v=p+11,*end=p+n;while(v<end&&(*v==' '||*v=='\t'))v++;
  const char *eq=memchr(v,'=',(size_t)(end-v));if(eq){const char *last=memchr(eq+1,';',(size_t)(end-eq-1));if(!last)last=end;while(last>eq+1&&(last[-1]=='\r'||last[-1]=='\n'))last--;
   char *target=NULL;size_t cap=0,key=(size_t)(eq-v),len=(size_t)(last-eq-1);
   if(key==7&&!memcmp(v,"MUSIC_U",7)){target=nc.music_u;cap=sizeof(nc.music_u);}else if(key==6&&!memcmp(v,"__csrf",6)){target=nc.csrf;cap=sizeof(nc.csrf);}else if(key==5&&!memcmp(v,"NMTID",5)){target=nc.nmtid;cap=sizeof(nc.nmtid);}
   if(target&&len<cap){int valid=1;for(const char *q=eq+1;q<last;q++)if((unsigned char)*q<33||(unsigned char)*q>126)valid=0;if(valid){memcpy(target,eq+1,len);target[len]=0;}}
  }
 }
 return n;
}
/* Fixed provider host/path, verified TLS, bounded response. Credentials never
   appear in argv, environment, logs, or the browser response. */
static void *nc_request(const char *path,void *data){
 void *header=mj.object();char reqid[48];unsigned char random[4];nc.random(random,4);snprintf(reqid,sizeof(reqid),"%ld_%u",(long)time(NULL),(unsigned)random[0]*256+random[1]);
 nc_add_s(header,"os","pc");nc_add_s(header,"appver","3.1.17.204416");nc_add_s(header,"osver","Microsoft-Windows-10-Professional-build-19045-64bit");nc_add_s(header,"deviceId","lx06-native-console");nc_add_s(header,"channel","netease");nc_add_s(header,"requestId",reqid);nc_add_s(header,"__csrf",nc.csrf);
 if(nc.music_u[0])nc_add_s(header,"MUSIC_U",nc.music_u);if(nc.nmtid[0])nc_add_s(header,"NMTID",nc.nmtid);
 mj.add(data,"header",header);mj.add(data,"e_r",mj.parse("false"));
 char *body=malloc(28000),*response=malloc(NC_RESPONSE_CAP);char url[256],cookie[6000];void *result=NULL;CURL *curl=NULL;struct curl_slist *headers=NULL;
 nc.error[0]=0;nc.last_api_code=0;
 if(!body||!response||strncmp(path,"/api/",5)||nc_encrypt(path,mj.encode(data),body,28000)){snprintf(nc.error,sizeof(nc.error),"request_encoding_failed");goto done;}
 snprintf(url,sizeof(url),"https://interfacepc.music.163.com/eapi/%s",path+5);
 snprintf(cookie,sizeof(cookie),"os=pc; appver=3.1.17.204416; __remember_me=true%s%s%s%s%s%s",nc.music_u[0]?"; MUSIC_U=":"",nc.music_u,nc.csrf[0]?"; __csrf=":"",nc.csrf,nc.nmtid[0]?"; NMTID=":"",nc.nmtid);
 curl=nc.init();if(!curl){snprintf(nc.error,sizeof(nc.error),"network_init_failed");goto done;}
 headers=nc.append(headers,"Content-Type: application/x-www-form-urlencoded");headers=nc.append(headers,"Expect:");
 struct nc_response receive={response,0,NC_RESPONSE_CAP,0};response[0]=0;
#define O(k,v) if(nc.set(curl,k,v)!=CURLE_OK){snprintf(nc.error,sizeof(nc.error),"network_option_failed");goto done;}
 O(CURLOPT_URL,url);O(CURLOPT_POST,1L);O(CURLOPT_POSTFIELDS,body);O(CURLOPT_POSTFIELDSIZE,(long)strlen(body));O(CURLOPT_HTTPHEADER,headers);O(CURLOPT_COOKIE,cookie);
 O(CURLOPT_USERAGENT,"NeteaseMusic/3.1.17.204416 (PC; Windows 10)");O(CURLOPT_REFERER,"https://music.163.com/");O(CURLOPT_PROXY,"");O(CURLOPT_FOLLOWLOCATION,0L);O(CURLOPT_SSL_VERIFYPEER,1L);O(CURLOPT_SSL_VERIFYHOST,2L);O(CURLOPT_NOSIGNAL,1L);O(CURLOPT_CONNECTTIMEOUT_MS,3000L);O(CURLOPT_TIMEOUT_MS,20000L);O(CURLOPT_WRITEFUNCTION,nc_receive);O(CURLOPT_WRITEDATA,&receive);O(CURLOPT_HEADERFUNCTION,nc_header);
#undef O
 CURLcode transfer=nc.perform(curl);long status=0,verify=-1;nc.getinfo(curl,CURLINFO_RESPONSE_CODE,&status);nc.getinfo(curl,CURLINFO_SSL_VERIFYRESULT,&verify);
 if(transfer!=CURLE_OK){if(receive.overflow)snprintf(nc.error,sizeof(nc.error),"response_too_large");else snprintf(nc.error,sizeof(nc.error),"transfer_error_%d",(int)transfer);goto done;}
 if(status!=200||verify){snprintf(nc.error,sizeof(nc.error),"http_or_tls_rejected");goto done;}
 result=mj.parse(response);if(!result||mj.type(result)!=4){if(result)mj.put(result);result=NULL;snprintf(nc.error,sizeof(nc.error),"unexpected_response");goto done;}
 lx_music_number(result,"code",&nc.last_api_code);
done:
 if(curl)nc.cleanup(curl);if(headers)nc.free_headers(headers);if(body){memset(body,0,28000);free(body);}if(response){memset(response,0,NC_RESPONSE_CAP);free(response);}memset(cookie,0,sizeof(cookie));mj.put(data);return result;
}
static int nc_refresh(void){
 if(!nc.music_u[0]){snprintf(nc.error,sizeof(nc.error),"not_logged_in");return 1;}
 void *o=nc_request("/api/w/nuser/account/get",mj.object()),*profile=NULL,*v=NULL;int ok=o&&nc.last_api_code==200&&mj.get(o,"profile",&profile)&&mj.type(profile)==4;
 nc.authenticated=0;
 if(ok){const char *nick=nc_string(profile,"nickname");if(nick)snprintf(nc.nickname,sizeof(nc.nickname),"%s",nick);if(mj.get(profile,"userId",&v))snprintf(nc.uid,sizeof(nc.uid),"%lld",(long long)mj.integer64(v));lx_music_number(profile,"vipType",&nc.vip_type);nc.authenticated=1;}
 if(o)mj.put(o);if(!ok&&!nc.error[0])snprintf(nc.error,sizeof(nc.error),"session_not_confirmed");return !ok;
}
static void *nc_status(void){
 void *o=mj.object();nc_add_i(o,"ok",1);nc_add_s(o,"state",nc.authenticated?"logged_in":nc.music_u[0]?"session_saved":"logged_out");nc_add_i(o,"authenticated",nc.authenticated);nc_add_i(o,"session_saved",nc.music_u[0]!=0);nc_add_s(o,"nickname",nc.nickname);nc_add_s(o,"uid",nc.uid);nc_add_i(o,"vip_type",nc.vip_type);nc_add_s(o,"last_error",nc.error);nc_add_i(o,"api_code",nc.last_api_code);return o;
}
static void *nc_qr_start(void){
 void *d=mj.object();nc_add_i(d,"type",3);void *o=nc_request("/api/login/qrcode/unikey",d),*data=NULL;
 const char *key=o?nc_string(o,"unikey"):NULL;if(!key&&o&&mj.get(o,"data",&data))key=nc_string(data,"unikey");
 void *out=mj.object();if(key&&nc_value_valid(key,sizeof(nc.qr))&&*key){snprintf(nc.qr,sizeof(nc.qr),"%s",key);nc.qr_until=lx_music_ms()+180000;nc.qr_code=801;nc.last_poll=0;
  char url[512];snprintf(url,sizeof(url),"https://music.163.com/login?codekey=%s",nc.qr);nc_add_i(out,"ok",1);nc_add_s(out,"url",url);nc_add_i(out,"expires_in",180);
 }else{nc_add_i(out,"ok",0);nc_add_s(out,"error",nc.error[0]?nc.error:"qr_key_rejected");nc_add_i(out,"api_code",nc.last_api_code);}
 if(o)mj.put(o);return out;
}
static void *nc_qr_check(void){
 if(!nc.qr[0]||lx_music_ms()>nc.qr_until)nc.qr_code=800;
 else if(lx_music_ms()-nc.last_poll>=2500){
  nc.last_poll=lx_music_ms();void *d=mj.object();nc_add_i(d,"type",3);nc_add_s(d,"key",nc.qr);void *o=nc_request("/api/login/qrcode/client/login",d);if(o){nc.qr_code=nc.last_api_code;mj.put(o);}
  if(nc.qr_code==803){if(!nc.music_u[0])snprintf(nc.error,sizeof(nc.error),"login_cookie_missing");else if(nc_save())snprintf(nc.error,sizeof(nc.error),"session_save_failed");else nc_refresh();}
 }
 void *out=nc_status();nc_add_i(out,"qr_code",nc.qr_code);nc_add_i(out,"ok",nc.qr_code!=800&&!nc.error[0]);return out;
}
static void *nc_logout(void){
 nc_pending_ticket[0]=nc_pending_url[0]=0;nc_pending_count=0;
 int bad=unlink(NC_SESSION)&&errno!=ENOENT;memset(nc.music_u,0,sizeof(nc.music_u));memset(nc.csrf,0,sizeof(nc.csrf));nc.nickname[0]=nc.uid[0]=nc.qr[0]=nc.error[0]=0;nc.authenticated=nc.vip_type=0;
 void *o=nc_status();nc_add_i(o,"ok",!bad);return o;
}
static int nc_id(void *obj,const char *field,char out[32]){
 void *v=NULL;if(!mj.get(obj,field,&v))return 0;
 if(mj.type(v)==3){int64_t n=mj.integer64(v);if(n<=0)return 0;snprintf(out,32,"%lld",(long long)n);return 1;}
 if(mj.type(v)!=6||mj.length(v)<1||mj.length(v)>19)return 0;const char *s=mj.string(v);if((size_t)mj.length(v)!=strlen(s))return 0;for(const char *p=s;*p;p++)if(*p<'0'||*p>'9')return 0;snprintf(out,32,"%s",s);return strcmp(out,"0")!=0;
}
static void *nc_search(const char *query){
 void *d=mj.object();nc_add_s(d,"s",query);nc_add_i(d,"type",1);nc_add_i(d,"limit",12);nc_add_i(d,"offset",0);
 void *o=nc_request("/api/search/get",d),*result=NULL,*songs=NULL,*out=mj.object(),*items=mj.array();int ok=o&&nc.last_api_code==200&&mj.get(o,"result",&result)&&mj.get(result,"songs",&songs)&&mj.type(songs)==5;
 if(ok){int n=mj.array_length(songs);if(n>12)n=12;for(int i=0;i<n;i++){
  void *song=mj.index(songs,i),*item=mj.object(),*artists=NULL;char id[32],artist[256]="";const char *title=nc_string(song,"name");
  if(!nc_id(song,"id",id)||!title){mj.put(item);continue;}
  if((mj.get(song,"artists",&artists)||mj.get(song,"ar",&artists))&&mj.type(artists)==5){int a=mj.array_length(artists);if(a>4)a=4;for(int j=0;j<a;j++){const char *s=nc_string(mj.index(artists,j),"name");if(s){size_t off=strlen(artist);snprintf(artist+off,sizeof(artist)-off,"%s%s",off?" / ":"",s);}}}
  nc_add_s(item,"id",id);nc_add_s(item,"title",title);nc_add_s(item,"artist",artist);mj.append(items,item);
 }}
 nc_add_i(out,"ok",ok);if(!ok)nc_add_s(out,"error",nc.error[0]?nc.error:"search_rejected");mj.add(out,"items",items);if(o)mj.put(o);return out;
}
/* Bounded chart metadata; never return artwork, cookies or stream URLs. */
static void *nc_charts(void){
 void *o=nc_request("/api/toplist",mj.object()),*list=NULL,*out=mj.object(),*items=mj.array();
 int ok=o&&nc.last_api_code==200&&mj.get(o,"list",&list)&&mj.type(list)==5;
 if(ok){int n=mj.array_length(list);if(n>80)n=80;for(int i=0;i<n;i++){
  void *entry=mj.index(list,i);char id[32];const char *name=nc_string(entry,"name");
  if(!nc_id(entry,"id",id)||!name)continue;void *item=mj.object();nc_add_s(item,"id",id);nc_add_s(item,"name",name);mj.append(items,item);
 }}nc_add_i(out,"ok",ok);mj.add(out,"items",items);if(!ok)nc_add_s(out,"error",nc.error[0]?nc.error:"charts_unavailable");if(o)mj.put(o);return out;
}
static void *nc_user_playlists(void){
 void *d=mj.object(),*out=mj.object(),*items=mj.array();if(!nc.authenticated||!nc.uid[0]){nc_add_i(out,"ok",0);nc_add_s(out,"error","login_required");mj.add(out,"items",items);return out;}
 nc_add_s(d,"uid",nc.uid);nc_add_i(d,"limit",50);nc_add_i(d,"offset",0);nc_add_i(d,"includeVideo",0);void *o=nc_request("/api/user/playlist",d),*list=NULL;int ok=o&&nc.last_api_code==200&&mj.get(o,"playlist",&list)&&mj.type(list)==5;
 if(ok){int n=mj.array_length(list);if(n>50)n=50;for(int i=0;i<n;i++){void *p=mj.index(list,i),*count=NULL;char id[32];const char *name=nc_string(p,"name");if(!nc_id(p,"id",id)||!name)continue;void *item=mj.object();nc_add_s(item,"id",id);nc_add_s(item,"name",name);if(mj.get(p,"trackCount",&count)&&mj.type(count)==3)nc_add_i(item,"track_count",mj.integer(count));mj.append(items,item);}}
 nc_add_i(out,"ok",ok);if(!ok)nc_add_s(out,"error",nc.error[0]?nc.error:"playlists_unavailable");mj.add(out,"items",items);if(o)mj.put(o);return out;
}
static void *nc_chart_tracks(const char *id){
 void *d=mj.object();nc_add_s(d,"id",id);nc_add_i(d,"n",20);nc_add_i(d,"s",0);
 void *o=nc_request("/api/v6/playlist/detail",d),*playlist=NULL,*tracks=NULL,*out=mj.object(),*items=mj.array();
 int ok=o&&nc.last_api_code==200&&mj.get(o,"playlist",&playlist)&&mj.get(playlist,"tracks",&tracks)&&mj.type(tracks)==5;
 if(ok){const char *name=nc_string(playlist,"name");if(name)nc_add_s(out,"name",name);int n=mj.array_length(tracks);if(n>20)n=20;
  for(int i=0;i<n;i++){void *song=mj.index(tracks,i),*ar=NULL;char sid[32],artist[256]="";const char *title=nc_string(song,"name");if(!nc_id(song,"id",sid)||!title)continue;
   if(mj.get(song,"ar",&ar)&&mj.type(ar)==5){int a=mj.array_length(ar);if(a>4)a=4;for(int j=0;j<a;j++){const char *s=nc_string(mj.index(ar,j),"name");if(s){size_t z=strlen(artist);snprintf(artist+z,sizeof(artist)-z,"%s%s",z?" / ":"",s);}}}
   void *item=mj.object();nc_add_s(item,"id",sid);nc_add_s(item,"title",title);nc_add_s(item,"artist",artist);nc_add_i(item,"rank",i+1);mj.append(items,item);
  }
 }nc_add_i(out,"ok",ok);mj.add(out,"items",items);nc_add_i(out,"limit",20);if(!ok)nc_add_s(out,"error",nc.error[0]?nc.error:"chart_unavailable");if(o)mj.put(o);return out;
}
static void *nc_play(const char *id,const char *quality,const char *title,const char *artist){
 nc_pending_ticket[0]=nc_pending_url[0]=0;nc_pending_count=0;
 void *out=mj.object();if(!nc.authenticated){nc_add_i(out,"ok",0);nc_add_s(out,"error","login_required");return out;}
 void *d=mj.object();char ids[40];snprintf(ids,sizeof(ids),"[%s]",id);nc_add_s(d,"ids",ids);nc_add_s(d,"level",quality);nc_add_s(d,"encodeType",!strcmp(quality,"lossless")?"flac":"mp3");
 void *o=nc_request("/api/song/enhance/player/url/v1",d),*data=NULL,*track=NULL,*trial=NULL;int code=-1;
 if(o&&mj.get(o,"data",&data)&&mj.type(data)==5)track=mj.index(data,0);const char *url=track?nc_string(track,"url"):NULL;int preview=track&&mj.get(track,"freeTrialInfo",&trial)&&mj.type(trial)!=0;
 if(track)lx_music_number(track,"code",&code);
 if(nc.last_api_code==200&&code==200&&url&&!preview){char result[1024];int bad;
  struct lx_music_track meta={0};snprintf(meta.audio_id,sizeof(meta.audio_id),"netease-%s",id);if(title&&strlen(title)<sizeof(meta.title))snprintf(meta.title,sizeof(meta.title),"%s",title);if(artist&&strlen(artist)<sizeof(meta.artist))snprintf(meta.artist,sizeof(meta.artist),"%s",artist);
  if(nc_defer_play){nc_pending_count=0;bad=strlen(url)>=sizeof(nc_pending_url)||(strncmp(url,"http://",7)&&strncmp(url,"https://",8));if(!bad){unsigned char bytes[16];bad=nc.random(bytes,16)!=1;if(!bad){snprintf(nc_pending_url,sizeof(nc_pending_url),"%s",url);nc_pending_track=meta;for(int i=0;i<16;i++)snprintf(nc_pending_ticket+i*2,3,"%02x",bytes[i]);nc_pending_until=lx_music_ms()+90000;nc_add_s(out,"play_ticket",nc_pending_ticket);nc_add_i(out,"queued",1);}}}
  else {const char *urls[1]={url};bad=lx_music_play_queue(urls,&meta,1,"网易云音乐",result,sizeof(result));}
  nc_add_i(out,"ok",!bad);nc_add_s(out,"track_id",id);nc_add_s(out,"quality",quality);const char *actual=nc_string(track,"level");if(actual)nc_add_s(out,"actual_quality",actual);int bitrate=0;if(lx_music_number(track,"br",&bitrate))nc_add_i(out,"bitrate",bitrate);if(bad)nc_add_s(out,"error","playback_not_confirmed");}
 else{nc_add_i(out,"ok",0);nc_add_i(out,"api_code",nc.last_api_code);nc_add_i(out,"track_code",code);nc_add_i(out,"preview",preview);nc_add_s(out,"error",nc.error[0]?nc.error:preview?"preview_only":"stream_unavailable");}
 if(o)mj.put(o);return out;
}
/* Resolve and enqueue the complete top-20 chart in the factory player. The
   player owns ordering/progress; Sidekick retains only safe display metadata. */
static void *nc_play_chart(const char *id,const char *quality){
 if(nc_defer_play){nc_pending_ticket[0]=nc_pending_url[0]=0;nc_pending_count=0;}
 void *d=mj.object();nc_add_s(d,"id",id);nc_add_i(d,"n",20);nc_add_i(d,"s",0);void *detail=nc_request("/api/v6/playlist/detail",d),*pl=NULL,*tracks=NULL,*out=mj.object();
 if(!detail||nc.last_api_code!=200||!mj.get(detail,"playlist",&pl)||!mj.get(pl,"tracks",&tracks)||mj.type(tracks)!=5){nc_add_i(out,"ok",0);nc_add_s(out,"error",nc.error[0]?nc.error:"chart_unavailable");if(detail)mj.put(detail);return out;}
 int count=mj.array_length(tracks);if(count>20)count=20;if(count<1){nc_add_i(out,"ok",0);nc_add_s(out,"error","empty_playlist");mj.put(detail);return out;}char playlist_name[256]="网易云歌单";const char *returned_name=nc_string(pl,"name");if(returned_name)snprintf(playlist_name,sizeof(playlist_name),"%s",returned_name);
 char ids[640]="[",urls[20][901];const char *urlptr[20];struct lx_music_track queue[20];memset(queue,0,sizeof(queue));int used=0;
 for(int i=0;i<count;i++){void *song=mj.index(tracks,i),*ar=NULL;char sid[32],artist[256]="";const char *name=nc_string(song,"name");if(!nc_id(song,"id",sid)||!name)continue;if(used)strncat(ids,",",sizeof(ids)-strlen(ids)-1);strncat(ids,sid,sizeof(ids)-strlen(ids)-1);snprintf(queue[used].audio_id,sizeof(queue[used].audio_id),"netease-%s",sid);snprintf(queue[used].title,sizeof(queue[used].title),"%s",name);if(mj.get(song,"ar",&ar)&&mj.type(ar)==5){int na=mj.array_length(ar);if(na>4)na=4;for(int j=0;j<na;j++){const char *a=nc_string(mj.index(ar,j),"name");if(a){size_t z=strlen(artist);snprintf(artist+z,sizeof(artist)-z,"%s%s",z?" / ":"",a);}}}snprintf(queue[used].artist,sizeof(queue[used].artist),"%s",artist);used++;}
 strncat(ids,"]",sizeof(ids)-strlen(ids)-1);mj.put(detail);if(!used){nc_add_i(out,"ok",0);nc_add_s(out,"error","empty_playlist");return out;}
 void *req=mj.object();nc_add_s(req,"ids",ids);nc_add_s(req,"level",quality);nc_add_s(req,"encodeType",!strcmp(quality,"lossless")?"flac":"mp3");void *response=nc_request("/api/song/enhance/player/url/v1",req),*data=NULL;int resolved=0;
 if(response&&nc.last_api_code==200&&mj.get(response,"data",&data)&&mj.type(data)==5){int n=mj.array_length(data);if(n>used)n=used;for(int i=0;i<n;i++){void *song=mj.index(data,i),*trial=NULL;int code=-1;const char *url=nc_string(song,"url");if(!url||!lx_music_number(song,"code",&code)||code!=200||(mj.get(song,"freeTrialInfo",&trial)&&mj.type(trial)!=0)||strlen(url)>=sizeof(urls[0]))continue;snprintf(urls[resolved],sizeof(urls[0]),"%s",url);urlptr[resolved]=urls[resolved];if(resolved!=i){queue[resolved]=queue[i];}resolved++;}}
 if(response)mj.put(response);if(!resolved){nc_add_i(out,"ok",0);nc_add_s(out,"error",nc.error[0]?nc.error:"no_entitled_tracks");return out;}
 int bad=0;if(nc_defer_play){unsigned char bytes[16];bad=nc.random(bytes,16)!=1;if(!bad){nc_pending_count=resolved;snprintf(nc_pending_playlist,sizeof(nc_pending_playlist),"%s",playlist_name);for(int i=0;i<resolved;i++){snprintf(nc_pending_urls[i],sizeof(nc_pending_urls[i]),"%s",urlptr[i]);nc_pending_urlptr[i]=nc_pending_urls[i];nc_pending_queue[i]=queue[i];}for(int i=0;i<16;i++)snprintf(nc_pending_ticket+i*2,3,"%02x",bytes[i]);nc_pending_until=lx_music_ms()+90000;nc_add_s(out,"play_ticket",nc_pending_ticket);nc_add_i(out,"queued",1);}}
 else {char result[1024];bad=lx_music_play_queue(urlptr,queue,resolved,playlist_name,result,sizeof(result));}
 nc_add_i(out,"ok",!bad);nc_add_i(out,"queue_total",nc_defer_play?resolved:(bad?0:resolved));nc_add_i(out,"requested_total",used);nc_add_s(out,"playlist",playlist_name);if(bad)nc_add_s(out,"error","playback_not_confirmed");return out;
}
#endif
