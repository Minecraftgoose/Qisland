/* font.c — 字形覆盖率渲染
 *
 * 关键点（也是「不要让我看见锯齿 fa」的解法）：
 *   GDI 在 32bpp DIB 上不保证写 alpha 通道，所以先把字形以黑底白字画进
 *   临时 DIB，只读回 RGB 灰度当覆盖率。再以 GLYPH_SS 倍字号渲染，
 *   对放大图做 SS x SS box-downsample，得到的是真实面积覆盖率而不是
 *   单像素灰度近似 —— 边缘连续多级灰阶，放大也不会出现台阶。
 *   覆盖率按 (码点/文本哈希, 字号) 缓存，动画帧里只做颜色合成，不重复光栅化。
 */
#include "font.h"
#include "config.h"
#include "icons.h"

static HFONT make_ui_font(int px)
{
    return CreateFontW(px, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                       DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                       ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, FONT_FACE);
}

static int make_dib(int w, int h, HDC *dc, HBITMAP *bmp, void **bits)
{
    BITMAPINFO bi;
    HDC s;
    ZeroMemory(&bi, sizeof(bi));
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = w;
    bi.bmiHeader.biHeight = -h;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    s = GetDC(NULL);
    *dc = CreateCompatibleDC(s);
    ReleaseDC(NULL, s);
    if (!*dc) return 0;
    *bmp = CreateDIBSection(*dc, &bi, DIB_RGB_COLORS, bits, NULL, 0);
    if (!*bmp) { DeleteDC(*dc); *dc = NULL; return 0; }
    SelectObject(*dc, *bmp);
    return 1;
}

static void build_path(WCHAR *out, size_t cap)
{
    WCHAR mod[MAX_PATH];
    DWORD n;
    WCHAR *slash;
    n = GetModuleFileNameW(NULL, mod, MAX_PATH);
    if (!n) { wcscpy(out, FA_FONT_FILE); return; }
    mod[n] = 0;
    slash = wcsrchr(mod, L'\\');
    if (slash) *(slash + 1) = 0;
    _snwprintf(out, (int)cap, L"%s%s", mod, FA_FONT_FILE);
    out[cap - 1] = 0;
}

static unsigned int str_hash(const WCHAR *s)
{
    unsigned int h = 2166136261u;
    while (*s) { h ^= (unsigned int)(*s++); h *= 16777619u; }
    return h;
}

/* 以 SS 倍字号把字符串画进临时 DIB，返回降采样后的覆盖率位图 */
static GlyphMask *rasterize(FontSet *f, const WCHAR *s, int len, int px,
                            int kind, const WCHAR *face, unsigned int key)
{
    HFONT big = NULL, old;
    SIZE sz;
    int bigPx, sw, sh, ow, oh, ox, oy, i, j;
    unsigned char *bits = (unsigned char *)f->tmpBits;
    GlyphMask *m;

    for (i = 0; i < f->cacheN; i++) {
        m = &f->cache[i];
        if (m->key == key && m->px == px && m->kind == kind) return m;
    }
    if (f->cacheN >= GLYPH_CACHE_MAX) {          /* 简单全清，够用 */
        for (i = 0; i < f->cacheN; i++) free(f->cache[i].cov);
        f->cacheN = 0;
    }

    bigPx = px * GLYPH_SS;
    {
        int wt = (kind == 1) ? FW_HEAVY :
                 (kind == 3) ? FW_NORMAL : FW_SEMIBOLD;
        big = CreateFontW(bigPx, 0, 0, 0,
                      wt,
                      FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                      OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                      ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                      face);
    }
    if (!big) return NULL;
    old = (HFONT)SelectObject(f->tmpDC, big);

    if (!GetTextExtentPoint32W(f->tmpDC, s, len, &sz)) {
        SelectObject(f->tmpDC, old); DeleteObject(big); return NULL;
    }
    sw = sz.cx + 8; sh = sz.cy + 8;
    if (sw > FONT_TMP_W) sw = FONT_TMP_W;
    if (sh > FONT_TMP_H) sh = FONT_TMP_H;

    for (oy = 0; oy < sh; oy++)
        memset(bits + ((size_t)oy * (size_t)FONT_TMP_W) * 4, 0,
               (size_t)sw * 4);

    SetBkMode(f->tmpDC, TRANSPARENT);
    SetTextColor(f->tmpDC, RGB(255, 255, 255));
    ExtTextOutW(f->tmpDC, 4, 4, 0, NULL, s, len, NULL);

    ow = (sw + GLYPH_SS - 1) / GLYPH_SS;
    oh = (sh + GLYPH_SS - 1) / GLYPH_SS;
    m = &f->cache[f->cacheN++];
    ZeroMemory(m, sizeof(*m));
    m->key = key; m->px = px; m->kind = kind;
    m->w = ow; m->h = oh;
    m->cov = (float *)calloc((size_t)ow * (size_t)oh, sizeof(float));
    if (!m->cov) {
        SelectObject(f->tmpDC, old); DeleteObject(big);
        f->cacheN--; return NULL;
    }
    for (oy = 0; oy < oh; oy++) {
        for (ox = 0; ox < ow; ox++) {
            double acc = 0.0;
            for (j = 0; j < GLYPH_SS; j++) {
                int sy = oy * GLYPH_SS + j;
                const unsigned char *row;
                if (sy >= sh) continue;
                row = bits + ((size_t)sy * (size_t)FONT_TMP_W) * 4;
                for (i = 0; i < GLYPH_SS; i++) {
                    int sx = ox * GLYPH_SS + i;
                    const unsigned char *p;
                    if (sx >= sw) continue;
                    p = row + (size_t)sx * 4;
                    acc += (double)(p[0] + p[1] + p[2]) / (3.0 * 255.0);
                }
            }
            m->cov[(size_t)oy * (size_t)ow + (size_t)ox] =
                (float)(acc / (double)(GLYPH_SS * GLYPH_SS));
        }
    }
    SelectObject(f->tmpDC, old);
    DeleteObject(big);
    return m;
}

