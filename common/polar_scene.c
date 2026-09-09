/*
 * polar_scene.c - geometry generation.
 *
 * This is a straight port of intro_init_polar()/add_object()/add_cube() from
 * the original intro.cpp.  The only differences:
 *   - the fixed-function matrix stack is replaced by polar_math.h
 *   - quads are emitted as two triangles (WebGL has no GL_QUADS)
 *   - the outline font text that wglUseFontOutlines() produced at runtime is
 *     appended to each scene from pre-tessellated glyphs (glyphs.h)
 */
#include <stdlib.h>
#include <string.h>
#include "polar_math.h"
#include "polar_internal.h"
#include "glyphs.h"

unsigned int holdrand = 0;

#define OBJ_PLANE  0
#define OBJ_SPHERE 1
#define OBJ_CUERNO 2
#define OBJ_LOGO   3

#define MAX_VERTS 50000
#define MAX_IDX   (37500 * 6)      /* 150000 quad indices in the original */

typedef struct {
    const char *str;
    float x, y;
} str_text;

static const str_text scene_texts[7] = {
    { "fuzzion", -2.f,   0.f  },
    { ".chucho", -2.f,   1.4f },
    { "bp",       0.2f,  1.4f },
    { "Ufix",    -5.f,  -0.6f },
    { "Pain",     2.0f, -0.6f },
    { "Wonder",  -1.2f, -2.2f },
    { "Loading", -1.f,  -2.5f }
};
static const int text_numbers[SCENES] = { 1, 0, 0, 1, 0, 4, 0 };

static const short fourierx1s[34] = {
    32767, -5596, 6098, -6022, 10259, 2860, -1323, 1504, 1723, -1815, -766, -45, -219,
    822, 62, -304, 186, -147, 140, 173, -317, -117, -29, 22, 291, 117, -108, -98,
    -124, -58, 117, 72, -26, 39
};

mesh_t sc_normal[SCENES];
mesh_t sc_simple[SCENES];

/* ---- generation state (globals, as in the original) -------------------- */
static mat4     M;                  /* the "modelview" matrix */
static vert_t  *scratch_verts;
static unsigned *scratch_idx;
static mesh_t  *pscene;
static int      simple_pass;        /* icnt1 in the original */
static int      res_u, res_v, res_ulow, res_vlow, res_obj;

