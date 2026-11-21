/* textedit_glue.h — 把 stb_textedit.h 以「头文件模式」引入，统一字符类型 */
#ifndef QISLAND_TEXTEDIT_GLUE_H
#define QISLAND_TEXTEDIT_GLUE_H

#include "common.h"

#define STB_TEXTEDIT_CHARTYPE        WCHAR   /* 直接对接便签的 UTF-16 文本 */
#define STB_TEXTEDIT_POSITIONTYPE    int
#define STB_TEXTEDIT_UNDOSTATECOUNT  32
#define STB_TEXTEDIT_UNDOCHARCOUNT   512

#include "stb_textedit.h"

#endif /* QISLAND_TEXTEDIT_GLUE_H */
