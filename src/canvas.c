/* canvas.c — 画布实现 */
#include "canvas.h"
#include "config.h"

static int make_dib(int w, int h, HDC *dc, HBITMAP *bmp, void **bits)
{
    BITMAPINFO bi;
    HDC screen;
    ZeroMemory(&bi, sizeof(bi));
    bi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth       = w;
    bi.bmiHeader.biHeight      = -h;      /* top-down */
    bi.bmiHeader.biPlanes      = 1;
    bi.bmiHeader.biBitCount    = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    screen = GetDC(NULL);
    *dc = CreateCompatibleDC(screen);
    ReleaseDC(NULL, screen);
    if (!*dc) return 0;
    *bmp = CreateDIBSection(*dc, &bi, DIB_RGB_COLORS, bits, NULL, 0);
    if (!*bmp) { DeleteDC(*dc); *dc = NULL; return 0; }
    SelectObject(*dc, *bmp);
    return 1;
}

static void free_dib(HDC *dc, HBITMAP *bmp)
{
    if (*bmp) { DeleteObject(*bmp); *bmp = NULL; }
    if (*dc)  { DeleteDC(*dc);      *dc = NULL; }
}

int canvas_init(Canvas *c, int W, int H, double u, int ss)
{
    ZeroMemory(c, sizeof(*c));
    c->W = W; c->H = H; c->u = u; c->ss = ss;
    if (!make_dib(W, H, &c->outDC, &c->outBmp, &c->outBits)) return 0;
    if (!make_dib(W, H, &c->txtDC, &c->txtBmp, &c->txtBits)) return 0;
    canvas_reset_clip(c);
    return 1;
}

void canvas_reset_clip(Canvas *c)
{
    if (!c) return;
    c->clipX0 = 0; c->clipY0 = 0; c->clipX1 = c->W; c->clipY1 = c->H;
}

void canvas_set_clip(Canvas *c, int x0, int y0, int x1, int y1)
{
    if (!c) return;
    c->clipX0 = qs_iclamp(x0, 0, c->W);
    c->clipY0 = qs_iclamp(y0, 0, c->H);
    c->clipX1 = qs_iclamp(x1, 0, c->W);
    c->clipY1 = qs_iclamp(y1, 0, c->H);
    if (c->clipX1 < c->clipX0) c->clipX1 = c->clipX0;
    if (c->clipY1 < c->clipY0) c->clipY1 = c->clipY0;
}

void canvas_shutdown(Canvas *c)
{
    free_dib(&c->outDC, &c->outBmp);
    free_dib(&c->txtDC, &c->txtBmp);
    c->outBits = c->txtBits = NULL;
}

double canvas_sd_roundbox(double px, double py,
                          double cx, double cy, double hw, double hh, double r)
{
    double qx = fabs(px - cx) - (hw - r);
    double qy = fabs(py - cy) - (hh - r);
    double ax = qx > 0.0 ? qx : 0.0;
    double ay = qy > 0.0 ? qy : 0.0;
    double outside = sqrt(ax * ax + ay * ay);
    double inside  = qx > qy ? qx : qy;
    if (inside > 0.0) inside = 0.0;
    return outside + inside - r;
}

static double shape_alpha(double d, double soft)
{
    return qs_clamp(0.5 - d / soft, 0.0, 1.0);
}

void canvas_shape(Canvas *c, double w, double h, double r)
{
    unsigned char *dst = (unsigned char *)c->outBits;
    int W = c->W, H = c->H, ss = c->ss;
    double sx = ((double)W - w * c->u) * 0.5;
    double sw = w * c->u, sh = h * c->u;
    double cx = sx + sw * 0.5, cy = sh * 0.5;
    double hw = sw * 0.5, hh = sh * 0.5;
    double rr = r * c->u, soft = SOFT_PX * c->u;
    double margin = soft + 1.5;
    int x0, y0, x1, y1, x, y;

    if (rr > hh) rr = hh;
    memset(dst, 0, (size_t)W * (size_t)H * 4);
    x0 = qs_iclamp((int)floor(sx - soft - 2.0), 0, W);
    y0 = qs_iclamp((int)floor(-soft - 2.0), 0, H);
    x1 = qs_iclamp((int)ceil(sx + sw + soft + 2.0), 0, W);
    y1 = qs_iclamp((int)ceil(sh + soft + 2.0), 0, H);

    for (y = y0; y < y1; y++) {
        for (x = x0; x < x1; x++) {
            double d = canvas_sd_roundbox((double)x + 0.5, (double)y + 0.5,
                                          cx, cy, hw, hh, rr);
            double cov;
            if (d < -margin) cov = 1.0;
            else if (d > margin) cov = 0.0;
            else {
                double acc = 0.0;
                for (int j = 0; j < ss; j++) {
                    double py = (double)y + ((double)j + 0.5) / (double)ss;
                    for (int i = 0; i < ss; i++) {
                        double px = (double)x + ((double)i + 0.5) / (double)ss;
                        acc += shape_alpha(
                            canvas_sd_roundbox(px, py, cx, cy, hw, hh, rr), soft);
                    }
                }
                cov = acc / (double)(ss * ss);
            }
            if (cov <= 0.0) continue;
            {
                int a = (int)(cov * 255.0 + 0.5);
                unsigned char *o = dst + ((size_t)y * (size_t)W + (size_t)x) * 4;
                if (a > 255) a = 255;
                o[0] = (unsigned char)(CB_B * a / 255);
                o[1] = (unsigned char)(CB_G * a / 255);
                o[2] = (unsigned char)(CB_R * a / 255);
                o[3] = (unsigned char)a;
            }
        }
    }
}

