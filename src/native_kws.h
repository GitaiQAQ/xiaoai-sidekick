/* Offline "豆包豆包" wake word scorer: MFCC + DTW, header-only.
   cos/log/sqrt are implemented here so the firmware program keeps its
   existing runtime dependencies: libc and libdl only (no libm).
   Frame: 25 ms / 400 samples, hop 10 ms / 160 samples, 512-point FFT,
   24 mel filters over 0-8000 Hz, log, DCT-II -> 13 cepstra + 13 deltas. */
#ifndef LX06_NATIVE_KWS_H
#define LX06_NATIVE_KWS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "kws_templates.h"

#define LX_KWS_RATE 16000
#define LX_KWS_FRAME 400
#define LX_KWS_HOP 160
#define LX_KWS_FFT 512
#define LX_KWS_BINS (LX_KWS_FFT / 2 + 1)
#define LX_KWS_MEL 24
#define LX_KWS_COEF 13
#define LX_KWS_DIM (LX_KWS_COEF * 2)
#define LX_KWS_WIN 160              /* 1.6 s sliding window */
#define LX_KWS_STEP 20              /* 0.2 s hop */
#define LX_KWS_MIN_SPAN 40          /* 400 ms */
#define LX_KWS_MAX_SPAN 250         /* 2.5 s */
#define LX_KWS_MAX_FRAMES 250
#define LX_KWS_PREROLL (LX_KWS_RATE * 2)   /* two seconds retained in memory */
#ifndef LX_KWS_MIN_HITS
#define LX_KWS_MIN_HITS 3           /* consecutive windows below threshold */
#endif
#define LX_KWS_MIN_DB 25.0f         /* window peak below this is treated as silence */
#define LX_KWS_SPAN_MARGIN_DB 15.0f /* speech span = peak minus this many dB */
#define LX_KWS_FLOOR_MARGIN_DB 6.0f /* ...but always above the window noise floor */
#define LX_KWS_MERGE_GAP 15          /* 150 ms: merge syllable gaps */
#define LX_KWS_MIN_SNR_DB 9.0f       /* peak must exceed the window floor */
#define LX_KWS_SNR_DB 12.0f         /* peak must clear the running noise floor */
#define LX_KWS_LOUD_DB 38.0f         /* ...unless the window is speech-filled */
#define LX_KWS_PI 3.14159265358979f

/* ---------- small libm-free math ---------- */

static float lx_kws_cos(float x) {
    const float two_pi = 6.28318530718f;
    float sign = 1.0f, x2;
    int k = (int)(x * 0.15915494309f);
    x -= (float)k * two_pi;
    if (x < 0.0f) x += two_pi;                 /* [0, 2pi) */
    if (x > LX_KWS_PI) x -= two_pi;            /* [-pi, pi) */
    if (x > 1.57079632679f) { x = LX_KWS_PI - x; sign = -1.0f; }
    else if (x < -1.57079632679f) { x = -LX_KWS_PI - x; sign = -1.0f; }
    x2 = x * x;
    return sign * (1.0f + x2 * (-0.5f + x2 * (0.0416666667f + x2 * (-0.0013888889f +
        x2 * (2.4801587e-05f + x2 * (-2.7557319e-07f + x2 * 2.0876757e-09f))))));
}
static float lx_kws_sin(float x) { return lx_kws_cos(x - 1.57079632679f); }

static float lx_kws_log(float v) {
    union { float f; uint32_t u; } bits;
    float m, t, t2, sum;
    int e;
    if (!(v > 0.0f)) return -100.0f;
    bits.f = v;
    e = (int)((bits.u >> 23) & 0xFF) - 127;
    if (e < -126) return -100.0f;              /* subnormal: floored by callers */
    bits.u = (bits.u & 0x007FFFFFu) | 0x3F800000u;
    m = bits.f;                                /* [1, 2) */
    if (m > 1.41421356f) { m *= 0.5f; e += 1; }/* [0.707, 1.414) */
    t = (m - 1.0f) / (m + 1.0f);
    t2 = t * t;
    sum = t * (2.0f + t2 * (0.66666667f + t2 * (0.4f + t2 * (0.28571429f +
        t2 * (0.22222222f + t2 * 0.18181818f)))));
    return (float)e * 0.69314718056f + sum;
}
static float lx_kws_log10(float v) { return lx_kws_log(v) * 0.43429448190f; }

static float lx_kws_sqrt(float v) {
    union { float f; uint32_t u; } bits;
    float r;
    int i;
    if (!(v > 0.0f)) return 0.0f;
    bits.f = v;
    bits.u = (bits.u >> 1) + 0x1FC00000u;      /* rough exponent halving */
    r = bits.f;
    for (i = 0; i < 4; i++) r = 0.5f * (r + v / r);
    return r;
}

/* ---------- MFCC front end ---------- */

