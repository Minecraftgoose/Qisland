/* ui.h — 卡片布局、仅图标控件、命中判定、点击/拖动/滚轮 */
#ifndef QISLAND_UI_H
#define QISLAND_UI_H

#include "common.h"
#include "note.h"
#include "clip.h"

enum {
    H_NONE = 0,
    H_WHEEL,          /* 最左竖条：滚轮切换视图（**仅竖向阶段注册**，横向组不注册） */
    H_HIDE,           /* 2x2 左上：隐藏 */
    H_BRI,            /* 2x2 右上：亮度，单击切模式 / 拖动调值 */
    H_VOL,            /* 2x2 左下：音量，单击切模式 / 拖动调值 */
    H_TASKMGR,        /* 2x2 右下：任务管理器 */
    H_PLAY,           /* 启动 / 暂停 */
    H_RESET,          /* 重新倒计时 */
    H_SLIDER_VOL,     /* 计时器区上条：音量滑块 */
    H_SLIDER_BRI,     /* 计时器区下条：亮度滑块 */
    H_NUM_H,          /* 计时器 HH：悬浮滚轮调小时 */
    H_NUM_M,          /* 计时器 MM：悬浮滚轮调分钟 */
    H_NUM_S,          /* 计时器 SS：悬浮滚轮调秒 */
    H_NOTE,           /* 便签文本区：点击开始输入、滚轮滚动内容 */
    H_CLIP,           /* 剪贴板历史列表：点击某条写回剪贴板 / 滚轮滚列表 */
    H_CLIP_CLEAR,     /* 右上角 trash-can：清空历史（不动系统剪贴板） */
    H_NOTIFY,         /* 通知卡片本体：点一下关闭 */
    H_NOTIFY_CLOSE,   /* 通知右上角 xmark：关闭 */
    H_HDOTS,          /* 底部横向圆点：滚轮组内横切 / 点击圆点跳卡 */
    H_SET_TAB_GEN,    /* 设置页左栏「通用」 */
    H_SET_TAB_UPDATE, /* 设置页左栏「更新」 */
    H_SET_TAB_ABOUT,  /* 设置页左栏「关于」 */
    H_SET_UPDATE_BTN, /* 更新页「检查更新」按钮 */
    H_SET_LEAVE,      /* 收起模式：鼠标离开岛收起 */
    H_SET_OUTSIDE,    /* 收起模式：鼠标点击岛外收起 */
    H_SET_FIRST,      /* 收起模式：岛回到第一个卡片时收起 */
    H_SET_NEVER,      /* 收起模式：永不收起 */
    H_SET_AUTOSTART,  /* 开机自启动 */
    H_ABOUT_REPO      /* 关于页仓库链接：点击用系统默认浏览器打开 */
};

typedef struct { RECT rc; int id; } HitRect;
typedef struct { double x, y, w, h; } DBoxNotify;

typedef struct {
    int     view;
    int     mode;          /* 0 计时器 / 1 双滑块 */
    int     hoverSeg;      /* 1 / 0=HH / 1=MM / 2=SS，仅用于视觉高亮 */
    HitRect hits[16];
    int     hitN;
    RECT    wheelRect;
    RECT    hdotsRect;        /* 底部横向圆点命中区（非横向组时为 0） */
} Ui;

void  ui_init(Ui *u);
void  ui_set_view(App *a, int v);
void  ui_layout(App *a, double w, double h);
void  ui_draw(App *a, double w, double h, double k);
int   ui_hit(const App *a, int x, int y);
void  ui_activate(App *a, int id, int x, int y);
void  ui_click(App *a, int x, int y);
void  ui_note_box(const App *a, NoteBox *box);
void  ui_clip_box(const App *a, ClipBox *box);
void  ui_clip_clear_box(const App *a, ClipBox *box);
void  ui_notify_boxes(const App *a, DBoxNotify *body, DBoxNotify *close);

double ui_dots_pitch(double cardH, double u);
void   ui_hdots_center(double L, double W, double H, double pitch,
                       int n, int i, double *cx, double *cy);
void  ui_drag(App *a, int id, int x, int y);
void  ui_end_drag(App *a, int id);
void  ui_wheel(App *a, int delta);     /* 竖向滚轮：往下=下一个卡片 */
void  ui_wheel_h(App *a, int delta);   /* 水平滚轮：组内横向切换 */
void  ui_adjust(App *a, int seg, int step);
void  ui_poll(App *a);
void  ui_render(App *a);

/* 倒计时自然归零：自动展开 + 强制显示； 用户任何交互后解除 */
void  ui_countdown_ended(App *a);
void  ui_clear_end(App *a);

const RECT *ui_wheel_rect(const App *a);
RECT  ui_hit_rect(const App *a, int id);
void  ui_click_id(App *a, int id);

#endif /* QISLAND_UI_H */
