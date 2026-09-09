/*
 * polar_render.c - drawing and the ambient occlusion bake.
 *
 * The original baked per-vertex AO by, for every vertex of the high-res
 * scene, pointing a 70 degree camera along the vertex normal, rendering the
 * low-res scene in black on white into a 64x64 viewport and averaging the
 * pixels with glReadPixels.  That is ~170k tiny render+readback round trips.
 *
 * Here the same thing is done with instancing: one instanced draw renders
 * up to tiles*tiles hemisphere views into a big offscreen texture (each
 * instance builds its own look-at matrix in the vertex shader and gets
 * squeezed into its 64x64 tile), a reduction shader sums each tile, and a
 * single tiny glReadPixels fetches all averages.  Same maths, same numbers,
 * a few hundred draw calls instead of a few hundred thousand.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include "polar_gl.h"
#include "polar_math.h"
#include "polar_internal.h"
#include "polar.h"

#define BAKE_TILE      64          /* pantsize in the original */
#define BAKE_MAX_TILES 32          /* 32x32 tiles = 2048x2048 texture, 1024 views per readback */
#define BAKE_GROUP     8           /* instances per draw call; the draw is scissored to their
                                      strip of tiles so big triangles don't get rasterised (and
                                      discarded) over the whole 2048x2048 texture */

typedef struct { GLuint vbo, ibo, vao, vao_bake; } gpu_mesh_t;

static gpu_mesh_t g_normal[SCENES], g_simple[SCENES], g_loading;
static mesh_t     loading_mesh;

static GLuint prog_draw, prog_bake, prog_reduce;
static GLint  u_draw_mvp, u_bake_proj, u_bake_tiles, u_bake_tilepx, u_bake_base, u_reduce_tex;
static GLuint inst_vbo, fullscreen_vbo, fullscreen_vao;
static GLuint bake_fbo, bake_tex, reduce_fbo, reduce_tex;
static int    tiles;                     /* tiles per row of the bake texture */
static float *inst_data;
static unsigned char *readback;

static int bake_scene, bake_pos, bake_done, bake_total, bake_count;
static GLuint target_fbo = 0;          /* where polar_frame/polar_draw_loading draw */

/* ---- shaders ----------------------------------------------------------- */

static const char *vs_draw =
    "layout(location=0) in vec3 a_pos;\n"
    "layout(location=1) in float a_col;\n"
    "uniform mat4 u_mvp;\n"
    "out float v_col;\n"
    "void main(){ gl_Position = u_mvp * vec4(a_pos, 1.0); v_col = a_col; }\n";

static const char *fs_draw =
    "in float v_col;\n"
    "out vec4 o_color;\n"
    "void main(){ o_color = vec4(v_col, v_col, v_col, 1.0); }\n";

/* gluLookAt(pos, pos+normal, (0.01,1,0)) per instance, then remap the clip
 * space rectangle of this view into its tile. */
static const char *vs_bake =
    "layout(location=0) in vec3 a_pos;\n"
    "layout(location=1) in vec3 i_eye;\n"
    "layout(location=2) in vec3 i_nrm;\n"
    "uniform mat4 u_proj;\n"
    "uniform float u_tiles;\n"
    "uniform float u_tilepx;\n"
    "uniform float u_base;\n"
    "flat out vec4 v_rect;\n"
    "void main(){\n"
    "  vec3 f = normalize(i_nrm);\n"
    "  vec3 up = normalize(vec3(0.01, 1.0, 0.0));\n"
    "  vec3 s = normalize(cross(f, up));\n"
    "  vec3 u = cross(s, f);\n"
    "  vec3 p = a_pos - i_eye;\n"
    "  vec4 c = u_proj * vec4(dot(s, p), dot(u, p), -dot(f, p), 1.0);\n"
    "  float id = u_base + float(gl_InstanceID);\n"
    "  float ty = floor((id + 0.5) / u_tiles);\n"
    "  float tx = id - ty * u_tiles;\n"
    "  float sc = 1.0 / u_tiles;\n"
    "  vec2 center = (vec2(tx, ty) + 0.5) * 2.0 * sc - 1.0;\n"
    "  c.xy = c.xy * sc + c.w * center;\n"
    "  gl_Position = c;\n"
    "  v_rect = vec4(vec2(tx, ty) * u_tilepx, (vec2(tx, ty) + 1.0) * u_tilepx);\n"
    "}\n";