struct lx_kws_mfcc {
    float window[LX_KWS_FRAME];
    float cos_t[LX_KWS_FFT / 2];
    float sin_t[LX_KWS_FFT / 2];
    unsigned short rev[LX_KWS_FFT];
    float mel_w[LX_KWS_MEL][LX_KWS_BINS];
    float dct[LX_KWS_COEF][LX_KWS_MEL];
    float re[LX_KWS_FFT];
    float im[LX_KWS_FFT];
};

struct lx_kws_utt {
    int n;
    float f[LX_KWS_MAX_FRAMES][LX_KWS_DIM];
};

static void lx_kws_mfcc_init(struct lx_kws_mfcc *m) {
    float edge[LX_KWS_MEL + 2];
    int i, j, b;
    for (i = 0; i < LX_KWS_FRAME; i++)
        m->window[i] = 0.54f - 0.46f * lx_kws_cos(2.0f * LX_KWS_PI * (float)i /
                                                  (float)(LX_KWS_FRAME - 1));
    for (i = 0; i < LX_KWS_FFT / 2; i++) {
        float a = -2.0f * LX_KWS_PI * (float)i / (float)LX_KWS_FFT;
        m->cos_t[i] = lx_kws_cos(a);
        m->sin_t[i] = lx_kws_sin(a);
    }
    for (i = 0; i < LX_KWS_FFT; i++) {
        unsigned r = 0;
        for (j = 0; j < 9; j++) if (i & (1 << j)) r |= 1u << (8 - j);
        m->rev[i] = (unsigned short)r;
    }
    for (i = 0; i < LX_KWS_MEL + 2; i++) {
        float hz = 8000.0f * (float)i / (float)(LX_KWS_MEL + 1);
        edge[i] = 2595.0f * lx_kws_log10(1.0f + hz / 700.0f);
    }
    for (b = 0; b < LX_KWS_BINS; b++) {
        float hz = (float)b * (float)LX_KWS_RATE / (float)LX_KWS_FFT;
        float mel = 2595.0f * lx_kws_log10(1.0f + hz / 700.0f);
        for (i = 0; i < LX_KWS_MEL; i++) {
            float w = 0.0f;
            if (mel > edge[i] && mel < edge[i + 1])
                w = (mel - edge[i]) / (edge[i + 1] - edge[i]);
            else if (mel >= edge[i + 1] && mel < edge[i + 2])
                w = (edge[i + 2] - mel) / (edge[i + 2] - edge[i + 1]);
            m->mel_w[i][b] = w;
        }
    }
    for (i = 0; i < LX_KWS_COEF; i++)
        for (b = 0; b < LX_KWS_MEL; b++)
            m->dct[i][b] = lx_kws_cos(LX_KWS_PI * (float)i * ((float)b + 0.5f) /
                                      (float)LX_KWS_MEL);
}

static void lx_kws_fft(struct lx_kws_mfcc *m) {
    int i, j, len;
    for (i = 0; i < LX_KWS_FFT; i++) {
        int r = m->rev[i];
        if (r > i) {
            float tr = m->re[i], ti = m->im[i];
            m->re[i] = m->re[r]; m->im[i] = m->im[r];
            m->re[r] = tr; m->im[r] = ti;
        }
    }
    for (len = 2; len <= LX_KWS_FFT; len <<= 1) {
        int half = len >> 1, step = LX_KWS_FFT / len;
        for (i = 0; i < LX_KWS_FFT; i += len) {
            for (j = 0; j < half; j++) {
                int t = j * step, a = i + j, c = a + half;
                float wr = m->cos_t[t], wi = m->sin_t[t];
                float xr = m->re[c] * wr - m->im[c] * wi;
                float xi = m->re[c] * wi + m->im[c] * wr;
                m->re[c] = m->re[a] - xr; m->im[c] = m->im[a] - xi;
                m->re[a] += xr; m->im[a] += xi;
            }
        }
    }
}

/* One 25 ms frame: cepstra plus the frame level in dB (mean square). */
static void lx_kws_frame(struct lx_kws_mfcc *m, const short *pcm, float *c, float *db) {
    float loge[LX_KWS_MEL], acc = 0.0f;
    int i, b, k;
    for (i = 0; i < LX_KWS_FRAME; i++) {
        float v = (float)pcm[i];
        acc += v * v;
        m->re[i] = v * m->window[i];
        m->im[i] = 0.0f;
    }
    for (i = LX_KWS_FRAME; i < LX_KWS_FFT; i++) { m->re[i] = 0.0f; m->im[i] = 0.0f; }
    *db = 10.0f * lx_kws_log10(acc / (float)LX_KWS_FRAME + 1.0f);
    lx_kws_fft(m);
    for (b = 0; b < LX_KWS_MEL; b++) {
        float s = 0.0f;
        for (i = 0; i < LX_KWS_BINS; i++) {
            float p = m->re[i] * m->re[i] + m->im[i] * m->im[i];
            s += m->mel_w[b][i] * p;
        }
        loge[b] = lx_kws_log(s + 1e-4f);
    }
    for (k = 0; k < LX_KWS_COEF; k++) {
        float s = 0.0f;
        for (b = 0; b < LX_KWS_MEL; b++) s += m->dct[k][b] * loge[b];
        c[k] = s;
    }
}

