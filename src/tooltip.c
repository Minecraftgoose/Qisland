/* tooltip.c — 悬停提示气泡（黑底圆角 + 小尖角 + 白字）
 *
 * 为什么不用系统 TOOLTIPS_CLASS：主窗口是 WS_EX_LAYERED，所有内容都要画进
 * 32bpp 预乘 BGRA 位图再由 UpdateLayeredWindow 合成；WS_CHILD 控件不会被
 * 合成进去。所以气泡用 canvas_round_alpha / canvas_tri 自己写 alpha。
 *
 * 气泡可以浮出卡片（画在主形状的透明区），为此 WM_NCHITTEST 加了
 * anim_inside_target 条件：形状外的像素即便有 alpha 也返回 HTTRANSPARENT，
 * 不吞桌面点击。文字仍走 font_draw_label 的 4x4 覆盖率，无锯齿。
 */
#include "tooltip.h"
#include "app.h"
#include "config.h"
#include "font.h"
#include "ui.h"

/* 封闭清单第 8 类（见 AGENTS.md 第 4 节第 8 条）：悬停提示文案。
   仅鼠标停在控件上时出现，不常驻、不进任何页面布局。
   逐条登记，不得自行扩项。 */
const WCHAR *tooltip_text(int id)
{
    switch (id) {
    case H_WHEEL:        return L"滚轮切换卡片";
    case H_HIDE:         return L"隐藏灵动岛";
    case H_BRI:          return L"调节亮度，点击切换滑块";
    case H_VOL:          return L"调节音量，点击切换滑块";
    case H_TASKMGR:      return L"任务管理器";
    case H_PLAY:         return L"开始或暂停倒计时";
    case H_RESET:        return L"重新倒计时";
    case H_SLIDER_VOL:   return L"拖动调节音量";
    case H_SLIDER_BRI:   return L"拖动调节亮度";
    case H_NUM_H:        return L"滚轮调整小时";
    case H_NUM_M:        return L"滚轮调整分钟";
    case H_NUM_S:        return L"滚轮调整秒数";
    case H_NOTE:         return L"点击输入文字";
    case H_CLIP:         return L"点击写回剪贴板";
    case H_CLIP_CLEAR:   return L"清空历史";
    case H_NOTIFY_CLOSE: return L"关闭通知";
    case H_HDOTS:        return L"滚轮切换卡片，点击跳转";
    default:             return NULL;
    }
}

void tooltip_init(Tooltip *t)
{
    ZeroMemory(t, sizeof(*t));
    t->candId = H_NONE;
    t->shownId = H_NONE;
}

void tooltip_hide(App *a)
{
    Tooltip *t = &a->tooltip;
    if (t->active) { t->active = 0; a->dirty = 1; }
    t->candId = H_NONE;
    t->reshow = 0;
}

void tooltip_tick(App *a, int hoverId, const RECT *rc, DWORD now)
{
    Tooltip *t = &a->tooltip;
    DWORD initial = GetDoubleClickTime();
    DWORD reshow, autopop, wait;

    if (initial < 120) initial = 120;
    reshow  = initial / 5;
    autopop = initial * 10;

    /* 鼠标不在窗口形状内 / 收起态 / 该控件没有登记文案 → 立即收起 */
    if (!a->visible || hoverId == H_NONE || !tooltip_text(hoverId) ||
        a->anim.exp <= 0.5) {
        if (t->active) { t->active = 0; a->dirty = 1; }
        t->candId = H_NONE;
        t->reshow = 0;
        return;
    }

    /* 换控件：先撤掉旧气泡，下一个用短 reshow 延迟 */
    if (hoverId != t->candId) {
        int was = t->active;
        t->candId = hoverId;
        t->since  = now;
        t->reshow = was ? 1 : 0;
        if (was) { t->active = 0; a->dirty = 1; }
        return;
    }

    if (t->active) {
        /* autopop：指针一直不动也自动消失，与系统 tooltip 一致 */
        if (now - t->popAt >= autopop) { t->active = 0; a->dirty = 1; }
        return;
    }

    wait = t->reshow ? reshow : initial;
    if (now - t->since >= wait) {
        t->active  = 1;
        t->shownId = hoverId;
        t->rc      = *rc;
        t->popAt   = now;
        t->reshow  = 0;
        a->dirty   = 1;
    }
}

void tooltip_draw(App *a, Canvas *c)
{
    Tooltip *t = &a->tooltip;
    const WCHAR *s;
    double u = a->u, bw, bh, bx, by, anchorX, triTop;
    int px, tw, above = 0;

    if (!t->active) return;
    s = tooltip_text(t->shownId);
    if (!s) return;

    px = (int)(TIP_PX * u + 0.5);
    if (px < 10) px = 10;
    /* kind=3 走全局字体，与 font_draw_label 的字体一致，宽度才量得准 */
    tw = font_measure(&a->fonts, s, (int)wcslen(s), px, 3);
    if (tw <= 0) return;

    bw = tw + 2.0 * TIP_PAD_X * u;
    bh = px + 2.0 * TIP_PAD_Y * u;

    anchorX = (t->rc.left + t->rc.right) * 0.5;
    bx = qs_clamp(anchorX - bw * 0.5, 4.0, (double)c->W - bw - 4.0);
    by = t->rc.bottom + TIP_GAP * u;
    if (by + bh > (double)c->H - 4.0) {
        by = t->rc.top - TIP_GAP * u - bh;
        above = 1;
        if (by < 4.0) by = 4.0;
    }

    /* ui_draw 里可能留下裁剪框（便签 / 剪贴板文本区），这里放开，
       否则浮出卡片的气泡会被裁掉 */
    canvas_reset_clip(c);

    /* 阴影 → 尖角 → 1px 淡边 → 本体。都写 alpha，可落在形状外。 */
    canvas_round_alpha(c, bx + 2.0 * u, by + 3.0 * u, bw, bh,
                       TIP_R * u, 0, 0, 0, 0.26);

    triTop = above ? (by + bh - 1.0) : (by - TIP_TRI_H * u + 1.0);
    canvas_tri(c, anchorX, triTop, TIP_TRI_W * u, TIP_TRI_H * u,
               above ? -1 : 1, TIP_BG_R, TIP_BG_G, TIP_BG_B, 1.0);

    canvas_round_alpha(c, bx - 1.0, by - 1.0, bw + 2.0, bh + 2.0,
                       TIP_R * u + 1.0,
                       TIP_EDGE_R, TIP_EDGE_G, TIP_EDGE_B, 0.55);
    canvas_round_alpha(c, bx, by, bw, bh, TIP_R * u,
                       TIP_BG_R, TIP_BG_G, TIP_BG_B, 1.0);

    /* 文字：气泡区域 alpha 已 >0，blend_px 正常生效；4x4 覆盖率无锯齿 */
    font_draw_label(&a->fonts, c, s, px,
                    bx + bw * 0.5, by + bh * 0.5, 1,
                    TIP_FG_R, TIP_FG_G, TIP_FG_B, 1.0);
}
