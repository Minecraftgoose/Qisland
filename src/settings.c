/* settings.c — 设置页（唯一一张超大卡片） */
#include "settings.h"
#include "app.h"
#include "config.h"
#include "canvas.h"
#include "font.h"
#include "ui.h"
#include "anim.h"
#include "about.h"
#include "update.h"
#include "version.h"

/* 路径与持久化 */
#define SET_MAGIC   0x51455354u   /* 'QSET' */
#define SET_VERSION 2u            /* v2 = 单选 mode； v1 = 旧版位掩码，读时迁移 */

static void settings_path(WCHAR *out, size_t cap)
{
    WCHAR app[MAX_PATH];
    DWORD k = GetEnvironmentVariableW(L"APPDATA", app, MAX_PATH);
    if (!k || k >= MAX_PATH) {
        GetTempPathW(MAX_PATH, app);
        k = (DWORD)wcslen(app);
    }
    app[k] = 0;
    {
        WCHAR dir[MAX_PATH];
        _snwprintf(dir, MAX_PATH, L"%s\\Qisland", app);
        dir[MAX_PATH - 1] = 0;
        CreateDirectoryW(dir, NULL);   /* 已存在时忽略 ERROR_ALREADY_EXISTS */
    }
    _snwprintf(out, (int)cap, L"%s\\Qisland\\%s", app, SET_FILE);
    out[cap - 1] = 0;
}

/* v1 的 mask 是「可并存开关」； 升级成单选时按优先级取一个， mask==0（旧版「永不收起」）映射到 SET_MODE_NEVER，行为不突变。 */
static int mask_to_mode(DWORD mask)
{
    if (mask & 0x01u) return SET_MODE_LEAVE;
    if (mask & 0x02u) return SET_MODE_OUTSIDE;
    if (mask & 0x04u) return SET_MODE_FIRST;
    return SET_MODE_NEVER;
}

static void settings_load(Settings *s)
{
    WCHAR path[MAX_PATH];
    HANDLE h;
    DWORD magic = 0, ver = 0, a = 0, b = 0, got = 0;

    settings_path(path, MAX_PATH);
    h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL,
                    OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return;
    if (!ReadFile(h, &magic, 4, &got, NULL) || got != 4) goto done;
    if (!ReadFile(h, &ver,   4, &got, NULL) || got != 4) goto done;
    if (!ReadFile(h, &a,     4, &got, NULL) || got != 4) goto done;
    if (!ReadFile(h, &b,     4, &got, NULL) || got != 4) goto done;
    if (magic != SET_MAGIC) goto done;

    s->tab = (a == SET_TAB_ABOUT) ? SET_TAB_ABOUT
           : (a == SET_TAB_UPDATE) ? SET_TAB_UPDATE
           : SET_TAB_GENERAL;
    if (ver == 1) {
        s->collapseMode = mask_to_mode(b);
    } else if (ver == SET_VERSION) {
        /* 单选必选：越界值一律回默认，永远保证恰好一个 ON */
        s->collapseMode = ((int)b >= 0 && (int)b < SET_MODE_COUNT)
                          ? (int)b : SET_MODE_LEAVE;
    }
done:
    CloseHandle(h);
}

