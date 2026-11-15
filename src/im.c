/* im.c — QQ / 微信即时消息监听
 *
 * 只负责「发现信号」：所有通知形态、显示时长、关闭行为都交给 notify 模块，
 * 本模块不画任何像素、不写盘、不排队，也不弹第二个 UI。
 */
#include "im.h"
#include "app.h"
#include "config.h"
#include "icons.h"
#include "notify.h"
#include <commctrl.h>       /* TBBUTTONINFO / TB_BUTTONCOUNT / TB_GETBUTTONINFO */

/* RegisterShellHookWindow / DeregisterShellHookWindow 在部分 SDK 头里没声明，
   动态从 user32.dll 取；取不到就优雅降级（不影响岛本身）。 */
typedef BOOL (WINAPI *ShellHookFn)(HWND);
static ShellHookFn g_regShell = NULL, g_unregShell = NULL;

static void load_shell_api(void)
{
    HMODULE u32 = GetModuleHandleW(L"user32.dll");
    if (!u32) return;
    g_regShell = (ShellHookFn)(void *)
        GetProcAddress(u32, "RegisterShellHookWindow");
    g_unregShell = (ShellHookFn)(void *)
        GetProcAddress(u32, "DeregisterShellHookWindow");
}

/* ---------------- 进程与窗口工具 ---------------- */

/* Vista+：句柄只需 PROCESS_QUERY_LIMITED_INFORMATION，Win7 支持。
   返回完整 exe 路径（Win32 路径格式，可直接交给 SHGetFileInfoW）。 */
static int proc_exe(DWORD pid, WCHAR *out, DWORD cap)
{
    HANDLE h;
    DWORD n = cap;
    BOOL ok;
    if (!pid) return 0;
    h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!h) return 0;
    ok = QueryFullProcessImageNameW(h, 0, out, &n);
    CloseHandle(h);
    if (!ok || n == 0 || n >= cap) return 0;
    out[n] = 0;
    return 1;
}

static const WCHAR *base_name(const WCHAR *path)
{
    const WCHAR *p = path, *last = path;
    for (; *p; p++)
        if (*p == L'\\' || *p == L'/') last = p + 1;
    return last;
}

static int is_wechat(const WCHAR *base)
{
    return !_wcsicmp(base, L"Weixin.exe") ||
           !_wcsicmp(base, L"WeChat.exe") ||
           !_wcsicmp(base, L"WeChatAppEx.exe") ||
           !_wcsicmp(base, L"WeChatWin.exe");
}

static int is_qq(const WCHAR *base)
{
    return !_wcsicmp(base, L"QQ.exe") ||
           !_wcsicmp(base, L"QQNT.exe") ||
           !_wcsicmp(base, L"TIM.exe") ||
           !_wcsicmp(base, L"QQScLauncher.exe");
}

/* ---------------- QQ 后台线程：状态快照 ---------------- */

typedef struct {
    DWORD qqPid, wxPid;                      /* 找到的 QQ / 微信进程 pid */
    DWORD qqHash, wxHash;                    /* 各自可见窗口标题集合指纹 */
    int   nQQ, nWX;                          /* 各自可见聊天窗口数 */
    DWORD trayHash;
    int   trayValid;
} PollState;

static DWORD hash_wstr(DWORD h, const WCHAR *s)
{
    for (; *s; s++) {
        h ^= (DWORD)(*s & 0xFFFF);
        h *= 16777619u;
        h ^= (DWORD)((*s >> 8) & 0xFF);
        h *= 16777619u;
    }
    return h;
}

/* 枚举所有可见顶层窗口：分别统计属于 QQ / 微信进程的、有非空标题且
   不是纯主窗口标题的那些。聊天窗口出现 → 窗口数增加 + 标题 hash 变化。
   微信主路径是 Shell Hook，这里只作兜底：官方确认 NIM_MODIFY 不广播，
   若某个微信版本只闪托盘图标而不闪任务栏按钮，HSHELL_FLASH 会漏掉。 */
static BOOL CALLBACK enum_im_windows(HWND hwnd, LPARAM lp)
{
    PollState *st = (PollState *)lp;
    WCHAR path[MAX_PATH], txt[256];
    DWORD pid = 0;
    const WCHAR *base;
    int isQQ, isWX;

    if (!IsWindowVisible(hwnd)) return TRUE;
    GetWindowThreadProcessId(hwnd, &pid);
    if (!proc_exe(pid, path, MAX_PATH)) return TRUE;
    base = base_name(path);
    isQQ = is_qq(base); isWX = is_wechat(base);
    if (!isQQ && !isWX) return TRUE;
    if (isQQ && !st->qqPid) st->qqPid = pid;
    if (isWX && !st->wxPid) st->wxPid = pid;
    if (!GetWindowTextW(hwnd, txt, 256) || !txt[0]) return TRUE;
    if (!_wcsicmp(txt, L"QQ") || !_wcsicmp(txt, L"TIM") ||
        !_wcsicmp(txt, L"微信") || !_wcsicmp(txt, L"WeChat")) return TRUE;
    if (isQQ) { st->nQQ++; st->qqHash = hash_wstr(st->qqHash, txt); }
    else      { st->nWX++; st->wxHash = hash_wstr(st->wxHash, txt); }
    return TRUE;
}

