#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <dlfcn.h>
#include <sys/prctl.h>
#include <curl/curl.h>

/* Opaque json-c 0.12 ABI, checked against the upstream header. */
struct json_object;
static struct {
 struct json_object *(*parse)(const char *);
 int (*get)(struct json_object *,const char *,struct json_object **);
 int (*type)(struct json_object *);
 const char *(*string)(struct json_object *);
 int (*length)(struct json_object *);
 int (*put)(struct json_object *);
 struct json_object *(*index)(struct json_object *,int);
} j;
static char response[65537];
static size_t used;
static size_t receive(char *p,size_t a,size_t b,void *ctx) {
 size_t n; (void)ctx;
 if(b && a>SIZE_MAX/b)return 0;
 n=a*b; if(n>sizeof(response)-1-used)return 0;
 memcpy(response+used,p,n);used+=n;response[used]=0;return n;
}
static int valid_key(const char *s) {
 size_t n=strlen(s); if(!n || n>4096)return 0;
 for(size_t i=0;i<n;i++)if((unsigned char)s[i]<=32 || (unsigned char)s[i]>=127)return 0;
 return 1;
}
static int valid_model(const char *s) {
 size_t n=strlen(s);if(!n || n>128)return 0;
 for(size_t i=0;i<n;i++) {
  unsigned char c=s[i];
  if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||
       (i && (c=='_'||c=='.'||c==':'||c=='-'))))return 0;
 } return 1;
}
static int completion_valid(const char *text) {
 struct json_object *reply=j.parse(text),*choices=NULL,*first=NULL,*message=NULL,*content=NULL;
 int valid=reply&&j.type(reply)==4&&j.get(reply,"choices",&choices)&&j.type(choices)==5&&
  (first=j.index(choices,0))&&j.type(first)==4&&j.get(first,"message",&message)&&
  j.type(message)==4&&j.get(message,"content",&content)&&j.type(content)==6&&
  j.length(content)>0;
 if(reply)j.put(reply);return valid;
}
/* Responses API may return reasoning items before the message item, so scan the
   output array for the first entry whose content[0].text is a non-empty string. */
