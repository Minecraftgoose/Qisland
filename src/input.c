/* input.c — 便签输入：焦点、键盘、IME */
#include "input.h"
#include "app.h"
#include "config.h"
#include "note.h"
#include "ui.h"

static void changed(App *a)
{
    a->dirty = 1;
    note_save(&a->note, 0);
}

static void ime_pos(App *a)
{
    HIMC himc;
    HWND hwnd = a->hwnd;
    int x, y, visible;
    POINT pt;
    NoteBox box;

    if (!hwnd) return;
    ui_note_box(a, &box);
    visible = note_caret_xy(&a->note, a, box, &x, &y);
    if (!visible) { x = (int)box.x; y = (int)box.y; }
    pt.x = x; pt.y = y;
    ClientToScreen(hwnd, &pt);

    himc = ImmGetContext(hwnd);
    if (himc) {
        COMPOSITIONFORM cf;
        CANDIDATEFORM cd;
        ZeroMemory(&cf, sizeof(cf));
        cf.dwStyle = CFS_POINT;
        cf.ptCurrentPos = pt;
        ImmSetCompositionWindow(himc, &cf);
        ZeroMemory(&cd, sizeof(cd));
        cd.dwIndex = 0;
        cd.dwStyle = CFS_CANDIDATEPOS;
        cd.ptCurrentPos = pt;
        ImmSetCandidateWindow(himc, &cd);
        ImmReleaseContext(hwnd, himc);
    }
}

static void restore_noactivate(App *a)
{
    HWND hwnd = a->hwnd;
    LONG ex;
    if (!hwnd) return;
    ex = (LONG)GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    SetWindowLongPtrW(hwnd, GWL_EXSTYLE, (LONG_PTR)(ex | WS_EX_NOACTIVATE));
    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_FRAMECHANGED);
}

void input_init(App *a) { (void)a; }

void input_shutdown(App *a)
{
    if (a->note.editing) input_end(a);
}

int input_active(const App *a) { return a->note.editing; }

void input_begin(App *a)
{
    HWND hwnd = a->hwnd;
    LONG ex;
    if (!hwnd || a->ui.view != NOTE_VIEW || a->note.editing) return;
    ex = (LONG)GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    SetWindowLongPtrW(hwnd, GWL_EXSTYLE,
                      (LONG_PTR)(ex & ~(LONG)WS_EX_NOACTIVATE));
    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    SetForegroundWindow(hwnd);
    SetFocus(hwnd);
    note_set_editing(&a->note, 1);
    a->dirty = 1;
    ime_pos(a);
}

void input_end(App *a)
{
    if (!a->note.editing) return;
    note_set_editing(&a->note, 0);
    if (a->hwnd) SetFocus(NULL);
    restore_noactivate(a);
    a->dirty = 1;
}

static void paste(App *a)
{
    HGLOBAL h;
    WCHAR *p;
    if (!OpenClipboard(a->hwnd)) return;
    h = GetClipboardData(CF_UNICODETEXT);
    if (h) {
        p = (WCHAR *)GlobalLock(h);
        if (p) {
            int n = (int)wcslen(p);
            if (n > NOTE_MAX) n = NOTE_MAX;
            note_paste(&a->note, p, n);
            changed(a);
            GlobalUnlock(h);
        }
    }
    CloseClipboard();
}

int input_message(App *a, HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (!a->note.editing) return 0;

    switch (msg) {
    case WM_KILLFOCUS:
        input_end(a);
        return 0;

    case WM_IME_STARTCOMPOSITION:
        ime_pos(a);
        return 0;

    case WM_IME_COMPOSITION: {
        HIMC himc;
        LONG bytes;
        WCHAR buf[512];
        if (!(lp & GCS_RESULTSTR)) { ime_pos(a); return 0; }
        himc = ImmGetContext(hwnd);
        if (!himc) return 1;
        bytes = ImmGetCompositionStringW(himc, GCS_RESULTSTR, buf, sizeof(buf));
        ImmReleaseContext(hwnd, himc);
        if (bytes > 0) {
            int n = (int)bytes / (int)sizeof(WCHAR);
            if (n > 512) n = 512;
            note_insert(&a->note, buf, n);
            changed(a);
            ime_pos(a);
        }
        return 1;                     
    }
    case WM_IME_ENDCOMPOSITION:
        return 0;
    case WM_IME_CHAR:
        return 1;

    case WM_CHAR: {
        WCHAR ch = (WCHAR)wp;
        if (ch == 0x08)      note_backspace(&a->note);
        else if (ch == 0x0D) note_newline(&a->note);
        else if (ch == 0x1B) { input_end(a); return 1; }
        else if (ch >= 0x20 && ch < 0x80) note_insert(&a->note, &ch, 1);
        else return 0;
        changed(a);
        ime_pos(a);
        return 1;
    }

    case WM_KEYDOWN: {
        int ctrl  = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        int shift = (GetKeyState(VK_SHIFT)   & 0x8000) != 0;
        int sk    = shift ? NK_SHIFT : 0;
        int v = (int)wp;
        if (v == VK_ESCAPE) { input_end(a); return 1; }
        if (ctrl) {
            switch (v) {
            case 'A': note_select_all(&a->note); a->dirty = 1; ime_pos(a); return 1;
            case 'C': note_copy_selection(&a->note, 0); a->dirty = 1; return 1;
            case 'X': note_copy_selection(&a->note, 1); changed(a); ime_pos(a); return 1;
            case 'V': paste(a); ime_pos(a); return 1;
            case 'Z': note_key(&a->note, NK_UNDO); changed(a); ime_pos(a); return 1;
            case 'Y': note_key(&a->note, NK_REDO); changed(a); ime_pos(a); return 1;
            case VK_LEFT:  note_key(&a->note, NK_WORDLEFT  | sk); a->dirty = 1; ime_pos(a); return 1;
            case VK_RIGHT: note_key(&a->note, NK_WORDRIGHT | sk); a->dirty = 1; ime_pos(a); return 1;
            case VK_HOME:  note_key(&a->note, NK_TEXTSTART  | sk); a->dirty = 1; ime_pos(a); return 1;
            case VK_END:   note_key(&a->note, NK_TEXTEND    | sk); a->dirty = 1; ime_pos(a); return 1;
            default: return 0;
            }
        }
        switch (v) {
        case VK_LEFT:  note_key(&a->note, NK_LEFT  | sk); a->dirty = 1; ime_pos(a); return 1;
        case VK_RIGHT: note_key(&a->note, NK_RIGHT | sk); a->dirty = 1; ime_pos(a); return 1;
        case VK_UP:    note_key(&a->note, NK_UP    | sk); a->dirty = 1; ime_pos(a); return 1;
        case VK_DOWN:  note_key(&a->note, NK_DOWN  | sk); a->dirty = 1; ime_pos(a); return 1;
        case VK_HOME:  note_key(&a->note, NK_LINESTART | sk); a->dirty = 1; ime_pos(a); return 1;
        case VK_END:   note_key(&a->note, NK_LINEEND   | sk); a->dirty = 1; ime_pos(a); return 1;
        case VK_PRIOR: note_key(&a->note, NK_PGUP | sk); a->dirty = 1; ime_pos(a); return 1;
        case VK_NEXT:  note_key(&a->note, NK_PGDN | sk); a->dirty = 1; ime_pos(a); return 1;
        case VK_DELETE: note_delete(&a->note); changed(a); ime_pos(a); return 1;
        default: return 0;         /* Backspace / Enter 交给 WM_CHAR */
        }
    }
    default:
        return 0;
    }
}
