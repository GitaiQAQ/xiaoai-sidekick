/* macOS Foundation-backed JSON mocks. No sockets, ubus processes or device IO. */
#import <Foundation/Foundation.h>
#import <CoreFoundation/CoreFoundation.h>
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <ctype.h>
#include <time.h>
#include "../src/native_tools.h"
static id node(void *p){return (id)p;}
static void *jparse(const char *s){if(!s)return NULL;NSData *data=[NSData dataWithBytes:s length:strlen(s)];return [[NSJSONSerialization JSONObjectWithData:data options:NSJSONReadingMutableContainers|NSJSONReadingFragmentsAllowed error:NULL] retain];}
static int jget(void *o,const char *k,void **v){if(![node(o) isKindOfClass:[NSDictionary class]])return 0;id x=[node(o) objectForKey:[NSString stringWithUTF8String:k]];if(!x)return 0;*v=x;return 1;}
static int jtype(void *p){id o=node(p);if(!o||o==[NSNull null])return 0;if([o isKindOfClass:[NSString class]])return 6;if([o isKindOfClass:[NSDictionary class]])return 4;if([o isKindOfClass:[NSArray class]])return 5;if([o isKindOfClass:[NSNumber class]]){if(CFGetTypeID((CFTypeRef)o)==CFBooleanGetTypeID())return 1;const char *t=[o objCType];return !strcmp(t,"d")||!strcmp(t,"f")?2:3;}return -1;}
static int jint(void *p){return [node(p) intValue];}
static int64_t jint64(void *p){return [node(p) longLongValue];}
static int jarraylen(void *p){return (int)[node(p) count];}
static void *jindex(void *p,int i){return [node(p) objectAtIndex:(NSUInteger)i];}
static int jlength(void *p){return (int)[node(p) lengthOfBytesUsingEncoding:NSUTF8StringEncoding];}
static const char *jstring(void *p){return [node(p) UTF8String];}
static void jput(void *p){[node(p) release];}
static void *jobject(void){return [[NSMutableDictionary alloc]init];}
static void *jarray(void){return [[NSMutableArray alloc]init];}
static int jappend(void *p,void *v){[node(p) addObject:node(v)];jput(v);return 0;}
static void *jstr(const char *s){return [[NSString alloc]initWithUTF8String:s];}
static void *jnewint(int n){return [[NSNumber alloc]initWithInt:n];}
static void jadd(void *o,const char *k,void *v){[node(o) setObject:node(v) forKey:[NSString stringWithUTF8String:k]];jput(v);}
static const char *jencode(void *p){NSData *d=[NSJSONSerialization dataWithJSONObject:node(p) options:NSJSONWritingFragmentsAllowed error:NULL];return [[[NSString alloc]initWithData:d encoding:NSUTF8StringEncoding]autorelease].UTF8String;}
static struct {
 void *(*parse)(const char *);int (*get)(void *,const char *,void **);int (*type)(void *);int (*integer)(void *);int64_t (*integer64)(void *);int (*array_length)(void *);int (*object_length)(void *);void *(*index)(void *,int);int (*length)(void *);const char *(*string)(void *);void (*put)(void *);void *(*object)(void);void *(*array)(void);int (*append)(void *,void *);void *(*new_string)(const char *);void *(*new_int)(int);void (*add)(void *,const char *,void *);const char *(*encode)(void *);
} mj={jparse,jget,jtype,jint,jint64,jarraylen,jarraylen,jindex,jlength,jstring,jput,jobject,jarray,jappend,jstr,jnewint,jadd,jencode};
static void add_i(void *o,const char *k,int n){jadd(o,k,jnewint(n));}
static void add_s(void *o,const char *k,const char *s){jadd(o,k,jstr(s));}
static int agent_pid(void){return 123;}

static void jdel(void *o,const char *key){[node(o) removeObjectForKey:[NSString stringWithUTF8String:key]];}
