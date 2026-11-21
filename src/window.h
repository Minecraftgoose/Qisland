/* window.h — 分层窗口、DPI、Raw Input、热键、窗口过程
 * 0.3.1：DPI 感知由 src/qisland.manifest 声明（PerMonitorV2 → PerMonitor →
 * System 逐级回退，Win7 由 SetProcessDPIAware 兜底）；运行时按窗口所在显示器
 * 取 DPI，WM_DPICHANGED 时重建画布 / 字体 / 命中表。缩放上限 300%。
 * 注意：岛自身的右键菜单已删除；右键菜单只存在于系统托盘图标（见 tray.h）。 */
#ifndef QISLAND_WINDOW_H
#define QISLAND_WINDOW_H

#include "common.h"

int  window_init(App *a);
void window_shutdown(App *a);
void window_show(App *a);
void window_hide(App *a);
LRESULT CALLBACK window_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

/* 鼠标形状：客户区坐标 (cx,cy) 是否应显示工字形文本指针 IDC_IBEAM。
   只有第二张中卡片（便签）的文本区是工字，其余一律箭头。
   由 WM_SETCURSOR 调用；关于页链接命中时另外显示手型光标。 */
int  window_text_cursor(const App *a, int cx, int cy);
HCURSOR window_cursor_handle(int ibeam);

#endif /* QISLAND_WINDOW_H */
