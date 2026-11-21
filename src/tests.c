/* tests.c — 真实渲染导出、几何/能力诊断、静止与滚轮端到端测试 */
#include "tests.h"
#include "app.h"
#include "config.h"
#include "canvas.h"
#include "font.h"
#include "sys.h"
#include "anim.h"
#include "countdown.h"
#include "note.h"
#include "perf.h"
#include "clip.h"
#include "notify.h"
#include "balloon.h"
#include "icons.h"
#include "cards.h"
#include "input.h"
#include "ui.h"
#include "window.h"

/* 便签右缘滚动指示区域的亮像素数（空态应为 0，溢出时应 > 0） */
static int note_bar_lit(const App *a, NoteBox nb)
{
    const DWORD *bits = (const DWORD *)a->canvas.outBits;
    int cw = a->canvas.W, ch = a->canvas.H, lit = 0, xx, yy;
    if (!bits) return 0;
    for (yy = (int)nb.y; yy < (int)(nb.y + nb.h) && yy < ch; yy++)
        for (xx = (int)(nb.x + nb.w - NOTE_PAD - 4);
             xx < (int)(nb.x + nb.w - NOTE_PAD + 2) && xx < cw; xx++) {
            DWORD v = bits[yy * cw + xx];
            int mx = (int)((v >> 16) & 255);
            if ((int)((v >> 8) & 255) > mx) mx = (int)((v >> 8) & 255);
            if ((int)(v & 255) > mx) mx = (int)(v & 255);
            if (((v >> 24) & 255) > 200 && mx > 40) lit++;
        }
    return lit;
}

/* 选中高亮像素：合成后是暗蓝灰（B 明显高于 R/G），白字 R≈G≈B 不计入。
   用它证明「选中指示真的画进了 alpha 画布」，而不是只改了状态量。 */
static int note_sel_lit(const App *a, NoteBox nb)
{
    const DWORD *bits = (const DWORD *)a->canvas.outBits;
    int cw = a->canvas.W, ch = a->canvas.H, lit = 0, xx, yy;
    if (!bits) return 0;
    for (yy = (int)nb.y; yy < (int)(nb.y + nb.h) && yy < ch; yy++)
        for (xx = (int)nb.x; xx < (int)(nb.x + nb.w) && xx < cw; xx++) {
            DWORD v = bits[yy * cw + xx];
            int b = (int)(v & 255), g = (int)((v >> 8) & 255),
                r = (int)((v >> 16) & 255);
            if (((v >> 24) & 255) > 200 && b > r + 30 && b > g + 22)
                lit++;
        }
    return lit;
}

static void snap(App *a, int view, int mode, const char *path)
{
    double w, h, r;
    a->ui.view = view;
    a->ui.mode = mode;
    anim_view_size(&a->anim, view, &w, &h, &r);
    a->anim.curW = w; a->anim.curH = h; a->anim.curR = r;
    a->anim.expanded = 1; a->anim.exp = 1.0; a->anim.expV = 0.0;
    ui_render(a);
    if (path) canvas_dump_bmp(&a->canvas, path);   /* NULL = 只渲染，供像素诊断 */
}

/* 悬浮在 MM 段：淡底高亮，冒号可见 */
static void snap_hover(App *a, const char *path)
{
    double w, h, r;
    a->ui.view = 0; a->ui.mode = 0; a->ui.hoverSeg = 1;
    anim_view_size(&a->anim, 0, &w, &h, &r);
    a->anim.curW = w; a->anim.curH = h; a->anim.curR = r;
    a->anim.expanded = 1; a->anim.exp = 1.0; a->anim.expV = 0.0;
    ui_render(a);
    canvas_dump_bmp(&a->canvas, path);
    a->ui.hoverSeg = -1;
}

/* 倒计时归零后（通知已关闭）的正常计时器：00:00:00 + 标题「倒计时」。
   旧版 bell 图标 / 淡圆底 / 数字变色常驻提示已删除，这张图用来验证这一点。 */
static void snap_end(App *a, const char *path)
{
    double w, h, r;
    a->ui.view = 0; a->ui.mode = 0; a->ui.hoverSeg = -1;
    anim_view_size(&a->anim, 0, &w, &h, &r);
    a->anim.curW = w; a->anim.curH = h; a->anim.curR = r;
    a->anim.expanded = 1; a->anim.exp = 1.0; a->anim.expV = 0.0;
    a->cd.running = 0; a->cd.ended = 1; a->cd.remain = 0.0; a->cd.base = 0.0;
    ui_render(a);
    canvas_dump_bmp(&a->canvas, path);
    a->cd.ended = 0;
    countdown_reset(&a->cd);
}

/* 便签视图快照：view 1。selMode 0=无选区 1=选中第二行 2=跨行选中 */
static void snap_note(App *a, const WCHAR *text, int scrollTop,
                      int selMode, const char *path)
{
    double w, h, r;
    Note save;
    memcpy(&save, &a->note, sizeof(Note));

    a->ui.view = NOTE_VIEW; a->ui.mode = 0; a->ui.hoverSeg = -1;
    note_clear(&a->note);
    if (text && text[0]) note_insert(&a->note, text, (int)wcslen(text));
    a->note.stb.cursor = 0;
    a->note.stb.select_start = a->note.stb.select_end = 0;
    a->note.editing = 1;          /* 选区高亮与光标只在编辑态绘制 */
    anim_view_size(&a->anim, NOTE_VIEW, &w, &h, &r);
    a->anim.curW = w; a->anim.curH = h; a->anim.curR = r;
    a->anim.expanded = 1; a->anim.exp = 1.0; a->anim.expV = 0.0;
    ui_render(a);                 /* 先让 relayout 稳定 sig 和行表 */
    note_scroll(&a->note, scrollTop);
    if (selMode == 1) {
        /* 选中第二行整行（跨行选区在第 1..2 行可见） */
        int L = a->note.lineCount > 1 ? 1 : 0;
        a->note.stb.select_start = a->note.lineStart[L];
        a->note.stb.select_end = a->note.lineEnd[L];
        a->note.stb.cursor = a->note.stb.select_end;
    } else if (selMode == 2) {
        a->note.stb.select_start = 3;
        a->note.stb.select_end = a->note.len - 2;
        a->note.stb.cursor = a->note.stb.select_end;
    }
    ui_render(a);                 /* 再按滚动 / 选区渲染 */
    canvas_dump_bmp(&a->canvas, path);

    memcpy(&a->note, &save, sizeof(Note));
    a->ui.view = 0;
}

void tests_dump(App *a)
{
    char p[512];
    static const WCHAR longText[] =
        L"明天下午三点开会，记得带份材料和充电线。\n"
        L"顺便把上周的草稿整理一下，中文换行要自然，不能画出卡片外面去。\n"
        L"第三行再补一句：滚动条不要系统那种方块和箭头，越淡越好。\n"
        L"第四行：内容继续往下写，验证溢出后的滚动指示会出现。\n"
        L"第五行：最后一行，用来确认到底能不能滚到底。";
    a->anim.expanded = 0; a->anim.exp = 0.0; a->anim.expV = 0.0;
    ui_render(a);
    _snprintf(p, 512, "%s_island.bmp", a->dump);  canvas_dump_bmp(&a->canvas, p);
    _snprintf(p, 512, "%s_mid.bmp", a->dump);     snap(a, 0, 0, p);
    _snprintf(p, 512, "%s_hover.bmp", a->dump);   snap_hover(a, p);
    _snprintf(p, 512, "%s_end.bmp", a->dump);     snap_end(a, p);
    _snprintf(p, 512, "%s_slider.bmp", a->dump);  snap(a, 0, 1, p);
    _snprintf(p, 512, "%s_note_empty.bmp", a->dump); snap_note(a, NULL, 0, 0, p);
    _snprintf(p, 512, "%s_note_long.bmp", a->dump);  snap_note(a, longText, 0, 0, p);
    _snprintf(p, 512, "%s_note_selected.bmp", a->dump); snap_note(a, longText, 0, 1, p);
    _snprintf(p, 512, "%s_note_sel_multi.bmp", a->dump); snap_note(a, longText, 0, 2, p);
    _snprintf(p, 512, "%s_note_scrolled.bmp", a->dump); snap_note(a, longText, 3, 0, p);
    /* 第三张中卡片：固定演示值渲染，保证三个圆环/数字/文字都可复现。
       真实采集链路由 --diag / --perftest 单独验证。 */
    {
        double sc = a->perf.cpu, sm = a->perf.mem, sd = a->perf.disk;
        a->perf.cpu = 42.0; a->perf.mem = 68.0; a->perf.disk = 83.0;
        _snprintf(p, 512, "%s_perf.bmp", a->dump); snap(a, PERF_VIEW, 0, p);
        /* 高负载：三环同时向暖橙插值，展示功能警示态 */
        a->perf.cpu = 96.0; a->perf.mem = 92.0; a->perf.disk = 99.0;
        _snprintf(p, 512, "%s_perf_warn.bmp", a->dump); snap(a, PERF_VIEW, 0, p);
        a->perf.cpu = sc; a->perf.mem = sm; a->perf.disk = sd;
    }
    /* 第一个大卡片 = 剪贴板历史：空态 / 列表 / 选中回填 / 滚动四张真实像素。
       演示数据写到 clipboard_test.bin，不碰用户真实历史。 */
    {
        static const WCHAR *demo[] = {
            L"https://learn.microsoft.com/windows/win32/dataxchg/clipboard",
            L"明天下午三点开会，记得带份材料和充电线",
            L"Qisland clipboard history — 点一条即可直接粘贴",
            L"第一行\n第二行\t制表符会压成空格显示，回填时保留原文",
            L"这是一条特别长的剪贴板内容，用来验证单行截断与省略号不会溢出卡片右边界，后面还有很多很多字",
            L"git commit -m \"clipboard history card\"",
            L"192.168.1.1",
            L"最后一条：滚动到底部时右缘短滑块会跟着移动"
        };
        int i;
        a->clip.testFile = 1;
        clip_clear(&a->clip, NULL);
        _snprintf(p, 512, "%s_clip_empty.bmp", a->dump);
        snap(a, CLIP_VIEW, 0, p);
        for (i = 0; i < 8; i++)
            clip_push_text(&a->clip, demo[i], (int)wcslen(demo[i]));
        _snprintf(p, 512, "%s_clip_list.bmp", a->dump);
        snap(a, CLIP_VIEW, 0, p);
        a->clip.selected = 0;
        _snprintf(p, 512, "%s_clip_selected.bmp", a->dump);
        snap(a, CLIP_VIEW, 0, p);
        a->clip.selected = -1;
        clip_scroll(&a->clip, 3);
        _snprintf(p, 512, "%s_clip_scrolled.bmp", a->dump);
        snap(a, CLIP_VIEW, 0, p);
        clip_scroll(&a->clip, -3);
        clip_clear(&a->clip, NULL);
        a->clip.testFile = 0;
    }
    /* 即时通知卡片：FA 图标 / 软件 HICON 两个版本，都是中卡片尺寸 */
    {
        HICON ico = notify_icon_from_file(L"notepad.exe");
        if (!ico) ico = (HICON)LoadIconW(NULL, IDI_APPLICATION);
        notify_show_fa(&a->notify, a, FA_BELL, L"Qisland",
                       L"倒计时已结束 · 00:00:00", 60000);
        _snprintf(p, 512, "%s_notify_fa.bmp", a->dump);
        snap(a, 0, 0, p);
        if (ico) {
            notify_show_icon(&a->notify, a, ico, L"记事本",
                             L"软件图标示例：左侧显示来源程序 HICON", 60000);
            _snprintf(p, 512, "%s_notify_icon.bmp", a->dump);
            snap(a, 0, 0, p);
            /* 共享图标不销毁；文件图标才需要 */
            if (ico != (HICON)LoadIconW(NULL, IDI_APPLICATION)) DestroyIcon(ico);
        }
        notify_dismiss(&a->notify, a);
    }
    /* Win7 托盘气泡通知：与倒计时通知完全同一张卡片，左侧 FA / HICON，
       右侧「托盘消息」+ 读到的气泡正文（读不到则「有新消息」）。 */
    {
        HICON ico = notify_icon_from_file(L"notepad.exe");
        notify_show_fa(&a->notify, a, FA_COMMENT_DOTS, L"托盘消息",
                       L"这是 Win7 托盘气泡消息 · 测试正文", 60000);
        _snprintf(p, 512, "%s_balloon_fa.bmp", a->dump);
        snap(a, 0, 0, p);
        if (!ico) ico = (HICON)LoadIconW(NULL, IDI_APPLICATION);
        notify_show_icon(&a->notify, a, ico, L"托盘消息", L"有新消息", 60000);
        _snprintf(p, 512, "%s_balloon_icon.bmp", a->dump);
        snap(a, 0, 0, p);
        if (ico != (HICON)LoadIconW(NULL, IDI_APPLICATION)) DestroyIcon(ico);
        notify_dismiss(&a->notify, a);
    }
    /* 横2 大卡片 / 横3 超大卡片 = 最后一点，两个尺寸档都导出真实像素。
       last = 超大卡片的滑块模式，证明最后一点控件照常可用。 */
    _snprintf(p, 512, "%s_big.bmp", a->dump);     snap(a, 3, 0, p);
    _snprintf(p, 512, "%s_huge.bmp", a->dump);    snap(a, 4, 0, p);
    _snprintf(p, 512, "%s_last.bmp", a->dump);
    snap(a, cards_last_card(), 1, p);
}

/* 卡片本体是 12/12/14，指示点是 ACCENT(198) / DIM(108)，
   所以 max(R,G,B)>35 且 alpha>200 就能把「点」和空区域分开。 */
static int lit_at(const App *a, int x, int y)
{
    const DWORD *bits = (const DWORD *)a->canvas.outBits;
    DWORD v; int R, G, B, mx;
    if (x < 0 || y < 0 || x >= a->canvas.W || y >= a->canvas.H) return 0;
    v = bits[y * a->canvas.W + x];
    R = (int)((v >> 16) & 255);
    G = (int)((v >> 8) & 255);
    B = (int)(v & 255);
    mx = R > G ? R : G; if (B > mx) mx = B;
    return (((v >> 24) & 255) > 200 && mx > 35) ? 1 : 0;
}

/* 统计底部横排圆点：每个点中心 ±8px 方框内的亮像素。
   横点以卡片水平中心对称、按 ui_dots_pitch() 密集排列；命中区本身也是
   以同一中心构造的，所以 mid = 命中区中心即是卡片中心。 */
static int hdots_lit_px(const App *a, RECT hr, int n, int *perDot)
{
    double mid = ((double)hr.left + (double)hr.right) * 0.5;
    double cy  = ((double)hr.top + (double)hr.bottom) * 0.5;
    double pitch = ui_dots_pitch((double)a->anim.curH * a->u, a->u);
    double L = 0.0, W = (double)a->anim.curW * a->u, H = (double)a->anim.curH * a->u;
    int total = 0, i, dx, dy;
    if (perDot) for (i = 0; i < n && i < 16; i++) perDot[i] = 0;
    if (n <= 0) return 0;
    for (i = 0; i < n; i++) {
        double cx, cyy;
        int ix, iy;
        ui_hdots_center(L, W, H, pitch, n, i, &cx, &cyy);
        /* ui_hdots_center 里的 mid 是 L+W/2；这里用命中区实测中心平移到画布坐标 */
        ix = (int)(mid + (cx - (L + W * 0.5)) + 0.5);
        iy = (int)(cy + 0.5);
        for (dy = -8; dy <= 8; dy++)
            for (dx = -8; dx <= 8; dx++) {
                int q = lit_at(a, ix + dx, iy + dy);
                total += q;
                if (perDot && i < 16) perDot[i] += q;
            }
    }
    return total;
}

/* 统计左侧竖列点区域的亮像素：横向组应该为 0（整列不画）。 */
static int vstrip_lit_px(const App *a, RECT wr)
{
    int total = 0, x, y;
    for (y = wr.top; y < wr.bottom; y++)
        for (x = wr.left; x < wr.right; x++)
            total += lit_at(a, x, y);
    return total;
}

/* 剪贴板历史：内容区亮像素（文字/图标）、内容区下方溢出像素、
   右缘极淡短滑块亮像素。用来证明文字被 canvas_set_clip 裁住、
   且滚动指示只在溢出时出现。 */
static int clip_lit_px(const App *a, ClipBox cb)
{
    int total = 0, x, y;
    for (y = (int)cb.y; y < (int)(cb.y + cb.h); y++)
        for (x = (int)cb.x; x < (int)(cb.x + cb.w); x++)
            total += lit_at(a, x, y);
    return total;
}
static int clip_spill_px(const App *a, ClipBox cb)
{
    int total = 0, x, y, y0 = (int)(cb.y + cb.h) + 1;
    for (y = y0; y < y0 + 3 && y < a->canvas.H - 12; y++)
        for (x = (int)cb.x; x < (int)(cb.x + cb.w); x++)
            total += lit_at(a, x, y);
    return total;
}
static int clip_thumb_lit_px(const App *a, ClipBox cb)
{
    int total = 0, x, y;
    for (y = (int)cb.y; y < (int)(cb.y + cb.h); y++)
        for (x = (int)(cb.x + cb.w - CLIP_PAD - 5);
             x < (int)(cb.x + cb.w - CLIP_PAD + 2); x++)
            total += lit_at(a, x, y);
    return total;
}

