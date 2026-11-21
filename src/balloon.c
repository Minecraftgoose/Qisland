/* balloon.c — Win7 托盘气泡消息监听器 */
#include "balloon.h"
#include "app.h"
#include "config.h"
#include "icons.h"
#include "notify.h"

/* 系统提示图标资源 ID：Win32 标准数值。 不直接用 IDI_* 宏，因为它在部分 SDK 里被定义成 MAKEINTRESOURCE 指针，赋给 int 会产生类型歧义。 */
#define BALLOON_IDI_INFORMATION 32516
#define BALLOON_IDI_WARNING     32515
#define BALLOON_IDI_ERROR       32513

/* 回调需要的极少量全局：不指向 App，不跨线程传字符串 */
static HWND  g_target;
static DWORD g_trayPid;

static int is_balloon_class(const WCHAR *cls)
{
    const WCHAR *p = cls;
    size_t n = wcslen(BALLOON_CLASS);
    while (*p) {
        if (!_wcsnicmp(p, BALLOON_CLASS, n)) return 1;
        p++;
    }
    return 0;
}

/* WinEvent 回调：只过滤，只 PostMessage */
static void CALLBACK balloon_win_event(HWINEVENTHOOK hook, DWORD event,
                                       HWND hwnd, LONG idObject, LONG idChild,
                                       DWORD idEventThread, DWORD msEventTime)
{
    WCHAR cls[96];
    LONG  style;
    DWORD pid = 0;

    (void)hook; (void)idEventThread; (void)msEventTime;
    if (!g_target || !hwnd || !IsWindow(hwnd)) return;
    
    if (idObject != OBJID_WINDOW || idChild != CHILDID_SELF) return;
    if (!GetClassNameW(hwnd, cls, 96) || !is_balloon_class(cls)) return;

    /* TTS_BALLOON = 0x40（官方 Tooltip Styles，版本 5.80）：把普通 tooltip 排除掉，只认托盘气泡的卡通气球外观。 */
    style = (LONG)GetWindowLongPtrW(hwnd, GWL_STYLE);
    if (!(style & BALLOON_TTS_BALLOON)) return;

    /* 气泡由 explorer 显示； 真机按 Shell_TrayWnd 的 pid 收窄，既降噪又防误报。 */
    if (g_trayPid) {
        GetWindowThreadProcessId(hwnd, &pid);
        if (pid != g_trayPid) return;
    }

    /* 只送 HWND，文本由主线程自己读：绝不跨线程传指针或字符串 */
    PostMessageW(g_target, WM_APP_BALLOON_SIGNAL, (WPARAM)event, (LPARAM)hwnd);
}

/* 主线程：读气泡文本 */
int balloon_read_text(HWND hwnd, WCHAR *out, int cap)
{
    DWORD_PTR n = 0, got = 0;
    LRESULT r;

    if (out && cap > 0) out[0] = 0;
    if (!out || cap <= 0 || !hwnd || !IsWindow(hwnd)) return 0;

    
    r = SendMessageTimeoutW(hwnd, WM_GETTEXTLENGTH, 0, 0,
                            SMTO_ABORTIFHUNG, BALLOON_SEND_TIMEOUT_MS, &n);
    if (r && n > 0 && n < 4096) {
        int cc = (int)n + 1;
        WCHAR *buf = (WCHAR *)malloc((size_t)cc * sizeof(WCHAR));
        if (buf) {
            buf[0] = 0;
            r = SendMessageTimeoutW(hwnd, WM_GETTEXT, (WPARAM)cc, (LPARAM)buf,
                                    SMTO_ABORTIFHUNG, BALLOON_SEND_TIMEOUT_MS,
                                    &got);
            if (r && buf[0]) {
                wcsncpy(out, buf, cap - 1);
                out[cap - 1] = 0;
            }
            free(buf);
        }
    }
    /* 路径 2：某些版本把 szInfoTitle 放在窗口文本里，GetWindowText 兜底 */
    if (!out[0] && GetWindowTextW(hwnd, out, cap) > 0) return 1;
    return out[0] ? 1 : 0;
}