static void add_object(void)
{
    const float *damatrix = M.m;
    int cnt_u, cnt_v, cnt_vrt, cnt_comp, ktrx, ktry;
    float cal_u, cal_v, facc1, facc2;
    float ptmpvert[8], ptmppos[9];
    vert_t *pvert;
    unsigned *pquad;

    if (simple_pass) {
        res_u = res_ulow;
        res_v = res_vlow;
    }
    if (pscene->nverts + (res_u + 1) * (res_v + 1) > MAX_VERTS) return;
    if (pscene->nidx + res_u * res_v * 6 > MAX_IDX) return;

    pvert = pscene->verts + pscene->nverts;
    pquad = pscene->idx + pscene->nidx;

    for (cnt_v = 0; cnt_v <= res_v; cnt_v++)
        for (cnt_u = 0; cnt_u <= res_u; cnt_u++) {
            cnt_comp = 0;
            for (cnt_vrt = 0; cnt_vrt < 3; cnt_vrt++) {
                /* res==0 happens in scene 6 (a single degenerate vertex) */
                cal_u = res_u ? (float)cnt_u / res_u : 0.f;
                cal_v = res_v ? (float)cnt_v / res_v : 0.f;
                switch (cnt_vrt) {
                case 1: if (res_v) cal_v += 0.1f / res_v; break;
                case 2: if (res_u) cal_u += 0.1f / res_u; break;
                }

                /* calculate coordinates depending on object type */
                switch (res_obj) {
                case OBJ_PLANE:
                    ptmpvert[0] = cal_u * 2.f - 1.f;
                    ptmpvert[1] = 1.f;
                    ptmpvert[2] = cal_v * 2.f - 1.f;
                    break;
                case OBJ_CUERNO:
                    facc1 = 2.f * POLAR_PI * cal_u;
                    facc2 = (2.f + fcos(facc1) * (float)(1 - cal_v));
                    ptmpvert[0] = facc2 * fcos(POLAR_PI / 2.f * cal_v) - 2.f;
                    ptmpvert[1] = facc2 * fsin(POLAR_PI / 2.f * cal_v);
                    ptmpvert[2] = fsin(facc1) * (float)(1 - cal_v);
                    break;
                case OBJ_LOGO:
                    for (ktrx = 0; ktrx < 8; ktrx++) ptmpvert[ktrx] = 0;
                    for (ktry = 2; ktry < 6; ktry += 2) {
                        ptmpvert[0] = 0.f;
                        for (ktrx = 0; ktrx <= 33; ktrx++) {
                            ptmpvert[0] += 2.f * POLAR_PI * cal_v;
                            ptmpvert[ktry + 1] = -ptmpvert[ktry + 1] - ((float)(fourierx1s[ktrx])) * fcos(ptmpvert[0]) / 32767;
                            ptmpvert[ktry] += ((float)(fourierx1s[ktrx])) * fsin(ptmpvert[0]) / 32767;
                            if (ktrx & 1) ptmpvert[0] += 2.f * POLAR_PI * cal_v;
                        }
                        cal_v += 0.1f / res_v;
                    }
                    ptmpvert[0] = ptmpvert[2] + (ptmpvert[5] - ptmpvert[3]) * fcos(2.f * POLAR_PI * cal_u) * 4.f;
                    ptmpvert[1] = ptmpvert[3] - (ptmpvert[4] - ptmpvert[2]) * fcos(2.f * POLAR_PI * cal_u) * 4.f;
                    ptmpvert[2] = fsin(2.f * POLAR_PI * cal_u) * 0.1f;
                    break;
                default:
                    ptmpvert[0] = ptmpvert[1] = ptmpvert[2] = 0.f;
                    break;
                }
                ptmpvert[3] = 1.f;

                /* transform all coordinates */
                for (ktrx = 0; ktrx < 3; ktrx++) {
                    ptmppos[cnt_comp] = 0.f;
                    for (ktry = 0; ktry < 4; ktry++)
                        ptmppos[cnt_comp] += damatrix[ktrx + ktry * 4] * ptmpvert[ktry];
                    cnt_comp++;
                }
            }

            for (ktrx = 0; ktrx < 3; ktrx++) pvert->pos[ktrx] = ptmppos[ktrx];

            /* generate normal depending on transformed coordinates */
            pvert->nrm[0] = (ptmppos[4] - ptmppos[1]) * (ptmppos[8] - ptmppos[2]) -
                            (ptmppos[5] - ptmppos[2]) * (ptmppos[7] - ptmppos[1]);
            pvert->nrm[1] = (ptmppos[5] - ptmppos[2]) * (ptmppos[6] - ptmppos[0]) -
                            (ptmppos[3] - ptmppos[0]) * (ptmppos[8] - ptmppos[2]);
            pvert->nrm[2] = (ptmppos[3] - ptmppos[0]) * (ptmppos[7] - ptmppos[1]) -
                            (ptmppos[4] - ptmppos[1]) * (ptmppos[6] - ptmppos[0]);
            pvert->col = 0.f;

            if (cnt_u != res_u && cnt_v != res_v) {
                unsigned a = pscene->nverts;
                unsigned b = pscene->nverts + (res_u + 1);
                unsigned c = pscene->nverts + 1 + (res_u + 1);
                unsigned d = pscene->nverts + 1;
                pquad[0] = a; pquad[1] = b; pquad[2] = c;
                pquad[3] = a; pquad[4] = c; pquad[5] = d;
                pquad += 6;
                pscene->nidx += 6;
            }
            pvert++;
            pscene->nverts++;
        }
}

static void add_cube(void)
{
    res_obj = OBJ_PLANE;
    res_ulow = res_vlow = 1;
    add_object();
    m_rotate(&M, 90, 1, 0, 0);
    add_object();
    m_rotate(&M, 90, 1, 0, 0);
    add_object();
    m_rotate(&M, 90, 1, 0, 0);
    add_object();
    m_rotate(&M, 90, 0, 0, 1);
    add_object();
    m_rotate(&M, 180, 0, 0, 1);
    add_object();
}

