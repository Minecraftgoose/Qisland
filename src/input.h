/* input.h — 便签输入：焦点切换、键盘、IME 中文输入 */
#ifndef QISLAND_INPUT_H
#define QISLAND_INPUT_H

#include "common.h"

void input_init(App *a);
void input_shutdown(App *a);
void input_begin(App *a);     /* 进入便签编辑 */
void input_end(App *a);       /* 结束编辑并保存 */
int  input_active(const App *a);
int  input_message(App *a, HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

#endif /* QISLAND_INPUT_H */
