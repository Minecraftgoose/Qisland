/* clip.h — 第一个大卡片：剪贴板历史记录 */
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

/* 内部插入一条文本； 返回 1=新增 / 0=被去重或空白 */
int    clip_push_text(Clip *c, const WCHAR *s, int len);
/* 读系统剪贴板 CF_UNICODETEXT，返回字符数； 按 GlobalSize 找终止符 */
int    clip_read_text(HWND hwnd, WCHAR *out, int cap);
/* 写回系统剪贴板，返回 1 成功； 会抑制随后自己触发的通知 */
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
