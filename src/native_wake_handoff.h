/* Single owner of ALSA/AEC while acknowledgement and cloud setup run.
 * PCM remains in a bounded memory queue; never writes recordings. */
#ifndef LX_NATIVE_WAKE_HANDOFF_H
#define LX_NATIVE_WAKE_HANDOFF_H
#define LX_HANDOFF_CAP (16000 * 16)
struct lx_wake_handoff {
 pthread_mutex_t lock;pthread_cond_t ready;pthread_t thread;
 struct lx_capture *capture;short *pcm;
 unsigned head,tail,used,dropped;int active,stop,error,discard;
 unsigned long long next_emit_ns;
};
static struct lx_wake_handoff lx_handoff;
static void *lx_handoff_worker(void *unused) {
 (void)unused;short pcm[320];
 for(;;){
  pthread_mutex_lock(&lx_handoff.lock);int stop=lx_handoff.stop;pthread_mutex_unlock(&lx_handoff.lock);if(stop)break;
  long n=lx_capture_read(lx_handoff.capture,pcm,320);
  if(n<0){if(lx_capture_recover(lx_handoff.capture,n)>=0)continue;pthread_mutex_lock(&lx_handoff.lock);lx_handoff.error=1;pthread_cond_broadcast(&lx_handoff.ready);pthread_mutex_unlock(&lx_handoff.lock);break;}
  pthread_mutex_lock(&lx_handoff.lock);
  if(lx_handoff.discard){pthread_mutex_unlock(&lx_handoff.lock);continue;}
  for(long i=0;i<n;i++){
   if(lx_handoff.used==LX_HANDOFF_CAP){lx_handoff.dropped++;continue;}
   lx_handoff.pcm[lx_handoff.head]=pcm[i];lx_handoff.head=(lx_handoff.head+1)%LX_HANDOFF_CAP;lx_handoff.used++;
  }
  pthread_cond_signal(&lx_handoff.ready);pthread_mutex_unlock(&lx_handoff.lock);
 }
 return NULL;
}
static void lx_handoff_end(void) {
 if(!lx_handoff.active)return;
 pthread_mutex_lock(&lx_handoff.lock);lx_handoff.stop=1;pthread_cond_broadcast(&lx_handoff.ready);pthread_mutex_unlock(&lx_handoff.lock);
 pthread_join(lx_handoff.thread,NULL);
 printf("wake_handoff_end dropped_samples=%u\n",lx_handoff.dropped);fflush(stdout);
 pthread_cond_destroy(&lx_handoff.ready);pthread_mutex_destroy(&lx_handoff.lock);free(lx_handoff.pcm);memset(&lx_handoff,0,sizeof(lx_handoff));
}
static void lx_handoff_discard(void) {
 if(!lx_handoff.active)return;
 pthread_mutex_lock(&lx_handoff.lock);lx_handoff.discard=1;lx_handoff.used=0;lx_handoff.head=lx_handoff.tail=0;pthread_mutex_unlock(&lx_handoff.lock);
}
static int lx_handoff_begin(struct lx_capture *capture,const short *pre,int n) {
 if(lx_handoff.active||n<0||n>LX_HANDOFF_CAP)return -1;
 lx_handoff.pcm=malloc(LX_HANDOFF_CAP*sizeof(short));if(!lx_handoff.pcm)return -1;
 if(pthread_mutex_init(&lx_handoff.lock,NULL)){free(lx_handoff.pcm);memset(&lx_handoff,0,sizeof(lx_handoff));return -1;}
 if(pthread_cond_init(&lx_handoff.ready,NULL)){pthread_mutex_destroy(&lx_handoff.lock);free(lx_handoff.pcm);memset(&lx_handoff,0,sizeof(lx_handoff));return -1;}
 lx_handoff.capture=capture;
 if(n){memcpy(lx_handoff.pcm,pre,(size_t)n*sizeof(short));lx_handoff.used=(unsigned)n;lx_handoff.head=(unsigned)n%LX_HANDOFF_CAP;}
 if(pthread_create(&lx_handoff.thread,NULL,lx_handoff_worker,NULL)){pthread_cond_destroy(&lx_handoff.ready);pthread_mutex_destroy(&lx_handoff.lock);free(lx_handoff.pcm);memset(&lx_handoff,0,sizeof(lx_handoff));return -1;}
 lx_handoff.active=1;printf("wake_handoff_begin preroll_samples=%d capacity_samples=%d\n",n,LX_HANDOFF_CAP);fflush(stdout);return 0;
}
static long lx_handoff_read(short *pcm,int n) {
 pthread_mutex_lock(&lx_handoff.lock);
 while(!lx_handoff.used&&!lx_handoff.error&&!lx_handoff.stop){
  struct timespec until;clock_gettime(CLOCK_REALTIME,&until);until.tv_nsec+=100000000L;if(until.tv_nsec>=1000000000L){until.tv_sec++;until.tv_nsec-=1000000000L;}
  if(pthread_cond_timedwait(&lx_handoff.ready,&lx_handoff.lock,&until))break;
 }
 if(lx_handoff.error||lx_handoff.dropped){pthread_mutex_unlock(&lx_handoff.lock);return -1;}
 int got=lx_handoff.used<(unsigned)n?(int)lx_handoff.used:n;
 for(int i=0;i<got;i++){pcm[i]=lx_handoff.pcm[lx_handoff.tail];lx_handoff.tail=(lx_handoff.tail+1)%LX_HANDOFF_CAP;lx_handoff.used--;}
 pthread_mutex_unlock(&lx_handoff.lock);
 if(got>0){
  struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);
  unsigned long long now=(unsigned long long)t.tv_sec*1000000000ULL+(unsigned long long)t.tv_nsec;
  if(!lx_handoff.next_emit_ns)lx_handoff.next_emit_ns=now;
  lx_handoff.next_emit_ns+=(unsigned long long)got*62500ULL;
  if(lx_handoff.next_emit_ns>now){unsigned long long delta=lx_handoff.next_emit_ns-now;if(delta>100000000ULL)delta=100000000ULL;usleep(((unsigned)delta+999U)/1000U);}
 }
 return got;
}
#endif
