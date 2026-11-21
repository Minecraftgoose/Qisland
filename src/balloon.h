/* balloon.h — Win7 托盘气泡消息监听（替代原即时消息专属监听）
 *
 * 官方文档依据（Microsoft Learn，已精读）：
 *   NOTIFYICONDATA / NIF_INFO      szInfo / szInfoTitle / dwInfoFlags / uTimeout；
 *                                  uCallbackMessage 的 NIN_BALLOONSHOW /
 *                                  NIN_BALLOONTIMEOUT / NIN_BALLOONUSERCLICK
 *                                  **只发给注册图标的那个 hWnd**，不向第三方广播。
 *                                  XP+（Shell32 5.0/6.0），Win7 全覆盖。
 *   Shell_NotifyIcon(NIM_MODIFY)   同一时刻任务栏只显示一个气泡，新的排队；XP+。
 *   SetWinEventHook                WINEVENT_OUTOFCONTEXT：回调不映射进生成进程、
 *                                  系统排队、异步但保序；**调用线程必须有消息循环**；
 *                                  WINEVENT_SKIPOWNPROCESS 跳过本进程；Win2000+。
 *   WinEvents / Active Accessibility  客户端必须先 SetWinEventHook 才会收到事件；
 *                                  系统为系统提供的 UI 元素发事件；回调里可用
 *                                  AccessibleObjectFromEvent 取 IAccessible。
 *   Event Constants                EVENT_OBJECT_CREATE = 0x8000（窗口对象创建），
 *                                  EVENT_OBJECT_SHOW   = 0x8002（隐藏对象显示）。
 *   Tooltip Styles                 TTS_BALLOON（版本 5.80）= 卡通气球外观；
 *                                  Win7 实测样式位 0x40、窗口类 tooltips_class32、
 *                                  owner 恒为 explorer.exe。
 *   TTM_SETTITLE (WM_USER+33)     给气球加「标题字符串 + 图标」，Vista+；
 *                                  wParam = TTI_NONE/INFO/WARNING/ERROR(及 _LARGE)，
 *                                  >TTI_ERROR_LARGE 视为 HICON；lParam 标题 <=100 TCHAR。
 *   TTM_GETTITLE (WM_USER+35)     读回标题信息，lParam = TTGETTITLE*；要求
 *                                  comctl32.dll v6 manifest（explorer 侧满足）；Vista+。
 *   TTGETTITLE                    { dwSize, uTitleBitmap, cch, WCHAR *pszTitle }。
 *   UIA_ToolTipOpenedEventId       20000，打开工具提示时引发；最低客户端 Windows XP。
 *                                  （本版未用 UIA，Name 属性是正文而非标题，留作 Phase 2）
 *   WM_GETTEXT                     复制窗口文本；跨进程时系统封送 < WM_USER 的系统消息。
 *                                  社区 Win7 实测：气泡文本要用 WM_GETTEXTLENGTH +
 *                                  WM_GETTEXT，GetWindowText 常为空。
 *   消息封送边界                   官方：系统只对 0..WM_USER-1 的系统消息自动封送
 *                                  指针；>=WM_USER 不做 marshalling。TTM_GETTITLE 是
 *                                  WM_USER+35 且气泡窗口属于 explorer.exe，所以必须
 *                                  用 VirtualAllocEx/WriteProcessMemory 把 TTGETTITLE
 *                                  与字符串缓冲放在 explorer 进程里，再 ReadProcessMemory
 *                                  读回 —— 绝不能把本进程指针直接 SendMessage 过去。
 *   UnhookWinEvent                退出前反注册。
 *
 * 结论：**能收到**，但不是「系统把气泡消息发给本程序」。NIF_INFO 的回调只给图标
 * 拥有者；第三方只能**旁观** explorer 创建的气泡窗口：
 *   SetWinEventHook(CREATE/SHOW, OUTOFCONTEXT) → 过滤 tooltips_class32 +
 *   TTS_BALLOON + explorer pid → 只 PostMessage(HWND) 回主线程 → 主线程
 *   WM_GETTEXT 读正文 szInfo、TTM_GETTITLE 读标题 szInfoTitle + 标题图标
 *   → notify_show_*() 弹即时通知。
 * 没有全局钩子链（区别于 WH_MOUSE_LL 那次卡死）、没有后台线程、没有轮询、
 * 回调内不碰 App / Canvas / Ui / Notify、不置脏。
 *
 * 关于「程序名 / 图标」的准确边界（不要对外说成能读 exe 名）：
 *   · szInfoTitle 是**应用自己填的标题文本**（如 Greenshot 填 "Greenshot"），
 *     不是系统反查的进程名 / exe 名；别的程序可能填「导出完成」「提示」甚至空。
 *   · uTitleBitmap 是**气泡自带的标题图标类型**（TTI_INFO 蓝圈 i 等），不是
 *     该程序的托盘图标 / 可执行文件图标；>TTI_ERROR_LARGE 时是 explorer 进程的
 *     HICON 句柄，跨进程无效，必须忽略。
 *   · 真正的来源程序名与程序图标仍只能靠托盘图标枚举 + 进程反查（未实现）。
 *   · 标题读不到 → 程序名兜底「托盘消息」；图标读不到 → FA 消息兜底图标。
 *     不编造发送者或正文。
 *
 * 覆盖局限：只对使用**标准 NIF_INFO 托盘气泡**的程序有效；自绘气泡、应用内
 * 弹窗、Win10 Toast 收不到。若 explorer 侧 tooltip 是 v5 控件不响应 TTM_GETTITLE，
 * 标题静默回退，正文仍可用。
 */
