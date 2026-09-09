/*
 * main_web.c - web port of Polarfield (fuzzion, 2010).
 *
 * The common C core is compiled to WebAssembly with Emscripten and renders
 * through WebGL2 (Emscripten maps the GLES 3.0 calls 1:1).  This file only
 * provides what a browser needs on top:
 *   - a WebGL2 context on the <canvas>
 *   - a requestAnimationFrame main loop
 *   - Web Audio playback of the pre-rendered PCM (in JS via EM_JS)
 *
 * Browsers only allow audio after a user gesture, so shell.html calls
 * web_begin() from a click handler; until then we just wait.
 */
#include <stdio.h>
#include <stdlib.h>
#include <emscripten.h>
#include <emscripten/html5.h>
#include "polar_gl.h"
#include "polar.h"

#define CANVAS_W 1024
#define CANVAS_H 768

static float *song, *loading;
static int    song_len, loading_len;
static int    started, loading_playing, baked, finished;

/* ---- audio (JavaScript) ------------------------------------------------- */

/* Creating (or resuming) the AudioContext has to happen inside the user
 * gesture; the PCM buffers are handed over later from the main loop. */
EM_JS(void, js_audio_create, (void), {
    var a = { ctx: null, src: null, t0: 0, wall0: 0 };
    Module.polarAudio = a;
    try {
        a.ctx = new (window.AudioContext || window.webkitAudioContext)({ sampleRate: 44100 });
        if (a.ctx.state === 'suspended') a.ctx.resume();
    } catch (e) {
        console.warn('Web Audio unavailable, running silent: ' + e);
        a.ctx = null;
    }
});

EM_JS(void, js_audio_play, (const float *pcm, int len, int loop), {
    var a = Module.polarAudio;
    if (!a) return;
    a.wall0 = performance.now();
    if (!a.ctx || len <= 0) return;
    if (a.src) { try { a.src.stop(); } catch (e) {} }
    var b = a.ctx.createBuffer(1, len, 44100);
    b.copyToChannel(new Float32Array(HEAPF32.buffer, pcm, len), 0);
    var src = a.ctx.createBufferSource();
    src.buffer = b;
    src.loop = !!loop;
    src.connect(a.ctx.destination);
    a.t0 = a.ctx.currentTime;
    src.start(a.t0);
    a.src = src;
});

/* song clock: the audio clock when we have one, wall clock otherwise */
EM_JS(double, js_audio_song_ms, (void), {
    var a = Module.polarAudio;
    if (!a) return 0;
    if (a.ctx && a.ctx.state === 'running') return (a.ctx.currentTime - a.t0) * 1000.0;
    return performance.now() - a.wall0;
});

EM_JS(void, js_on_finished, (void), {
    if (Module.onPolarFinished) Module.onPolarFinished();
});

/* ---- main loop ------------------------------------------------------------ */

static void loop(void)
{
    if (finished) return;
    if (!started) {
        /* nothing to do until the user clicks; keep the canvas white */
        glClearColor(1.f, 1.f, 1.f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT);
        return;
    }
    if (!loading_playing) {
        /* the thread in mzk.c played pattern 0 while the intro loaded */
        js_audio_play(loading, loading_len, 1);
        loading_playing = 1;
    }
    if (!baked) {
        baked = polar_bake_step(4);          /* ~1024 vertices per batch */
        polar_draw_loading(CANVAS_W, CANVAS_H);
        if (baked) js_audio_play(song, song_len, 0);      /* mzk_start() */
        return;
    }
    {
        long ms = (long)js_audio_song_ms();
        polar_frame(ms, CANVAS_W, CANVAS_H);
        if (mzk_finished(ms)) {
            finished = 1;
            js_on_finished();
        }
    }
}

/* called from shell.html inside a click handler */
EMSCRIPTEN_KEEPALIVE void web_begin(void)
{
    if (started) return;
    started = 1;
    js_audio_create();
}

int main(void)
{
    EmscriptenWebGLContextAttributes attrs;
    EMSCRIPTEN_WEBGL_CONTEXT_HANDLE ctx;

    emscripten_set_canvas_element_size("#canvas", CANVAS_W, CANVAS_H);
    emscripten_webgl_init_context_attributes(&attrs);
    attrs.majorVersion = 2;
    attrs.minorVersion = 0;
    attrs.alpha = 0;
    attrs.depth = 1;
    attrs.stencil = 0;
    attrs.antialias = 1;
    attrs.preserveDrawingBuffer = 0;
    ctx = emscripten_webgl_create_context("#canvas", &attrs);
    if (ctx <= 0) {
        fprintf(stderr, "WebGL2 context creation failed (%d)\n", (int)ctx);
        return 1;
    }
    emscripten_webgl_make_context_current(ctx);

    if (!polar_init()) {
        fprintf(stderr, "polar_init() failed\n");
        return 1;
    }
    song_len    = mzk_render_song(&song);
    loading_len = mzk_render_loading(&loading, 4);

    emscripten_set_main_loop(loop, 0, 0);
    return 0;
}