void canvas_blend_px(Canvas *c, int x, int y,
                     int cr, int cg, int cb, double k)
{
    unsigned char *o;
    int da;
    double base[3], src[3], t;
    int i;
    if (!c->outBits || k <= 0.0 || x < 0 || y < 0 || x >= c->W || y >= c->H) return;
    if (x < c->clipX0 || x >= c->clipX1 ||
        y < c->clipY0 || y >= c->clipY1) return;
    if (k > 1.0) k = 1.0;
    o = (unsigned char *)c->outBits + ((size_t)y * (size_t)c->W + (size_t)x) * 4;
    da = o[3];
    if (da <= 0) return;
    t = k;
    base[0] = (double)o[0]; base[1] = (double)o[1]; base[2] = (double)o[2];
    src[0]  = (double)cb;   src[1]  = (double)cg;   src[2]  = (double)cr;
    for (i = 0; i < 3; i++) {
        double v = base[i] * (1.0 - t) + src[i] * t * (double)da / 255.0;
        o[i] = (unsigned char)qs_iclamp((int)(v + 0.5), 0, 255);
    }
}

void canvas_write_px(Canvas *c, int x, int y,
                     int cr, int cg, int cb, double a)
{
    unsigned char *o;
    double da, oa, src[3];
    int i;
    if (!c->outBits || a <= 0.0 || x < 0 || y < 0 ||
        x >= c->W || y >= c->H) return;
    if (x < c->clipX0 || x >= c->clipX1 ||
        y < c->clipY0 || y >= c->clipY1) return;
    if (a > 1.0) a = 1.0;
    o = (unsigned char *)c->outBits + ((size_t)y * (size_t)c->W + (size_t)x) * 4;
    da = (double)o[3] / 255.0;
    oa = a + da * (1.0 - a);
    src[0] = (double)cb; src[1] = (double)cg; src[2] = (double)cr;
    /* outBits 是预乘 BGRA：目标保留 (1-a)，源按自身 alpha 预乘后叠加 */
    for (i = 0; i < 3; i++) {
        double v = (double)o[i] * (1.0 - a) + src[i] * a;
        o[i] = (unsigned char)qs_iclamp((int)(v + 0.5), 0, 255);
    }
    o[3] = (unsigned char)qs_iclamp((int)(oa * 255.0 + 0.5), 0, 255);
}

void canvas_round_alpha(Canvas *c, double x, double y, double w, double h,
                        double r, int cr, int cg, int cb, double a)
{
    double cx = x + w * 0.5, cy = y + h * 0.5, hw = w * 0.5, hh = h * 0.5;
    double soft = 1.2;
    int x0 = qs_iclamp((int)floor(x - 2), 0, c->W);
    int y0 = qs_iclamp((int)floor(y - 2), 0, c->H);
    int x1 = qs_iclamp((int)ceil(x + w + 2), 0, c->W);
    int y1 = qs_iclamp((int)ceil(y + h + 2), 0, c->H);
    if (r > hh) r = hh;
    if (r > hw) r = hw;
    for (int yy = y0; yy < y1; yy++)
        for (int xx = x0; xx < x1; xx++) {
            double d = canvas_sd_roundbox((double)xx + 0.5, (double)yy + 0.5,
                                          cx, cy, hw, hh, r);
            double cov = qs_clamp(0.5 - d / soft, 0.0, 1.0);
            if (cov > 0.0)
                canvas_write_px(c, xx, yy, cr, cg, cb, cov * a);
        }
}

