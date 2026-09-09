/*
 * polar_synth.c - the music, ported from mzk.c.
 *
 * The original generated 8 samples with a little subtractive synth into
 * DirectSound buffers (one buffer per sample per channel, 64 in total) and a
 * thread ticked a 8-channel pattern sequencer every 110 ms, calling
 * SetFrequency / SetVolume / Play / Stop on those buffers.
 *
 * Here the same synth generates the same samples, and a software mixer
 * plays the same sequencer offline into a float PCM buffer.  Ports only have
 * to stream that buffer.  The sequencer state at a given time is exposed
 * through mzk_order_at_ms() so the visuals can key off `order` exactly like
 * the original did.
 */
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "polar_math.h"
#include "polar.h"

#define NUM_SAMPLES 8
#define NCHANNELS   8
#define TICK_FRAMES ((MZK_RATE * MZK_TICK_MS) / 1000)   /* 4851 */

typedef struct {
    unsigned char  wType;
    unsigned short nSamples;
    short freqIni;
    short dFreq;
    char  ampIni;
    char  dAmp;
    unsigned char cutIni;
    char  dCut;
    char  resIni;
    char  dRes;
    unsigned char bRandGain;
} SAMPLE;

static const SAMPLE smp[NUM_SAMPLES] = {
    { 2, 187,   779, -2000, 107, -105, 77,  -74,  48,  -65,    0 },
    { 1, 1000,  0,       0,  68,  -67, 253, -49,  25,  -17,  255 },
    { 0, 8250,  81,      0,  77,  -74, 68,  -69,  84,   12,    0 },
    { 1, 1000,  0,       0,  68,  -67, 253, -49,  25,  -17,  255 },
    { 0, 1000,  220,     0, 100,  -99, 50,  -38, -31,  118,    0 },
    { 2, 21250, 440,     0, 109, -108, 12,   30,  82,  -90,  255 },
    { 1, 3000,  440,     0, 100,  -99, 93,  -90, -89,   41,    0 },
    { 2, 500,   110,     0,  53,    0, 102, -81, -40, -123,   13 }
};

static const unsigned char channelList[29 * 17] = {
    6,197,0,0,0,197,0,200,0,0,0,0,0,0,0,195,0,
    6,0,0,200,0,0,0,204,0,0,0,0,0,200,0,0,0,
    6,0,0,200,0,0,0,209,0,0,0,0,0,200,0,0,0,
    2,197,0,128,0,192,0,128,0,192,0,128,0,192,0,128,0,
    2,193,0,128,0,192,0,128,0,192,0,128,0,192,0,128,0,
    1,161,15,97,15,97,15,65,0,0,0,97,161,209,0,129,0,
    7,15,0,197,0,15,0,197,67,133,0,197,0,15,0,197,0,
    4,204,76,0,140,0,204,0,76,204,76,0,140,0,204,0,76,
    4,200,72,0,136,0,200,0,72,200,72,0,136,0,200,0,72,
    0,193,0,0,0,193,0,0,0,193,0,0,0,193,0,0,0,
    1,0,0,0,0,209,15,0,81,15,145,15,0,209,15,0,0,
    3,225,97,161,97,225,97,161,97,225,97,161,97,225,97,161,97,
    2,213,64,192,64,192,64,192,64,192,0,0,64,192,64,192,64,
    2,209,64,192,64,192,64,192,64,192,0,0,64,192,64,192,64,
    7,15,0,193,0,15,0,193,67,129,0,193,0,15,0,193,0,
    2,202,64,192,64,192,64,192,64,192,0,0,64,192,64,192,64,
    4,197,69,0,133,0,197,0,69,197,69,0,133,0,197,0,69,
    7,15,0,202,0,15,0,202,72,138,0,202,0,15,0,202,0,
    6,197,0,0,0,197,0,202,0,0,0,0,0,0,0,195,0,
    6,0,0,202,0,0,0,209,0,0,0,0,0,202,0,0,0,
    6,195,0,0,0,195,0,200,0,0,0,0,0,0,0,195,0,
    2,200,64,192,64,192,64,192,64,192,0,0,64,192,64,192,64,
    4,195,67,0,131,0,195,0,67,195,67,0,131,0,195,0,67,
    7,15,0,200,0,15,0,200,71,136,0,200,0,15,0,200,0,
    2,204,64,192,64,192,64,192,64,192,0,0,64,192,64,192,64,
    4,199,71,0,135,0,199,0,71,199,71,0,135,0,199,0,71,
    7,15,0,204,0,15,0,204,81,140,0,204,0,15,0,204,0,
    6,196,0,0,0,196,0,199,0,0,0,0,0,0,0,196,0,
    6,0,0,199,0,0,0,204,0,0,0,0,0,199,0,0,0
};