int balloon_read_title(HWND hwnd, WCHAR *out, int cap, UINT *iconOut)
{
    DWORD pid = 0, i;
    HANDLE hProc = NULL;
    BYTE *remote = NULL;
    WCHAR *remoteBuf = NULL;
    SIZE_T allocSize, wrote = 0, got = 0;
    DWORD_PTR res = 0;
    BALLOON_TTGETTITLE tt, back;
    WCHAR buf[BALLOON_TITLE_MAX + 1];
    int ok = 0;

    if (out && cap > 0) out[0] = 0;
    if (iconOut) *iconOut = BALLOON_TTI_NONE;
    if (!out || cap <= 0 || !hwnd || !IsWindow(hwnd)) return 0;

    if (!GetWindowThreadProcessId(hwnd, &pid) || !pid) return 0;

    ZeroMemory(&tt, sizeof(tt));
    ZeroMemory(&back, sizeof(back));
    ZeroMemory(buf, sizeof(buf));
    tt.dwSize = (DWORD)sizeof(tt);
    tt.cch    = BALLOON_TITLE_MAX;
    tt.uTitleBitmap = BALLOON_TTI_NONE;

    if (pid == GetCurrentProcessId()) {
        tt.pszTitle = buf;
        SendMessageTimeoutW(hwnd, TTM_GETTITLE, 0, (LPARAM)&tt,
                            SMTO_ABORTIFHUNG, BALLOON_SEND_TIMEOUT_MS, &res);
        back = tt;
        if (iconOut) *iconOut = tt.uTitleBitmap;
    } else {
        hProc = OpenProcess(PROCESS_VM_OPERATION | PROCESS_VM_READ |
                            PROCESS_VM_WRITE, FALSE, pid);
        if (!hProc) return 0;
        allocSize = sizeof(tt) + sizeof(buf);
        remote = (BYTE *)VirtualAllocEx(hProc, NULL, allocSize,
                                        MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        if (!remote) { CloseHandle(hProc); return 0; }
        remoteBuf = (WCHAR *)(remote + sizeof(tt));
        tt.pszTitle = remoteBuf;
        if (!WriteProcessMemory(hProc, remote, &tt, sizeof(tt), &wrote) ||
            !WriteProcessMemory(hProc, remoteBuf, buf, sizeof(buf), &wrote))
            goto done;

        SendMessageTimeoutW(hwnd, TTM_GETTITLE, 0, (LPARAM)remote,
                            SMTO_ABORTIFHUNG, BALLOON_SEND_TIMEOUT_MS, &res);
        if (!ReadProcessMemory(hProc, remote, &back, sizeof(back), &got))
            goto done;
        if (iconOut) *iconOut = back.uTitleBitmap;

        /* 情形 A：控件把标题拷进我们给的缓冲 */
        ZeroMemory(buf, sizeof(buf));
        if (!ReadProcessMemory(hProc, remoteBuf, buf, sizeof(buf), &got))
            goto done;
        /* 情形 B：控件把 pszTitle 改成它自己内部的指针 */
        if (!buf[0] && back.pszTitle && back.pszTitle != remoteBuf) {
            SIZE_T n = (SIZE_T)back.cch;
            if (n > BALLOON_TITLE_MAX) n = BALLOON_TITLE_MAX;
            ZeroMemory(buf, sizeof(buf));
            if (!ReadProcessMemory(hProc, back.pszTitle, buf,
                                   n * sizeof(WCHAR), &got))
                goto done;
        }
    }
    buf[BALLOON_TITLE_MAX] = 0;
    if (!buf[0]) goto done;
    for (i = 0; buf[i]; i++)
        if (buf[i] == L'\r' || buf[i] == L'\n' || buf[i] == L'\t')
            buf[i] = L' ';
    wcsncpy(out, buf, cap - 1);
    out[cap - 1] = 0;
    ok = 1;

done:
    if (remote) VirtualFreeEx(hProc, remote, 0, MEM_RELEASE);
    if (hProc) CloseHandle(hProc);
    return ok;
}

static HICON balloon_title_hicon(UINT icon)
{
    int idi;
    switch (icon) {
    case BALLOON_TTI_INFO:
    case BALLOON_TTI_INFO_LARGE:     idi = BALLOON_IDI_INFORMATION; break;
    case BALLOON_TTI_WARNING:
    case BALLOON_TTI_WARNING_LARGE:  idi = BALLOON_IDI_WARNING;     break;
    case BALLOON_TTI_ERROR:
    case BALLOON_TTI_ERROR_LARGE:    idi = BALLOON_IDI_ERROR;       break;
    default: return NULL;
    }
    return LoadIconW(NULL, MAKEINTRESOURCEW(idi));
}

/* 主线程：信号 → 通知 */
void balloon_on_signal(App *a, DWORD event, HWND hwnd)
{
    Balloon *b = &a->balloon;
    WCHAR cls[96], text[BALLOON_TEXT_MAX + 1], title[BALLOON_TITLE_MAX + 1];
    LONG  style;
    DWORD now;
    UINT  titleIcon = BALLOON_TTI_NONE;
    HICON titleHicon = NULL;
    int i;

    (void)event;
    if (!b->enabled) return;
    b->events++;

    /* 二次校验：WinEvent 是异步排队，等主线程处理时窗口可能已销毁或复用 */
    if (!hwnd || !IsWindow(hwnd)) return;
    if (!GetClassNameW(hwnd, cls, 96) || !is_balloon_class(cls)) return;
    b->classHits++;
    style = (LONG)GetWindowLongPtrW(hwnd, GWL_STYLE);
    if (!(style & BALLOON_TTS_BALLOON)) return;
    b->styleHits++;
    if (g_trayPid) {
        DWORD pid = 0;
        GetWindowThreadProcessId(hwnd, &pid);
        if (pid != g_trayPid) return;
    }
    b->pidHits++;

    /* 去抖：气球排队显示时 CREATE/SHOW 会连发，3 秒内同一来源只弹一次 */
    now = qs_now();
    if (b->lastAt && now - b->lastAt < BALLOON_COOLDOWN_MS) {
        b->suppressed++;
        return;
    }
    if (b->lastAny && now - b->lastAny < BALLOON_GLOBAL_GAP_MS) {
        b->suppressed++;
        return;
    }

    text[0] = 0;
    if (!balloon_read_text(hwnd, text, BALLOON_TEXT_MAX + 1)) b->textFails++;
    
    for (i = 0; text[i]; i++)
        if (text[i] == L'\r' || text[i] == L'\n' || text[i] == L'\t')
            text[i] = L' ';
    b->lastTextLen = (int)wcslen(text);
    wcsncpy(b->lastText, text, BALLOON_TEXT_MAX);
    b->lastText[BALLOON_TEXT_MAX] = 0;

    /* 标题 szInfoTitle：应用自己填的标题文本（如 "Greenshot"），不是系统 反查的 exe 名。 读得到就用它当程序名，读不到才退回「托盘消息」。 */
    title[0] = 0;
    if (balloon_read_title(hwnd, title, BALLOON_TITLE_MAX + 1, &titleIcon)) {
        b->titleHits++;
        wcsncpy(b->lastTitle, title, BALLOON_TITLE_MAX);
        b->lastTitle[BALLOON_TITLE_MAX] = 0;
        b->lastTitleIcon = (int)titleIcon;
        titleHicon = balloon_title_hicon(titleIcon);
    } else {
        b->titleFails++;
        b->lastTitle[0] = 0;
        b->lastTitleIcon = 0;
    }

    /* 详情 = 真实读到的 szInfo； 读不到就只给「有新消息」信号，不编造正文。 */
    {
        const WCHAR *app = title[0] ? title : BALLOON_APP_NAME;
        const WCHAR *detail = text[0] ? text : BALLOON_NO_TEXT;
        if (titleHicon)
            notify_show_icon(&a->notify, a, titleHicon, app, detail, NOTIFY_MS);
        else
            notify_show_fa(&a->notify, a, FA_COMMENT_DOTS, app, detail, NOTIFY_MS);
    }

    b->lastAt = now;
    b->lastAny = now;
    b->hits++;
}

/* 生命周期 */
void balloon_init(App *a)
{
    Balloon *b = &a->balloon;
    HWND tray;

    ZeroMemory(b, sizeof(*b));
    b->hwnd = a->hwnd;
    b->enabled = 1;

    g_target  = a->hwnd;

    /* 官方允许按 idProcess 收窄：真机只收 explorer 的事件，开销极小。 */
    tray = FindWindowW(L"Shell_TrayWnd", NULL);
    if (tray) GetWindowThreadProcessId(tray, &b->trayPid);
    g_trayPid = b->trayPid;

    b->hook = SetWinEventHook((DWORD)EVENT_OBJECT_CREATE,
                              (DWORD)EVENT_OBJECT_SHOW,
                              NULL, balloon_win_event,
                              b->trayPid, 0,
                              WINEVENT_OUTOFCONTEXT |
                              WINEVENT_SKIPOWNPROCESS);
    b->hookOk = (b->hook != NULL);
}

void balloon_shutdown(App *a)
{
    Balloon *b = &a->balloon;
    if (!b->enabled) return;
    if (b->hook) {
        UnhookWinEvent(b->hook);
        b->hook = NULL;
    }
    b->hookOk = 0;
    g_target = NULL;
    g_trayPid = 0;
}

/* 只读查询 */
int          balloon_enabled(const App *a)     { return a->balloon.enabled; }
int          balloon_hook_ok(const App *a)     { return a->balloon.hookOk; }
int          balloon_events(const App *a)      { return a->balloon.events; }
int          balloon_hits(const App *a)        { return a->balloon.hits; }
int          balloon_suppressed(const App *a)  { return a->balloon.suppressed; }
int          balloon_class_hits(const App *a)  { return a->balloon.classHits; }
int          balloon_style_hits(const App *a)  { return a->balloon.styleHits; }
int          balloon_text_len(const App *a)    { return a->balloon.lastTextLen; }
const WCHAR *balloon_last_text(const App *a)   { return a->balloon.lastText; }
int          balloon_title_hits(const App *a)  { return a->balloon.titleHits; }
int          balloon_title_fails(const App *a) { return a->balloon.titleFails; }
int          balloon_last_title_icon(const App *a) { return a->balloon.lastTitleIcon; }
const WCHAR *balloon_last_title(const App *a)  { return a->balloon.lastTitle; }
DWORD        balloon_tray_pid(const App *a)    { return a->balloon.trayPid; }