/* 通知卡片：图标区 / 文本区亮像素，以及竖列点、横排点是否一个都没画 */
static int rect_lit_px(const App *a, double x0, double y0, double x1, double y1)
{
    int total = 0, x, y;
    for (y = (int)y0; y < (int)y1 && y < a->canvas.H; y++)
        for (x = (int)x0; x < (int)x1 && x < a->canvas.W; x++)
            total += lit_at(a, x, y);
    return total;
}
static void notify_lit_parts(const App *a, int *icon, int *text)
{
    DBoxNotify body, close;
    double L, W, H;
    ui_notify_boxes(a, &body, &close);
    L = (double)(a->canvas.W - a->anim.curW * a->u) * 0.5;
    W = a->anim.curW * a->u; H = a->anim.curH * a->u;
    (void)body; (void)close;
    /* 图标区 / 文字区坐标与 notify.c 共用 config.h 宏 */
    *icon = rect_lit_px(a, L + W * NOTIFY_ICON_X, H * 0.27,
                        L + W * NOTIFY_ICON_X + H * NOTIFY_ICON_S, H * 0.73);
    *text = rect_lit_px(a, L + W * NOTIFY_TEXT_X, H * 0.24,
                        L + W * (NOTIFY_TEXT_X + NOTIFY_TEXT_W), H * 0.82);
}
static int notify_dots_lit_px(const App *a)
{
    /* 竖列点落在最左 8% 宽；横排点落在底部 HDOTS_Y 附近 */
    double W = a->anim.curW * a->u, H = a->anim.curH * a->u;
    double L = (double)(a->canvas.W - W) * 0.5;
    /* 竖点带只取最左窄条：通知的左侧图标从 0.048W 起，不能把它算进来 */
    int v = rect_lit_px(a, L + W * 0.010, H * 0.18,
                        L + W * 0.045, H * 0.82);
    int h = rect_lit_px(a, L + W * 0.20, H * 0.88,
                        L + W * 0.80, H * 0.99);
    return v + h;
}
static int notify_no_file(void)
{
    WCHAR app[MAX_PATH], pat[MAX_PATH];
    WIN32_FIND_DATAW fd;
    HANDLE h;
    DWORD k = GetEnvironmentVariableW(L"APPDATA", app, MAX_PATH);
    if (!k || k >= MAX_PATH) return 1;
    app[k] = 0;
    _snwprintf(pat, MAX_PATH, L"%s\\Qisland\\notify*", app);
    pat[MAX_PATH - 1] = 0;
    h = FindFirstFileW(pat, &fd);
    if (h == INVALID_HANDLE_VALUE) return 1;   /* 没有任何通知文件 */
    FindClose(h);
    return 0;
}

/* 通知图标「圆形底」检测：以图标中心为圆心的环带
   [0.44*iconH, 0.60*iconH]。旧版在这里画半径 0.62*iconH 的极淡圆底，
   环带必然一片亮；删掉圆底后，环带内既没有图标笔画（FA 字形半宽约
   0.26*iconH）也没有容器 → 亮像素必须为 0。 */
static int notify_halo_lit_px(const App *a)
{
    double W = a->anim.curW * a->u, H = a->anim.curH * a->u;
    double L = (double)(a->canvas.W - W) * 0.5;
    double iw = H * NOTIFY_ICON_S;
    double cx = L + W * NOTIFY_ICON_X + iw * 0.5;
    double cy = H * 0.5;
    double r0 = iw * 0.44, r1 = iw * 0.60;
    int x, y, total = 0;
    for (y = (int)(cy - r1 - 1); y <= (int)(cy + r1 + 1); y++)
        for (x = (int)(cx - r1 - 1); x <= (int)(cx + r1 + 1); x++) {
            double dx = x - cx, dy = y - cy, d = dx * dx + dy * dy;
            if (d >= r0 * r0 && d <= r1 * r1 && lit_at(a, x, y)) total++;
        }
    return total;
}

/* 超大卡片留空检查：内容区（排除底部横点带）亮像素应为 0、横排点仍要画、
   命中表只有滚轮竖条 + 横点两个、卡片中心不命中任何内容控件。 */
static void huge_empty_check(App *a, int *contentLit, int *dotsLit,
                             int *hitN, int *centerHit)
{
    int sv = a->ui.view, sm = a->ui.mode, sh = a->ui.hoverSeg;
    int v = cards_count() - 1;
    double W, H, r, L;
    a->ui.view = v; a->ui.mode = 0; a->ui.hoverSeg = -1;
    anim_view_size(&a->anim, v, &W, &H, &r);
    a->anim.curW = W; a->anim.curH = H; a->anim.curR = r;
    a->anim.expanded = 1; a->anim.exp = 1.0; a->anim.expV = 0.0;
    ui_render(a);
    W = a->anim.curW * a->u; H = a->anim.curH * a->u;
    L = (double)(a->canvas.W - W) * 0.5;
    *contentLit = rect_lit_px(a, L + W * 0.06, H * 0.05,
                              L + W * 0.94, H * 0.82);
    *dotsLit = hdots_lit_px(a, a->ui.hdotsRect,
                            cards_group_count(GRP_H), NULL);
    *hitN = a->ui.hitN;
    *centerHit = ui_hit(a, (int)(L + W * 0.5), (int)(H * 0.45));
    a->ui.view = sv; a->ui.mode = sm; a->ui.hoverSeg = sh;
    a->dirty = 1;
}

/* 通知图标右缘 → 文字起点的间隙（设备像素）。绘制与断言共用 config.h 宏。 */
static double notify_gap_px(const App *a)
{
    double W = MID_W * a->u, H = MID_H * a->u;
    return W * NOTIFY_TEXT_X - (W * NOTIFY_ICON_X + H * NOTIFY_ICON_S);
}

/* 旧版「bell 图标 + 淡圆底 + 数字变色」常驻提示是否真的删干净：
   cd.ended=1 与 cd.ended=0 两次渲染应逐像素完全一致（diff=0）。
   旧实现这里一定会画出一片淡圆底，diff 会很大。 */
static int old_end_hint_diff_px(App *a)
{
    size_t n = (size_t)a->canvas.W * (size_t)a->canvas.H * 4;
    unsigned char *p0 = (unsigned char *)malloc(n);
    unsigned char *p1 = (unsigned char *)malloc(n);
    size_t i;
    int diff = 0, sv = a->ui.view, sm = a->ui.mode, sh = a->ui.hoverSeg;
    double sr = a->cd.remain;
    int srun = a->cd.running, se = a->cd.ended;
    if (!p0 || !p1) { free(p0); free(p1); return -1; }
    a->ui.view = 0; a->ui.mode = 0; a->ui.hoverSeg = -1;
    a->cd.remain = 0.0; a->cd.running = 0;
    a->anim.expanded = 1; a->anim.exp = 1.0; a->anim.expV = 0.0;
    a->cd.ended = 0; ui_render(a); memcpy(p0, a->canvas.outBits, n);
    a->cd.ended = 1; ui_render(a); memcpy(p1, a->canvas.outBits, n);
    for (i = 0; i < n; i++) if (p0[i] != p1[i]) diff++;
    free(p0); free(p1);
    a->ui.view = sv; a->ui.mode = sm; a->ui.hoverSeg = sh;
    a->cd.remain = sr; a->cd.running = srun; a->cd.ended = se;
    a->dirty = 1;
    return diff;
}

/* 滑块模式：play/reset 既不画也不响应点击。
   右侧 0.85W..0.99W 区域在 mode=1 下应当一个亮像素都没有。 */
static void slider_mode_check(App *a, int *hitPlay, int *hitReset, int *rightLit)
{
    int sv = a->ui.view, sm = a->ui.mode, sh = a->ui.hoverSeg;
    double W = a->anim.curW * a->u, H = a->anim.curH * a->u;
    double L = (double)(a->canvas.W - W) * 0.5;
    a->ui.view = 0; a->ui.mode = 1; a->ui.hoverSeg = -1;
    a->anim.expanded = 1; a->anim.exp = 1.0; a->anim.expV = 0.0;
    ui_render(a);
    *hitPlay  = (ui_hit(a, (int)(L + W * 0.891), (int)(H * 0.64)) == H_PLAY);
    *hitReset = (ui_hit(a, (int)(L + W * 0.951), (int)(H * 0.64)) == H_RESET);
    *rightLit = rect_lit_px(a, L + W * 0.85, H * 0.42,
                            L + W * 0.99, H * 0.86);
    a->ui.view = sv; a->ui.mode = sm; a->ui.hoverSeg = sh;
    a->dirty = 1;
}

