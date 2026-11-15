/* notify.c — 即时通知：内存态覆盖视图、图标光栅化、两行文本、无持久化 */
#include "notify.h"
#include "app.h"
#include "canvas.h"
#include "font.h"
#include "icons.h"
#include "anim.h"
#include "ui.h"

void notify_init(Notify *n)
{
    ZeroMemory(n, sizeof(*n));
}

int notify_active(const Notify *n) { return n && n->active; }

HICON notify_icon_from_file(const WCHAR *path)
{
    SHFILEINFOW sfi;
    UINT flags = SHGFI_ICON | SHGFI_LARGEICON;
    if (!path || !path[0]) return NULL;
    ZeroMemory(&sfi, sizeof(sfi));
    if (!SHGetFileInfoW(path, 0, &sfi, sizeof(sfi), flags)) return NULL;
    return sfi.hIcon;   /* 调用方 DestroyIcon */
}

/* 弹出：保存现场 → 覆盖尺寸档为中卡片 → 强制显示与展开。
   不写盘、不排队、不做任何文件 IO。 */
static void notify_begin(Notify *n, App *a, DWORD ms)
{
    if (n->active) n->replaced++;
    n->savedView = a->ui.view;
    n->savedMode = a->ui.mode;
    n->forcedVisible = !a->visible;
    n->started = qs_now();
    n->until = n->started + (ms ? ms : NOTIFY_MS_DEFAULT);
    n->active = 1;
    n->shown++;

    /* 通知不进导航表：用 Anim 的尺寸覆盖位把所有 anim_view_size 调用
       统一切到中卡片，切档弹簧照常工作，窗口余量不用重算（中卡最小）。 */
    a->anim.overrideMid = 1;

    if (!a->visible) {
        a->visible = 1;
        ShowWindow(a->hwnd, SW_SHOWNA);
        SetWindowPos(a->hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    }
    a->anim.expanded = 1;
    a->hover = 1;
    a->ui.hoverSeg = -1;
    a->wheelAccum = 0;
    a->dirty = 1;
    {
        double w, h, r;
        anim_geometry(&a->anim, &w, &h, &r);
        ui_layout(a, w * a->u, h * a->u);   /* 立刻刷新命中区，不留一帧空窗 */
    }
}

void notify_show_fa(Notify *n, App *a, unsigned faIcon,
                    const WCHAR *appName, const WCHAR *detail, DWORD ms)
{
    if (!n || !a) return;
    n->faIcon = faIcon;
    n->hIcon = NULL;
    if (appName) {
        wcsncpy(n->appName, appName, NOTIFY_APP_MAX - 1);
        n->appName[NOTIFY_APP_MAX - 1] = 0;
    } else n->appName[0] = 0;
    if (detail) {
        wcsncpy(n->detail, detail, NOTIFY_DETAIL_MAX - 1);
        n->detail[NOTIFY_DETAIL_MAX - 1] = 0;
    } else n->detail[0] = 0;
    notify_begin(n, a, ms);
}

void notify_show_icon(Notify *n, App *a, HICON hIcon,
                      const WCHAR *appName, const WCHAR *detail, DWORD ms)
{
    if (!n || !a) return;
    if (hIcon) {
        n->faIcon = 0;
        n->hIcon = hIcon;
    } else {
        n->faIcon = FA_BELL;      /* 取不到图标就回退，不显示空白 */
        n->hIcon = NULL;
    }
    if (appName) {
        wcsncpy(n->appName, appName, NOTIFY_APP_MAX - 1);
        n->appName[NOTIFY_APP_MAX - 1] = 0;
    } else n->appName[0] = 0;
    if (detail) {
        wcsncpy(n->detail, detail, NOTIFY_DETAIL_MAX - 1);
        n->detail[NOTIFY_DETAIL_MAX - 1] = 0;
    } else n->detail[0] = 0;
    notify_begin(n, a, ms);
}

void notify_dismiss(Notify *n, App *a)
{
    if (!n || !n->active) return;
    n->active = 0;
    n->dismissed++;
    /* 旧版「00:00:00 + bell 常驻提示」已删除：通知就是唯一的提醒形态。
       点掉或过期都清掉内部一次性触发标记，回到完全正常的计时器状态，
       不会残留任何 bell 图标 / 淡圆底 / 数字变色。 */
    ui_clear_end(a);
    a->anim.overrideMid = 0;
    a->ui.view = n->savedView;
    a->ui.mode = n->savedMode;
    if (n->forcedVisible) {
        a->visible = 0;
        a->hover = 0;
        a->anim.expanded = 0;
        ShowWindow(a->hwnd, SW_HIDE);
    } else {
        /* 不强制 hover：下一帧 ui_poll 按真实光标重新判定展开/收起 */
        a->hover = 0;
        a->anim.expanded = 0;
    }
    a->wheelAccum = 0;
    a->dirty = 1;
    {
        double w, h, r;
        anim_geometry(&a->anim, &w, &h, &r);
        ui_layout(a, w * a->u, h * a->u);
    }
}

void notify_tick(Notify *n, App *a, DWORD now)
{
    if (!n->active) return;                 /* 静止时零开销、不置脏 */
    if (now >= n->until) {
        n->expired++;
        notify_dismiss(n, a);
    }
}

/* ---------------- HICON 光栅化：独立 DIB → 覆盖率 → alpha 画布 ---------------- */
static void draw_hicon(Canvas *c, HICON ico,
                       double x, double y, double w, double h, double k)
{
    HDC dc;
    BITMAPINFO bi;
    HBITMAP bmp, oldBmp;
    void *bits = NULL;
    int iw, ih, xx, yy;
    DWORD *px;

    if (!ico || w < 4.0 || h < 4.0) return;
    iw = (int)(w + 0.5); ih = (int)(h + 0.5);
    dc = CreateCompatibleDC(NULL);
    if (!dc) return;
    ZeroMemory(&bi, sizeof(bi));
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = iw;
    bi.bmiHeader.biHeight = -ih;        /* top-down */
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    bmp = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
    if (!bmp || !bits) { if (bmp) DeleteObject(bmp); DeleteDC(dc); return; }
    oldBmp = (HBITMAP)SelectObject(dc, bmp);
    px = (DWORD *)bits;
    /* 不透明黑底：GDI 在 32bpp DIB 上不保证写 alpha，先给一个确定的基底 */
    for (xx = 0; xx < iw * ih; xx++) px[xx] = 0xFF000000u;
    DrawIconEx(dc, 0, 0, ico, iw, ih, 0, NULL, DI_NORMAL);
    GdiFlush();

    for (yy = 0; yy < ih; yy++) {
        for (xx = 0; xx < iw; xx++) {
            DWORD v = px[yy * iw + xx];
            int b = (int)(v & 255), g = (int)((v >> 8) & 255),
                r = (int)((v >> 16) & 255);
            int mx = b > g ? b : g; if (r > mx) mx = r;
            if (mx <= 8) continue;
            /* 黑底上 DrawIconEx 的结果是「颜色 × alpha」；反预乘还原色相，
               再用 mx 当覆盖率叠进画布，边缘连续灰阶、无硬边。 */
            int rr = qs_iclamp(r * 255 / mx, 0, 255);
            int gg = qs_iclamp(g * 255 / mx, 0, 255);
            int bb = qs_iclamp(b * 255 / mx, 0, 255);
            canvas_blend_px(c, (int)(x + xx), (int)(y + yy),
                            rr, gg, bb, (double)mx / 255.0 * k);
        }
    }
    SelectObject(dc, oldBmp);
    DeleteObject(bmp);
    DeleteDC(dc);
}

/* ---------------- 通知卡片绘制：中卡片尺寸，左图标 + 右两行文字 ---------------- */
void notify_draw(Notify *n, App *a, double W, double H, double k)
{
    Canvas *c = &a->canvas;
    FontSet *f = &a->fonts;
    double L = ((double)c->W - W) * 0.5;
    double iconX, iconY, iconW, iconH;
    double tx, tw, cy1, cy2;
    int pxName, pxDetail;
    WCHAR line[NOTIFY_DETAIL_MAX + 1];

    /* 左侧图标区：布局常量在 config.h，绘制与测试同源，避免两处算得不一样。
       TEXT_X 与图标右缘之间留 NOTIFY_GAP_MIN 以上，文字不再贴着图标。 */
    iconW = H * NOTIFY_ICON_S; iconH = iconW;
    iconX = L + W * NOTIFY_ICON_X;
    iconY = H * 0.5 - iconH * 0.5;

    /* 极淡圆底：给图标一个容器感（软件图标/HICON 也一样），不抢文字 */
    canvas_disc(c, iconX + iconW * 0.5, iconY + iconH * 0.5,
               iconH * 0.62, HOVER_BG_R, HOVER_BG_G, HOVER_BG_B, k * 0.42);

    if (n->faIcon && font_fa_ok(f)) {
        int fpx = qs_iclamp((int)(iconH * 0.86), 18, 64);
        font_draw_icon(f, c, n->faIcon, fpx,
                       iconX + iconW * 0.5, iconY + iconH * 0.5, 1,
                       INK_R, INK_G, INK_B, k);
    } else if (n->hIcon) {
        draw_hicon(c, n->hIcon, iconX, iconY, iconW, iconH, k);
    } else {
        /* 两者都没有：极淡圆点占位，不留空白 */
        canvas_disc(c, iconX + iconW * 0.5, iconY + iconH * 0.5,
                    iconH * 0.20, DIM_R, DIM_G, DIM_B, k * 0.5);
    }

    /* 右侧文本区：程序名（大）/ 详细信息（小灰）。
       右端 = TEXT_X + TEXT_W = 0.905W，正好避开右上角 xmark。 */
    tx = L + W * NOTIFY_TEXT_X;
    tw = W * NOTIFY_TEXT_W;
    pxName = qs_iclamp((int)(H * 0.175), 16, 28);
    pxDetail = qs_iclamp((int)(H * 0.125), 12, 20);
    cy1 = H * 0.38;
    cy2 = H * 0.70;

    if (n->appName[0])
        font_draw_label(f, c, n->appName, pxName, tx, cy1, 0,
                        INK_R, INK_G, INK_B, k);

    /* 详细信息按文本区宽度截断补省略号（与粘贴板历史同一套思路） */
    if (n->detail[0]) {
        int len = (int)wcslen(n->detail), take = len;
        WCHAR ell[] = L"\x2026";
        int ellW = font_measure(f, ell, 1, pxDetail, 3);
        int maxW = (int)tw;
        if (font_measure(f, n->detail, len, pxDetail, 3) > maxW) {
            int lo = 0, hi = len, best = 0, room = maxW - ellW;
            if (room < 0) room = 0;
            while (lo <= hi) {
                int mid = (lo + hi) / 2;
                if (font_measure(f, n->detail, mid, pxDetail, 3) <= room) {
                    best = mid; lo = mid + 1;
                } else hi = mid - 1;
            }
            take = best;
            if (take > NOTIFY_DETAIL_MAX - 2) take = NOTIFY_DETAIL_MAX - 2;
            wcsncpy(line, n->detail, take);
            line[take] = L'\x2026';
            line[take + 1] = 0;
        } else {
            wcsncpy(line, n->detail, NOTIFY_DETAIL_MAX);
            line[NOTIFY_DETAIL_MAX] = 0;
        }
        font_draw_label(f, c, line, pxDetail, tx, cy2, 0,
                        DIM_R, DIM_G, DIM_B, k * 0.95);
    }

    /* 右上角关闭：仅图标，无文字；点它 dismiss */
    {
        double cx = L + W * 0.945, cy = H * 0.22;
        int cpx = qs_iclamp((int)(H * 0.10), 12, 20);
        font_draw_icon(f, c, FA_XMARK, cpx, cx, cy, 1,
                       DIM_R, DIM_G, DIM_B, k * 0.75);
    }
}

int notify_shown(const Notify *n) { return n->shown; }
int notify_dismissed(const Notify *n) { return n->dismissed; }
int notify_expired(const Notify *n) { return n->expired; }
const WCHAR *notify_app_name(const Notify *n) { return n->appName; }
const WCHAR *notify_detail(const Notify *n) { return n->detail; }
