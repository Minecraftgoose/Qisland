/* note.c — 便签：stb_textedit 编辑内核 + 自绘换行 / 选中高亮 / 持久化
 *
 * 为什么用库：
 *   UpdateLayeredWindow 的分层窗口把整块 alpha 位图交给系统合成，所有绘制
 *   都必须在内存 DC 完成；MSDN 与实测都确认「父窗口上的 WS_CHILD 子控件
 *   不会被合成，直接不可见」。所以 EDIT / RichEdit / Scintilla 这类真控件
 *   放上来就是一片空白，选区高亮更是无从谈起。
 *   改用 stb_textedit.h v1.16（public domain，无窗口、无渲染、不做分配）：
 *   它只管 cursor / select_start / select_end / undo，换行与字宽由本文件回调
 *   提供，选中矩形由 canvas 画进 alpha 画布 —— 既有库的标准选区交互，又保留
 *   圆角裁切、4x4 字形抗锯齿和「不突兀」的观感。
 */
#include "note.h"
#include "app.h"
#include "canvas.h"
#include "config.h"
#include "font.h"
#include "icons.h"

/* ---------------- stb_textedit 宿主适配（仅本编译单元） ---------------- */
#define STB_TEXTEDIT_STRING               Note
#define STB_TEXTEDIT_STRINGLEN(obj)       ((obj)->len)
#define STB_TEXTEDIT_GETCHAR(obj, i)      ((obj)->text[i])
#define STB_TEXTEDIT_NEWLINE              L'\n'
#define STB_TEXTEDIT_IS_SPACE(ch)         ((ch) == L' ' || (ch) == L'\t' || (ch) == L'\n')
#define STB_TEXTEDIT_GETWIDTH_NEWLINE     0.0f

/* UTF-16 代理对：光标/选区不允许落在 emoji 两个 code unit 中间 */
#define NOTE_IS_HIGH(ch) ((ch) >= 0xD800 && (ch) <= 0xDBFF)
#define NOTE_IS_LOW(ch)  ((ch) >= 0xDC00 && (ch) <= 0xDFFF)
#define STB_TEXTEDIT_GETPREVCHARINDEX(obj, idx) \
    (((idx) > 1 && NOTE_IS_LOW((obj)->text[(idx) - 1])) ? (idx) - 2 : (idx) - 1)
#define STB_TEXTEDIT_GETNEXTCHARINDEX(obj, idx) \
    (((idx) + 1 < (obj)->len && NOTE_IS_HIGH((obj)->text[idx])) ? (idx) + 2 : (idx) + 1)

#define STB_TEXTEDIT_K_SHIFT        0x40000000
#define STB_TEXTEDIT_K_LEFT         0x01
#define STB_TEXTEDIT_K_RIGHT        0x02
#define STB_TEXTEDIT_K_UP           0x03
#define STB_TEXTEDIT_K_DOWN         0x04
#define STB_TEXTEDIT_K_LINESTART    0x05
#define STB_TEXTEDIT_K_LINEEND      0x06
#define STB_TEXTEDIT_K_TEXTSTART    0x07
#define STB_TEXTEDIT_K_TEXTEND      0x08
#define STB_TEXTEDIT_K_DELETE       0x09
#define STB_TEXTEDIT_K_BACKSPACE    0x0A
#define STB_TEXTEDIT_K_UNDO         0x0B
#define STB_TEXTEDIT_K_REDO         0x0C
#define STB_TEXTEDIT_K_PGUP         0x0D
#define STB_TEXTEDIT_K_PGDOWN       0x0E
#define STB_TEXTEDIT_K_WORDLEFT     0x0F
#define STB_TEXTEDIT_K_WORDRIGHT    0x10

static float note_stb_getwidth(Note *n, int rowStart, int i);
static void  note_stb_layoutrow(StbTexteditRow *r, Note *n, int start_i);
static int   note_stb_insert(Note *n, int pos, const WCHAR *s, int count);
static int   note_stb_delete(Note *n, int pos, int count);
static void  relayout(Note *n);
static void  note_sync_scroll(Note *n);