void tests_diag(App *a)
{
    FILE *f = fopen("diag.txt", "w");
    int i;
    if (!f) return;

    fprintf(f, "screen=%dx%d dpi=%.3f window=%dx%d\n",
            a->scrW, a->scrH, a->u, a->winW, a->winH);
    fprintf(f, "island=%.0fx%.0f r=%.0f\n", ISLAND_W, ISLAND_H, ISLAND_R);
    for (i = 0; i < cards_count(); i++) {
        double w, h, r;
        anim_view_size(&a->anim, i, &w, &h, &r);
        fprintf(f, "view%d = %.1f x %.1f r=%.1f\n", i, w, h, r);
    }
    {
        double w, h, r;
        anim_view_size(&a->anim, cards_last_card(), &w, &h, &r);
        fprintf(f, "overshoot_fit need=%.1fx%.1f window=%.1fx%.1f -> %s\n",
                w * 1.0825, h * 1.0825,
                (double)a->winW / a->u, (double)a->winH / a->u,
                (w * 1.0825 <= (double)a->winW / a->u &&
                 h * 1.0825 <= (double)a->winH / a->u) ? "OK" : "CLIP");
    }
    fprintf(f, "font_awesome_loaded=%d fa_face=%ls fa_selfcheck=%d glyph_ss=%d\n",
            a->fonts.loaded, font_fa_face(&a->fonts), font_fa_ok(&a->fonts),
            GLYPH_SS);
    fprintf(f, "capability volume=%d brightness_wmi=%d brightness_ddci=%d rawinput=%d\n",
            a->sys.volOK, a->sys.wmiOK, a->sys.ddcOK, a->rawOK);
    fprintf(f, "ui_text_policy: only_label=倒计时 digits=HH:MM:SS colon=two_sdf_dots buttons=icon_only no_button_trackbars\n");
    fprintf(f, "mode_policy: 0=timer 1=timer_area_becomes_vol+bri_sliders, triggered by BRI or VOL click\n");

    /* 卡片表与导航：全部由 cards[] 推导，加卡片只改那张表 */
    {
        int n = cards_count(), k, nBig = 0, nHuge = 0;
        for (k = 0; k < n; k++) {
            if (cards_tier(k) == TIER_BIG)  nBig++;
            if (cards_tier(k) == TIER_HUGE) nHuge++;
        }
        fprintf(f, "cards_nav: count=%d vdots=%d hgroups=%d (only ONE horizontal) "
                   "last_card=%d big_cards=%d huge_cards=%d\n",
                n, CARDS_VDOTS, cards_hgroup_count(), cards_last_card(), nBig, nHuge);
        fprintf(f, "cards_table: idx id tier draw group slot vdot default\n");
        for (k = 0; k < n; k++) {
            const CardDef *cd = cards_at(k);
            static const char *tn[] = {"MID", "BIG", "HUGE"};
            static const char *dn[] = {"TIMER", "NOTE", "PERF", "EMPTY", "CLIP"};
            static const char *gn[] = {"V", "H"};
            fprintf(f, "  card%d %-6s %-4s %-5s %s slot=%d vdot=%d def=%d\n",
                    k, cd->id, tn[cd->tier], dn[cd->draw], gn[cd->group],
                    cd->slot, cd->vdot, cd->isDefault);
        }
        for (k = 0; k < CARDS_VDOTS; k++)
            fprintf(f, "  vdot%d anchor=%d\n", k, cards_anchor(k));
        fprintf(f, "nav_direction: down(delta<0)=next, up(delta>0)=prev "
                   "(fixed from the reversed old behavior)\n");
        fprintf(f, "nav_assert: down_from_0=%d up_from_1=%d "
                   "h1_is_v3=%d h2_is_v4=%d "
                   "only_one_hgroup=%d one_big=%d one_huge=%d "
                   "big_before_huge=%d last_is_huge=%d last_is_last_dot=%d "
                   "hdotsN=%d hmove_last_clamped=%d "
                   "V_linear_1to2=%d H_linear_2to3=%d H_linear_3to4=%d "
                   "end_clamped=%d\n",
                cards_next(0, +1), cards_next(1, -1),
                (cards_group(cards_anchor(2)) == GRP_H &&
                 cards_slot(cards_anchor(2)) == 0),
                (cards_group(cards_anchor(3)) == GRP_H &&
                 cards_slot(cards_anchor(3)) == 1),
                cards_hgroup_count() == 1,
                nBig == 1,
                nHuge == 1,
                cards_tier(3) == TIER_BIG && cards_tier(4) == TIER_HUGE,
                cards_tier(cards_last_card()) == TIER_HUGE,
                cards_slot(cards_last_card()) == cards_group_count(GRP_H) - 1,
                cards_group_count(GRP_H),
                cards_hmove(cards_last_card(), +1) == cards_last_card(),
                cards_next(1, +1) == 2,
                cards_next(2, +1) == 3,
                cards_next(3, +1) == 4,
                cards_next(4, +1) == 4);
        fprintf(f, "nav_expect: down_from_0=1 up_from_1=0 h1_is_v3=1 h2_is_v4=1 "
                   "only_one_hgroup=1 one_big=1 one_huge=1 big_before_huge=1 "
                   "last_is_huge=1 last_is_last_dot=1 hdotsN=3 "
                   "hmove_last_clamped=1 V_linear_1to2=1 H_linear_2to3=1 "
                   "H_linear_3to4=1 end_clamped=1\n");
        fprintf(f, "nav_horizontal: WM_MOUSEHWHEEL / Shift+wheel calls cards_hmove() "
                   "inside the ONE horizontal group (横1 perf -> 横2 big1 -> 横3 huge1)\n");
        fprintf(f, "nav_extensible: adding a card = one row in cards[]; "
                   "dots/sizes/wheel sequence/diag all derive from the table\n");
    }

    /* ---- 大卡片 + 超大卡片共存，但仍**只有一个横向组** ----
       横1 性能 / 横2 大卡片 / 横3 超大卡片（最后一点）；
       竖④ 锚点 = slot1 大卡片 =「横向第二个」。
       big2/3/4、huge0/2、第二个横向组 GRP_U 删除；用户本轮要求
       「超大卡片留空」→ huge1 改为 DRAW_EMPTY，因此 nEmpty 应为 1。 */
    {
        int nH = cards_group_count(GRP_H);
        int d  = cards_group_default(GRP_H);
        int first = cards_group_first(GRP_H);
        int last  = cards_group_last(GRP_H);
        int n  = cards_count(), k, nBig = 0, nHuge = 0, nEmpty = 0;
        for (k = 0; k < n; k++) {
            if (cards_tier(k) == TIER_BIG)  nBig++;
            if (cards_tier(k) == TIER_HUGE) nHuge++;
            if (cards_draw(k) == DRAW_EMPTY) nEmpty++;
        }
        {
            int okOneGroup   = (cards_hgroup_count() == 1);
            int okOrder      = (cards_group_by_order(0) == GRP_V &&
                                cards_group_by_order(1) == GRP_H);
            int okThreeDots  = (nH == 3);
            int okOneBig     = (nBig == 1);
            int okOneHuge    = (nHuge == 1 && nEmpty == 1 &&
                                cards_draw(last) == DRAW_EMPTY);
            int okBigBefore  = (cards_tier(3) == TIER_BIG &&
                                cards_tier(4) == TIER_HUGE);
            int okLastIsHuge = (last == 4 && cards_tier(last) == TIER_HUGE);
            int okLastIsEnd  = (cards_slot(last) == nH - 1);
            int okV4IsH2     = (cards_anchor(3) == 3 && cards_slot(3) == 1);
            int okDefault    = (d == 3 && cards_slot(d) == 1);
            int okHmove      = (cards_hmove(d, -1) == first) &&
                               (cards_hmove(d, +1) == last) &&
                               (cards_hmove(last, +1) == last);
            int okLinear     = (cards_next(0,+1) == 1) &&
                               (cards_next(1,+1) == 2) &&
                               (cards_next(2,+1) == 3) &&
                               (cards_next(3,+1) == 4) &&
                               (cards_next(4,+1) == 4);

            fprintf(f, "single_hgroup_policy: ONE horizontal group only, BOTH a BIG "
                       "card and a HUGE card live in the SAME 3-dot row; "
                       "LAST dot = HUGE (content EMPTY); extra cards + GRP_U deleted\n");
            fprintf(f, "single_hgroup: count=%d order=V/H ok=%d "
                       "is_hgroup H=%d V=%d\n",
                    cards_hgroup_count(), okOrder,
                    cards_is_hgroup(GRP_H), cards_is_hgroup(GRP_V));
            fprintf(f, "single_hgroup_table: cards=%d first=%d last=%d default=%d "
                       "big_cards=%d huge_cards=%d empty_cards=%d\n",
                    n, first, last, d, nBig, nHuge, nEmpty);
            fprintf(f, "single_hgroup_dots=%d (expect 3: 横1 perf / 横2 big1 / 横3 huge1) "
                       "last_is_huge=%d last_is_last_dot=%d\n",
                    nH, okLastIsHuge, okLastIsEnd);
            fprintf(f, "single_hgroup_one_big=%d one_huge=%d big_before_huge=%d "
                       "v4_is_h2=%d default_is_h2=%d\n",
                    okOneBig, okOneHuge, okBigBefore, okV4IsH2, okDefault);
            fprintf(f, "single_hgroup_hmove default->first=%d default->last=%d "
                       "last_clamped=%d ok=%d\n",
                    cards_hmove(d, -1), cards_hmove(d, +1),
                    cards_hmove(last, +1), okHmove);
            fprintf(f, "single_hgroup_linear 0->%d->%d->%d->%d->%d "
                       "(end clamped) ok=%d\n",
                    cards_next(0,+1), cards_next(1,+1), cards_next(2,+1),
                    cards_next(3,+1), cards_next(4,+1), okLinear);
            fprintf(f, "single_hgroup_verdict=%d\n",
                    okOneGroup && okOrder && okThreeDots && okOneBig &&
                    okOneHuge && okBigBefore && okLastIsHuge && okLastIsEnd &&
                    okV4IsH2 && okDefault && okHmove && okLinear);
        }
    }

    /* 布局与滚轮热区 */
    {
        double w, h, r;
        anim_view_size(&a->anim, 0, &w, &h, &r);
        a->anim.curW = w; a->anim.curH = h; a->anim.curR = r;
        a->anim.expanded = 1; a->anim.exp = 1.0; a->anim.expV = 0.0;
        ui_render(a);
        /* 左竖条像素审计：grip 六点 / 连接线必须消失，四个指示点仍在 */
        {
            const DWORD *bits = (const DWORD *)a->canvas.outBits;
            int cw = a->canvas.W, chh = a->canvas.H;
            RECT wr = a->ui.wheelRect;
            int cx = (wr.left + wr.right) / 2;
            double wh = (double)(wr.bottom - wr.top);
            int gripH = (int)(wh * 0.1625 + 0.5);  /* 原 grip 盒 = 卡片 H*0.13 */
            int gripLit = 0, lineLit = 0, dotLit = 0;
            double dotY[CARDS_VDOTS];
            for (i = 0; i < CARDS_VDOTS; i++)
                dotY[i] = wr.top + wh *
                          (0.30 + 0.55 * (double)i / (double)(CARDS_VDOTS - 1));
            for (int y = wr.top; y < wr.bottom && y < chh; y++) {
                for (int x = wr.left; x < wr.right && x < cw; x++) {
                    DWORD v = bits[y * cw + x];
                    int mx, nearDot = 0;
                    if (((v >> 24) & 0xFF) < 200) continue;  /* 只看不透明区 */
                    mx = (int)((v >> 16) & 0xFF);
                    if ((int)((v >> 8) & 0xFF) > mx) mx = (int)((v >> 8) & 0xFF);
                    if ((int)(v & 0xFF) > mx) mx = (int)(v & 0xFF);
                    if (mx <= 50) continue;   /* 卡片本体 14；旧竖线 54 会被抓到 */
                    if (y < wr.top + gripH) gripLit++;
                    for (i = 0; i < CARDS_VDOTS; i++)
                        if (fabs((double)y - dotY[i]) <= 7.0) nearDot = 1;
                    if (nearDot && abs(x - cx) <= 7) dotLit++;
                    else if (abs(x - cx) <= 3) lineLit++;
                }
            }
            fprintf(f, "wheel_grip_lit=%d (expect 0) wheel_line_lit=%d (expect 0) "
                       "wheel_dots_lit=%d (expect >0)\n",
                    gripLit, lineLit, dotLit);
        }
        fprintf(f, "hit_rects=%d wheel_at_left=%s wheel_rect=(%d,%d)-(%d,%d) card_left=%d\n",
                a->ui.hitN,
                (a->ui.wheelRect.left < a->ui.hits[1].rc.left) ? "YES" : "NO",
                (int)a->ui.wheelRect.left, (int)a->ui.wheelRect.top,
                (int)a->ui.wheelRect.right, (int)a->ui.wheelRect.bottom,
                (int)(((double)a->canvas.W - w * a->u) * 0.5));
        for (i = 0; i < a->ui.hitN; i++)
            fprintf(f, "  hit id=%d rc=(%d,%d)-(%d,%d)\n", a->ui.hits[i].id,
                    (int)a->ui.hits[i].rc.left, (int)a->ui.hits[i].rc.top,
                    (int)a->ui.hits[i].rc.right, (int)a->ui.hits[i].rc.bottom);
        /* 3.html 的 2x2：左上隐藏 / 右上亮度 / 左下音量 / 右下 taskmgr */
        {
            RECT h = ui_hit_rect(a, H_HIDE);
            RECT br = ui_hit_rect(a, H_BRI);
            RECT v = ui_hit_rect(a, H_VOL);
            RECT t = ui_hit_rect(a, H_TASKMGR);
            fprintf(f, "grid2x2 hide_TL=%d bri_TR=%d vol_BL=%d taskmgr_BR=%d\n",
                    (h.left < br.left && h.top < v.top),
                    (br.left > h.left && br.top < t.top),
                    (v.left < t.left && v.top > h.top),
                    (t.left > v.left && t.top > br.top));
            fprintf(f, "taskmgr_in_grid=%s (was header area before)\n",
                    (t.left < a->winW / 2) ? "YES" : "NO");
        }
        /* 切到双滑块模式，验证命中区与位置 */
        {
            double w2, h2, r2;
            RECT sv, sb;
            a->ui.mode = 1;
            anim_view_size(&a->anim, 0, &w2, &h2, &r2);
            ui_layout(a, w2 * a->u, h2 * a->u);
            sv = ui_hit_rect(a, H_SLIDER_VOL);
            sb = ui_hit_rect(a, H_SLIDER_BRI);
            fprintf(f, "mode1 hit_rects=%d vol_slider_valid=%d bri_slider_valid=%d\n",
                    a->ui.hitN, sv.right > sv.left, sb.right > sb.left);
            fprintf(f, "sliders_in_timer_area=%s vol_above_bri=%d\n",
                    (sv.left > ui_hit_rect(a, H_TASKMGR).right) ? "YES" : "NO",
                    sv.bottom <= sb.top);
            a->ui.mode = 0;
            ui_layout(a, w2 * a->u, h2 * a->u);
        }
        /* HH:MM:SS 三段命中与两处冒号间隙 */
        {
            RECT h = ui_hit_rect(a, H_NUM_H);
            RECT m = ui_hit_rect(a, H_NUM_M);
            RECT s = ui_hit_rect(a, H_NUM_S);
            fprintf(f, "num_hit_rects=3 valid=%d order=%d\n",
                    (h.right > h.left && m.right > m.left && s.right > s.left),
                    (h.right <= m.left && m.right <= s.left));
            fprintf(f, "colon_gap_px=%ld/%ld (both > 8 so the two dots fit)\n",
                    (long)(m.left - h.right), (long)(s.left - m.right));
        }
    }

    /* 第二个中卡片：便签 */
    {
        double w, h, r;
        RECT nr;
        NoteBox nb;
        static const WCHAR longT[] =
            L"明天下午三点开会，记得带份材料和充电线。\n"
            L"顺便把上周的草稿整理一下，中文换行要自然，不能画出卡片外面去。\n"
            L"第三行再补一句：滚动条不要系统那种方块和箭头，越淡越好。\n"
            L"第四行：内容继续往下写，验证溢出后的滚动指示会出现。\n"
            L"第五行：最后一行，用来确认到底能不能滚到底。";
        int bar0, bar1, spill;

        a->ui.view = NOTE_VIEW; a->ui.mode = 0; a->ui.hoverSeg = -1;
        note_clear(&a->note);
        anim_view_size(&a->anim, NOTE_VIEW, &w, &h, &r);
        a->anim.curW = w; a->anim.curH = h; a->anim.curR = r;
        a->anim.expanded = 1; a->anim.exp = 1.0; a->anim.expV = 0.0;
        ui_render(a);                                  /* 空态 */
        nr = ui_hit_rect(a, H_NOTE);
        ui_note_box(a, &nb);
        {
            RECT e = {0, 0, 0, 0};
            int h = (ui_hit_rect(a, H_HIDE).right > e.left);
            int b = (ui_hit_rect(a, H_BRI).right > e.left);
            int v = (ui_hit_rect(a, H_VOL).right > e.left);
            int t = (ui_hit_rect(a, H_TASKMGR).right > e.left);
            fprintf(f, "note_no_first_page_buttons hide=%d bri=%d vol=%d taskmgr=%d "
                       "hitN=%d (expect 0/0/0/0, hitN=2: wheel+note)\n",
                    h, b, v, t, a->ui.hitN);
        }

        /* 右缘滚动指示区域的亮像素 */
        bar0 = note_bar_lit(a, nb);
        fprintf(f, "note_view=%d hit_valid=%d box=(%.0f,%.0f %.0fx%.0f)\n",
                NOTE_VIEW, nr.right > nr.left, nb.x, nb.y, nb.w, nb.h);
        fprintf(f, "note_empty lines=%d vis=%d overflow=%d bar_lit=%d (expect 0)\n",
                note_line_count(&a->note), note_vis_lines(&a->note),
                note_overflow(&a->note), bar0);
        fprintf(f, "note_scrollbar: no EDIT/RichEdit control, no WS_VSCROLL, "
                   "self_drawn_2px_round_thumb only\n");

        note_insert(&a->note, longT, (int)wcslen(longT));
        a->dirty = 1;
        ui_render(a);                                  /* 长文本溢出态 */
        bar1 = note_bar_lit(a, nb);
        fprintf(f, "note_long len=%d lines=%d vis=%d overflow=%d bar_lit=%d (expect >0)\n",
                a->note.len, note_line_count(&a->note),
                note_vis_lines(&a->note), note_overflow(&a->note), bar1);
        /* 文本内容区右侧 / 下方不得有文字溢出（clip 生效） */
        spill = 0;
        {
            const DWORD *bits = (const DWORD *)a->canvas.outBits;
            int cw = a->canvas.W, ch = a->canvas.H;
            int xx, yy;
            for (yy = (int)nb.y; yy < (int)(nb.y + nb.h) && yy < ch; yy++)
                for (xx = (int)(nb.x + nb.w + 2); xx < cw; xx++) {
                    DWORD v = bits[yy * cw + xx];
                    if (((v >> 24) & 255) > 200 && (int)(v & 255) > 70) spill++;
                }
            for (yy = (int)(nb.y + nb.h + 2); yy < ch; yy++)
                for (xx = (int)nb.x; xx < cw; xx++) {
                    DWORD v = bits[yy * cw + xx];
                    if (((v >> 24) & 255) > 200 && (int)(v & 255) > 70) spill++;
                }
        }
        fprintf(f, "note_clip_spill_px=%d (expect 0, text clipped to note box)\n", spill);
        note_scroll(&a->note, 2);
        fprintf(f, "note_scroll topLine=%d (expect %d = lines-vis, clamped to max)\n",
                a->note.topLine, note_line_count(&a->note) - note_vis_lines(&a->note));

        /* 选中指示：stb_textedit 状态 + 真的画进 alpha 画布的高亮像素 */
        {
            int sel0, sel1;
            a->note.editing = 1;
            a->note.stb.select_start = a->note.stb.select_end = 0;
            ui_render(a);
            sel0 = note_sel_lit(a, nb);
            note_select_all(&a->note);
            ui_render(a);
            sel1 = note_sel_lit(a, nb);
            {
                int lo, hi;
                note_selection_range(&a->note, &lo, &hi);
                fprintf(f, "note_selection sel=(%d,%d) len=%d has=%d "
                           "drag/shift/ctrl+A/ctrl+C-X-V/undo provided by stb_textedit v1.16\n",
                        lo, hi, a->note.len, note_has_selection(&a->note));
            }
            fprintf(f, "note_sel_lit none=%d all=%d (expect 0 / >0; "
                       "self-drawn dark blue-grey highlight, no child EDIT control)\n",
                    sel0, sel1);
            /* 高亮不得溢出到文本区右侧（clip 生效） */
            {
                const DWORD *bits = (const DWORD *)a->canvas.outBits;
                int cw = a->canvas.W, ch = a->canvas.H, xx, yy, out = 0;
                for (yy = (int)nb.y; yy < (int)(nb.y + nb.h) && yy < ch; yy++)
                    for (xx = (int)(nb.x + nb.w + 2); xx < cw; xx++) {
                        DWORD v = bits[yy * cw + xx];
                        int b = (int)(v & 255), g = (int)((v >> 8) & 255),
                            r = (int)((v >> 16) & 255);
                        if (((v >> 24) & 255) > 200 && b > r + 30 && b > g + 22) out++;
                    }
                fprintf(f, "note_sel_outside_px=%d (expect 0)\n", out);
            }
            a->note.stb.select_start = a->note.stb.select_end = a->note.stb.cursor;
        }

        /* 恢复 */
        note_clear(&a->note);
        a->note.editing = 0;
        a->ui.view = 0;
        ui_render(a);
    }

    /* 鼠标形状：便签文本区必须是工字形 IDC_IBEAM，其余一律箭头。
       除了判定函数，还走真实 WM_SETCURSOR → SetCursor → GetCursor 链路，
       证明系统光标真的被换了，而不是只改了个布尔量。 */
    {
        double w, h, r;
        NoteBox nb;
        POINT p;
        HCURSOR hI = LoadCursorW(NULL, IDC_IBEAM);
        HCURSOR hA = LoadCursorW(NULL, IDC_ARROW);
        HCURSOR gotI, gotA;
        int center, strip, timerPage, outside;
        a->visible = 1;
        a->ui.view = NOTE_VIEW; a->ui.mode = 0; a->ui.hoverSeg = -1;
        anim_view_size(&a->anim, NOTE_VIEW, &w, &h, &r);
        a->anim.curW = w; a->anim.curH = h; a->anim.curR = r;
        a->anim.expanded = 1; a->anim.exp = 1.0; a->anim.expV = 0.0;
        ui_render(a);
        ui_note_box(a, &nb);

        center = window_text_cursor(a, (int)(nb.x + nb.w * 0.5),
                                    (int)(nb.y + nb.h * 0.5));
        strip  = window_text_cursor(a, (int)(nb.x * 0.5),
                                    (int)(nb.y + nb.h * 0.5));
        outside = window_text_cursor(a, -20, -20);
        a->ui.view = 0;
        timerPage = window_text_cursor(a, (int)(nb.x + nb.w * 0.5),
                                       (int)(nb.y + nb.h * 0.5));
        a->ui.view = NOTE_VIEW;

        p.x = (int)(nb.x + nb.w * 0.5);
        p.y = (int)(nb.y + nb.h * 0.5);
        ClientToScreen(a->hwnd, &p);
        SetCursorPos(p.x, p.y);
        SendMessageW(a->hwnd, WM_SETCURSOR, (WPARAM)a->hwnd,
                     MAKELPARAM(HTCLIENT, WM_MOUSEMOVE));
        gotI = GetCursor();

        p.x = (int)(nb.x * 0.5);
        p.y = (int)(nb.y + nb.h * 0.5);
        ClientToScreen(a->hwnd, &p);
        SetCursorPos(p.x, p.y);
        SendMessageW(a->hwnd, WM_SETCURSOR, (WPARAM)a->hwnd,
                     MAKELPARAM(HTCLIENT, WM_MOUSEMOVE));
        gotA = GetCursor();

        fprintf(f, "cursor_shape: IDC_IBEAM over note text, IDC_ARROW elsewhere "
                   "(WM_SETCURSOR + SetCursor + return TRUE)\n");
        fprintf(f, "cursor_note_center=%d wheel_strip=%d outside=%d timer_page=%d "
                   "(expect 1/0/0/0)\n", center, strip, outside, timerPage);
        fprintf(f, "cursor_setcursor_ibeam=%d arrow=%d handles_valid=%d "
                   "(expect 1/1/1)\n", gotI == hI, gotA == hA, !!hI && !!hA);
        fprintf(f, "cursor_no_redraw: WM_SETCURSOR only calls SetCursor, never "
                   "sets dirty / never calls UpdateLayeredWindow\n");

        a->ui.view = 0;
        SetCursorPos(a->scrW / 2, a->scrH / 2);
        ui_render(a);
    }

    /* 第三个中卡片：性能监测（圆环 / 数字 / 硬件信息） */
    {
        double w, h, r, W, H, L;
        double sCpu = a->perf.cpu, sMem = a->perf.mem, sDisk = a->perf.disk;
        int ringLit = 0, textLit = 0, mid = 0, lv[256], cw, ch, xx, yy;
        int sampleR[3] = {0}, sampleG[3] = {0}, sampleB[3] = {0}, sampleN[3] = {0};
        double radD, x0D, cyD, gapD;
        int colorDistOK, satOK, warnOK, baseOK;
        const DWORD *bits;
        char hw[1024];
        ZeroMemory(lv, sizeof(lv));

        a->ui.view = PERF_VIEW; a->ui.mode = 0; a->ui.hoverSeg = -1;
        anim_view_size(&a->anim, PERF_VIEW, &w, &h, &r);
        a->anim.curW = w; a->anim.curH = h; a->anim.curR = r;
        a->anim.expanded = 1; a->anim.exp = 1.0; a->anim.expV = 0.0;
        /* 固定演示值，让圆环/数字/文字可复现；真实值在下面单独输出 */
        a->perf.cpu = 42.0; a->perf.mem = 68.0; a->perf.disk = 83.0;
        ui_render(a);

        W = w * a->u; H = h * a->u;
        L = ((double)a->canvas.W - W) * 0.5;
        bits = (const DWORD *)a->canvas.outBits;
        cw = a->canvas.W; ch = a->canvas.H;
        /* 环区：统计「彩色」像素（饱和度 > 30），中性底环被排除。
           灰阶级数按亮度统计，验证彩色化之后抗锯齿仍是连续过渡。 */
        for (yy = (int)(H * 0.12); yy < (int)(H * 0.78) && yy < ch; yy++)
            for (xx = (int)(L + W * 0.09); xx < (int)(L + W * 0.60) && xx < cw; xx++) {
                DWORD v = bits[yy * cw + xx];
                int B = (int)(v & 255), G = (int)((v >> 8) & 255);
                int R = (int)((v >> 16) & 255);
                int mx = R > G ? R : G; if (B > mx) mx = B;
                int mn = R < G ? R : G; if (B < mn) mn = B;
                if (((v >> 24) & 255) > 200 && mx - mn > 30) {
                    ringLit++;
                    lv[qs_iclamp((R + G + B) / 3, 0, 255)]++;
                }
            }
        for (xx = 1; xx < 255; xx++) if (lv[xx] > 0) mid++;

        /* 三个环顶部弧起点的实际像素平均色（5x5 内取彩色像素）：
           用来断言三色确实可辨识，而不是「都换成彩色但看不出区别」。 */
        radD = qs_clamp(H * 0.245, 20.0, 46.0);
        gapD = W * 0.026;
        x0D = L + W * 0.112;
        cyD = H * 0.445;
        for (xx = 0; xx < 3; xx++) {
            double scx = x0D + radD + (double)xx * (2.0 * radD + gapD);
            int py = (int)(cyD - radD + 0.5), pxi;
            for (yy = py - 2; yy <= py + 2; yy++)
                for (pxi = (int)scx - 2; pxi <= (int)scx + 2; pxi++) {
                    DWORD v;
                    int B, G, R, mx, mn;
                    if (yy < 0 || yy >= ch || pxi < 0 || pxi >= cw) continue;
                    v = bits[yy * cw + pxi];
                    B = (int)(v & 255); G = (int)((v >> 8) & 255);
                    R = (int)((v >> 16) & 255);
                    mx = R > G ? R : G; if (B > mx) mx = B;
                    mn = R < G ? R : G; if (B < mn) mn = B;
                    if (((v >> 24) & 255) > 200 && mx - mn > 30) {
                        sampleR[xx] += R; sampleG[xx] += G; sampleB[xx] += B;
                        sampleN[xx]++;
                    }
                }
            if (sampleN[xx] > 0) {
                sampleR[xx] /= sampleN[xx]; sampleG[xx] /= sampleN[xx];
                sampleB[xx] /= sampleN[xx];
            }
        }
        {
            double d01 = sqrt((double)(sampleR[0]-sampleR[1])*(sampleR[0]-sampleR[1]) +
                              (double)(sampleG[0]-sampleG[1])*(sampleG[0]-sampleG[1]) +
                              (double)(sampleB[0]-sampleB[1])*(sampleB[0]-sampleB[1]));
            double d12 = sqrt((double)(sampleR[1]-sampleR[2])*(sampleR[1]-sampleR[2]) +
                              (double)(sampleG[1]-sampleG[2])*(sampleG[1]-sampleG[2]) +
                              (double)(sampleB[1]-sampleB[2])*(sampleB[1]-sampleB[2]));
            double d02 = sqrt((double)(sampleR[0]-sampleR[2])*(sampleR[0]-sampleR[2]) +
                              (double)(sampleG[0]-sampleG[2])*(sampleG[0]-sampleG[2]) +
                              (double)(sampleB[0]-sampleB[2])*(sampleB[0]-sampleB[2]));
            colorDistOK = (sampleN[0] > 0 && sampleN[1] > 0 && sampleN[2] > 0 &&
                           d01 > 40.0 && d12 > 40.0 && d02 > 40.0);
            satOK = 1;
            for (xx = 0; xx < 3; xx++) {
                int R = sampleR[xx], G = sampleG[xx], B = sampleB[xx];
                int mx = R > G ? R : G; if (B > mx) mx = B;
                int mn = R < G ? R : G; if (B < mn) mn = B;
                if (mx - mn <= 30) satOK = 0;
            }
            /* 纯函数断言：低负载=基色、100%=警示色 */
            {
                int r0, g0, b0, rw, gw, bw;
                perf_ring_rgb(0, 0.0, &r0, &g0, &b0);
                perf_ring_rgb(0, 100.0, &rw, &gw, &bw);
                baseOK = (r0 == PERF_CPU_R && g0 == PERF_CPU_G && b0 == PERF_CPU_B);
                warnOK = (rw == PERF_WARN_R && gw == PERF_WARN_G && bw == PERF_WARN_B);
            }
            fprintf(f, "perf_ring_colors CPU=(%d,%d,%d) MEM=(%d,%d,%d) "
                       "DISK=(%d,%d,%d) samples=%d/%d/%d\n",
                    sampleR[0], sampleG[0], sampleB[0],
                    sampleR[1], sampleG[1], sampleB[1],
                    sampleR[2], sampleG[2], sampleB[2],
                    sampleN[0], sampleN[1], sampleN[2]);
            fprintf(f, "perf_ring_color_dist d01=%.0f d12=%.0f d02=%.0f "
                       "distinguishable=%d saturated=%d (expect >40 / 1 / 1)\n",
                    d01, d12, d02, colorDistOK, satOK);
            fprintf(f, "perf_ring_gradient base_at_0=%d warn_at_100=%d "
                       "arc_shade=%.2f (expect 1/1; >85%% smoothstep to warn)\n",
                    baseOK, warnOK, PERF_ARC_SHADE);
        }
        /* 右区硬件信息文字：DIM / ACCENT 灰度像素 */
        for (yy = (int)(H * 0.05); yy < (int)(H * 0.98) && yy < ch; yy++)
            for (xx = (int)(L + W * 0.60); xx < (int)(L + W * 0.99) && xx < cw; xx++) {
                DWORD v = bits[yy * cw + xx];
                int B = (int)(v & 255), R = (int)((v >> 16) & 255);
                if (((v >> 24) & 255) > 200 && R >= 70 && R <= 230 && B - R < 30)
                    textLit++;
            }

        perf_hw_summary(&a->perf, hw, (int)sizeof(hw));
        fprintf(f, "perf_view=%d card=%.0fx%.0f text_policy=RELAXED (only this view; "
                   "others still keep the single Chinese label)\n",
                PERF_VIEW, MID_W, MID_H);
        fprintf(f, "perf_rings=3 labels=CPU/内存/磁盘 demo_values=42/68/83 "
                   "ring_center_percent + hardware_lines=%d\n", PERF_HW_LINES);
        fprintf(f, "perf_palette=COLOR (CPU blue / MEM violet / DISK teal, "
                   "warm-orange warning >=85%%); other views stay neutral monochrome\n");
        fprintf(f, "perf_ring_lit_px=%d antialias_levels=%d (expect >0 / >3; "
                   "color is only the blend source, 2x2 area ratio unchanged = no jaggies)\n",
                ringLit, mid);
        fprintf(f, "perf_hw_text_lit_px=%d (expect >0, width-fitted with ellipsis)\n",
                textLit);
        fprintf(f, "perf_hw: %s\n", hw);
        fprintf(f, "perf_api: GetSystemTimes(diff) / GlobalMemoryStatusEx / "
                   "GetDiskFreeSpaceExW / GetSystemInfo / EnumDisplayDevicesW / "
                   "RegQueryValueExW(ProcessorNameString) / GetVersionExW\n");
        fprintf(f, "perf_no_pdh: counter names are localized & need pdh.lib + 2 samples; "
                   "GetSystemTimes is the lighter official path\n");
        fprintf(f, "perf_idle: samples only when PERF_VIEW is expanded; "
                   "collapsed / other views keep zero-redraw\n");

        a->perf.cpu = sCpu; a->perf.mem = sMem; a->perf.disk = sDisk;
        fprintf(f, "perf_live cpu=%.1f mem=%.1f disk=%.1f samples=%d\n",
                a->perf.cpu, a->perf.mem, a->perf.disk, a->perf.sampleCount);
        fprintf(f, "perf_hitN_on_page=%d (expect 1: wheel strip only, "
                   "no clickable controls on a read-only page)\n", a->ui.hitN);
        a->ui.view = 0;
        ui_render(a);
    }

    /* 悬停 */
    {
        int cx = a->winW / 2;
        int iy = (int)(ISLAND_H * 0.5 * a->u);
        int cy = (int)(BIG_H * 0.5 * a->u);
        a->anim.expanded = 0;
        fprintf(f, "hover island_center collapsed=%d card_mid collapsed=%d\n",
                anim_inside_target(&a->anim, 0, a->u, a->winW, HOVER_MARGIN, cx, iy),
                anim_inside_target(&a->anim, 0, a->u, a->winW, HOVER_MARGIN, cx, cy));
        a->anim.expanded = 1;
        fprintf(f, "hover island_center huge=%d card_mid huge=%d outside=%d\n",
                anim_inside_target(&a->anim, 4, a->u, a->winW, HOVER_MARGIN, cx, iy),
                anim_inside_target(&a->anim, 4, a->u, a->winW, HOVER_MARGIN, cx, cy),
                anim_inside_target(&a->anim, 4, a->u, a->winW, HOVER_MARGIN, -30, cy));
    }

    /* 档位夹取 / 按钮链路 / 倒计时 */
    ui_set_view(a, -9); fprintf(f, "view clamp low = %d\n", a->ui.view);
    ui_set_view(a, 99); fprintf(f, "view clamp high = %d\n", a->ui.view);
    ui_set_view(a, 0);
    countdown_reset(&a->cd);
    fprintf(f, "countdown default=%d remain=%.1f\n",
            CD_DEFAULT_SEC, countdown_remain(&a->cd));
    a->cd.start = qs_now() - 65000; a->cd.base = CD_DEFAULT_SEC; a->cd.running = 1;
    fprintf(f, "countdown after_65s remain=%.1f\n", countdown_remain(&a->cd));
    countdown_reset(&a->cd);

    /* 悬浮段滚轮调时：时 ±1h / 分 ±1m / 秒 ±5s，且保持 running */
    {
        Countdown t;
        countdown_init(&t);
        countdown_adjust(&t, 0, 1);
        fprintf(f, "adjust H +1 -> %.0f (expect %.0f)\n",
                countdown_remain(&t), (double)(CD_DEFAULT_SEC + CD_STEP_H));
        countdown_adjust(&t, 1, 1);
        fprintf(f, "adjust M +1 -> %.0f (expect %.0f)\n",
                countdown_remain(&t), (double)(CD_DEFAULT_SEC + CD_STEP_H + CD_STEP_M));
        countdown_adjust(&t, 2, -1);
        fprintf(f, "adjust S -1 -> %.0f (expect %.0f)\n",
                countdown_remain(&t),
                (double)(CD_DEFAULT_SEC + CD_STEP_H + CD_STEP_M - CD_STEP_S));
        t.running = 1; t.start = qs_now(); t.base = countdown_remain(&t);
        countdown_adjust(&t, 1, -1);
        fprintf(f, "adjust while running keeps_running=%d\n", t.running);
        t.running = 1; t.start = qs_now() - 400000; t.base = 300;
        countdown_stop_if_zero(&t);
        fprintf(f, "natural_zero ended=%d remain=%.1f\n",
                t.ended, countdown_remain(&t));
        countdown_clear_ended(&t);
        fprintf(f, "clear_ended -> %d\n", t.ended);
    }

    fprintf(f, "taskmgr_hit_valid=%d\n",
            (ui_hit_rect(a, H_TASKMGR).right > ui_hit_rect(a, H_TASKMGR).left));

    /* 亮度 / 音量按钮 → 计时器变双滑块，再点收回 */
    fprintf(f, "mode initial=%d\n", a->ui.mode);
    ui_click_id(a, H_BRI);
    fprintf(f, "click BRI -> mode=%d vol_slider=%d bri_slider=%d\n",
            a->ui.mode,
            ui_hit_rect(a, H_SLIDER_VOL).right > ui_hit_rect(a, H_SLIDER_VOL).left,
            ui_hit_rect(a, H_SLIDER_BRI).right > ui_hit_rect(a, H_SLIDER_BRI).left);
    {
        RECT r = ui_hit_rect(a, H_SLIDER_BRI);
        if (r.right > r.left) {
            ui_click(a, r.left + 2, (r.top + r.bottom) / 2);
            fprintf(f, "bri slider left -> %.1f\n", a->sys.bri);
            ui_click(a, r.right - 2, (r.top + r.bottom) / 2);
            fprintf(f, "bri slider right -> %.1f\n", a->sys.bri);
        }
    }
    ui_click_id(a, H_VOL);
    fprintf(f, "click VOL (toggle back) -> mode=%d\n", a->ui.mode);
    ui_click_id(a, H_VOL);
    fprintf(f, "click VOL again -> mode=%d\n", a->ui.mode);
    {
        RECT r = ui_hit_rect(a, H_SLIDER_VOL);
        if (r.right > r.left) {
            ui_click(a, r.left + 2, (r.top + r.bottom) / 2);
            fprintf(f, "vol slider left -> %.3f\n", a->sys.vol);
            ui_click(a, r.right - 2, (r.top + r.bottom) / 2);
            fprintf(f, "vol slider right -> %.3f\n", a->sys.vol);
        }
    }
    ui_click_id(a, H_BRI);
    fprintf(f, "click BRI (back to timer) -> mode=%d\n", a->ui.mode);
    a->drag = 0;
    ui_click_id(a, H_PLAY);
    fprintf(f, "click play -> running=%d\n", a->cd.running);
    ui_click_id(a, H_PLAY);
    fprintf(f, "click pause -> running=%d\n", a->cd.running);
    ui_click_id(a, H_RESET);
    fprintf(f, "click reset -> remain=%.1f\n", countdown_remain(&a->cd));
    fprintf(f, "set_volume(0.5)=%d set_brightness(60)=%d\n",
            sys_set_volume(&a->sys, 0.5),
            sys_set_brightness(&a->sys, 60, a->hwnd));

    /* 弹簧 */
    for (int phase = 0; phase < 2; phase++) {
        double x = phase ? 1.0 : 0.0, v = 0.0;
        double target = phase ? 0.0 : 1.0;
        double damp = phase ? EXP_DAMP_IN : EXP_DAMP_OUT;
        double hh = (1.0 / 240.0) / 4.0;
        double peak = x, low = x;
        int settle = -1;
        for (int s = 0; s < 480; s++) {
            for (int j = 0; j < 4; j++) {
                v += (EXP_K * (target - x) - damp * v) * hh;
                x += v * hh;
            }
            if (x > peak) peak = x;
            if (x < low) low = x;
            if (settle < 0 && fabs(x - target) < 0.01 && fabs(v) < 0.05)
                settle = (int)(s * 1000.0 / 240.0);
        }
        fprintf(f, "spring %s peak=%.4f low=%.4f settle_ms=%d\n",
                phase ? "collapse" : "expand", peak, low, settle);
    }

    /* ---- 横排圆点：3 点，横2 大卡片 / 横3 超大卡片 = 最后一点 ---- */
    {
        int v0 = a->ui.view;
        int nH = cards_group_count(GRP_H);
        int dotBig[16], dotHuge[16];
        int litBig, litTimer, litHuge, curBig = 0, curHuge = 0, i;
        int vTimer, vBig, vHuge;
        int wBigDown = -1, wBigUp = -1, wHugeDown = -1, wHugeUp = -1;
        int jump[3] = { -1, -1, -1 };
        RECT hr, wr;

        ZeroMemory(dotBig, sizeof(dotBig)); ZeroMemory(dotHuge, sizeof(dotHuge));

        /* view3 = big1（横2 / 竖④）：横排 3 点，第 2 个最大最亮 */
        a->ui.view = 3; a->ui.mode = 0; ui_render(a);
        hr = a->ui.hdotsRect;
        wr = a->ui.wheelRect;
        litBig = hdots_lit_px(a, hr, nH, dotBig);
        vBig = vstrip_lit_px(a, wr);          /* 应为 0：竖条不显示 */
        for (i = 0; i < nH && i < 16; i++)
            if (dotBig[i] > dotBig[curBig]) curBig = i;

        /* view0 = 计时器（竖①）：竖列点可见，同一区域没有横排点 */
        a->ui.view = 0; ui_render(a);
        litTimer = hdots_lit_px(a, hr, nH, NULL);
        vTimer = vstrip_lit_px(a, a->ui.wheelRect);

        /* view4 = huge1（横3 / 最后一点）：横排 3 点，第 3 个最亮 */
        a->ui.view = 4; ui_render(a);
        litHuge = hdots_lit_px(a, a->ui.hdotsRect, nH, dotHuge);
        vHuge = vstrip_lit_px(a, a->ui.wheelRect);
        for (i = 0; i < nH && i < 16; i++)
            if (dotHuge[i] > dotHuge[curHuge]) curHuge = i;

        /* 把光标移到当前卡片的横排点上，再发滚轮；四种组合全测 */
        {
            int start[4] = { 3, 3, 4, 4 };
            int delta[4] = { -120, 120, -120, 120 };
            int *out[4] = { &wBigDown, &wBigUp, &wHugeDown, &wHugeUp };
            int k;
            for (k = 0; k < 4; k++) {
                POINT p;
                a->ui.view = start[k]; ui_render(a);
                p.x = (a->ui.hdotsRect.left + a->ui.hdotsRect.right) / 2;
                p.y = (a->ui.hdotsRect.top + a->ui.hdotsRect.bottom) / 2;
                ClientToScreen(a->hwnd, &p);
                SetCursorPos(p.x, p.y);
                a->lastWheel = 0;             /* 绕过防抖，测的是路由本身 */
                ui_wheel(a, delta[k]);
                *out[k] = a->ui.view;
            }
        }

        /* 点 3 个圆点：slot0 性能 / slot1 大卡片 / slot2 超大卡片。
           坐标与 draw_hdots() 同源：命中区中心 ± pitch。 */
        for (i = 0; i < 3; i++) {
            RECT r;
            double mid, pitch, x, y;
            a->ui.view = 3; ui_render(a);
            r = a->ui.hdotsRect;
            mid = ((double)r.left + (double)r.right) * 0.5;
            pitch = ui_dots_pitch((double)a->anim.curH * a->u, a->u);
            x = mid + ((double)i - (double)(nH - 1) * 0.5) * pitch;
            y = (r.top + r.bottom) / 2;
            ui_click(a, (int)(x + 0.5), (int)y);
            jump[i] = a->ui.view;
        }

        fprintf(f, "hdots_policy: horizontal DOT ROW replaces vertical DOT COLUMN when vdot==%d; "
                   "vdot 0..2 keep the column\n", CARDS_VDOTS - 1);
        fprintf(f, "hdots_style: same canvas_disc / radii %.1f:%.1f / ACCENT:DIM; "
                   "CENTERED + SYMMETRIC, dense pitch=%.1f px (BIG-card reference pitch=%.1f px, cap=%.1f); "
                   "NO track / bar / segment / label / arrow\n",
                HDOTS_R_ON, HDOTS_R_OFF,
                ui_dots_pitch((double)a->anim.curH * a->u, a->u),
                (double)BIG_H * a->u * 0.80 * (HDOTS_P1 - HDOTS_P0) /
                    (double)(CARDS_VDOTS - 1),
                HDOTS_PITCH_MAX * a->u);
        {
            double p = ui_dots_pitch((double)a->anim.curH * a->u, a->u);
            double left, right, c0, c1, cy;
            ui_hdots_center(0.0, (double)a->anim.curW * a->u,
                            (double)a->anim.curH * a->u, p, nH, 0, &left, &cy);
            ui_hdots_center(0.0, (double)a->anim.curW * a->u,
                            (double)a->anim.curH * a->u, p, nH, nH - 1, &right, &cy);
            c0 = (double)a->anim.curW * a->u * 0.5;
            c1 = (left + right) * 0.5;
            fprintf(f, "hdots_centered: midpoint=%.1f card_center=%.1f "
                       "symmetric=%d dense=%d (expect 1 / 1)\n",
                    c1, c0, (int)(fabs(c1 - c0) < 0.01),
                    (int)(p < (double)a->anim.curW * a->u * 0.15));
        }
        fprintf(f, "hdots_rect=(%d,%d)-(%d,%d) dots=%d "
                   "(横1 perf / 横2 big1 / 横3 huge1 = LAST dot)\n",
                (int)hr.left, (int)hr.top, (int)hr.right, (int)hr.bottom, nH);
        fprintf(f, "hdots_lit big1=%d huge1=%d timer=%d (expect >0 / >0 / 0)\n",
                litBig, litHuge, litTimer);
        fprintf(f, "vstrip_lit timer=%d big1=%d huge1=%d "
                   "(expect >0 / 0 / 0 = 竖条消失)\n", vTimer, vBig, vHuge);
        fprintf(f, "hdots_current_dot big1=%d huge1=%d (expect 1 / 2)\n",
                curBig, curHuge);
        fprintf(f, "hdots_per_dot_big1=");
        for (i = 0; i < nH && i < 16; i++) fprintf(f, "%s%d", i ? "/" : "", dotBig[i]);
        fprintf(f, "\nhdots_per_dot_huge1=");
        for (i = 0; i < nH && i < 16; i++) fprintf(f, "%s%d", i ? "/" : "", dotHuge[i]);
        fprintf(f, "\n");
        fprintf(f, "hdots_wheel big1_down=%d (expect 4) big1_up=%d (expect 2) "
                   "huge1_down=%d (expect 4 clamp) huge1_up=%d (expect 3)\n",
                wBigDown, wBigUp, wHugeDown, wHugeUp);
        fprintf(f, "hdots_click_dots=%d/%d/%d expected=%d/%d/%d ok=%d\n",
                jump[0], jump[1], jump[2],
                cards_group_card(GRP_H, 0), cards_group_card(GRP_H, 1),
                cards_group_card(GRP_H, 2),
                jump[0] == cards_group_card(GRP_H, 0) &&
                jump[1] == cards_group_card(GRP_H, 1) &&
                jump[2] == cards_group_card(GRP_H, 2));
        fprintf(f, "hdots_single_group_verdict=%d\n",
                nH == 3 && curBig == 1 && curHuge == 2 &&
                litBig > 0 && litHuge > 0 && litTimer == 0 &&
                vTimer > 0 && vBig == 0 && vHuge == 0 &&
                wBigDown == 4 && wBigUp == 2 &&
                wHugeDown == 4 && wHugeUp == 3 &&
                jump[0] == 2 && jump[1] == 3 && jump[2] == 4);
        fprintf(f, "hdots_nav: ONE horizontal group only, BIG + HUGE share the same "
                   "3-dot row; wheel on dots = group-first hmove, at the end everything "
                   "clamps; left strip keeps accepting wheel (invisible)\n");

        a->ui.view = v0; a->dirty = 1;
    }

    /* ---- 第一个大卡片：剪贴板历史 ---- */
    {
        static const WCHAR *demo[] = {
            L"https://learn.microsoft.com/windows/win32/dataxchg/clipboard",
            L"明天下午三点开会，记得带份材料和充电线",
            L"Qisland clipboard history — 点一条即可直接粘贴",
            L"第一行\n第二行\t压成空格显示，回填时保留原文",
            L"这是一条特别长的内容，用来验证单行截断与省略号不会溢出卡片右边界",
            L"git commit -m \"clipboard history card\"",
            L"192.168.1.1",
            L"最后一条：滚动到底部时右缘短滑块会跟着移动"
        };
        int i, v0 = a->ui.view, litEmpty, litList, spill, thumb;
        ClipBox cb;
        a->clip.testFile = 1;
        clip_clear(&a->clip, NULL);
        snap(a, CLIP_VIEW, 0, NULL);      /* 只渲染，不导出 */
        ui_clip_box(a, &cb);
        litEmpty = clip_lit_px(a, cb);
        for (i = 0; i < 8; i++)
            clip_push_text(&a->clip, demo[i], (int)wcslen(demo[i]));
        clip_scroll(&a->clip, 2);
        snap(a, CLIP_VIEW, 0, NULL);
        ui_clip_box(a, &cb);
        litList = clip_lit_px(a, cb);
        spill = clip_spill_px(a, cb);
        thumb = clip_thumb_lit_px(a, cb);
        fprintf(f, "clip_api: AddClipboardFormatListener + WM_CLIPBOARDUPDATE (Vista+, Win7 OK); "
                   "CF_UNICODETEXT / GlobalAlloc(GMEM_MOVEABLE) / SetClipboardData ownership\n");
        fprintf(f, "clip_policy: text only; newest on top; identical skipped; "
                   "old re-copy moves to top; self-write suppressed; no title/label/placeholder text\n");
        fprintf(f, "clip_listener=%d count=%d captures=%d dup_skipped=%d "
                   "self_skipped=%d (writeback exercised only in --cliptest)\n",
                clip_listener(&a->clip), clip_count(&a->clip),
                clip_captures(&a->clip), clip_dups(&a->clip),
                clip_self_skipped(&a->clip));
        fprintf(f, "clip_layout vis_rows=%d overflow=%d font_px=%d line_h=%d\n",
                clip_vis_rows(&a->clip), clip_overflow(&a->clip),
                a->clip.px, a->clip.lineH);
        fprintf(f, "clip_pixels empty_lit=%d list_lit=%d spill_below=%d thumb_lit=%d "
                   "(expect >0 / >0 / 0 / >0)\n", litEmpty, litList, spill, thumb);
        fprintf(f, "clip_file=%%APPDATA%%\\Qisland\\clipboard.bin "
                   "(diag/demo -> clipboard_test.bin)\n");
        clip_clear(&a->clip, NULL);
        a->ui.view = v0; a->dirty = 1;
    }

    /* ---- 即时通知系统 ---- */
    {
        int iconLit = 0, textLit = 0, dotsLit = 0, v1 = a->ui.view;
        double w, h, r;
        notify_show_fa(&a->notify, a, FA_BELL, L"Qisland",
                       L"倒计时已结束 · 00:00:00", 60000);
        snap(a, 0, 0, NULL);
        anim_view_size(&a->anim, a->ui.view, &w, &h, &r);
        notify_lit_parts(a, &iconLit, &textLit);
        dotsLit = notify_dots_lit_px(a);
        fprintf(f, "notify_api: notification card = MID size; left FA/HICON, "
                   "right appName + detail; NO storage / NO file / NO queue\n");
        fprintf(f, "notify_policy: in-memory single slot, newest replaces current, "
                   "expires after %d ms, click or xmark dismisses\n", NOTIFY_MS);
        fprintf(f, "notify_size=%.1fx%.1f mid=%.1fx%.1f match=%d\n",
                w, h, MID_W, MID_H,
                (int)(fabs(w - MID_W) < 0.01 && fabs(h - MID_H) < 0.01));
        fprintf(f, "notify_pixels icon_lit=%d text_lit=%d dots_lit=%d "
                   "(expect >0 / >0 / 0)\n", iconLit, textLit, dotsLit);
        fprintf(f, "notify_icon_halo_lit_px=%d (expect 0: 图标底下不再画灰白圆底 / 圆形容器)\n",
                notify_halo_lit_px(a));
        fprintf(f, "notify_no_file=%d (expect 1)\n", notify_no_file());
        fprintf(f, "notify_countdown_hook: ui_countdown_ended -> notify_show_fa"
                   "(FA_BELL, Qisland, 倒计时已结束)\n");
        fprintf(f, "notify_gap_px=%.1f min=%.1f ok=%d "
                   "(图标右缘 → 程序名起点，文字不再贴着图标)\n",
                notify_gap_px(a), NOTIFY_GAP_MIN * a->u,
                (int)(notify_gap_px(a) >= NOTIFY_GAP_MIN * a->u));
        notify_dismiss(&a->notify, a);
        /* 通知关闭后再测超大卡留空：否则 notify_draw 会覆盖整个卡片 */
        {
            int cl = 0, dl = 0, hn = 0, ch = 0;
            huge_empty_check(a, &cl, &dl, &hn, &ch);
            fprintf(f, "huge_empty: content_lit=%d hdots_lit=%d hitN=%d center_hit=%d "
                       "(expect 0 / >0 / 2 / 0: 超大卡只留形状与横排点，无任何内容控件)\n",
                    cl, dl, hn, ch);
        }
        a->ui.view = v1; a->dirty = 1;
    }

    /* ---- 旧结束提示删除 + 滑块模式 play/reset 隐藏 ---- */
    {
        int diff = old_end_hint_diff_px(a);
        int hp = 0, hr = 0, rl = 0;
        slider_mode_check(a, &hp, &hr, &rl);
        fprintf(f, "old_end_hint_diff_px=%d (expect 0: cd.ended=1 与 =0 逐像素一致，"
                   "旧 bell 常驻提示已整体删除)\n", diff);
        fprintf(f, "old_end_hint_removed=%d (expect 1)\n", (int)(diff == 0));
        fprintf(f, "slider_mode play_hit=%d reset_hit=%d right_lit=%d "
                   "(expect 0 / 0 / 0: 双滑块模式不画也不响应 play/reset)\n",
                hp, hr, rl);
        fprintf(f, "countdown_notify_only: 归零 -> notify_show_fa(FA_BELL) -> "
                   "点掉/过期 -> ui_clear_end -> 正常计时器，无残留提示态\n");
        a->ui.view = cards_anchor(0); a->ui.mode = 0; a->dirty = 1;
    }

    /* ---- Win7 托盘气泡消息监听 ---- */
    {
        fprintf(f, "balloon_verdict: CAN receive, but only by OBSERVING explorer's balloon "
                   "window; NIF_INFO callbacks are NOT broadcast to 3rd parties\n");
        fprintf(f, "balloon_official: NOTIFYICONDATA NIF_INFO (szInfo/szInfoTitle/dwInfoFlags/"
                   "uTimeout); NIN_BALLOONSHOW / NIN_BALLOONTIMEOUT / NIN_BALLOONUSERCLICK "
                   "go ONLY to the icon owner's hWnd (Shell_NotifyIcon NIM_MODIFY, XP+)\n");
        fprintf(f, "balloon_hook: SetWinEventHook(EVENT_OBJECT_CREATE 0x8000 / "
                   "EVENT_OBJECT_SHOW 0x8002, WINEVENT_OUTOFCONTEXT | "
                   "WINEVENT_SKIPOWNPROCESS); caller thread MUST have a message loop; "
                   "async but sequential; Win2000+\n");
        fprintf(f, "balloon_winevent: WinEvents/Active Accessibility - client must call "
                   "SetWinEventHook to receive events; AccessibleObjectFromEvent can fetch "
                   "IAccessible inside the callback\n");
        fprintf(f, "balloon_filter: hwnd + OBJID_WINDOW/CHILDID_SELF -> class "
                   "tooltips_class32 -> TTS_BALLOON 0x40 (official Tooltip Styles 5.80) "
                   "-> explorer pid; callback touches NO App/Canvas/Ui/Notify, "
                   "only PostMessage(HWND)\n");
        fprintf(f, "balloon_text: WM_GETTEXTLENGTH + WM_GETTEXT via "
                   "SendMessageTimeout(SMTO_ABORTIFHUNG, %dms), GetWindowText fallback; "
                   "UIA_ToolTipOpenedEventId(20000) + get_CurrentName is a Phase-2 upgrade\n",
                BALLOON_SEND_TIMEOUT_MS);
        fprintf(f, "balloon_uia: UIA_ToolTipOpenedEventId = 20000 (min client Windows XP); "
                   "AddAutomationEventHandler(root, TreeScope_Subtree) official sample\n");
        fprintf(f, "balloon_limit: only STANDARD NIF_INFO tray balloons; self-drawn popups / "
                   "in-app toasts / Win10 Toast are invisible. No sender/process name "
                   "(balloon belongs to explorer), never fabricated\n");
        fprintf(f, "balloon_safety: NO global hook chain (unlike the WH_MOUSE_LL freeze), "
                   "NO worker thread, NO polling, NO explorer memory reads, NO dirty flag\n");
        fprintf(f, "balloon_enabled=%d hook_ok=%d tray_pid=%lu events=%d hits=%d "
                   "suppressed=%d class_hits=%d style_hits=%d text_len=%d\n",
                balloon_enabled(a), balloon_hook_ok(a),
                (unsigned long)balloon_tray_pid(a), balloon_events(a),
                balloon_hits(a), balloon_suppressed(a),
                balloon_class_hits(a), balloon_style_hits(a), balloon_text_len(a));
        fprintf(f, "balloon_last_text=%ls\n", balloon_last_text(a));
        fprintf(f, "balloon_switch: --balloonoff disables listening; --balloontest injects a "
                   "fake tooltip balloon (old --imoff/--imtest kept as aliases)\n");
    }
    fclose(f);
}

