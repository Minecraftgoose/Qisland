/* balloon.h — Win7 托盘气泡消息监听（替代原即时消息专属监听） */
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

/* TTGETTITLE：64 位下指针 8 字节、结构体 24 字节，与 explorer 同架构。 跨进程时整块结构 + 字符串缓冲都放在目标进程地址空间内。 */
typedef struct {
    DWORD  dwSize;
    UINT   uTitleBitmap;
    UINT   cch;
    WCHAR *pszTitle;
} BALLOON_TTGETTITLE;

typedef struct {
    HWND          hwnd;          /* 主窗口：PostMessage 目标 */
    HWINEVENTHOOK hook;          /* SetWinEventHook 返回值 */
    int           enabled;       /* 始终启用； 保留字段供状态查询 */
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

/* 生命周期：window_init 之后、消息循环之前 init； 退出前 shutdown */
void balloon_init(App *a);
void balloon_shutdown(App *a);

/* WndProc 里处理 WM_APP_BALLOON_SIGNAL。 event 仅作诊断，hwnd 是气泡窗口句柄，文本由主线程自己读（不跨线程传指针）。 */
void balloon_on_signal(App *a, DWORD event, HWND hwnd);

/* 只读状态查询 */
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

/* 读文本链路（主线程调用） */
int  balloon_read_text(HWND hwnd, WCHAR *out, int cap);
/* 读标题 szInfoTitle + 标题图标 uTitleBitmap（跨进程 TTM_GETTITLE） */
int  balloon_read_title(HWND hwnd, WCHAR *out, int cap, UINT *iconOut);

#endif /* QISLAND_BALLOON_H */