static const char *fs_bake =
    "flat in vec4 v_rect;\n"
    "out vec4 o_color;\n"
    "void main(){\n"
    "  if (gl_FragCoord.x < v_rect.x || gl_FragCoord.y < v_rect.y ||\n"
    "      gl_FragCoord.x >= v_rect.z || gl_FragCoord.y >= v_rect.w) discard;\n"
    "  o_color = vec4(0.0, 0.0, 0.0, 1.0);\n"
    "}\n";

static const char *vs_reduce =
    "layout(location=0) in vec2 a_pos;\n"
    "void main(){ gl_Position = vec4(a_pos, 0.0, 1.0); }\n";

/* sums the blue channel of one 64x64 tile (the original read GL_BLUE) and
 * encodes the 20 bit integer sum into three bytes */
static const char *fs_reduce =
    "uniform sampler2D u_tex;\n"
    "out vec4 o_color;\n"
    "void main(){\n"
    "  ivec2 base = ivec2(gl_FragCoord.xy) * 64;\n"
    "  float s = 0.0;\n"
    "  for (int y = 0; y < 64; y++)\n"
    "    for (int x = 0; x < 64; x++)\n"
    "      s += floor(texelFetch(u_tex, base + ivec2(x, y), 0).b * 255.0 + 0.5);\n"
    "  float hi = floor(s / 65536.0);\n"
    "  float mid = floor((s - hi * 65536.0) / 256.0);\n"
    "  float lo = s - hi * 65536.0 - mid * 256.0;\n"
    "  o_color = vec4(hi / 255.0, mid / 255.0, lo / 255.0, 1.0);\n"
    "}\n";

static GLuint compile(GLenum type, const char *src)
{
    const char *parts[2] = { GLSL_HEADER, src };
    GLuint sh = glCreateShader(type);
    GLint ok = 0;
    glShaderSource(sh, 2, parts, 0);
    glCompileShader(sh);
    glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[2048];
        glGetShaderInfoLog(sh, sizeof(log), 0, log);
        fprintf(stderr, "shader compile error:\n%s\n", log);
        glDeleteShader(sh);
        return 0;
    }
    return sh;
}

static GLuint link(const char *vs, const char *fs)
{
    GLuint v = compile(GL_VERTEX_SHADER, vs);
    GLuint f = compile(GL_FRAGMENT_SHADER, fs);
    GLuint p;
    GLint ok = 0;
    if (!v || !f) return 0;
    p = glCreateProgram();
    glAttachShader(p, v);
    glAttachShader(p, f);
    glLinkProgram(p);
    glDeleteShader(v);
    glDeleteShader(f);
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[2048];
        glGetProgramInfoLog(p, sizeof(log), 0, log);
        fprintf(stderr, "program link error:\n%s\n", log);
        glDeleteProgram(p);
        return 0;
    }
    return p;
}

/* ---- GL meshes ---------------------------------------------------------- */

static void upload_mesh(gpu_mesh_t *g, const mesh_t *m, int with_bake_vao)
{
    glGenBuffers(1, &g->vbo);
    glGenBuffers(1, &g->ibo);
    glBindBuffer(GL_ARRAY_BUFFER, g->vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(sizeof(vert_t) * m->nverts), m->verts, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g->ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)(sizeof(unsigned) * m->nidx), m->idx, GL_STATIC_DRAW);

    /* VAO for normal drawing: position + grey */
    glGenVertexArrays(1, &g->vao);
    glBindVertexArray(g->vao);
    glBindBuffer(GL_ARRAY_BUFFER, g->vbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g->ibo);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(vert_t), (void *)offsetof(vert_t, pos));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, sizeof(vert_t), (void *)offsetof(vert_t, col));

    /* VAO for the instanced bake: position + per instance eye/normal */
    if (with_bake_vao) {
        glGenVertexArrays(1, &g->vao_bake);
        glBindVertexArray(g->vao_bake);
        glBindBuffer(GL_ARRAY_BUFFER, g->vbo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g->ibo);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(vert_t), (void *)offsetof(vert_t, pos));
        glBindBuffer(GL_ARRAY_BUFFER, inst_vbo);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *)0);
        glVertexAttribDivisor(1, 1);
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *)(3 * sizeof(float)));
        glVertexAttribDivisor(2, 1);
    }
    glBindVertexArray(0);
}