#ifndef QISLAND_BALLOON_H
#define QISLAND_BALLOON_H

#include "common.h"
#include "config.h"

/* 部分 SDK 头文件未给出这些常量，按官方定义补齐 */
#ifndef EVENT_OBJECT_CREATE
#define EVENT_OBJECT_CREATE 0x8000L
#endif
#ifndef EVENT_OBJECT_SHOW
#define EVENT_OBJECT_SHOW   0x8002L
#endif
#ifndef OBJID_WINDOW
#define OBJID_WINDOW        ((LONG)0x00000000)
#endif
#ifndef CHILDID_SELF
#define CHILDID_SELF        0
#endif
#ifndef WINEVENT_OUTOFCONTEXT
#define WINEVENT_OUTOFCONTEXT    0x0000
#endif
#ifndef WINEVENT_SKIPOWNPROCESS
#define WINEVENT_SKIPOWNPROCESS  0x0002
#endif

/* Tooltip 消息与图标常量（commctrl.h；本工程不依赖 v6 manifest 头，
   只按官方数值自行补齐，避免引入 comctl32 链接依赖） */
#ifndef TTM_SETTITLE
#define TTM_SETTITLE    (WM_USER + 33)
#endif
#ifndef TTM_GETTITLE
#define TTM_GETTITLE    (WM_USER + 35)
#endif
#define BALLOON_TTI_NONE            0
#define BALLOON_TTI_INFO            1
#define BALLOON_TTI_WARNING         2
#define BALLOON_TTI_ERROR           3
#define BALLOON_TTI_INFO_LARGE      4
#define BALLOON_TTI_WARNING_LARGE   5
#define BALLOON_TTI_ERROR_LARGE     6

/* TTGETTITLE：64 位下指针 8 字节、结构体 24 字节，与 explorer 同架构。
   跨进程时整块结构 + 字符串缓冲都放在目标进程地址空间内。 */
typedef struct {
    DWORD  dwSize;
    UINT   uTitleBitmap;
    UINT   cch;
    WCHAR *pszTitle;
} BALLOON_TTGETTITLE;

typedef struct {
    HWND          hwnd;          /* 主窗口：PostMessage 目标 */
    HWINEVENTHOOK hook;          /* SetWinEventHook 返回值 */
    int           enabled;       /* --balloonoff 时关闭 */
    int           testMode;      /* --balloontest：允许本进程假气泡 */
    int           hookOk;
    DWORD         trayPid;       /* explorer / Shell_TrayWnd 的 pid，0 = 不过滤 */

    /* 去抖与诊断（只由主线程写） */
    DWORD  lastAt, lastAny;
    int    hits, suppressed, events;
    int    classHits, styleHits, pidHits, textFails;
    int    lastTextLen;
    WCHAR  lastText[BALLOON_TEXT_MAX + 1];
    int    titleHits, titleFails;              /* TTM_GETTITLE 成功 / 失败次数 */
    int    lastTitleIcon;                      /* 上次读到的 uTitleBitmap */
    WCHAR  lastTitle[BALLOON_TITLE_MAX + 1];  /* 上次读到的 szInfoTitle */
} Balloon;

/* 生命周期：window_init 之后、消息循环之前 init；退出前 shutdown */
void balloon_init(App *a);
void balloon_shutdown(App *a);

/* WndProc 里处理 WM_APP_BALLOON_SIGNAL；也可被 --balloontest 直接注入。
   event 仅作诊断，hwnd 是气泡窗口句柄，文本由主线程自己读（不跨线程传指针）。 */
void balloon_on_signal(App *a, DWORD event, HWND hwnd);

/* 只读查询，供 --diag / --balloontest */
int          balloon_enabled(const App *a);
int          balloon_hook_ok(const App *a);
int          balloon_events(const App *a);
int          balloon_hits(const App *a);
int          balloon_suppressed(const App *a);
int          balloon_class_hits(const App *a);
int          balloon_style_hits(const App *a);
int          balloon_text_len(const App *a);
const WCHAR *balloon_last_text(const App *a);
int          balloon_title_hits(const App *a);
int          balloon_title_fails(const App *a);
int          balloon_last_title_icon(const App *a);
const WCHAR *balloon_last_title(const App *a);
DWORD        balloon_tray_pid(const App *a);

/* --balloontest 专用 */
void balloon_test_rewind(App *a, DWORD ms);
/* 造一个「假气泡」窗口：类名含 tooltips_class32，可选 TTS_BALLOON 样式位，
   窗口文本可被 WM_GETTEXT 读出。返回 1 成功，句柄写 *out。 */
int  balloon_test_make(const WCHAR *text, int balloonStyle, HWND *out);
void balloon_test_destroy(HWND hwnd);
/* 读文本链路单独暴露给测试（主线程调用） */
int  balloon_read_text(HWND hwnd, WCHAR *out, int cap);
/* 读标题 szInfoTitle + 标题图标 uTitleBitmap（跨进程 TTM_GETTITLE） */
int  balloon_read_title(HWND hwnd, WCHAR *out, int cap, UINT *iconOut);

#endif /* QISLAND_BALLOON_H */