/* dir = 1：顶点在上 (cx, yTop)，底边在 yTop+h —— 气泡在按钮下方，尖角朝上 dir = -1：底边在上，顶点在下 (cx, yTop+h) —— 气泡翻到按钮上方，尖角朝下 */
void canvas_tri(Canvas *c, double cx, double yTop, double halfW, double h,
                int dir, int cr, int cg, int cb, double a)
{
    int y0 = qs_iclamp((int)floor(yTop) - 1, 0, c->H);
    int y1 = qs_iclamp((int)ceil(yTop + h) + 1, 0, c->H);
    if (h <= 0.0 || halfW <= 0.0) return;
    for (int yy = y0; yy < y1; yy++) {
        double t = (dir > 0) ? (((double)yy + 0.5 - yTop) / h)
                             : ((yTop + h - ((double)yy + 0.5)) / h);
        double hw, xa, xb;
        int ix0, ix1;
        if (t < 0.0) t = 0.0;
        if (t > 1.0) t = 1.0;
        hw = halfW * t;
        xa = cx - hw; xb = cx + hw;
        ix0 = qs_iclamp((int)floor(xa) - 1, 0, c->W);
        ix1 = qs_iclamp((int)ceil(xb) + 1, 0, c->W);
        for (int xx = ix0; xx < ix1; xx++) {
            double dl = (double)xx + 0.5 - xa;
            double dr = xb - ((double)xx + 0.5);
            double d = dl < dr ? dl : dr;
            double cov = qs_clamp(0.5 + d, 0.0, 1.0);
            if (cov > 0.0)
                canvas_write_px(c, xx, yy, cr, cg, cb, cov * a);
        }
    }
}

void canvas_round(Canvas *c, double x, double y, double w, double h, double r,
                  int cr, int cg, int cb, double k)
{
    double cx = x + w * 0.5, cy = y + h * 0.5, hw = w * 0.5, hh = h * 0.5;
    double soft = 1.2;
    int x0 = qs_iclamp((int)floor(x - 2), 0, c->W);
    int y0 = qs_iclamp((int)floor(y - 2), 0, c->H);
    int x1 = qs_iclamp((int)ceil(x + w + 2), 0, c->W);
    int y1 = qs_iclamp((int)ceil(y + h + 2), 0, c->H);
    if (r > hh) r = hh;
    if (r > hw) r = hw;
    for (int yy = y0; yy < y1; yy++)
        for (int xx = x0; xx < x1; xx++) {
            double d = canvas_sd_roundbox((double)xx + 0.5, (double)yy + 0.5,
                                          cx, cy, hw, hh, r);
            double cov = qs_clamp(0.5 - d / soft, 0.0, 1.0);
            if (cov > 0.0) canvas_blend_px(c, xx, yy, cr, cg, cb, cov * k);
        }
}

void canvas_frame(Canvas *c, double x, double y, double w, double h, double r,
                  double th, int cr, int cg, int cb, double k)
{
    double cx = x + w * 0.5, cy = y + h * 0.5, soft = 1.1;
    int x0 = qs_iclamp((int)floor(x - 2), 0, c->W);
    int y0 = qs_iclamp((int)floor(y - 2), 0, c->H);
    int x1 = qs_iclamp((int)ceil(x + w + 2), 0, c->W);
    int y1 = qs_iclamp((int)ceil(y + h + 2), 0, c->H);
    double ir = r > th ? r - th : 0.5;
    for (int yy = y0; yy < y1; yy++)
        for (int xx = x0; xx < x1; xx++) {
            double px = (double)xx + 0.5, py = (double)yy + 0.5;
            double dO = canvas_sd_roundbox(px, py, cx, cy, w * 0.5, h * 0.5, r);
            double dI = canvas_sd_roundbox(px, py, cx, cy,
                                           w * 0.5 - th, h * 0.5 - th, ir);
            double cov = qs_clamp(0.5 - dO / soft, 0.0, 1.0)
                       - qs_clamp(0.5 - dI / soft, 0.0, 1.0);
            if (cov > 0.0) canvas_blend_px(c, xx, yy, cr, cg, cb, cov * k);
        }
}

void canvas_disc(Canvas *c, double cx, double cy, double rad,
                 int cr, int cg, int cb, double k)
{
    int x0 = qs_iclamp((int)floor(cx - rad - 2), 0, c->W);
    int y0 = qs_iclamp((int)floor(cy - rad - 2), 0, c->H);
    int x1 = qs_iclamp((int)ceil(cx + rad + 2), 0, c->W);
    int y1 = qs_iclamp((int)ceil(cy + rad + 2), 0, c->H);
    for (int y = y0; y < y1; y++)
        for (int x = x0; x < x1; x++) {
            double dx = (double)x + 0.5 - cx, dy = (double)y + 0.5 - cy;
            double d = sqrt(dx * dx + dy * dy) - rad;
            double cov = qs_clamp(0.5 - d / 1.1, 0.0, 1.0);
            if (cov > 0.0) canvas_blend_px(c, x, y, cr, cg, cb, cov * k);
        }
}

