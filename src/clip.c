/* clip.c — 剪贴板历史 */
#include "clip.h"
#include "app.h"
#include "canvas.h"
#include "font.h"
#include "icons.h"
#include "cards.h"

#ifndef WM_CLIPBOARDUPDATE
#define WM_CLIPBOARDUPDATE 0x031D
#endif

#define CLIP_MAGIC   0x31504C51u   /* 'QLP1' */
#define CLIP_VERSION 1u
#define CLIP_POLL_MS 400
#define CLIP_SELF_MS 500

typedef BOOL (WINAPI *PFN_ADDLISTENER)(HWND);
typedef BOOL (WINAPI *PFN_REMLISTENER)(HWND);

static HWND g_listenerHwnd = NULL;

static int clip_path(const Clip *c, WCHAR *out, size_t cap)
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
    _snwprintf(out, (int)cap, L"%s\\%s", app,
               (c && c->testFile) ? CLIP_TEST_FILE : CLIP_FILE);
    out[cap - 1] = 0;
    return 1;
}

void clip_save(Clip *c, int force)
{
    WCHAR path[MAX_PATH];
    HANDLE h;
    DWORD wrote = 0, magic = CLIP_MAGIC, ver = CLIP_VERSION, cnt, i;
    if (!force && !c->dirty) return;
    if (!clip_path(c, path, MAX_PATH)) return;
    h = CreateFileW(path, GENERIC_WRITE, FILE_SHARE_READ, NULL,
                    CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return;
    cnt = (DWORD)c->count;
    WriteFile(h, &magic, 4, &wrote, NULL);
    WriteFile(h, &ver, 4, &wrote, NULL);
    WriteFile(h, &cnt, 4, &wrote, NULL);
    for (i = 0; i < cnt; i++) {
        DWORD len = (DWORD)c->items[i].len;
        DWORD stamp = c->items[i].stamp;
        WriteFile(h, &len, 4, &wrote, NULL);
        if (len) WriteFile(h, c->items[i].text, len * sizeof(WCHAR), &wrote, NULL);
        WriteFile(h, &stamp, 4, &wrote, NULL);
    }
    CloseHandle(h);
    c->dirty = 0;
}

static void clip_load(Clip *c)
{
    WCHAR path[MAX_PATH];
    HANDLE h;
    DWORD got = 0, magic = 0, ver = 0, cnt = 0, i;
    if (!clip_path(c, path, MAX_PATH)) return;
    h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                    NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return;
    if (!ReadFile(h, &magic, 4, &got, NULL) || got != 4 || magic != CLIP_MAGIC) goto done;
    if (!ReadFile(h, &ver, 4, &got, NULL) || got != 4 || ver != CLIP_VERSION) goto done;
    if (!ReadFile(h, &cnt, 4, &got, NULL) || got != 4 || cnt > CLIP_MAX_ITEMS) goto done;
    for (i = 0; i < cnt; i++) {
        DWORD len = 0, stamp = 0;
        if (!ReadFile(h, &len, 4, &got, NULL) || got != 4 || len > CLIP_MAX_CHARS) break;
        c->items[c->count].len = (int)len;
        if (len && (!ReadFile(h, c->items[c->count].text,
                              len * sizeof(WCHAR), &got, NULL) ||
                    got != len * sizeof(WCHAR))) {
            c->items[c->count].len = 0;
            break;
        }
        c->items[c->count].text[len] = 0;
        if (!ReadFile(h, &stamp, 4, &got, NULL) || got != 4) stamp = 0;
        c->items[c->count].stamp = stamp;
        c->count++;
    }
done:
    CloseHandle(h);
}

void clip_reload(Clip *c)
{
    ZeroMemory(c->items, sizeof(c->items));
    c->count = 0;
    c->top = 0;
    c->selected = -1;
    c->hoverRow = -1;
    c->sig = 0;
    c->dirty = 0;
    clip_load(c);
}

/* listener 注册 */
void clip_detach(Clip *c)
{
    PFN_REMLISTENER pRemove;
    if (c->detached || !c->u32) return;
    pRemove = (PFN_REMLISTENER)(void *)
        GetProcAddress(c->u32, "RemoveClipboardFormatListener");
    if (pRemove && c->listenerOK && g_listenerHwnd) pRemove(g_listenerHwnd);
    c->detached = 1;
}

void clip_init(Clip *c, App *a)
{
    PFN_ADDLISTENER pAdd;
    ZeroMemory(c, sizeof(*c));
    c->selected = -1;
    c->hoverRow = -1;
    clip_load(c);
    c->lastSeq = GetClipboardSequenceNumber();

    c->u32 = LoadLibraryW(L"user32.dll");
    if (!c->u32) return;
    pAdd = (PFN_ADDLISTENER)(void *)
        GetProcAddress(c->u32, "AddClipboardFormatListener");
    if (!pAdd) return;
    g_listenerHwnd = a->hwnd;
    c->listenerOK = pAdd(a->hwnd) ? 1 : 0;
}

void clip_shutdown(Clip *c, App *a)
{
    (void)a;
    if (!c->detached && c->u32 && c->listenerOK && g_listenerHwnd) {
        PFN_REMLISTENER pRemove = (PFN_REMLISTENER)(void *)
            GetProcAddress(c->u32, "RemoveClipboardFormatListener");
        if (pRemove) pRemove(g_listenerHwnd);
    }
    c->detached = 1;
    if (c->dirty) clip_save(c, 1);
    if (c->u32) { FreeLibrary(c->u32); c->u32 = NULL; }
}

int clip_read_text(HWND hwnd, WCHAR *out, int cap)
{
    HANDLE h;
    const WCHAR *p;
    SIZE_T bytes;
    int maxc, n = 0, retry;

    if (!IsClipboardFormatAvailable(CF_UNICODETEXT)) return 0;
    for (retry = 0; retry < 5; retry++) {
        if (OpenClipboard(hwnd)) break;
        Sleep(10);
    }
    if (retry >= 5) return 0;
    h = GetClipboardData(CF_UNICODETEXT);
    if (h) {
        p = (const WCHAR *)GlobalLock(h);
        if (p) {
            bytes = GlobalSize(h);
            maxc = (int)(bytes / sizeof(WCHAR));
            while (n < maxc && p[n] != 0) n++;
            if (n >= cap) n = cap - 1;
            if (n > 0) memcpy(out, p, (size_t)n * sizeof(WCHAR));
            out[n] = 0;
            GlobalUnlock(h);
        }
    }
    CloseClipboard();
    return n;
}

int clip_set_clipboard(Clip *c, App *a, const WCHAR *s, int len)
{
    HGLOBAL h;
    WCHAR *p;
    int ok = 0, retry;

    if (!s || len <= 0) return 0;
    if (len > CLIP_MAX_CHARS) len = CLIP_MAX_CHARS;
    c->selfWrite = 1;
    for (retry = 0; retry < 5; retry++) {
        if (OpenClipboard(a->hwnd)) break;
        Sleep(10);
    }
    if (retry < 5) {
        EmptyClipboard();
        h = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT,
                        (SIZE_T)(len + 1) * sizeof(WCHAR));
        if (h) {
            p = (WCHAR *)GlobalLock(h);
            if (p) {
                memcpy(p, s, (size_t)len * sizeof(WCHAR));
                p[len] = 0;
                GlobalUnlock(h);
                if (SetClipboardData(CF_UNICODETEXT, h) != NULL) {
                    ok = 1;
                } else {
                    GlobalFree(h); 
                }
            } else GlobalFree(h);
        }
        CloseClipboard();
    }
    c->writeOK = ok;
    c->lastSeq = GetClipboardSequenceNumber();
    if (ok) {
        memcpy(c->lastWritten, s, (size_t)len * sizeof(WCHAR));
        c->lastWritten[len] = 0;
        c->lastWrittenLen = len;
        c->lastWrittenAt = qs_now();
    }
    c->selfWrite = 0;
    return ok;
}

