/*
 * main_mac.m - macOS port of Polarfield (fuzzion, 2010).
 *
 * Replaces src/_windows/main_*.cpp + the DirectSound glue of mzk.c with:
 *   - a Cocoa window holding an OpenGL 4.1 core profile NSOpenGLView
 *   - a CoreAudio output AudioUnit streaming the pre-rendered PCM
 *
 * No third party dependencies.  ESC quits, `--fullscreen` starts full screen.
 */
#import <Cocoa/Cocoa.h>
#import <AudioToolbox/AudioToolbox.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include "polar_gl.h"
#include "polar.h"

/* ---- audio ------------------------------------------------------------- */

typedef struct {
    const float *buf;
    int len;
    int loop;
} clip_t;

static AudioUnit   audio_unit;
static clip_t      audio_clip;                 /* what the render thread plays */
static clip_t      audio_pending;              /* clip requested by the main thread */
static atomic_int  audio_switch;               /* 1 = pick up audio_pending */
static atomic_long audio_pos;                  /* frames played of the current clip */
static int         audio_pos_frames;

static OSStatus audio_render(void *inRefCon, AudioUnitRenderActionFlags *flags,
                             const AudioTimeStamp *ts, UInt32 bus, UInt32 nframes,
                             AudioBufferList *io)
{
    float *out = (float *)io->mBuffers[0].mData;
    UInt32 i;
    (void)inRefCon; (void)flags; (void)ts; (void)bus;

    if (atomic_exchange(&audio_switch, 0)) {
        audio_clip = audio_pending;
        audio_pos_frames = 0;
    }
    for (i = 0; i < nframes; i++) {
        float s = 0.f;
        if (audio_clip.buf) {
            if (audio_pos_frames >= audio_clip.len) {
                if (audio_clip.loop) audio_pos_frames = 0;
            }
            if (audio_pos_frames < audio_clip.len)
                s = audio_clip.buf[audio_pos_frames];
        }
        out[i * 2 + 0] = s;
        out[i * 2 + 1] = s;
        audio_pos_frames++;
    }
    atomic_store(&audio_pos, audio_pos_frames);
    return noErr;
}

static int audio_init(void)
{
    AudioComponentDescription desc = { kAudioUnitType_Output, kAudioUnitSubType_DefaultOutput,
                                       kAudioUnitManufacturer_Apple, 0, 0 };
    AudioComponent comp = AudioComponentFindNext(NULL, &desc);
    AudioStreamBasicDescription fmt = { 0 };
    AURenderCallbackStruct cb = { audio_render, NULL };

    if (!comp || AudioComponentInstanceNew(comp, &audio_unit) != noErr) return 0;

    fmt.mSampleRate       = MZK_RATE;
    fmt.mFormatID         = kAudioFormatLinearPCM;
    fmt.mFormatFlags      = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked;
    fmt.mChannelsPerFrame = 2;
    fmt.mBitsPerChannel   = 32;
    fmt.mBytesPerFrame    = 8;
    fmt.mFramesPerPacket  = 1;
    fmt.mBytesPerPacket   = 8;
    if (AudioUnitSetProperty(audio_unit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, 0,
                             &fmt, sizeof(fmt)) != noErr) return 0;
    if (AudioUnitSetProperty(audio_unit, kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input, 0,
                             &cb, sizeof(cb)) != noErr) return 0;
    if (AudioUnitInitialize(audio_unit) != noErr) return 0;
    if (AudioOutputUnitStart(audio_unit) != noErr) return 0;
    return 1;
}

static void audio_play(const float *buf, int len, int loop)
{
    audio_pending.buf = buf;
    audio_pending.len = len;
    audio_pending.loop = loop;
    atomic_store(&audio_switch, 1);
}

static long audio_ms(void)
{
    return (long)(atomic_load(&audio_pos) * 1000 / MZK_RATE);
}

static void audio_end(void)
{
    if (audio_unit) {
        AudioOutputUnitStop(audio_unit);
        AudioUnitUninitialize(audio_unit);
        AudioComponentInstanceDispose(audio_unit);
        audio_unit = 0;
    }
}

/* ---- view ---------------------------------------------------------------- */

@interface PolarView : NSOpenGLView
{
    NSTimer *timer;
    float   *song, *loading;
    int      song_len, loading_len;
    int      baked;
    int      failed;
}
@end

@implementation PolarView