static void delete_mesh(gpu_mesh_t *g)
{
    if (g->vao) glDeleteVertexArrays(1, &g->vao);
    if (g->vao_bake) glDeleteVertexArrays(1, &g->vao_bake);
    if (g->vbo) glDeleteBuffers(1, &g->vbo);
    if (g->ibo) glDeleteBuffers(1, &g->ibo);
    memset(g, 0, sizeof(*g));
}

static int make_fbo(GLuint *fbo, GLuint *tex, int w, int h)
{
    glGenTextures(1, tex);
    glBindTexture(GL_TEXTURE_2D, *tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glGenFramebuffers(1, fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, *fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, *tex, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        fprintf(stderr, "framebuffer %dx%d incomplete\n", w, h);
        return 0;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return 1;
}

/* ---- public ------------------------------------------------------------- */

int polar_init(void)
{
    static const float fullscreen_tri[6] = { -1.f, -1.f, 3.f, -1.f, -1.f, 3.f };
    GLint maxtex = 0;
    int i;

    if (!scene_generate()) return 0;

    prog_draw   = link(vs_draw, fs_draw);
    prog_bake   = link(vs_bake, fs_bake);
    prog_reduce = link(vs_reduce, fs_reduce);
    if (!prog_draw || !prog_bake || !prog_reduce) return 0;
    u_draw_mvp    = glGetUniformLocation(prog_draw, "u_mvp");
    u_bake_proj   = glGetUniformLocation(prog_bake, "u_proj");
    u_bake_tiles  = glGetUniformLocation(prog_bake, "u_tiles");
    u_bake_tilepx = glGetUniformLocation(prog_bake, "u_tilepx");
    u_bake_base   = glGetUniformLocation(prog_bake, "u_base");
    u_reduce_tex  = glGetUniformLocation(prog_reduce, "u_tex");

    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxtex);
    tiles = maxtex / BAKE_TILE;
    if (tiles > BAKE_MAX_TILES) tiles = BAKE_MAX_TILES;
    tiles -= tiles % BAKE_GROUP;             /* a draw group must not wrap to the next row */
    if (tiles < BAKE_GROUP) tiles = BAKE_GROUP;

    inst_data = (float *)malloc(sizeof(float) * 6 * tiles * tiles);
    readback  = (unsigned char *)malloc(4 * tiles * tiles);
    glGenBuffers(1, &inst_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, inst_vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(sizeof(float) * 6 * tiles * tiles), 0, GL_DYNAMIC_DRAW);

    glGenVertexArrays(1, &fullscreen_vao);
    glBindVertexArray(fullscreen_vao);
    glGenBuffers(1, &fullscreen_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, fullscreen_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(fullscreen_tri), fullscreen_tri, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, (void *)0);
    glBindVertexArray(0);

    if (!make_fbo(&bake_fbo, &bake_tex, tiles * BAKE_TILE, tiles * BAKE_TILE)) return 0;
    if (!make_fbo(&reduce_fbo, &reduce_tex, tiles, tiles)) return 0;

    bake_total = 0;
    for (i = 0; i < SCENES; i++) {
        upload_mesh(&g_normal[i], &sc_normal[i], 0);
        upload_mesh(&g_simple[i], &sc_simple[i], 1);
        bake_total += sc_normal[i].nbake;
    }

    /* the "Loading" text: glTranslatef(-1, -2.5, -4) at 1 em */
    loading_mesh.verts = (vert_t *)malloc(sizeof(vert_t) * 4096);
    loading_mesh.idx   = (unsigned *)malloc(sizeof(unsigned) * 4096);
    loading_mesh.nverts = loading_mesh.nidx = 0;
    mesh_add_text(&loading_mesh, 4096, "Loading", -1.f, -2.5f, -4.f, 1.f);
    upload_mesh(&g_loading, &loading_mesh, 0);

    bake_scene = bake_pos = bake_done = bake_count = 0;
    return 1;
}

float polar_bake_progress(void)
{
    return bake_total ? (float)bake_count / (float)bake_total : 1.f;
}

int polar_bake_step(int nbatches)
{
    mat4 proj;
    m_identity(&proj);
    m_perspective(&proj, 70.f, 1.f, 0.2f, 30.f);        /* gluPerspective(70,1,.2,30) */

    while (nbatches-- > 0 && !bake_done) {
        mesh_t *m = &sc_normal[bake_scene];
        int count = m->nbake - bake_pos, i;
        if (count > tiles * tiles) count = tiles * tiles;

        if (count <= 0) {
            /* scene finished: push the baked colours to the GPU */
            glBindBuffer(GL_ARRAY_BUFFER, g_normal[bake_scene].vbo);
            glBufferSubData(GL_ARRAY_BUFFER, 0, (GLsizeiptr)(sizeof(vert_t) * m->nverts), m->verts);
            bake_pos = 0;
            if (++bake_scene >= SCENES) bake_done = 1;
            continue;
        }

        for (i = 0; i < count; i++) {
            const vert_t *v = &m->verts[bake_pos + i];
            memcpy(inst_data + i * 6, v->pos, sizeof(float) * 3);
            memcpy(inst_data + i * 6 + 3, v->nrm, sizeof(float) * 3);
        }
        glBindBuffer(GL_ARRAY_BUFFER, inst_vbo);
        glBufferSubData(GL_ARRAY_BUFFER, 0, (GLsizeiptr)(sizeof(float) * 6 * count), inst_data);

        /* 1. render `count` hemisphere views of the low-res scene */
        glBindFramebuffer(GL_FRAMEBUFFER, bake_fbo);
        glViewport(0, 0, tiles * BAKE_TILE, tiles * BAKE_TILE);
        glDisable(GL_DEPTH_TEST);              /* the original bake ran without depth test */
        glDisable(GL_CULL_FACE);
        glClearColor(1.f, 1.f, 1.f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(prog_bake);
        glUniformMatrix4fv(u_bake_proj, 1, GL_FALSE, proj.m);
        glUniform1f(u_bake_tiles, (float)tiles);
        glUniform1f(u_bake_tilepx, (float)BAKE_TILE);
        glBindVertexArray(g_simple[bake_scene].vao_bake);
        glBindBuffer(GL_ARRAY_BUFFER, inst_vbo);
        glEnable(GL_SCISSOR_TEST);
        for (i = 0; i < count; i += BAKE_GROUP) {
            int n = count - i < BAKE_GROUP ? count - i : BAKE_GROUP;
            int tx = i % tiles, ty = i / tiles;
            /* instanced attributes have no base instance in ES3, so re-point them */
            glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *)(size_t)(i * 6 * sizeof(float)));
            glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *)(size_t)(i * 6 * sizeof(float) + 3 * sizeof(float)));
            glUniform1f(u_bake_base, (float)i);
            glScissor(tx * BAKE_TILE, ty * BAKE_TILE, n * BAKE_TILE, BAKE_TILE);
            glDrawElementsInstanced(GL_TRIANGLES, sc_simple[bake_scene].nidx, GL_UNSIGNED_INT, 0, n);
        }
        glDisable(GL_SCISSOR_TEST);

        /* 2. sum every tile */
        glBindFramebuffer(GL_FRAMEBUFFER, reduce_fbo);
        glViewport(0, 0, tiles, tiles);
        glUseProgram(prog_reduce);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, bake_tex);
        glUniform1i(u_reduce_tex, 0);
        glBindVertexArray(fullscreen_vao);
        glDrawArrays(GL_TRIANGLES, 0, 3);

        /* 3. read the sums back: colour = sum / (64*64*256), as in the original */
        glReadPixels(0, 0, tiles, tiles, GL_RGBA, GL_UNSIGNED_BYTE, readback);
        for (i = 0; i < count; i++) {
            const unsigned char *px = readback + i * 4;
            int sum = (px[0] << 16) | (px[1] << 8) | px[2];
            m->verts[bake_pos + i].col = (float)sum / (float)(BAKE_TILE * BAKE_TILE * 256);
        }
        bake_pos += count;
        bake_count += count;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, target_fbo);
    glBindVertexArray(0);
    return bake_done;
}

