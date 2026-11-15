/* config.h — 全部硬编码参数集中在这里。改完重编即可。 */
#ifndef QISLAND_CONFIG_H
#define QISLAND_CONFIG_H

#include "cards.h"      /* CARDS_VDOTS：菜单与竖点数量由卡片表推导 */

#define APP_NAME        L"Qisland"
#define WND_CLASS       L"QislandLayeredClass"
#define MUTEX_NAME      L"QislandSingleton"

/* 岛 */
#define ISLAND_W        240.0
#define ISLAND_H        40.0
#define ISLAND_R        20.0
#define CLOCK_PAD       24.0

/* 字体与抗锯齿 */
#define FONT_FACE       L"Segoe UI"
#define FONT_FACE_CJK   L"Microsoft YaHei"   /* 标签「倒计时」用，Win7 自带雅黑 */
#define FA_FACE_NAMEID1 L"Font Awesome 6 Free Solid"  /* TTF nameID=1/4 */
#define FA_FACE_TYPO    L"Font Awesome 6 Free"        /* 官方 CSS / nameID=16 */
#define FA_FONT_FILE    L"assets\\fa-solid-900.ttf"
#define GLYPH_SS        4                 /* 图标/文字的 4x4 面积覆盖率超采样 */
#define SOFT_PX         1.7               /* 形状边缘过渡带（输出像素） */
#define SHAPE_SS        4                 /* 形状过渡带子采样倍率 */

/* 卡片数量不再是固定宏：由 src/cards.c 的 cards[] 表推导（cards_count()）。
   竖向指示点固定 4 个（CARDS_VDOTS，见 cards.h）。
   尺寸档像素仍在这里；哪张卡用哪档由 cards[] 的 tier 决定。 */
#define DEFAULT_VIEW    0
#define MID_W           480.0
#define MID_H           130.0
#define MID_R           20.0
#define BIG_W           480.0
#define BIG_H           260.0
#define BIG_R           20.0
#define HUGE_R          28.0
#define HUGE_GAP        24.0
#define PAD             12.0
#define OVERSHOOT       1.10
#define TOP_GAP         10.0

/* 动画 */
#define EXP_K           110.0
#define EXP_DAMP_OUT    13.0
#define EXP_DAMP_IN     19.0
#define SIZE_TAU        0.11
#define HOVER_MARGIN    2.0
#define WHEEL_DEBOUNCE  90
#define APPLY_THROTTLE  120
#define CD_DEFAULT_SEC  300
#define CD_MAX_SEC      (24 * 3600 - 1)

/* 第三个中卡片：性能监测（view 2）
   本页是唯一放开文字政策的一页：允许中文标签与硬件信息文字。
   其余页面仍然只有「倒计时」三个字 + 读数 + 图标。 */
#define PERF_VIEW       2
#define PERF_SAMPLE_MS  1000              /* 采样间隔；GetSystemTimes 差分必须间隔足够 */
#define PERF_RING_SS    2                 /* 圆环每像素 2x2 子采样求面积覆盖率 */
#define PERF_ARC_SOFT   0.035             /* 弧端点角度软化（弧度），避免扇形末端硬边 */
#define PERF_WARN_PCT   85.0              /* 高负载警示阈值：超过后向暖色插值 */
#define PERF_RING_BG_R  44                /* 环底保持中性 TRACK，彩环才有对比 */
#define PERF_RING_BG_G  46
#define PERF_RING_BG_B  54
/* 三个环各自可辨识的基色（黑底上够亮、不过饱和）。
   色相刻意拉开：冷蓝 CPU / 紫 内存 / 青绿 磁盘。 */
#define PERF_CPU_R      86
#define PERF_CPU_G      186
#define PERF_CPU_B      244
#define PERF_MEM_R      214
#define PERF_MEM_G      128
#define PERF_MEM_B      232
#define PERF_DISK_R     104
#define PERF_DISK_G     214
#define PERF_DISK_B     158
#define PERF_WARN_R     242               /* 高负载：暖橙，功能性警示 */
#define PERF_WARN_G     138
#define PERF_WARN_B     96
#define PERF_ARC_SHADE  0.78              /* 弧起点亮度，沿弧渐增到 1.0 */
#define PERF_NUM_MIX    0.38              /* 环心数字：基色向白提亮，保证可读 */