/* Feature metric switches (overridable at compile time for tuning runs). */
#ifndef LX_KWS_SKIP_C0
#define LX_KWS_SKIP_C0 0
#endif
#ifndef LX_KWS_DELTA_SCALE
#define LX_KWS_DELTA_SCALE 1.0f
#endif
#ifndef LX_KWS_USE_STATIC
#define LX_KWS_USE_STATIC 1
#endif
#ifndef LX_KWS_USE_DELTA
#define LX_KWS_USE_DELTA 1
#endif
#ifndef LX_KWS_VAR_NORM
#define LX_KWS_VAR_NORM 0
#endif
#ifndef LX_KWS_BAND
#define LX_KWS_BAND 0.2f
#endif
#ifndef LX_KWS_C0_SCALE
#define LX_KWS_C0_SCALE 0.25f
#endif
#ifndef LX_KWS_CMN
#define LX_KWS_CMN 0
#endif

/* Append deltas and remove the per-utterance mean (cepstral mean normalisation),
   optionally also scaling every dimension to unit variance. */
static void lx_kws_finalize(struct lx_kws_utt *u, const float c[][LX_KWS_COEF]) {
    float mean[LX_KWS_DIM];
    float mean0 = 0.0f;
    int t, d;
    if (u->n <= 0) return;
    for (t = 0; t < u->n; t++)
        for (d = 0; d < LX_KWS_COEF; d++) u->f[t][d] = c[t][d];
    for (t = 0; t < u->n; t++) {
        int p = t > 0 ? t - 1 : 0, q = t + 1 < u->n ? t + 1 : u->n - 1;
        for (d = 0; d < LX_KWS_COEF; d++)
            u->f[t][LX_KWS_COEF + d] = 0.5f * (c[q][d] - c[p][d]);
    }
    for (d = 0; d < LX_KWS_DIM; d++) mean[d] = 0.0f;
    for (t = 0; t < u->n; t++)
        for (d = 0; d < LX_KWS_DIM; d++) mean[d] += u->f[t][d];
    for (d = 0; d < LX_KWS_DIM; d++) mean[d] /= (float)u->n;
#if LX_KWS_CMN == 0
    mean0 = mean[0];
#else
    (void)mean0;
#endif
#if LX_KWS_CMN
    for (t = 0; t < u->n; t++)
        for (d = 0; d < LX_KWS_DIM; d++) u->f[t][d] -= mean[d];
#else
    /* Without full CMN keep only the level-normalised energy contour, scaled
       down so it supports the syllable rhythm without dominating the cost. */
    (void)mean;
    for (t = 0; t < u->n; t++) u->f[t][0] = (u->f[t][0] - mean0) * LX_KWS_C0_SCALE;
#endif
#if LX_KWS_VAR_NORM
    {
        float inv[LX_KWS_DIM];
        for (d = 0; d < LX_KWS_DIM; d++) inv[d] = 0.0f;
        for (t = 0; t < u->n; t++)
            for (d = 0; d < LX_KWS_DIM; d++) inv[d] += u->f[t][d] * u->f[t][d];
        for (d = 0; d < LX_KWS_DIM; d++) {
            float sd = lx_kws_sqrt(inv[d] / (float)u->n);
            inv[d] = sd > 1e-3f ? 1.0f / sd : 0.0f;
        }
        for (t = 0; t < u->n; t++)
            for (d = 0; d < LX_KWS_DIM; d++) u->f[t][d] *= inv[d];
    }
#endif
}

/* Features for pcm[0..nsamp): hop 10 ms, frame 25 ms. Rows go to c/db. */
static int lx_kws_extract(struct lx_kws_mfcc *m, const short *pcm, int nsamp,
                          float c[][LX_KWS_COEF], float *db, float *peak_db) {
    int t = 0;
    if (peak_db) *peak_db = -100.0f;
    while (t * LX_KWS_HOP + LX_KWS_FRAME <= nsamp && t < LX_KWS_MAX_FRAMES) {
        lx_kws_frame(m, pcm + t * LX_KWS_HOP, c[t], &db[t]);
        if (peak_db && db[t] > *peak_db) *peak_db = db[t];
        t++;
    }
    return t;
}

/* Trim a dB profile to the burst group that contains its peak. Bursts are
   merged across dips shorter than LX_KWS_MERGE_GAP frames (syllable gaps). */