- (instancetype)initWithFrame:(NSRect)frame
{
    NSOpenGLPixelFormatAttribute attrs[] = {
        NSOpenGLPFAOpenGLProfile, NSOpenGLProfileVersion4_1Core,
        NSOpenGLPFADoubleBuffer,
        NSOpenGLPFAColorSize, 24,
        NSOpenGLPFADepthSize, 24,
        NSOpenGLPFAAccelerated,
        0
    };
    NSOpenGLPixelFormat *pf = [[NSOpenGLPixelFormat alloc] initWithAttributes:attrs];
    self = [super initWithFrame:frame pixelFormat:pf];
    if (self) [self setWantsBestResolutionOpenGLSurface:YES];
    return self;
}

- (BOOL)acceptsFirstResponder { return YES; }

- (void)keyDown:(NSEvent *)event
{
    if ([event keyCode] == 53) [NSApp terminate:nil];      /* ESC */
}

- (void)prepareOpenGL
{
    GLint vsync = 1;
    [super prepareOpenGL];
    [[self openGLContext] makeCurrentContext];
    [[self openGLContext] setValues:&vsync forParameter:NSOpenGLContextParameterSwapInterval];

    if (!polar_init()) {
        fprintf(stderr, "polar_init() failed\n");
        failed = 1;
        return;
    }
    song_len    = mzk_render_song(&song);
    loading_len = mzk_render_loading(&loading, 4);

    if (!audio_init()) fprintf(stderr, "audio init failed, running silent\n");
    audio_play(loading, loading_len, 1);              /* the thread in mzk.c played pattern 0 while loading */

    timer = [NSTimer timerWithTimeInterval:1.0 / 240.0 target:self selector:@selector(tick:) userInfo:nil repeats:YES];
    [[NSRunLoop currentRunLoop] addTimer:timer forMode:NSRunLoopCommonModes];
}

- (void)reshape
{
    [super reshape];
    [[self openGLContext] update];
}

- (void)tick:(NSTimer *)t
{
    NSRect px = [self convertRectToBacking:[self bounds]];
    int w = (int)px.size.width, h = (int)px.size.height;
    (void)t;
    if (failed || w <= 0 || h <= 0) return;

    [[self openGLContext] makeCurrentContext];
    CGLLockContext([[self openGLContext] CGLContextObj]);

    if (!baked) {
        baked = polar_bake_step(8);
        polar_draw_loading(w, h);
        if (baked) audio_play(song, song_len, 0);      /* mzk_start() */
    } else {
        long ms = audio_ms();
        polar_frame(ms, w, h);
        if (mzk_finished(ms)) {
            CGLUnlockContext([[self openGLContext] CGLContextObj]);
            [NSApp terminate:nil];
            return;
        }
    }
    [[self openGLContext] flushBuffer];
    CGLUnlockContext([[self openGLContext] CGLContextObj]);
}

- (void)dealloc
{
    [timer invalidate];
    free(song);
    free(loading);
}

@end

/* ---- app ------------------------------------------------------------------ */

@interface PolarAppDelegate : NSObject <NSApplicationDelegate>
@end

@implementation PolarAppDelegate
- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)app { (void)app; return YES; }
- (void)applicationWillTerminate:(NSNotification *)n { (void)n; audio_end(); }
@end

int main(int argc, const char **argv)
{
    int fullscreen = 0, i;
    for (i = 1; i < argc; i++)
        if (!strcmp(argv[i], "--fullscreen") || !strcmp(argv[i], "-f")) fullscreen = 1;

    @autoreleasepool {
        NSApplication *app = [NSApplication sharedApplication];
        PolarAppDelegate *delegate = [PolarAppDelegate new];
        NSRect rect = NSMakeRect(0, 0, 1024, 768);
        NSWindowStyleMask style = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                                  NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable;
        NSWindow *window = [[NSWindow alloc] initWithContentRect:rect styleMask:style
                                                         backing:NSBackingStoreBuffered defer:NO];
        PolarView *view = [[PolarView alloc] initWithFrame:rect];

        /* a minimal menu so Cmd+Q works */
        NSMenu *bar = [NSMenu new];
        NSMenuItem *appItem = [NSMenuItem new];
        NSMenu *appMenu = [NSMenu new];
        [appMenu addItemWithTitle:@"Quit" action:@selector(terminate:) keyEquivalent:@"q"];
        [appItem setSubmenu:appMenu];
        [bar addItem:appItem];
        [app setMainMenu:bar];

        [app setActivationPolicy:NSApplicationActivationPolicyRegular];
        [app setDelegate:delegate];
        [window setTitle:@"Polarfield - fuzzion"];
        [window setContentView:view];
        [window setContentAspectRatio:NSMakeSize(4, 3)];
        [window center];
        [window makeFirstResponder:view];
        [window makeKeyAndOrderFront:nil];
        if (fullscreen) [window toggleFullScreen:nil];
        [app activateIgnoringOtherApps:YES];
        [app run];
    }
    return 0;
}
