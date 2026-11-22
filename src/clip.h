/* clip.h 剪贴板历史记录 */
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
    int      top;
    int      selected;
    int      hoverRow;
    int      dirty;

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

    unsigned int sig;
    int      px, lineH, visRows;
    WCHAR    line[CLIP_MAX_ITEMS][CLIP_LINE_CHARS + 1];
    int      lineLen[CLIP_MAX_ITEMS];
    double   areaX, areaY, areaW, areaH;
} Clip;

void   clip_init(Clip *c, App *a);
void   clip_shutdown(Clip *c, App *a);
void   clip_detach(Clip *c);
void   clip_poll(Clip *c, App *a);
void   clip_on_update(Clip *c, App *a);    /* WM_CLIPBOARDUPDATE */

int    clip_push_text(Clip *c, const WCHAR *s, int len);
int    clip_read_text(HWND hwnd, WCHAR *out, int cap);
int    clip_set_clipboard(Clip *c, App *a, const WCHAR *s, int len);
void   clip_clear(Clip *c, App *a);
void   clip_scroll(Clip *c, int rows);
void   clip_click(Clip *c, App *a, ClipBox box, int x, int y);
int    clip_hover_row(Clip *c, ClipBox box, int x, int y);
void   clip_draw(Clip *c, App *a, ClipBox box, double k);
void   clip_save(Clip *c, int force);
void   clip_reload(Clip *c);

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