static int lx_kws_trim(const float *db, int n, int *first, int *last) {
    float maxdb = -200.0f, mindb = 1e6f, thr;
    int i, j, peak = 0, start, end;
    *first = *last = -1;
    if (n <= 0) return 0;
    for (i = 0; i < n; i++) {
        if (db[i] > maxdb) { maxdb = db[i]; peak = i; }
        if (db[i] < mindb) mindb = db[i];
    }
    if (maxdb < LX_KWS_MIN_DB) return 0;
    /* Reject stationary broadband noise: a real utterance stands well above
       the window floor. Very loud windows are exempt because they may be
       filled by speech from edge to edge. */
    if (maxdb - mindb < LX_KWS_MIN_SNR_DB && maxdb < LX_KWS_LOUD_DB) return 0;
    thr = maxdb - LX_KWS_SPAN_MARGIN_DB;
    if (thr < mindb + LX_KWS_FLOOR_MARGIN_DB) thr = mindb + LX_KWS_FLOOR_MARGIN_DB;
    if (db[peak] <= thr) return 0;
    start = end = peak;
    i = peak;
    while (i > 0) {
        int gap = 0;
        j = i - 1;
        while (j >= 0 && db[j] <= thr && gap < LX_KWS_MERGE_GAP) { j--; gap++; }
        if (j < 0 || gap >= LX_KWS_MERGE_GAP) break;
        start = j;
        i = j;
    }
    i = peak;
    while (i < n - 1) {
        int gap = 0;
        j = i + 1;
        while (j < n && db[j] <= thr && gap < LX_KWS_MERGE_GAP) { j++; gap++; }
        if (j >= n || gap >= LX_KWS_MERGE_GAP) break;
        end = j;
        i = j;
    }
    *first = start;
    *last = end;
    return end - start + 1;
}

/* Extract, trim and normalise one utterance from pcm[0..nsamp). */
static int lx_kws_utt_from_pcm(struct lx_kws_mfcc *m, const short *pcm, int nsamp,
                               struct lx_kws_utt *u, int *span_frames, float *peak_db) {
    static float c[LX_KWS_MAX_FRAMES][LX_KWS_COEF];
    static float db[LX_KWS_MAX_FRAMES];
    int n, first, last, span;
    n = lx_kws_extract(m, pcm, nsamp, c, db, peak_db);
    u->n = 0;
    span = lx_kws_trim(db, n, &first, &last);
    if (span_frames) *span_frames = span;
    if (span <= 0) return 0;
    u->n = span;
    lx_kws_finalize(u, (const float (*)[LX_KWS_COEF])&c[first][0]);
    return span;
}

/* ---------- DTW ---------- */

static float lx_kws_dist(const float *a, const float *b) {
    float s = 0.0f;
    int d;
    for (d = (LX_KWS_USE_STATIC ? (LX_KWS_SKIP_C0 ? 1 : 0) : LX_KWS_COEF); d < LX_KWS_COEF; d++) {
        float t = a[d] - b[d];
        s += t * t;
    }
    for (d = (LX_KWS_USE_DELTA ? LX_KWS_COEF : LX_KWS_DIM); d < LX_KWS_DIM; d++) {
        float t = (a[d] - b[d]) * LX_KWS_DELTA_SCALE;
        s += t * t;
    }
    return lx_kws_sqrt(s);
}

/* Sakoe-Chiba band (20 % of the template length) around the length-normalised
   diagonal; the path cost is divided by the total frame count. */
static float lx_kws_dtw(const float *a, int na, const float *b, int nb) {
    static float row0[LX_KWS_MAX_FRAMES + 1], row1[LX_KWS_MAX_FRAMES + 1];
    float *prev = row0, *cur = row1, *tmp;
    const float inf = 1e30f;
    float band = LX_KWS_BAND * (float)nb + 1.5f;
    int i, j;
    if (na <= 0 || nb <= 0 || na > LX_KWS_MAX_FRAMES || nb > LX_KWS_MAX_FRAMES)
        return -1.0f;
    for (j = 0; j <= nb; j++) prev[j] = inf;
    prev[0] = 0.0f;
    for (i = 1; i <= na; i++) {
        float diag = (float)i * (float)nb / (float)na;
        cur[0] = inf;
        for (j = 1; j <= nb; j++) {
            float best;
            if ((float)j < diag - band || (float)j > diag + band) { cur[j] = inf; continue; }
            best = prev[j];
            if (prev[j - 1] < best) best = prev[j - 1];
            if (cur[j - 1] < best) best = cur[j - 1];
            if (best >= inf * 0.5f) { cur[j] = inf; continue; }
            cur[j] = best + lx_kws_dist(a + (i - 1) * LX_KWS_DIM, b + (j - 1) * LX_KWS_DIM);
        }
        tmp = prev; prev = cur; cur = tmp;
    }
    if (prev[nb] >= inf * 0.5f) return -1.0f;
    return prev[nb] / (float)(na + nb);
}

/* Best template distance for one utterance; templates whose length differs by
   more than 25 % are skipped so partial syllables cannot match. */