/* 插入 / 去重 */
static int only_space(const WCHAR *s, int len)
{
    int i;
    for (i = 0; i < len; i++)
        if (s[i] != L' ' && s[i] != L'\t' && s[i] != L'\r' && s[i] != L'\n')
            return 0;
    return 1;
}

int clip_push_text(Clip *c, const WCHAR *s, int len)
{
    int i, j;
    if (!s || len <= 0) return 0;
    if (len > CLIP_MAX_CHARS) len = CLIP_MAX_CHARS;
    if (only_space(s, len)) return 0;
    c->captures++;

    if (c->count > 0 && c->items[0].len == len &&
        memcmp(c->items[0].text, s, (size_t)len * sizeof(WCHAR)) == 0) {
        c->dupSkipped++;
        return 0;
    }
    for (i = 1; i < c->count; i++) {
        if (c->items[i].len == len &&
            memcmp(c->items[i].text, s, (size_t)len * sizeof(WCHAR)) == 0) {
            ClipItem tmp = c->items[i];
            tmp.stamp = qs_now();
            for (j = i; j > 0; j--) c->items[j] = c->items[j - 1];
            c->items[0] = tmp;
            c->top = 0;
            c->sig = 0;
            c->dirty = 1;
            clip_save(c, 1);
            return 1;
        }
    }
    if (c->count < CLIP_MAX_ITEMS) c->count++;
    for (j = c->count - 1; j > 0; j--) c->items[j] = c->items[j - 1];
    c->items[0].len = len;
    memcpy(c->items[0].text, s, (size_t)len * sizeof(WCHAR));
    c->items[0].text[len] = 0;
    c->items[0].stamp = qs_now();
    c->top = 0;
    c->sig = 0;
    c->dirty = 1;
    clip_save(c, 1);
    return 1;
}

