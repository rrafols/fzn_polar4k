/*
 * polar.h - public interface of the platform independent core of
 * "Polarfield" (fuzzion, 2010).  A port only has to:
 *
 *   1. create an OpenGL 3.3 core / OpenGL ES 3.0 / WebGL2 context
 *   2. call polar_init()
 *   3. play mzk loading music, call polar_bake_step() + polar_draw_loading()
 *      every frame until the bake is finished
 *   4. start the song, then call polar_frame(ms) every frame with the
 *      audio clock in milliseconds since the song started
 */
#ifndef POLAR_H
#define POLAR_H

#ifdef __cplusplus
extern "C" {
#endif

/* --- scene generation + rendering (polar_scene.c / polar_render.c) ----- */

/* Generates all geometry and GL objects.  Needs a current GL context.
 * Returns 0 on failure. */
int   polar_init(void);

/* Runs up to `nbatches` ambient-occlusion bake batches (1024 vertices each).
 * Returns 1 once every scene has been baked. */
int   polar_bake_step(int nbatches);
float polar_bake_progress(void);          /* 0..1 */

/* Draws the original "Loading" screen into the current framebuffer. */
void  polar_draw_loading(int width, int height);

/* Draws one frame.  `ms` is milliseconds since the song started. */
void  polar_frame(long ms, int width, int height);

/* Framebuffer object that polar_frame()/polar_draw_loading() render into
 * (default 0 = the window).  Useful for offscreen capture. */
void  polar_set_framebuffer(unsigned fbo);

/* Frees GL objects. */
void  polar_end(void);

/* --- music (polar_synth.c) ---------------------------------------------- */

#define MZK_RATE     44100
#define MZK_TICK_MS  110               /* one tracker row */
#define MZK_LAST_ORDER 54              /* the song ends when order > 54 */

/* Renders the whole song (order 1..54) to mono float PCM at MZK_RATE.
 * Returns the number of frames; *out is malloc()ed. */
int   mzk_render_song(float **out);

/* Renders `bars` bars of the loading pattern (order 0) meant to be looped. */
int   mzk_render_loading(float **out, int bars);

/* Sequencer position derived from the audio clock, like the original
 * `order` global the visuals key off. */
int   mzk_order_at_ms(long ms);
int   mzk_finished(long ms);

#ifdef __cplusplus
}
#endif
#endif
