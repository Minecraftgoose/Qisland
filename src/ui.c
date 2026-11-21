/* ui.c — 界面文字只出现在 AGENTS.md 封闭清单登记的位置
 *
 * 计时器页：「倒计时」标题 +「滚轮调整时间」一行；
 * 便签页：「便签」；剪贴板历史页：「剪贴板」；
 * 性能监测页：标签与硬件信息；剪贴板条目 / 通知文字是数据本身。
 * 以上字符串均由用户逐字点名并登记进封闭清单，不新增任何说明、
 * placeholder 或按钮文字。
 *
 * 布局按 3.html 的示意：
 *   最左竖条 = 滚轮热区；左侧 2x2 四个图标 =
 *     隐藏 / 亮度 / 音量 / taskmgr（右下，即红箭头指的位置）；
 *   右侧 =「倒计时」标题 +「滚轮调整时间」+ HH:MM:SS 读数 + 启动 / 重置。
 *   胶囊态（收起）左侧是时钟，倒计时进行时右侧同时显示剩余时间。
 * 冒号用两个圆点画（符号，不是文字）；悬浮在 HH/MM/SS 某一段上时
 * 该段淡底高亮，滚轮按段调整时间。
 * 按钮下方不再画任何滑动条；音量和亮度按钮本身仍可点击或左右拖动调节。
 * 第二张中卡片（idx 1）是日期时钟：带秒的时钟 + 一整组历法日期，
 * 只读展示、没有按钮；第三张中卡片（idx 2）是便签：右侧整块变成文本区，没有输入框边框、
 * 没有提示文字、没有系统滚动条；内容溢出时右缘出现极淡的圆头指示条。
 * 所有控件用 SDF 覆盖率叠加，天然被圆角裁切；图标用 font 模块的
 * 4x4 面积覆盖率超采样，保证无锯齿。
 */
#include "ui.h"
#include "app.h"
#include "config.h"
#include "icons.h"
#include "canvas.h"
#include "font.h"
#include "sys.h"
#include "anim.h"
#include "countdown.h"
#include "note.h"
#include "perf.h"
#include "date.h"
#include "cards.h"
#include "notify.h"
#include "tooltip.h"
#include "settings.h"
#include "about.h"
#include "capsuleicon.h"
#include "input.h"
#include "update.h"

typedef struct { double x, y, w, h; } DBox;

typedef struct {
    double W, H, L;
    DBox wheel, hide, bri, vol, task, play, reset;
    DBox num[3], colon[2];
    DBox title, hint, pageTitle, sVol, sBri, note, hdots;
    DBox clip, clipClear;                 /* 剪贴板历史内容区 / 清空图标 */
    double dotsPitch;                     /* 横点间距（= 竖点间距，带密集上限） */
    DBox notify, notifyClose;             /* 即时通知卡片本体 / 右上角关闭 */
    SetBoxes set;                         /* 设置页：绘制与命中同源的稳定几何 */
    int iconPx, numPx, titlePx;
} Boxes;

/* 按卡片表的渲染器分派，不再靠 view 编号区间硬判。
   新增的留空卡（DRAW_EMPTY）什么都不画，也不注册可点控件。 */
static int timer_view(int v) { return cards_draw(v) == DRAW_TIMER; }
static int note_view(int v)  { return cards_draw(v) == DRAW_NOTE;  }
static int perf_view(int v)  { return cards_draw(v) == DRAW_PERF;  }
static int empty_view(int v) { return cards_draw(v) == DRAW_EMPTY; }
static int clip_view(int v)  { return cards_draw(v) == DRAW_CLIP; }
static int date_view(int v)  { return cards_draw(v) == DRAW_DATE; }
static int settings_view(int v) { return cards_draw(v) == DRAW_SETTINGS; }

/* 横排圆点从最后一个竖点起接管：**只有一条横向组 GRP_H**，共 3 个点
   （横1 性能 / 横2 大卡片 / 横3 超大卡片 = 最后一点）。
   大卡片与超大卡片都在这一排里，不拆第二个组。
   进入横向阶段后竖列圆点整列不画。
   竖点现在 5 个（加卡即加点）：GRP_H slot0（性能卡）兼任竖④，vdot=3，
   仍留在竖向阶段显示竖列点；slot1 / slot2 同为 vdot4，共用竖⑤。 */
static int hdots_on(int v)
{
    return cards_is_hgroup(cards_group(v)) &&
           cards_vdot(v) == CARDS_VDOTS - 1;
}

/* 竖点间距：wheel 区高 = 0.80H，4 个点跨 0.55 → pitch = 0.80H*0.55/3。
   横点直接用这个 pitch，超大卡片上钳到上限保持「密集」。 */
double ui_dots_pitch(double cardH, double u)
{
    double p = cardH * 0.80 * (HDOTS_P1 - HDOTS_P0) /
               (double)(CARDS_VDOTS - 1);
    double cap = HDOTS_PITCH_MAX * (u > 0.0 ? u : 1.0);
    return p < cap ? p : cap;
}

void ui_hdots_center(double L, double W, double H, double pitch,
                     int n, int i, double *cx, double *cy)
{
    double mid = L + W * 0.5;
    /* n 个点以 mid 为对称中心：i=0 → -(n-1)/2*pitch，i=n-1 → +(n-1)/2*pitch */
    *cx = mid + ((double)i - (double)(n - 1) * 0.5) * pitch;
    *cy = H * HDOTS_Y;
}

/* 第 i 个横向圆点的中心：与绘制 / 命中 / 测试同源 */
static void hdots_center(const Boxes *b, int n, int i, double *cx, double *cy)
{
    ui_hdots_center(b->L, b->W, b->H, b->dotsPitch, n, i, cx, cy);
}

/* 点 x 最接近第几个圆点（点击跳卡用，和绘制同源） */
static int hdots_slot(const Boxes *b, int n, int x)
{
    int i, best = 0;
    double bd = 1e18;
    for (i = 0; i < n; i++) {
        double cx, cy, d;
        hdots_center(b, n, i, &cx, &cy);
        d = fabs((double)x - cx);
        if (d < bd) { bd = d; best = i; }
    }
    return best;
}

/* settings.c 的几何用 SBox（与 DBox 同为 {x,y,w,h} 四个 double），
   这里显式转换，避免两个结构类型直接混用。 */
static DBox s2d(SBox s)
{
    DBox d;
    d.x = s.x; d.y = s.y; d.w = s.w; d.h = s.h;
    return d;
}

static RECT to_rect(DBox b)
{
    RECT r;
    r.left = (int)floor(b.x); r.top = (int)floor(b.y);
    r.right = (int)ceil(b.x + b.w); r.bottom = (int)ceil(b.y + b.h);
    return r;
}