/* 第二个中卡片：便签（view 1） */
#define NOTE_VIEW       1                 /* 001 = 便签，其余中卡片仍是计时器 */
#define NOTE_MAX        2048              /* UTF-16 字符上限 */
#define NOTE_MAX_LINES  192
#define NOTE_FILE       L"Qisland\\note.txt"   /* 相对 %APPDATA% */
#define NOTE_PAD        7.0               /* 文本区内边距（设备像素） */
#define NOTE_SCROLL_W   5.0               /* 美化滚动条预留宽度 */
/* 正文字号：按文本区高度取比例，下限 22、上限 30。
   中卡片约 480×130 时取到 26px（原来 12px 的两倍多）。 */
#define NOTE_FONT_RATIO 0.215
#define NOTE_FONT_MIN   22
#define NOTE_FONT_MAX   30
#define NOTE_LINE_GAP   1.28              /* 行高 / 字号：字大了要收紧，保证 3 行可见 */
#define NOTE_SCROLL_STEP 1                /* 滚轮一格滚动行数 */
#define NOTE_THUMB_R    150               /* 极淡的圆头短滑块（不画轨道） */
#define NOTE_THUMB_G    156
#define NOTE_THUMB_B    172
/* 选中指示：暗蓝灰，压在黑底上不刺眼；白字仍保持原色 */
#define NOTE_SEL_R      82
#define NOTE_SEL_G      112
#define NOTE_SEL_B      168
#define NOTE_SEL_K      0.62              /* 高亮合成系数 */

/* 第一个大卡片：粘贴板历史（DRAW_CLIP，view 由 cards[] 决定）
   内容本身就是文字（用户复制的东西），但界面不新增任何标题/标签/提示文字；
   条目单行截断，滚动条与便签同款：无系统滚动条、无轨道、只有极淡圆头短滑块。 */
#define CLIP_VIEW       3                 /* 测试用锚点；实际判断用 cards_draw() */
#define CLIP_FILE       L"Qisland\\clipboard.bin"   /* 相对 %APPDATA% */
#define CLIP_TEST_FILE  L"Qisland\\clipboard_test.bin" /* 诊断/演示隔离，不碰真实历史 */
#define CLIP_PAD        7.0               /* 内容区内边距（设备像素） */
#define CLIP_SCROLL_W   8.0               /* 右缘滚动指示预留宽度 */
#define CLIP_CHECK_W    28.0              /* 行右端「已回填」图标预留宽度 */
#define CLIP_FONT_RATIO 0.125             /* 字号 = 内容区高 × 比例，208px→26，与便签一致 */
#define CLIP_FONT_MIN   18
#define CLIP_FONT_MAX   26
#define CLIP_LINE_GAP   1.48              /* 行高 / 字号 */
#define CLIP_SCROLL_STEP 1                /* 滚轮一格滚动行数 */

/* 即时通知：与中卡片同尺寸；左侧软件图标/FA，右侧程序名 + 详细信息。
   不储存、不写盘、不排队（内存里只有当前一条）。 */
#define NOTIFY_MS       5000              /* 默认显示毫秒数；到点自动消失 */
#define NOTIFY_VIEW_TAG  -1              /* 通知不是导航卡片，不占竖点/横点 */

/* 通知卡片布局：图标区 / 文字区都用卡片宽高的比例，绘制与测试共用同一组宏。
   TEXT_X 必须明显大于 ICON_X + ICON_S*H/W，否则文字会贴到图标上。
   中卡片 480x130 下：图标右缘 = 24 + 54.6 = 78.6，文字起点 = 108 → 间隙 29.4px。 */
