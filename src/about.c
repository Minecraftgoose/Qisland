/* about.c — 设置页「关于」右栏
 *
 * 布局（0.3 轮按用户要求改：字号整体放大 + 全部纵向排列）：
 *   左上软件图标（exe 资源 IDI_QISLAND，即「中间黑」程序图标）
 *   右侧 Qisland + 下一行仓库地址（整行可点的链接，下划线提示），
 *   下方一条极淡横线；头部固定不滚。
 *   四个板块「使用的库 / 架构 / 许可证 / 使用的资源」单列依次向下排布，
 *   不再两列并排；每个板块内的胶囊也**每个独占一行**，纵向堆叠。
 *   胶囊「主体」亮色、「· 许可」淡色；长条目自动收缩字号以免越出板块宽。
 *
 * 可滚动：内容按文档流排布，超出 aboutBody 的部分裁掉；
 *   滚轮累加到 WHEEL_DELTA 走一步，右缘一条 3px 淡圆条指示位置。
 *   文档高度 docH 在绘制时算一次并缓存，滚轮据此 clamp。
 *   字号放大 + 单列后文档远高于可视区，滚动条一定会出现。
 *
 * 仓库链接：命中框由 settings_boxes() 给（b->aboutRepo），与文字绘制同源；
 *   点击由 ui.c 的 H_ABOUT_REPO → sys_open_repo() 用 ShellExecuteW 打开
 *   系统默认浏览器。链接本身不随文档滚动，任何滚动位置都能点。
 *
 * 实现约束（与项目其它页面一致）：
 *   · 只用目标态标称尺寸 W0/H0/L0，动画期间不重排、不重算字号；
 *   · 文字走全局 OPPO Sans（font_draw_label），无系统控件、无子窗口；
 *   · layered DIB 上 GDI 不保证写 alpha，程序图标先画进独立 32bpp DIB
 *     再按覆盖率叠进画布（同 notify.c 的 HICON 路径），不做灰白圆底；
 *   · 不建线程、不设定时器、不写盘；滚动只置一次脏，静止零重绘不变。
 */
#include "about.h"
#include "app.h"
#include "config.h"
#include "canvas.h"
#include "font.h"

/* ---------------- 文案（逐字来自确认稿，封闭清单第 15 条） ---------------- */
typedef struct { const WCHAR *head; const WCHAR *tail; } Chip;

static const Chip kLibs[] = {
    { L"stb_textedit.h",  NULL },
    { L"lunar-calendar",  NULL },
    { L"lunar-javascript", NULL },
    { L"Win32 / GDI",     NULL },
};
static const Chip kArch[] = {
    { L"纯 C · MinGW-w64",           NULL },
    { L"Layered Window · 32bpp DIB", NULL },
};
static const Chip kLic[] = {
    { L"Qisland",            L"Apache-2.0" },
    { L"OPPO Sans 4.0",      L"OPPO Sans Fonts License" },
    { L"Font Awesome 6.7.2", L"CC BY 4.0 / OFL 1.1 / MIT" },
    { L"lunar-calendar",     L"Apache-2.0" },
    { L"lunar-javascript",   L"MIT" },
    { L"stb_textedit.h",     L"公共领域" },
};
static const Chip kRes[] = {
    { L"OPPO Sans 4.0",          NULL },
    { L"Font Awesome Free 6.7.2", NULL },
};

static const WCHAR *kTitles[4] = {
    L"使用的库", L"架构", L"许可证", L"使用的资源"
};
static const Chip *kChips[4]  = { kLibs, kArch, kLic, kRes };
static const int   kChipN[4]  = {
    (int)(sizeof(kLibs) / sizeof(kLibs[0])),
    (int)(sizeof(kArch) / sizeof(kArch[0])),
    (int)(sizeof(kLic)  / sizeof(kLic[0])),
    (int)(sizeof(kRes)  / sizeof(kRes[0])),
};

/* ---------------- 胶囊度量 / 绘制 ---------------- */
static int chip_pad_x(int px) { return (int)(px * 0.95 + 0.5); }
static int chip_h(int px)     { return (int)(px * 2.10 + 0.5); }

