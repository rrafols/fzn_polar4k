/*
 * polar_gl.h - picks the right OpenGL header for the platform.
 *
 * The core only uses the intersection of OpenGL 3.3 core and OpenGL ES 3.0
 * (WebGL2): VAOs, VBOs, GLSL 330/300es, FBOs, instancing, glReadPixels.
 */
#ifndef POLAR_GL_H
#define POLAR_GL_H

#if defined(__EMSCRIPTEN__)
#  include <GLES3/gl3.h>
#  define GLSL_HEADER "#version 300 es\nprecision highp float;\nprecision highp int;\nprecision highp sampler2D;\n"
#elif defined(__APPLE__)
#  define GL_SILENCE_DEPRECATION 1
#  include <OpenGL/gl3.h>
#  define GLSL_HEADER "#version 330 core\n"
#else
#  include <GL/glcorearb.h>
#  define GLSL_HEADER "#version 330 core\n"
#endif

#endif
