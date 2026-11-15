/* input.h — 便签输入：焦点切换、键盘、IME 中文输入
 *
 * 主窗口平时是 WS_EX_NOACTIVATE（不抢焦点、不进任务栏），收不到 WM_CHAR。
 * 点击便签时临时去掉 NOACTIVATE 让窗口可获得焦点，结束时加回去 ——
 * 比另开一个隐藏输入窗口简单，且 IME 候选框能直接跟随便签光标。
 */
#ifndef QISLAND_INPUT_H
#define QISLAND_INPUT_H

#include "common.h"

void input_init(App *a);
void input_shutdown(App *a);
void input_begin(App *a);     /* 进入便签编辑 */
void input_end(App *a);       /* 结束编辑并保存 */
int  input_active(const App *a);
/* 返回 1 = 已处理（window_proc 直接 return 0），0 = 继续 DefWindowProc */
int  input_message(App *a, HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

#endif /* QISLAND_INPUT_H */
