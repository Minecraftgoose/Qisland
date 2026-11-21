/* main.c — Qisland 入口：装配各模块、消息循环、退出清理
 *
 * 模块分工：
 *   canvas  逐像素预乘 BGRA 画布 / SDF 光栅化
 *   font    私有 FA 字体加载 / 4x4 面积覆盖率字形渲染
 *   sys     Core Audio 音量 / WMI + DDC-CI 亮度 / 启动进程
 *   anim    视图尺寸 / 展开弹簧
 *   countdown 倒计时状态机
 *   date    日期时钟：带秒时钟 + 农历/干支/节气/周数，纯函数无状态
 *   lunar   本地农历库封装：vendored 纯 C 库，同步查询，无网络/无线程
 *   perf    性能监测：GetSystemTimes / GlobalMemoryStatusEx / 磁盘 + 圆环绘制
 *   clip    剪贴板历史：AddClipboardFormatListener / CF_UNICODETEXT / 自绘列表
 *   notify  即时通知：内存态覆盖视图、HICON/FA 图标、程序名+详细信息、无持久化
 *   balloon Win7 托盘气泡监听：SetWinEventHook(OUTOFCONTEXT) 旁观 + PostMessage 回主线程
 *   ui      仅图标布局 / 命中 / 交互 / 整帧渲染
 *   window  分层窗口 / Raw Input / 热键 / 菜单
 */
#include "app.h"
#include "config.h"
#include "window.h"
#include "canvas.h"
#include "font.h"
#include "sys.h"
#include "anim.h"
#include "countdown.h"
#include "note.h"
#include "perf.h"
#include "clip.h"
#include "cards.h"
#include "notify.h"
#include "balloon.h"
#include "tray.h"
#include "input.h"
#include "ui.h"
#include "update.h"

DWORD qs_now(void) { return GetTickCount(); }
double qs_clamp(double v, double a, double b) { return v < a ? a : (v > b ? b : v); }
int    qs_iclamp(int v, int a, int b) { return v < a ? a : (v > b ? b : v); }
void   qs_clock_w(WCHAR *out, size_t cap)
{
    SYSTEMTIME st;
    GetLocalTime(&st);
    _snwprintf(out, (int)cap, L"%02d:%02d", st.wHour, st.wMinute);
    out[cap - 1] = 0;
}
void   qs_clock_a(char *out, size_t cap)
{
    WCHAR w[16];
    qs_clock_w(w, 16);
    WideCharToMultiByte(CP_UTF8, 0, w, -1, out, (int)cap, NULL, NULL);
}