static void chip_full(const Chip *ch, WCHAR *buf, int cap)
{
    if (!ch->tail) {
        _snwprintf(buf, cap, L"%s", ch->head);
    } else {
        _snwprintf(buf, cap, L"%s · %s", ch->head, ch->tail);
    }
    buf[cap - 1] = 0;
}

static int chip_w(FontSet *f, const Chip *ch, int px)
{
    WCHAR buf[192];
    chip_full(ch, buf, 192);
    return font_measure(f, buf, (int)wcslen(buf), px, 2) + 2 * chip_pad_x(px);
}

static void chip_draw(App *a, const Chip *ch, int px,
                      double x, double y, double w, double h, double k)
{
    Canvas *c = &a->canvas;
    WCHAR buf[192];
    int eff = px, tw, pad;
    double cy = y + h * 0.5;
    double tx;

    /* 字号放大后个别长条目（如 Font Awesome 那行许可）可能比板块还宽。
       逐 px 收缩到放得下为止；高度仍按标称 px 算，量高与绘制两遍一致。 */
    chip_full(ch, buf, 192);
    tw = font_measure(&a->fonts, buf, (int)wcslen(buf), eff, 2);
    while (eff > 10 && tw + 2 * chip_pad_x(eff) > (int)w) {
        eff--;
        tw = font_measure(&a->fonts, buf, (int)wcslen(buf), eff, 2);
    }
    pad = chip_pad_x(eff);
    tx = x + pad;

    /* 淡底 + 极淡描边，圆角 = 半高 → 胶囊 */
    canvas_round(c, x, y, w, h, h * 0.5,
                 255, 255, 255, k * ABOUT_CHIP_BG);
    canvas_frame(c, x, y, w, h, h * 0.5, 1.0,
                 255, 255, 255, k * ABOUT_CHIP_BORDER);

    if (!ch->tail) {
        font_draw_label(&a->fonts, c, ch->head, eff, tx, cy, 0,
                        ABOUT_TEXT_R, ABOUT_TEXT_G, ABOUT_TEXT_B, k);
        return;
    }
    {
        static const WCHAR sep[] = L" · ";
        int w1 = font_measure(&a->fonts, ch->head,
                              (int)wcslen(ch->head), eff, 2);
        int ws = font_measure(&a->fonts, sep, 3, eff, 2);
        font_draw_label(&a->fonts, c, ch->head, eff, tx, cy, 0,
                        ABOUT_HEAD_R, ABOUT_HEAD_G, ABOUT_HEAD_B, k);
        font_draw_label(&a->fonts, c, sep, eff, tx + w1, cy, 0,
                        ABOUT_LIC_R, ABOUT_LIC_G, ABOUT_LIC_B, k);
        font_draw_label(&a->fonts, c, ch->tail, eff, tx + w1 + ws, cy, 0,
                        ABOUT_LIC_R, ABOUT_LIC_G, ABOUT_LIC_B, k);
    }
}

/* 一个板块：标题 + **逐行纵向**胶囊，每个胶囊独占一行。
   draw=0 只算高度，draw=1 同时绘制；两遍走同一套行距，滚轮 clamp 用的
   docH 与画面严格一致。宽度按内容自适应，超长条目由 chip_draw 收缩字号。 */
static double block_run(App *a, int bi, double x, double y, double w,
                        int px, int titlePx, int draw, double k)
{
    Canvas *c = &a->canvas;
    double chipH = (double)chip_h(px);
    double gapY  = px * 0.62;
    double cy = y;
    int i;

    if (draw)
        font_draw_label(&a->fonts, c, kTitles[bi], titlePx,
                        x, cy + titlePx * 0.5, 0,
                        ABOUT_LABEL_R, ABOUT_LABEL_G, ABOUT_LABEL_B,
                        k * 0.92);
    cy += titlePx + px * 0.75;

    for (i = 0; i < kChipN[bi]; i++) {
        int cw = chip_w(&a->fonts, &kChips[bi][i], px);
        if (cw > w) cw = (int)w;                  /* 保险：绝不越出板块宽 */
        if (draw)
            chip_draw(a, &kChips[bi][i], px, x, cy,
                      (double)cw, chipH, k);
        cy += chipH + gapY;
    }
    /* 最后一行底部减板块起点 = 板块总高（含标题） */
    return (cy - gapY) - y;
}

