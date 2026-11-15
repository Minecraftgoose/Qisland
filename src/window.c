/* window.c — 窗口与输入路由
 *
 * 输入设计（沿用上一版修过的坑）：
 *   · 高频 WM_MOUSEMOVE 绝不置脏标记，否则 UpdateLayeredWindow 风暴会拖垮 Win7 主线程
 *   · WM_NCHITTEST 必须先 ScreenToClient 再查最终 alpha，透明区 HTTRANSPARENT
 *   · 不用全局 WH_MOUSE_LL 钩子；滚轮走 Raw Input(RIDEV_INPUTSINK)，
 *     因为 WM_MOUSEWHEEL 只发给焦点窗口而本窗口 WS_EX_NOACTIVATE
 *   · 悬停与点击由主循环轮询（ui_poll），窗口过程只做消息路由
 */
#include "window.h"
#include "app.h"
#include "config.h"
#include "canvas.h"
#include "anim.h"
#include "countdown.h"
#include "ui.h"
#include "input.h"
#include "clip.h"
#include "notify.h"
#include "im.h"

#ifndef WM_CLIPBOARDUPDATE
#define WM_CLIPBOARDUPDATE 0x031D
#endif

static App *app_of(HWND hwnd)
{
    return (App *)(LONG_PTR)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
}

/* 系统标准光标是共享句柄（LoadCursor 内部 LR_SHARED），加载一次即可，
   不需要 DestroyCursor。 */
static HCURSOR curArrow = NULL, curIBeam = NULL;

HCURSOR window_cursor_handle(int ibeam)
{
    if (!curArrow) curArrow = LoadCursorW(NULL, IDC_ARROW);
    if (!curIBeam) curIBeam = LoadCursorW(NULL, IDC_IBEAM);
    return ibeam ? curIBeam : curArrow;
}

int window_text_cursor(const App *a, int cx, int cy)
{
    NoteBox nb;
    /* 只有展开后的便签页才把文本区当编辑区；收起(岛)与计时器页都不算。
       用 ui_note_box 的实时动画几何判断，不依赖 ui_render 构建的命中表，
       切档当帧也不会滞后。 */
    if (!a || !a->visible || a->ui.view != NOTE_VIEW || a->anim.exp <= 0.5)
        return 0;
    if (notify_active(&a->notify)) return 0;   /* 通知卡片不是文本编辑区 */
    ui_note_box(a, &nb);
    return (cx >= nb.x && cx < nb.x + nb.w &&
            cy >= nb.y && cy < nb.y + nb.h);
}

static void popup_menu(App *a, int sx, int sy)
{
    HMENU m = CreatePopupMenu();
    if (!m) return;
    /* 菜单也不用文字：4 个圆点表示竖向锚点，几何符号表示隐藏/退出。
       点数量、当前高亮点都由卡片表推导，以后加卡片不用改这里。 */
    for (int i = 0; i < CARDS_VDOTS; i++) {
        WCHAR lab[12];
        for (int j = 0; j < CARDS_VDOTS; j++)
            lab[j] = (j == i) ? L'\x25CF' : L'\x25CB';   /* ● / ○ */
        lab[CARDS_VDOTS] = 0;
        AppendMenuW(m, MF_STRING | (cards_vdot(a->ui.view) == i ? MF_CHECKED : 0),
                    MENU_VIEW0 + i, lab);
    }
    AppendMenuW(m, MF_SEPARATOR, 0, NULL);
    AppendMenuW(m, MF_STRING, 1, L"\x2212");             /* − 隐藏 */
    AppendMenuW(m, MF_STRING, 2, L"\x00D7");             /* × 退出 */
    {
        int cmd = TrackPopupMenu(m, TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON,
                                 sx, sy, 0, a->hwnd, NULL);
        DestroyMenu(m);
        if (cmd >= MENU_VIEW0 && cmd <= MENU_VIEW_LAST)
            ui_set_view(a, cards_anchor(cmd - MENU_VIEW0));
        else if (cmd == 1) window_hide(a);
        else if (cmd == 2) PostQuitMessage(0);
    }
}

