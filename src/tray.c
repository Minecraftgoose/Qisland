/* tray.c — 系统通知区域图标与右键菜单
 *
 * 本轮修复：右键「菜单闪一下又关了」。
 *   根因 1（主因）：TrackPopupMenu 内部有**模态消息循环**，会继续取走并
 *     dispatch 线程消息队列里的消息。NOTIFYICON_VERSION_4 下右键常连发
 *     WM_RBUTTONUP + WM_CONTEXTMENU，第二条在菜单仍显示时被送到
 *     tray_on_message → 嵌套再弹一次菜单 → 第一个菜单立刻被取消，
 *     看起来就是「闪一下就关」。
 *   根因 2：宿主窗口是一次性的，PostMessage(host, WM_NULL) 刚投递完就
 *     DestroyWindow，那条官方用来「强制任务切换、修第二次弹出立刻消失」
 *     的消息永远没被取走，等于没做。
 *   根因 3：宿主用了 LWA_ALPHA=0（完全透明），alpha=0 的 layered 窗口
 *     连命中测试都穿透，SetForegroundWindow 可能因此失败。
 *
 * 修法：
 *   · menuOpen 防重入 + 300ms 去抖，吃掉同一轮右键的第二次事件；
 *   · 宿主窗口改为**常驻**（tray_init 建、tray_shutdown 销毁），
 *     线程消息循环 GetMessageW(NULL,...) 会取到 WM_NULL 并 dispatch；
 *   · alpha 改 1（肉眼不可见但非零），前台激活可靠；
 *   · 菜单关闭后才 PostMessage(WM_NULL)，且不再销毁宿主。
 *
 * 官方依据：TrackPopupMenu 文档 —— 通知图标菜单调用前当前窗口必须是
 *   前台窗口，否则点菜单外不消失；已是前台时第二次弹出会立刻消失，
 *   需 SetForegroundWindow → TrackPopupMenu → PostMessage(hDlg, WM_NULL)。
 *
 * 其余约束不变：不建线程、不设定时器、不置脏、不触发重绘；
 * 图标是 LoadIconW 共享句柄不 DestroyIcon；退出统一 PostQuitMessage →
 * main.c cleanup → tray_shutdown，保证 NIM_DELETE。
 */
#include "tray.h"
#include "app.h"
#include "config.h"
#include "window.h"

#ifndef NOTIFYICON_VERSION_4
#define NOTIFYICON_VERSION_4  4
#endif

#define TRAY_HOST_CLASS   L"QislandTrayMenuHost"
#define TRAY_MENU_GAP_MS  300            /* 同一轮右键双事件的去抖窗口 */

static int  hostClassOK = 0;
static HWND trayHost    = NULL;          /* 常驻：菜单 owner，不能每次销毁 */
static int  menuOpen    = 0;             /* 防重入：TrackPopupMenu 期间置 1 */
static DWORD lastMenuTick = 0;

/* 菜单宿主窗口：1x1、屏幕外、WS_EX_TOOLWINDOW（不进任务栏 / Alt+Tab）、
   WS_EX_LAYERED + alpha=1（几乎看不见但不是 0），不带 WS_EX_NOACTIVATE，
   所以能真正成为前台窗口。主窗口是 NOACTIVATE 分层窗口，当不了前台。 */
static HWND tray_make_host(void)
{
    WNDCLASSEXW wc;
    HWND h;
    HINSTANCE inst = GetModuleHandleW(NULL);

    if (!hostClassOK) {
        ZeroMemory(&wc, sizeof(wc));
        wc.cbSize = sizeof(wc);
        wc.style = 0;
        wc.lpfnWndProc = DefWindowProcW;
        wc.hInstance = inst;
        wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
        wc.lpszClassName = TRAY_HOST_CLASS;
        if (RegisterClassExW(&wc)) hostClassOK = 1;
        else if (GetLastError() == ERROR_CLASS_ALREADY_EXISTS) hostClassOK = 1;
        else return NULL;
    }
    h = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_LAYERED,
                        TRAY_HOST_CLASS, L"", WS_POPUP,
                        -120, -120, 1, 1, NULL, NULL, inst, NULL);
    if (!h) return NULL;
    /* alpha=1：肉眼不可见，但 layered hit-test 有效、可成为前台窗口。
       原来写 0（完全透明）会让 SetForegroundWindow 有失败风险。 */
    SetLayeredWindowAttributes(h, 0, 1, LWA_ALPHA);
    ShowWindow(h, SW_SHOW);              /* 必须显示，隐藏窗口当不了前台 */
    return h;
}