static float lx_kws_match(const float *f, int n, int *which_out) {
    float best = 1e30f;
    int i, which = -1;
    for (i = 0; i < LX_KWS_TEMPLATE_COUNT; i++) {
        int len = (int)lx_kws_template_len[i];
        float d;
        if ((long)n * 4 < (long)len * 3 || (long)n * 4 > (long)len * 5) continue;
        d = lx_kws_dtw(f, n, lx_kws_template_feat + lx_kws_template_off[i] * LX_KWS_DIM, len);
        if (d >= 0.0f && d < best) { best = d; which = i; }
    }
    if (which_out) *which_out = which;
    return which >= 0 ? best : -1.0f;
}

/* ---------- enrollment segments ---------- */

struct lx_kws_seg { int start_ms, dur_ms; };

/* Same 20 ms energy endpointing as native_kws_enroll.c so segment ids match. */
static int lx_kws_segment(const short *pcm, int nsamp, struct lx_kws_seg *out, int max) {
    static float rms[4096];
    const int frame = 320, hop = 160;
    int frames = (nsamp - frame) / hop, i, n = 0, in_seg = 0, start = 0, last = 0;
    float max_rms = 0.0f, thr;
    if (frames > (int)(sizeof(rms) / sizeof(rms[0]))) frames = (int)(sizeof(rms) / sizeof(rms[0]));
    if (frames <= 0) return 0;
    for (i = 0; i < frames; i++) {
        double acc = 0.0;
        int k;
        for (k = 0; k < frame; k++) { double v = pcm[i * hop + k]; acc += v * v; }
        rms[i] = (float)(acc / frame);
        if (rms[i] > max_rms) max_rms = rms[i];
    }
    thr = max_rms * 0.02f;
    if (thr < 150.0f) thr = 150.0f;
    for (i = 0; i < frames; i++) {
        int ms = i * 10;
        if (rms[i] > thr) {
            if (!in_seg) { in_seg = 1; start = ms; }
            last = ms;
        } else if (in_seg && ms - last > 250) {
            in_seg = 0;
            if (last - start >= 300 && n < max)
                out[n].start_ms = start, out[n].dur_ms = last - start, n++;
        }
    }
    if (in_seg && last - start >= 300 && n < max)
        out[n].start_ms = start, out[n].dur_ms = last - start, n++;
    return n;
}

/* ---------- sliding-window detector ---------- */

struct lx_kws {
    struct lx_kws_mfcc mf;
    short buf[LX_KWS_FRAME];
    int fill;
    float cep[LX_KWS_WIN][LX_KWS_COEF];
    float db[LX_KWS_WIN];
    int nframes;
    int hops;
    int evals;
    int hits;
    int min_hits;   /* runtime override: music masking needs one more window */
    int woke;
    int span_ms;
    float peak_db;
    float floor_db;
    float noise_db;
    float flux;
    int noise_init;
    float last_dist;
    float best_dist;
    int best_template;
    short preroll[LX_KWS_PREROLL];
    int preroll_pos;
};

typedef void (*lx_kws_report)(void *ctx, struct lx_kws *k, int span_ms, float dist,
                              int hit, int woke);

/* Optional runtime override (0 = use the generated threshold). */
static float lx_kws_threshold_override = 0.0f;

static float lx_kws_threshold(void) {
    return lx_kws_threshold_override > 0.0f ? lx_kws_threshold_override
                                            : lx_kws_template_threshold;
}

static void lx_kws_init(struct lx_kws *k) {
    memset(k, 0, sizeof(*k));
    lx_kws_mfcc_init(&k->mf);
    k->best_dist = 1e30f;
    k->min_hits = LX_KWS_MIN_HITS;
}

/* Value at the given percentile of a dB profile (used for the noise floor). */
static float lx_kws_percentile(const float *v, int n, int pct) {
    static unsigned short hist[241];
    float lo = 1e9f, hi = -1e9f, width;
    int i, total = 0, want;
    if (n <= 0) return 0.0f;
    for (i = 0; i < n; i++) {
        if (v[i] < lo) lo = v[i];
        if (v[i] > hi) hi = v[i];
    }
    if (!(hi > lo)) return lo;
    for (i = 0; i <= 240; i++) hist[i] = 0;
    width = (hi - lo) / 240.0f;
    for (i = 0; i < n; i++) {
        int b = (int)((v[i] - lo) / width);
        if (b < 0) b = 0;
        if (b > 240) b = 240;
        hist[b]++;
    }
    want = n * pct / 100;
    for (i = 0; i <= 240; i++) {
        total += hist[i];
        if (total > want) return lo + width * (float)i;
    }
    return hi;
}