#define NOTIFY_ICON_X   0.050            /* 图标区左端（占卡片宽） */
#define NOTIFY_ICON_S   0.420            /* 图标边长（占卡片高） */
#define NOTIFY_TEXT_X   0.225            /* 程序名 / 详细信息左端（占卡片宽） */
#define NOTIFY_TEXT_W   0.680            /* 文本区宽（占卡片宽），右端避开 xmark */
#define NOTIFY_GAP_MIN  24.0             /* 图标右缘到文字起点的最小间隙（逻辑像素） */
#define CD_STEP_H       3600          /* 悬浮时位：一格 ±1 小时 */
#define CD_STEP_M       60            /* 悬浮分位：一格 ±1 分钟 */
#define CD_STEP_S       5             /* 悬浮秒位：一格 ±5 秒   */
#define COLON_GAP       0.030         /* 数字块间隙（占卡片宽），冒号落在这里 */
#define HOVER_BG_R      46            /* 悬浮数字段的淡底，纯视觉，无文字 */
#define HOVER_BG_G      50
#define HOVER_BG_B      60

/* 配色：完全中性的单色。草图里的紫框/绿框/红框只是标注示意，
   不进入实际 UI；状态差异只用亮度与实心/空心区分。 */
#define CB_R 12
#define CB_G 12
#define CB_B 14
#define INK_R 236
#define INK_G 238
#define INK_B 244
#define DIM_R 108
#define DIM_G 112
#define DIM_B 126
#define TRACK_R 44
#define TRACK_G 46
#define TRACK_B 54
#define ACCENT_R 198
#define ACCENT_G 206
#define ACCENT_B 222

/* 横向指示圆点：进入第 4 个竖点（横向组）后，竖列圆点整列消失，
   改在卡片底部画一排圆点 —— 和竖着的那列完全同款（半径 / 配色一致），
   但**居中对称、密集排列**：间距直接取竖点的间距公式
   H*0.80*(HDOTS_P1-HDOTS_P0)/(CARDS_VDOTS-1)，再钳到 HDOTS_PITCH_MAX，
   免得超大卡片上被拉得过疏。没有轨道、长条、文字、箭头，只有点。 */
#define HDOTS_Y         0.930             /* 横向点纵向位置（占卡片高比例） */
#define HDOTS_HIT_PAD   9.0               /* 命中区上下各扩几像素，便于点/滚 */
#define HDOTS_HIT_PAD_X 10.0              /* 命中区左右各扩几像素 */
#define HDOTS_R_ON      4.2               /* 当前点半径：与竖点完全一致 */
#define HDOTS_R_OFF     2.6               /* 非当前点半径：与竖点完全一致 */
/* 竖点纵向分布仍用这两个；横点间距由它推出来的 pitch 决定 */
#define HDOTS_P0        0.30
#define HDOTS_P1        0.85
#define HDOTS_PITCH_MAX 38.0              /* 设备像素（逻辑），乘 u；密集上限 */

/* 计时器 / 消息 ID */
#define IDT_COUNTDOWN   2
#define HOTKEY_ID       1
#define WM_APP_SHOW     (WM_APP + 1)

/* ---- QQ / 微信即时消息监听（仅 Win7 及以上）----
   微信：RegisterShellHookWindow + HSHELL_FLASH，走主线程消息循环；
   QQ  ：独立后台线程轮询托盘图标 / 窗口状态，PostMessage 回主线程。
   两条链路都不安装全局钩子（WH_MOUSE_LL 那次卡死的教训）。 */
#define WM_APP_IM_SIGNAL   (WM_APP + 2)  /* 后台线程 -> 主线程 */
#define IM_SIG_WECHAT      1
#define IM_SIG_QQ          2
#define IM_SIG_QQFLASH     3
#define IM_POLL_MS         500          /* QQ 后台轮询周期 */
#define IM_COOLDOWN_MS     3000         /* 同一来源去抖，避免闪烁连发刷屏 */
#define IM_GLOBAL_GAP_MS   1200         /* 不同来源之间的最小间隔 */
#define IM_SEND_TIMEOUT_MS 200          /* 跨进程 SendMessage 超时，防线程挂死 */
#define IM_TRAY_BUTTON_MAX 64           /* 托盘 toolbar 最多读多少个按钮 */
#define IM_TITLE_MAX       8            /* 参与指纹比较的 QQ 窗口数上限 */
#define MENU_VIEW0      20
/* 右键菜单只列 4 个竖点，点击跳到该点的锚点卡片 */
#define MENU_VIEW_LAST  (MENU_VIEW0 + CARDS_VDOTS - 1)

#endif /* QISLAND_CONFIG_H */
