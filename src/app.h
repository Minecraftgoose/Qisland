/* app.h — 程序运行时状态聚合。
 * 各模块头文件只用 App* 前向声明（见 common.h），不包含本文件，
 * 因此不存在循环依赖；实现文件包含本文件即可拿到完整状态。 */
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


    /* 测试/诊断开关（命令行） */
    int       bench, diag, wheelTest, gestureTest, cdTest, noteTest, perfTest;
    int       clipTest, notifyTest;
    int       balloonOff, balloonTest;
    char     *dump;
    DWORD     testStart, testElapsed;
    int       wheelSent, wheelBefore, wheelLog[12], wheelLogN;
    int       gestureStage, gestureMode1, gestureMode2, gestureMode3;
    double    gestureVol;
    int       cdStage, cdEnded, cdExpanded, cdVisible, cdCleared;
    double    cdAfterMinUp, cdAfterMinDown, cdAfterSecUp;
    int       cdHitN, cdMode, cdAccum, cdCurX, cdCurY;
    RECT      cdNumR;
    int       noteStage, noteLines0, noteLines1, noteOverflow, noteEditing;
    int       noteLen, noteCaret, noteSysBar, noteRoundtrip, noteTopLine;
    int       noteEditingAfterClick, noteTopLineBefore, noteTopLineUp;
    int       noteSelAll, noteSelShift, noteSelDrag, noteSelReplace, noteSelLit;
    int       noteSelCopy, noteSelUndo, noteSelStart, noteSelEnd;
    int       perfStage, perfSamples0, perfSamples1, perfCpuOK, perfMemOK;
    int       perfDiskOK, perfHwCPU, perfHwGPU, perfHwOS, perfRingsLit;
    int       perfTextLit, perfNoDirtyWhenHidden, perfHiddenSamples;
    /* --cliptest：剪贴板历史端到端 */
    int       clipStage, clipCount0, clipCount1, clipDup, clipSelfSkip;
    int       clipWriteOK, clipRoundtrip, clipListener, clipLit, clipSpill;
    int       clipTopBefore, clipTopAfter, clipScrollOK, clipSelected;
    int       clipSelfNoGrow, clipClickWriteOK, clipEmptyLit, clipThumb;
    /* --notifytest：即时通知系统端到端 */
    int       notifyStage, notifyActive0, notifyActive1, notifyMidSize;
    int       notifyIconLit, notifyTextLit, notifyNoDots, notifyNoTrack;
    int       notifyNoHalo;
    int       notifyDismissOK, notifyViewRestored, notifyExpiredOK;
    int       notifyHIconLit, notifyNoFile, notifyCountdownOK;
    int       notifyShown, notifyNoPersist;
    /* --balloontest */
    int       balloonHookOK, balloonTrayPid, balloonEvents0;
    int       balloonFakeOK, balloonPlainIgnored, balloonSignalActive;
    int       balloonTextOK, balloonSupp, balloonRewindOK;
    int       balloonNoFile, balloonOffOK, balloonEvents;
    int       balloonHits, balloonClassHits, balloonStyleHits;
    int       balloonTextLen, balloonStage;
    /* 本轮：间距 / 旧提示删除 / 滑块模式隐藏 play-reset */
    int       notifyGapOK, notifyOldHintDiff;
    int       sliderPlayHit, sliderResetHit, sliderRightLit;
};

#endif /* QISLAND_APP_H */
