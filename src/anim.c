/* anim.c — 尺寸与弹簧 */
#include "anim.h"
#include "config.h"
#include "canvas.h"
#include "cards.h"

/* 宽度带一次回弹，高度用慢起步的三次曲线 */
static double intro_back(double x)
{
    double u = x - 1.0;
    return 1.0 + (INTRO_BACK_C1 + 1.0) * u * u * u + INTRO_BACK_C1 * u * u;
}

static double intro_cubic(double x)
{
    double u = 1.0 - x;
    return 1.0 - u * u * u;
}

void anim_intro_begin(Anim *a)
{
    a->intro = 0.0;
    a->introDone = 0;
    a->introStart = qs_now();
}

void anim_intro_geometry(const Anim *a, double w, double h, double r,
                         double *iw, double *ih, double *ir,
                         double *fade, double *lift)
{
    double t, x, fw, fh;
    *iw = w; *ih = h; *ir = r; *fade = 1.0; *lift = 0.0;
    if (a->introDone) return;

    t = a->intro;
    if (t < 0.0) t = 0.0;
    if (t > 1.0) t = 1.0;
    fw = intro_back(t);
    x = (t - INTRO_H_DELAY) / (1.0 - INTRO_H_DELAY);
    if (x < 0.0) x = 0.0;
    if (x > 1.0) x = 1.0;
    fh = intro_cubic(x);

    *iw = INTRO_LINE_W + (w - INTRO_LINE_W) * fw;
    *ih = INTRO_LINE_H + (h - INTRO_LINE_H) * fh;
    *ir = r * fh;
    if (*ir > *ih * 0.5) *ir = *ih * 0.5;

    x = (t - INTRO_TEXT_AT) / (1.0 - INTRO_TEXT_AT);
    if (x < 0.0) x = 0.0;
    if (x > 1.0) x = 1.0;
    *fade = x;
    *lift = (1.0 - x) * INTRO_TEXT_LIFT;
}

/* 任意位置写一颗带柔边的光点，透明区域也能落笔 */
static void glow_px(Canvas *c, double cx, double cy, double rad,
                    int cr, int cg, int cb, double a)
{
    int x0, y0, x1, y1, x, y;
    if (a <= 0.002 || rad < 0.5) return;
    x0 = qs_iclamp((int)floor(cx - rad - 1.0), 0, c->W);
    y0 = qs_iclamp((int)floor(cy - rad - 1.0), 0, c->H);
    x1 = qs_iclamp((int)ceil(cx + rad + 1.0), 0, c->W);
    y1 = qs_iclamp((int)ceil(cy + rad + 1.0), 0, c->H);
    for (y = y0; y < y1; y++) {
        for (x = x0; x < x1; x++) {
            double dx = (double)x + 0.5 - cx;
            double dy = (double)y + 0.5 - cy;
            double q = 1.0 - sqrt(dx * dx + dy * dy) / rad;
            if (q > 0.0)
                canvas_write_px(c, x, y, cr, cg, cb, a * q * q);
        }
    }
}

/* 光晕强度：起步微光，汇聚时冲到峰值，落定收回常驻值 */
static double intro_glow_amp(const Anim *a)
{
    double t, x;
    if (a->introDone) return INTRO_GLOW_IDLE;
    t = a->intro;
    if (t <= 0.0) return INTRO_GLOW_IDLE;
    if (t < INTRO_GLOW_PEAK) {
        x = t / INTRO_GLOW_PEAK;
        x = x * x * (3.0 - 2.0 * x);
        return INTRO_GLOW_IDLE + (INTRO_GLOW_MAX - INTRO_GLOW_IDLE) * x;
    }
    x = (t - INTRO_GLOW_PEAK) / (1.0 - INTRO_GLOW_PEAK);
    if (x > 1.0) x = 1.0;
    x = x * x * (3.0 - 2.0 * x);
    return INTRO_GLOW_MAX + (INTRO_GLOW_IDLE - INTRO_GLOW_MAX) * x;
}

/* 按圆角矩形距离场铺外发光，内缘再补一道极窄高光 */
static void intro_glow(Canvas *c, double w, double h, double r,
                       double u, double amp)
{
    double sx, cx, cy, hw, hh, rr, gp, gin, soft;
    int x0, y0, x1, y1, x, y;
    sx = ((double)c->W - w * u) * 0.5;
    cx = sx + w * u * 0.5;
    cy = h * u * 0.5;
    hw = w * u * 0.5;
    hh = h * u * 0.5;
    rr = r * u;
    if (rr > hh) rr = hh;
    gp = INTRO_GLOW_PX * u;
    gin = INTRO_GLOW_IN * u;
    soft = INTRO_GLOW_TOP_SOFT;
    if (soft < 0.05) soft = 0.05;
    x0 = qs_iclamp((int)floor(sx - gp - 2.0), 0, c->W);
    y0 = 0;
    x1 = qs_iclamp((int)ceil(sx + w * u + gp + 2.0), 0, c->W);
    y1 = qs_iclamp((int)ceil(h * u + gp + 2.0), 0, c->H);
    for (y = y0; y < y1; y++) {
        for (x = x0; x < x1; x++) {
            double d = canvas_sd_roundbox((double)x + 0.5, (double)y + 0.5,
                                          cx, cy, hw, hh, rr);
            double q, s, up, w;
            if (d >= 0.0) {
                if (d >= gp) continue;
                q = 1.0 - d / gp;
                s = amp * q * q;
            } else {
                if (-d >= gin) continue;
                q = 1.0 + d / gin;
                s = amp * q * q * 0.32;
            }
            /* 越朝上越淡，左右与下方的光平滑收进顶部 */
            up = (cy - ((double)y + 0.5)) / (gp * soft);
            w = 1.0;
            if (up > 0.0) {
                if (up > 1.0) up = 1.0;
                w = 1.0 - up;
                w = w * w * (3.0 - 2.0 * w);
            }
            s *= w;
            if (s > 0.002)
                canvas_write_px(c, x, y, INTRO_GLOW_CR, INTRO_GLOW_CG,
                                INTRO_GLOW_CB, s);
        }
    }
}

