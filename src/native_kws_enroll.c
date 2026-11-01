/* Slice the enrollment recording into utterances by short-term energy.
   Prints segment timing and peak so we can confirm five "豆包豆包" takes. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define FRAME 320              /* 20 ms at 16 kHz */
#define HOP 160                /* 10 ms */
#define MIN_GAP_MS 250
#define MIN_SEG_MS 300

int main(int argc,char **argv) {
    const char *path=argc>1?argv[1]:"/tmp/doubao_enroll.wav";
    FILE *f=fopen(path,"rb");
    long size;unsigned char *raw;short *pcm;long samples,i;
    int frames;
    float *rms;
    float max_rms=0;
    int in_seg=0,seg_start=0,last_loud=0,segments=0;
    if(!f){fputs("open failed\n",stderr);return 1;}
    fseek(f,0,SEEK_END);size=ftell(f);fseek(f,0,SEEK_SET);
    raw=malloc((size_t)size);
    if(!raw||fread(raw,1,(size_t)size,f)!=(size_t)size){fputs("read failed\n",stderr);return 1;}
    fclose(f);
    pcm=(short *)(raw+44);
    samples=(size-44)/2;
    frames=(int)((samples-FRAME)/HOP);
    rms=malloc(sizeof(float)*(size_t)frames);
    if(!rms)return 1;
    for(i=0;i<frames;i++) {
        const short *p=pcm+i*HOP;double acc=0;int k;
        for(k=0;k<FRAME;k++){double v=p[k];acc+=v*v;}
        rms[i]=(float)(acc/FRAME);
        if(rms[i]>max_rms)max_rms=rms[i];
    }
    printf("seconds=%.1f frames=%d max_rms=%.0f\n",(double)samples/16000.0,frames,max_rms);
    {
        float thr=max_rms*0.02f;if(thr<150.0f)thr=150.0f;
        printf("threshold_rms=%.0f\n",thr);
        for(i=0;i<frames;i++) {
            int loud=rms[i]>thr;
            long ms=i*10;
            if(loud) {
                if(!in_seg){in_seg=1;seg_start=(int)ms;}
                last_loud=(int)ms;
            } else if(in_seg&&ms-last_loud>MIN_GAP_MS) {
                in_seg=0;
                if(last_loud-seg_start>=MIN_SEG_MS) {
                    printf("segment %d start=%dms end=%dms dur=%dms\n",
                        ++segments,seg_start,last_loud,last_loud-seg_start);
                }
            }
        }
        if(in_seg&&last_loud-seg_start>=MIN_SEG_MS)
            printf("segment %d start=%dms end=%dms dur=%dms\n",
                ++segments,seg_start,last_loud,last_loud-seg_start);
    }
    printf("segments=%d\n",segments);
    free(rms);free(raw);
    return segments?0:1;
}
