/* Synthetic-only ownership, ordering, overflow and restart checks. */
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <assert.h>
struct lx_capture {unsigned sample,calls;int fast;pthread_t owner;};
static long lx_capture_read(struct lx_capture *c,short *pcm,int n){
 if(!c->calls)c->owner=pthread_self();else assert(pthread_equal(c->owner,pthread_self()));
 if(c->fast&&c->calls>=1600)return -1;c->calls++;
 if(!c->fast)usleep(5000);
 for(int i=0;i<n;i++)pcm[i]=(short)((c->sample++)%30000);
 return n;
}
static int lx_capture_recover(struct lx_capture *c,long error){(void)c;(void)error;return -1;}
#include "../src/native_wake_handoff.h"
int main(void){
 struct lx_capture capture={0};short pre[640],out[320];for(int i=0;i<640;i++)pre[i]=-1;
 assert(!lx_handoff_begin(&capture,pre,640));usleep(30000);
 for(int j=0;j<2;j++){assert(lx_handoff_read(out,320)==320);for(int i=0;i<320;i++)assert(out[i]==-1);}
 assert(lx_handoff_read(out,320)==320);for(int i=0;i<320;i++)assert(out[i]==i);
 lx_handoff_discard();usleep(15000);pthread_mutex_lock(&lx_handoff.lock);assert(lx_handoff.used==0);assert(lx_handoff.dropped==0);pthread_mutex_unlock(&lx_handoff.lock);
 lx_handoff_end();unsigned calls=capture.calls;usleep(15000);assert(capture.calls==calls);assert(!lx_handoff.active);
 capture=(struct lx_capture){.fast=1};assert(!lx_handoff_begin(&capture,NULL,0));usleep(50000);
 pthread_mutex_lock(&lx_handoff.lock);assert(lx_handoff.dropped>0);pthread_mutex_unlock(&lx_handoff.lock);assert(lx_handoff_read(out,320)==-1);lx_handoff_end();
 capture=(struct lx_capture){0};assert(!lx_handoff_begin(&capture,NULL,0));assert(lx_handoff_read(out,320)==320);lx_handoff_end();
 puts("handoff_test=pass ordering=pass single_reader=pass bounded_overflow=pass restart=pass join=pass");return 0;
}