void canvas_line(Canvas *c, double ax, double ay, double bx, double by, double th,
                 int cr, int cg, int cb, double k)
{
    double vx = bx - ax, vy = by - ay, len2 = vx * vx + vy * vy;
    int x0 = qs_iclamp((int)floor((ax < bx ? ax : bx) - th - 2), 0, c->W);
    int y0 = qs_iclamp((int)floor((ay < by ? ay : by) - th - 2), 0, c->H);
    int x1 = qs_iclamp((int)ceil((ax > bx ? ax : bx) + th + 2), 0, c->W);
    int y1 = qs_iclamp((int)ceil((ay > by ? ay : by) + th + 2), 0, c->H);
    if (len2 < 0.0001) return;
    for (int y = y0; y < y1; y++)
        for (int x = x0; x < x1; x++) {
            double px = (double)x + 0.5, py = (double)y + 0.5;
            double t = qs_clamp(((px - ax) * vx + (py - ay) * vy) / len2, 0.0, 1.0);
            double dx = px - (ax + t * vx), dy = py - (ay + t * vy);
            double d = sqrt(dx * dx + dy * dy) - th * 0.5;
            double cov = qs_clamp(0.5 - d / 1.1, 0.0, 1.0);
            if (cov > 0.0) canvas_blend_px(c, x, y, cr, cg, cb, cov * k);
        }
}

int canvas_alpha_at(const Canvas *c, int x, int y)
{
    const unsigned char *p;
    if (!c->outBits || x < 0 || y < 0 || x >= c->W || y >= c->H) return 0;
    p = (const unsigned char *)c->outBits
      + ((size_t)y * (size_t)c->W + (size_t)x) * 4;
    return p[3];
}

void canvas_present(Canvas *c, HWND hwnd)
{
    POINT pt = {0, 0};
    SIZE sz = { c->W, c->H };
    BLENDFUNCTION bf;
    bf.BlendOp = AC_SRC_OVER;
    bf.BlendFlags = 0;
    bf.SourceConstantAlpha = 255;
    bf.AlphaFormat = AC_SRC_ALPHA;
    UpdateLayeredWindow(hwnd, NULL, NULL, &sz, c->outDC, &pt, 0, &bf, ULW_ALPHA);
}

void canvas_dump_bmp(Canvas *c, const char *path)
{
    int w = c->W, h = c->H, row = (w * 3 + 3) & ~3;
    BITMAPFILEHEADER fh;
    BITMAPINFOHEADER ih;
    FILE *f;
    unsigned char *line;
    ZeroMemory(&fh, sizeof(fh)); ZeroMemory(&ih, sizeof(ih));
    fh.bfType = 0x4D42;
    fh.bfOffBits = sizeof(fh) + sizeof(ih);
    fh.bfSize = fh.bfOffBits + (DWORD)row * (DWORD)h;
    ih.biSize = sizeof(ih); ih.biWidth = w; ih.biHeight = h;
    ih.biPlanes = 1; ih.biBitCount = 24; ih.biCompression = BI_RGB;
    ih.biSizeImage = (DWORD)row * (DWORD)h;
    f = fopen(path, "wb");
    if (!f) return;
    fwrite(&fh, sizeof(fh), 1, f);
    fwrite(&ih, sizeof(ih), 1, f);
    line = (unsigned char *)calloc((size_t)row, 1);
    if (line) {
        for (int y = h - 1; y >= 0; y--) {
            const unsigned char *s = (const unsigned char *)c->outBits
                                   + (size_t)y * (size_t)w * 4;
            for (int x = 0; x < w; x++) {
                int a = s[x * 4 + 3];
                int b = a ? (int)(s[x * 4 + 0] * 255 / a) : 0;
                int g = a ? (int)(s[x * 4 + 1] * 255 / a) : 0;
                int r = a ? (int)(s[x * 4 + 2] * 255 / a) : 0;
                line[x * 3 + 0] = (unsigned char)(232 + (b - 232) * a / 255);
                line[x * 3 + 1] = (unsigned char)(228 + (g - 228) * a / 255);
                line[x * 3 + 2] = (unsigned char)(226 + (r - 226) * a / 255);
            }
            fwrite(line, 1, (size_t)row, f);
        }
        free(line);
    }
    fclose(f);
}