static void mark_dirty(App *a)
{
    if (a->anim.exp > 0.02 && cards_draw(a->ui.view) == DRAW_CLIP)
        a->dirty = 1;
}

void clip_on_update(Clip *c, App *a)
{
    WCHAR buf[CLIP_MAX_CHARS + 1];
    DWORD seq;
    int n;

    seq = GetClipboardSequenceNumber();
    if (seq && seq == c->lastSeq) return;
    c->lastSeq = seq;

    if (GetClipboardOwner() == a->hwnd) { c->selfSkipped++; return; }
    n = clip_read_text(a->hwnd, buf, CLIP_MAX_CHARS + 1);
    if (n <= 0) return;
    if (c->lastWrittenLen == n && c->lastWrittenAt &&
        qs_now() - c->lastWrittenAt < CLIP_SELF_MS &&
        memcmp(c->lastWritten, buf, (size_t)n * sizeof(WCHAR)) == 0) {
        c->selfSkipped++;
        return;
    }
    if (clip_push_text(c, buf, n)) mark_dirty(a);
}

void clip_poll(Clip *c, App *a)
{
    DWORD now = qs_now();
    if (now - c->lastPollAt < CLIP_POLL_MS) return;
    c->lastPollAt = now;
    if (c->listenerOK) return;             /* 有 listener 就不重复轮询 */
    {
        DWORD seq = GetClipboardSequenceNumber();
        if (seq && seq != c->lastSeq) { c->pollHits++; clip_on_update(c, a); }
    }
}

void clip_clear(Clip *c, App *a)
{
    c->count = 0;
    c->top = 0;
    c->selected = -1;
    c->hoverRow = -1;
    c->sig = 0;
    c->dirty = 1;
    clip_save(c, 1);
    if (a) a->dirty = 1;
}

static unsigned int clip_sig(const Clip *c, int px, double w)
{
    unsigned int h = 2166136261u;
    int i;
    h ^= (unsigned int)c->count; h *= 16777619u;
    h ^= (unsigned int)px;       h *= 16777619u;
    h ^= (unsigned int)(int)w;   h *= 16777619u;
    for (i = 0; i < c->count; i++) {
        int j;
        h ^= (unsigned int)c->items[i].len; h *= 16777619u;
        for (j = 0; j < c->items[i].len; j += 3) {
            h ^= (unsigned int)c->items[i].text[j]; h *= 16777619u;
        }
    }
    return h;
}

