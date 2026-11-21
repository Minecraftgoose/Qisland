/* clip.h — 第一个大卡片：剪贴板历史记录
 *
 * 官方依据（先读文档再动手）：
 *   · AddClipboardFormatListener / RemoveClipboardFormatListener / WM_CLIPBOARDUPDATE
 *     最低支持 Windows Vista（desktop apps），Win7 SP1 完全可用；
 *     窗口被加入系统维护的 listener 列表后，剪贴板内容每次变化都会
 *     **post** WM_CLIPBOARDUPDATE，不要求它是 clipboard viewer chain 的一员，
 *     也不要求窗口有焦点 —— 这对 WS_EX_NOACTIVATE 的分层窗口正合适。
 *     比老的 SetClipboardViewer 链安全：不用转发 WM_DRAWCLIPBOARD，
 *     也不会因为忘记处理 WM_CHANGECBCHAIN 把整条链弄断。
 *   · SetClipboardData(CF_UNICODETEXT, hMem)：调用前必须 OpenClipboard 且
 *     窗口是当前 owner；hMem 必须用 GlobalAlloc(GMEM_MOVEABLE)；成功后
 *     **内存所有权转移给系统**，不能再写入或 GlobalFree，失败才自己释放。
 *     另外官方明确：响应 WM_RENDERFORMAT 时不得调用 SetClipboardData —— 我们
 *     不使用延迟呈现，所以不涉及。
 *   · GetClipboardSequenceNumber（Windows 2000+）：内容变化或清空时递增，
 *     用来合并重复投递的通知，并作为 listener 不可用时的轮询兜底。
 *   · GetClipboardOwner：EmptyClipboard 会把 owner 设为当前 OpenClipboard 的
 *     窗口，用来识别「本程序自己写回」的通知，避免把回填当成新复制。
 *   · 读 CF_UNICODETEXT 时不假定字符串一定带 null terminator，
 *     按 GlobalSize/2 个字符为上限找终止符（Old New Thing 的官方示例做法）。
 *
 * 设计：
 *   · 只记 CF_UNICODETEXT 文本（图片 / 自定义格式不进历史）；
 *   · 最新在顶部，完全相同则跳过，重复复制旧条目则移到顶部；
 *   · 上限 32 条，单条 1024 UTF-16 字符；换行压成空格，单行截断加省略号；
 *   · 点某条即写回系统剪贴板（不是"复制一份历史"，是真的可直接粘贴）；
 *   · 自写回的通知被抑制，历史不会自己增殖；
 *   · 滚动完全自绘：没有系统滚动条、没有轨道，只有右缘极淡圆头短滑块；
 *   · 界面无标题、无标签、无 placeholder 文字，只有条目本身 + 图标。
 *   · 历史存 %APPDATA%\Qisland\clipboard.bin，二进制长度前缀格式。
 */
#ifndef QISLAND_CLIP_H
#define QISLAND_CLIP_H

#include "common.h"
#include "config.h"

typedef struct { double x, y, w, h; } ClipBox;

#define CLIP_MAX_ITEMS   32
#define CLIP_MAX_CHARS   1024
#define CLIP_LINE_CHARS  192

typedef struct {
    WCHAR  text[CLIP_MAX_CHARS + 1];
    int    len;
    DWORD  stamp;
} ClipItem;

typedef struct {
    ClipItem items[CLIP_MAX_ITEMS];
    int      count;
    int      top;            /* 首条可见索引 */
    int      selected;       /* 最近回填的条目，-1 = 无 */
    int      hoverRow;       /* 悬浮的可见行号，-1 = 无 */
    int      dirty;

    /* 监听与去重 */
    int      listenerOK;
    int      detached;
    int      testFile;           /* 诊断/演示走 clipboard_test.bin，不碰真实历史 */
    int      selfWrite;
    int      captures, dupSkipped, selfSkipped, writeOK, pollHits;
    DWORD    lastSeq, lastPollAt;
    WCHAR    lastWritten[CLIP_MAX_CHARS + 1];
    int      lastWrittenLen;
    DWORD    lastWrittenAt;
    HMODULE  u32;

    /* 布局缓存：内容/尺寸不变时不重算截断 */
    unsigned int sig;
    int      px, lineH, visRows;
    WCHAR    line[CLIP_MAX_ITEMS][CLIP_LINE_CHARS + 1];
    int      lineLen[CLIP_MAX_ITEMS];
    double   areaX, areaY, areaW, areaH;
} Clip;

void   clip_init(Clip *c, App *a);
void   clip_shutdown(Clip *c, App *a);
void   clip_detach(Clip *c);               /* WM_DESTROY：反注册 listener */
void   clip_poll(Clip *c, App *a);         /* listener 不可用时的序列兜底 */
void   clip_on_update(Clip *c, App *a);    /* WM_CLIPBOARDUPDATE */

/* 内部插入一条文本；返回 1=新增 / 0=被去重或空白 */
int    clip_push_text(Clip *c, const WCHAR *s, int len);
/* 读系统剪贴板 CF_UNICODETEXT，返回字符数；按 GlobalSize 找终止符 */
int    clip_read_text(HWND hwnd, WCHAR *out, int cap);
/* 写回系统剪贴板，返回 1 成功；会抑制随后自己触发的通知 */
int    clip_set_clipboard(Clip *c, App *a, const WCHAR *s, int len);
void   clip_clear(Clip *c, App *a);        /* 清空历史，不动系统剪贴板 */
void   clip_scroll(Clip *c, int rows);
void   clip_click(Clip *c, App *a, ClipBox box, int x, int y);
/* 返回悬浮的可见行号（-1 无），只在其变化时置脏 */
int    clip_hover_row(Clip *c, ClipBox box, int x, int y);
void   clip_draw(Clip *c, App *a, ClipBox box, double k);
void   clip_save(Clip *c, int force);
void   clip_reload(Clip *c);           /* 清空内存后从文件重读，验证持久化 */

/* 诊断 */
int    clip_count(const Clip *c);
int    clip_listener(const Clip *c);
int    clip_captures(const Clip *c);
int    clip_dups(const Clip *c);
int    clip_self_skipped(const Clip *c);
int    clip_write_ok(const Clip *c);
int    clip_vis_rows(const Clip *c);
int    clip_overflow(const Clip *c);
const WCHAR *clip_at(const Clip *c, int idx);

#endif /* QISLAND_CLIP_H */