static void compute(App *a, double W, double H, Boxes *b)
{
    double L = ((double)a->canvas.W - W) * 0.5;
    double gx, cw, ch, gy, gapx, gapy, tx, tw, gap, nw;
    /* 文字排版一律用「目标态标称尺寸」，不用动画中间态 W/H。
       anim_geometry() 在展开弹簧(8.25% 过冲)和切档插值(SIZE_TAU)期间
       每帧都变；若拿它算字号，px 会跨整数反复跳 → 字形重新光栅化、
       字宽非线性变化、截断签名失效 → 每帧重排文字，看起来就是「抖」。
       W0/H0 在同一张卡内恒定，动画期间文字只整体平移 + 淡入。 */
    double tw0, th0, tr0, W0, H0, L0;

    anim_view_size(&a->anim, a->ui.view, &tw0, &th0, &tr0);
    W0 = tw0 * a->u; H0 = th0 * a->u;
    L0 = ((double)a->canvas.W - W0) * 0.5;

    ZeroMemory(b, sizeof(*b));
    b->W = W; b->H = H; b->L = L;

    /* 最左竖条：滚轮切换热区 */
    b->wheel = (DBox){ L + 0.016 * W, H * 0.10, 0.078 * W, H * 0.80 };

    /* 2x2 图标按钮：与 3.html 的四格一一对应
       左上隐藏 / 右上亮度 / 左下音量 / 右下 taskmgr（红箭头所指） */
    gx = L + 0.115 * W; cw = 0.125 * W; ch = H * 0.31;
    gy = H * 0.13; gapx = 0.018 * W; gapy = H * 0.07;
    b->hide = (DBox){ gx,             gy,             cw, ch };
    b->bri  = (DBox){ gx + cw + gapx, gy,             cw, ch };
    b->vol  = (DBox){ gx,             gy + ch + gapy, cw, ch };
    b->task = (DBox){ gx + cw + gapx, gy + ch + gapy, cw, ch };

    /* 标题 / 提示 / 数字 / 启动 / 重置
       标题与提示都与读数区同宽、同中心，字号也相同（放大后的 titlePx）。
       三行依次落在 0.07H / 0.29H / 0.46H，互不重叠。 */
    tx = L + 0.405 * W; tw = 0.360 * W;
    b->title = (DBox){ tx, H * 0.07, tw, H * 0.19 };
    b->hint  = (DBox){ tx, H * 0.29, tw, H * 0.15 };
    gap = COLON_GAP * W;                    /* 冒号就落在两个间隙里 */
    nw = (tw - 2.0 * gap) / 3.0;
    for (int i = 0; i < 3; i++) {
        b->num[i] = (DBox){ tx + (nw + gap) * (double)i, H * 0.46,
                            nw, H * 0.36 };
        if (i < 2)
            b->colon[i] = (DBox){ tx + (nw + gap) * (double)i + nw,
                                  H * 0.46, gap, H * 0.36 };
    }

    /* 模式 1：原计时器区换成上下两条滑块（音量 / 亮度），
       位置与三块读数占的区域一致，不挤到 2x2 四格 */
    {
        double sy = H * 0.40, sh = H * 0.22;
        b->sVol = (DBox){ tx, sy,             tw, sh };
        b->sBri = (DBox){ tx, sy + H * 0.24,  tw, sh };
    }

    /* 便签 / 剪贴板页标题条：与「倒计时」同字号、同全局字体、同一视觉层级。
       内容区整体下移，终点不变（便签 0.94H、剪贴板 0.88H），
       字号比例在 config.h 里同步补偿，保证正文一个像素都不缩。 */
    /* 以下四个盒子 + 三个字号全部用 W0/H0：它们是文字排版基准。
       绘制时仍会随主形状长大而整体平移/淡入，但内部不重排。
       用户要求「剪贴板和便签要居中，相对于卡片居中」：三个盒子一律
       以卡片水平中心对称摆放（x = L0 + (W0 - w)/2），标题绘制也正好
       落在卡片正中。note.c / clip.c 里的文字行再按同一中心逐行居中。 */
    b->pageTitle = (DBox){ L0 + 0.075 * W0, H0 * 0.02, 0.850 * W0, H0 * 0.15 };

    /* 便签视图：第一页的 2x2 四按钮不出现，整块卡片（除最左滚轮竖条）
       全是文本区。不画输入框边框、不画纸块，文字直接落在卡片本体上。 */
    b->note = (DBox){ L0 + 0.075 * W0, H0 * 0.19, 0.850 * W0, H0 * 0.75 };

    /* 剪贴板历史：内容区避开右侧清空图标与底部横排圆点（HDOTS_Y≈0.93H）。
       条目文字直接落在卡片本体上。 */
    b->clip      = (DBox){ L0 + 0.105 * W0, H0 * 0.19, 0.790 * W0, H0 * 0.69 };
    b->clipClear = (DBox){ L0 + 0.905 * W0, H0 * 0.19, 0.055 * W0, H0 * 0.10 };

    b->play  = (DBox){ L + 0.865 * W, H * 0.46, 0.052 * W, H * 0.36 };
    b->reset = (DBox){ L + 0.925 * W, H * 0.46, 0.052 * W, H * 0.36 };

    /* 底部横向圆点命中区：以卡片水平中心为对称中心、按竖点间距铺开，
       左右各扩一点便于点/滚。圆点本身在 draw_hdots() 里按同一个
       ui_hdots_center() 逐个画，不画任何轨道。 */
    {
        int n = 0;
        double span, half;
        if (hdots_on(a->ui.view))
            n = cards_group_count(cards_group(a->ui.view));
        b->dotsPitch = ui_dots_pitch(H, a->u);
        if (n < 1) n = 1;
        span = (double)(n - 1) * b->dotsPitch;
        half = HDOTS_R_ON + HDOTS_HIT_PAD_X;
        b->hdots = (DBox){ L + W * 0.5 - span * 0.5 - half,
                           H * HDOTS_Y - HDOTS_HIT_PAD,
                           span + 2.0 * half, HDOTS_HIT_PAD * 2.0 };
    }

    /* 即时通知：与中卡片同尺寸；左侧图标 + 右侧程序名/详细信息。
       整卡可点关闭，右上角 xmark 单独命中（先注册，优先命中）。 */
    /* 通知卡片同样用稳定尺寸：绘制(notify_draw)与命中区必须同源，
       否则展开过冲期间关闭热区会和图标错位。 */
    b->notify      = (DBox){ L0 + 0.01 * W0, H0 * 0.06, 0.98 * W0, H0 * 0.88 };
    b->notifyClose = (DBox){ L0 + 0.905 * W0, H0 * 0.08, 0.080 * W0, H0 * 0.28 };

    /* 设置页：与 date/clip/note 一样用目标态标称尺寸，动画期间不重排。 */
    settings_boxes(W0, H0, L0, &b->set);

    b->iconPx  = qs_iclamp((int)(H0 * 0.17), 16, 52);
    b->numPx   = qs_iclamp((int)(H0 * 0.28), 22, 64);
    b->titlePx = qs_iclamp((int)(H0 * TITLE_PX_RATIO), TITLE_PX_MIN, TITLE_PX_MAX);
}

/* ---- 底部横向指示圆点 ----
   进入第 4 个竖点后，竖列圆点整列消失，改在底部画一排圆点。
   和竖着的那列完全同款：同样的 canvas_disc、同样的半径
   (HDOTS_R_ON / HDOTS_R_OFF)、同样的 ACCENT/DIM 配色；
   间距直接取竖点的 pitch，但**以卡片水平中心对称、密集排开**
   （不再按 0.30W..0.85W 铺满整张卡）。
   只有一个横向组，点数 = cards_group_count() = 3：
   横1 性能 / 横2 大卡片 / 横3 超大卡片（最后一点）。
   停在大卡片时第 2 点亮 =「竖向第四个 = 横向第二个」；
   停在超大卡片时第 3 点亮 = 横向最后一点。
   没有轨道、没有长条、没有文字、没有箭头，边缘连续灰阶无锯齿。 */
static void draw_hdots(App *a, const Boxes *b, double k)
{
    Canvas *c = &a->canvas;
    CardGroup g = cards_group(a->ui.view);
    int n = cards_group_count(g);
    int cur = cards_slot(a->ui.view);
    int i;

    if (n <= 0) return;
    for (i = 0; i < n; i++) {
        double cx, cy;
        int on = (i == cur);
        hdots_center(b, n, i, &cx, &cy);
        canvas_disc(c, cx, cy, on ? HDOTS_R_ON : HDOTS_R_OFF,
                    on ? ACCENT_R : DIM_R,
                    on ? ACCENT_G : DIM_G,
                    on ? ACCENT_B : DIM_B,
                    on ? k : k * 0.65);
    }
}