/* DirectSound volumes in 1/100 dB */
static const int vols[4] = { -10000, -1200, -600, 0 };

static const signed char orderList[55] = {
    0,                         /* loading (0) */
    1,0,0,0,1,0,0,0,           /* logo fuzzion (1-8) */
    1,0,0,0,                   /* pulpoide (9-12) */
    1,0,0,0,0,0,0,             /* dacube 3 (13-19) */
    1,1,0,0,0,1,0,0,0,1,0,0,0, /* pinxos (20-32) */
    1,0,1,0,1,1,1,1,1,1,1,1,-12,0,0,0,0,0,0,-6,0,
    0                          /* [54]: the original read past the array here */
};

static const unsigned char patternList[19 * 8] = {
    255,255,255,255,255, 255, 0,  1,
    255,255,255,255,255,  0,  1,  3,
    255,255,255,255,255,  0,  2,  4,
    255,255,255,255,  0,  1,  3,  5,
    255,  0,  1,  3,  4,  5,  6,  7,
    255,255,255,255,255,  0,  1,  4,
      0,  1,  6,  7,  9, 10, 11, 12,
      0,  2,  8,  9, 10, 11, 13, 14,
      9, 10, 11, 15, 16, 17, 18, 19,
      1,  9, 10, 11, 20, 21, 22, 23,
      9, 10, 11, 24, 25, 26, 27, 28,
    255,255,255,255,255,  0,  1,  5,
    255,255,255,255,255,  1,  5, 20,
    255,255,255,255,255,  5, 18, 19,
    255,255,255,255,255,  5, 27, 28,
    255,255,255,255,  0,  1,  5,  6,
    255,255,255,255,  1,  5, 20,  23,
    255,255,255,255,  5, 17, 18, 19,
    255,255,255,255,  5, 26, 27, 28
};

/* ---- sample generation --------------------------------------------------- */

static unsigned char *sampledata[NUM_SAMPLES * NCHANNELS];   /* 8 bit unsigned, like DirectSound */
static int            samplelen[NUM_SAMPLES];
static int            freqtable[12 * 9];
static int            samples_ready;

static void genSample(int sample, unsigned char *fb)
{
    float fVal, fdVal, amp, damp, buf0, buf1, lfc, lfb, f, q, dres, dcut;
    double phase;
    int iVal, i, ns;
    signed short uVal;
    float val, tr;
    const SAMPLE *s = smp + sample;

    ns = s->nSamples << 4;

    fVal = s->freqIni;
    fdVal = 1.f + s->dFreq / (16.f * 256.f * 256.f);
    amp = s->ampIni / 100.f;
    damp = (0.01f * s->dAmp) / ns;
    buf0 = 0.f;
    buf1 = 0.f;
    f = s->cutIni * 85.f / 44100.f;
    q = s->resIni / 100.f;
    dres = (0.01f * s->dRes) / ns;
    dcut = (s->dCut * 85.f / 44100.f) / ns;
    phase = 0.0;

    for (i = 0; i < ns; i++) {
        phase += fVal;
        iVal = (int)lrint(phase);
        uVal = (signed short)(iVal & 0xffff);
        tr = 2.f * uVal / 65536.f;                  /* saw */

        if (s->wType)
            tr = (float)sin(phase * 2.0 * 3.1415926535897932 / 44100.0);   /* sine */

        if (s->wType == 2)                          /* square */
            tr = (tr > 0) ? 1.f : -1.f;

        tr += s->bRandGain * myRandFloat() / 127.f; /* noise */

        /* resonant low pass */
        lfc = f;
        lfb = q + q / (1.0f - lfc);
        val = tr;
        buf0 += lfc * (val - buf0 + lfb * (buf0 - buf1));
        buf1 += lfc * (buf0 - buf1);
        tr = buf1;
        f += dcut;
        q += dres;

        tr *= amp;
        fVal *= fdVal;
        amp += damp;

        tr *= 80.f;
        if (tr > 80.f) tr = 80.f;
        if (tr < -80.f) tr = -80.f;
        tr += 127;

        fb[i] = (unsigned char)float2int(tr);
    }
}

static void init_samples(void)
{
    int i, c;
    double fq;
    if (samples_ready) return;

    /* mzk_init_loader() generated every sample once per channel, each with
     * its own run of the shared random generator (holdrand starts at 0) */
    holdrand = 0;
    for (i = 0; i < NUM_SAMPLES; i++) {
        samplelen[i] = smp[i].nSamples << 4;
        for (c = 0; c < NCHANNELS; c++) {
            sampledata[c + i * NCHANNELS] = (unsigned char *)malloc((size_t)samplelen[i]);
            genSample(i, sampledata[c + i * NCHANNELS]);
        }
    }

    /* freqtable[i] = 1378.125 * 1.05946309436^i, rounded (fist) at each step */
    fq = 1378.125;
    for (i = 0; i < 12 * 9; i++) {
        freqtable[i] = (int)lrint(fq);
        fq *= 1.05946309436;
    }
    samples_ready = 1;
}