#define STB_TEXTEDIT_GETWIDTH(obj, n, i)     note_stb_getwidth((Note *)(obj), (n), (i))
#define STB_TEXTEDIT_LAYOUTROW(r, obj, n)    note_stb_layoutrow((r), (Note *)(obj), (n))
#define STB_TEXTEDIT_INSERTCHARS(obj, p, c, n) \
    note_stb_insert((Note *)(obj), (p), (const WCHAR *)(c), (n))
#define STB_TEXTEDIT_DELETECHARS(obj, p, n)  note_stb_delete((Note *)(obj), (p), (n))

/* v1.16 自身有两处「未使用」（find_charpos 的 z、未调用的 stb_textedit_char），
   只在包含它这一小段屏蔽，项目自身代码仍保持 -Wall -Wextra 全开。 */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-function"
#define STB_TEXTEDIT_IMPLEMENTATION
#include "stb_textedit.h"       /* 生成 static 编辑函数；header 部分已被 guard 跳过 */
#pragma GCC diagnostic pop

enum { OP_LEFT = 1, OP_RIGHT, OP_UP, OP_DOWN, OP_HOME, OP_END,
       OP_PGUP, OP_PGDN };

static unsigned int text_sig(const Note *n)
{
    unsigned int h = 2166136261u;
    int i;
    for (i = 0; i < n->len; i++) { h ^= (unsigned int)n->text[i]; h *= 16777619u; }
    return h ^ (unsigned int)(n->len << 3);
}

/* ---------------- 持久化：%APPDATA%\Qisland\note.txt（UTF-8，无 BOM） ---------------- */
static int note_path(WCHAR *out, size_t cap)
{
    WCHAR app[MAX_PATH];
    DWORD k = GetEnvironmentVariableW(L"APPDATA", app, MAX_PATH);
    if (!k || k >= MAX_PATH) {
        GetTempPathW(MAX_PATH, app);
        k = (DWORD)wcslen(app);
    }
    app[k] = 0;
    {
        WCHAR dir[MAX_PATH];
        _snwprintf(dir, MAX_PATH, L"%s\\Qisland", app);
        dir[MAX_PATH - 1] = 0;
        CreateDirectoryW(dir, NULL);
    }
    _snwprintf(out, (int)cap, L"%s\\%s", app, NOTE_FILE);
    out[cap - 1] = 0;
    return 1;
}

void note_init(Note *n)
{
    WCHAR path[MAX_PATH];
    HANDLE h;
    ZeroMemory(n, sizeof(*n));
    stb_textedit_initialize_state(&n->stb, 0);   /* 多行 */
    n->stb.row_count_per_page = 3;
    if (!note_path(path, MAX_PATH)) return;
    h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                    NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h != INVALID_HANDLE_VALUE) {
        char buf[NOTE_MAX * 3 + 8];
        DWORD got = 0;
        if (ReadFile(h, buf, sizeof(buf) - 4, &got, NULL) && got > 0) {
            int off = (got >= 3 && (unsigned char)buf[0] == 0xEF &&
                       (unsigned char)buf[1] == 0xBB &&
                       (unsigned char)buf[2] == 0xBF) ? 3 : 0;
            buf[got] = 0;
            int wlen = MultiByteToWideChar(CP_UTF8, 0, buf + off, -1,
                                           n->text, NOTE_MAX);
            if (wlen > 0) {
                int i, w = 0;
                for (i = 0; i < wlen - 1 && w < NOTE_MAX; i++) {
                    WCHAR ch = n->text[i];
                    if (ch == L'\r') continue;
                    n->text[w++] = ch;
                }
                n->text[w] = 0;
                n->len = w;
            }
        }
        CloseHandle(h);
    }
    n->text[n->len] = 0;
    n->lastSave = qs_now();
}