/* ---- 图标：FA 字形，缺失时降级为几何形状（同样用 SDF，无锯齿） ---- */
static void icon(App *a, unsigned cp, int px, DBox box,
                 int cr, int cg, int cb, double k)
{
    double cx = box.x + box.w * 0.5;
    double cy = box.y + box.h * 0.5;
    if (font_fa_ok(&a->fonts)) {
        font_draw_icon(&a->fonts, &a->canvas, cp, px, cx, cy,
                       1, cr, cg, cb, k);
        return;
    }
    /* 降级 */
    {
        double r = px * 0.28;
        Canvas *c = &a->canvas;
        switch (cp) {
        case FA_PLAY:
            canvas_line(c, cx - r, cy - r, cx - r, cy + r, 2, cr, cg, cb, k);
            canvas_line(c, cx - r, cy - r, cx + r, cy, 2, cr, cg, cb, k);
            canvas_line(c, cx + r, cy, cx - r, cy + r, 2, cr, cg, cb, k);
            break;
        case FA_PAUSE:
            canvas_line(c, cx - r * 0.6, cy - r, cx - r * 0.6, cy + r, 3, cr, cg, cb, k);
            canvas_line(c, cx + r * 0.6, cy - r, cx + r * 0.6, cy + r, 3, cr, cg, cb, k);
            break;
        default:
            canvas_disc(c, cx, cy, r, cr, cg, cb, k * 0.55);
            canvas_frame(c, cx - r, cy - r, r * 2.0, r * 2.0, r * 0.5,
                         1.6, cr, cg, cb, k);
            break;
        }
    }
}

/* 滑块轨道几何：绘制与命中判定共用同一套公式，避免「看着在 30% 实际是 45%」 */
static void slider_geom(DBox b, double *iconW, double *x0, double *w)
{
    double iw = b.h * 0.85;
    double gap = 8.0;
    double ww;
    if (iw < 12.0) iw = 12.0;
    ww = b.w - iw - gap - 6.0;
    if (ww < 12.0) ww = 12.0;
    *iconW = iw;
    *x0 = b.x + iw + gap;
    *w = ww;
}

/* 计时器区的大滑块：左侧图标 + 圆角槽 + 填充 + 圆形把手。
   只在计时器区域出现，与「按钮下方细滑轨」不是一回事（后者已删除）。 */
static void slider(App *a, DBox b, unsigned cp, double val,
                   int cr, int cg, int cb, double k)
{
    Canvas *c = &a->canvas;
    double iconW, x0, w;
    double th = 9.0;
    double y, cy, v;
    DBox ib;

    slider_geom(b, &iconW, &x0, &w);
    y = b.y + (b.h - th) * 0.5;
    cy = b.y + b.h * 0.5;
    v = qs_clamp(val, 0.0, 1.0);
    ib = (DBox){ b.x, b.y, iconW, b.h };

    icon(a, cp, qs_iclamp((int)(b.h * 0.62), 12, 34), ib,
         cr, cg, cb, k);
    canvas_round(c, x0, y, w, th, th * 0.5,
                 TRACK_R, TRACK_G, TRACK_B, k);
    if (v > 0.0)
        canvas_round(c, x0, y, w * v, th, th * 0.5,
                     cr, cg, cb, k * 0.92);
    canvas_disc(c, x0 + w * v, cy, b.h * 0.24,
                INK_R, INK_G, INK_B, k);
}

static void add_hit(Ui *u, int id, DBox b)
{
    if (u->hitN >= (int)(sizeof(u->hits) / sizeof(u->hits[0]))) return;
    u->hits[u->hitN].rc = to_rect(b);
    u->hits[u->hitN].id = id;
    u->hitN++;
}

void ui_init(Ui *u)
{
    ZeroMemory(u, sizeof(*u));
    u->view = DEFAULT_VIEW;
    u->mode = 0;              /* 每次启动回到计时器 */
    u->hoverSeg = -1;
}

void ui_set_view(App *a, int v)
{
    v = qs_iclamp(v, 0, cards_count() - 1);
    if (v != a->ui.view) {
        if (a->note.editing) input_end(a);   /* 切档前先保存便签 */
        tooltip_hide(a);                     /* 气泡别指着上一张卡的按钮 */
        a->ui.view = v; a->dirty = 1;
        /* 「岛回到第一个卡片时收起」：切回 idx0 立刻回到胶囊态。
           hover 保持 1（鼠标多半还在岛上），避免下一帧 over!=hover
           又把刚收起的卡片展开；等真正移开再移入才会重新展开。 */
        if (v == 0 && settings_collapse_on_first(&a->settings)) {
            a->anim.expanded = 0;
            a->hover = 1;
        }
    }
}

void ui_note_box(const App *a, NoteBox *box)
{
    double w, h, r;
    Boxes b;
    anim_geometry(&a->anim, &w, &h, &r);
    compute((App *)a, w * a->u, h * a->u, &b);
    box->x = b.note.x; box->y = b.note.y;
    box->w = b.note.w; box->h = b.note.h;
}

void ui_clip_box(const App *a, ClipBox *box)
{
    double w, h, r;
    Boxes b;
    anim_geometry(&a->anim, &w, &h, &r);
    compute((App *)a, w * a->u, h * a->u, &b);
    box->x = b.clip.x; box->y = b.clip.y;
    box->w = b.clip.w; box->h = b.clip.h;
}

void ui_clip_clear_box(const App *a, ClipBox *box)
{
    double w, h, r;
    Boxes b;
    anim_geometry(&a->anim, &w, &h, &r);
    compute((App *)a, w * a->u, h * a->u, &b);
    box->x = b.clipClear.x; box->y = b.clipClear.y;
    box->w = b.clipClear.w; box->h = b.clipClear.h;
}

void ui_notify_boxes(const App *a, DBoxNotify *body, DBoxNotify *close)
{
    double w, h, r;
    Boxes b;
    anim_geometry(&a->anim, &w, &h, &r);
    compute((App *)a, w * a->u, h * a->u, &b);
    body->x = b.notify.x; body->y = b.notify.y;
    body->w = b.notify.w; body->h = b.notify.h;
    close->x = b.notifyClose.x; close->y = b.notifyClose.y;
    close->w = b.notifyClose.w; close->h = b.notifyClose.h;
}