/* ---- text ------------------------------------------------------------- */

int mesh_add_text(mesh_t *m, int capacity, const char *str, float x, float y, float z, float scale)
{
    float pen = 0.f;
    for (; *str; str++) {
        const glyph_t *g = &glyph_table[(unsigned char)*str & 127];
        int i;
        if (m->nverts + g->count > capacity) return 0;
        for (i = 0; i < g->count; i++) {
            vert_t *v = m->verts + m->nverts;
            v->pos[0] = x + (pen + glyph_verts[(g->first + i) * 2 + 0]) * scale;
            v->pos[1] = y + glyph_verts[(g->first + i) * 2 + 1] * scale;
            v->pos[2] = z;
            v->nrm[0] = 0.f; v->nrm[1] = 0.f; v->nrm[2] = 1.f;
            v->col = 0.f;
            m->idx[m->nidx++] = (unsigned)m->nverts;
            m->nverts++;
        }
        pen += g->advance;
    }
    return 1;
}

/* draw_scena() drew each text with glTranslatef(x, y, -1); glScalef(2,2,2) */
static void add_scene_texts(mesh_t *m, int text_ini, int text_num)
{
    int i;
    m->nbake = m->nverts;
    for (i = text_ini; i < text_num; i++)
        mesh_add_text(m, MAX_VERTS, scene_texts[i].str, scene_texts[i].x, scene_texts[i].y, -1.f, 2.f);
}

/* ---- scenes ----------------------------------------------------------- */

static int finish_mesh(mesh_t *dst, const mesh_t *src)
{
    *dst = *src;
    dst->verts = (vert_t *)malloc(sizeof(vert_t) * (src->nverts ? src->nverts : 1));
    dst->idx   = (unsigned *)malloc(sizeof(unsigned) * (src->nidx ? src->nidx : 1));
    if (!dst->verts || !dst->idx) return 0;
    memcpy(dst->verts, src->verts, sizeof(vert_t) * src->nverts);
    memcpy(dst->idx, src->idx, sizeof(unsigned) * src->nidx);
    return 1;
}