/* ---- sequencer + mixer --------------------------------------------------- */

typedef struct {
    int    playing;
    double pos;         /* position in source frames */
    double rate;        /* source frames per output frame */
    float  gain;        /* DirectSound volume, linear */
    int    len;
    const unsigned char *data;
} voice_t;

typedef struct {
    voice_t voices[NUM_SAMPLES * NCHANNELS];
    int row, order, ol;
} seq_t;

static void seq_init(seq_t *s, int order)
{
    int i;
    memset(s, 0, sizeof(*s));
    for (i = 0; i < NUM_SAMPLES * NCHANNELS; i++) {
        s->voices[i].gain = 1.f;                     /* buffers start at 0 dB */
        s->voices[i].data = sampledata[i];
        s->voices[i].len = samplelen[i / NCHANNELS];
    }
    s->row = 0;
    s->order = order;
    s->ol = 0;
}

/* one iteration of threadmain(): advance a row and fire events */
static void seq_tick(seq_t *s)
{
    int i, pt, sample, note, vol, oct;

    s->row++;
    if (s->row > 15) {
        s->row = 0;
        if (s->order > 0) {
            s->ol += orderList[s->order] << 3;   /* delta encoding */
            s->order++;
        }
    }

    for (i = 0; i < 8; i++) {
        pt = patternList[s->ol + i];
        if (pt == 255) continue;
        pt = (pt << 4) + pt;                    /* *17 */

        sample = channelList[pt] * NCHANNELS + i;
        note = channelList[pt + s->row + 1];

        vol = (note >> 6) & 0x03;
        oct = (((note >> 4) & 0x03) + 4) * 12;
        note = (note & 0x0f);

        if (vol != 0)
            s->voices[sample].gain = (float)pow(10.0, vols[vol] / 2000.0);
        if (note != 0) {                        /* note on / off */
            s->voices[sample].playing = 0;
            s->voices[sample].pos = 0.0;
            if (note != 0xf) {
                s->voices[sample].rate = (double)freqtable[oct + note - 1] / 44100.0;
                s->voices[sample].playing = 1;
            }
        }
    }
}

static void seq_mix(seq_t *s, float *out, int frames)
{
    int v, k;
    memset(out, 0, sizeof(float) * frames);
    for (v = 0; v < NUM_SAMPLES * NCHANNELS; v++) {
        voice_t *vc = &s->voices[v];
        if (!vc->playing) continue;
        for (k = 0; k < frames; k++) {
            int i = (int)vc->pos;
            float a, b, fr;
            if (i >= vc->len - 1) { vc->playing = 0; break; }
            fr = (float)(vc->pos - i);
            a = ((float)vc->data[i] - 128.f) / 128.f;
            b = ((float)vc->data[i + 1] - 128.f) / 128.f;
            out[k] += (a + fr * (b - a)) * vc->gain;
            vc->pos += vc->rate;
        }
    }
    for (k = 0; k < frames; k++) {
        if (out[k] > 1.f) out[k] = 1.f;
        if (out[k] < -1.f) out[k] = -1.f;
    }
}

static int render(float **out, int start_order, int ticks, int tail_frames)
{
    seq_t seq;
    int t, total = ticks * TICK_FRAMES + tail_frames;
    float *buf = (float *)malloc(sizeof(float) * (size_t)total);
    if (!buf) return 0;
    init_samples();
    seq_init(&seq, start_order);
    for (t = 0; t < ticks; t++) {
        seq_tick(&seq);
        seq_mix(&seq, buf + t * TICK_FRAMES, TICK_FRAMES);
    }
    if (tail_frames) seq_mix(&seq, buf + ticks * TICK_FRAMES, tail_frames);
    *out = buf;
    return total;
}

/* ---- public ---------------------------------------------------------------- */

/* mzk_start() set row=0, order=1; the song is over once order > 54, which
 * happens on the 16*54th tick. */
#define SONG_TICKS (16 * MZK_LAST_ORDER)

int mzk_render_song(float **out)
{
    return render(out, 1, SONG_TICKS, MZK_RATE * 2);
}

int mzk_render_loading(float **out, int bars)
{
    return render(out, 0, bars * 16, 0);
}

int mzk_order_at_ms(long ms)
{
    long k;
    if (ms < 0) ms = 0;
    k = ms / MZK_TICK_MS;                  /* ticks elapsed since mzk_start */
    return 1 + (int)((k + 1) / 16);
}

int mzk_finished(long ms)
{
    return mzk_order_at_ms(ms) > MZK_LAST_ORDER;
}