void ui_layout(App *a, double W, double H)
{
    Boxes b;
    Ui *u = &a->ui;
    compute(a, W, H, &b);
    u->hitN = 0;
    SetRect(&u->wheelRect, 0, 0, 0, 0);
    SetRect(&u->hdotsRect, 0, 0, 0, 0);

    /* 即时通知：不注册任何导航热区（竖条 / 横点 / 卡片控件全部不响应），
       只有「点卡片关闭」与「右上角 xmark 关闭」。通知不占竖点/横点。 */
    if (notify_active(&a->notify)) {
        add_hit(u, H_NOTIFY_CLOSE, b.notifyClose);   /* 先注册，优先命中 */
        add_hit(u, H_NOTIFY, b.notify);
        return;
    }

    /* 导航热区与指示器严格互斥（本轮用户要求）：
       显示横排圆点 = 已进入横向组 → **左侧竖条热区整个不注册**，
       wheelRect 保持 (0,0,0,0)，滚轮、点击、悬停提示都不在那儿响应；
       竖向阶段反过来，只注册竖条、不注册横点。
       以前这里「竖条不画点但仍接受滚轮」是为了连滚不断点，现已取消。 */
    if (hdots_on(a->ui.view)) {
        /* 横向组：底部一排圆点的命中区（滚轮 / 点击） */
        add_hit(u, H_HDOTS, b.hdots);
        u->hdotsRect = u->hits[u->hitN - 1].rc;
    } else {
        add_hit(u, H_WHEEL, b.wheel);
        u->wheelRect = u->hits[u->hitN - 1].rc;   /* 卡片最左侧 */
    }

    if (note_view(a->ui.view)) {
        /* 便签页只有文本区：第一页的 2x2 四按钮既看不见也不响应点击 */
        add_hit(u, H_NOTE, b.note);
    } else if (clip_view(a->ui.view)) {
        /* 剪贴板历史：列表 + 右上角清空图标，其余控件一个都不注册。
           空态连清空图标也不画/不响应，避免两个图标一起出现显得突兀。 */
        add_hit(u, H_CLIP, b.clip);
        if (clip_count(&a->clip) > 0) add_hit(u, H_CLIP_CLEAR, b.clipClear);
    } else if (settings_view(a->ui.view)) {
        /* 设置页：左栏三个页签「通用 / 更新 / 关于」；「通用」右栏四个单选
           收起模式 + 自启动；「更新」右栏注册检查更新按钮，结果区滚轮滚动由
           wheel_core 用 b.set.updateBody 判定；「关于」右栏只注册仓库链接。
           几何直接取 settings_boxes() 的数组，绘制与命中严格同源。 */
        static const int setIds[SET_SW_N] = {
            H_SET_LEAVE, H_SET_OUTSIDE, H_SET_FIRST,
            H_SET_NEVER, H_SET_AUTOSTART
        };
        int i;
        int tab = settings_tab(&a->settings);
        add_hit(u, H_SET_TAB_GEN,    s2d(b.set.tabGeneral));
        add_hit(u, H_SET_TAB_UPDATE, s2d(b.set.tabUpdate));
        add_hit(u, H_SET_TAB_ABOUT,  s2d(b.set.tabAbout));
        if (tab == SET_TAB_GENERAL) {
            for (i = 0; i < SET_SW_N; i++)
                add_hit(u, setIds[i], s2d(b.set.rows[i]));
        } else if (tab == SET_TAB_UPDATE) {
            add_hit(u, H_SET_UPDATE_BTN, s2d(b.set.updateBtn));
        } else {
            add_hit(u, H_ABOUT_REPO, s2d(b.set.aboutRepo));
        }
    } else if (perf_view(a->ui.view) || empty_view(a->ui.view) ||
               date_view(a->ui.view)) {
        /* 日期时钟页 / 性能监测页 / 留空卡都是只读展示：不注册任何可点控件，
           只保留最左竖条的滚轮切档热区（上面已 add_hit）。 */
    } else {
        add_hit(u, H_HIDE,    b.hide);
        add_hit(u, H_BRI,     b.bri);
        add_hit(u, H_VOL,     b.vol);
        add_hit(u, H_TASKMGR, b.task);
        /* 启动 / 重置只属于倒计时读数：切到音量亮度双滑块后
           既不画也不响应点击（2x2 四按钮与两条滑块保留）。 */
        if (u->mode == 0) {
            add_hit(u, H_PLAY,  b.play);
            add_hit(u, H_RESET, b.reset);
        }
        if (u->mode == 1) {
            add_hit(u, H_SLIDER_VOL, b.sVol);
            add_hit(u, H_SLIDER_BRI, b.sBri);
        } else {
            add_hit(u, H_NUM_H, b.num[0]);
            add_hit(u, H_NUM_M, b.num[1]);
            add_hit(u, H_NUM_S, b.num[2]);
        }
    }
}

