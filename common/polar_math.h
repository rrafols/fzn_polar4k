/*
 * polar_math.h - tiny math helpers replacing minimath.c / ext.cpp x87 asm
 * and the fixed-function OpenGL matrix stack the original relied on.
 *
 * Matrices are 4x4 column-major floats, exactly like glGetFloatv(GL_MODELVIEW_MATRIX).
 * All m_* "stack" operations post-multiply the current matrix, matching
 * glTranslatef / glRotatef / glScalef / gluLookAt semantics.
 */
#ifndef POLAR_MATH_H
#define POLAR_MATH_H

#include <math.h>
#include <string.h>

#define POLAR_PI 3.1415926535897932384626433832795f

/* ---- deterministic LCG identical to the original myRandFloat() -------- */
extern unsigned int holdrand;

static inline float myRandFloat(void)
{
    holdrand = holdrand * 214013u + 2531011u;
    return (float)(short)(holdrand & 0xffffu) / 32767.f;
}

static inline float fsin(float v) { return sinf(v); }
static inline float fcos(float v) { return cosf(v); }
/* x87 fistp with default control word == round to nearest even */
static inline int float2int(float v) { return (int)lrintf(v); }

/* ---- 4x4 column major matrices ---------------------------------------- */
typedef struct { float m[16]; } mat4;

static inline void m_identity(mat4 *a)
{
    memset(a->m, 0, sizeof(a->m));
    a->m[0] = a->m[5] = a->m[10] = a->m[15] = 1.f;
}

/* a = a * b */
static inline void m_mul(mat4 *a, const mat4 *b)
{
    float r[16];
    int i, j, k;
    for (i = 0; i < 4; i++)
        for (j = 0; j < 4; j++) {
            float s = 0.f;
            for (k = 0; k < 4; k++) s += a->m[i + k * 4] * b->m[k + j * 4];
            r[i + j * 4] = s;
        }
    memcpy(a->m, r, sizeof(r));
}

static inline void m_translate(mat4 *a, float x, float y, float z)
{
    mat4 t; m_identity(&t);
    t.m[12] = x; t.m[13] = y; t.m[14] = z;
    m_mul(a, &t);
}

static inline void m_scale(mat4 *a, float x, float y, float z)
{
    mat4 t; m_identity(&t);
    t.m[0] = x; t.m[5] = y; t.m[10] = z;
    m_mul(a, &t);
}

/* glRotatef: angle in degrees, axis is normalised like GL does */
static inline void m_rotate(mat4 *a, float deg, float x, float y, float z)
{
    float len = sqrtf(x * x + y * y + z * z);
    float rad = deg * POLAR_PI / 180.f;
    float c = cosf(rad), s = sinf(rad), ic;
    mat4 t;
    if (len == 0.f) return;
    x /= len; y /= len; z /= len;
    ic = 1.f - c;
    m_identity(&t);
    t.m[0] = x * x * ic + c;     t.m[4] = x * y * ic - z * s; t.m[8]  = x * z * ic + y * s;
    t.m[1] = y * x * ic + z * s; t.m[5] = y * y * ic + c;     t.m[9]  = y * z * ic - x * s;
    t.m[2] = x * z * ic - y * s; t.m[6] = y * z * ic + x * s; t.m[10] = z * z * ic + c;
    m_mul(a, &t);
}

/* gluPerspective */
static inline void m_perspective(mat4 *a, float fovy_deg, float aspect, float znear, float zfar)
{
    float f = 1.f / tanf(fovy_deg * POLAR_PI / 360.f);
    mat4 t;
    memset(t.m, 0, sizeof(t.m));
    t.m[0]  = f / aspect;
    t.m[5]  = f;
    t.m[10] = (zfar + znear) / (znear - zfar);
    t.m[11] = -1.f;
    t.m[14] = (2.f * zfar * znear) / (znear - zfar);
    m_mul(a, &t);
}

/* gluLookAt */
static inline void m_lookat(mat4 *a, float ex, float ey, float ez,
                            float cx, float cy, float cz,
                            float ux, float uy, float uz)
{
    float fx = cx - ex, fy = cy - ey, fz = cz - ez;
    float len = sqrtf(fx * fx + fy * fy + fz * fz);
    float sx, sy, sz, tx, ty, tz;
    mat4 t;
    if (len != 0.f) { fx /= len; fy /= len; fz /= len; }
    len = sqrtf(ux * ux + uy * uy + uz * uz);
    if (len != 0.f) { ux /= len; uy /= len; uz /= len; }
    /* s = f x up */
    sx = fy * uz - fz * uy; sy = fz * ux - fx * uz; sz = fx * uy - fy * ux;
    len = sqrtf(sx * sx + sy * sy + sz * sz);
    if (len != 0.f) { sx /= len; sy /= len; sz /= len; }
    /* u = s x f */
    tx = sy * fz - sz * fy; ty = sz * fx - sx * fz; tz = sx * fy - sy * fx;
    m_identity(&t);
    t.m[0] = sx;  t.m[4] = sy;  t.m[8]  = sz;
    t.m[1] = tx;  t.m[5] = ty;  t.m[9]  = tz;
    t.m[2] = -fx; t.m[6] = -fy; t.m[10] = -fz;
    m_mul(a, &t);
    m_translate(a, -ex, -ey, -ez);
}

#endif
