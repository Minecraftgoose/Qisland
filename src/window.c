/* window.c — 窗口与输入路由
 *
 * 输入设计（沿用上一版修过的坑）：
 *   · 高频 WM_MOUSEMOVE 绝不置脏标记，否则 UpdateLayeredWindow 风暴会拖垮 Win7 主线程
 *   · WM_NCHITTEST 必须先 ScreenToClient 再查最终 alpha，透明区 HTTRANSPARENT
 *   · 不用全局 WH_MOUSE_LL 钩子；滚轮走 Raw Input(RIDEV_INPUTSINK)，
 *     因为 WM_MOUSEWHEEL 只发给焦点窗口而本窗口 WS_EX_NOACTIVATE
 *   · 悬停与点击由主循环轮询（ui_poll），窗口过程只做消息路由
 *   · 0.3.1 起：进程 DPI 感知由 qisland.manifest 声明，运行时按窗口所在
 *     显示器取 DPI，并在 WM_DPICHANGED 时重建画布 / 字体 / 命中表
 */
#include "window.h"
#include "app.h"
#include "config.h"
#include "canvas.h"
#include "font.h"
#include "anim.h"
#include "countdown.h"
#include "ui.h"
#include "input.h"
#include "clip.h"
#include "notify.h"
#include "balloon.h"
#include "tray.h"
#include "update.h"

#ifndef WM_CLIPBOARDUPDATE
#define WM_CLIPBOARDUPDATE 0x031D
#endif
/* Win7 SDK 没有 WM_DPICHANGED（Win8.1 才加），自己兜底，不抬 WINVER */
#ifndef WM_DPICHANGED
#define WM_DPICHANGED 0x02E0
#endif

static App *app_of(HWND hwnd)
{
    return (App *)(LONG_PTR)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
}

/* 系统标准光标是共享句柄（LoadCursor 内部 LR_SHARED），加载一次即可，
   不需要 DestroyCursor。 */
static HCURSOR curArrow = NULL, curIBeam = NULL, curHand = NULL;

#ifndef IDC_HAND
#define IDC_HAND 32649
#endif

HCURSOR window_cursor_handle(int ibeam)
{
    if (!curArrow) curArrow = LoadCursorW(NULL, IDC_ARROW);
    if (!curIBeam) curIBeam = LoadCursorW(NULL, IDC_IBEAM);
    return ibeam ? curIBeam : curArrow;
}

/* 关于页仓库链接：命中时用系统手型光标，明确「这是可以点的链接」。 */
static HCURSOR window_hand_cursor_handle(void)
{
    if (!curHand) curHand = LoadCursorW(NULL, MAKEINTRESOURCEW(IDC_HAND));
    return curHand ? curHand : window_cursor_handle(0);
}