void ui_draw(App *a, double W, double H, double k)
{
    Boxes b;
    Canvas *c = &a->canvas;
    Ui *u = &a->ui;
    Sys *s = &a->sys;
    Countdown *cd = &a->cd;
    /* 自绘整页的模块用稳定尺寸，避免其内部字号随动画 H 每帧重算。
       通知态 overrideMid 会让 anim_view_size 直接返回中卡尺寸。 */
    double tw0, th0, tr0, W0, H0;
    anim_view_size(&a->anim, a->ui.view, &tw0, &th0, &tr0);
    W0 = tw0 * a->u; H0 = th0 * a->u;
    compute(a, W, H, &b);

    /* 即时通知：整张卡片换成通知内容，不画竖点/横点、不画任何导航指示。
       文字政策：程序名 + 详细信息是通知内容本身，与性能页同属放开范围。 */
    if (notify_active(&a->notify)) {
        notify_draw(&a->notify, a, W0, H0, k);
        return;
    }

    /* 竖列 4 个圆点：只在竖向阶段（vdot 0..2）显示。
       一旦进入横向组，整列点不画 —— 改由底部横排圆点接管。
       没有 grip 图标、没有连接线；半径 / 配色 / 分布与横排完全同款。 */
    if (!hdots_on(u->view)) {
        double cx = b.wheel.x + b.wheel.w * 0.5;
        int cur = cards_vdot(u->view);
        for (int i = 0; i < CARDS_VDOTS; i++) {
            double y = b.wheel.y + b.wheel.h *
                       (HDOTS_P0 + (HDOTS_P1 - HDOTS_P0) *
                        (double)i / (double)(CARDS_VDOTS - 1));
            int on = (i == cur);
            canvas_disc(c, cx, y, on ? HDOTS_R_ON : HDOTS_R_OFF,
                        on ? ACCENT_R : DIM_R,
                        on ? ACCENT_G : DIM_G,
                        on ? ACCENT_B : DIM_B, on ? k : k * 0.65);
        }
    }

    /* 底部横排圆点：放在所有 early return 之前，留空卡 / 超大卡也照样有。
       与竖列互斥，任一时刻只会出现其中一种。 */
    if (hdots_on(a->ui.view)) draw_hdots(a, &b, k);

    /* 用户逐字点名的两个页面标题，登记在 AGENTS.md 封闭清单第 6/7 条：
       「便签」用于便签页，「剪贴板」用于剪贴板历史页。
       0.2 轮用户更正用词：「剪切板」→「剪贴板」，全包统一，
       之前混用的「粘贴板」也一并统一；这只是更正字符串，不是新增加字。
       与「倒计时」同字号、同全局字体、同淡灰，落在内容区上方的空条里；
       其余页面（计时器 / 性能 / 留空卡 / 通知）一个字都不加。 */
    if (note_view(a->ui.view) || clip_view(a->ui.view)) {
        const WCHAR *t = note_view(a->ui.view) ? L"便签" : L"剪贴板";
        font_draw_label(&a->fonts, c, t, b.titlePx,
                        b.pageTitle.x + b.pageTitle.w * 0.5,
                        b.pageTitle.y + b.pageTitle.h * 0.5, 1,
                        DIM_R, DIM_G, DIM_B, k);
    }

    /* 第二张中卡片：带秒时钟 + 农历 / 干支 / 节气 / 周数等日期信息。
       W0/H0 是目标态标称尺寸，文字不随展开弹簧重排（AGENTS.md 第 4 节第 10 条）。 */
    if (date_view(a->ui.view)) {
        date_draw(&a->fonts, c, W0, H0, k);
        return;
    }

    /* 第四张中卡片：性能监测。圆环 + 数字 + 硬件信息。 */
    if (perf_view(a->ui.view)) {
        perf_draw(&a->perf, a, W0, H0, k);
        return;
    }

    /* 第一个大卡片：剪贴板历史。条目文字是数据本身，不是 UI 标签；
       右上角只有一个 trash-can 图标用于清空，没有任何说明文字。 */
    if (clip_view(a->ui.view)) {
        ClipBox cb;
        cb.x = b.clip.x; cb.y = b.clip.y;
        cb.w = b.clip.w; cb.h = b.clip.h;
        clip_draw(&a->clip, a, cb, k);
        if (clip_count(&a->clip) > 0)
            icon(a, FA_TRASH_CAN, qs_iclamp((int)(b.titlePx * 0.55), 14, 26),
                 b.clipClear, DIM_R, DIM_G, DIM_B, k * 0.72);
        return;
    }

    /* 唯一的超大卡片 = 设置页：左右栏「通用 / 关于」。
       横排圆点在上方已先画完，这里只画内容区并 return。 */
    if (settings_view(a->ui.view)) {
        double L0 = ((double)a->canvas.W - W0) * 0.5;
        settings_draw(a, W0, H0, L0, &b.set, k);
        return;
    }

    /* 留空卡：纯卡片形状 + 底部横排圆点，任何内容都不画。
       横点在上方已先画完，这里 return 只裁掉内容区。 */
    if (empty_view(a->ui.view)) return;

    /* 2x2 四个图标属于计时器页，便签页 / 性能页 / 留空卡一个都不画。
       仅图标，无框、无文字、无下方滑动条。 */
    if (timer_view(a->ui.view)) {
        icon(a, FA_HIDE, b.iconPx, b.hide, INK_R, INK_G, INK_B, k);
        icon(a, s->bri < 25 ? FA_BRIGHTNESS_LOW : FA_BRIGHTNESS,
             b.iconPx, b.bri, INK_R, INK_G, INK_B, k);
        icon(a, s->vol < 0.02 ? FA_VOLUME_MUTE :
             (s->vol < 0.35 ? FA_VOLUME_LOW : FA_VOLUME),
             b.iconPx, b.vol, INK_R, INK_G, INK_B, k);
        icon(a, FA_TASKMGR, b.iconPx, b.task, DIM_R, DIM_G, DIM_B, k);
    }

    if (note_view(a->ui.view)) {
        /* 便签：整块只有文字（和极淡的空态图标 / 滚动指示），
           没有读数、没有 play/reset、没有任何边框。 */
        NoteBox nb;
        nb.x = b.note.x; nb.y = b.note.y;
        nb.w = b.note.w; nb.h = b.note.h;
        note_draw(&a->note, a, nb, k);
        return;
    }

    if (u->mode == 0) {
        /* 标题「倒计时」：字号放大。旧版「cd.ended → bell 图标 + 淡圆底 +
           数字变色」的常驻结束提示已整体删除：归零只弹即时通知卡片，
           通知关闭/过期后这里就是完全正常的计时器，不残留提示态。 */
        font_draw_label(&a->fonts, c, L"倒计时", b.titlePx,
                        b.title.x + b.title.w * 0.5,
                        b.title.y + b.title.h * 0.5, 1,
                        DIM_R, DIM_G, DIM_B, k);

        /* 用户本轮逐字点名的一行提示，与标题同字号、同全局字体、同一中心，
           落在标题与读数之间的空行；比标题淡一档，不抢读数。 */
        font_draw_label(&a->fonts, c, L"滚轮调整时间", b.titlePx,
                        b.hint.x + b.hint.w * 0.5,
                        b.hint.y + b.hint.h * 0.5, 1,
                        DIM_R, DIM_G, DIM_B, k * 0.80);

        /* HH:MM:SS 读数（数字 + 两个圆点冒号，都不是标签文字） */
        {
            double rem = countdown_remain(cd);
            int hh = (int)(rem / 3600.0);
            int mm = (int)((rem - hh * 3600.0) / 60.0);
            int ss = (int)(rem - hh * 3600.0 - mm * 60.0);
            WCHAR num[3][8];
            int nr = INK_R, ng = INK_G, nb_ = INK_B;
            double cy = b.num[0].y + b.num[0].h * 0.5;
            _snwprintf(num[0], 8, L"%02d", hh);
            _snwprintf(num[1], 8, L"%02d", mm);
            _snwprintf(num[2], 8, L"%02d", ss);
            for (int i = 0; i < 3; i++) {
                DBox bx = b.num[i];
                /* 悬浮段淡底：告诉用户滚轮会调这一段，纯视觉无文字 */
                if (u->hoverSeg == i)
                    canvas_round(c, bx.x, bx.y + bx.h * 0.08,
                                 bx.w, bx.h * 0.84, 8.0,
                                 HOVER_BG_R, HOVER_BG_G, HOVER_BG_B, k * 0.55);
                font_draw_text(&a->fonts, c, num[i], b.numPx,
                               bx.x + bx.w * 0.5, cy, 1,
                               nr, ng, nb_, k);
            }
            /* 冒号 = 每处上下两个圆点，SDF 绘制所以同样无锯齿 */
            for (int i = 0; i < 2; i++) {
                double ccx = b.colon[i].x + b.colon[i].w * 0.5;
                double rad = qs_clamp(b.numPx * 0.055, 1.6, 4.0);
                double dy  = b.numPx * 0.13;
                canvas_disc(c, ccx, cy - dy, rad, nr, ng, nb_, k * 0.9);
                canvas_disc(c, ccx, cy + dy, rad, nr, ng, nb_, k * 0.9);
            }
        }
    } else {
        /* 用户逐字点名：把 sliders 设置图标换成文字「亮度、音量调节」。
           登记在 AGENTS.md 封闭清单第 11 条（逐字点名，不是放开政策）。
           字号按标题框宽自适应收一档，保证整串不越界、不压到两条滑块。 */
        {
            const WCHAR *t = L"亮度、音量调节";
            int px = b.titlePx;
            double cx = b.title.x + b.title.w * 0.5;
            double cy = b.title.y + b.title.h * 0.5;
            while (px > 12 && font_measure(&a->fonts, t, (int)wcslen(t),
                                           px, 2) > b.title.w * 0.96)
                px--;
            font_draw_label(&a->fonts, c, t, px, cx, cy, 1,
                            DIM_R, DIM_G, DIM_B, k);
        }
        slider(a, b.sVol,
               s->vol < 0.02 ? FA_VOLUME_MUTE :
               (s->vol < 0.35 ? FA_VOLUME_LOW : FA_VOLUME),
               s->vol, ACCENT_R, ACCENT_G, ACCENT_B, k);
        slider(a, b.sBri,
               s->bri < 25 ? FA_BRIGHTNESS_LOW : FA_BRIGHTNESS,
               s->bri / 100.0, INK_R, INK_G, INK_B, k);
    }

    /* 启动 / 重置图标：只属于倒计时读数，滑块模式不绘制也不注册命中。 */
    if (u->mode == 0) {
        icon(a, cd->running ? FA_PAUSE : FA_PLAY,
             qs_iclamp((int)(b.iconPx * 0.9), 14, 44), b.play,
             cd->running ? ACCENT_R : INK_R,
             cd->running ? ACCENT_G : INK_G,
             cd->running ? ACCENT_B : INK_B, k);
        icon(a, FA_RESET, qs_iclamp((int)(b.iconPx * 0.9), 14, 44), b.reset,
             DIM_R, DIM_G, DIM_B, k);
    }
}

int ui_hit(const App *a, int x, int y)
{
    for (int i = 0; i < a->ui.hitN; i++) {
        const RECT *r = &a->ui.hits[i].rc;
        if (x >= r->left && x < r->right && y >= r->top && y < r->bottom)
            return a->ui.hits[i].id;
    }
    return H_NONE;
}

RECT ui_hit_rect(const App *a, int id)
{
    RECT e = {0, 0, 0, 0};
    for (int i = 0; i < a->ui.hitN; i++)
        if (a->ui.hits[i].id == id) return a->ui.hits[i].rc;
    return e;
}

static void set_from_x(App *a, int id, int x)
{
    double v;
    int isVol = (id == H_VOL || id == H_SLIDER_VOL);

    if (id == H_SLIDER_VOL || id == H_SLIDER_BRI) {
        /* 用轨道真实范围映射，不能用整个命中矩形（左侧有图标占位） */
        Boxes b;
        double w, h, r, iconW, x0, tw;
        DBox s;
        anim_geometry(&a->anim, &w, &h, &r);
        compute(a, w * a->u, h * a->u, &b);
        s = isVol ? b.sVol : b.sBri;
        slider_geom(s, &iconW, &x0, &tw);
        v = qs_clamp((double)(x - x0) / tw, 0.0, 1.0);
    } else {
        RECT rc = ui_hit_rect(a, id);
        if (rc.right <= rc.left) return;
        v = qs_clamp((double)(x - rc.left) / (double)(rc.right - rc.left),
                     0.0, 1.0);
    }

    if (isVol) {
        if (fabs(v - a->sys.vol) > 0.004) {
            a->sys.vol = v; a->dirty = 1;
            sys_apply_volume(&a->sys, 0);
        }
    } else {
        if (fabs(v * 100.0 - a->sys.bri) > 0.4) {
            a->sys.bri = v * 100.0; a->dirty = 1;
            sys_apply_brightness(&a->sys, 0, a->hwnd);
        }
    }
}

