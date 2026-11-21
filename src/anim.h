/* anim.h — 几何尺寸与展开/收起弹簧（欠阻尼展开、近临界阻尼收起） */
#ifndef QISLAND_ANIM_H
#define QISLAND_ANIM_H

#include "common.h"

typedef struct {
    double curW, curH, curR;
    double exp, expV;
    int    expanded;
    int    overrideMid;      /* 通知态：所有尺寸查询强制返回中卡片 */
    DWORD  lastTick;
    double hugeW, hugeH;      /* 超大卡片逻辑尺寸，按屏幕算 */
} Anim;

void   anim_init_sizes(Anim *a, int scrW, int scrH, double u,
                       int *winW, int *winH);
void   anim_view_size(const Anim *a, int view,
                      double *w, double *h, double *r);
void   anim_tick(Anim *a, double dt, int view);
int    anim_active(const Anim *a, int view);
void   anim_geometry(const Anim *a, double *w, double *h, double *r);

int    anim_inside_target(const Anim *a, int view, double u,
                          int winW, double margin, int px, int py);

#endif /* QISLAND_ANIM_H */
