/* anim.h — 几何尺寸与展开/收起弹簧 */
#ifndef QISLAND_ANIM_H
#define QISLAND_ANIM_H

#include "common.h"
#include "canvas.h"

typedef struct {
    double curW, curH, curR;
    double exp, expV;
    int    expanded;
    int    overrideMid;
    DWORD  lastTick;
    double hugeW, hugeH;
    double intro;          /* 入场进度 0..1，按真实时间推进 */
    int    introDone;
    DWORD  introStart;
} Anim;

void   anim_init_sizes(Anim *a, int scrW, int scrH, double u,
                       int *winW, int *winH);
void   anim_view_size(const Anim *a, int view,
                      double *w, double *h, double *r);
void   anim_tick(Anim *a, double dt, int view);
int    anim_active(const Anim *a, int view);
void   anim_geometry(const Anim *a, double *w, double *h, double *r);
void   anim_intro_begin(Anim *a);
void   anim_intro_geometry(const Anim *a, double w, double h, double r,
                           double *iw, double *ih, double *ir,
                           double *fade, double *lift);
void   anim_intro_draw(Canvas *c, const Anim *a, double u,
                       double w, double h, double r);

int    anim_inside_target(const Anim *a, int view, double u,
                          int winW, double margin, int px, int py);

#endif /* QISLAND_ANIM_H */
