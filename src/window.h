/* window.h — 分层窗口、DPI、Raw Input、热键、右键菜单、窗口过程 */
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
   抽出来供 WM_SETCURSOR 与 --diag 断言共用，避免测试与实现各判一套。 */
int  window_text_cursor(const App *a, int cx, int cy);
HCURSOR window_cursor_handle(int ibeam);

#endif /* QISLAND_WINDOW_H */