int scene_generate(void)
{
    mesh_t work;
    int demostate, icnt2, icnt3, cnt_scn;
    float f_calc;

    scratch_verts = (vert_t *)malloc(sizeof(vert_t) * MAX_VERTS);
    scratch_idx   = (unsigned *)malloc(sizeof(unsigned) * MAX_IDX);
    if (!scratch_verts || !scratch_idx) return 0;

    for (simple_pass = 0; simple_pass < 2; simple_pass++) {
        cnt_scn = 0;
        for (demostate = 0; demostate < SCENES; demostate++) {
            int text_ini, text_num;
            work.verts = scratch_verts;
            work.idx   = scratch_idx;
            work.nverts = work.nidx = work.nbake = 0;
            pscene = &work;

            text_ini = cnt_scn;
            cnt_scn = text_num = cnt_scn + text_numbers[demostate];

            /* cuerno by default */
            res_u = res_v = 20;
            res_ulow = res_vlow = 6;
            res_obj = OBJ_CUERNO;

            m_identity(&M);

            switch (demostate) {
            case 0: /* logo fuzzion */
                m_translate(&M, -4.f, 0.4f, -0.6f);
                res_u = 8;   res_v = 200;
                res_ulow = 4; res_vlow = 75;
                res_obj = OBJ_LOGO;
                add_object();
                /* rotated plane */
                m_identity(&M);
                m_rotate(&M, 90, 1, 0, 0);
                m_translate(&M, 0, -1.8f, 0);
                res_u = res_v = 100;
                res_ulow = res_vlow = 1;
                break;

            case 6: /* npi */
                holdrand = 30;
                for (icnt2 = 0; icnt2 < 40; icnt2++) {
                    m_rotate(&M, myRandFloat() * 180, 1, 0, 0);
                    m_rotate(&M, myRandFloat() * 180, 0, 1, 0);
                    m_rotate(&M, myRandFloat() * 180, 0, 0, 1);
                    m_scale(&M, 1, 2, 1);
                    m_translate(&M, 0.f, -3.f, 0);
                    add_object();
                    m_scale(&M, -1, -1, -1);
                    add_object();
                    m_identity(&M);
                }
                m_scale(&M, 1.6f, 1.6f, 1.6f);
                res_u = 8;   res_v = 200;
                res_ulow = 4; res_vlow = 75;
                res_obj = OBJ_LOGO;
                add_object();
                res_u = res_v = res_ulow = res_vlow = 0;
                break;

            case 2:
                holdrand = 40;
                for (icnt2 = 0; icnt2 < 20; icnt2++) {
                    m_rotate(&M, myRandFloat() * 180, 1, 0, 0);
                    m_rotate(&M, myRandFloat() * 180, 0, 1, 0);
                    m_rotate(&M, myRandFloat() * 180, 0, 0, 1);
                    m_scale(&M, 1, 2, 1);
                    add_object();
                    m_identity(&M);
                }
                m_translate(&M, 0, -5.f, 0);
                res_u = res_v = 100;
                res_ulow = res_vlow = 1;
                break;

            case 1:
            case 4:
                res_u = res_v = 10;
                for (icnt3 = 0; icnt3 < 3; icnt3++)
                    for (icnt2 = 0; icnt2 < 4; icnt2++) {
                        m_rotate(&M, 90.f + 120.f * icnt3, 1, 1, 1);
                        m_rotate(&M, 90.f * icnt2, 0, 1, 0);
                        m_scale(&M, 0.8f, 0.8f, 0.8f);
                        m_translate(&M, 3, 0, 3);
                        m_scale(&M, 1.f, 3, 1.f);
                        add_cube();
                        m_identity(&M);
                    }
                m_translate(&M, 0, -5.f, 0);
                res_u = res_v = 100;
                res_ulow = res_vlow = 1;
                break;

            case 5: /* credits */
                holdrand = 10;
                res_u = res_v = 12;
                for (icnt2 = 0; icnt2 < 25; icnt2++) {
                    f_calc = 2 * (myRandFloat() + 1.1f);
                    m_translate(&M, 0, 0, -3.f);
                    m_translate(&M, myRandFloat() * 10, myRandFloat() * 8, myRandFloat());
                    m_scale(&M, f_calc, f_calc, 0.4f);
                    add_cube();
                    m_identity(&M);
                }
                m_rotate(&M, 90, 1, 0, 0);
                m_translate(&M, 0, -5.f, 0);
                res_u = res_v = 100;
                res_ulow = res_vlow = 1;
                break;

            case 3:
                holdrand = 10;
                res_u = res_v = 4;
                for (icnt2 = 0; icnt2 < 100; icnt2++) {
                    f_calc = 0.2f * (myRandFloat() + 1.1f);
                    m_translate(&M, myRandFloat() * 4, myRandFloat(), myRandFloat());
                    m_scale(&M, f_calc, f_calc, f_calc);
                    add_cube();
                    m_identity(&M);
                }
                m_rotate(&M, 90, 1, 0, 0);
                m_translate(&M, 0, -1.8f, 0);
                res_u = res_v = 100;
                res_ulow = res_vlow = 1;
                break;
            }
            /* every scene ends with a big flat plane */
            res_obj = OBJ_PLANE;
            m_scale(&M, 10, 0.1f, 10);
            add_object();

            add_scene_texts(&work, text_ini, text_num);

            if (!finish_mesh(simple_pass ? &sc_simple[demostate] : &sc_normal[demostate], &work))
                return 0;
        }
    }

    free(scratch_verts);
    free(scratch_idx);
    scratch_verts = 0;
    scratch_idx = 0;
    return 1;
}

void scene_free(void)
{
    int i;
    for (i = 0; i < SCENES; i++) {
        free(sc_normal[i].verts); free(sc_normal[i].idx);
        free(sc_simple[i].verts); free(sc_simple[i].idx);
        memset(&sc_normal[i], 0, sizeof(mesh_t));
        memset(&sc_simple[i], 0, sizeof(mesh_t));
    }
}