static void lx_kws_eval(struct lx_kws *k, lx_kws_report report, void *ctx) {
    static struct lx_kws_utt u;
    int span, hit = 0, which = -1;
    float best = -1.0f;
    int first = -1, last = -1, i;
    k->evals++;
    k->span_ms = 0;
    {
        float hi = -200.0f, lo = 1e6f;
        for (i = 0; i < LX_KWS_WIN; i++) {
            if (k->db[i] > hi) hi = k->db[i];
            if (k->db[i] < lo) lo = k->db[i];
        }
        k->peak_db = hi;
        k->floor_db = lo;
    }
    /* Running noise floor from the window's 10th percentile: falls fast,
       rises slowly (about 4 s), never below the digital-silence clamp. */
    {
        float p10 = lx_kws_percentile(k->db, LX_KWS_WIN, 10);
        if (!k->noise_init) { k->noise_db = p10; k->noise_init = 1; }
        else if (p10 < k->noise_db) k->noise_db += (p10 - k->noise_db) * 0.5f;
        else k->noise_db += (p10 - k->noise_db) * 0.02f;
        if (k->noise_db < 5.0f) k->noise_db = 5.0f;
    }
    span = 0;
    if (k->peak_db >= k->noise_db + LX_KWS_SNR_DB || k->peak_db >= LX_KWS_LOUD_DB)
        span = lx_kws_trim(k->db, LX_KWS_WIN, &first, &last);
    if (span > 0) {
        k->span_ms = span * 10;
        if (span >= LX_KWS_MIN_SPAN && span <= LX_KWS_MAX_SPAN) {
            u.n = span;
            lx_kws_finalize(&u, (const float (*)[LX_KWS_COEF])&k->cep[first][0]);
            {
                float sum = 0.0f;
                for (i = first; i < last; i++) {
                    float d = k->db[i + 1] - k->db[i];
                    sum += d < 0.0f ? -d : d;
                }
                k->flux = sum / (float)(span - 1);
            }
            best = lx_kws_match(u.f[0], span, &which);
            if (which >= 0 && best < lx_kws_threshold()) hit = 1;
        }
    }
    if (which >= 0) {
        k->last_dist = best;
        if (best < k->best_dist) { k->best_dist = best; k->best_template = which; }
    } else {
        k->last_dist = -1.0f;
    }
    if (hit) { if (k->hits < 100) k->hits++; } else k->hits = 0;
    if (k->hits >= k->min_hits) k->woke = 1;
    if (report) report(ctx, k, k->span_ms, k->last_dist, hit, k->woke);
}

static void lx_kws_push(struct lx_kws *k, const short *pcm, int n,
                        lx_kws_report report, void *ctx) {
    int pos = 0, p;
    for (p = 0; p < n; p++) {
        k->preroll[k->preroll_pos] = pcm[p];
        k->preroll_pos = (k->preroll_pos + 1) % LX_KWS_PREROLL;
    }
    while (pos < n) {
        int need = LX_KWS_FRAME - k->fill;
        int take = (n - pos) < need ? (n - pos) : need;
        memcpy(k->buf + k->fill, pcm + pos, (size_t)take * sizeof(short));
        k->fill += take;
        pos += take;
        if (k->fill == LX_KWS_FRAME) {
            float c[LX_KWS_COEF], db;
            int i;
            lx_kws_frame(&k->mf, k->buf, c, &db);
            if (k->nframes == LX_KWS_WIN) {
                memmove(k->cep[0], k->cep[1], sizeof(k->cep[0]) * (LX_KWS_WIN - 1));
                memmove(k->db, k->db + 1, sizeof(k->db[0]) * (LX_KWS_WIN - 1));
                k->nframes = LX_KWS_WIN - 1;
            }
            for (i = 0; i < LX_KWS_COEF; i++) k->cep[k->nframes][i] = c[i];
            k->db[k->nframes] = db;
            k->nframes++;
            memmove(k->buf, k->buf + LX_KWS_HOP,
                    (size_t)(LX_KWS_FRAME - LX_KWS_HOP) * sizeof(short));
            k->fill -= LX_KWS_HOP;
            k->hops++;
            if (k->hops >= LX_KWS_STEP) {
                k->hops = 0;
                if (k->nframes == LX_KWS_WIN) lx_kws_eval(k, report, ctx);
            }
        }
    }
}

/* Copy the preroll in chronological order into out (returns sample count). */
__attribute__((unused))
static int lx_kws_preroll(struct lx_kws *k, short *out) {
    int i;
    for (i = 0; i < LX_KWS_PREROLL; i++)
        out[i] = k->preroll[(k->preroll_pos + i) % LX_KWS_PREROLL];
    return LX_KWS_PREROLL;
}

/* ---------- WAV scoring / template dump ---------- */