void note_save(Note *n, int force)
{
    WCHAR path[MAX_PATH];
    HANDLE h;
    char buf[NOTE_MAX * 3 + 8];
    DWORD wrote = 0;
    int bytes;
    if (!force && qs_now() - n->lastSave < 700) return;
    if (!n->dirty && !force) return;
    if (!note_path(path, MAX_PATH)) return;
    bytes = WideCharToMultiByte(CP_UTF8, 0, n->text, n->len,
                                buf, sizeof(buf) - 1, NULL, NULL);
    if (bytes < 0) return;
    buf[bytes] = 0;
    h = CreateFileW(path, GENERIC_WRITE, FILE_SHARE_READ, NULL,
                    CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return;
    WriteFile(h, buf, (DWORD)bytes, &wrote, NULL);
    CloseHandle(h);
    n->dirty = 0;
    n->lastSave = qs_now();
}

void note_shutdown(Note *n) { if (n->dirty) note_save(n, 1); }

void note_clear(Note *n)
{
    n->len = 0;
    n->text[0] = 0;
    n->stb.cursor = 0;
    n->stb.select_start = n->stb.select_end = 0;
    n->stb.has_preferred_x = 0;
    n->topLine = 0;
    n->sig = 0;
    n->dirty = 0;
}

/* ---------------- stb 回调 ---------------- */
static int note_stb_insert(Note *n, int pos, const WCHAR *s, int count)
{
    int room;
    if (!s || count <= 0) return 1;
    room = NOTE_MAX - n->len;
    if (room <= 0) return 0;
    if (count > room) count = room;
    if (pos < 0) pos = 0;
    if (pos > n->len) pos = n->len;
    memmove(n->text + pos + count, n->text + pos,
            (size_t)(n->len - pos + 1) * sizeof(WCHAR));
    memcpy(n->text + pos, s, (size_t)count * sizeof(WCHAR));
    n->len += count;
    n->text[n->len] = 0;
    n->dirty = 1; n->sig = 0;
    return 1;
}

static int note_stb_delete(Note *n, int pos, int count)
{
    if (pos < 0 || count <= 0 || pos >= n->len) return 0;
    if (pos + count > n->len) count = n->len - pos;
    memmove(n->text + pos, n->text + pos + count,
            (size_t)(n->len - pos - count + 1) * sizeof(WCHAR));
    n->len -= count;
    n->dirty = 1; n->sig = 0;
    return 1;
}

static float note_stb_getwidth(Note *n, int rowStart, int i)
{
    int idx = rowStart + i;
    FontSet *fonts = (FontSet *)n->fontPtr;
    if (!fonts || idx < 0 || idx >= n->len) return 0.0f;
    if (n->text[idx] == L'\n') return 0.0f;   /* == GETWIDTH_NEWLINE */
    return (float)font_measure(fonts, n->text + idx, 1, n->px, 3);
}

static int find_line(const Note *n, int idx)
{
    int L;
    if (n->lineCount <= 0) return 0;
    for (L = 0; L < n->lineCount; L++)
        if (idx <= n->lineEnd[L]) return L;
    return n->lineCount - 1;
}

/* 关键：stb 从全文第 0 个字符开始遍历（i += r.num_chars）。
   行末若有显式 '\n'，num_chars 必须把它算进去，否则下一行起点错位。
   y 几何从 0 起、每行 baseline_y_delta = lineH，滚动由调用方把
   topLine*lineH 加进鼠标 y 来对齐。 */
static void note_stb_layoutrow(StbTexteditRow *r, Note *n, int start_i)
{
    FontSet *fonts = (FontSet *)n->fontPtr;
    int L, s, e, hasNL;
    relayout(n);
    L = find_line(n, start_i);
    s = n->lineStart[L]; e = n->lineEnd[L];
    hasNL = (e < n->len && n->text[e] == L'\n');
    r->num_chars = (e - s) + (hasNL ? 1 : 0);
    r->x0 = 0.0f;
    r->x1 = fonts ? (float)font_measure(fonts, n->text + s, e - s, n->px, 3) : 1.0f;
    if (r->x1 < 1.0f) r->x1 = 1.0f;
    r->ymin = 0.0f;
    r->ymax = (float)n->lineH;
    r->baseline_y_delta = (float)n->lineH;
}

/* ---------------- 换行布局 ---------------- */
static int breakable_after(WCHAR ch)
{
    return ch == L'，' || ch == L'。' || ch == L'、' || ch == L'；' ||
           ch == L'：' || ch == L'！' || ch == L'？' || ch == L'…' ||
           ch == L'～' || ch == L'）' || ch == L'」' || ch == L'』' ||
           ch == L'”' || ch == L'’' || ch == L',' || ch == L'.' ||
           ch == L';' || ch == L':' || ch == L'!' || ch == L'?' ||
           ch == L' ' || ch == L'\t';
}

static int no_line_start(WCHAR ch)
{
    return (ch >= 0x3000 && ch <= 0x303F) ||
           (ch >= 0xFF01 && ch <= 0xFF0F) ||
           ch == L'，' || ch == L'。' || ch == L'、' || ch == L'；' ||
           ch == L'：' || ch == L'！' || ch == L'？' || ch == L'）' ||
           ch == L'」' || ch == L'』' || ch == L'”' || ch == L'’' ||
           ch == L'…' || ch == L'—' || ch == L'～';
}

static void relayout(Note *n)
{
    NoteBox box = n->box;
    FontSet *fonts = (FontSet *)n->fontPtr;
    double maxW;
    int i, w, lines = 0, vis;
    unsigned int sig;
    if (!fonts) return;

    n->areaX = box.x; n->areaY = box.y;
    n->areaW = box.w; n->areaH = box.h;
    n->px = qs_iclamp((int)(box.h * NOTE_FONT_RATIO + 0.5),
                      NOTE_FONT_MIN, NOTE_FONT_MAX);
    n->lineH = (int)(n->px * NOTE_LINE_GAP + 0.5);
    if (n->lineH < 12) n->lineH = 12;
    vis = (int)((box.h - 2.0 * NOTE_PAD) / (double)n->lineH);
    if (vis < 1) vis = 1;
    n->visLines = vis;
    n->stb.row_count_per_page = vis;
    maxW = box.w - 2.0 * NOTE_PAD - NOTE_SCROLL_W;
    if (maxW < 24.0) maxW = 24.0;

    sig = text_sig(n) ^ (unsigned int)(n->px << 12) ^
          (unsigned int)((int)maxW << 3) ^ (unsigned int)(vis << 20);
    if (sig == n->sig && n->lineCount > 0) return;
    n->sig = sig;

    i = 0; w = 0;
    while (lines < NOTE_MAX_LINES) {
        int start = i, lastBreak = -1;
        n->lineStart[lines] = start;
        w = 0;
        while (i < n->len && n->text[i] != L'\n') {
            WCHAR ch = n->text[i];
            int cw = font_measure(fonts, n->text + i, 1, n->px, 3);
            if (w + cw > maxW && i > start) {
                if (lastBreak > start) {
                    i = lastBreak;
                } else if (no_line_start(ch) && i > start + 1) {
                    w -= font_measure(fonts, n->text + i - 1, 1, n->px, 3);
                    i--;
                }
                break;
            }
            w += cw;
            i++;
            if (breakable_after(ch) && i < n->len && n->text[i] != L'\n')
                lastBreak = i;
        }
        n->lineEnd[lines] = i;
        lines++;
        if (i >= n->len) break;
        if (n->text[i] == L'\n') { i++; if (i >= n->len) {
            if (lines < NOTE_MAX_LINES) {
                n->lineStart[lines] = i; n->lineEnd[lines] = i; lines++;
            }
            break;
        } }
    }
    if (lines == 0) { n->lineStart[0] = 0; n->lineEnd[0] = 0; lines = 1; }
    n->lineCount = lines;
    stb_textedit_clamp(n, &n->stb);
    note_sync_scroll(n);
}

/* 光标 / 选区端点始终在可见范围 */
static void note_sync_scroll(Note *n)
{
    int cl = find_line(n, n->stb.cursor);
    int maxTop = n->lineCount - n->visLines;
    if (maxTop < 0) maxTop = 0;
    if (cl < n->topLine) n->topLine = cl;
    if (cl >= n->topLine + n->visLines) n->topLine = cl - n->visLines + 1;
    n->topLine = qs_iclamp(n->topLine, 0, maxTop);
}

/* ---------------- 对外编辑接口 ---------------- */
static void note_ctx(Note *n, const App *a, NoteBox box)
{
    n->fontPtr = (void *)&((App *)a)->fonts;
    n->box = box;
    relayout(n);
}

void note_insert(Note *n, const WCHAR *s, int count)
{
    if (!s || count <= 0) return;
    stb_textedit_text(n, &n->stb, s, count);   /* 有选区时自动替换 */
    note_sync_scroll(n);
}

void note_backspace(Note *n) { note_key(n, NK_BACKSPACE); }
void note_delete(Note *n)    { note_key(n, NK_DELETE); }
void note_newline(Note *n)   { WCHAR ch = L'\n'; note_insert(n, &ch, 1); }

static int to_stb_key(int k)
{
    int shift = (k & NK_SHIFT) ? STB_TEXTEDIT_K_SHIFT : 0;
    int base;
    k &= ~NK_SHIFT;
    switch (k) {
    case NK_LEFT:       base = STB_TEXTEDIT_K_LEFT; break;
    case NK_RIGHT:      base = STB_TEXTEDIT_K_RIGHT; break;
    case NK_UP:         base = STB_TEXTEDIT_K_UP; break;
    case NK_DOWN:       base = STB_TEXTEDIT_K_DOWN; break;
    case NK_LINESTART:  base = STB_TEXTEDIT_K_LINESTART; break;
    case NK_LINEEND:    base = STB_TEXTEDIT_K_LINEEND; break;
    case NK_TEXTSTART:  base = STB_TEXTEDIT_K_TEXTSTART; break;
    case NK_TEXTEND:    base = STB_TEXTEDIT_K_TEXTEND; break;
    case NK_DELETE:     base = STB_TEXTEDIT_K_DELETE; break;
    case NK_BACKSPACE:  base = STB_TEXTEDIT_K_BACKSPACE; break;
    case NK_UNDO:       base = STB_TEXTEDIT_K_UNDO; break;
    case NK_REDO:       base = STB_TEXTEDIT_K_REDO; break;
    case NK_PGUP:       base = STB_TEXTEDIT_K_PGUP; break;
    case NK_PGDN:       base = STB_TEXTEDIT_K_PGDOWN; break;
    case NK_WORDLEFT:   base = STB_TEXTEDIT_K_WORDLEFT; break;
    case NK_WORDRIGHT:  base = STB_TEXTEDIT_K_WORDRIGHT; break;
    default:            return -1;
    }
    return base | shift;
}

void note_key(Note *n, int key)
{
    int sk = to_stb_key(key);
    if (sk < 0) return;
    stb_textedit_key(n, &n->stb, sk);
    note_sync_scroll(n);
}

void note_move(Note *n, int op)
{
    static const int map[9] = { 0, NK_LEFT, NK_RIGHT, NK_UP, NK_DOWN,
                               NK_LINESTART, NK_LINEEND, NK_PGUP, NK_PGDN };
    if (op >= 1 && op <= 8) note_key(n, map[op]);
}

void note_select_all(Note *n)
{
    n->stb.select_start = 0;
    n->stb.select_end = n->len;
    n->stb.cursor = n->len;
    note_sync_scroll(n);
}

int  note_has_selection(const Note *n)
{
    return n->stb.select_start != n->stb.select_end;
}

void note_selection_range(const Note *n, int *a, int *b)
{
    int s = n->stb.select_start, e = n->stb.select_end;
    *a = s < e ? s : e;
    *b = s > e ? s : e;
}

int note_copy_selection(Note *n, int cut)
{
    int a, b, cnt;
    HGLOBAL h;
    WCHAR *p;
    if (!note_has_selection(n)) return 0;
    note_selection_range(n, &a, &b);
    cnt = b - a;
    if (cnt <= 0) return 0;
    if (!OpenClipboard(NULL)) return 0;
    EmptyClipboard();
    h = GlobalAlloc(GMEM_MOVEABLE, ((size_t)cnt + 1) * sizeof(WCHAR));
    if (h) {
        p = (WCHAR *)GlobalLock(h);
        if (p) {
            memcpy(p, n->text + a, (size_t)cnt * sizeof(WCHAR));
            p[cnt] = 0;
            GlobalUnlock(h);
            SetClipboardData(CF_UNICODETEXT, h);
        } else GlobalFree(h);
    }
    CloseClipboard();
    if (cut) stb_textedit_cut(n, &n->stb);
    return cnt;
}

void note_paste(Note *n, const WCHAR *s, int count)
{
    WCHAR buf[NOTE_MAX + 1];
    int i, w = 0;
    if (!s || count <= 0) return;
    if (count > NOTE_MAX) count = NOTE_MAX;
    for (i = 0; i < count && w < NOTE_MAX; i++) {
        if (s[i] == L'\r') continue;
        buf[w++] = (s[i] == L'\t') ? L' ' : s[i];
    }
    buf[w] = 0;
    stb_textedit_paste(n, &n->stb, buf, w);    /* 有选区时自动替换 */
    note_sync_scroll(n);
}

void note_scroll(Note *n, int lines)
{
    int maxTop = n->lineCount - n->visLines;
    if (maxTop < 0) maxTop = 0;
    n->topLine = qs_iclamp(n->topLine + lines, 0, maxTop);
}

void note_set_editing(Note *n, int on)
{
    n->editing = on;
    if (!on) {
        note_save(n, 1);
        /* 退出编辑时清掉选区，避免下次打开还留着一块高亮 */
        n->stb.select_start = n->stb.select_end = n->stb.cursor;
    }
}

/* ---------------- 鼠标：点击定位 / 拖动框选 ---------------- */
static void stb_coords(Note *n, NoteBox box, int x, int y, float *sx, float *sy)
{
    *sx = (float)(x - box.x - NOTE_PAD);
    /* stb 从全文第一行开始累加 baseline_y_delta，把已滚掉的行补回来 */
    *sy = (float)(y - box.y - NOTE_PAD) + (float)(n->topLine * n->lineH);
}

void note_click(Note *n, const App *a, NoteBox box, int x, int y)
{
    float sx, sy;
    note_ctx(n, a, box);
    stb_coords(n, box, x, y, &sx, &sy);
    stb_textedit_click(n, &n->stb, sx, sy);
    note_sync_scroll(n);
}

void note_drag_to(Note *n, const App *a, NoteBox box, int x, int y)
{
    float sx, sy;
    note_ctx(n, a, box);
    stb_coords(n, box, x, y, &sx, &sy);
    stb_textedit_drag(n, &n->stb, sx, sy);
    note_sync_scroll(n);
}

static int caret_line(const Note *n) { return find_line(n, n->stb.cursor); }

static int caret_col_width(const Note *n, const App *a, int line)
{
    int s = n->lineStart[line];
    int e = n->stb.cursor;
    if (e < s) e = s;
    if (e > n->lineEnd[line]) e = n->lineEnd[line];
    return font_measure(&((App *)a)->fonts, n->text + s, e - s, n->px, 3);
}

int note_caret_xy(Note *n, const App *a, NoteBox box, int *x, int *y)
{
    int line;
    note_ctx(n, a, box);
    line = caret_line(n);
    *x = (int)(box.x + NOTE_PAD + caret_col_width(n, a, line));
    *y = (int)(box.y + NOTE_PAD +
               (line - n->topLine) * n->lineH + n->lineH * 0.5);
    return line >= n->topLine && line < n->topLine + n->visLines;
}

int note_line_count(const Note *n) { return n->lineCount; }
int note_vis_lines(const Note *n)  { return n->visLines; }
int note_overflow(const Note *n)   { return n->lineCount > n->visLines; }

/* ---------------- 绘制 ---------------- */
static void draw_selection(Note *n, App *a, double tx, double ty, double k)
{
    Canvas *c = &a->canvas;
    FontSet *fonts = (FontSet *)n->fontPtr;
    int lo, hi, L, end;
    if (!fonts || !note_has_selection(n)) return;
    note_selection_range(n, &lo, &hi);
    if (lo < 0) lo = 0;
    if (hi > n->len) hi = n->len;
    end = n->topLine + n->visLines;
    if (end > n->lineCount) end = n->lineCount;
    for (L = n->topLine; L < end; L++) {
        int s = n->lineStart[L], e = n->lineEnd[L];
        int p = lo > s ? lo : s, q = hi < e ? hi : e;
        double x0, x1, yy;
        if (p >= q) continue;
        x0 = tx + font_measure(fonts, n->text + s, p - s, n->px, 3);
        x1 = tx + font_measure(fonts, n->text + s, q - s, n->px, 3);
        if (x1 - x0 < 1.0) x1 = x0 + 1.0;
        yy = ty + (L - n->topLine) * n->lineH;
        canvas_round(c, x0, yy + 2.0, x1 - x0,
                     (double)n->lineH - 4.0, 2.0,
                     NOTE_SEL_R, NOTE_SEL_G, NOTE_SEL_B, k * NOTE_SEL_K);
    }
}

void note_draw(Note *n, App *a, NoteBox box, double k)
{
    Canvas *c = &a->canvas;
    int L, end, cx, cy, vis;
    double tx, ty, trackX, trackY, trackH, thumbH, thumbY;
    WCHAR line[NOTE_MAX + 1];

    note_ctx(n, a, box);
    tx = box.x + NOTE_PAD;
    ty = box.y + NOTE_PAD;

    canvas_set_clip(c, (int)tx, (int)ty,
                    (int)(box.x + box.w - NOTE_PAD),
                    (int)(box.y + box.h - NOTE_PAD));

    /* 空态：只有一个极淡的 pencil 图标，没有任何提示文字、没有输入框边框 */
    if (n->len == 0) {
        double icx = tx + 10.0, icy = ty + n->lineH * 0.5;
        if (font_fa_ok(&a->fonts))
            font_draw_icon(&a->fonts, c, FA_PENCIL,
                           qs_iclamp((int)(n->px * 0.68), 14, 20), icx, icy, 1,
                           DIM_R, DIM_G, DIM_B, k * 0.42);
        else
            canvas_disc(c, icx, icy, 3.0, DIM_R, DIM_G, DIM_B, k * 0.42);
    }

    /* 选中指示先画，文字覆盖其上；只在编辑态出现，不编辑时不留高亮 */
    if (n->editing) draw_selection(n, a, tx, ty, k);

    end = n->topLine + n->visLines;
    if (end > n->lineCount) end = n->lineCount;
    for (L = n->topLine; L < end; L++) {
        int s = n->lineStart[L], e = n->lineEnd[L], cnt = e - s;
        if (cnt <= 0) continue;
        if (cnt > NOTE_MAX) cnt = NOTE_MAX;
        memcpy(line, n->text + s, (size_t)cnt * sizeof(WCHAR));
        line[cnt] = 0;
        font_draw_note(&a->fonts, c, line, n->px,
                       tx, ty + (L - n->topLine) * n->lineH +
                           n->lineH * 0.5,
                       0, INK_R, INK_G, INK_B, k * 0.92);
    }

    /* 光标：常显淡竖线，编辑态才画 */
    if (n->editing && note_caret_xy(n, a, box, &cx, &cy)) {
        canvas_round(c, (double)cx, (double)(cy - n->lineH * 0.34),
                     1.6, n->lineH * 0.68, 0.8,
                     INK_R, INK_G, INK_B, k * 0.72);
    }
    canvas_reset_clip(c);

    /* 美化滚动指示：没有系统滚动条，连整条轨道都不画，只有极淡短滑块 */
    if (!note_overflow(n)) return;
    vis = n->visLines;
    trackX = box.x + box.w - NOTE_PAD - 2.0;
    trackY = ty + 2.0;
    trackH = box.h - 2.0 * NOTE_PAD - 4.0;
    thumbH = qs_clamp(trackH * 0.22, 16.0, 34.0);
    if (thumbH > trackH) thumbH = trackH;
    {
        int maxTop = n->lineCount - vis;
        double p = maxTop > 0 ? (double)n->topLine / (double)maxTop : 0.0;
        thumbY = trackY + (trackH - thumbH) * p;
    }
    canvas_round(c, trackX - 0.5, thumbY, 3.0, thumbH, 1.5,
                 NOTE_THUMB_R, NOTE_THUMB_G, NOTE_THUMB_B, k * 0.55);
}
