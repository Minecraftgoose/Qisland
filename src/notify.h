/* notify.h — 即时通知系统
 *
 * 约束（按用户口径）：
 *   · 通知卡片 = 中卡片尺寸（480×130），不进 cards[] 导航表、不占竖点/横点；
 *   · 左侧 = 软件图标（HICON）或 Font Awesome 字形；
 *   · 右侧 = 程序名，换行，详细信息；
 *   · **不储存、不写盘、不排队**：内存里只有当前一条，后来的直接顶掉前一条，
 *     过期或点掉即消失，退出进程什么都不剩；
 *   · 第一个接入方：倒计时自然归零 → 弹一条提醒；
 *   · 0.3.1 起每次弹出播一声 assets\notice.wav（PlaySoundW 异步，静默失败）。
 *     读的是运行时资源文件，不是通知数据落盘；缺文件 / 无音频设备时只是没声音。
 *
 * 为什么图标自己画：
 *   分层窗口用 UpdateLayeredWindow 上屏，WS_CHILD 控件不会被合成；
 *   HICON 也不能 DrawIcon 到最终 alpha 位图（GDI 在 32bpp DIB 上不保证写
 *   alpha）。所以先把图标画进一块独立的不透明黑底 DIB，读回 RGB 亮度当
 *   覆盖率、反预乘还原色相，再用 canvas_blend_px 叠进 alpha 画布 —— 与
 *   文字/FA 字形用的是同一套「覆盖率」思路，边缘同样无锯齿。
 */
#ifndef QISLAND_NOTIFY_H
#define QISLAND_NOTIFY_H

#include "common.h"
#include "config.h"

#define NOTIFY_APP_MAX    64
#define NOTIFY_DETAIL_MAX 256
#define NOTIFY_MS_DEFAULT 5000

typedef struct {
    int      active;
    unsigned faIcon;                 /* FA 码点；0 = 用 HICON */
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

/* 弹一条：faIcon 非 0 走 FA 字形；否则用 hIcon。
   ms = 显示毫秒数，到点自动消失。新通知直接替换当前这条（即时，不排队）。 */
void   notify_show_fa(Notify *n, App *a, unsigned faIcon,
                      const WCHAR *appName, const WCHAR *detail, DWORD ms);
void   notify_show_icon(Notify *n, App *a, HICON hIcon,
                        const WCHAR *appName, const WCHAR *detail, DWORD ms);

void   notify_dismiss(Notify *n, App *a);
void   notify_tick(Notify *n, App *a, DWORD now);  /* 每帧调用，过期才动作 */
void   notify_draw(Notify *n, App *a, double W, double H, double k);

/* 从 exe / ico 文件取 HICON（SHGetFileInfoW）；调用方负责 DestroyIcon。
   取不到返回 NULL，调用方应回退到 FA 字形。 */
HICON  notify_icon_from_file(const WCHAR *path);

/* 诊断 */
int    notify_shown(const Notify *n);
int    notify_dismissed(const Notify *n);
int    notify_expired(const Notify *n);
const WCHAR *notify_app_name(const Notify *n);
const WCHAR *notify_detail(const Notify *n);

#endif /* QISLAND_NOTIFY_H */
