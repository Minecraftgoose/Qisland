/* textedit_glue.h — 把 stb_textedit.h 以「头文件模式」引入，统一字符类型
 *
 * stb_textedit.h v1.16 (Sean Barrett, public domain) 是一个**无窗口、无渲染**
 * 的多行文本编辑内核：它只维护 cursor / select_start / select_end / undo，
 * 由宿主提供换行布局、字宽、字符串增删。之所以用它而不是 EDIT/RichEdit
 * 子窗口控件，是因为 UpdateLayeredWindow 的分层窗口上任何 WS_CHILD 子窗口
 * 都不会被合成到 alpha 位图（子控件不可见），这是 Win32 的硬限制。
 *
 * 这里定义的是 header 与 implementation 模式**必须一致**的四个宏；
 * 其余宿主宏（STRING / LAYOUTROW / GETWIDTH ...）只在 note.c 定义。
 */
#ifndef QISLAND_TEXTEDIT_GLUE_H
#define QISLAND_TEXTEDIT_GLUE_H

#include "common.h"

#define STB_TEXTEDIT_CHARTYPE        WCHAR   /* 直接对接便签的 UTF-16 文本 */
#define STB_TEXTEDIT_POSITIONTYPE    int
#define STB_TEXTEDIT_UNDOSTATECOUNT  32
#define STB_TEXTEDIT_UNDOCHARCOUNT   512

#include "stb_textedit.h"

#endif /* QISLAND_TEXTEDIT_GLUE_H */