static void clip_layout(Clip *c, App *a, ClipBox box)
{
    FontSet *f = &a->fonts;
    unsigned int sig;
    int i, j, maxW, ellW;
    double lineH;

    c->px = qs_iclamp((int)(box.h * CLIP_FONT_RATIO),
                      CLIP_FONT_MIN, CLIP_FONT_MAX);
    lineH = (double)c->px * CLIP_LINE_GAP;
    c->lineH = (int)(lineH + 0.5);
    if (c->lineH < 8) c->lineH = 8;
    c->visRows = (int)((box.h - 2.0 * CLIP_PAD) / (double)c->lineH);
    if (c->visRows < 1) c->visRows = 1;
    if (c->visRows > CLIP_MAX_ITEMS) c->visRows = CLIP_MAX_ITEMS;

    c->areaX = box.x + CLIP_PAD;
    c->areaY = box.y + CLIP_PAD;
    c->areaW = box.w - 2.0 * CLIP_PAD;
    c->areaH = box.h - 2.0 * CLIP_PAD;

    sig = clip_sig(c, c->px, c->areaW);
    if (sig == c->sig) return;
    c->sig = sig;

    maxW = (int)(c->areaW - CLIP_SCROLL_W - CLIP_CHECK_W);
    if (maxW < 40) maxW = 40;
    ellW = font_measure(f, L"\x2026", 1, c->px, 3);   /* … */

    for (i = 0; i < c->count; i++) {
        WCHAR flat[CLIP_MAX_CHARS + 1];
        int m = 0, w;
        const WCHAR *src = c->items[i].text;
        int len = c->items[i].len;
        for (j = 0; j < len && m < CLIP_MAX_CHARS; j++) {
            WCHAR ch = src[j];
            flat[m++] = (ch == L'\r' || ch == L'\n' || ch == L'\t') ? L' ' : ch;
        }
        flat[m] = 0;
        w = font_measure(f, flat, m, c->px, 3);
        if (w <= maxW) {
            int cp = m < CLIP_LINE_CHARS ? m : CLIP_LINE_CHARS;
            memcpy(c->line[i], flat, (size_t)cp * sizeof(WCHAR));
            c->line[i][cp] = 0;
            c->lineLen[i] = cp;
        } else {
            int lo = 0, hi = m, best = 0, room = maxW - ellW;
            if (room < 0) room = 0;
            while (lo <= hi) {
                int mid = (lo + hi) / 2;
                if (font_measure(f, flat, mid, c->px, 3) <= room) {
                    best = mid; lo = mid + 1;
                } else hi = mid - 1;
            }
            if (best > CLIP_LINE_CHARS - 1) best = CLIP_LINE_CHARS - 1;
            memcpy(c->line[i], flat, (size_t)best * sizeof(WCHAR));
            c->line[i][best] = L'\x2026';
            c->line[i][best + 1] = 0;
            c->lineLen[i] = best + 1;
        }
    }
}

void clip_scroll(Clip *c, int rows)
{
    int maxTop = c->count - c->visRows;
    if (maxTop < 0) maxTop = 0;
    c->top = qs_iclamp(c->top + rows, 0, maxTop);
}

int clip_hover_row(Clip *c, ClipBox box, int x, int y)
{
    int row = -1;
    if (x >= box.x && x < box.x + box.w &&
        y >= box.y && y < box.y + box.h && c->lineH > 0) {
        row = (int)((y - box.y - CLIP_PAD) / (double)c->lineH);
        if (row < 0 || row >= c->visRows || c->top + row >= c->count) row = -1;
    }
    c->hoverRow = row;
    return row;
}

void clip_click(Clip *c, App *a, ClipBox box, int x, int y)
{
    int row = clip_hover_row(c, box, x, y);
    int idx;
    if (row < 0) return;
    idx = c->top + row;
    if (idx < 0 || idx >= c->count) return;
    c->selected = idx;
    clip_set_clipboard(c, a, c->items[idx].text, c->items[idx].len);
    a->dirty = 1;
}