/* ---------------- 文档流：四个板块单列纵向依次排布 ----------------
   0.3 轮起不再「使用的库 / 架构」两列并排，全部用整行宽度依次向下。
   内容高度远高于 aboutBody.h，滚轮一定滚得动。 */
static double about_doc(App *a, const SBox *body, double H,
                        int px, int titlePx, int draw, double k)
{
    double gap = H * ABOUT_BLOCK_GAP;
    double y   = body->y - a->settings.aboutScroll;
    double h, total = 0.0;
    int bi;

    for (bi = 0; bi < 4; bi++) {
        h = block_run(a, bi, body->x, y, body->w,
                      px, titlePx, draw, k);
        y += h + gap;
        total += h + gap;
    }
    return total - gap;    /* 末尾那段 gap 只是留白，不计进文档高 */
}

/* ---------------- 程序图标：独立 DIB → 覆盖率 → alpha 画布 ----------------
   这是「中间黑」那张资源图标。它在深底上很暗，光栅化时把覆盖率适度放大
   并提亮色相（对应效果稿的 brightness(1.75)），但不加任何圆形底衬。 */
static void draw_program_icon(App *a, double x, double y, double size, double k)
{
    Canvas *c = &a->canvas;
    static HICON ico = NULL;
    HDC dc;
    BITMAPINFO bi;
    HBITMAP bmp, oldBmp;
    void *bits = NULL;
    DWORD *pxv;
    int iw, ih, xx, yy;

    if (!ico) ico = LoadIconW(a->inst, MAKEINTRESOURCEW(IDI_QISLAND));
    if (!ico || size < 8.0) return;
    iw = (int)(size + 0.5); ih = iw;

    dc = CreateCompatibleDC(NULL);
    if (!dc) return;
    ZeroMemory(&bi, sizeof(bi));
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = iw;
    bi.bmiHeader.biHeight = -ih;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    bmp = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
    if (!bmp || !bits) { if (bmp) DeleteObject(bmp); DeleteDC(dc); return; }
    oldBmp = (HBITMAP)SelectObject(dc, bmp);
    pxv = (DWORD *)bits;
    for (xx = 0; xx < iw * ih; xx++) pxv[xx] = 0xFF000000u;
    DrawIconEx(dc, 0, 0, ico, iw, ih, 0, NULL, DI_NORMAL);
    GdiFlush();

    for (yy = 0; yy < ih; yy++) {
        for (xx = 0; xx < iw; xx++) {
            DWORD v = pxv[yy * iw + xx];
            int b = (int)(v & 255), g = (int)((v >> 8) & 255),
                r = (int)((v >> 16) & 255);
            int mx = b > g ? b : g; if (r > mx) mx = r;
            double cov;
            int rr, gg, bb;
            if (mx <= 6) continue;
            rr = qs_iclamp(r * 255 / mx, 0, 255);
            gg = qs_iclamp(g * 255 / mx, 0, 255);
            bb = qs_iclamp(b * 255 / mx, 0, 255);
            /* 暗色图标：放大覆盖率、把色相压到冷灰白一档，形状才看得见 */
            cov = qs_clamp((double)mx / 255.0 * ABOUT_ICON_GAIN, 0.0, 1.0);
            rr = qs_iclamp((int)(rr * ABOUT_ICON_TINT_R), 0, 255);
            gg = qs_iclamp((int)(gg * ABOUT_ICON_TINT_G), 0, 255);
            bb = qs_iclamp((int)(bb * ABOUT_ICON_TINT_B), 0, 255);
            canvas_blend_px(c, (int)(x + xx), (int)(y + yy),
                            rr, gg, bb, cov * k);
        }
    }
    SelectObject(dc, oldBmp);
    DeleteObject(bmp);
    DeleteDC(dc);
}