static short *lx_kws_read_wav(const char *path, int *nsamp) {
    FILE *f = fopen(path, "rb");
    unsigned char head[12], chunk[8];
    unsigned char *data = NULL;
    unsigned int data_len = 0, fmt_ok = 0;
    short *pcm;
    if (!f) return NULL;
    if (fread(head, 1, 12, f) != 12 || memcmp(head, "RIFF", 4) || memcmp(head + 8, "WAVE", 4)) {
        fclose(f);
        return NULL;
    }
    while (fread(chunk, 1, 8, f) == 8) {
        unsigned int len = (unsigned int)chunk[4] | ((unsigned int)chunk[5] << 8) |
                           ((unsigned int)chunk[6] << 16) | ((unsigned int)chunk[7] << 24);
        if (!memcmp(chunk, "fmt ", 4)) {
            unsigned char fmt[16];
            unsigned int take = len < 16 ? len : 16;
            if (fread(fmt, 1, take, f) != take) break;
            if (take < 16) break;
            fmt_ok = (fmt[0] == 1 && fmt[1] == 0 && fmt[2] == 1 && fmt[3] == 0 &&
                      (fmt[4] | (fmt[5] << 8)) == LX_KWS_RATE &&
                      fmt[14] == 16 && fmt[15] == 0);
            if (len > take) fseek(f, (long)(len - take), SEEK_CUR);
        } else if (!memcmp(chunk, "data", 4)) {
            data = malloc(len ? len : 1);
            if (!data) break;
            if (fread(data, 1, len, f) != len) { free(data); data = NULL; break; }
            data_len = len;
            break;
        } else {
            fseek(f, (long)((len + 1) & ~1u), SEEK_CUR);
        }
    }
    fclose(f);
    if (!data || !fmt_ok) { free(data); return NULL; }
    pcm = malloc((size_t)data_len + sizeof(short));
    if (!pcm) { free(data); return NULL; }
    memcpy(pcm, data, data_len);
    free(data);
    *nsamp = (int)(data_len / 2);
    return pcm;
}

/* Prints segment timing, the pairwise DTW matrix of the kept segments, the
   tightest template cluster and (when templates exist) per-segment verdicts
   plus a full-file sliding-window scan. dump!=0 also prints raw features. */
