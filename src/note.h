/* note.h — 第二个中卡片：便签
 *
 * 设计约束：
 *   · 不画输入框边框、不画白纸块、不写 placeholder 文字 —— 文字直接落在
 *     卡片本体上，所以「不太突兀」。空态只有一个极淡的 pencil 图标。
 *   · 不使用 EDIT / RichEdit 子窗口控件：UpdateLayeredWindow 的分层窗口上
 *     任何 WS_CHILD 子窗口都不会被合成到 alpha 位图，控件会整个不可见。
 *     文本编辑状态改由 stb_textedit.h（无窗口、无渲染的 public domain 库）
 *     维护，渲染仍走本项目的 canvas / font，圆角裁切与抗锯齿不变。
 *   · 滚动完全自绘，没有系统滚动条；内容溢出时才在右缘出现一条极淡的
 *     圆头指示条（美化版），不画箭头、不画滑轨方块。
 *   · 文本自绘走 font 模块的 4x4 面积覆盖率，中文用雅黑，无锯齿。
 */
#ifndef QISLAND_NOTE_H
#define QISLAND_NOTE_H

#include "common.h"
#include "config.h"
#include "textedit_glue.h"

typedef struct { double x, y, w, h; } NoteBox;

typedef struct {
    WCHAR  text[NOTE_MAX + 1];
    int    len;

    /* 编辑状态：光标、选区两端、撤销栈全部由 stb_textedit 维护。
       select_start != select_end 即「有选中」，由 note_draw 画成淡高亮。 */
    STB_TexteditState stb;

    int    editing;
    int    dirty;             /* 有未保存改动 */
    DWORD  lastSave;

    /* 布局缓存：只在文本 / 尺寸变化时重算，动画帧不重复测量 */
    int    lineStart[NOTE_MAX_LINES];
    int    lineEnd[NOTE_MAX_LINES];
    int    lineCount;
    int    visLines;
    int    topLine;
    int    px, lineH;
    double areaX, areaY, areaW, areaH;
    unsigned int sig;

    /* stb 的 LAYOUTROW / GETWIDTH 回调只能拿到 Note*，所以这里存宿主上下文 */
    void   *fontPtr;          /* FontSet* */
    NoteBox box;              /* 最近一次布局的设备像素文本区 */
} Note;

void note_init(Note *n);
void note_shutdown(Note *n);           /* 退出时强制保存 */
void note_save(Note *n, int force);
void note_clear(Note *n);              /* 清空文本与编辑状态（测试/诊断用） */

/* 文本编辑（内部走 stb_textedit，会自动替换当前选区） */
void note_insert(Note *n, const WCHAR *s, int count);
void note_backspace(Note *n);
void note_delete(Note *n);
void note_newline(Note *n);
void note_move(Note *n, int op);       /* 见 note.c 的 OP_* */
void note_scroll(Note *n, int lines);

/* 键盘：key 是 note.h 里定义的 NK_* 值（含 SHIFT 位） */
enum {
    NK_LEFT = 1, NK_RIGHT, NK_UP, NK_DOWN,
    NK_LINESTART, NK_LINEEND, NK_TEXTSTART, NK_TEXTEND,
    NK_DELETE, NK_BACKSPACE, NK_UNDO, NK_REDO,
    NK_PGUP, NK_PGDN, NK_WORDLEFT, NK_WORDRIGHT,
    NK_SHIFT = 0x40000000
};
void note_key(Note *n, int key);
void note_select_all(Note *n);
int  note_has_selection(const Note *n);
void note_selection_range(const Note *n, int *a, int *b);
/* 复制/剪切选中段到剪贴板；返回复制的字符数 */
int  note_copy_selection(Note *n, int cut);
/* 粘贴：会替换当前选区 */
void note_paste(Note *n, const WCHAR *s, int count);

/* 交互 */
void note_set_editing(Note *n, int on);
void note_click(Note *n, const App *a, NoteBox box, int x, int y);
void note_drag_to(Note *n, const App *a, NoteBox box, int x, int y);
int  note_caret_xy(Note *n, const App *a, NoteBox box, int *x, int *y);

/* 绘制（设备像素） */
void note_draw(Note *n, App *a, NoteBox box, double k);

/* 诊断用 */
int  note_line_count(const Note *n);
int  note_vis_lines(const Note *n);
int  note_overflow(const Note *n);

#endif /* QISLAND_NOTE_H */
