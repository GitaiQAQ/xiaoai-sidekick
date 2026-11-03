#include "../src/native_capture.h"
#include <assert.h>
static int cancel(void *a,const short *b,const short *c,short *d){(void)a;(void)b;(void)c;(void)d;return 0;}
static int preprocess(void *a,short *b){(void)a;(void)b;return 0;}
int main(void){struct lx_capture c={0};assert(!lx_capture_cloud_ready(NULL));assert(!lx_capture_cloud_ready(&c));c.aec=1;c.echo=&c;c.pp=&c;c.ecancel=cancel;c.prun=preprocess;assert(lx_capture_cloud_ready(&c));c.failed=1;assert(!lx_capture_cloud_ready(&c));c.failed=0;c.echo=NULL;assert(!lx_capture_cloud_ready(&c));c.echo=&c;c.pp=NULL;assert(!lx_capture_cloud_ready(&c));c.pp=&c;c.ecancel=NULL;assert(!lx_capture_cloud_ready(&c));c.ecancel=cancel;c.prun=NULL;assert(!lx_capture_cloud_ready(&c));puts("cloud_aec_guard=pass plain=blocked failed=blocked missing_state=blocked missing_function=blocked");return 0;}