static void gesture_move_click(App *a, int id, int down)
{
    RECT r = ui_hit_rect(a, id);
    POINT p;
    INPUT in;
    if (r.right <= r.left) return;
    p.x = (r.left + r.right) / 2;
    p.y = (r.top + r.bottom) / 2;
    ClientToScreen(a->hwnd, &p);
    SetCursorPos(p.x, p.y);
    Sleep(60);                       /* 让 ui_poll 轮询到新光标位置 */
    ZeroMemory(&in, sizeof(in));
    in.type = INPUT_MOUSE;
    in.mi.dwFlags = down ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP;
    SendInput(1, &in, sizeof(INPUT));
}

static void gesture_drag(App *a, int id)
{
    RECT r = ui_hit_rect(a, id);
    POINT p1, p2;
    INPUT in;
    if (r.right <= r.left) return;
    p1.x = r.left + 2;  p1.y = (r.top + r.bottom) / 2;
    p2.x = r.right - 2; p2.y = p1.y;
    ClientToScreen(a->hwnd, &p1);
    ClientToScreen(a->hwnd, &p2);
    SetCursorPos(p1.x, p1.y);
    Sleep(60);
    ZeroMemory(&in, sizeof(in));
    in.type = INPUT_MOUSE;
    in.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
    SendInput(1, &in, sizeof(INPUT));
    Sleep(120);                       /* ui_poll 看到 down，记录 pressId */
    SetCursorPos(p2.x, p2.y);
    Sleep(160);                       /* 位移 > DRAG_THRESH，进入拖动 */
    in.mi.dwFlags = MOUSEEVENTF_LEFTUP;
    SendInput(1, &in, sizeof(INPUT));
}

