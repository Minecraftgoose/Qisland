/* font.h — 字体与图标渲染 */
#ifndef QISLAND_FONT_H
#define QISLAND_FONT_H

#include "common.h"
#include "canvas.h"

#define GLYPH_CACHE_MAX 96
#define FONT_TMP_W      4096
#define FONT_TMP_H      640

typedef struct {
    unsigned int key;
    int  px;                /* 输出字号 */
    int  kind;              /* 0=UI文字 1=FA图标 2=中文标签 */
    int  w, h;              /* 输出覆盖率位图尺寸 */
    float *cov;             /* w*h，0..1 */
} GlyphMask;

typedef struct {
    double u;
    WCHAR  fontPath[MAX_PATH];   /* FA 图标字体路径 */
    WCHAR  oppoPath[MAX_PATH];   /* OPPO Sans 4.0 路径 */
    WCHAR  face[64];             /* FA 家族 */
    WCHAR  textFace[64];         /* 全局正文家族：OPPO Sans 4.0 或回退 */
    WCHAR  cjkFace[64];          /* 中文正文家族：同上 */
    int    loaded;          /* FA 的 AddFontResourceExW 结果 */
    int    oppoLoaded;      /* OPPO 的 AddFontResourceExW 结果 */
    int    oppoOK;          /* 探测通过：全局字体确实可用 */
    int    faOK;            /* 自检通过：FA 字形确实可用 */
    HFONT  uiClock, uiSmall, uiMid, uiBig;
    HFONT  measureFont;     /* 缓存的测量字体，便签换行用 */
    int    measurePx, measureKind;
    HDC    tmpDC;
    HBITMAP tmpBmp;
    void  *tmpBits;
    GlyphMask cache[GLYPH_CACHE_MAX];
    int    cacheN;
} FontSet;

int  font_init(FontSet *f, double u);
void font_shutdown(FontSet *f);

/* 单码点 FA 图标； x/y 为输出左上角（设备像素），align 0左 1中 2右 */
void font_draw_icon(FontSet *f, Canvas *c, unsigned int cp, int px,
                    double x, double y, int align,
                    int cr, int cg, int cb, double k);
/* 普通文字：HH MM SS 读数与岛态时钟 */
void font_draw_text(FontSet *f, Canvas *c, const WCHAR *s, int px,
                    double x, double y, int align,
                    int cr, int cg, int cb, double k);

void font_draw_label(FontSet *f, Canvas *c, const WCHAR *s, int px,
                     double x, double y, int align,
                     int cr, int cg, int cb, double k);
void font_draw_note(FontSet *f, Canvas *c, const WCHAR *s, int px,
                    double x, double y, int align,
                    int cr, int cg, int cb, double k);
int  font_measure(FontSet *f, const WCHAR *s, int len, int px, int kind);

/* 自检结果，供 --diag 输出 */
int  font_oppo_ok(const FontSet *f);
const WCHAR *font_text_face(const FontSet *f);
int  font_fa_ok(const FontSet *f);
const WCHAR *font_fa_face(const FontSet *f);

#endif /* QISLAND_FONT_H */