/* 绘制 */
void clip_draw(Clip *c, App *a, ClipBox box, double k)
{
    Canvas *cv = &a->canvas;
    int r, end, idx;
    double tx, ty, trackX, trackY, trackH, thumbH, thumbY;
    
    double centerX = (double)a->canvas.W * 0.5;

    clip_layout(c, a, box);
    tx = c->areaX;
    ty = c->areaY;

    canvas_set_clip(cv, (int)(box.x + 2.0), (int)ty,
                    (int)(box.x + box.w - CLIP_PAD),
                    (int)(box.y + box.h - CLIP_PAD));

    if (c->count == 0) {
        double icx = centerX, icy = ty + c->lineH * 0.5;
        if (font_fa_ok(&a->fonts))
            font_draw_icon(&a->fonts, cv, FA_CLIPBOARD,
                           qs_iclamp((int)(c->px * 0.9), 16, 26), icx, icy, 1,
                           DIM_R, DIM_G, DIM_B, k * 0.42);
        else
            canvas_disc(cv, icx, icy, 4.0, DIM_R, DIM_G, DIM_B, k * 0.42);
    }

    end = c->top + c->visRows;
    if (end > c->count) end = c->count;
    for (r = 0; r < c->visRows && c->top + r < end; r++) {
        idx = c->top + r;
        double yy = ty + r * c->lineH;
        int lit = (idx == c->selected);
        int hov = (r == c->hoverRow);
        if (lit || hov) {
            canvas_round(cv, tx - 3.0, yy + 2.0,
                         c->areaW - CLIP_SCROLL_W + 6.0,
                         (double)c->lineH - 4.0, 5.0,
                         HOVER_BG_R, HOVER_BG_G, HOVER_BG_B,
                         k * (lit ? 0.80 : 0.45));
        }
        if (c->lineLen[idx] > 0)
            font_draw_note(&a->fonts, cv, c->line[idx], c->px,
                           centerX, yy + c->lineH * 0.5, 1,
                           lit ? ACCENT_R : INK_R,
                           lit ? ACCENT_G : INK_G,
                           lit ? ACCENT_B : INK_B,
                           k * (lit ? 1.0 : 0.92));
        if (lit && font_fa_ok(&a->fonts)) {
            font_draw_icon(&a->fonts, cv, FA_CHECK,
                           qs_iclamp((int)(c->px * 0.72), 12, 22),
                           box.x + box.w - CLIP_PAD - CLIP_CHECK_W * 0.5,
                           yy + c->lineH * 0.5, 1,
                           ACCENT_R, ACCENT_G, ACCENT_B, k * 0.85);
        }
    }
    canvas_reset_clip(cv);

    if (c->count <= c->visRows) return;
    trackX = box.x + box.w - CLIP_PAD - 2.0;
    trackY = ty + 2.0;
    trackH = box.h - 2.0 * CLIP_PAD - 4.0;
    thumbH = qs_clamp(trackH * (double)c->visRows / (double)c->count,
                      16.0, 46.0);
    if (thumbH > trackH) thumbH = trackH;
    {
        int maxTop = c->count - c->visRows;
        double p = maxTop > 0 ? (double)c->top / (double)maxTop : 0.0;
        thumbY = trackY + (trackH - thumbH) * p;
    }
    canvas_round(cv, trackX - 0.5, thumbY, 3.0, thumbH, 1.5,
                 NOTE_THUMB_R, NOTE_THUMB_G, NOTE_THUMB_B, k * 0.55);
}

/* 诊断 */
int clip_count(const Clip *c) { return c->count; }
int clip_listener(const Clip *c) { return c->listenerOK; }
int clip_captures(const Clip *c) { return c->captures; }
int clip_dups(const Clip *c) { return c->dupSkipped; }
int clip_self_skipped(const Clip *c) { return c->selfSkipped; }
int clip_write_ok(const Clip *c) { return c->writeOK; }
int clip_vis_rows(const Clip *c) { return c->visRows; }
int clip_overflow(const Clip *c) { return c->count > c->visRows; }
const WCHAR *clip_at(const Clip *c, int idx)
{
    if (idx < 0 || idx >= c->count) return L"";
    return c->items[idx].text;
}