static int single_instance(void)
{
    HANDLE mx = CreateMutexW(NULL, TRUE, MUTEX_NAME);
    if (mx && GetLastError() == ERROR_ALREADY_EXISTS) {
        HWND old = FindWindowW(WND_CLASS, NULL);
        if (old) {
            DWORD_PTR res = 0;
            LRESULT ok = SendMessageTimeoutW(old, WM_APP_SHOW, 0, 0,
                                             SMTO_ABORTIFHUNG | SMTO_BLOCK,
                                             400, &res);
            if (ok != 0) { CloseHandle(mx); return 1; }
        }
    }
    return 0;   /* 由调用方继续持有 mx（进程退出时系统回收） */
}

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE prev, PWSTR cmdline, int show)
{
    static App app;
    App *a = &app;
    int active = 0;
    int swAnim = 0;           /* 设置页开关弹簧是否未收敛，并入帧驱动 */

    (void)prev; (void)cmdline; (void)show;
    a->inst = inst;
    a->visible = 1;
    a->dirty = 1;

    if (single_instance()) return 0;

    ui_init(&a->ui);
    tooltip_init(&a->tooltip);
    settings_init(&a->settings);   /* 读收起偏好 + 同步注册表自启动真值 */
    if (!window_init(a)) {
        MessageBoxW(NULL, L"Qisland: window init failed", APP_NAME, MB_OK | MB_ICONERROR);
        goto cleanup;
    }
    if (!font_init(&a->fonts, a->u)) {
        MessageBoxW(NULL, L"Qisland: font init failed", APP_NAME, MB_OK | MB_ICONERROR);
        goto cleanup;
    }
    sys_init(&a->sys);
    countdown_init(&a->cd);
    note_init(&a->note);
    perf_init(&a->perf);
    clip_init(&a->clip, a);       /* 需要 hwnd：注册剪贴板 listener */
    notify_init(&a->notify);
    balloon_init(a);            /* 需要 hwnd：注册 WinEvent 气泡监听（无后台线程） */
    tray_init(a);               /* 需要 hwnd：系统托盘图标 + 右键菜单（NIM_ADD） */
    input_init(a);

    a->anim.lastTick = qs_now();
    a->lastRender = a->anim.lastTick;
    a->dirty = 0;
    ui_render(a);

    for (;;) {
        MSG msg;
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) goto cleanup;
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        {
            DWORD now = qs_now();
            double dt = (double)(now - a->anim.lastTick) / 1000.0;
            if (dt < 0.0) dt = 0.0;
            if (dt > 0.05) dt = 0.05;
            a->anim.lastTick = now;
            anim_tick(&a->anim, dt, a->ui.view);
            /* 设置页开关弹簧：和展开弹簧共用同一帧驱动，不设新定时器、
               不建线程；收敛后返回 0，静止零重绘不变。 */
            swAnim = settings_tick(&a->settings, dt);
            ui_poll(a);
            /* listener 不可用时按剪贴板序列号兜底；有 listener 时直接返回，
               不置脏、不产生任何重绘 */
            clip_poll(&a->clip, a);
            /* 通知只在 active 时检查过期，静止零开销、不置脏 */
            notify_tick(&a->notify, a, now);

            /* 性能页展开时才采样；每秒一次，有新样本才置脏。
               静止 / 收起 / 其他视图一律不采样，零重绘策略不变。 */
            if (a->visible && a->hover &&
                cards_draw(a->ui.view) == DRAW_PERF &&
                perf_tick(&a->perf, now))
                a->dirty = 1;

            /* 日期时钟页带秒：只有它展开且可见时才比较秒串，每秒置一次脏。
               其它页面（含收起态）完全不进这段，静止零重绘策略不变。
               用 cards_draw() 判定而非写死下标，以后插卡不用同步。
               历法数据由 src/vendor/ 本地库同步提供，无请求、无缓存、
               无 serial、无后台线程，不需要在这里做任何触发。 */
            if (a->visible && a->anim.exp > 0.02 &&
                !notify_active(&a->notify) &&
                cards_draw(a->ui.view) == DRAW_DATE) {
                SYSTEMTIME st;
                char cs[12];
                GetLocalTime(&st);
                _snprintf(cs, (int)sizeof(cs), "%02d:%02d:%02d",
                          st.wHour, st.wMinute, st.wSecond);
                cs[sizeof(cs) - 1] = 0;
                if (strcmp(cs, a->lastSecClock) != 0) {
                    memcpy(a->lastSecClock, cs, sizeof(cs));
                    a->dirty = 1;
                }
            }


            {
                char c[16];
                int changed, need;
                qs_clock_a(c, sizeof(c));
                active = anim_active(&a->anim, a->ui.view) || swAnim;
                changed = (strcmp(c, a->lastClock) != 0);
                need = active || a->dirty || changed;
                if (a->dirty) a->dirty = 0;
                if (need && a->visible) {
                    if (!active || now - a->lastRender >= 16) {
                        ui_render(a);
                        a->lastRender = now;
                    }
                }
            }
        }
        Sleep(active ? 8 : 24);
    }

cleanup:
    update_shutdown();          /* 先让检查更新的线程收手，别再往将销毁的窗口发消息 */
    input_shutdown(a);          /* 编辑中会先保存便签 */
    note_shutdown(&a->note);
    clip_shutdown(&a->clip, a);
    /* 通知无持久化：退出什么都不用写，内存态随进程消失 */
    balloon_shutdown(a);        /* UnhookWinEvent，退出前反注册 */
    tray_shutdown(a);           /* 必须在 window_shutdown 之前：NIM_DELETE 需要 hwnd */
    perf_shutdown(&a->perf);
    window_shutdown(a);
    font_shutdown(&a->fonts);
    sys_shutdown(&a->sys);
    canvas_shutdown(&a->canvas);
    return 0;
}
