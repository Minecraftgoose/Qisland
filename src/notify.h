/* notify.h — 即时通知系统 */
#ifndef QISLAND_NOTIFY_H
#define QISLAND_NOTIFY_H

#include "common.h"
#include "config.h"

#define NOTIFY_APP_MAX    64
#define NOTIFY_DETAIL_MAX 256
#define NOTIFY_MS_DEFAULT 5000

typedef struct {
    int      active;
    unsigned faIcon;                 /* FA 码点； 0 = 用 HICON */
    HICON    hIcon;                  /* 外部提供，模块不负责 DestroyIcon */
    WCHAR    appName[NOTIFY_APP_MAX];
    WCHAR    detail[NOTIFY_DETAIL_MAX];
    DWORD    until;                  /* 过期时刻（GetTickCount） */
    DWORD    started;

    int      savedView, savedMode;   /* 弹之前的状态，dismiss 时还原 */
    int      forcedVisible;          /* 弹之前是隐藏的，dismiss 后仍隐藏 */

    /* 诊断计数（纯内存，不落盘） */
    int      shown, dismissed, expired, replaced;
} Notify;

void   notify_init(Notify *n);
int    notify_active(const Notify *n);

/* 弹一条：faIcon 非 0 走 FA 字形； 否则用 hIcon。 */
void   notify_show_fa(Notify *n, App *a, unsigned faIcon,
                      const WCHAR *appName, const WCHAR *detail, DWORD ms);
void   notify_show_icon(Notify *n, App *a, HICON hIcon,
                        const WCHAR *appName, const WCHAR *detail, DWORD ms);

void   notify_dismiss(Notify *n, App *a);
void   notify_tick(Notify *n, App *a, DWORD now);  /* 每帧调用，过期才动作 */
void   notify_draw(Notify *n, App *a, double W, double H, double k);

/* 从 exe / ico 文件取 HICON（SHGetFileInfoW）； 调用方负责 DestroyIcon。 */
HICON  notify_icon_from_file(const WCHAR *path);

/* 诊断 */
int    notify_shown(const Notify *n);
int    notify_dismissed(const Notify *n);
int    notify_expired(const Notify *n);
const WCHAR *notify_app_name(const Notify *n);
const WCHAR *notify_detail(const Notify *n);

#endif /* QISLAND_NOTIFY_H */