/* 真实鼠标事件走一遍「按下→松开」路径，验证松开判定没有把点击吃掉。
   阶段：点 BRI → mode1；点 VOL → mode0；再进 mode1 后在滑块上拖动。 */
static void gesture_frame(App *a)
{
    DWORD t = qs_now() - a->testStart;
    a->testElapsed = t;
    switch (a->gestureStage) {
    case 0:
        if (t >= 200) { gesture_move_click(a, H_BRI, 1); a->gestureStage = 1; }
        break;
    case 1:
        if (t >= 420) { gesture_move_click(a, H_BRI, 0); a->gestureStage = 2; }
        break;
    case 2:
        if (t >= 700) {
            a->gestureMode1 = a->ui.mode;
            gesture_move_click(a, H_VOL, 1);
            a->gestureStage = 3;
        }
        break;
    case 3:
        if (t >= 920) { gesture_move_click(a, H_VOL, 0); a->gestureStage = 4; }
        break;
    case 4:
        if (t >= 1200) { a->gestureMode2 = a->ui.mode; a->gestureStage = 5; }
        break;
    case 5:
        if (t >= 1400) { gesture_move_click(a, H_BRI, 1); a->gestureStage = 6; }
        break;
    case 6:
        if (t >= 1600) { gesture_move_click(a, H_BRI, 0); a->gestureStage = 7; }
        break;
    case 7:
        if (t >= 1850) {
            gesture_drag(a, H_SLIDER_VOL);
            a->gestureStage = 8;
        }
        break;
    case 8:
        if (t >= 2400) {
            a->gestureMode3 = a->ui.mode;
            a->gestureVol = a->sys.vol;
            a->gestureStage = 9;
        }
        break;
    default: break;
    }
}