void ui_activate(App *a, int id, int x, int y)
{
    if (id != H_NONE) ui_clear_end(a);   /* 任何交互都确认掉结束提示 */
    switch (id) {
    case H_NOTE: {
        NoteBox nb;
        ui_note_box(a, &nb);
        note_click(&a->note, a, nb, x, y);
        input_begin(a);                  /* 临时取得焦点，开始接收键盘 / IME */
        break;
    }
    case H_CLIP: {
        ClipBox cb;
        ui_clip_box(a, &cb);
        clip_click(&a->clip, a, cb, x, y);   /* 点某条 = 写回系统剪贴板 */
        break;
    }
    case H_CLIP_CLEAR:
        clip_clear(&a->clip, a);             /* 只清历史，不动系统剪贴板 */
        break;
    case H_NOTIFY:
    case H_NOTIFY_CLOSE:
        notify_dismiss(&a->notify, a);       /* 点一下即关闭，回到原视图 */
        break;
    case H_SET_TAB_GEN: case H_SET_TAB_UPDATE: case H_SET_TAB_ABOUT:
    case H_SET_UPDATE_BTN:
    case H_SET_LEAVE: case H_SET_OUTSIDE:
    case H_SET_FIRST: case H_SET_NEVER: case H_SET_AUTOSTART:
        settings_click(a, id, x, y);
        break;
    case H_HIDE:
        a->visible = 0; a->hover = 0; a->anim.expanded = 0; a->dirty = 1;
        ShowWindow(a->hwnd, SW_HIDE);
        break;
    case H_TASKMGR:
        sys_open_taskmgr();
        a->dirty = 1;
        break;
    case H_ABOUT_REPO:
        /* 关于页仓库链接：交给系统默认浏览器，不在岛内嵌任何网页 */
        sys_open_repo();
        a->dirty = 1;
        break;
    case H_PLAY:
        countdown_toggle(&a->cd); a->dirty = 1;
        break;
    case H_RESET:
        countdown_reset(&a->cd); a->dirty = 1;
        break;
    case H_VOL: case H_BRI:
        /* 单击亮度或音量 → 计时器变双滑块（原先右下切换按钮的功能），
           再点一次收回到计时器。 */
        a->ui.mode = a->ui.mode ? 0 : 1;
        {
            double w, h, r;
            anim_geometry(&a->anim, &w, &h, &r);
            ui_layout(a, w * a->u, h * a->u);   /* 立刻刷新命中区，不留一帧空窗 */
        }
        a->dirty = 1;
        break;
    case H_SLIDER_VOL: case H_SLIDER_BRI:
        set_from_x(a, id, x);
        if (id == H_SLIDER_VOL) sys_apply_volume(&a->sys, 1);
        else sys_apply_brightness(&a->sys, 1, a->hwnd);
        break;
    case H_HDOTS: {
        /* 点第几个圆点就跳到该组第几张卡。用 hdots_slot() 找最近的点，
           与 draw_hdots() 用的是同一个 hdots_center()，不会错位。 */
        Boxes b;
        double w, h, r;
        CardGroup g = cards_group(a->ui.view);
        int n = cards_group_count(g), slot, j;
        anim_geometry(&a->anim, &w, &h, &r);
        compute(a, w * a->u, h * a->u, &b);
        if (n <= 0) break;
        slot = hdots_slot(&b, n, x);
        j = cards_group_card(g, slot);
        if (j >= 0) ui_set_view(a, j);
        break;
    }
    default:
        break;
    }
}

void ui_click(App *a, int x, int y)
{
    ui_activate(a, ui_hit(a, x, y), x, y);
}

void ui_drag(App *a, int id, int x, int y)
{
    (void)y;
    if (id == H_VOL || id == H_BRI ||
        id == H_SLIDER_VOL || id == H_SLIDER_BRI)
        set_from_x(a, id, x);
}

void ui_end_drag(App *a, int id)
{
    if (id == H_VOL || id == H_SLIDER_VOL) sys_apply_volume(&a->sys, 1);
    else if (id == H_BRI || id == H_SLIDER_BRI)
        sys_apply_brightness(&a->sys, 1, a->hwnd);
    a->drag = 0;
    a->dirty = 1;
}

void ui_click_id(App *a, int id)
{
    RECT r = ui_hit_rect(a, id);
    if (r.right > r.left)
        ui_click(a, (r.left + r.right) / 2, (r.top + r.bottom) / 2);
}

static int in_rect(const RECT *r, POINT c)
{
    return r && r->right > r->left &&
           c.x >= r->left && c.x < r->right &&
           c.y >= r->top && c.y < r->bottom;
}

/* 光标落在 HH / MM / SS 哪一段；未展开或滑块模式下命中表里没有这些 id */
static int num_segment(const App *a, int x, int y)
{
    int i;
    for (i = 0; i < a->ui.hitN; i++) {
        const HitRect *h = &a->ui.hits[i];
        if (h->id < H_NUM_H || h->id > H_NUM_S) continue;
        if (x >= h->rc.left && x < h->rc.right &&
            y >= h->rc.top && y < h->rc.bottom)
            return h->id - H_NUM_H;
    }
    return -1;
}

void ui_clear_end(App *a)
{
    if (a->cd.ended || a->endNotified) {
        countdown_clear_ended(&a->cd);
        a->endNotified = 0;
        a->dirty = 1;
    }
}

void ui_adjust(App *a, int seg, int step)
{
    ui_clear_end(a);                 /* 用户动了时间，结束提示自然解除 */
    if (countdown_adjust(&a->cd, seg, step)) a->dirty = 1;
}

/* 倒计时自然归零：自动显示 + 强制展开，并保持到用户交互。
   只有图标，没有新增文字。 */
void ui_countdown_ended(App *a)
{
    if (a->endNotified) return;
    a->endNotified = 1;

    /* 倒计时归零唯一的提醒形态：新版即时通知卡片。
       旧版「强制切回计时器页 + 00:00:00 变灰 + bell 常驻提示」已删除；
       通知自己负责显示/展开/强制可见，并在关闭或过期后还原到弹出前的
       视图与模式 —— 不打断用户当时正在看的页面。 */
    notify_show_fa(&a->notify, a, FA_BELL, L"Qisland",
                   L"倒计时已结束 · 00:00:00", NOTIFY_MS);
}

/* 唯一横向组的滚轮：组内优先（横1 性能 ↔ 横2 大卡片 ↔ 横3 超大卡片），
   组内走到头再由 cards_next 推进；当前只有一组，到头就停住，不再跨到别的组。
   pureGroup=1（Shift / 水平滚轮）只在组内横切，不跨组。
   往下滚（delta<0）= 下一张，往上滚 = 上一张。 */
static void hdots_wheel(App *a, int delta, int pureGroup)
{
    int dir = delta < 0 ? 1 : -1;
    int cur = a->ui.view;
    int j = cards_hmove(cur, dir);
    if (j == cur && !pureGroup) j = cards_next(cur, dir);
    ui_set_view(a, j);
}

/* horiz=1 表示水平滚轮（或 Shift+竖向滚轮）：纯组内横向切换。
   horiz=0 是主竖向滚轮：竖向阶段由左侧竖条沿整条序列走；横向阶段
   **竖条热区已不注册**，只有底部横排圆点走 hdots_wheel。
   只有一条横向组，进与退都是同一套。 */
