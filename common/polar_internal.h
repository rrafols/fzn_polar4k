/* polar_internal.h - shared between polar_scene.c and polar_render.c */
#ifndef POLAR_INTERNAL_H
#define POLAR_INTERNAL_H

#define SCENES 7

/* what the original packed as GL_C4F_N3F_V3F; colour is a single grey */
typedef struct {
    float pos[3];
    float nrm[3];
    float col;
} vert_t;

typedef struct {
    vert_t   *verts;
    int       nverts;
    int       nbake;     /* first nbake vertices get an AO colour; the rest is text (black) */
    unsigned *idx;       /* triangle list */
    int       nidx;
} mesh_t;

extern mesh_t sc_normal[SCENES];   /* full resolution: what is drawn */
extern mesh_t sc_simple[SCENES];   /* low resolution: the occluders during the AO bake */

/* builds sc_normal / sc_simple, returns 0 on failure */
int  scene_generate(void);
void scene_free(void);

/* appends the polygons of `str` (1 em == `scale` units) to a mesh, at world
 * position (x,y,z).  Used for the loading screen text.  Returns 0 if full. */
int  mesh_add_text(mesh_t *m, int capacity, const char *str, float x, float y, float z, float scale);

#endif