/* Win7 托盘结构：Shell_TrayWnd → TrayNotifyWnd → SysPager → ToolbarWindow32。
   **未文档化的 explorer 内部结构**，所以全程 best-effort：
   取不到窗口 / 打不开进程 / 消息超时都返回 0，由窗口枚举信号兜底。
   跨进程 TB_GETBUTTONINFO 必须把 TBBUTTONINFO 放进 explorer 的地址空间。 */
static DWORD tray_hash(int *okOut)
{
    HWND tray, tn, pager, tb;
    DWORD pid = 0, h = 2166136261u;
    HANDLE hp;
    void *remote = NULL;
    TBBUTTONINFO bi;
    DWORD_PTR res = 0;
    LRESULT nBtn;
    int i, n;

    *okOut = 0;
    tray = FindWindowW(L"Shell_TrayWnd", NULL);
    if (!tray) return 0;
    tn = FindWindowExW(tray, NULL, L"TrayNotifyWnd", NULL);
    if (!tn) return 0;
    pager = FindWindowExW(tn, NULL, L"SysPager", NULL);
    tb = pager ? FindWindowExW(pager, NULL, L"ToolbarWindow32", NULL)
               : FindWindowExW(tn, NULL, L"ToolbarWindow32", NULL);
    if (!tb) return 0;

    GetWindowThreadProcessId(tb, &pid);
    hp = OpenProcess(PROCESS_VM_OPERATION | PROCESS_VM_READ |
                     PROCESS_VM_WRITE, FALSE, pid);
    if (!hp) return 0;
    remote = VirtualAllocEx(hp, NULL, sizeof(bi),
                            MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remote) { CloseHandle(hp); return 0; }

    /* SMTO_ABORTIFHUNG + 超时：explorer 卡住时后台线程也不会挂死，
       im_shutdown() 的 WaitForSingleObject 才能保证按时返回。 */
    nBtn = SendMessageTimeoutW(tb, TB_BUTTONCOUNT, 0, 0,
                               SMTO_ABORTIFHUNG, IM_SEND_TIMEOUT_MS, &res);
    if (!nBtn) goto out;
    n = (int)res;
    if (n < 0) n = 0;
    if (n > IM_TRAY_BUTTON_MAX) n = IM_TRAY_BUTTON_MAX;

    for (i = 0; i < n; i++) {
        ZeroMemory(&bi, sizeof(bi));
        bi.cbSize = sizeof(bi);
        /* 不要 TBIF_TEXT：pszText 是跨进程指针，且字符串池索引读不出来 */
        bi.dwMask = TBIF_IMAGE | TBIF_STATE | TBIF_LPARAM;
        if (!WriteProcessMemory(hp, remote, &bi, sizeof(bi), NULL)) break;
        res = 0;
        if (!SendMessageTimeoutW(tb, TB_GETBUTTONINFO, (WPARAM)i,
                                 (LPARAM)remote, SMTO_ABORTIFHUNG,
                                 IM_SEND_TIMEOUT_MS, &res)) continue;
        if (!ReadProcessMemory(hp, remote, &bi, sizeof(bi), NULL)) break;
        h ^= (DWORD)bi.iImage;  h *= 16777619u;
        h ^= (DWORD)bi.fsState; h *= 16777619u;
        h ^= (DWORD)(DWORD_PTR)bi.lParam; h *= 16777619u;
    }
    *okOut = 1;

out:
    VirtualFreeEx(hp, remote, 0, MEM_RELEASE);
    CloseHandle(hp);
    return h;
}

static void take_snapshot(PollState *st, Im *im)
{
    int trayOK = 0;
    ZeroMemory(st, sizeof(*st));
    st->qqHash = st->wxHash = 2166136261u;
    EnumWindows(enum_im_windows, (LPARAM)st);
    InterlockedIncrement(&im->winPolls);
    st->trayHash = tray_hash(&trayOK);
    st->trayValid = trayOK;
    InterlockedIncrement(&im->trayPolls);
    if (trayOK) InterlockedIncrement(&im->trayOK);
}

/* 线程函数：只读系统状态 + 只写自己的栈和 Interlocked 计数，
   绝不触碰 App / Canvas / Ui / Notify 的可变字段。 */