void polar_draw_loading(int width, int height)
{
    mat4 mvp;
    m_identity(&mvp);
    m_perspective(&mvp, 70.f, 1.f, 0.2f, 30.f);   /* projection left over from init, 1:1 aspect */

    glBindFramebuffer(GL_FRAMEBUFFER, target_fbo);
    glViewport(0, 0, width, height);
    glDisable(GL_DEPTH_TEST);
    glClearColor(1.f, 1.f, 1.f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glUseProgram(prog_draw);
    glUniformMatrix4fv(u_draw_mvp, 1, GL_FALSE, mvp.m);
    glBindVertexArray(g_loading.vao);
    glDrawElements(GL_TRIANGLES, loading_mesh.nidx, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

void polar_frame(long ms, int width, int height)
{
    int order = mzk_order_at_ms(ms);
    int demostate;
    float p0, p1;
    mat4 mvp;

    demostate = order % 7;
    if (order < 46) demostate = 5;
    if (order < 38) demostate = 6;
    if (order < 34) demostate = 1;
    if (order < 29) demostate = 6;
    if (order < 22) demostate = 3;
    if (order < 14) demostate = 2;
    if (order < 10) demostate = 0;

    m_identity(&mvp);
    m_perspective(&mvp, 60.f, (float)width / (float)height, 0.2f, 30.f);

    /* camera; the fall-throughs are intentional and match the original */
    p1 = p0 = (float)ms * 0.0008f;
    switch (demostate) {
    case 6:
        p1 += (float)order;
        /* fall through */
    case 1:
    case 4:
        p0 += (float)order;
        /* fall through */
    case 2:
        m_lookat(&mvp, 12.f * fsin(p0), 5.f * (1 - fcos(p0)), -12.f * fcos(p1), 0, 0, 0, 0, 1, 0);
        break;
    default:
        m_lookat(&mvp, -2 + 5 * fsin((float)ms * 0.0005f), 2.f * fcos((float)ms * 0.0001f), 8.f, 0, 0, 0, 0, 1, 0);
        break;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, target_fbo);
    glViewport(0, 0, width, height);
    glClearColor(1.f, 1.f, 1.f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glUseProgram(prog_draw);
    glUniformMatrix4fv(u_draw_mvp, 1, GL_FALSE, mvp.m);
    glBindVertexArray(g_normal[demostate].vao);
    glDrawElements(GL_TRIANGLES, sc_normal[demostate].nidx, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

void polar_set_framebuffer(unsigned fbo)
{
    target_fbo = (GLuint)fbo;
}

void polar_end(void)
{
    int i;
    for (i = 0; i < SCENES; i++) {
        delete_mesh(&g_normal[i]);
        delete_mesh(&g_simple[i]);
    }
    delete_mesh(&g_loading);
    if (prog_draw) glDeleteProgram(prog_draw);
    if (prog_bake) glDeleteProgram(prog_bake);
    if (prog_reduce) glDeleteProgram(prog_reduce);
    if (bake_fbo) glDeleteFramebuffers(1, &bake_fbo);
    if (reduce_fbo) glDeleteFramebuffers(1, &reduce_fbo);
    if (bake_tex) glDeleteTextures(1, &bake_tex);
    if (reduce_tex) glDeleteTextures(1, &reduce_tex);
    if (inst_vbo) glDeleteBuffers(1, &inst_vbo);
    if (fullscreen_vbo) glDeleteBuffers(1, &fullscreen_vbo);
    if (fullscreen_vao) glDeleteVertexArrays(1, &fullscreen_vao);
    free(inst_data); free(readback);
    free(loading_mesh.verts); free(loading_mesh.idx);
    scene_free();
}