static void tray_menu(App *a)
{
    POINT pt;
    HMENU m;
    int cmd;
    DWORD now = GetTickCount();

    /* ① 防重入：TrackPopupMenu 的模态循环会 dispatch 同线程的后续消息，
       没有这层保护，连发的第二个托盘事件会嵌套弹菜单、把第一个顶掉。 */
    if (menuOpen) return;
    /* ② 去抖：吃掉同一轮右键的 WM_RBUTTONUP + WM_CONTEXTMENU 双发。 */
    if (now - lastMenuTick < TRAY_MENU_GAP_MS) return;
    if (!trayHost || !IsWindow(trayHost)) return;
    if (!GetCursorPos(&pt)) return;      /* 键盘触发拿不到坐标就放弃，不崩 */

    menuOpen = 1;
    lastMenuTick = now;

    m = CreatePopupMenu();
    if (m) {
        /* 两项都是用户逐字点名；分隔符只是视觉分组，不是文字。 */
        AppendMenuW(m, MF_STRING, TRAY_MENU_SHOW, L"显示/隐藏灵动岛");
        AppendMenuW(m, MF_SEPARATOR, 0, NULL);
        AppendMenuW(m, MF_STRING, TRAY_MENU_EXIT, L"退出");

        /* 官方三件套第 1 步：菜单 owner 必须是前台窗口。 */
        SetForegroundWindow(trayHost);
        BringWindowToTop(trayHost);
        cmd = (int)TrackPopupMenu(m,
                                  TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON,
                                  pt.x, pt.y, 0, trayHost, NULL);
        /* 官方三件套第 3 步：强制任务切换，避免下一次弹出立刻消失。
           宿主窗口现在常驻，这条 WM_NULL 会被线程消息循环真正取走；
           以前投递完立刻 DestroyWindow，等于没发。 */
        PostMessageW(trayHost, WM_NULL, 0, 0);
        DestroyMenu(m);

        switch (cmd) {
        case TRAY_MENU_SHOW:
            if (a->visible) window_hide(a);
            else            window_show(a);
            break;
        case TRAY_MENU_EXIT:
            PostQuitMessage(0);          /* 走 main.c cleanup，托盘会 NIM_DELETE */
            break;
        }
    }
    menuOpen = 0;
}

int tray_init(App *a)
{
    NOTIFYICONDATAW n;

    /* 宿主先建好：菜单依赖它常驻，不能等到右键时才创建。 */
    trayHost = tray_make_host();
    if (!trayHost) return 0;

    ZeroMemory(&n, sizeof(n));
    n.cbSize = (DWORD)sizeof(n);
    n.hWnd = a->hwnd;
    n.uID  = TRAY_UID;
    n.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    n.uCallbackMessage = WM_APP_TRAY;
    n.hIcon = LoadIconW(a->inst, MAKEINTRESOURCEW(IDI_QISLAND));
    _snwprintf(n.szTip, 128, L"%s", APP_NAME);
    n.szTip[127] = 0;

    if (!Shell_NotifyIconW(NIM_ADD, &n)) {
        DestroyWindow(trayHost);
        trayHost = NULL;
        return 0;                        /* 没加图标，tray_shutdown 无需清理 */
    }
    /* 官方：除非有充分理由，Vista+ 一律用 NOTIFYICON_VERSION_4。
       失败不影响图标存在，只是退回旧版消息语义，仍可用。 */
    n.uVersion = NOTIFYICON_VERSION_4;
    Shell_NotifyIconW(NIM_SETVERSION, &n);
    a->trayOK = 1;
    return 1;
}

void tray_shutdown(App *a)
{
    NOTIFYICONDATAW n;
    if (a->trayOK && a->hwnd) {
        ZeroMemory(&n, sizeof(n));
        n.cbSize = (DWORD)sizeof(n);
        n.hWnd = a->hwnd;
        n.uID  = TRAY_UID;
        Shell_NotifyIconW(NIM_DELETE, &n);   /* 官方：退出必须删，否则留残影 */
    }
    if (trayHost) {
        DestroyWindow(trayHost);
        trayHost = NULL;
    }
    menuOpen = 0;
    a->trayOK = 0;
}

void tray_on_message(App *a, WPARAM wp, LPARAM lp)
{
    UINT ev = (UINT)LOWORD(lp);
    (void)wp;
    /* 右键抬起 / 键盘或触摸触发的上下文菜单。左键不处理，避免误触。
       两个事件可能连发，由 tray_menu() 的防重入 + 去抖吃掉第二次。 */
    if (ev == WM_RBUTTONUP || ev == WM_CONTEXTMENU)
        tray_menu(a);
}