void window_show(App *a)
{
    a->visible = 1; a->hover = 0; a->anim.expanded = 0;
    ShowWindow(a->hwnd, SW_SHOWNA);
    SetWindowPos(a->hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    a->dirty = 1;
}
void window_hide(App *a)
{
    a->visible = 0; a->hover = 0; a->anim.expanded = 0; a->dirty = 1;
    ShowWindow(a->hwnd, SW_HIDE);
}

LRESULT CALLBACK window_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    App *a = app_of(hwnd);
    if (!a) return DefWindowProcW(hwnd, msg, wp, lp);

    /* 便签编辑态：键盘 / IME / 失焦先交给 input 模块 */
    if (msg == WM_CHAR || msg == WM_KEYDOWN ||
        msg == WM_IME_STARTCOMPOSITION || msg == WM_IME_COMPOSITION ||
        msg == WM_IME_ENDCOMPOSITION || msg == WM_IME_CHAR ||
        msg == WM_KILLFOCUS) {
        if (input_message(a, hwnd, msg, wp, lp)) return 0;
    }

    /* SHELLHOOK 的 ID 是 RegisterWindowMessage 运行时给的，不能做 case：
       在 switch 之前统一识别。它不是全局钩子，不拦截任何输入。 */
    if (im_shell_message(a, msg, wp, lp)) return 0;

    switch (msg) {
    case WM_APP_IM_SIGNAL:
        /* QQ 后台线程只送 (kind, pid)，字符串/图标全部由主线程取 */
        im_on_signal(a, (int)wp, (DWORD)lp);
        return 0;
    case WM_SETCURSOR: {
        /* 官方要求：光标必须在这里「每次鼠标移动都重新 SetCursor」——
           只设一次的话下一次鼠标移动就会被重置；且处理后必须返回 TRUE，
           否则 DefWindowProc 会用窗口类光标（箭头）把它覆盖回去。
           lParam 低字是 WM_NCHITTEST 的结果，只有客户区才由我们决定。 */
        POINT p;
        int ibeam = 0;
        if (LOWORD(lp) != HTCLIENT) break;
        if (!GetCursorPos(&p)) break;
        ScreenToClient(hwnd, &p);
        ibeam = window_text_cursor(a, p.x, p.y);
        SetCursor(window_cursor_handle(ibeam));
        return TRUE;
    }
    case WM_NCHITTEST: {
        POINT sp;
        sp.x = GET_X_LPARAM(lp);
        sp.y = GET_Y_LPARAM(lp);
        ScreenToClient(hwnd, &sp);
        return canvas_alpha_at(&a->canvas, sp.x, sp.y) > 8
             ? HTCLIENT : HTTRANSPARENT;
    }
    case WM_MOUSEMOVE:
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_LBUTTONDBLCLK:
        /* 点击只改变功能状态，不产生任何弹簧形变 */
        return 0;
    case WM_MOUSEWHEEL:
        ui_wheel(a, GET_WHEEL_DELTA_WPARAM(wp));
        return 0;
    case WM_MOUSEHWHEEL:
        /* 硬件水平滚轮：在当前组内横向切换（横1 性能 / 横2 大卡片 / 横3 超大卡片） */
        ui_wheel_h(a, GET_WHEEL_DELTA_WPARAM(wp));
        return 0;
    case WM_INPUT: {
        if (!a->rawOK) break;
        {
            RAWINPUT raw;
            UINT sz = sizeof(raw);
            if (GetRawInputData((HRAWINPUT)lp, RID_INPUT, &raw, &sz,
                                sizeof(RAWINPUTHEADER)) == (UINT)-1) break;
            if (raw.header.dwType == RIM_TYPEMOUSE) {
                if (raw.data.mouse.usButtonFlags & RI_MOUSE_WHEEL)
                    ui_wheel(a, (short)raw.data.mouse.usButtonData);
                else if (raw.data.mouse.usButtonFlags & RI_MOUSE_HWHEEL)
                    ui_wheel_h(a, (short)raw.data.mouse.usButtonData);
            }
        }
        return 0;
    }
    case WM_CLIPBOARDUPDATE:
        /* Vista+ 的 AddClipboardFormatListener 投递到这里；NOACTIVATE
           分层窗口也能收到。自写回的通知在 clip 模块里被抑制。 */
        clip_on_update(&a->clip, a);
        return 0;
    case WM_TIMER:
        if (wp == IDT_COUNTDOWN) {
            if (a->cd.running) {
                countdown_stop_if_zero(&a->cd);   /* 归零时置 cd.ended */
                a->dirty = 1;
            }
            /* 只触发一次展开；结束后不再置脏，静止零重绘保持不变 */
            if (a->cd.ended && !a->endNotified) ui_countdown_ended(a);
        }
        return 0;
    case WM_RBUTTONUP: {
        POINT p;
        p.x = GET_X_LPARAM(lp);
        p.y = GET_Y_LPARAM(lp);
        ClientToScreen(hwnd, &p);
        popup_menu(a, p.x, p.y);
        return 0;
    }
    case WM_HOTKEY:
        if (wp == HOTKEY_ID) {
            if (a->visible) window_hide(a);
            else {
                a->visible = 1;
                ShowWindow(hwnd, SW_SHOWNA);
                SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                             SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
                a->dirty = 1;
            }
        }
        return 0;
    case WM_APP_SHOW:
        window_show(a);
        return 0;
    case WM_DESTROY:
        KillTimer(hwnd, IDT_COUNTDOWN);
        clip_detach(&a->clip);       /* 反注册必须在窗口销毁前 */
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

int window_init(App *a)
{
    HMODULE u32;
    BOOL (WINAPI *pSetDpi)(void) = NULL;
    WNDCLASSEXW wc;
    int posX, posY;
    RAWINPUTDEVICE rid;

    u32 = LoadLibraryW(L"user32.dll");
    if (u32) pSetDpi = (BOOL (WINAPI *)(void))(void *)
        GetProcAddress(u32, "SetProcessDPIAware");
    if (pSetDpi) pSetDpi();
    {
        HDC sdc = GetDC(NULL);
        int dpi = GetDeviceCaps(sdc, LOGPIXELSX);
        ReleaseDC(NULL, sdc);
        if (dpi < 96) dpi = 96;
        a->u = (double)dpi / 96.0;
        if (a->u > 2.0) a->u = 2.0;
        a->ss = (a->u > 1.25) ? 2 : SHAPE_SS;
    }
    a->scrW = GetSystemMetrics(SM_CXSCREEN);
    a->scrH = GetSystemMetrics(SM_CYSCREEN);
    anim_init_sizes(&a->anim, a->scrW, a->scrH, a->u, &a->winW, &a->winH);

    if (!canvas_init(&a->canvas, a->winW, a->winH, a->u, a->ss)) return 0;

    ZeroMemory(&wc, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.style = CS_DBLCLKS;
    wc.lpfnWndProc = window_proc;
    wc.hInstance = a->inst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = WND_CLASS;
    RegisterClassExW(&wc);

    posX = (a->scrW - a->winW) / 2;
    posY = (int)(TOP_GAP * a->u + 0.5);
    a->hwnd = CreateWindowExW(WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW |
                              WS_EX_NOACTIVATE,
                              WND_CLASS, APP_NAME, WS_POPUP | WS_VISIBLE,
                              posX, posY, a->winW, a->winH,
                              NULL, NULL, a->inst, NULL);
    if (!a->hwnd) return 0;
    SetWindowLongPtrW(a->hwnd, GWLP_USERDATA, (LONG_PTR)a);

    rid.usUsagePage = 0x01;
    rid.usUsage = 0x02;
    rid.dwFlags = RIDEV_INPUTSINK;
    rid.hwndTarget = a->hwnd;
    a->rawOK = RegisterRawInputDevices(&rid, 1, sizeof(rid));

    SetTimer(a->hwnd, IDT_COUNTDOWN, 200, NULL);
    RegisterHotKey(a->hwnd, HOTKEY_ID, MOD_CONTROL | MOD_ALT, 'N');
    SetWindowPos(a->hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
    return 1;
}

void window_shutdown(App *a)
{
    if (a->hwnd) {
        KillTimer(a->hwnd, IDT_COUNTDOWN);
        UnregisterHotKey(a->hwnd, HOTKEY_ID);
        DestroyWindow(a->hwnd);
        a->hwnd = NULL;
    }
}
