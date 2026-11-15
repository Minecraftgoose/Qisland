/* anim.c — 尺寸与弹簧 */
#include "anim.h"
#include "config.h"
#include "canvas.h"
#include "cards.h"

/* 尺寸档由卡片表决定，不再靠 view 编号区间硬判。
   以后插一张新卡只要填 tier，尺寸自动跟上。 */
void anim_view_size(const Anim *a, int view,
                    double *w, double *h, double *r)
{
    /* 通知卡片不进 cards[]：用覆盖位把当前视图的几何统一切成中卡片，
       切档弹簧、目标态命中、窗口余量全部自动跟上，不用改导航表。 */
    if (a->overrideMid) {
        *w = MID_W; *h = MID_H; *r = MID_R;
        return;
    }
    switch (cards_tier(view)) {
    case TIER_BIG:  *w = BIG_W;  *h = BIG_H;  *r = BIG_R;  break;
    case TIER_HUGE: *w = a->hugeW; *h = a->hugeH; *r = HUGE_R; break;
    default:        *w = MID_W;  *h = MID_H;  *r = MID_R;  break;
    }
}

void anim_init_sizes(Anim *a, int scrW, int scrH, double u,
                     int *winW, int *winH)
{
    double halfW = (double)scrW / u * 0.5;
    double halfH = (double)scrH / u * 0.5;
    double capW, capH;
    int W, H;

    a->hugeW = halfW - 2.0 * HUGE_GAP;
    a->hugeH = halfH - 2.0 * HUGE_GAP;
    if (a->hugeW < BIG_W * 1.20) a->hugeW = BIG_W * 1.20;
    if (a->hugeH < BIG_H * 1.20) a->hugeH = BIG_H * 1.20;

    /* 窗口容得下最大卡片 + 展开过冲 1.0825 */
    W = (int)((a->hugeW * OVERSHOOT + 2.0 * PAD) * u + 0.5);
    H = (int)((a->hugeH * OVERSHOOT + 2.0 * PAD) * u + 0.5);
    {
        int maxW = scrW - 4;
        int maxH = scrH - (int)(TOP_GAP * u) - 4;
        if (W > maxW) W = maxW;
        if (H > maxH) H = maxH;
    }
    if (W < 200) W = 200;
    if (H < 120) H = 120;
    capW = ((double)W / u - 2.0 * PAD) / OVERSHOOT;
    capH = ((double)H / u - 2.0 * PAD) / OVERSHOOT;
    if (a->hugeW > capW) a->hugeW = capW;
    if (a->hugeH > capH) a->hugeH = capH;

    *winW = W; *winH = H;
    {
        double w, h, r;
        anim_view_size(a, DEFAULT_VIEW, &w, &h, &r);
        a->curW = w; a->curH = h; a->curR = r;
    }
    a->exp = 0.0; a->expV = 0.0; a->expanded = 0;
    a->lastTick = qs_now();
}

void anim_tick(Anim *a, double dt, int view)
{
    double tw, th, tr, k, h, target, damp;
    int i;
    const int SUB = 4;

    anim_view_size(a, view, &tw, &th, &tr);
    k = 1.0 - exp(-dt / SIZE_TAU);      /* 切档时尺寸平滑追随 */
    a->curW += (tw - a->curW) * k;
    a->curH += (th - a->curH) * k;
    a->curR += (tr - a->curR) * k;
    if (fabs(a->curW - tw) < 0.05) a->curW = tw;
    if (fabs(a->curH - th) < 0.05) a->curH = th;
    if (fabs(a->curR - tr) < 0.05) a->curR = tr;

    h = dt / (double)SUB;
    target = a->expanded ? 1.0 : 0.0;
    damp = a->expanded ? EXP_DAMP_OUT : EXP_DAMP_IN;
    for (i = 0; i < SUB; i++) {
        a->expV += (EXP_K * (target - a->exp) - damp * a->expV) * h;
        a->exp  += a->expV * h;
    }
}

int anim_active(const Anim *a, int view)
{
    double tw, th, tr;
    anim_view_size(a, view, &tw, &th, &tr);
    if (fabs(a->curW - tw) > 0.05 || fabs(a->curH - th) > 0.05 ||
        fabs(a->curR - tr) > 0.05) return 1;
    if (fabs(a->exp - (a->expanded ? 1.0 : 0.0)) > 0.0005 ||
        fabs(a->expV) > 0.002) return 1;
    return 0;
}

void anim_geometry(const Anim *a, double *w, double *h, double *r)
{
    *w = ISLAND_W + (a->curW - ISLAND_W) * a->exp;
    *h = ISLAND_H + (a->curH - ISLAND_H) * a->exp;
    *r = ISLAND_R + (a->curR - ISLAND_R) * a->exp;
    if (*r > *h * 0.5) *r = *h * 0.5;
}

int anim_inside_target(const Anim *a, int view, double u,
                       int winW, double margin, int px, int py)
{
    double w, h, r, sx, d;
    if (a->expanded) anim_view_size(a, view, &w, &h, &r);
    else { w = ISLAND_W; h = ISLAND_H; r = ISLAND_R; }
    w *= u; h *= u; r *= u;
    if (r > h * 0.5) r = h * 0.5;
    sx = ((double)winW - w) * 0.5;
    d = canvas_sd_roundbox((double)px + 0.5, (double)py + 0.5,
                           sx + w * 0.5, h * 0.5, w * 0.5, h * 0.5, r);
    return d <= margin * u;
}