static int window_link_cursor(const App *a, int cx, int cy)
{
    return (a && a->visible && ui_hit(a, cx, cy) == H_ABOUT_REPO);
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

/* ---------------- 高 DPI（0.3.1）----------------
   取 DPI 的优先级：
     GetDpiForWindow(hwnd)  Win10 1607+，返回**窗口所在显示器**的真实 DPI
     GetDpiForSystem()      Win8.1+，系统 DPI
     GetDeviceCaps(LOGPIXELSX) 全版本兜底，但只反映主屏，per-monitor 会错
   两个新 API 全部 GetProcAddress 动态取，不抬 WINVER、不引入链接期依赖，
   Win7 照样能跑。user32 是常驻系统 DLL，这里不 FreeLibrary（与原实现一致）。 */
static UINT window_query_dpi(HWND hwnd)
{
    HMODULE u32 = LoadLibraryW(L"user32.dll");
    HDC sdc;
    int dpi;
    UINT d;

    if (u32) {
        UINT (WINAPI *pWin)(HWND) = (UINT (WINAPI *)(HWND))(void *)
            GetProcAddress(u32, "GetDpiForWindow");
        if (pWin && hwnd) {
            d = pWin(hwnd);
            if (d >= DPI_BASE) return d;
        }
        UINT (WINAPI *pSys)(void) = (UINT (WINAPI *)(void))(void *)
            GetProcAddress(u32, "GetDpiForSystem");
        if (pSys) {
            d = pSys();
            if (d >= DPI_BASE) return d;
        }
    }
    sdc = GetDC(NULL);
    dpi = sdc ? GetDeviceCaps(sdc, LOGPIXELSX) : DPI_BASE;
    if (sdc) ReleaseDC(NULL, sdc);
    return (dpi >= DPI_BASE) ? (UINT)dpi : (UINT)DPI_BASE;
}

static double window_scale_from_dpi(UINT dpi)
{
    double u = (double)dpi / (double)DPI_BASE;
    if (u < DPI_SCALE_MIN) u = DPI_SCALE_MIN;
    if (u > DPI_SCALE_MAX) u = DPI_SCALE_MAX;   /* 0.3 之前钳在 2.0 */
    return u;
}

/* WM_DPICHANGED：窗口被拖到不同缩放的显示器、或用户改了系统缩放时，
   重新按新 u 建立画布尺寸、超采样档、HFONT 与字形缓存。
   wParam 低字是新 DPI，lParam 是系统建议的新窗口矩形（屏幕坐标）。
   返回 0 表示已自行调整窗口，系统不再插手。 */
static void window_apply_dpi(App *a, HWND hwnd, UINT dpi, const RECT *suggested)
{
    double nu, w, h, r, gw, gh, gr;
    int ss, W, H, x, y;
    Canvas nc;

    /* 初始化还没完成（CreateWindowExW 期间）或 DPI 没变：都不要重建 */
    if (!a->dpiReady) return;
    nu = window_scale_from_dpi(dpi);
    if (fabs(nu - a->u) < 0.001) return;

    ss = (nu > DPI_SS_THRESHOLD) ? 2 : SHAPE_SS;
    a->scrW = GetSystemMetrics(SM_CXSCREEN);
    a->scrH = GetSystemMetrics(SM_CYSCREEN);
    anim_init_sizes(&a->anim, a->scrW, a->scrH, nu, &W, &H);
    /* anim_init_sizes 会把动画尺寸重置成默认视图，这里立刻恢复当前视图，
       避免跨屏时卡片尺寸突然跳一下 */
    anim_view_size(&a->anim, a->ui.view, &w, &h, &r);
    a->anim.curW = w; a->anim.curH = h; a->anim.curR = r;

    /* 先成功分配新画布再换掉旧的：几十 MB 的 DIB 分配失败时保持原状，
       不会出现"旧画布已释放、新画布没建成"的崩坏状态 */
    if (!canvas_init(&nc, W, H, nu, ss)) return;
    canvas_shutdown(&a->canvas);
    a->canvas = nc;

    a->u = nu; a->ss = ss; a->winW = W; a->winH = H;

    /* 字号按 u 生成 HFONT、字形缓存按 (码点,字号) 索引，两者都必须重建 */
    font_shutdown(&a->fonts);
    font_init(&a->fonts, nu);

    x = suggested ? suggested->left : (a->scrW - W) / 2;
    y = suggested ? suggested->top  : (int)(TOP_GAP * nu + 0.5);
    SetWindowPos(hwnd, NULL, x, y, W, H, SWP_NOZORDER | SWP_NOACTIVATE);

    anim_geometry(&a->anim, &gw, &gh, &gr);
    ui_layout(a, gw * a->u, gh * a->u);   /* 命中表与画布一起刷新 */
    a->dirty = 1;
}

/* 灵动岛本身的右键菜单**已整体删除**（本轮用户要求）。
   原来这里有一个用 ●/○ 圆点跳卡 + − 隐藏 + × 退出的 popup_menu，
   现在菜单只在系统托盘图标上，见 tray.c。右键点岛什么都不弹。 */

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

    switch (msg) {
    case WM_APP_UPDATE:
        /* 工作线程 / 看门狗 → 主线程：写结果区 + 弹 Qisland 即时通知。
           窗口过程绝不碰网络，也不在这里分配大块内存。 */
        update_on_message(a, wp, lp);
        return 0;
    case WM_DPICHANGED:
        /* 低字 = 新 DPI X；lParam = 系统建议的新窗口矩形。
           自己重建资源并 SetWindowPos，返回 0 告诉系统不用再插手。 */
        window_apply_dpi(a, hwnd, LOWORD(wp), (const RECT *)lp);
        return 0;
    case WM_APP_BALLOON_SIGNAL:
        /* WinEvent 回调只送 (event, HWND)；气泡文本由主线程自己读 */
        balloon_on_signal(a, (DWORD)wp, (HWND)lp);
        return 0;
    case WM_SETCURSOR: {
        /* 官方要求：光标必须在这里「每次鼠标移动都重新 SetCursor」——
           只设一次的话下一次鼠标移动就会被重置；且处理后必须返回 TRUE，
           否则 DefWindowProc 会用窗口类光标（箭头）把它覆盖回去。
           lParam 低字是 WM_NCHITTEST 的结果，只有客户区才由我们决定。 */
        POINT p;
        int ibeam = 0, hand = 0;
        if (LOWORD(lp) != HTCLIENT) break;
        if (!GetCursorPos(&p)) break;
        ScreenToClient(hwnd, &p);
        hand = window_link_cursor(a, p.x, p.y);
        ibeam = window_text_cursor(a, p.x, p.y);
        SetCursor(hand ? window_hand_cursor_handle()
                       : window_cursor_handle(ibeam));
        return TRUE;
    }
    case WM_NCHITTEST: {
        POINT sp;
        int inShape;
        sp.x = GET_X_LPARAM(lp);
        sp.y = GET_Y_LPARAM(lp);
        ScreenToClient(hwnd, &sp);
        /* 悬停气泡会写到主形状之外的透明区。加目标态几何判定，
           让形状外的气泡像素一律 HTTRANSPARENT，不吞桌面点击；
           形状内仍用最终 alpha，保留抗锯齿边缘的软命中。 */
        inShape = anim_inside_target(&a->anim, a->ui.view, a->u,
                                     a->canvas.W, 0.0, sp.x, sp.y);
        return (inShape && canvas_alpha_at(&a->canvas, sp.x, sp.y) > 8)
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
    case WM_APP_TRAY:
        /* 托盘图标回调（NOTIFYICON_VERSION_4）：lParam 低字是事件，
           右键 / 上下文菜单才弹托盘菜单。岛自身的右键菜单已删除。 */
        tray_on_message(a, wp, lp);
        return 0;
    case WM_RBUTTONUP:
    case WM_CONTEXTMENU:
        /* 不在岛上弹任何菜单；交给 DefWindowProc 走默认，不拦截桌面右键。 */
        return 0;
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

    /* 进程 DPI 感知优先由 qisland.manifest 声明（Win10 1703+ PerMonitorV2、
       Win8.1+ PerMonitor）。Win7 不认 manifest 里的 pm / dpiAwareness，
       这里用 SetProcessDPIAware() 兜底；manifest 已生效时该调用返回 FALSE，
       不会把感知级别降回 System DPI。 */
    u32 = LoadLibraryW(L"user32.dll");
    if (u32) pSetDpi = (BOOL (WINAPI *)(void))(void *)
        GetProcAddress(u32, "SetProcessDPIAware");
    if (pSetDpi) pSetDpi();

    a->u  = window_scale_from_dpi(window_query_dpi(NULL));
    a->ss = (a->u > DPI_SS_THRESHOLD) ? 2 : SHAPE_SS;
    a->scrW = GetSystemMetrics(SM_CXSCREEN);
    a->scrH = GetSystemMetrics(SM_CYSCREEN);
    anim_init_sizes(&a->anim, a->scrW, a->scrH, a->u, &a->winW, &a->winH);

    if (!canvas_init(&a->canvas, a->winW, a->winH, a->u, a->ss)) return 0;

    ZeroMemory(&wc, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.style = CS_DBLCLKS;
    wc.lpfnWndProc = window_proc;
    wc.hInstance = a->inst;
    /* 「中间黑」那张是程序图标：exe 资源 + 窗口类大小图标同源。
       windres 把 qisland.rc 编进 exe，ID 见 config.h 的 IDI_QISLAND。 */
    wc.hIcon   = LoadIconW(a->inst, MAKEINTRESOURCEW(IDI_QISLAND));
    wc.hIconSm = wc.hIcon;
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
    a->dpiReady = 1;   /* 资源齐了，之后才允许 WM_DPICHANGED 重建 */
    return 1;
}

void window_shutdown(App *a)
{
    a->dpiReady = 0;
    if (a->hwnd) {
        KillTimer(a->hwnd, IDT_COUNTDOWN);
        UnregisterHotKey(a->hwnd, HOTKEY_ID);
        DestroyWindow(a->hwnd);
        a->hwnd = NULL;
    }
}
