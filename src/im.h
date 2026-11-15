/* im.h — QQ / 微信即时消息监听（仅 Windows 7 及以上）
 *
 * 架构（按「分而治之」）：
 *   · 微信   RegisterShellHookWindow + HSHELL_FLASH，在**主 UI 线程**的消息
 *            循环里处理，不额外开线程、不装全局钩子。
 *   · QQ     **独立后台线程**每 IM_POLL_MS 轮询托盘图标 / 进程窗口状态，
 *            只通过 PostMessage(WM_APP_IM_SIGNAL) 把 (kind, pid) 送回主线程。
 *   · 主线程 统一去抖 → 由 pid 反查 exe → 取软件 HICON → notify_show_icon()。
 *
 * 线程安全：后台线程不读也不写 App / Canvas / Notify 的任何字段，状态全在
 * 自己的栈上；诊断计数用 Interlocked* 累加。信号只携带 DWORD pid，不跨线程
 * 传指针或字符串。退出时置 stop 标志并 WaitForSingleObject，不留野线程。
 *
 * 官方依据（已核过 Microsoft Learn）：
 *   RegisterShellHookWindow   HSHELL_FLASH 在文档表格中，lParam = 需闪烁的
 *                             窗口 HWND；最低 Win2000，SDK XP SP1+ 才有。
 *   RegisterWindowMessageW    同字符串返回同一 ID，注册持续到会话结束；Win2000+。
 *   QueryFullProcessImageNameW Vista+，句柄需 PROCESS_QUERY_LIMITED_INFORMATION。
 *   Shell_NotifyIcon(NIM_MODIFY) **不向其他应用广播**任何通知 —— 这是 QQ 只能
 *                             轮询的原因，也说明托盘 toolbar 属于未文档化内部结构。
 *   TB_GETBUTTONINFO / TBBUTTONINFO  Vista+；跨进程必须 VirtualAllocEx +
 *   WriteProcessMemory + ReadProcessMemory，且用 SendMessageTimeout 设超时。
 */
#ifndef QISLAND_IM_H
#define QISLAND_IM_H

#include "common.h"

/* winuser.h 在部分 SDK / mingw 版本里只给了 HSHELL_HIGHBIT，没给 HSHELL_FLASH。
   按官方定义补上：HSHELL_FLASH = HSHELL_HIGHBIT | HSHELL_REDRAW = 0x8006。 */
#ifndef HSHELL_HIGHBIT
#define HSHELL_HIGHBIT 0x8000
#endif
#ifndef HSHELL_FLASH
#define HSHELL_FLASH   (HSHELL_HIGHBIT | 0x0006)
#endif

typedef struct {
    HWND   hwnd;                 /* 主窗口：PostMessage 目标 */
    UINT   shellMsg;             /* RegisterWindowMessageW(L"SHELLHOOK") */
    int    registered;           /* RegisterShellHookWindow 是否成功 */
    int    enabled;              /* --imoff 时关闭 */
    int    testMode;             /* --imtest：允许伪造进程名跑完整链路 */

    HANDLE thread;               /* QQ 轮询线程 */
    volatile LONG stop;          /* Interlocked 停标志 */

    /* HICON 由本模块缓存并拥有：通知 active 期间每帧重绘都要用它，
       不能弹完就 DestroyIcon。im_shutdown() 统一销毁。 */
    HICON  icoWeChat, icoQQ;
    WCHAR  exeWeChat[MAX_PATH], exeQQ[MAX_PATH];

    /* 去抖与诊断（只由主线程写） */
    DWORD  lastAt[4], lastAny;
    int    hits[4], suppressed, shellMsgs, shellHits;
    WCHAR  lastApp[64];

    /* 后台线程诊断（Interlocked 累加，主线程用 Interlocked 读） */
    volatile LONG trayPolls, trayOK, winPolls, threadExited;
} Im;

/* 生命周期：window_init 之后、消息循环之前调 init；退出前调 shutdown */
void im_init(App *a);
void im_shutdown(App *a);

/* WndProc 里调用：处理已注册的 SHELLHOOK 消息；返回 1 表示该消息已识别。 */
int  im_shell_message(App *a, UINT msg, WPARAM wp, LPARAM lp);

/* WndProc 里处理 WM_APP_IM_SIGNAL；也可被 --imtest 直接调用注入。 */
void im_on_signal(App *a, int kind, DWORD pid);

/* 只读查询，供 --diag / --imtest */
int         im_enabled(const App *a);
int         im_hook_ok(const App *a);
int         im_shell_api_present(const App *a);  /* user32 是否导出该 API */
UINT        im_shell_msg(const App *a);
int         im_thread_running(const App *a);
int         im_hits(const App *a, int kind);
int         im_suppressed(const App *a);
int         im_shell_msgs(const App *a);
long        im_tray_polls(const App *a);
long        im_tray_ok(const App *a);
long        im_win_polls(const App *a);
const WCHAR *im_last_app(const App *a);
long        im_thread_exited(const App *a);

/* --imtest 专用：把去抖时间戳往前拨，等价于「假装冷却已过」。
   只在 testMode 下生效，真机逻辑完全不变。 */
void im_test_rewind(App *a, DWORD ms);

/* --imtest 专用：提前停掉后台线程并确认已干净退出（不重启）。 */
int  im_join_for_test(App *a);

#endif /* QISLAND_IM_H */
