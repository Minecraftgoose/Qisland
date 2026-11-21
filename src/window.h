/* window.h — 分层窗口、DPI、Raw Input、热键、窗口过程 */
#ifndef QISLAND_WINDOW_H
#define QISLAND_WINDOW_H

#include "common.h"

int  window_init(App *a);
void window_shutdown(App *a);
void window_show(App *a);
void window_hide(App *a);
LRESULT CALLBACK window_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

int  window_text_cursor(const App *a, int cx, int cy);
HCURSOR window_cursor_handle(int ibeam);

#endif /* QISLAND_WINDOW_H */
