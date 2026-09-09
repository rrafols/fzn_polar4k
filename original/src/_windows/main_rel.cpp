//--------------------------------------------------------------------------//
// iq / rgba  .  tiny codes  .  2008                                        //
//--------------------------------------------------------------------------//

#define WIN32_LEAN_AND_MEAN
#define WIN32_EXTRA_LEAN
#include <windows.h>
#include <mmsystem.h>
#include "../mzk.h"
#include "../config.h"
#include "../intro.h"

//----------------------------------------------------------------------------

#ifdef __cplusplus
extern "C" 
{
#endif
int  _fltused = 0;
#ifdef __cplusplus
}
#endif

#ifdef MODE_ULTRA
static const PIXELFORMATDESCRIPTOR pfd={
	0,						// Size Of This Pixel Format Descriptor
	0,						// Version Number
	PFD_SUPPORT_OPENGL |	// Format Must Support OpenGL
	PFD_DOUBLEBUFFER,		// Must Support Double Buffering
	0,						// Request An RGBA Format
	0,						// Select Our Color Depth
	0, 0, 0, 0, 0, 0,		// Color Bits Ignored
	0,						// No Alpha Buffer
	0,						// Shift Bit Ignored
	0,						// No Accumulation Buffer
	0, 0, 0, 0,				// Accumulation Bits Ignored
	0,						// 16Bit Z-Buffer (Depth Buffer)  
	0,						// No Stencil Buffer
	0,						// No Auxiliary Buffer
	0,						// Main Drawing Layer
	0,						// Reserved
	0, 0, 0					// Layer Masks Ignored
};
#else
static const PIXELFORMATDESCRIPTOR pfd={
	sizeof(PIXELFORMATDESCRIPTOR),	// Size Of This Pixel Format Descriptor
	1,								// Version Number
	PFD_DRAW_TO_WINDOW |			// Format Must Support Window
	PFD_SUPPORT_OPENGL |			// Format Must Support OpenGL
	PFD_DOUBLEBUFFER,				// Must Support Double Buffering
	PFD_TYPE_RGBA,					// Request An RGBA Format
	32,								// Select Our Color Depth
	0, 0, 0, 0, 0, 0,				// Color Bits Ignored
	0,								// No Alpha Buffer
	0,								// Shift Bit Ignored
	0,								// No Accumulation Buffer
	0, 0, 0, 0,						// Accumulation Bits Ignored
	32,								// 16Bit Z-Buffer (Depth Buffer)  
	0,								// No Stencil Buffer
	0,								// No Auxiliary Buffer
	PFD_MAIN_PLANE,					// Main Drawing Layer
	0,								// Reserved
	0, 0, 0							// Layer Masks Ignored
};
#endif

#ifdef MODE_FULLSCREEN
#ifdef MODE_ULTRA
static DEVMODE screenSettings = { {0},
#if _MSC_VER < 1400
    0,0,148,0,0x001c0000,{0},0,0,0,0,0,0,0,0,0,{0},0,32,SCREEN_XRES,SCREEN_YRES,0,0,      // Visual C++ 6.0
#else
    0,0,156,0,0x001c0000,{0},0,0,0,0,0,{0},0,32,SCREEN_XRES,SCREEN_YRES,{0}, 0,           // Visuatl Studio 2005
#endif
#if(WINVER >= 0x0400)
    0,0,0,0,0,0,
#if (WINVER >= 0x0500) || (_WIN32_WINNT >= 0x0400)
    0,0
#endif
#endif
    };
#else
DEVMODE		screenSettings;
#endif
#endif

void entrypoint( void )
{
#ifdef MODE_FULLSCREEN
#ifndef MODE_ULTRA
	screenSettings.dmSize=sizeof(screenSettings);
	screenSettings.dmFields=DM_BITSPERPEL|DM_PELSWIDTH|DM_PELSHEIGHT;
	screenSettings.dmPelsWidth=SCREEN_XRES;
	screenSettings.dmPelsHeight=SCREEN_YRES;
	screenSettings.dmBitsPerPel=32;
#endif
	ChangeDisplaySettings(&screenSettings,CDS_FULLSCREEN);
    ShowCursor( 0 );
#endif
    // create window
	HWND hWND = CreateWindow("edit", 0, WS_POPUP|WS_VISIBLE|WS_MAXIMIZE, 0,0,0,0,0,0,0,0);
    HDC hDC = GetDC(hWND);
    // initalize opengl
    SetPixelFormat(hDC, ChoosePixelFormat(hDC, &pfd), &pfd);
    wglMakeCurrent(hDC, wglCreateContext(hDC));

	    // init mzk
	mzk_init_loader(hWND);

    // init intro
    intro_init(hDC);

	mzk_start();

    // play intro
    do 
    {
        intro_frame( mzk_play() );
        //wglSwapLayerBuffers( hDC, WGL_SWAP_MAIN_PLANE );
		SwapBuffers( hDC );
//    }while ( !GetAsyncKeyState(VK_ESCAPE) && t<(MZK_DURATION*1000) );
    }while ( !GetAsyncKeyState(VK_ESCAPE));

#ifndef MODE_ULTRA
	mzk_end();
    ChangeDisplaySettings( 0, 0 );
    ShowCursor(1);
#endif
    ExitProcess(0);
}