static DWORD WINAPI im_poll_thread(void *p)
{
    Im *im = (Im *)p;
    PollState prev, cur;
    int havePrev = 0;

    ZeroMemory(&prev, sizeof(prev));
    for (;;) {
        int k;
        take_snapshot(&cur, im);

        if (havePrev) {
            int trayChanged = cur.trayValid && prev.trayValid &&
                              cur.trayHash != prev.trayHash;
            /* 只在「可见聊天窗口数增加」时判定，标题变化但不增不报，
               减少用户自己切换会话造成的误报。 */
            int qqWin = cur.nQQ > prev.nQQ && cur.qqHash != prev.qqHash;
            int wxWin = cur.nWX > prev.nWX && cur.wxHash != prev.wxHash;
            if (cur.qqPid && (trayChanged || qqWin))
                PostMessageW(im->hwnd, WM_APP_IM_SIGNAL,
                             (WPARAM)IM_SIG_QQ, (LPARAM)cur.qqPid);
            if (cur.wxPid && wxWin)
                PostMessageW(im->hwnd, WM_APP_IM_SIGNAL,
                             (WPARAM)IM_SIG_WECHAT, (LPARAM)cur.wxPid);
        }
        prev = cur;
        havePrev = 1;

        /* 500ms 拆成 10×50ms，退出时最多等 50ms，不留野线程 */
        for (k = 0; k < IM_POLL_MS / 50; k++) {
            if (InterlockedCompareExchange(&im->stop, 0, 0)) goto done;
            Sleep(50);
        }
    }
done:
    InterlockedExchange(&im->threadExited, 1);
    return 0;
}

/* ---------------- 主线程：信号 → 通知 ---------------- */

void im_on_signal(App *a, int kind, DWORD pid)
{
    Im *im = &a->im;
    DWORD now;
    int isWC;
    WCHAR path[MAX_PATH];
    const WCHAR *app, *detail;
    HICON *slot;
    WCHAR *exeSlot;
    unsigned fa;

    if (!im->enabled || kind < IM_SIG_WECHAT || kind > IM_SIG_QQFLASH) return;
    now = qs_now();
    /* 去抖：闪烁是连发的，不在冷却期内直接丢掉，避免把通知刷成一片 */
    if (im->lastAt[kind] && now - im->lastAt[kind] < IM_COOLDOWN_MS) {
        im->suppressed++;
        return;
    }
    if (im->lastAny && now - im->lastAny < IM_GLOBAL_GAP_MS) {
        im->suppressed++;
        return;
    }

    isWC   = (kind == IM_SIG_WECHAT);
    app    = isWC ? L"微信" : L"QQ";
    detail = L"有新消息";          /* 只有信号，没有内容：不编造谁发了什么 */
    slot   = isWC ? &im->icoWeChat : &im->icoQQ;
    exeSlot= isWC ? im->exeWeChat  : im->exeQQ;
    fa     = isWC ? FA_COMMENT_DOTS : FA_COMMENT;

    /* 由 pid 反查 exe → 取软件自身图标。HICON 缓存在 Im 里，
       通知 active 期间每帧重绘都要用，不能弹完就销毁。 */
    path[0] = 0;
    if (!*slot && proc_exe(pid, path, MAX_PATH)) {
        HICON ico = notify_icon_from_file(path);
        if (ico) {
            *slot = ico;
            wcsncpy(exeSlot, path, MAX_PATH - 1);
            exeSlot[MAX_PATH - 1] = 0;
        }
    }

    if (*slot)
        notify_show_icon(&a->notify, a, *slot, app, detail, NOTIFY_MS);
    else
        notify_show_fa(&a->notify, a, fa, app, detail, NOTIFY_MS);

    im->lastAt[kind] = now;
    im->lastAny = now;
    im->hits[kind]++;
    wcsncpy(im->lastApp, app, 63);
    im->lastApp[63] = 0;
}

/* WndProc 里在 switch 之前调用：shellMsg 是运行时注册的 ID，不能做 case。 */
int im_shell_message(App *a, UINT msg, WPARAM wp, LPARAM lp)
{
    Im *im = &a->im;
    HWND h;
    DWORD pid = 0;
    WCHAR path[MAX_PATH];
    const WCHAR *base;
    int testSelf;

    if (!im->enabled || !im->shellMsg || msg != im->shellMsg) return 0;
    im->shellMsgs++;
    if ((DWORD)wp != (DWORD)HSHELL_FLASH) return 1;   /* 识别但不关心 */

    h = (HWND)lp;
    if (!h || !IsWindow(h)) return 1;
    GetWindowThreadProcessId(h, &pid);
    if (!pid) return 1;

    testSelf = im->testMode && pid == GetCurrentProcessId();
    if (!testSelf) {
        if (!proc_exe(pid, path, MAX_PATH)) return 1;
        base = base_name(path);
        if (!is_wechat(base) && !is_qq(base)) return 1;
        im->shellHits++;
        /* QQ 前台 / 任务栏按钮闪烁同样会产生 HSHELL_FLASH ——
           方案原文说 QQ 不产生，这里补上，几乎零成本。 */
        im_on_signal(a, is_wechat(base) ? IM_SIG_WECHAT : IM_SIG_QQFLASH, pid);
    } else {
        im->shellHits++;
        im_on_signal(a, IM_SIG_WECHAT, pid);   /* --imtest 走完整链路 */
    }
    return 1;
}

