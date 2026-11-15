/* main.c — Qisland 入口：装配各模块、消息循环、退出清理
 *
 * 模块分工：
 *   canvas  逐像素预乘 BGRA 画布 / SDF 光栅化
 *   font    私有 FA 字体加载 / 4x4 面积覆盖率字形渲染
 *   sys     Core Audio 音量 / WMI + DDC-CI 亮度 / 启动进程
 *   anim    视图尺寸 / 展开弹簧
 *   countdown 倒计时状态机
 *   perf    性能监测：GetSystemTimes / GlobalMemoryStatusEx / 磁盘 + 圆环绘制
 *   clip    粘贴板历史：AddClipboardFormatListener / CF_UNICODETEXT / 自绘列表
 *   notify  即时通知：内存态覆盖视图、HICON/FA 图标、程序名+详细信息、无持久化
 *   im      QQ/微信监听：微信 Shell Hook（主线程）+ QQ 后台轮询线程，PostMessage 回主线程
 *   ui      仅图标布局 / 命中 / 交互 / 整帧渲染
 *   window  分层窗口 / Raw Input / 热键 / 菜单
 *   tests   导出 / 诊断 / 端到端验证
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
#include "notify.h"
#include "im.h"
#include "input.h"
#include "ui.h"
#include "tests.h"

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

static void parse_args(App *a, int argc, WCHAR **argv)
{
    for (int i = 1; i < argc; i++) {
        if (!_wcsicmp(argv[i], L"--dump") && i + 1 < argc) {
            char buf[512];
            WideCharToMultiByte(CP_UTF8, 0, argv[i + 1], -1,
                                buf, 512, NULL, NULL);
            a->dump = _strdup(buf);
            i++;
        } else if (!_wcsicmp(argv[i], L"--bench")) a->bench = 1;
        else if (!_wcsicmp(argv[i], L"--diag"))  a->diag = 1;
        else if (!_wcsicmp(argv[i], L"--wheel")) a->wheelTest = 1;
        else if (!_wcsicmp(argv[i], L"--gesture")) a->gestureTest = 1;
        else if (!_wcsicmp(argv[i], L"--cdtest"))  a->cdTest = 1;
        else if (!_wcsicmp(argv[i], L"--notetest")) a->noteTest = 1;
        else if (!_wcsicmp(argv[i], L"--perftest")) a->perfTest = 1;
        else if (!_wcsicmp(argv[i], L"--cliptest")) a->clipTest = 1;
        else if (!_wcsicmp(argv[i], L"--notifytest")) a->notifyTest = 1;
        else if (!_wcsicmp(argv[i], L"--imtest"))     a->imTest = 1;
        else if (!_wcsicmp(argv[i], L"--imoff"))      a->imOff = 1;
    }
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
    int argc = 0;
    WCHAR **argv;
    DWORD startup;
    int active = 0;

    (void)prev; (void)cmdline; (void)show;
    a->inst = inst;
    a->visible = 1;
    a->dirty = 1;

    argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    parse_args(a, argc, argv);
    if (single_instance()) { if (argv) LocalFree(argv); return 0; }

    ui_init(&a->ui);
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
    im_init(a);                 /* 需要 hwnd：注册 Shell Hook 窗口 + 起 QQ 轮询线程 */
    input_init(a);

    a->anim.lastTick = qs_now();
    a->lastRender = a->anim.lastTick;
    a->dirty = 0;
    ui_render(a);

    if (a->dump) { tests_dump(a); goto cleanup; }
    if (a->diag)  { tests_diag(a); goto cleanup; }

    if (a->wheelTest || a->gestureTest || a->cdTest || a->noteTest ||
        a->perfTest || a->clipTest || a->notifyTest || a->imTest) {
        /* 测试态：先展开生成命中区 */
        a->anim.expanded = 1; a->anim.exp = 1.0; a->anim.expV = 0.0;
        a->hover = 1; a->dirty = 0;
        ui_render(a);
        if (a->wheelTest) {
            const RECT *wr = ui_wheel_rect(a);
            if (wr->right > wr->left) {
                POINT p;
                p.x = (wr->left + wr->right) / 2;
                p.y = (wr->top + wr->bottom) / 2;
                ClientToScreen(a->hwnd, &p);
                SetCursorPos(p.x, p.y);
            }
        }
        if (a->cdTest) {
            /* 首帧就必须在卡片内，否则 ui_poll 判定「移开」会把卡片收起，
               之后数字段命中表就退回岛尺寸了 */
            RECT r = ui_hit_rect(a, H_NUM_M);
            if (r.right > r.left) {
                POINT p;
                p.x = (r.left + r.right) / 2;
                p.y = (r.top + r.bottom) / 2;
                ClientToScreen(a->hwnd, &p);
                SetCursorPos(p.x, p.y);
            }
        }
    }

    startup = qs_now();
    a->testStart = startup;
    for (;;) {
        MSG msg;
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) goto done;
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
            ui_poll(a);
            /* listener 不可用时按剪贴板序列号兜底；有 listener 时直接返回，
               不置脏、不产生任何重绘 */
            clip_poll(&a->clip, a);
            /* 通知只在 active 时检查过期，静止零开销、不置脏 */
            notify_tick(&a->notify, a, now);

            /* 性能页展开时才采样；每秒一次，有新样本才置脏。
               静止 / 收起 / 其他视图一律不采样，零重绘策略不变。 */
            if (a->visible && a->hover && a->ui.view == PERF_VIEW &&
                perf_tick(&a->perf, now))
                a->dirty = 1;

            {
                char c[16];
                int changed, need;
                qs_clock_a(c, sizeof(c));
                active = anim_active(&a->anim, a->ui.view);
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
        tests_frame(a);
        if (tests_should_stop(a)) break;
        Sleep(active ? 8 : 24);
    }

done:
    tests_finish(a);

cleanup:
    input_shutdown(a);          /* 编辑中会先保存便签 */
    note_shutdown(&a->note);
    clip_shutdown(&a->clip, a);
    /* 通知无持久化：退出什么都不用写，内存态随进程消失 */
    im_shutdown(a);             /* 先停后台线程并反注册 Shell Hook */
    perf_shutdown(&a->perf);
    window_shutdown(a);
    font_shutdown(&a->fonts);
    sys_shutdown(&a->sys);
    canvas_shutdown(&a->canvas);
    free(a->dump);
    a->dump = NULL;
    if (argv) LocalFree(argv);
    return 0;
}