/* 自检：FA 字形是否真的被选中。
 * 用官方码点 f04b(play) 渲染，统计中心区域覆盖率：
 * fallback 字体在 PUA 上通常给空白或空心方框（中心几乎为空），
 * 而真实 play 是实心三角，中心有明显覆盖。 */
static int probe_face(FontSet *f, const WCHAR *face)
{
    WCHAR ch[2];
    HFONT hf, old;
    SIZE sz;
    unsigned char *bits = (unsigned char *)f->tmpBits;
    int x, y, lit = 0, center = 0, total = 0;
    int sw, sh;

    ch[0] = (WCHAR)FA_PLAY; ch[1] = 0;
    hf = CreateFontW(32 * GLYPH_SS, 0, 0, 0, FW_HEAVY, FALSE, FALSE, FALSE,
                     DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                     ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, face);
    if (!hf) return 0;
    old = (HFONT)SelectObject(f->tmpDC, hf);
    if (!GetTextExtentPoint32W(f->tmpDC, ch, 1, &sz)) {
        SelectObject(f->tmpDC, old); DeleteObject(hf); return 0;
    }
    sw = sz.cx + 8; sh = sz.cy + 8;
    if (sw > FONT_TMP_W) sw = FONT_TMP_W;
    if (sh > FONT_TMP_H) sh = FONT_TMP_H;
    for (y = 0; y < sh; y++)
        memset(bits + ((size_t)y * (size_t)FONT_TMP_W) * 4, 0, (size_t)sw * 4);
    SetBkMode(f->tmpDC, TRANSPARENT);
    SetTextColor(f->tmpDC, RGB(255, 255, 255));
    ExtTextOutW(f->tmpDC, 4, 4, 0, NULL, ch, 1, NULL);

    for (y = 0; y < sh; y++)
        for (x = 0; x < sw; x++) {
            const unsigned char *p = bits +
                ((size_t)y * (size_t)FONT_TMP_W + (size_t)x) * 4;
            int v = (p[0] + p[1] + p[2]) / 3;
            total++;
            if (v > 40) {
                lit++;
                if (x > sw / 3 && x < sw * 2 / 3 &&
                    y > sh / 3 && y < sh * 2 / 3) center++;
            }
        }
    SelectObject(f->tmpDC, old);
    DeleteObject(hf);
    return (lit > total / 40) && (center > lit / 8);
}

int font_init(FontSet *f, double u)
{
    static const WCHAR *cands[2] = { FA_FACE_NAMEID1, FA_FACE_TYPO };
    ZeroMemory(f, sizeof(*f));
    f->u = u;

    f->uiClock = make_ui_font((int)(20 * u + 0.5));
    f->uiSmall = make_ui_font((int)(12 * u + 0.5));
    f->uiMid   = make_ui_font((int)(16 * u + 0.5));
    f->uiBig   = make_ui_font((int)(20 * u + 0.5));
    if (!make_dib(FONT_TMP_W, FONT_TMP_H, &f->tmpDC, &f->tmpBmp, &f->tmpBits))
        return 0;
    if (!f->uiClock || !f->uiSmall || !f->uiMid || !f->uiBig) return 0;

    build_path(f->fontPath, MAX_PATH);
    f->loaded = AddFontResourceExW(f->fontPath, FR_PRIVATE, 0);
    wcscpy(f->face, FA_FACE_NAMEID1);

    if (f->loaded > 0) {
        for (int i = 0; i < 2; i++) {
            if (probe_face(f, cands[i])) {
                wcscpy(f->face, cands[i]);
                f->faOK = 1;
                break;
            }
        }
    }
    return 1;
}

