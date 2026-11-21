/* tooltip.h — 鼠标悬停在图标按钮上时的功能提示气泡
 *
 * 不建窗口、不进钩子链、不用 TOOLTIPS_CLASS：分层窗口的 WS_CHILD 控件
 * 不会被 UpdateLayeredWindow 合成，所以气泡自绘进主 Canvas。
 * 只在「显示 / 隐藏」两帧置脏，显示期间静止不重绘。
 *
 * 时序按官方 TTM_SETDELAYTIME 口径：初始延迟 = GetDoubleClickTime()
 * （默认 500ms）、换按钮 reshow = 1/5、autopop = 10×（约 5 秒）。
 */
#ifndef QISLAND_TOOLTIP_H
#define QISLAND_TOOLTIP_H

#include "common.h"
#include "canvas.h"

typedef struct {
    int    candId;       /* 当前悬停的命中 id（H_NONE 表示没停在控件上） */
    DWORD  since;        /* 进入该 id 的时刻 */
    int    active;       /* 气泡是否正在显示 */
    int    shownId;      /* 已显示的气泡对应哪个控件 */
    RECT   rc;           /* 显示时锁定的按钮矩形（客户区设备像素） */
    DWORD  popAt;        /* autopop 计时起点 */
    int    reshow;       /* 刚换过按钮：用短延迟 */
} Tooltip;

void tooltip_init(Tooltip *t);
void tooltip_hide(App *a);
void tooltip_tick(App *a, int hoverId, const RECT *rc, DWORD now);
void tooltip_draw(App *a, Canvas *c);

/* 文案表：只有登记在这里的控件才有气泡；返回 NULL 表示该控件不提示 */
const WCHAR *tooltip_text(int id);

#endif /* QISLAND_TOOLTIP_H */
