/*
 * capture_mac.c - headless capture tool (macOS).
 *
 * Renders frames of the intro at given times into PNG files and writes the
 * song as a WAV, without opening a window.  Handy to verify the common core
 * or to grab stills for a README.
 *
 *   capture out_dir 500 5000 17000 ...    (times in ms since the song start)
 *
 * Build: see the `capture` target in ports/mac-x86_64/Makefile
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <OpenGL/OpenGL.h>
#include <OpenGL/gl3.h>
#include <ImageIO/ImageIO.h>
#include <CoreServices/CoreServices.h>
#include "polar.h"

#define W 1024
#define H 768

static int write_png(const char *path, const unsigned char *rgba, int w, int h)
{
    CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
    CGDataProviderRef dp;
    CGImageRef img;
    CFURLRef url;
    CGImageDestinationRef dst;
    unsigned char *flipped = malloc((size_t)w * h * 4);
    int y, ok = 0;
    for (y = 0; y < h; y++)                       /* GL is bottom-up */
        memcpy(flipped + (size_t)y * w * 4, rgba + (size_t)(h - 1 - y) * w * 4, (size_t)w * 4);
    dp = CGDataProviderCreateWithData(NULL, flipped, (size_t)w * h * 4, NULL);
    img = CGImageCreate(w, h, 8, 32, w * 4, cs, kCGImageAlphaNoneSkipLast, dp, NULL, 0, kCGRenderingIntentDefault);
    url = CFURLCreateFromFileSystemRepresentation(NULL, (const UInt8 *)path, (CFIndex)strlen(path), 0);
    dst = CGImageDestinationCreateWithURL(url, CFSTR("public.png"), 1, NULL);
    if (dst) {
        CGImageDestinationAddImage(dst, img, NULL);
        ok = CGImageDestinationFinalize(dst);
        CFRelease(dst);
    }
    CFRelease(url); CGImageRelease(img); CGDataProviderRelease(dp); CGColorSpaceRelease(cs);
    free(flipped);
    return ok;
}

static void write_wav(const char *path, const float *pcm, int frames)
{
    FILE *f = fopen(path, "wb");
    int i, datasize = frames * 2, v;
    unsigned int u;
    unsigned short s;
    if (!f) return;
    fwrite("RIFF", 1, 4, f); u = 36 + datasize; fwrite(&u, 4, 1, f);
    fwrite("WAVEfmt ", 1, 8, f); u = 16; fwrite(&u, 4, 1, f);
    s = 1; fwrite(&s, 2, 1, f); s = 1; fwrite(&s, 2, 1, f);
    u = MZK_RATE; fwrite(&u, 4, 1, f); u = MZK_RATE * 2; fwrite(&u, 4, 1, f);
    s = 2; fwrite(&s, 2, 1, f); s = 16; fwrite(&s, 2, 1, f);
    fwrite("data", 1, 4, f); u = datasize; fwrite(&u, 4, 1, f);
    for (i = 0; i < frames; i++) {
        v = (int)(pcm[i] * 32767.f);
        if (v > 32767) v = 32767; if (v < -32768) v = -32768;
        s = (unsigned short)(short)v;
        fwrite(&s, 2, 1, f);
    }
    fclose(f);
}

int main(int argc, char **argv)
{
    CGLPixelFormatAttribute attrs[] = { kCGLPFAOpenGLProfile, (CGLPixelFormatAttribute)kCGLOGLPVersion_GL4_Core,
                                        kCGLPFAAccelerated, (CGLPixelFormatAttribute)0 };
    CGLPixelFormatObj pf; CGLContextObj ctx; GLint npf;
    GLuint fbo, color, depth;
    unsigned char *pixels;
    float *song;
    int song_len, i;
    char path[1024];

    if (argc < 3) { fprintf(stderr, "usage: %s out_dir ms [ms...]\n", argv[0]); return 1; }

    if (CGLChoosePixelFormat(attrs, &pf, &npf) != kCGLNoError || !pf) { fprintf(stderr, "no pixel format\n"); return 1; }
    if (CGLCreateContext(pf, NULL, &ctx) != kCGLNoError) { fprintf(stderr, "no context\n"); return 1; }
    CGLDestroyPixelFormat(pf);
    CGLSetCurrentContext(ctx);
    printf("GL: %s / %s\n", glGetString(GL_RENDERER), glGetString(GL_VERSION));

    glGenTextures(1, &color);
    glBindTexture(GL_TEXTURE_2D, color);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, W, H, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
    glGenRenderbuffers(1, &depth);
    glBindRenderbuffer(GL_RENDERBUFFER, depth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, W, H);
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) { fprintf(stderr, "fbo incomplete\n"); return 1; }

    if (!polar_init()) { fprintf(stderr, "polar_init failed\n"); return 1; }
    polar_set_framebuffer(fbo);
    pixels = malloc(W * H * 4);

    polar_draw_loading(W, H);
    glReadPixels(0, 0, W, H, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    snprintf(path, sizeof(path), "%s/loading.png", argv[1]);
    write_png(path, pixels, W, H);

    while (!polar_bake_step(16)) printf("bake %3.0f%%\r", polar_bake_progress() * 100.f);
    glFinish();
    printf("bake done            \n");

    for (i = 2; i < argc; i++) {
        long ms = atol(argv[i]);
        polar_frame(ms, W, H);
        glReadPixels(0, 0, W, H, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
        snprintf(path, sizeof(path), "%s/frame_%06ld.png", argv[1], ms);
        write_png(path, pixels, W, H);
        printf("%s (order %d)\n", path, mzk_order_at_ms(ms));
    }

    song_len = mzk_render_song(&song);
    snprintf(path, sizeof(path), "%s/song.wav", argv[1]);
    write_wav(path, song, song_len);
    printf("%s (%d frames, %.1f s)\n", path, song_len, song_len / (float)MZK_RATE);

    polar_end();
    CGLSetCurrentContext(NULL);
    CGLDestroyContext(ctx);
    return 0;
}
