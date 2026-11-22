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
    int       trayOK;
    int       dpiReady;
    int       pressId, pressX, pressY, pressMoved;
    int       noteSel;
    int       endNotified;
    int       wheelAccum;
    DWORD     lastRender, lastWheel, renderCount;
    char      lastClock[16];
    char      lastSecClock[12];
};

#endif /* QISLAND_APP_H */