static void cd_move(App *a, int id)
{
    RECT r = ui_hit_rect(a, id);
    POINT p;
    if (r.right <= r.left) return;
    p.x = (r.left + r.right) / 2;
    p.y = (r.top + r.bottom) / 2;
    ClientToScreen(a->hwnd, &p);
    SetCursorPos(p.x, p.y);
    Sleep(60);
}

static void cd_wheel(App *a, int d)
{
    WPARAM wp = (WPARAM)(DWORD)(((DWORD)(short)d & 0xFFFFu) << 16);
    PostMessageW(a->hwnd, WM_MOUSEWHEEL, wp, MAKELPARAM(0, 0));
}

/* 真实光标 + WM_MOUSEWHEEL → ui_wheel → GetCursorPos → num_segment
   → countdown_adjust；再模拟自然归零，验证自动展开与用户确认后解除。 */
static void cd_frame(App *a)
{
    DWORD t = qs_now() - a->testStart;
    a->testElapsed = t;
    switch (a->cdStage) {
    case 0:
        if (t >= 200) { cd_move(a, H_NUM_M); a->cdStage = 1; }
        break;
    case 1:
        if (t >= 400) {
            POINT p, c;
            a->cdHitN = a->ui.hitN;
            a->cdMode = a->ui.mode;
            a->cdAccum = a->wheelAccum;
            a->cdNumR = ui_hit_rect(a, H_NUM_M);
            if (GetCursorPos(&p)) {
                c = p; ScreenToClient(a->hwnd, &c);
                a->cdCurX = c.x; a->cdCurY = c.y;
            }
            cd_wheel(a, 120);
            a->cdStage = 2;
        }
        break;
    case 2:
        if (t >= 700) {
            a->cdAfterMinUp = countdown_remain(&a->cd);
            cd_wheel(a, -120);
            a->cdStage = 3;
        }
        break;
    case 3:
        if (t >= 1000) {
            a->cdAfterMinDown = countdown_remain(&a->cd);
            cd_move(a, H_NUM_S);
            a->cdStage = 4;
        }
        break;
    case 4:
        if (t >= 1250) { cd_wheel(a, 120); a->cdStage = 5; }
        break;
    case 5:
        if (t >= 1550) {
            a->cdAfterSecUp = countdown_remain(&a->cd);
            a->cd.running = 1;
            a->cd.base = 300;
            a->cd.start = qs_now() - 400000;   /* 墙钟推算已归零 */
            a->cd.remain = 300;
            a->cdStage = 6;
        }
        break;
    case 6:
        if (t >= 2000) {
            a->cdEnded = a->cd.ended;
            a->cdExpanded = a->anim.expanded;
            a->cdVisible = a->visible;
            a->cdStage = 7;
        }
        break;
    case 7:
        if (t >= 2250) {
            /* 归零现在会弹即时通知卡片，点右上角 xmark 关闭 = 用户确认提醒；
               ui_activate 同时清掉 cd.ended / endNotified，回到正常计时器。 */
            ui_click_id(a, H_NOTIFY_CLOSE);
            a->cdCleared = (!a->cd.ended && !a->endNotified &&
                            !notify_active(&a->notify));
            a->cdStage = 8;
        }
        break;
    default: break;
    }
}

/* 便签端到端：点击 → 键盘输入 → 中文换行溢出 → 滚轮滚内容 → 保存回读 → 退出编辑 */
static void note_frame(App *a)
{
    DWORD t = qs_now() - a->testStart;
    a->testElapsed = t;
    switch (a->noteStage) {
    case 0:
        if (t >= 150) {
            double w, h, r;
            RECT nr;
            POINT p;
            INPUT in;
            a->ui.view = NOTE_VIEW; a->ui.mode = 0; a->ui.hoverSeg = -1;
            note_clear(&a->note);
            anim_view_size(&a->anim, NOTE_VIEW, &w, &h, &r);
            a->anim.curW = w; a->anim.curH = h; a->anim.curR = r;
            a->anim.expanded = 1; a->anim.exp = 1.0; a->anim.expV = 0.0;
            ui_render(a);
            a->noteLines0 = note_line_count(&a->note);
            nr = ui_hit_rect(a, H_NOTE);
            if (nr.right > nr.left) {
                p.x = (nr.left + nr.right) / 2;
                p.y = (nr.top + nr.bottom) / 2;
                ClientToScreen(a->hwnd, &p);
                SetCursorPos(p.x, p.y);
                Sleep(40);
                ZeroMemory(&in, sizeof(in));
                in.type = INPUT_MOUSE;
                in.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
                SendInput(1, &in, sizeof(INPUT));
                Sleep(40);
                in.mi.dwFlags = MOUSEEVENTF_LEFTUP;
                SendInput(1, &in, sizeof(INPUT));
            }
            a->noteStage = 1;
        }
        break;
    case 1:
        if (t >= 450) {
            a->noteEditingAfterClick = a->note.editing;
            if (!a->note.editing) input_begin(a);  /* wine 无窗口管理器时兜底 */
            /* 走 window_proc 转发的同一个入口：WM_CHAR / WM_KEYDOWN */
            input_message(a, a->hwnd, WM_CHAR, (WPARAM)L'H', 0);
            input_message(a, a->hwnd, WM_CHAR, (WPARAM)L'i', 0);
            input_message(a, a->hwnd, WM_CHAR, (WPARAM)13, 0);   /* 换行 */
            a->noteStage = 2;
        }
        break;
    case 2:
        if (t >= 750) {
            static const WCHAR zh[] =
                L"明天下午三点开会，记得带份材料和充电线。\n"
                L"顺便把上周的草稿整理一下，中文换行要自然。\n"
                L"第三行：滚动指示要极淡，不要系统方块和箭头。\n"
                L"第四行：内容继续往下写，验证溢出与裁剪。\n"
                L"第五行：最后一行，确认能滚到底。";
            NoteBox nb;
            note_insert(&a->note, zh, (int)wcslen(zh));
            a->dirty = 1;
            ui_render(a);
            ui_note_box(a, &nb);
            a->noteLines1 = note_line_count(&a->note);
            a->noteOverflow = note_overflow(&a->note);
            a->noteSysBar = note_bar_lit(a, nb);
            a->noteLen = a->note.len;
            a->noteCaret = a->note.stb.cursor;

            /* 选区链路：全选 → 高亮像素 → Shift 缩选 → 拖选 →
               打字替换选区 → 撤销 → 复制选中段 */
            {
                int before, lo, hi, selLen;
                note_select_all(&a->note);
                a->noteSelAll = (a->note.stb.select_start == 0 &&
                                 a->note.stb.select_end == a->note.len &&
                                 note_has_selection(&a->note));
                ui_render(a);
                a->noteSelLit = note_sel_lit(a, nb);   /* 高亮真的画出来了 */

                note_key(&a->note, NK_LEFT | NK_SHIFT);
                a->noteSelShift = (a->note.stb.select_end == a->note.len - 1);

                note_scroll(&a->note, -1000);           /* 回到首行再拖选 */
                /* 落在第 1 行（长中文行），能框到十几个字，不是只选 "Hi" */
                {
                    int yy = (int)(nb.y + NOTE_PAD + a->note.lineH * 1.5);
                    note_click(&a->note, a, nb,
                               (int)(nb.x + NOTE_PAD + 4), yy);
                    note_drag_to(&a->note, a, nb,
                                 (int)(nb.x + nb.w * 0.62), yy);
                }
                a->noteSelDrag = note_has_selection(&a->note);
                note_selection_range(&a->note, &lo, &hi);
                a->noteSelStart = lo; a->noteSelEnd = hi;
                selLen = hi - lo;

                before = a->note.len;
                {
                    static const WCHAR rep[] = L"替换";
                    note_insert(&a->note, rep, 2);
                }
                a->noteSelReplace = (!note_has_selection(&a->note) &&
                                     a->note.len == before - selLen + 2);
                /* 替换选区是「删除 + 插入」两条 undo 记录，逐条撤销直到还原 */
                {
                    int guard = 0;
                    while (a->note.len != before && guard++ < 4)
                        note_key(&a->note, NK_UNDO);
                    a->noteSelUndo = (a->note.len == before);
                }

                note_select_all(&a->note);
                a->noteSelCopy = note_copy_selection(&a->note, 0);

                /* 光标回到末尾：后续滚轮测试需要 topLine 在最大值 */
                a->note.stb.select_start = a->note.stb.select_end = a->note.len;
                a->note.stb.cursor = a->note.len;
                a->noteLen = a->note.len;
                a->noteCaret = a->note.stb.cursor;
                ui_render(a);
            }
            a->noteStage = 3;
        }
        break;
    case 3:
        if (t >= 1000) {
            NoteBox nb;
            POINT p;
            ui_note_box(a, &nb);
            p.x = (int)(nb.x + nb.w * 0.5);
            p.y = (int)(nb.y + nb.h * 0.5);
            ClientToScreen(a->hwnd, &p);
            SetCursorPos(p.x, p.y);
            a->noteTopLineBefore = a->note.topLine;
            ui_wheel(a, +120);        /* 上滚一行（光标在末尾时才有空间） */
            a->noteTopLineUp = a->note.topLine;
            ui_wheel(a, -120);        /* 下滚一行，回到原位 */
            a->noteTopLine = a->note.topLine;
            a->noteStage = 4;
        }
        break;
    case 4:
        if (t >= 1250) {
            NoteBox nb;
            ui_note_box(a, &nb);
            a->noteSysBar = note_bar_lit(a, nb);
            note_save(&a->note, 1);   /* 强制落盘 */
            {
                Note back;
                note_init(&back);
                a->noteRoundtrip = (back.len == a->note.len &&
                                    back.len > 0 &&
                                    memcmp(back.text, a->note.text,
                                           (size_t)back.len * sizeof(WCHAR)) == 0);
            }
            input_message(a, a->hwnd, WM_KEYDOWN, VK_ESCAPE, 0);
            a->noteEditing = a->note.editing;   /* 结束时应为 0 */
            a->noteStage = 5;
        }
        break;
    default: break;
    }
}

/* 性能监测端到端：展开第三张中卡片 → 两次差分采样 → 渲染像素断言 →
   切回收起态验证不再采样（静止零重绘不回退）。 */
static void perf_frame(App *a)
{
    DWORD t = qs_now() - a->testStart;
    a->testElapsed = t;
    switch (a->perfStage) {
    case 0:
        if (t >= 150) {
            double w, h, r, W, H, L;
            POINT p;
            a->ui.view = PERF_VIEW; a->ui.mode = 0; a->ui.hoverSeg = -1;
            anim_view_size(&a->anim, PERF_VIEW, &w, &h, &r);
            a->anim.curW = w; a->anim.curH = h; a->anim.curR = r;
            a->anim.expanded = 1; a->anim.exp = 1.0; a->anim.expV = 0.0;
            a->visible = 1; a->hover = 1;
            ui_render(a);
            W = w * a->u; H = h * a->u;
            L = ((double)a->canvas.W - W) * 0.5;
            p.x = (int)(L + W * 0.50); p.y = (int)(H * 0.50);
            ClientToScreen(a->hwnd, &p);
            SetCursorPos(p.x, p.y);           /* 让 ui_poll 保持展开 */
            a->perfSamples0 = a->perf.sampleCount;
            a->perfStage = 1;
        }
        break;
    case 1:
        if (t >= 620) {
            /* 两次采样间隔 >100ms，GetSystemTimes 差分才有意义 */
            perf_force_sample(&a->perf);
            Sleep(130);
            perf_force_sample(&a->perf);
            a->perfSamples1 = a->perf.sampleCount;
            a->perfCpuOK = (a->perf.cpu >= 0.0 && a->perf.cpu <= 100.0);
            a->perfMemOK = (a->perf.mem >= 0.0 && a->perf.mem <= 100.0 &&
                            a->perf.memTotalGB > 0.0);
            a->perfDiskOK = (a->perf.disk >= 0.0 && a->perf.disk <= 100.0 &&
                             a->perf.diskTotalGB > 0.0);
            a->perfHwCPU = (int)wcslen(a->perf.cpuName) > 3;
            a->perfHwGPU = (int)wcslen(a->perf.gpuName) > 3;
            a->perfHwOS = (int)wcslen(a->perf.osName) > 3;
            a->perfStage = 2;
        }
        break;
    case 2:
        if (t >= 1250) {
            double w, h, r, W, H, L;
            const DWORD *bits = (const DWORD *)a->canvas.outBits;
            int cw = a->canvas.W, ch = a->canvas.H, xx, yy;
            a->ui.view = PERF_VIEW;
            ui_render(a);
            anim_view_size(&a->anim, PERF_VIEW, &w, &h, &r);
            W = w * a->u; H = h * a->u;
            L = ((double)cw - W) * 0.5;
            a->perfRingsLit = 0; a->perfTextLit = 0;
            for (yy = (int)(H * 0.12); yy < (int)(H * 0.78) && yy < ch; yy++)
                for (xx = (int)(L + W * 0.09); xx < (int)(L + W * 0.60) && xx < cw; xx++) {
                    DWORD v = bits[yy * cw + xx];
                    int B = (int)(v & 255), G = (int)((v >> 8) & 255);
                    int R = (int)((v >> 16) & 255);
                    int mx = R > G ? R : G; if (B > mx) mx = B;
                    int mn = R < G ? R : G; if (B < mn) mn = B;
                    if (((v >> 24) & 255) > 200 && mx - mn > 30) a->perfRingsLit++;
                }
            for (yy = (int)(H * 0.05); yy < (int)(H * 0.98) && yy < ch; yy++)
                for (xx = (int)(L + W * 0.60); xx < (int)(L + W * 0.99) && xx < cw; xx++) {
                    DWORD v = bits[yy * cw + xx];
                    int B = (int)(v & 255), R = (int)((v >> 16) & 255);
                    if (((v >> 24) & 255) > 200 && R >= 70 && R <= 230 && B - R < 30)
                        a->perfTextLit++;
                }
            /* 切回收起态：记录采样计数，stage 3 再核对是否还在涨 */
            a->ui.view = 0; a->hover = 0;
            a->perfHiddenSamples = a->perf.sampleCount;
            a->perfStage = 3;
        }
        break;
    case 3:
        if (t >= 1800) {
            a->perfNoDirtyWhenHidden =
                (a->perf.sampleCount == a->perfHiddenSamples);
        }
        break;
    default: break;
    }
}

/* ---- --cliptest：剪贴板历史端到端 ----
   覆盖：去重 / 空白跳过 / 旧条目提到顶部 / 上限 / 滚动 / 真实写回系统剪贴板 /
   自写回通知不增殖 / 点击行回填 / 持久化 roundtrip / 像素裁切与滚动指示。
   演示数据一律走 clipboard_test.bin，不碰用户真实历史。 */