static int responses_valid(const char *text) {
 struct json_object *reply=j.parse(text),*output=NULL,*item=NULL,*content=NULL,*part=NULL,*value=NULL;
 int valid=0,i;
 if(!reply)return 0;
 if(j.type(reply)==4&&j.get(reply,"output",&output)&&j.type(output)==5) {
  for(i=0;i<16&&!valid;i++) {
   item=j.index(output,i);if(!item)break;
   if(j.type(item)!=4)continue;
   if(!j.get(item,"content",&content)||j.type(content)!=5)continue;
   part=j.index(content,0);
   if(part&&j.type(part)==4&&j.get(part,"text",&value)&&j.type(value)==6&&j.length(value)>0)valid=1;
  }
 }
 j.put(reply);return valid;
}
/* Structure-only report: item/content type names and text length, never text. */
static void responses_report(const char *text) {
 struct json_object *reply=j.parse(text),*output=NULL,*item=NULL,*content=NULL,*part=NULL,*v=NULL;
 int i;
 if(!reply)return;
 if(j.type(reply)==4&&j.get(reply,"output",&output)&&j.type(output)==5) {
  for(i=0;i<3;i++) {
   const char *kind="?";
   item=j.index(output,i);if(!item)break;
   if(j.type(item)==4&&j.get(item,"type",&v)&&j.type(v)==6)kind=j.string(v);
   content=NULL;
   if(j.type(item)==4&&j.get(item,"content",&content)&&j.type(content)==5) {
    part=j.index(content,0);
    if(part&&j.type(part)==4&&j.get(part,"text",&v)&&j.type(v)==6)
     printf("ark_responses_item%d type=%s content0_text_len=%zu\n",i,kind,(size_t)j.length(v));
    else printf("ark_responses_item%d type=%s content0_no_text\n",i,kind);
   } else printf("ark_responses_item%d type=%s no_content\n",i,kind);
  }
 }
 j.put(reply);
}
int main(int argc,char **argv) {
 void *json=NULL,*curl=NULL; struct json_object *cfg=NULL,*key=NULL,*model=NULL;
 CURL *c=NULL; struct curl_slist *headers=NULL; int result=1,initialized=0;
 char input[8193]={0},authorization[4120]={0},body[512]={0};
 CURLcode (*init_global)(long); void (*cleanup_global)(void);
 CURL *(*init)(void); void (*cleanup)(CURL *);
 CURLcode (*option)(CURL *,CURLoption,...),(*perform)(CURL *),(*info)(CURL *,CURLINFO,...);
 struct curl_slist *(*append)(struct curl_slist *,const char *);
 void (*free_headers)(struct curl_slist *);
 long http=0,verify=-1; CURLcode status;
 if(prctl(PR_SET_DUMPABLE,0L,0L,0L,0L)||prctl(PR_GET_DUMPABLE,0L,0L,0L,0L))return 1;
 int selftest=argc==2&&!strcmp(argv[1],"--self-test");
 int check=argc==2&&!strcmp(argv[1],"--check-config");
 int responses=argc==2&&!strcmp(argv[1],"--responses");
 if(!selftest&&!check&&!responses&&(argc!=2 || strcmp(argv[1],"--stdin-config")))return 2;
 json=dlopen("libjson-c.so.2",RTLD_NOW|RTLD_LOCAL);if(!json)goto done;
#define J(field,symbol) *(void **)(&j.field)=dlsym(json,symbol); if(!j.field)goto done
 J(parse,"json_tokener_parse");J(get,"json_object_object_get_ex");
 J(type,"json_object_get_type");J(string,"json_object_get_string");J(put,"json_object_put");
 J(index,"json_object_array_get_idx");
 J(length,"json_object_get_string_len");
 if(selftest) {
  int pass=valid_key("dummy-test")&&!valid_key("x\r\nAuthorization: x")&&
   !valid_key("")&&valid_model("ep-test_1.0")&&!valid_model("x\"}")&&
   !valid_model("-bad")&&!valid_model("a/b")&&
   completion_valid("{\"choices\":[{\"message\":{\"content\":\"OK\"}}]}")&&
   !completion_valid("{\"error\":{\"message\":\"test\"}}")&&
   !completion_valid("{\"choices\":[]}")&&!completion_valid("{")&&
   !completion_valid("{\"choices\":[{\"message\":{\"content\":null}}]}");
  printf("ark_json_self_test=%s network_calls=0\n",pass?"pass":"fail");
  dlclose(json);return !pass;
 }
 size_t count=fread(input,1,sizeof(input)-1,stdin);
 if(ferror(stdin)||count==sizeof(input)-1||memchr(input,0,count))goto done;
 cfg=j.parse(input);
 if(!cfg||j.type(cfg)!=4||!j.get(cfg,"api_key",&key)||!j.get(cfg,"model",&model)||
    j.type(key)!=6||j.type(model)!=6||!valid_key(j.string(key))||!valid_model(j.string(model))||
    (size_t)j.length(key)!=strlen(j.string(key))||(size_t)j.length(model)!=strlen(j.string(model)))goto done;
 if(check){puts("ark_stdin_config_valid=1 network_calls=0");result=0;goto done;}
 snprintf(authorization,sizeof(authorization),"Authorization: Bearer %s",j.string(key));
 if(responses)
  snprintf(body,sizeof(body),"{\"model\":\"%s\",\"stream\":false,\"max_output_tokens\":256,\"input\":[{\"role\":\"user\",\"content\":[{\"type\":\"input_text\",\"text\":\"Reply only OK\"}]}]}",j.string(model));
 else
  snprintf(body,sizeof(body),"{\"model\":\"%s\",\"messages\":[{\"role\":\"user\",\"content\":\"Reply only OK\"}],\"max_tokens\":32,\"stream\":false}",j.string(model));
 curl=dlopen("libcurl.so.4",RTLD_NOW|RTLD_LOCAL);if(!curl)goto done;
#define C(field,symbol) *(void **)(&field)=dlsym(curl,symbol); if(!field)goto done
 C(init_global,"curl_global_init");C(cleanup_global,"curl_global_cleanup");
 C(init,"curl_easy_init");C(cleanup,"curl_easy_cleanup");C(option,"curl_easy_setopt");
 C(perform,"curl_easy_perform");C(info,"curl_easy_getinfo");
 C(append,"curl_slist_append");C(free_headers,"curl_slist_free_all");
 if(init_global(CURL_GLOBAL_DEFAULT))goto done;initialized=1;
 headers=append(NULL,authorization);if(!headers)goto done;
 struct curl_slist *next=append(headers,"Content-Type: application/json");if(!next)goto done;headers=next;
 c=init();if(!c)goto done;
#define O(k,v) if(option(c,k,v))goto done
 O(CURLOPT_URL,responses?"https://ark.cn-beijing.volces.com/api/v3/responses":
   "https://ark.cn-beijing.volces.com/api/v3/chat/completions");
 O(CURLOPT_HTTPHEADER,headers);O(CURLOPT_POSTFIELDS,body);
 O(CURLOPT_SSL_VERIFYPEER,1L);O(CURLOPT_SSL_VERIFYHOST,2L);
 O(CURLOPT_FOLLOWLOCATION,0L);O(CURLOPT_PROXY,"");O(CURLOPT_NOSIGNAL,1L);
 O(CURLOPT_CONNECTTIMEOUT_MS,5000L);O(CURLOPT_TIMEOUT_MS,20000L);
 O(CURLOPT_WRITEFUNCTION,receive);
 status=perform(c);
 if(info(c,CURLINFO_RESPONSE_CODE,&http)||info(c,CURLINFO_SSL_VERIFYRESULT,&verify))goto done;
 printf("ark_curl_code=%d http_status=%ld verify_result=%ld response_bytes=%zu\n",(int)status,http,verify,used);
 /* Never print provider error bodies or generated content. */
 result=status!=CURLE_OK||http!=200||verify!=0;
 if(!result) {
  if(responses)responses_report(response);
  int valid=responses?responses_valid(response):completion_valid(response);
  printf("ark_%s_shape_valid=%d\n",responses?"responses":"completion",valid);result=!valid;
 }
done:
 if(c)cleanup(c);
 if(headers)free_headers(headers);
 if(initialized)cleanup_global();
 if(cfg)j.put(cfg);
 volatile char *wipe=input;for(size_t i=0;i<sizeof(input);i++)wipe[i]=0;
 wipe=authorization;for(size_t i=0;i<sizeof(authorization);i++)wipe[i]=0;
 if(curl)dlclose(curl);if(json)dlclose(json);
 printf("ark_operation=%s result=%s audio_uploaded=0\n",check?"config-check":"http-smoke",result?"fail":"pass");return result;
}