/* ---------------- 主绘制 ---------------- */
void about_draw(App *a, double W, double H, double L,
                const SetBoxes *b, double k)
{
    Canvas *c = &a->canvas;
    const SBox *body = &b->aboutBody;
    int px      = qs_iclamp((int)(H * ABOUT_CHIP_PX),
                            ABOUT_CHIP_PX_MIN, ABOUT_CHIP_PX_MAX);
    int titlePx = qs_iclamp((int)(H * ABOUT_TITLE_PX),
                            ABOUT_TITLE_PX_MIN, ABOUT_TITLE_PX_MAX);
    int namePx  = qs_iclamp((int)(H * ABOUT_NAME_PX),
                            ABOUT_NAME_PX_MIN, ABOUT_NAME_PX_MAX);
    int repoPx  = b->repoPx;      /* 与 settings_boxes() 的命中框同源 */
    double rx = L + SET_RIGHT_X * W;
    double rw = SET_RIGHT_W * W;
    double icon = H * ABOUT_ICON_R;
    double ix = rx, iy = H * ABOUT_ICON_Y;
    double tx = ix + icon + W * ABOUT_HEAD_GAP;
    double docH, viewH, maxS, thumbH, thumbY;

    (void)L;

    /* 头部：固定不滚 */
    draw_program_icon(a, ix, iy, icon, k);
    font_draw_label(&a->fonts, c, L"Qisland", namePx,
                    tx, iy + icon * 0.36, 0,
                    ABOUT_HEAD_R, ABOUT_HEAD_G, ABOUT_HEAD_B, k);
    /* 仓库链接：文字与下划线共用 b->aboutRepo 这一份命中框（ui.c 的
       H_ABOUT_REPO 也用它），字号按可用宽逐 px 收缩，下划线只画到实际字宽。
       整行范围内点击都会由 sys_open_repo() 打开系统默认浏览器。 */
    {
        const SBox *repo = &b->aboutRepo;
        const WCHAR *url = QISLAND_REPO_URL;
        int len = (int)wcslen(url);
        int rp = repoPx;
        int tw = font_measure(&a->fonts, url, len, rp, 2);
        double cy = repo->y + repo->h * 0.5;
        double avail = repo->w - 6.0;
        while (rp > ABOUT_REPO_PX_MIN && tw > (int)avail) {
            rp--;
            tw = font_measure(&a->fonts, url, len, rp, 2);
        }
        if (tw > (int)avail) tw = (int)avail;
        font_draw_label(&a->fonts, c, url, rp, repo->x, cy, 0,
                        ABOUT_REPO_R, ABOUT_REPO_G, ABOUT_REPO_B, k * 0.95);
        canvas_round(c, repo->x, cy + rp * 0.62, (double)tw, 1.0, 0.5,
                     ABOUT_REPO_R, ABOUT_REPO_G, ABOUT_REPO_B, k * 0.70);
    }
    canvas_round(c, rx, H * ABOUT_HR_Y, rw, 1.0, 0.5,
                 DIM_R, DIM_G, DIM_B, k * 0.28);

    /* 文档高度两遍同源：先量后画 */
    docH  = about_doc(a, body, H, px, titlePx, 0, k);
    viewH = body->h;
    a->settings.aboutDocH = docH;
    maxS = docH - viewH;
    if (maxS < 0.0) maxS = 0.0;
    if (a->settings.aboutScroll > maxS) a->settings.aboutScroll = maxS;
    if (a->settings.aboutScroll < 0.0) a->settings.aboutScroll = 0.0;

    canvas_set_clip(c, (int)body->x, (int)body->y,
                    (int)(body->x + body->w), (int)(body->y + body->h));
    about_doc(a, body, H, px, titlePx, 1, k);
    canvas_reset_clip(c);

    /* 右缘一条极淡圆条：仅内容超出时显示，不画轨道方块和箭头 */
    if (maxS > 0.5) {
        thumbH = qs_clamp(viewH * viewH / docH, 24.0, viewH);
        thumbY = body->y + (viewH - thumbH) *
                 (a->settings.aboutScroll / maxS);
        canvas_round(c, body->x + body->w + H * ABOUT_SCROLL_GAP * 0.35,
                     thumbY, 3.0, thumbH, 1.5,
                     DIM_R, DIM_G, DIM_B, k * 0.55);
    }
}

void about_wheel(App *a, double viewH, int dir)
{
    double maxS = a->settings.aboutDocH - viewH;
    double s;
    if (maxS < 0.0) maxS = 0.0;
    s = a->settings.aboutScroll + (double)dir * viewH * ABOUT_WHEEL_STEP;
    if (s < 0.0) s = 0.0;
    if (s > maxS) s = maxS;
    a->settings.aboutScroll = s;
}