/* 光点从画布四周边界沿径向汇入胶囊轮廓，带尾迹并错峰抵达 */
static void intro_particles(Canvas *c, const Anim *a, double u,
                            double w, double h)
{
    const double TAU = 6.28318530717958647692;
    double cx, cy, hw, hh, ox, oy, t;
    int i, j;
    cx = (double)c->W * 0.5;
    cy = h * u * 0.5;
    ox = cx;
    oy = (double)c->H * 0.5;
    hw = w * u * 0.5;
    hh = h * u * 0.5;
    t = a->intro;
    for (i = 0; i < INTRO_DOTS; i++) {
        double th = TAU * (double)i / (double)INTRO_DOTS + 0.21;
        double dx = cos(th), dy = sin(th);
        double tx = 1e9, ty = 1e9, tmax;
        double sx, sy, ex, ey, delay, p, e, px, py, env, len, nx, ny, a0;
        if (fabs(dx) > 1e-6) tx = ox / fabs(dx);
        if (fabs(dy) > 1e-6) ty = oy / fabs(dy);
        tmax = (tx < ty) ? tx : ty;
        if (tmax < 1.0) tmax = 1.0;
        sx = ox + dx * tmax * INTRO_SPREAD;
        sy = oy + dy * tmax * INTRO_SPREAD;
        ex = cx + dx * hw * 0.86;
        ey = cy + dy * hh * 0.86;
        delay = (double)((i * 7) % 13) / 13.0 * INTRO_DOT_DELAY;
        p = t / (INTRO_FLY_END - delay);
        if (p <= 0.0 || p >= 1.0) continue;
        e = p * p * (3.0 - 2.0 * p);
        px = sx + (ex - sx) * e;
        py = sy + (ey - sy) * e;
        if (p < 0.16) env = p / 0.16;
        else if (p > INTRO_DOT_FADE) env = (1.0 - p) / (1.0 - INTRO_DOT_FADE);
        else env = 1.0;
        len = sqrt((ex - sx) * (ex - sx) + (ey - sy) * (ey - sy));
        if (len < 0.001) continue;
        nx = (ex - sx) / len;
        ny = (ey - sy) / len;
        for (j = INTRO_TRAIL; j >= 1; j--) {
            double off = (double)j * INTRO_TRAIL_GAP * u;
            double q = env * (1.0 - (double)j / (double)(INTRO_TRAIL + 1)) * 0.62;
            glow_px(c, px - nx * off, py - ny * off,
                    INTRO_DOT_R * 0.72 * u,
                    INTRO_DOT_CR, INTRO_DOT_CG, INTRO_DOT_CB, q);
        }
        a0 = env * INTRO_DOT_A;
        glow_px(c, px, py, INTRO_DOT_R * u,
                INTRO_DOT_CR, INTRO_DOT_CG, INTRO_DOT_CB, a0);
        glow_px(c, px, py, INTRO_DOT_R * 0.42 * u, 255, 255, 255, a0 * 0.85);
    }
}

/* 入场绘制：光晕每帧都在，落定后岛态保留一层极淡常驻光 */
void anim_intro_draw(Canvas *c, const Anim *a, double u,
                     double w, double h, double r)
{
    canvas_reset_clip(c);
    if (a->introDone && a->exp > 0.02) return;
    intro_glow(c, w, h, r, u, intro_glow_amp(a));
    if (!a->introDone && a->intro < INTRO_FLY_END)
        intro_particles(c, a, u, w, h);
}

/* 尺寸档由卡片表决定 */
void anim_view_size(const Anim *a, int view,
                    double *w, double *h, double *r)
{
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

    if (!a->introDone) {
        double t = (double)(qs_now() - a->introStart) / (double)INTRO_MS;
        if (t >= 1.0) {
            /* 先渲染一帧终值，下一帧才收手 */
            if (a->intro >= 1.0) a->introDone = 1;
            else a->intro = 1.0;
        } else {
            a->intro = t;
        }
    }

    anim_view_size(a, view, &tw, &th, &tr);
    k = 1.0 - exp(-dt / SIZE_TAU);
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
    if (!a->introDone) return 1;
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
