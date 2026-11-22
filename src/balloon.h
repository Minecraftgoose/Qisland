/* balloon.h — Win7 托盘气泡消息监听 */
#ifndef QISLAND_BALLOON_H
#define QISLAND_BALLOON_H

#include "common.h"
#include "config.h"

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

typedef struct {
    DWORD  dwSize;
    UINT   uTitleBitmap;
    UINT   cch;
    WCHAR *pszTitle;
} BALLOON_TTGETTITLE;

typedef struct {
    HWND          hwnd;
    HWINEVENTHOOK hook;
    int           enabled;
    int           hookOk;
    DWORD         trayPid;

    DWORD  lastAt, lastAny;
    int    hits, suppressed, events;
    int    classHits, styleHits, pidHits, textFails;
    int    lastTextLen;
    WCHAR  lastText[BALLOON_TEXT_MAX + 1];
    int    titleHits, titleFails;
    int    lastTitleIcon;
    WCHAR  lastTitle[BALLOON_TITLE_MAX + 1];
} Balloon;

void balloon_init(App *a);
void balloon_shutdown(App *a);

void balloon_on_signal(App *a, DWORD event, HWND hwnd);

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

int  balloon_read_text(HWND hwnd, WCHAR *out, int cap);
int  balloon_read_title(HWND hwnd, WCHAR *out, int cap, UINT *iconOut);

#endif /* QISLAND_BALLOON_H */
