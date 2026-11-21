/* app.h — 程序运行时状态聚合。 */
#ifndef QISLAND_APP_H
#define QISLAND_APP_H

#include "common.h"
#include "config.h"
#include "canvas.h"
#include "font.h"
#include "sys.h"
#include "anim.h"
#include "countdown.h"
#include "note.h"
#include "perf.h"
#include "clip.h"
#include "notify.h"
#include "balloon.h"
#include "tooltip.h"
#include "settings.h"
#include "ui.h"

struct App {
    HINSTANCE inst;
    HWND      hwnd;
    double    u;
    int       ss;
    int       scrW, scrH, winW, winH;

    Canvas    canvas;
    FontSet   fonts;
    Sys       sys;
    Anim      anim;
    Countdown cd;
    Note      note;
    Perf      perf;
    Clip      clip;
    Notify    notify;
    Balloon   balloon;
    Tooltip   tooltip;
    Settings  settings;
    Ui        ui;

    int       visible, dirty, hover, prevDown, drag, rawOK;
    int       trayOK;           /* 托盘图标已 NIM_ADD，退出时要 NIM_DELETE */
    int       dpiReady;         /* window_init 完成后才允许 WM_DPICHANGED 重建资源 */
    /* 按下→松开期间记录，用来区分「单击」与「按住拖动」 */
    int       pressId, pressX, pressY, pressMoved;
    int       noteSel;          /* 左键按住便签文本区：正在拖动框选 */
    /* 结束提示：只触发一次展开，用户交互后解除 */
    int       endNotified;
    /* 精细滚轮累加到 WHEEL_DELTA(120) 再走一步，兼容高分辨率滚轮 */
    int       wheelAccum;
    DWORD     lastRender, lastWheel, renderCount;
    char      lastClock[16];
    char      lastSecClock[12];   /* 日期时钟页带秒，只在它展开时比较 */
};

#endif /* QISLAND_APP_H */