static void clip_frame(App *a)
{
    static WCHAR firstSaved[CLIP_MAX_CHARS + 1];
    int want;
    a->testElapsed = qs_now() - a->testStart;
    want = (int)(a->testElapsed / 300);
    if (want > 8) want = 8;
    if (want <= a->clipStage) return;
    switch (want) {
    case 1: {
        ClipBox cb;
        a->clip.testFile = 1;
        clip_clear(&a->clip, NULL);
        a->clipCount0 = clip_count(&a->clip);
        snap(a, CLIP_VIEW, 0, NULL);
        ui_clip_box(a, &cb);
        a->clipEmptyLit = clip_lit_px(a, cb);  /* 空态图标亮像素 */
        break;
    }
    case 2: {
        static const WCHAR *t[] = {
            L"第一条中文内容",
            L"第二条：Qisland clipboard history",
            L"第三条：https://learn.microsoft.com/windows/win32/dataxchg/clipboard",
            L"第四条：第一行\n第二行\t回填时保留原文",
            L"第五条：git commit -m \"clipboard history card\""
        };
        int i;
        clip_push_text(&a->clip, t[0], (int)wcslen(t[0]));
        clip_push_text(&a->clip, t[0], (int)wcslen(t[0]));  /* 完全相同 → 去重 */
        clip_push_text(&a->clip, L"   \n  ", 6);           /* 全空白 → 跳过 */
        for (i = 1; i < 5; i++)
            clip_push_text(&a->clip, t[i], (int)wcslen(t[i]));
        clip_push_text(&a->clip, t[0], (int)wcslen(t[0]));  /* 旧条目 → 提到顶部 */
        a->clipCount1 = clip_count(&a->clip);               /* 期望 5 */
        a->clipDup = clip_dups(&a->clip);                   /* 期望 1 */
        break;
    }
    case 3: {
        int i;
        WCHAR buf[64];
        for (i = 0; i < 4; i++) {
            _snwprintf(buf, 64, L"补充条目 %d：验证溢出与滚动", i + 1);
            buf[63] = 0;
            clip_push_text(&a->clip, buf, (int)wcslen(buf));
        }
        snap(a, CLIP_VIEW, 0, NULL);
        a->clipTopBefore = a->clip.top;
        clip_scroll(&a->clip, 2);
        a->clipTopAfter = a->clip.top;
        a->clipScrollOK = (a->clipTopAfter > a->clipTopBefore &&
                           clip_overflow(&a->clip));
        clip_scroll(&a->clip, -2);
        break;
    }
    case 4: {
        static const WCHAR text[] = L"回填验证：中文 ABC 123";
        WCHAR back[CLIP_MAX_CHARS + 1];
        int n, before = clip_count(&a->clip);
        a->clipWriteOK = clip_set_clipboard(&a->clip, a, text,
                                            (int)wcslen(text));
        n = clip_read_text(a->hwnd, back, CLIP_MAX_CHARS + 1);
        if (n <= 0 || wcscmp(back, text) != 0) a->clipWriteOK = 0;
        /* 强制让 WM_CLIPBOARDUPDATE 走完整条判定（正常运行时序列号会挡住），
           验证自写回不会被当成一次新复制 */
        a->clip.lastSeq = 0;
        clip_on_update(&a->clip, a);
        a->clipSelfNoGrow = (clip_count(&a->clip) == before);
        a->clipSelfSkip = clip_self_skipped(&a->clip);
        break;
    }
    case 5: {
        ClipBox cb;
        WCHAR back[CLIP_MAX_CHARS + 1];
        int n;
        ui_clip_box(a, &cb);
        clip_click(&a->clip, a, cb,
                   (int)(cb.x + 20.0), (int)(cb.y + CLIP_PAD + 2.0));
        a->clipSelected = a->clip.selected;
        n = clip_read_text(a->hwnd, back, CLIP_MAX_CHARS + 1);
        a->clipClickWriteOK =
            (a->clipSelected >= 0 && n > 0 &&
             wcscmp(back, clip_at(&a->clip, a->clipSelected)) == 0);
        break;
    }
    case 6: {
        int before = clip_count(&a->clip);
        wcsncpy(firstSaved, clip_at(&a->clip, 0), CLIP_MAX_CHARS);
        firstSaved[CLIP_MAX_CHARS] = 0;
        clip_save(&a->clip, 1);
        clip_reload(&a->clip);
        a->clipRoundtrip = (clip_count(&a->clip) == before && before > 0 &&
                            wcscmp(clip_at(&a->clip, 0), firstSaved) == 0);
        break;
    }
    case 7: {
        ClipBox cb;
        snap(a, CLIP_VIEW, 0, NULL);
        ui_clip_box(a, &cb);
        a->clipLit = clip_lit_px(a, cb);
        a->clipSpill = clip_spill_px(a, cb);
        a->clipThumb = clip_thumb_lit_px(a, cb);
        break;
    }
    case 8: {
        a->clipListener = clip_listener(&a->clip);
        break;
    }
    default: break;
    }
    a->clipStage = want;
}

/* ---- --notifytest：即时通知系统端到端 ----
   覆盖：中卡尺寸覆盖 / FA 与 HICON 两种图标 / 程序名+详细信息两行 /
   不画竖点横点 / 点击关闭与视图还原 / 过期自动关闭 / 不写盘 /
   倒计时归零触发通知（用户要求的第一个接入方）。 */
static void notify_frame(App *a)
{
    int want;
    a->testElapsed = qs_now() - a->testStart;
    want = (int)(a->testElapsed / 300);
    if (want > 8) want = 8;
    if (want <= a->notifyStage) return;
    switch (want) {
    case 1: {
        double w, h, r;
        a->notifyActive0 = notify_active(&a->notify);
        notify_show_fa(&a->notify, a, FA_BELL, L"Qisland",
                       L"倒计时已结束 · 00:00:00", 60000);
        snap(a, 0, 0, NULL);
        anim_view_size(&a->anim, a->ui.view, &w, &h, &r);
        a->notifyActive1 = notify_active(&a->notify);
        a->notifyMidSize = (int)(fabs(w - MID_W) < 0.01 &&
                                 fabs(h - MID_H) < 0.01);
        break;
    }
    case 2: {
        int ic, tx;
        notify_lit_parts(a, &ic, &tx);
        a->notifyIconLit = ic;
        a->notifyTextLit = tx;
        a->notifyNoDots = (notify_dots_lit_px(a) == 0);
        a->notifyNoHalo = (notify_halo_lit_px(a) == 0);
        a->notifyNoFile = notify_no_file();
        a->notifyNoPersist = (a->ui.wheelRect.right == 0 &&
                              a->ui.hdotsRect.right == 0);
        a->notifyGapOK = (int)(notify_gap_px(a) >= NOTIFY_GAP_MIN * a->u);
        break;
    }
    case 3: {
        DBoxNotify body, close;
        int saved = a->ui.view;
        ui_notify_boxes(a, &body, &close);
        /* 点右上角 xmark：优先命中 H_NOTIFY_CLOSE，dismiss 并还原视图 */
        ui_click(a, (int)(close.x + close.w * 0.5),
                 (int)(close.y + close.h * 0.5));
        a->notifyDismissOK = !notify_active(&a->notify);
        a->notifyViewRestored = (a->ui.view == saved);
        (void)body;
        break;
    }
    case 4: {
        HICON ico = notify_icon_from_file(L"notepad.exe");
        int ic, tx;
        if (!ico) ico = (HICON)LoadIconW(NULL, IDI_APPLICATION);
        if (ico) {
            notify_show_icon(&a->notify, a, ico, L"记事本",
                             L"软件图标示例：左侧显示来源程序 HICON", 60000);
            snap(a, 0, 0, NULL);
            notify_lit_parts(a, &ic, &tx);
            a->notifyHIconLit = ic;
            /* 共享图标不 DestroyIcon；文件图标才需要 */
            if (ico != (HICON)LoadIconW(NULL, IDI_APPLICATION)) DestroyIcon(ico);
        } else {
            /* 连系统图标都取不到才降级到 FA（wine 上一般不会） */
            notify_show_fa(&a->notify, a, FA_BELL, L"记事本",
                           L"软件图标不可用，已回退 FA", 60000);
            snap(a, 0, 0, NULL);
            notify_lit_parts(a, &ic, &tx);
            a->notifyHIconLit = ic;
        }
        break;
    }
    case 5: {
        /* 把过期时刻提前，模拟到点自动消失（不写盘、不排队） */
        int before = notify_expired(&a->notify);
        a->notify.until = qs_now() - 1;
        notify_tick(&a->notify, a, qs_now());
        a->notifyExpiredOK = (!notify_active(&a->notify) &&
                              notify_expired(&a->notify) == before + 1);
        break;
    }
    case 6: {
        /* 倒计时自然归零：走真实的 ui_countdown_ended 链路 */
        int saved = 2;
        a->ui.view = saved;                 /* 故意停在中卡片，验证还原 */
        a->cd.ended = 1;
        a->endNotified = 0;
        ui_countdown_ended(a);
        a->notifyCountdownOK =
            (notify_active(&a->notify) &&
             wcscmp(notify_app_name(&a->notify), L"Qisland") == 0 &&
             wcsstr(notify_detail(&a->notify), L"倒计时") != NULL);
        notify_dismiss(&a->notify, a);
        a->notifyViewRestored &= (a->ui.view == 0 || a->ui.view == saved);
        ui_clear_end(a);
        break;
    }
    case 7: {
        a->notifyShown = notify_shown(&a->notify);
        a->notifyNoFile &= notify_no_file();
        /* 通知已关闭，此时渲染的是正常卡片，断言才有效 */
        a->notifyOldHintDiff = old_end_hint_diff_px(a);
        slider_mode_check(a, &a->sliderPlayHit, &a->sliderResetHit,
                          &a->sliderRightLit);
        break;
    }
    case 8:
        break;
    default: break;
    }
    a->notifyStage = want;
}

/* ---- --balloontest：Win7 托盘气泡监听端到端 ----
   覆盖：WinEvent hook 注册 / 假气泡窗口（tooltips_class32 + TTS_BALLOON）/
   普通 tooltip 被过滤 / 真实 WinEvent 投递（wine 可能不实现）/ 同一条
   PostMessage → WndProc → balloon_on_signal → 读文本 → notify 链路 /
   去抖 / 拨回冷却后再弹 / 不写盘。原即时消息专属监听已整体删除。 */
static void balloon_frame(App *a)
{
    int want;
    static HWND fakeBalloon = NULL, plainTip = NULL;
    a->testElapsed = qs_now() - a->testStart;
    want = (int)(a->testElapsed / 350);
    if (want > 7) want = 7;
    if (want <= a->balloonStage) return;
    switch (want) {
    case 1:
        a->balloonHookOK   = balloon_hook_ok(a);
        a->balloonTrayPid  = (int)balloon_tray_pid(a);
        a->balloonEvents0  = balloon_events(a);
        /* 普通 tooltip（无 TTS_BALLOON 样式）不应触发任何通知 */
        balloon_test_make(L"普通提示（不是气泡）", 0, &plainTip);
        break;
    case 2:
        a->balloonPlainIgnored = !notify_active(&a->notify);
        /* 真气泡：类名含 tooltips_class32 子串 + TTS_BALLOON 位 + 可读文本 */
        a->balloonFakeOK = balloon_test_make(
            L"这是 Win7 托盘气泡消息 · 测试正文", 1, &fakeBalloon);
        break;
    case 3:
        /* WinEvent 是异步排队的：真机能投递，wine 若未实现则用
           PostMessage 直接注入同一条 WndProc → balloon_on_signal 链路。 */
        a->balloonEvents = balloon_events(a);
        if (a->balloonEvents <= a->balloonEvents0 && fakeBalloon)
            PostMessageW(a->hwnd, WM_APP_BALLOON_SIGNAL,
                         (WPARAM)EVENT_OBJECT_SHOW, (LPARAM)fakeBalloon);
        break;
    case 4: {
        WCHAR buf[BALLOON_TEXT_MAX + 1];
        int n = balloon_read_text(fakeBalloon, buf, BALLOON_TEXT_MAX + 1);
        a->balloonSignalActive = notify_active(&a->notify);
        a->balloonTextOK  = (n > 0 && wcsstr(buf, L"托盘气泡") != NULL);
        a->balloonTextLen = balloon_text_len(a);
        /* 冷却期内再注入一次 → 必须被压制 */
        if (fakeBalloon)
            PostMessageW(a->hwnd, WM_APP_BALLOON_SIGNAL,
                         (WPARAM)EVENT_OBJECT_CREATE, (LPARAM)fakeBalloon);
        break;
    }
    case 5:
        a->balloonSupp = balloon_suppressed(a);
        balloon_test_rewind(a, BALLOON_COOLDOWN_MS + 1000);
        if (fakeBalloon)
            PostMessageW(a->hwnd, WM_APP_BALLOON_SIGNAL,
                         (WPARAM)EVENT_OBJECT_SHOW, (LPARAM)fakeBalloon);
        break;
    case 6:
        a->balloonRewindOK = (balloon_hits(a) >= 2);
        a->balloonNoFile   = notify_no_file();
        a->balloonOffOK    = balloon_enabled(a);   /* --balloonoff 单独验证 */
        break;
    case 7:
        a->balloonEvents     = balloon_events(a);
        a->balloonHits       = balloon_hits(a);
        a->balloonClassHits  = balloon_class_hits(a);
        a->balloonStyleHits  = balloon_style_hits(a);
        balloon_test_destroy(fakeBalloon); fakeBalloon = NULL;
        balloon_test_destroy(plainTip);   plainTip   = NULL;
        break;
    default: break;
    }
    a->balloonStage = want;
}

void tests_frame(App *a)
{
    if (a->bench) {
        for (int k = 0; k < 8; k++) {
            LPARAM p = MAKELPARAM(200 + k, 20);
            PostMessageW(a->hwnd, WM_MOUSEMOVE, 0, p);
            PostMessageW(a->hwnd, WM_LBUTTONDOWN, MK_LBUTTON, p);
            PostMessageW(a->hwnd, WM_LBUTTONUP, 0, p);
            /* 光标消息洪泛：只 SetCursor，绝不能触发重绘 */
            PostMessageW(a->hwnd, WM_SETCURSOR, (WPARAM)a->hwnd,
                         MAKELPARAM(HTCLIENT, WM_MOUSEMOVE));
        }
        a->testElapsed = qs_now() - a->testStart;
        return;
    }
    if (a->wheelTest) {
        /* 5 张卡、唯一横向组 3 点。往下滚（delta<0）= 下一个卡片。
           步 1-4 左竖条：0→1→2(性能)→3(大卡片)→4(超大卡片)；
           步 5-6 光标移到底部横排圆点：末端夹取 / 组内横切回大卡片；
           步 7-9 回到左竖条：上滚回性能页，再下滚走回超大卡片。 */
        static const int seq[9] = { -120, -120, -120, -120, -120,
                                    120, 120, -120, -120 };
        a->testElapsed = qs_now() - a->testStart;
        {
            int want = (int)(a->testElapsed / 320);
            if (want > 9) want = 9;
            if (want > a->wheelSent && a->wheelSent < 9) {
                if (a->wheelLogN == 0) a->wheelBefore = a->ui.view;
                if (a->wheelLogN < 12) a->wheelLog[a->wheelLogN++] = a->ui.view;
                {
                    /* 步 5/6（下标 4/5）用横排点热区，其余用左竖条 */
                    const RECT *wr = (a->wheelSent >= 4 && a->wheelSent <= 5)
                                   ? &a->ui.hdotsRect : ui_wheel_rect(a);
                    if (wr->right > wr->left) {
                        POINT p;
                        p.x = (wr->left + wr->right) / 2;
                        p.y = (wr->top + wr->bottom) / 2;
                        ClientToScreen(a->hwnd, &p);
                        SetCursorPos(p.x, p.y);
                    }
                }
                Sleep(120);
                {
                    int d = seq[a->wheelSent];
                    WPARAM wp = (WPARAM)(DWORD)(((DWORD)(short)d & 0xFFFFu) << 16);
                    PostMessageW(a->hwnd, WM_MOUSEWHEEL, wp, MAKELPARAM(0, 0));
                }
                a->wheelSent++;
            }
        }
    }
    if (a->gestureTest) gesture_frame(a);
    if (a->cdTest) cd_frame(a);
    if (a->noteTest) note_frame(a);
    if (a->perfTest) perf_frame(a);
    if (a->clipTest) clip_frame(a);
    if (a->notifyTest) notify_frame(a);
    if (a->balloonTest) balloon_frame(a);
}

int tests_should_stop(const App *a)
{
    if (a->bench && a->testElapsed >= 3000) return 1;
    if (a->wheelTest && a->wheelSent >= 9 && a->testElapsed >= 3100) return 1;
    if (a->gestureTest && a->gestureStage >= 9 && a->testElapsed >= 2600) return 1;
    if (a->cdTest && a->cdStage >= 8 && a->testElapsed >= 2700) return 1;
    if (a->noteTest && a->noteStage >= 5 && a->testElapsed >= 1600) return 1;
    if (a->perfTest && a->perfStage >= 3 && a->testElapsed >= 2000) return 1;
    if (a->clipTest && a->clipStage >= 8 && a->testElapsed >= 2700) return 1;
    if (a->notifyTest && a->notifyStage >= 8 && a->testElapsed >= 2700) return 1;
    if (a->balloonTest && a->balloonStage >= 7 && a->testElapsed >= 3000) return 1;
    if ((a->bench || a->wheelTest || a->gestureTest || a->cdTest ||
         a->noteTest || a->perfTest || a->clipTest || a->notifyTest || a->balloonTest) &&
        a->testElapsed >= 8000) return 1;
    return 0;
}