static void wheel_core(App *a, int delta, int horiz)
{
    POINT p, c;
    DWORD n;
    int seg;

    if (delta == 0 || !GetCursorPos(&p)) return;
    /* 通知期间滚轮不切卡片、不滚任何内容，也不拦截输入 */
    if (notify_active(&a->notify)) { a->wheelAccum = 0; return; }
    c = p;
    ScreenToClient(a->hwnd, &c);

    /* ① 卡片最左侧竖条：仅竖向阶段存在。横向组时 wheelRect 是 (0,0,0,0)，
       这里必然不命中，左侧滚轮彻底不切卡片（内容页则继续滚自己的列表）。 */
    if (in_rect(&a->ui.wheelRect, c)) {
        n = qs_now();
        if (n - a->lastWheel < WHEEL_DEBOUNCE) return;
        a->lastWheel = n;
        if (horiz || (GetAsyncKeyState(VK_SHIFT) & 0x8000)) {
            ui_set_view(a, cards_hmove(a->ui.view, delta > 0 ? 1 : -1));
        } else {
            /* 方向修正：往下滚（delta<0）= 下一个卡片。
               0→1→2→3→4 全程线性邻接，能滚到性能页、大卡片与超大卡片。 */
            ui_set_view(a, cards_next(a->ui.view, delta < 0 ? 1 : -1));
        }
        return;
    }

    /* ①a 底部横向圆点：普通滚轮组内横切 + 到头跨组，不用 Shift。 */
    if (hdots_on(a->ui.view) && in_rect(&a->ui.hdotsRect, c)) {
        n = qs_now();
        if (n - a->lastWheel < WHEEL_DEBOUNCE) return;
        a->lastWheel = n;
        hdots_wheel(a, delta, horiz || (GetAsyncKeyState(VK_SHIFT) & 0x8000));
        return;
    }

    /* ①b 便签视图：悬浮在文本区时滚轮滚内容（不是切视图），
       同样累加到 WHEEL_DELTA 才走一行，且完全不拦截输入。 */
    if (note_view(a->ui.view)) {
        NoteBox nb;
        ui_note_box(a, &nb);
        if (c.x >= nb.x && c.x < nb.x + nb.w &&
            c.y >= nb.y && c.y < nb.y + nb.h) {
            a->wheelAccum += delta;
            while (a->wheelAccum >= WHEEL_DELTA ||
                   a->wheelAccum <= -WHEEL_DELTA) {
                int st = a->wheelAccum > 0 ? 1 : -1;
                a->wheelAccum -= st * WHEEL_DELTA;
                note_scroll(&a->note, -st * NOTE_SCROLL_STEP);
                a->dirty = 1;
            }
        } else {
            a->wheelAccum = 0;
        }
        return;
    }

    /* ①b2 剪贴板历史：悬浮在列表上时滚轮滚条目（不是切卡片），
       同样累加到 WHEEL_DELTA 才走一行，且完全不拦截输入。 */
    if (clip_view(a->ui.view)) {
        ClipBox cb;
        ui_clip_box(a, &cb);
        if (c.x >= cb.x && c.x < cb.x + cb.w &&
            c.y >= cb.y && c.y < cb.y + cb.h) {
            a->wheelAccum += delta;
            while (a->wheelAccum >= WHEEL_DELTA ||
                   a->wheelAccum <= -WHEEL_DELTA) {
                int st = a->wheelAccum > 0 ? 1 : -1;
                a->wheelAccum -= st * WHEEL_DELTA;
                clip_scroll(&a->clip, -st * CLIP_SCROLL_STEP);
                a->dirty = 1;
            }
        } else {
            a->wheelAccum = 0;
        }
        return;
    }

    /* ①b3 关于页 / 更新页：内容比可视区高，右栏内容框内滚轮滚文档
       （不切卡片）。与绘制共用 settings_boxes() 的 aboutBody / updateBody，
       裁剪范围、docH clamp、滚动条位置三者不会错位。
       横向阶段底部横点热区已在 ①a 优先处理，滚到那里仍是切卡。 */
    if (settings_view(a->ui.view) &&
        (settings_tab(&a->settings) == SET_TAB_ABOUT ||
         settings_tab(&a->settings) == SET_TAB_UPDATE)) {
        Boxes ab;
        double w, h, r, bx, by, bw, bh;
        int isUpd = (settings_tab(&a->settings) == SET_TAB_UPDATE);
        anim_geometry(&a->anim, &w, &h, &r);
        compute(a, w * a->u, h * a->u, &ab);
        if (isUpd) {
            bx = ab.set.updateBody.x; by = ab.set.updateBody.y;
            bw = ab.set.updateBody.w; bh = ab.set.updateBody.h;
        } else {
            bx = ab.set.aboutBody.x; by = ab.set.aboutBody.y;
            bw = ab.set.aboutBody.w; bh = ab.set.aboutBody.h;
        }
        if (c.x >= bx && c.x < bx + bw && c.y >= by && c.y < by + bh) {
            a->wheelAccum += delta;
            while (a->wheelAccum >= WHEEL_DELTA ||
                   a->wheelAccum <= -WHEEL_DELTA) {
                int st = a->wheelAccum > 0 ? 1 : -1;
                a->wheelAccum -= st * WHEEL_DELTA;
                if (isUpd) update_result_wheel(-st, bh);
                else       about_wheel(a, bh, -st);
                a->dirty = 1;
            }
        } else {
            a->wheelAccum = 0;
        }
        return;
    }

    /* ①c 日期时钟页 / 性能监测页 / 设置页 / 留空卡都是只读或点选界面：
       竖向阶段只有最左竖条切档，横向阶段只有底部圆点切档；
       其余位置的滚轮既不滚内容也不调时间，且不拦截输入。 */
    if (perf_view(a->ui.view) || empty_view(a->ui.view) ||
        date_view(a->ui.view) || settings_view(a->ui.view)) {
        a->wheelAccum = 0;
        return;
    }

    /* ② 悬浮在倒计时数字上：按段调时间（仅计时器模式） */
    if (a->ui.mode != 0) return;
    seg = num_segment(a, c.x, c.y);
    if (seg < 0) { a->wheelAccum = 0; return; }

    /* RAWMOUSE.usButtonData 是带符号 short，高分辨率滚轮可能小于 120，
       按官方建议累加到 WHEEL_DELTA 再走一步。 */
    a->wheelAccum += delta;
    while (a->wheelAccum >= WHEEL_DELTA ||
           a->wheelAccum <= -WHEEL_DELTA) {
        int st = a->wheelAccum > 0 ? 1 : -1;
        a->wheelAccum -= st * WHEEL_DELTA;
        ui_adjust(a, seg, st);
    }
}

void ui_wheel(App *a, int delta)   { wheel_core(a, delta, 0); }
void ui_wheel_h(App *a, int delta) { wheel_core(a, delta, 1); }

const RECT *ui_wheel_rect(const App *a) { return &a->ui.wheelRect; }

#define DRAG_THRESH 4       /* 设备像素：超过才算拖动，否则算单击 */