static int lx_kws_score_file(const char *path, FILE *out, int dump) {
    static struct lx_kws_utt *seg[16];
    static unsigned char kept[16];
    struct lx_kws_seg segs[16];
    struct lx_kws_mfcc *mf = NULL;
    short *pcm;
    int n = 0, nseg, i, j, nkept = 0;
    int tpl[3] = {-1, -1, -1}, ntpl = 0;
    float intra_max = 0.0f, threshold, best_sum = 1e30f;
    pcm = lx_kws_read_wav(path, &n);
    if (!pcm) { fprintf(out, "wav_open_failed path=%s\n", path); return 1; }
    fprintf(out, "wav=%s seconds=%.2f samples=%d rate=%d\n", path,
            (double)n / (double)LX_KWS_RATE, n, LX_KWS_RATE);
    fprintf(out, "templates=%d header_threshold=%.4f intra_max=%.4f\n",
            LX_KWS_TEMPLATE_COUNT, lx_kws_template_threshold, lx_kws_template_intra_max);
    mf = calloc(1, sizeof(*mf));
    if (!mf) { free(pcm); return 1; }
    lx_kws_mfcc_init(mf);
    nseg = lx_kws_segment(pcm, n, segs, 16);
    fprintf(out, "segments=%d\n", nseg);
    for (i = 0; i < nseg; i++) {
        float peak = -100.0f;
        int frames = 0, span = 0;
        int start = segs[i].start_ms * LX_KWS_RATE / 1000;
        int length = (segs[i].dur_ms + 200) * LX_KWS_RATE / 1000;
        seg[i] = calloc(1, sizeof(struct lx_kws_utt));
        if (!seg[i]) { free(pcm); free(mf); return 1; }
        kept[i] = 0;
        if (segs[i].dur_ms < 400) {
            fprintf(out, "segment %d start=%dms dur=%dms rejected=too_short\n",
                    i + 1, segs[i].start_ms, segs[i].dur_ms);
            continue;
        }
        if (segs[i].dur_ms > 2500) {
            fprintf(out, "segment %d start=%dms dur=%dms rejected=too_long\n",
                    i + 1, segs[i].start_ms, segs[i].dur_ms);
            continue;
        }
        if (start + length > n) length = n - start;
        frames = lx_kws_utt_from_pcm(mf, pcm + start, length, seg[i], &span, &peak);
        if (frames <= 0 || span < LX_KWS_MIN_SPAN || span > LX_KWS_MAX_SPAN) {
            fprintf(out, "segment %d start=%dms dur=%dms rejected=span=%dms\n",
                    i + 1, segs[i].start_ms, segs[i].dur_ms, span * 10);
            seg[i]->n = 0;
            continue;
        }
        kept[i] = 1;
        fprintf(out, "segment %d start=%dms dur=%dms span=%dms frames=%d peak=%.1fdB kept=1\n",
                i + 1, segs[i].start_ms, segs[i].dur_ms, span * 10, frames, peak);
        nkept++;
    }
    /* pairwise DTW over kept segments */
    for (i = 0; i < nseg; i++) {
        for (j = i + 1; j < nseg; j++) {
            if (!kept[i] || !kept[j]) continue;
            fprintf(out, "dtw[%d][%d]=%.4f\n", i, j,
                    lx_kws_dtw(seg[i]->f[0], seg[i]->n, seg[j]->f[0], seg[j]->n));
        }
    }
    /* tightest triangle (or the only kept pair / single segment) */
    if (nkept >= 3) {
        int a, b, c;
        for (a = 0; a < nseg; a++) if (kept[a])
            for (b = a + 1; b < nseg; b++) if (kept[b])
                for (c = b + 1; c < nseg; c++) if (kept[c]) {
                    float s = lx_kws_dtw(seg[a]->f[0], seg[a]->n, seg[b]->f[0], seg[b]->n)
                            + lx_kws_dtw(seg[a]->f[0], seg[a]->n, seg[c]->f[0], seg[c]->n)
                            + lx_kws_dtw(seg[b]->f[0], seg[b]->n, seg[c]->f[0], seg[c]->n);
                    if (s < best_sum) { best_sum = s; tpl[0] = a; tpl[1] = b; tpl[2] = c; }
                }
        ntpl = 3;
    } else if (nkept == 2) {
        for (i = 0; i < nseg; i++) if (kept[i]) tpl[ntpl++] = i;
    } else if (nkept == 1) {
        for (i = 0; i < nseg; i++) if (kept[i]) { tpl[0] = i; ntpl = 1; }
    }
    if (ntpl) {
        for (i = 0; i < ntpl; i++)
            for (j = i + 1; j < ntpl; j++) {
                float d = lx_kws_dtw(seg[tpl[i]]->f[0], seg[tpl[i]]->n,
                                     seg[tpl[j]]->f[0], seg[tpl[j]]->n);
                if (d > intra_max) intra_max = d;
            }
        threshold = intra_max * 1.3f;
        fprintf(out, "cluster=");
        for (i = 0; i < ntpl; i++) fprintf(out, "%s%d", i ? "," : "", tpl[i]);
        fprintf(out, " intra_max=%.4f proposed_threshold=%.4f\n", intra_max, threshold);
    } else {
        threshold = 0.0f;
        fprintf(out, "cluster=none\n");
    }
    if (dump) {
        for (i = 0; i < nseg; i++) {
            int t, d;
            if (!kept[i]) continue;
            fprintf(out, "dump segment=%d frames=%d\n", i, seg[i]->n);
            for (t = 0; t < seg[i]->n; t++) {
                fprintf(out, "f");
                for (d = 0; d < LX_KWS_DIM; d++) fprintf(out, " %.4f", (double)seg[i]->f[t][d]);
                fprintf(out, "\n");
            }
        }
    }
    /* verdict of every kept segment against the compiled templates */
    if (LX_KWS_TEMPLATE_COUNT > 0) {
        for (i = 0; i < nseg; i++) {
            float best = -1.0f;
            int which = -1;
            if (!kept[i]) continue;
            best = lx_kws_match(seg[i]->f[0], seg[i]->n, &which);
            fprintf(out, "verdict segment=%d dur=%dms dist=%.4f template=%d result=%s\n",
                    i, segs[i].dur_ms, (double)best, which,
                    (which >= 0 && best < lx_kws_template_threshold) ? "below" : "above");
        }
        /* feed the whole file through the live detector (same code path) */
        {
            struct lx_kws *k = calloc(1, sizeof(*k));
            int woke_at = -1, woke_count = 0, step = 320;
            float bg_min = 1e30f;
            int bg_count = 0, scored = 0;
            if (k) {
                lx_kws_init(k);
                for (i = 0; i + step <= n; i += step) {
                    int before = k->evals;
                    int t1 = (i + step) * 1000 / LX_KWS_RATE, t0 = t1 - 1600, overlap = 0;
                    lx_kws_push(k, pcm + i, step, NULL, NULL);
                    if (k->evals == before) continue;
                    fprintf(out, "scan t=%dms span=%dms peak=%.1f floor=%.1f flux=%.2f dist=%.4f hits=%d wake=%d\n",
                            t1, k->span_ms, (double)k->peak_db, (double)k->floor_db,
                            (double)k->flux, (double)k->last_dist, k->hits,
                            k->hits == LX_KWS_MIN_HITS);
                    if (k->hits == LX_KWS_MIN_HITS) {
                        if (woke_at < 0) woke_at = t1;
                        woke_count++;
                    }
                    for (j = 0; j < nseg; j++)
                        if (segs[j].start_ms + segs[j].dur_ms > t0 && segs[j].start_ms < t1)
                            overlap = 1;
                    if (k->last_dist >= 0.0f && !overlap) {
                        bg_count++;
                        if (k->last_dist < bg_min) bg_min = k->last_dist;
                    }
                    if (k->last_dist >= 0.0f) scored++;
                }
                fprintf(out, "scan_result wakes=%d first_wake_ms=%d windows=%d scored=%d "
                             "background=%d background_min=%.4f best=%.4f\n",
                        woke_count, woke_at, k->evals, scored, bg_count,
                        bg_count ? (double)bg_min : -1.0, (double)k->best_dist);
                free(k);
            }
        }
    } else {
        fprintf(out, "verdict skipped templates_missing\n");
    }
    for (i = 0; i < nseg; i++) free(seg[i]);
    free(mf);
    free(pcm);
    return 0;
}

#endif