void font_shutdown(FontSet *f)
{
    for (int i = 0; i < f->cacheN; i++) free(f->cache[i].cov);
    f->cacheN = 0;
    if (f->uiClock) DeleteObject(f->uiClock);
    if (f->uiSmall) DeleteObject(f->uiSmall);
    if (f->uiMid)   DeleteObject(f->uiMid);
    if (f->uiBig)   DeleteObject(f->uiBig);
    if (f->measureFont) { DeleteObject(f->measureFont); f->measureFont = NULL; }
    if (f->tmpBmp) DeleteObject(f->tmpBmp);
    if (f->tmpDC)  DeleteDC(f->tmpDC);
    if (f->loaded > 0)
        RemoveFontResourceExW(f->fontPath, FR_PRIVATE, 0);
}

static void blit_mask(FontSet *f, Canvas *c, GlyphMask *m,
                      double x, double y, int align,
                      int cr, int cg, int cb, double k)
{
    int dx = (int)(x + 0.5), dy = (int)(y + 0.5);
    if (align == 1) dx -= m->w / 2;
    else if (align == 2) dx -= m->w;
    for (int oy = 0; oy < m->h; oy++)
        for (int ox = 0; ox < m->w; ox++) {
            double cov = (double)m->cov[(size_t)oy * (size_t)m->w + (size_t)ox];
            if (cov > 0.0)
                canvas_blend_px(c, dx + ox, dy + oy, cr, cg, cb, cov * k);
        }
}

void font_draw_icon(FontSet *f, Canvas *c, unsigned int cp, int px,
                    double x, double y, int align,
                    int cr, int cg, int cb, double k)
{
    WCHAR ch[2];
    GlyphMask *m;
    if (k <= 0.002 || !f->faOK) return;
    ch[0] = (WCHAR)cp; ch[1] = 0;
    m = rasterize(f, ch, 1, px, 1, f->face, cp);
    if (m) blit_mask(f, c, m, x, y - (double)m->h * 0.5,
                     align, cr, cg, cb, k);
}

static void draw_text_face(FontSet *f, Canvas *c, const WCHAR *s, int px,
                           int kind, const WCHAR *face,
                           double x, double y, int align,
                           int cr, int cg, int cb, double k)
{
    GlyphMask *m;
    int len;
    unsigned int key;
    if (k <= 0.002 || !s || !s[0]) return;
    len = (int)wcslen(s);
    key = str_hash(s) ^ ((unsigned int)kind << 24);
    m = rasterize(f, s, len, px, kind, face, key);
    if (m) blit_mask(f, c, m, x, y - (double)m->h * 0.5,
                     align, cr, cg, cb, k);
}

void font_draw_text(FontSet *f, Canvas *c, const WCHAR *s, int px,
                    double x, double y, int align,
                    int cr, int cg, int cb, double k)
{
    draw_text_face(f, c, s, px, 0, FONT_FACE, x, y, align, cr, cg, cb, k);
}

void font_draw_label(FontSet *f, Canvas *c, const WCHAR *s, int px,
                     double x, double y, int align,
                     int cr, int cg, int cb, double k)
{
    draw_text_face(f, c, s, px, 2, FONT_FACE_CJK, x, y, align, cr, cg, cb, k);
}

void font_draw_note(FontSet *f, Canvas *c, const WCHAR *s, int px,
                    double x, double y, int align,
                    int cr, int cg, int cb, double k)
{
    draw_text_face(f, c, s, px, 3, FONT_FACE_CJK, x, y, align, cr, cg, cb, k);
}

int font_measure(FontSet *f, const WCHAR *s, int len, int px, int kind)
{
    HFONT old;
    SIZE sz;
    int wt;
    if (!f->tmpDC || !s || len <= 0 || px < 1) return 0;
    wt = (kind == 1) ? FW_HEAVY : (kind == 3) ? FW_NORMAL : FW_SEMIBOLD;
    if (!f->measureFont || f->measurePx != px || f->measureKind != kind) {
        HFONT nf = CreateFontW(px, 0, 0, 0, wt,
                               FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                               OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                               ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                               (kind == 3) ? FONT_FACE_CJK : FONT_FACE);
        if (!nf) return 0;
        if (f->measureFont) DeleteObject(f->measureFont);
        f->measureFont = nf;
        f->measurePx = px; f->measureKind = kind;
    }
    old = (HFONT)SelectObject(f->tmpDC, f->measureFont);
    if (!GetTextExtentPoint32W(f->tmpDC, s, len, &sz)) sz.cx = 0;
    SelectObject(f->tmpDC, old);
    return sz.cx;
}

int font_fa_ok(const FontSet *f) { return f->faOK; }
const WCHAR *font_fa_face(const FontSet *f) { return f->face; }