/* ---------------- 生命周期 ---------------- */

void im_init(App *a)
{
    Im *im = &a->im;
    ZeroMemory(im, sizeof(*im));
    im->hwnd = a->hwnd;
    im->testMode = a->imTest;
    im->enabled = !a->imOff;
    if (!im->enabled) return;

    load_shell_api();
    im->shellMsg = RegisterWindowMessageW(L"SHELLHOOK");
    /* RegisterShellHookWindow 不是全局钩子：它只把 shell 事件 post 到
       注册的窗口，不进钩子链、不拦截任何输入，不会重演 WH_MOUSE_LL 卡死。 */
    if (im->shellMsg && g_regShell)
        im->registered = g_regShell(a->hwnd) ? 1 : 0;

    im->thread = CreateThread(NULL, 0, im_poll_thread, im, 0, NULL);
    if (!im->thread) im->threadExited = 1;
}

static int join_thread(Im *im)
{
    DWORD w;
    if (!im->thread) return 1;
    InterlockedExchange(&im->stop, 1);
    w = WaitForSingleObject(im->thread, IM_POLL_MS * 4 + 1000);
    if (w == WAIT_OBJECT_0) CloseHandle(im->thread);
    else CloseHandle(im->thread);   /* 超时也不 TerminateThread：避免资源泄漏 */
    im->thread = NULL;
    return (w == WAIT_OBJECT_0 &&
            InterlockedCompareExchange(&im->threadExited, 0, 0) != 0);
}

void im_test_rewind(App *a, DWORD ms)
{
    Im *im = &a->im;
    int k;
    if (!im->testMode) return;
    for (k = 1; k <= 3; k++)
        im->lastAt[k] = (im->lastAt[k] > ms) ? im->lastAt[k] - ms : 0;
    im->lastAny = (im->lastAny > ms) ? im->lastAny - ms : 0;
}

int im_join_for_test(App *a)
{
    return join_thread(&a->im);
}

void im_shutdown(App *a)
{
    Im *im = &a->im;
    if (!im->enabled) return;
    join_thread(im);
    if (im->registered && g_unregShell) g_unregShell(im->hwnd);
    im->registered = 0;
    /* 缓存的 HICON 归本模块所有；SHGetFileInfoW 返回的是新句柄，需销毁 */
    if (im->icoWeChat) { DestroyIcon(im->icoWeChat); im->icoWeChat = NULL; }
    if (im->icoQQ)     { DestroyIcon(im->icoQQ);     im->icoQQ = NULL; }
}

/* ---------------- 只读查询 ---------------- */
int  im_enabled(const App *a)  { return a->im.enabled; }
int  im_hook_ok(const App *a)  { return a->im.registered; }
int  im_shell_api_present(const App *a) { (void)a; return g_regShell != NULL; }
UINT im_shell_msg(const App *a){ return a->im.shellMsg; }
int  im_thread_running(const App *a) { return a->im.thread != NULL; }
int  im_hits(const App *a, int k) { return (k >= 1 && k <= 3) ? a->im.hits[k] : 0; }
int  im_suppressed(const App *a){ return a->im.suppressed; }
int  im_shell_msgs(const App *a){ return a->im.shellMsgs; }
/* 只读诊断：volatile LONG 的读取本身在 x86/x64 上是原子的；
   InterlockedCompareExchange(x,0,0) 当作带内存屏障的原子读，
   强转去掉 const 是安全的（不会写入）。 */
#define IM_ATOMIC_READ(field) \
    ((long)InterlockedCompareExchange((volatile LONG *)&(field), 0, 0))
long im_tray_polls(const App *a){ return IM_ATOMIC_READ(a->im.trayPolls); }
long im_tray_ok(const App *a)   { return IM_ATOMIC_READ(a->im.trayOK); }
long im_win_polls(const App *a) { return IM_ATOMIC_READ(a->im.winPolls); }
long im_thread_exited(const App *a) { return IM_ATOMIC_READ(a->im.threadExited); }
const WCHAR *im_last_app(const App *a) { return a->im.lastApp; }
