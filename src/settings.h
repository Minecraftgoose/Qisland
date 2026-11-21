/* settings.h — 最后的超大卡片：设置页（左右栏，左栏「通用」「更新」「关于」） */
#ifndef QISLAND_SETTINGS_H
#define QISLAND_SETTINGS_H

#include "common.h"

#define SET_TAB_GENERAL  0
#define SET_TAB_ABOUT    1
#define SET_TAB_UPDATE   2   /* 夹在「通用」与「关于」中间； 值保持 2，旧设置文件里没有它 */

/* 收起模式：单选，0..SET_MODE_COUNT-1，必选其一 */
#define SET_MODE_LEAVE    0
#define SET_MODE_OUTSIDE  1
#define SET_MODE_FIRST    2
#define SET_MODE_NEVER    3
#define SET_MODE_COUNT    4

/* 开关索引：前 4 个对应收起模式，最后一个是开机自启动 */
#define SET_SW_AUTO       4
#define SET_SW_N          5

typedef struct { double x, y, w, h; } SBox;

/* 全部几何用**目标态标称尺寸** W0/H0/L0 算（AGENTS.md 第 4 节第 10 条）， 绘制与命中区共用这一份，动画期间不重排、不重算字号。 */
typedef struct {
    SBox    tabGeneral, tabUpdate, tabAbout;      /* 左栏三个页签命中区 */
    SBox    rows[SET_SW_N];                       /* 右栏五行（整行可点） */
    SBox    sws[SET_SW_N];                        /* 五个开关的绘制矩形 */
    SBox    groupBox;                             /* 「收起岛设置」组标题 */
    SBox    aboutBody;                            /* 关于页滚动内容框（绘制/滚轮同源） */
    SBox    aboutRepo;                            /* 关于页仓库链接：整行可点（绘制/命中同源） */
    SBox    updateBtn;                            /* 更新页「检查更新」按钮 */
    SBox    updateBody;                           /* 更新页结果区（绘制/滚轮同源） */
    double  dividerX, dividerY0, dividerY1;      /* 左右栏竖分隔线 */
    double  fineLineY;                           /* 两组之间的极淡横线 */
    double  fineX0, fineX1;
    int     tabPx, groupPx, rowPx, repoPx;       /* repoPx 也供 about.c 直接取 */
    int     updTitlePx, updVerPx, updBtnPx, updTextPx;
} SetBoxes;

typedef struct {
    int      tab;              /* SET_TAB_GENERAL / SET_TAB_UPDATE / SET_TAB_ABOUT */
    int      collapseMode;     /* SET_MODE_*，单选必选 */
    int      autostart;        /* 注册表真值 */
    int      autostartOK;      /* 最近一次注册表读写是否成功（诊断用，不落盘） */
    double   swCur[SET_SW_N];  /* 开关动画当前值 0..1（可轻微过冲） */
    double   swVel[SET_SW_N];  /* 弹簧速度 */
    double   aboutScroll;      /* 关于页滚动偏移（设备像素），不持久化 */
    double   aboutDocH;        /* 关于页文档总高，绘制时算，滚轮 clamp 用 */
} Settings;

void settings_init(Settings *s);
void settings_boxes(double W, double H, double L, SetBoxes *b);
void settings_draw(App *a, double W, double H, double L,
                   const SetBoxes *b, double k);
void settings_click(App *a, int id, int x, int y);

/* 每帧和 anim_tick 同帧调用； 返回 1 = 动画未收敛，主循环据此继续重绘。 */
int  settings_tick(Settings *s, double dt);
double settings_sw_value(const Settings *s, int idx);

/* 纯查询：ui.c 在「鼠标移开 / 点击岛外 / 切回第一卡」三个时机分别问一次 */
int  settings_tab(const Settings *s);
int  settings_collapse_on_leave(const Settings *s);
int  settings_collapse_on_outside(const Settings *s);
int  settings_collapse_on_first(const Settings *s);
int  settings_autostart(const Settings *s);

#endif /* QISLAND_SETTINGS_H */