void tests_finish(App *a)
{
    if (a->balloonTest) {
        FILE *f = fopen("balloontest.txt", "w");
        if (f) {
            fprintf(f, "verdict=CAN receive Win7 tray balloons, but only by OBSERVING "
                       "explorer's tooltip window; NIF_INFO callbacks are NOT broadcast\n");
            fprintf(f, "official: NOTIFYICONDATA NIF_INFO -> szInfo/szInfoTitle; "
                       "NIN_BALLOONSHOW/TIMEOUT/USERCLICK go ONLY to the icon owner "
                       "(Shell_NotifyIcon NIM_MODIFY, XP+)\n");
            fprintf(f, "hook: SetWinEventHook(CREATE 0x8000 / SHOW 0x8002, "
                       "WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS); "
                       "caller thread must have a message loop; Win2000+\n");
            fprintf(f, "filter: tooltips_class32 + TTS_BALLOON(0x40) + explorer pid; "
                       "callback only PostMessage(HWND), no App/UI state, no dirty flag\n");
            fprintf(f, "text: WM_GETTEXTLENGTH + WM_GETTEXT (SendMessageTimeout "
                       "SMTO_ABORTIFHUNG %dms), GetWindowText fallback; "
                       "UIA_ToolTipOpenedEventId(20000) is a Phase-2 upgrade\n",
                    BALLOON_SEND_TIMEOUT_MS);
            fprintf(f, "safety: no global hook chain / no worker thread / no polling / "
                       "no explorer memory reads\n");
            fprintf(f, "stage=%d\n", a->balloonStage);
            fprintf(f, "hook_ok=%d tray_pid=%d events=%d (expect 1 / "
                       "any pid / >=1: wine also provides Shell_TrayWnd)\n",
                    a->balloonHookOK, a->balloonTrayPid, a->balloonEvents);
            fprintf(f, "fake_balloon_built=%d plain_tooltip_ignored=%d "
                    "(expect 1 / 1)\n", a->balloonFakeOK, a->balloonPlainIgnored);
            fprintf(f, "signal_notify_active=%d text_read_ok=%d text_len=%d "
                    "(expect 1 / 1 / >0)\n",
                    a->balloonSignalActive, a->balloonTextOK, a->balloonTextLen);
            fprintf(f, "cooldown_suppressed=%d hits_after_rewind=%d "
                    "(expect >=1 / 1)\n", a->balloonSupp, a->balloonRewindOK);
            fprintf(f, "class_hits=%d style_hits=%d total_hits=%d "
                    "(expect >0 / >0 / >=2)\n",
                    a->balloonClassHits, a->balloonStyleHits, a->balloonHits);
            fprintf(f, "no_storage_file=%d enabled=%d (expect 1 / 1: "
                    "通知仍完全内存态)\n", a->balloonNoFile, a->balloonOffOK);
            fprintf(f, "render_count=%lu elapsed_ms=%lu\n",
                    (unsigned long)a->renderCount, (unsigned long)a->testElapsed);
            fclose(f);
        }
    }
    if (a->notifyTest) {
        FILE *f = fopen("notifytest.txt", "w");
        if (f) {
            fprintf(f, "path=ui_countdown_ended -> notify_show_fa -> anim.overrideMid -> "
                       "notify_draw -> click/expire -> notify_dismiss\n");
            fprintf(f, "stage=%d\n", a->notifyStage);
            fprintf(f, "active_before=%d active_after=%d (expect 0 / 1)\n",
                    a->notifyActive0, a->notifyActive1);
            fprintf(f, "size_is_mid=%d (expect 1: notification card = MID 480x130)\n",
                    a->notifyMidSize);
            fprintf(f, "icon_lit=%d text_lit=%d (expect >0 / >0: FA/HICON + appName + detail)\n",
                    a->notifyIconLit, a->notifyTextLit);
            fprintf(f, "no_vertical_or_horizontal_dots=%d no_nav_hit_rects=%d "
                       "(expect 1 / 1)\n", a->notifyNoDots, a->notifyNoPersist);
            fprintf(f, "hicon_lit=%d (software icon path, FA fallback allowed)\n",
                    a->notifyHIconLit);
            fprintf(f, "click_dismiss=%d view_restored=%d (expect 1 / 1)\n",
                    a->notifyDismissOK, a->notifyViewRestored);
            fprintf(f, "expire_auto_dismiss=%d (expect 1)\n", a->notifyExpiredOK);
            fprintf(f, "countdown_hook=%d (expect 1: 倒计时归零弹通知)\n",
                    a->notifyCountdownOK);
            fprintf(f, "no_storage_file=%d shown=%d "
                       "(expect 1 / >=3: 完全内存态，不写盘不排队)\n",
                    a->notifyNoFile, a->notifyShown);
            fprintf(f, "notify_icon_halo_lit=%d (expect 1: 图标底下无灰白圆底 / 圆形容器)\n",
                    a->notifyNoHalo);
            fprintf(f, "notify_gap_ok=%d gap_px=%.1f min=%.1f "
                       "(expect 1: 程序名不贴图标)\n",
                    a->notifyGapOK, notify_gap_px(a), NOTIFY_GAP_MIN * a->u);
            fprintf(f, "old_end_hint_diff_px=%d removed=%d "
                       "(expect 0 / 1: 旧 bell 常驻提示已删)\n",
                    a->notifyOldHintDiff, (int)(a->notifyOldHintDiff == 0));
            fprintf(f, "slider_mode play_hit=%d reset_hit=%d right_lit=%d "
                       "(expect 0 / 0 / 0: 双滑块模式隐藏 play/reset)\n",
                    a->sliderPlayHit, a->sliderResetHit, a->sliderRightLit);
            fprintf(f, "render_count=%lu elapsed_ms=%lu\n",
                    (unsigned long)a->renderCount, (unsigned long)a->testElapsed);
            fclose(f);
        }
    }
    if (a->clipTest) {
        FILE *f = fopen("cliptest.txt", "w");
        if (f) {
            fprintf(f, "view=%d (first BIG card = clipboard history)\n", CLIP_VIEW);
            fprintf(f, "path=AddClipboardFormatListener -> WM_CLIPBOARDUPDATE -> "
                       "CF_UNICODETEXT -> dedup -> clip_draw -> click -> SetClipboardData\n");
            fprintf(f, "stage=%d\n", a->clipStage);
            fprintf(f, "count_empty=%d count_after_inserts=%d (expect 0 / 5)\n",
                    a->clipCount0, a->clipCount1);
            fprintf(f, "dup_skipped=%d (expect 1) self_skipped=%d\n",
                    a->clipDup, a->clipSelfSkip);
            fprintf(f, "scroll top %d -> %d overflow_ok=%d (expect 0 -> 2 / 1)\n",
                    a->clipTopBefore, a->clipTopAfter, a->clipScrollOK);
            fprintf(f, "set_clipboard_write_ok=%d readback_match=%d "
                       "(wine may reject X11 clipboard -> expect degraded)\n",
                    clip_write_ok(&a->clip), a->clipWriteOK);
            fprintf(f, "self_update_no_grow=%d (expect 1)\n", a->clipSelfNoGrow);
            fprintf(f, "click_selected=%d click_writeback_match=%d (expect >=0 / 1)\n",
                    a->clipSelected, a->clipClickWriteOK);
            fprintf(f, "persist_roundtrip=%d (expect 1)\n", a->clipRoundtrip);
            fprintf(f, "empty_lit=%d list_lit=%d spill_below=%d thumb_lit=%d "
                       "(expect >0 / >0 / 0 / >0)\n",
                    a->clipEmptyLit, a->clipLit, a->clipSpill, a->clipThumb);
            fprintf(f, "listener_ok=%d (Vista+ AddClipboardFormatListener)\n",
                    a->clipListener);
            fprintf(f, "render_count=%lu elapsed_ms=%lu\n",
                    (unsigned long)a->renderCount, (unsigned long)a->testElapsed);
            fprintf(f, "api_ownership=GlobalAlloc(GMEM_MOVEABLE) + SetClipboardData; "
                       "system owns hMem on success, never GlobalFree\n");
            fclose(f);
        }
    }
    if (a->noteTest) {
        FILE *f = fopen("notetest.txt", "w");
        if (f) {
            fprintf(f, "view=%d (second medium card = sticky note)\n", NOTE_VIEW);
            fprintf(f, "path=click H_NOTE -> input_begin -> WM_CHAR -> note_insert -> note_draw\n");
            fprintf(f, "stage=%d\n", a->noteStage);
            fprintf(f, "empty_lines=%d (expect 1)\n", a->noteLines0);
            fprintf(f, "editing_after_click=%d (1 = click acquired focus; 0 = test env has no WM)\n",
                    a->noteEditingAfterClick);
            fprintf(f, "typed_len=%d caret=%d\n", a->noteLen, a->noteCaret);
            fprintf(f, "long_lines=%d overflow=%d (expect >0/1)\n",
                    a->noteLines1, a->noteOverflow);
            fprintf(f, "self_drawn_thumb_lit=%d (expect >0 when overflow)\n",
                    a->noteSysBar);
            fprintf(f, "editor=stb_textedit v1.16 (public domain, no HWND child control)\n");
            fprintf(f, "select_all=%d shift_shrink=%d drag_select=%d sel=(%d,%d)\n",
                    a->noteSelAll, a->noteSelShift, a->noteSelDrag,
                    a->noteSelStart, a->noteSelEnd);
            fprintf(f, "selection_highlight_lit=%d (expect >0, drawn into alpha bitmap)\n",
                    a->noteSelLit);
            fprintf(f, "type_replaces_selection=%d undo_restores=%d copy_chars=%d\n",
                    a->noteSelReplace, a->noteSelUndo, a->noteSelCopy);
            fprintf(f, "wheel_up=%d from=%d (expect -1)  wheel_down=%d (expect back to %d)\n",
                    a->noteTopLineUp, a->noteTopLineBefore,
                    a->noteTopLine, a->noteTopLineBefore);
            fprintf(f, "persist_roundtrip=%d (expect 1)\n", a->noteRoundtrip);
            fprintf(f, "editing_after_esc=%d (expect 0)\n", a->noteEditing);
            fprintf(f, "note_scrollbar: no EDIT/RichEdit, no WS_VSCROLL, clipped to note box\n");
            fprintf(f, "render_count=%lu elapsed_ms=%lu\n",
                    (unsigned long)a->renderCount, (unsigned long)a->testElapsed);
            fclose(f);
        }
    }
    if (a->perfTest) {
        FILE *f = fopen("perftest.txt", "w");
        if (f) {
            char hw[1024];
            perf_hw_summary(&a->perf, hw, (int)sizeof(hw));
            fprintf(f, "view=%d (third medium card = performance monitor)\n", PERF_VIEW);
            fprintf(f, "path=GetSystemTimes(diff) -> GlobalMemoryStatusEx -> "
                       "GetDiskFreeSpaceExW -> perf_draw -> alpha bitmap\n");
            fprintf(f, "stage=%d\n", a->perfStage);
            fprintf(f, "samples %d -> %d (expect increasing)\n",
                    a->perfSamples0, a->perfSamples1);
            fprintf(f, "live cpu=%.1f mem=%.1f disk=%.1f\n",
                    a->perf.cpu, a->perf.mem, a->perf.disk);
            fprintf(f, "range cpu_ok=%d mem_ok=%d disk_ok=%d (expect 1/1/1)\n",
                    a->perfCpuOK, a->perfMemOK, a->perfDiskOK);
            fprintf(f, "hw cpu=%d gpu=%d os=%d (expect 1/1/1)\n",
                    a->perfHwCPU, a->perfHwGPU, a->perfHwOS);
            fprintf(f, "hw_summary=%s\n", hw);
            fprintf(f, "ring_lit_px=%d hw_text_lit_px=%d (expect >0/>0)\n",
                    a->perfRingsLit, a->perfTextLit);
            fprintf(f, "no_sampling_when_hidden=%d (expect 1, zero-redraw kept)\n",
                    a->perfNoDirtyWhenHidden);
            fprintf(f, "render_count=%lu elapsed_ms=%lu\n",
                    (unsigned long)a->renderCount, (unsigned long)a->testElapsed);
            fclose(f);
        }
    }
    if (a->cdTest) {
        FILE *f = fopen("cdtest.txt", "w");
        if (f) {
            fprintf(f, "path=cursor_on_digits -> WM_MOUSEWHEEL -> ui_wheel -> num_segment -> countdown_adjust\n");
            fprintf(f, "stage=%d\n", a->cdStage);
            fprintf(f, "MM wheel up   -> %.1f (expect 360)\n", a->cdAfterMinUp);
            fprintf(f, "MM wheel down -> %.1f (expect 300)\n", a->cdAfterMinDown);
            fprintf(f, "SS wheel up   -> %.1f (expect 305)\n", a->cdAfterSecUp);
            fprintf(f, "natural_zero  -> ended=%d expanded=%d visible=%d "
                       "(expect 1/1/1: 归零只弹新版通知，不再切页/画 bell)\n",
                    a->cdEnded, a->cdExpanded, a->cdVisible);
            fprintf(f, "click notify -> cleared=%d (expect 1: 通知关闭即确认提醒)\n",
                    a->cdCleared);
            fprintf(f, "snapshot hitN=%d mode=%d accum=%d cursor=(%d,%d) numM=(%ld,%ld)-(%ld,%ld)\n",
                    a->cdHitN, a->cdMode, a->cdAccum, a->cdCurX, a->cdCurY,
                    (long)a->cdNumR.left, (long)a->cdNumR.top,
                    (long)a->cdNumR.right, (long)a->cdNumR.bottom);
            fprintf(f, "render_count=%lu elapsed_ms=%lu\n",
                    (unsigned long)a->renderCount, (unsigned long)a->testElapsed);
            fclose(f);
        }
    }
    if (a->gestureTest) {
        FILE *f = fopen("gesture.txt", "w");
        if (f) {
            fprintf(f, "path=SendInput(LEFTDOWN/LEFTUP) -> GetAsyncKeyState -> ui_poll\n");
            fprintf(f, "stage=%d\nmode_after_click_BRI=%d (expect 1)\nmode_after_click_VOL=%d (expect 0)\n",
                    a->gestureStage, a->gestureMode1, a->gestureMode2);
            fprintf(f, "mode_after_slider_drag=%d (expect 1, drag must not toggle)\n",
                    a->gestureMode3);
            fprintf(f, "vol_after_drag_to_right=%.3f (SendInput drag; wine may miss the "
                       "poll window and leave the 0.5 default)\n", a->gestureVol);
            /* 纯逻辑对照：直接点音量滑块右端，不依赖 SendInput 时序。
               只要这行是 1.000，set_from_x / slider_geom 就没回退。 */
            {
                double saved = a->sys.vol;
                int savedMode = a->ui.mode;
                a->ui.mode = 1;
                {
                    double w, h, r;
                    anim_geometry(&a->anim, &w, &h, &r);
                    ui_layout(a, w * a->u, h * a->u);
                }
                {
                    RECT sr = ui_hit_rect(a, H_SLIDER_VOL);
                    if (sr.right > sr.left)
                        ui_click(a, sr.right - 2, (sr.top + sr.bottom) / 2);
                }
                fprintf(f, "vol_direct_click_right=%.3f (expect 1.000; pure logic, "
                           "no SendInput)\n", a->sys.vol);
                a->sys.vol = saved; a->ui.mode = savedMode;
            }
            fprintf(f, "render_count=%lu\nelapsed_ms=%lu\n",
                    (unsigned long)a->renderCount, (unsigned long)a->testElapsed);
            fclose(f);
        }
    }
    if (a->bench) {
        FILE *f = fopen("bench.txt", "w");
        if (f) {
            fprintf(f, "elapsed_ms=%lu\nrender_count=%lu\nflood_msg_per_frame=24\n",
                    (unsigned long)a->testElapsed, (unsigned long)a->renderCount);
            {
                RECT r;
                GetWindowRect(a->hwnd, &r);
                const int px[3] = { r.left + a->winW / 2, r.left + 5, r.left - 60 };
                const int py[3] = { r.top + (int)(ISLAND_H * 0.5 * a->u),
                                    r.top + a->winH - 5, r.top + 20 };
                const char *nm[3] = { "island_center", "transparent_corner",
                                      "outside_window" };
                for (int i = 0; i < 3; i++) {
                    LRESULT v = SendMessageW(a->hwnd, WM_NCHITTEST, 0,
                                             MAKELPARAM(px[i], py[i]));
                    fprintf(f, "hit %s = %ld (%s)\n", nm[i], (long)v,
                            v == HTCLIENT ? "CLIENT" : "TRANSPARENT");
                }
            }
            fprintf(f, "fa_selfcheck=%d volume=%d wmi=%d ddci=%d rawinput=%d\n",
                    font_fa_ok(&a->fonts), a->sys.volOK, a->sys.wmiOK,
                    a->sys.ddcOK, a->rawOK);
            fclose(f);
        }
    }
    if (a->wheelTest) {
        FILE *f = fopen("wheel.txt", "w");
        if (f) {
            if (a->wheelLogN < 12) a->wheelLog[a->wheelLogN++] = a->ui.view;
            fprintf(f, "raw_input_registered=%d\n", a->rawOK);
            fprintf(f, "wheel_events_sent=%d\n", a->wheelSent);
            fprintf(f, "steps=left_strip 1-4 / bottom_3_DOTS 5-6 / left_strip 7-9; "
                       "ONE horizontal group (3 dots), 5 cards total (0..4): "
                       "横1 perf / 横2 BIG / 横3 HUGE = LAST dot\n");
            fprintf(f, "view_before=%d\nview_after=%d\nview_sequence=",
                    a->wheelBefore, a->ui.view);
            for (int i = 0; i < a->wheelLogN; i++)
                fprintf(f, "%s%d", i ? "->" : "", a->wheelLog[i]);
            fprintf(f, "\nrender_count=%lu\n", (unsigned long)a->renderCount);
            fclose(f);
        }
    }
}