void ui_poll(App *a)
{
    POINT pt, c;
    int down, over;
    if (!a->visible || !a->hwnd || !IsWindowVisible(a->hwnd)) return;
    if (!GetCursorPos(&pt)) return;
    c = pt;
    ScreenToClient(a->hwnd, &c);

    if (a->note.editing && a->ui.view != NOTE_VIEW) input_end(a);
    over = anim_inside_target(&a->anim, a->ui.view, a->u, a->canvas.W,
                              HOVER_MARGIN, c.x, c.y);
    if (a->note.editing) over = 1;                  /* 编辑期间不收起 */
    /* cd.ended 不再强制展开：归零只弹通知，通知自己负责强制展开 */
    if (notify_active(&a->notify)) over = 1;        /* 通知强制展开到过期/点掉 */
    if (over != a->hover) {
        a->hover = over;
        /* 进入总是展开；离开是否收起由设置决定。
           关闭「鼠标离开岛收起」后，移开鼠标保持展开（其它两个条件
           或「永不收起」由设置组合给出）。 */
        if (over) a->anim.expanded = 1;
        else if (settings_collapse_on_leave(&a->settings))
            a->anim.expanded = 0;
        a->dirty = 1;
    }

    /* 悬停提示：只在「进入 / 退出 / 显示 / 消失」几帧置脏，
       气泡显示期间不置脏，静止零重绘不变。 */
    {
        int hid = (over && a->anim.exp > 0.5) ? ui_hit(a, c.x, c.y) : H_NONE;
        RECT hr;
        if (hid != H_NONE) hr = ui_hit_rect(a, hid);
        else SetRect(&hr, 0, 0, 0, 0);
        tooltip_tick(a, hid, &hr, qs_now());
    }

    /* 悬浮数字段：只在跨段时置脏，鼠标在同一段内移动不产生重绘 */
    {
        int seg = (over && timer_view(a->ui.view) &&
                   a->ui.mode == 0 && a->anim.exp > 0.5)
                  ? num_segment(a, c.x, c.y) : -1;
        if (seg != a->ui.hoverSeg) {
            a->ui.hoverSeg = seg;
            a->dirty = 1;
        }
    }

    /* 剪贴板历史：悬浮行淡底，只在跨行时置脏（同一行内移动不重绘） */
    {
        int old = a->clip.hoverRow, want = -1;
        if (over && clip_view(a->ui.view) && a->anim.exp > 0.5) {
            RECT r = ui_hit_rect(a, H_CLIP);
            ClipBox cb;
            cb.x = r.left; cb.y = r.top;
            cb.w = r.right - r.left; cb.h = r.bottom - r.top;
            if (cb.w > 0 && cb.h > 0) want = 1;
        }
        if (want) {
            RECT r = ui_hit_rect(a, H_CLIP);
            ClipBox cb;
            cb.x = r.left; cb.y = r.top;
            cb.w = r.right - r.left; cb.h = r.bottom - r.top;
            clip_hover_row(&a->clip, cb, c.x, c.y);
        } else {
            a->clip.hoverRow = -1;
        }
        if (a->clip.hoverRow != old) a->dirty = 1;
    }

    down = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
    if (down && !a->prevDown) {
        /* 点击卡片外：结束便签编辑并保存（不吞掉这次点击） */
        if (!over && a->note.editing) input_end(a);
        /* 「鼠标点击岛外收起」：只有开启该条件才把展开状态收回胶囊态，
           且 hover 与 over 保持一致，下一帧不会立刻重新展开。 */
        if (!over && settings_collapse_on_outside(&a->settings) &&
            a->anim.expanded) {
            a->anim.expanded = 0;
            a->hover = 0;
            a->dirty = 1;
        }
        /* 只记录，不立即执行 —— 否则单击会被误判成调值 */
        a->pressId = over ? ui_hit(a, c.x, c.y) : H_NONE;
        a->pressX = c.x;
        a->pressY = c.y;
        a->pressMoved = 0;
        a->drag = 0;
        if (a->pressId == H_NOTE) {
            /* 便签：按下即定位 anchor 并清旧选区，拖动时逐帧扩展选区。
               这里不走「松开才 activate」的通用逻辑，否则一松手就把刚
               框好的选区重新点没了。 */
            NoteBox nb;
            ui_note_box(a, &nb);
            note_click(&a->note, a, nb, c.x, c.y);
            input_begin(a);
            a->noteSel = 1;
            a->pressId = H_NONE;
            a->dirty = 1;
        }
    } else if (down && a->noteSel) {
        NoteBox nb;
        ui_note_box(a, &nb);
        note_drag_to(&a->note, a, nb, c.x, c.y);
        a->dirty = 1;
    } else if (down && a->pressId != H_NONE) {
        if (!a->pressMoved &&
            (abs(c.x - a->pressX) > DRAG_THRESH ||
             abs(c.y - a->pressY) > DRAG_THRESH)) {
            a->pressMoved = 1;
            if (a->pressId == H_VOL || a->pressId == H_BRI ||
                a->pressId == H_SLIDER_VOL || a->pressId == H_SLIDER_BRI) {
                a->drag = a->pressId;
                ui_drag(a, a->drag, c.x, c.y);
            }
        } else if (a->pressMoved && a->drag) {
            ui_drag(a, a->drag, c.x, c.y);
        }
    }
    if (!down && a->prevDown) {
        if (a->drag) {
            ui_end_drag(a, a->drag);
        } else if (!a->pressMoved && a->pressId != H_NONE) {
            ui_activate(a, a->pressId, a->pressX, a->pressY);
        }
        a->pressId = H_NONE;
        a->pressMoved = 0;
        a->drag = 0;
        a->noteSel = 0;
    }
    a->prevDown = down;
}

void ui_render(App *a)
{
    double w, h, r, W, H, ta;
    WCHAR clockW[16];
    char clockA[16];

    a->renderCount++;
    qs_clock_a(clockA, sizeof(clockA));
    qs_clock_w(clockW, 16);
    anim_geometry(&a->anim, &w, &h, &r);
    W = w * a->u; H = h * a->u;

    canvas_shape(&a->canvas, w, h, r);

    /* 岛态：左侧时钟（数字，展开时淡出）；倒计时进行时右侧同时显示
       剩余时间。两侧边距同为 CLOCK_PAD，左对齐 / 右对齐对称。 */
    if (a->anim.exp < 0.98) {
        double sx = ((double)a->canvas.W - ISLAND_W * a->u) * 0.5;
        ta = 1.0 - qs_clamp(a->anim.exp * 1.7, 0.0, 1.0);
        font_draw_text(&a->fonts, &a->canvas, clockW,
                       (int)(20 * a->u + 0.5),
                       sx + CLOCK_PAD * a->u, ISLAND_H * a->u * 0.5, 0,
                       INK_R, INK_G, INK_B, ta);

        if (a->cd.running) {
            double rem = countdown_remain(&a->cd);
            int hh = (int)(rem / 3600.0);
            int mm = (int)((rem - hh * 3600.0) / 60.0);
            int ss = (int)(rem - hh * 3600.0 - mm * 60.0);
            WCHAR cdw[16];
            if (hh > 0) _snwprintf(cdw, 16, L"%02d:%02d:%02d", hh, mm, ss);
            else        _snwprintf(cdw, 16, L"%02d:%02d", mm, ss);
            font_draw_text(&a->fonts, &a->canvas, cdw,
                           (int)(CD_ISLAND_PX * a->u + 0.5),
                           sx + ISLAND_W * a->u - CLOCK_PAD * a->u,
                           ISLAND_H * a->u * 0.5, 2,
                           ACCENT_R, ACCENT_G, ACCENT_B, ta);
        } else {
            /* 没有倒计时时，胶囊右侧显示用户图标包里「中间白、边缘黑」
               那张圆图标；右缘与上面剩余时间文字的右缘对齐。
               素材离线编成 64x64 灰度+alpha 数组，不读外部文件、不依赖
               WIC/GDI+；按 alpha 叠进 layered DIB，展开时随 ta 一起淡出。 */
            double d = CAPSULE_ICON_RATIO * ISLAND_H * a->u;
            double right = sx + ISLAND_W * a->u - CLOCK_PAD * a->u;
            capsule_icon_draw(&a->canvas, right - d * 0.5,
                              ISLAND_H * a->u * 0.5, d, ta);
        }
    }

    if (a->anim.exp > 0.02) {
        double k = qs_clamp((a->anim.exp - 0.22) / 0.55, 0.0, 1.0);
        ui_layout(a, W, H);
        ui_draw(a, W, H, k);
    } else {
        a->ui.hitN = 0;
        SetRect(&a->ui.wheelRect, 0, 0, 0, 0);
    }

    /* 气泡最后画，覆盖所有页面（含通知态的 early return） */
    tooltip_draw(a, &a->canvas);

    canvas_present(&a->canvas, a->hwnd);
    memcpy(a->lastClock, clockA, sizeof(a->lastClock));
}