static void settings_save(const Settings *s)
{
    WCHAR path[MAX_PATH];
    HANDLE h;
    DWORD magic = SET_MAGIC, ver = SET_VERSION;
    DWORD tab = (DWORD)s->tab, mode = (DWORD)s->collapseMode, wrote = 0;

    settings_path(path, MAX_PATH);
    h = CreateFileW(path, GENERIC_WRITE, FILE_SHARE_READ, NULL,
                    CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return;
    WriteFile(h, &magic, 4, &wrote, NULL);
    WriteFile(h, &ver,   4, &wrote, NULL);
    WriteFile(h, &tab,   4, &wrote, NULL);
    WriteFile(h, &mode,  4, &wrote, NULL);
    CloseHandle(h);
}

static int autostart_read(void)
{
    HKEY k;
    DWORD type = 0, cb = 0;
    LONG r = RegOpenKeyExW(HKEY_CURRENT_USER, SET_RUN_KEY, 0,
                           KEY_QUERY_VALUE, &k);
    if (r != ERROR_SUCCESS) return 0;
    r = RegQueryValueExW(k, SET_RUN_VAL, NULL, &type, NULL, &cb);
    RegCloseKey(k);
    return (r == ERROR_SUCCESS) ? 1 : 0;
}

static int autostart_write(int on)
{
    HKEY k;
    LONG r;
    WCHAR exe[MAX_PATH], cmd[MAX_PATH + 4];

    if (!on) {
        r = RegOpenKeyExW(HKEY_CURRENT_USER, SET_RUN_KEY, 0,
                          KEY_SET_VALUE, &k);
        if (r != ERROR_SUCCESS) return 1;    /* 键不存在 = 本来就没开 */
        r = RegDeleteValueW(k, SET_RUN_VAL);
        RegCloseKey(k);
        return (r == ERROR_SUCCESS || r == ERROR_FILE_NOT_FOUND) ? 1 : 0;
    }
    if (!GetModuleFileNameW(NULL, exe, MAX_PATH)) return 0;
    exe[MAX_PATH - 1] = 0;
    _snwprintf(cmd, MAX_PATH + 4, L"\"%s\"", exe);
    cmd[MAX_PATH + 3] = 0;
    r = RegCreateKeyExW(HKEY_CURRENT_USER, SET_RUN_KEY, 0, NULL, 0,
                        KEY_SET_VALUE, NULL, &k, NULL);
    if (r != ERROR_SUCCESS) return 0;
    r = RegSetValueExW(k, SET_RUN_VAL, 0, REG_SZ, (const BYTE *)cmd,
                       (DWORD)((wcslen(cmd) + 1) * sizeof(WCHAR)));
    RegCloseKey(k);
    return (r == ERROR_SUCCESS) ? 1 : 0;
}

static void sw_target(const Settings *s, int idx, double *target)
{
    if (idx == SET_SW_AUTO) *target = s->autostart ? 1.0 : 0.0;
    else *target = (s->collapseMode == idx) ? 1.0 : 0.0;
}

static void sw_snap(Settings *s)
{
    int i;
    for (i = 0; i < SET_SW_N; i++) {
        double t;
        sw_target(s, i, &t);
        s->swCur[i] = t;
        s->swVel[i] = 0.0;
    }
}

int settings_tick(Settings *s, double dt)
{
    const int SUB = 4;
    double h, k, damp;
    int i, j, active = 0;

    if (dt <= 0.0) return 0;
    h = dt / (double)SUB;
    k = SET_SW_K;
    damp = SET_SW_DAMP;

    for (i = 0; i < SET_SW_N; i++) {
        double t;
        sw_target(s, i, &t);
        for (j = 0; j < SUB; j++) {
            s->swVel[i] += (k * (t - s->swCur[i]) - damp * s->swVel[i]) * h;
            s->swCur[i] += s->swVel[i] * h;
        }
        if (fabs(s->swCur[i] - t) < 0.0015 && fabs(s->swVel[i]) < 0.004) {
            s->swCur[i] = t;
            s->swVel[i] = 0.0;
        } else {
            active = 1;
        }
    }
    return active;
}

double settings_sw_value(const Settings *s, int idx)
{
    if (idx < 0 || idx >= SET_SW_N) return 0.0;
    return s->swCur[idx];
}

/* 生命周期 */
void settings_init(Settings *s)
{
    ZeroMemory(s, sizeof(*s));
    s->tab = SET_TAB_GENERAL;
    /* 默认沿用程序原有交互：鼠标离开即收起。 单选必选，永远有一个 ON。 */
    s->collapseMode = SET_MODE_LEAVE;
    settings_load(s);            /* 兼容 v1 位掩码文件，自动迁移成单选 */
    s->autostart = autostart_read();
    s->autostartOK = 1;
    sw_snap(s);                  /* 启动不播放动画，直接落在正确状态 */
}

/* 几何（绘制 / 命中唯一来源） */
static void row_boxes(double W, double H, double rx, double rw,
                      double y, double hh, SBox *row, SBox *sw)
{
    *row = (SBox){ rx, y, rw, hh };
    *sw  = (SBox){ rx + rw - SET_SW_W * W, y + (hh - SET_SW_H * H) * 0.5,
                   SET_SW_W * W, SET_SW_H * H };
}

void settings_boxes(double W, double H, double L, SetBoxes *b)
{
    double lx = L + SET_LEFT_X * W;
    double rx = L + SET_RIGHT_X * W;
    double rw = SET_RIGHT_W * W;
    int i;

    ZeroMemory(b, sizeof(*b));

    b->tabGeneral = (SBox){ lx, H * SET_TAB_Y0,
                            SET_LEFT_W * W, H * SET_TAB_H };
    b->tabUpdate  = (SBox){ lx, H * (SET_TAB_Y0 + SET_TAB_DY),
                            SET_LEFT_W * W, H * SET_TAB_H };
    b->tabAbout   = (SBox){ lx, H * (SET_TAB_Y0 + 2.0 * SET_TAB_DY),
                            SET_LEFT_W * W, H * SET_TAB_H };
    b->dividerX   = L + SET_DIV_X * W;
    b->dividerY0  = H * SET_DIV_Y0;
    b->dividerY1  = H * SET_DIV_Y1;

    b->groupBox   = (SBox){ rx, H * SET_GROUP_Y, rw, H * SET_GROUP_H };

    /* 关于页滚动内容框：右缘留一条窄缝放滚动条，底部避开 0.93H 横排圆点。 绘制（about.c）与滚轮判定（ui.c wheel_core）都用这一个盒子。 */
    b->aboutBody  = (SBox){ rx, H * ABOUT_BODY_Y,
                            rw - H * ABOUT_SCROLL_GAP, H * ABOUT_BODY_H };

    /* 仓库链接：头部固定不滚，整行可点。 命中框与 about.c 的文字绘制共用 b->aboutRepo / b->repoPx，字号按标称 H0 算，动画期间不重排。 */
    {
        double icon = H * ABOUT_ICON_R;
        double iy   = H * ABOUT_ICON_Y;
        double txL  = rx + icon + W * ABOUT_HEAD_GAP;
        int rp = qs_iclamp((int)(H * ABOUT_REPO_PX),
                           ABOUT_REPO_PX_MIN, ABOUT_REPO_PX_MAX);
        double cyL = iy + icon * 0.78;
        double wL  = (rx + rw) - txL - H * ABOUT_SCROLL_GAP * 0.5;
        if (wL < H * 0.10) wL = H * 0.10;
        b->aboutRepo = (SBox){ txL, cyL - rp * 0.75, wL, rp * 1.50 };
        b->repoPx = rp;
    }

    /* 四行收起条件（单选） */
    for (i = 0; i < SET_MODE_COUNT; i++)
        row_boxes(W, H, rx, rw, H * (SET_ROW_Y0 + i * SET_ROW_DY),
                  H * SET_ROW_H, &b->rows[i], &b->sws[i]);

    b->fineLineY = H * SET_LINE_Y;
    b->fineX0 = rx;
    b->fineX1 = rx + rw;
    row_boxes(W, H, rx, rw, H * SET_AUTO_Y, H * SET_ROW_H,
              &b->rows[SET_SW_AUTO], &b->sws[SET_SW_AUTO]);

    /* 更新页：按钮与结果区同用右栏坐标，绘制 / 命中 / 滚轮判定同源 */
    b->updateBtn  = (SBox){ rx + rw * 0.02, H * UPD_BTN_Y,
                            rw * UPD_BTN_W_RATIO, H * UPD_BTN_H };
    b->updateBody = (SBox){ rx, H * UPD_BODY_Y,
                            rw - H * UPD_SCROLL_GAP, H * UPD_BODY_H };

    b->tabPx   = qs_iclamp((int)(H * SET_TAB_PX_RATIO),   SET_TAB_PX_MIN,   SET_TAB_PX_MAX);
    b->groupPx = qs_iclamp((int)(H * SET_GROUP_PX_RATIO), SET_GROUP_PX_MIN, SET_GROUP_PX_MAX);
    b->rowPx   = qs_iclamp((int)(H * SET_ROW_PX_RATIO),   SET_ROW_PX_MIN,   SET_ROW_PX_MAX);
    b->updTitlePx = qs_iclamp((int)(H * UPD_TITLE_PX_RATIO), UPD_TITLE_PX_MIN, UPD_TITLE_PX_MAX);
    b->updVerPx   = qs_iclamp((int)(H * UPD_VER_PX_RATIO),   UPD_VER_PX_MIN,   UPD_VER_PX_MAX);
    b->updBtnPx   = qs_iclamp((int)(H * UPD_BTN_PX_RATIO),   UPD_BTN_PX_MIN,   UPD_BTN_PX_MAX);
    b->updTextPx  = qs_iclamp((int)(H * UPD_TEXT_PX_RATIO),  UPD_TEXT_PX_MIN,  UPD_TEXT_PX_MAX);
}

/* 绘制 轨道颜色和圆点位置都由弹簧值 v 连续插值，不按整数状态硬跳； v 允许轻微过冲（0<v<1 之外一点点），显示时夹到 [0,1]。 */
static void draw_switch(Canvas *c, const SBox *sw, double v, double k)
{
    double vv = qs_clamp(v, 0.0, 1.0);
    double rad = sw->h * 0.34;
    double pad = sw->h * 0.14;
    double cy  = sw->y + sw->h * 0.5;
    double cx  = sw->x + rad + pad +
                 (sw->w - 2.0 * (rad + pad)) * vv;
    int r = (int)(TRACK_R + (SET_ON_R - TRACK_R) * vv + 0.5);
    int g = (int)(TRACK_G + (SET_ON_G - TRACK_G) * vv + 0.5);
    int bb = (int)(TRACK_B + (SET_ON_B - TRACK_B) * vv + 0.5);

    canvas_round(c, sw->x, sw->y, sw->w, sw->h, sw->h * 0.5,
                 r, g, bb, k * (0.85 + 0.07 * vv));
    /* 圆点经过中点时略胀一点，给出「弹到位」的手感 */
    canvas_disc(c, cx, cy, rad * (1.0 + 0.10 * (1.0 - fabs(vv * 2.0 - 1.0))),
                INK_R, INK_G, INK_B, k);
}

static void draw_row(App *a, const SetBoxes *b, const WCHAR *label,
                     int idx, double k)
{
    const SBox *row = &b->rows[idx];
    const SBox *sw  = &b->sws[idx];
    double v = settings_sw_value(&a->settings, idx);
    int on = (v > 0.5);

    font_draw_label(&a->fonts, &a->canvas, label, b->rowPx,
                    row->x, row->y + row->h * 0.5, 0,
                    on ? INK_R : DIM_R, on ? INK_G : DIM_G,
                    on ? INK_B : DIM_B, k);
    draw_switch(&a->canvas, sw, v, k);
}

static void draw_tab(App *a, const SetBoxes *b, const WCHAR *label,
                     const SBox *tab, int on, double k)
{
    Canvas *c = &a->canvas;
    double cy = tab->y + tab->h * 0.5;
    if (on) {
        canvas_round(c, tab->x - SET_TAB_BAR_GAP,
                     cy - tab->h * 0.30, 2.4, tab->h * 0.60, 1.2,
                     SET_ON_R, SET_ON_G, SET_ON_B, k);
    }
    font_draw_label(&a->fonts, c, label, b->tabPx,
                    tab->x, cy, 0,
                    on ? INK_R : DIM_R, on ? INK_G : DIM_G,
                    on ? INK_B : DIM_B, k * (on ? 1.0 : 0.78));
}

/* 更新页右栏：当前版本 + 按钮 + 可滚动结果 结果区按 \n 与可用宽度软换行； 两遍（先量高后绘制）同源，滚动条不会错位。 */
static double update_doc(App *a, const WCHAR *txt, const SBox *body,
                         int px, double scroll, int draw, double k)
{
    const WCHAR *p = txt;
    double lineH = px * UPD_LINE_GAP;
    double y = body->y - scroll;
    int maxChars = (int)(body->w / (px * 0.62));
    int lines = 0;
    WCHAR line[1024];

    if (maxChars < 8) maxChars = 8;
    if (maxChars > 1000) maxChars = 1000;
    if (!txt || !txt[0]) return lineH;

    while (*p) {
        int n = 0;
        while (p[n] && p[n] != L'\n' && n < maxChars) n++;
        if (n > 0) {
            if (draw) {
                int i;
                for (i = 0; i < n; i++) line[i] = p[i];
                line[n] = 0;
                font_draw_label(&a->fonts, &a->canvas, line, px,
                                body->x, y + lineH * 0.5, 0,
                                INK_R, INK_G, INK_B, k);
            }
            y += lineH;
            lines++;
            p += n;
        }
        if (*p == L'\n') p++;
    }
    return (double)lines * lineH;
}

static void draw_update(App *a, double H, const SetBoxes *b, double k)
{
    Canvas *c = &a->canvas;
    const SBox *btn = &b->updateBtn;
    const SBox *body = &b->updateBody;
    const WCHAR *txt = update_result_text();
    double docH, viewH, maxS, thumbH, thumbY, scroll;

    if (!txt[0]) txt = L"点击「检查更新」查询最新版本。";

    font_draw_label(&a->fonts, c, L"当前版本", b->updTitlePx,
                    body->x, H * UPD_TITLE_Y, 0,
                    ABOUT_LABEL_R, ABOUT_LABEL_G, ABOUT_LABEL_B, k);
    font_draw_label(&a->fonts, c, QI_VERSION_UI_W, b->updVerPx,
                    body->x, H * UPD_VER_Y, 0,
                    INK_R, INK_G, INK_B, k);

    /* 按钮按状态显示检查 / 下载 / 退出并更新，下载时内部就是进度条 */
    {
        int st = update_state();
        int pm = update_progress_permille();
        DWORD by = update_progress_bytes();
        const WCHAR *label;
        WCHAR prog[64];
        double fill = 0.0;

        if (st == UPD_ST_DOWNLOADING) {
            if (pm > 0) {
                fill = (double)pm / 1000.0;
                _snwprintf(prog, 64, L"下载中… %d%%", pm / 10);
            } else {
                _snwprintf(prog, 64, L"下载中… %.1f MB",
                           (double)by / (1024.0 * 1024.0));
            }
            prog[63] = 0;
            label = prog;
        } else if (st == UPD_ST_CHECKING) label = L"检查中…";
        else if (st == UPD_ST_READY)     label = L"下载更新";
        else if (st == UPD_ST_DONE)      label = L"退出Qisland并更新";
        else                              label = L"检查更新";

        canvas_round(c, btn->x, btn->y, btn->w, btn->h, btn->h * 0.5,
                     UPD_BTN_R, UPD_BTN_G, UPD_BTN_B,
                     k * (st == UPD_ST_CHECKING ? 0.45 : 0.85));
        if (fill > 0.0) {
            double fw = btn->w * fill;
            double rr = fw * 0.5 < btn->h * 0.5 ? fw * 0.5 : btn->h * 0.5;
            canvas_round(c, btn->x, btn->y, fw, btn->h, rr,
                         SET_ON_R, SET_ON_G, SET_ON_B, k * 0.95);
        }
        font_draw_label(&a->fonts, c, label, b->updBtnPx,
                        btn->x + btn->w * 0.5, btn->y + btn->h * 0.5, 1,
                        INK_R, INK_G, INK_B, k);
    }

    docH  = update_doc(a, txt, body, b->updTextPx, 0.0, 0, k);
    viewH = body->h;
    update_result_set_doch(docH, viewH);
    scroll = update_result_scroll();
    maxS = docH - viewH;
    if (maxS < 0.0) maxS = 0.0;

    canvas_set_clip(c, (int)body->x, (int)body->y,
                    (int)(body->x + body->w), (int)(body->y + body->h));
    update_doc(a, txt, body, b->updTextPx, scroll, 1, k);
    canvas_reset_clip(c);

    if (maxS > 0.5) {
        thumbH = qs_clamp(viewH * viewH / docH, 24.0, viewH);
        thumbY = body->y + (viewH - thumbH) * (scroll / maxS);
        canvas_round(c, body->x + body->w + H * UPD_SCROLL_GAP * 0.35,
                     thumbY, 3.0, thumbH, 1.5,
                     DIM_R, DIM_G, DIM_B, k * 0.55);
    }
}

void settings_draw(App *a, double W, double H, double L,
                   const SetBoxes *b, double k)
{
    Canvas *c = &a->canvas;
    const Settings *s = &a->settings;

    /* 左右栏之间一条极淡竖线； 不用框、不用纸块 */
    canvas_round(c, b->dividerX, b->dividerY0, 1.0,
                 b->dividerY1 - b->dividerY0, 0.5,
                 DIM_R, DIM_G, DIM_B, k * 0.32);

    draw_tab(a, b, L"通用", &b->tabGeneral,
             s->tab == SET_TAB_GENERAL, k);
    draw_tab(a, b, L"更新", &b->tabUpdate,
             s->tab == SET_TAB_UPDATE, k);
    draw_tab(a, b, L"关于", &b->tabAbout,
             s->tab == SET_TAB_ABOUT, k);

    /* 「关于」页：图标 + 名称 + 仓库 + 四个胶囊板块，内容可滚轮滚动。 */
    if (s->tab == SET_TAB_ABOUT) {
        about_draw(a, W, H, L, b, k);
        return;
    }
    /* 「更新」页：当前版本 + 检查更新按钮 + 结果区，通用页的开关一个都不画。 */
    if (s->tab == SET_TAB_UPDATE) {
        draw_update(a, H, b, k);
        return;
    }

    font_draw_label(&a->fonts, c, L"收起岛设置", b->groupPx,
                    b->groupBox.x,
                    b->groupBox.y + b->groupBox.h * 0.5, 0,
                    INK_R, INK_G, INK_B, k);

    draw_row(a, b, L"鼠标离开岛收起",   SET_MODE_LEAVE,   k);
    draw_row(a, b, L"鼠标点击岛外收起", SET_MODE_OUTSIDE, k);
    draw_row(a, b, L"岛回到第一个卡片时收起", SET_MODE_FIRST, k);
    draw_row(a, b, L"永不收起",         SET_MODE_NEVER,   k);

    canvas_round(c, b->fineX0, b->fineLineY, b->fineX1 - b->fineX0,
                 1.0, 0.5, DIM_R, DIM_G, DIM_B, k * 0.22);

    draw_row(a, b, L"开机自启动", SET_SW_AUTO, k);
}

/* 点击 */
static void relayout(App *a)
{
    double w, h, r;
    anim_geometry(&a->anim, &w, &h, &r);
    ui_layout(a, w * a->u, h * a->u);
}

void settings_click(App *a, int id, int x, int y)
{
    Settings *s = &a->settings;
    (void)x; (void)y;

    /* 更新页主按钮：按当前状态分派检查 / 下载 / 退出并更新 */
    if (id == H_SET_UPDATE_BTN) {
        update_button_click(a->hwnd);
        a->dirty = 1;
        return;
    }

    
    if (id == H_SET_TAB_GEN || id == H_SET_TAB_UPDATE || id == H_SET_TAB_ABOUT) {
        s->aboutScroll = 0.0;
        update_result_reset_scroll();
    }

    switch (id) {
    case H_SET_TAB_GEN:    s->tab = SET_TAB_GENERAL; break;
    case H_SET_TAB_UPDATE: s->tab = SET_TAB_UPDATE;  break;
    case H_SET_TAB_ABOUT:  s->tab = SET_TAB_ABOUT;   break;
    /* 单选必选：点任一项直接设为当前模式； 再点已选中项也不清空， 所以四个里永远恰好一个 ON，不存在「全不选」。 */
    case H_SET_LEAVE:     s->collapseMode = SET_MODE_LEAVE;   break;
    case H_SET_OUTSIDE:   s->collapseMode = SET_MODE_OUTSIDE; break;
    case H_SET_FIRST:     s->collapseMode = SET_MODE_FIRST;   break;
    case H_SET_NEVER:     s->collapseMode = SET_MODE_NEVER;   break;
    case H_SET_AUTOSTART:
        s->autostartOK = autostart_write(!s->autostart);
        /* 以注册表为唯一真值，写失败就不改界面状态 */
        s->autostart = autostart_read();
        break;
    default: return;
    }
    settings_save(s);
    relayout(a);          /* 切页签后立刻刷新命中区，不留一帧空窗 */
    a->dirty = 1;         /* 首帧； 之后由 settings_tick() 驱动到收敛 */
}

/* 纯查询 */
int settings_tab(const Settings *s) { return s->tab; }
int settings_collapse_on_leave(const Settings *s)
{ return s->collapseMode == SET_MODE_LEAVE; }
int settings_collapse_on_outside(const Settings *s)
{ return s->collapseMode == SET_MODE_OUTSIDE; }
int settings_collapse_on_first(const Settings *s)
{ return s->collapseMode == SET_MODE_FIRST; }
int settings_autostart(const Settings *s) { return s->autostart; }
