/* config.h — 硬编码参数 */
#ifndef QISLAND_CONFIG_H
#define QISLAND_CONFIG_H

#include "cards.h"

#define APP_NAME        L"Qisland"
#define WND_CLASS       L"QislandLayeredClass"
#define MUTEX_NAME      L"QislandSingleton"

#define IDI_QISLAND     1

#define CAPSULE_ICON_RATIO  0.56

/* 岛 */
#define ISLAND_W        240.0
#define ISLAND_H        40.0
#define ISLAND_R        20.0
#define CLOCK_PAD       24.0

#define TITLE_PX_RATIO  0.18
#define TITLE_PX_MIN    18
#define TITLE_PX_MAX    34
#define CD_ISLAND_PX    18.0              /* 胶囊右侧剩余倒计时的字号 */


#define OPPO_FACE       L"OPPO Sans 4.0"              /* TTF nameID=1/4 */
#define OPPO_FONT_FILE  L"assets\\OPPO Sans 4.0.ttf"
#define FONT_FACE       OPPO_FACE
#define FONT_FACE_CJK   OPPO_FACE
#define FONT_FACE_FALLBACK      L"Segoe UI"
#define FONT_FACE_FALLBACK_CJK  L"Microsoft YaHei"
#define FA_FACE_NAMEID1 L"Font Awesome 6 Free Solid"
#define FA_FACE_TYPO    L"Font Awesome 6 Free"
#define FA_FONT_FILE    L"assets\\fa-solid-900.ttf"
#define GLYPH_SS        4                 /* 图标/文字的超采样 */
#define SOFT_PX         1.7               /* 形状边缘过渡带） */
#define SHAPE_SS        4                 /* 形状过渡带子采样倍率 */

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

/* 启动入场：胶囊从顶部一条细线长出，参数集中在这里调 */
#define INTRO_MS         520          /* 总时长，毫秒 */
#define INTRO_LINE_W     2.0         /* 起始细线宽，逻辑像素 */
#define INTRO_LINE_H     2.0         /* 起始细线高，逻辑像素 */
#define INTRO_H_DELAY    0.10        /* 高度比宽度晚起步的比例 */
#define INTRO_BACK_C1    1.62        /* 宽度回弹强度，越大弹得越明显 */
#define INTRO_TEXT_AT    0.55        /* 岛内文字开始淡入的进度 */
#define INTRO_TEXT_LIFT  6.0         /* 文字上移归位的距离，逻辑像素 */
#define HOVER_MARGIN    2.0
#define WHEEL_DEBOUNCE  90
#define APPLY_THROTTLE  120
#define CD_DEFAULT_SEC  300
#define CD_MAX_SEC      (24 * 3600 - 1)

#define NOTE_VIEW       2
#define PERF_VIEW       3
#define CLIP_VIEW       4

#define DATE_LEFT_X     0.082
#define DATE_SEP_X      0.536
#define DATE_RIGHT_X    0.580
#define DATE_GREG_GAP   8.0         /* 公历与「星期·第N周」之间的间距(px) */

#define DATE_CLOCK_PX   0.262       /* 大时钟 ≈ 34px @ 中卡 */
#define DATE_GREG_PX    0.135       /* 公历 ≈ 17px */
#define DATE_LUNAR_PX   0.128       /* 右栏干支年 ≈ 16px */
#define DATE_LUNAR_BIG_PX 0.155     /* 农历日期 ≈ 20px */
#define DATE_TERM_PX    0.140       /* 节气 ≈ 18px */
#define DATE_FESTIVAL_PX 0.155      /* 节日基准 ≈ 20px */
#define DATE_YIJI_PX    0.128       /* 宜 / 忌 事项 ≈ 17px */
#define DATE_SMALL_PX   0.112       /* 其余辅助信息 ≈ 14px */

/* 垂直 */
#define DATE_ROW_CLOCK  1.00
#define DATE_ROW_TEXT   1.25
#define DATE_GAP_A      0.030       /* 时钟 → 日期行 */
#define DATE_GAP_B      0.024       /* 日期行 → 宜忌组 */
#define DATE_GAP_YIJI   0.012       /* 宜 → 忌 */
#define DATE_GAP_RIGHT  0.015       /* 右栏 5 行等距 */

#define DATE_YI_R        74
#define DATE_YI_G       222
#define DATE_YI_B       128
#define DATE_JI_R       248
#define DATE_JI_G       113
#define DATE_JI_B       113
#define DATE_YIJI_TEXT_R 206
#define DATE_YIJI_TEXT_G 212
#define DATE_YIJI_TEXT_B 218
#define DATE_DIGIT_STEP 0.600       /* 时钟数字等宽步进（× 字号） */
#define DATE_COLON_W    0.340       /* 冒号占位宽（× 字号） */

/* 性能监测 */
#define PERF_SAMPLE_MS  1000
#define PERF_RING_SS    2
#define PERF_ARC_SOFT   0.035             
#define PERF_WARN_PCT   85.0
#define PERF_RING_BG_R  44
#define PERF_RING_BG_G  46
#define PERF_RING_BG_B  54
#define PERF_CPU_R      86
#define PERF_CPU_G      186
#define PERF_CPU_B      244
#define PERF_MEM_R      214
#define PERF_MEM_G      128
#define PERF_MEM_B      232
#define PERF_DISK_R     104
#define PERF_DISK_G     214
#define PERF_DISK_B     158
#define PERF_WARN_R     242
#define PERF_WARN_G     138
#define PERF_WARN_B     96
#define PERF_ARC_SHADE  0.78
#define PERF_NUM_MIX    0.38

/* 便签 */
#define NOTE_MAX        2048
#define NOTE_MAX_LINES  192
#define NOTE_FILE       L"Qisland\\note.txt"   /* 相对 %APPDATA% */
#define NOTE_PAD        7.0
#define NOTE_SCROLL_W   5.0
#define NOTE_FONT_RATIO 0.252
#define NOTE_FONT_MIN   22
#define NOTE_FONT_MAX   30
#define NOTE_LINE_GAP   1.28              /* 行高 / 字号 */
#define NOTE_SCROLL_STEP 1                /* 滚轮一格滚动行数 */
#define NOTE_THUMB_R    150               /* 圆头短滑块 */
#define NOTE_THUMB_G    156
#define NOTE_THUMB_B    172
#define NOTE_SEL_R      82
#define NOTE_SEL_G      112
#define NOTE_SEL_B      168
#define NOTE_SEL_K      0.62

#define CLIP_FILE       L"Qisland\\clipboard.bin"   /* 相对 %APPDATA% */
#define CLIP_TEST_FILE  L"Qisland\\clipboard_test.bin" /* 诊断/演示隔离，不碰真实历史 */
#define CLIP_PAD        7.0               /* 内容区内边距 */
#define CLIP_SCROLL_W   8.0               /* 右缘滚动指示预留宽度 */
#define CLIP_CHECK_W    28.0
#define CLIP_FONT_RATIO 0.145
#define CLIP_FONT_MIN   18
#define CLIP_FONT_MAX   26
#define CLIP_LINE_GAP   1.48              /* 行高 / 字号 */
#define CLIP_SCROLL_STEP 1                /* 滚轮一格滚动行数 */

/* 即时通知 */
#define NOTIFY_MS       5000
#define NOTIFY_VIEW_TAG  -1

#define NOTIFY_SOUND_FILE   L"notice.wav"

#define DPI_BASE            96            /* 100% = 96 DPI */
#define DPI_SCALE_MIN       1.0
#define DPI_SCALE_MAX       3.0           /* 支持到 300% */
#define DPI_SS_THRESHOLD    1.25

#define NOTIFY_ICON_X   0.050            /* 图标区左端（占卡片宽） */
#define NOTIFY_ICON_S   0.420            /* 图标边长（占卡片高） */
#define NOTIFY_TEXT_X   0.225            /* 程序名 / 详细信息左端（占卡片宽） */
#define NOTIFY_TEXT_W   0.680            /* 文本区宽（占卡片宽），右端避开 xmark */
#define NOTIFY_GAP_MIN  24.0             /* 图标右缘到文字起点的最小间隙（逻辑像素） */
#define CD_STEP_H       3600          /* 悬浮时位：一格 ±1 小时 */
#define CD_STEP_M       60            /* 悬浮分位：一格 ±1 分钟 */
#define CD_STEP_S       5             /* 悬浮秒位：一格 ±5 秒 */
#define COLON_GAP       0.030         /* 数字块间隙（占卡片宽），冒号落在这里 */

#define TIP_PX          14.0
#define TIP_PAD_X       11.0
#define TIP_PAD_Y       7.0
#define TIP_GAP         9.0            /* 气泡与按钮之间的间隙 */
#define TIP_R           6.0
#define TIP_TRI_W       6.0            /* 尖角底边半宽 */
#define TIP_TRI_H       7.0
#define TIP_BG_R        20
#define TIP_BG_G        22
#define TIP_BG_B        26
#define TIP_EDGE_R      92
#define TIP_EDGE_G      98
#define TIP_EDGE_B      110
#define TIP_FG_R        240
#define TIP_FG_G        242
#define TIP_FG_B        248
#define HOVER_BG_R      46
#define HOVER_BG_G      50
#define HOVER_BG_B      60

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

#define HDOTS_Y         0.930             /* 横向点纵向位置（占卡片高比例） */
#define HDOTS_HIT_PAD   9.0               /* 命中区上下各扩几像素，便于点/滚 */
#define HDOTS_HIT_PAD_X 10.0              /* 命中区左右各扩几像素 */
#define HDOTS_R_ON      4.2               /* 当前点半径：与竖点完全一致 */
#define HDOTS_R_OFF     2.6               /* 非当前点半径：与竖点完全一致 */
#define HDOTS_P0        0.30
#define HDOTS_P1        0.85
#define HDOTS_PITCH_MAX 38.0

/* 计时器 / 消息 ID */
#define IDT_COUNTDOWN   2
#define HOTKEY_ID       1
#define WM_APP_SHOW     (WM_APP + 1)

#define WM_APP_TRAY       (WM_APP + 3)
#define TRAY_UID          1
#define TRAY_MENU_SHOW    100
#define TRAY_MENU_EXIT    101

#define WM_APP_BALLOON_SIGNAL   (WM_APP + 2)
#define BALLOON_COOLDOWN_MS     3000
#define BALLOON_GLOBAL_GAP_MS   1200
#define BALLOON_SEND_TIMEOUT_MS 200
#define BALLOON_TEXT_MAX        256
#define BALLOON_TITLE_MAX       100
#define BALLOON_TTS_BALLOON     0x40
#define BALLOON_CLASS           L"tooltips_class32"
#define BALLOON_APP_NAME        L"托盘消息"
#define BALLOON_NO_TEXT         L"有新消息"

#define SET_FILE          L"settings.bin"
#define SET_RUN_KEY       L"Software\\Microsoft\\Windows\\CurrentVersion\\Run"
#define SET_RUN_VAL       L"Qisland"

#define SET_LEFT_X        0.055            /* 左栏左端 */
#define SET_LEFT_W        0.200            /* 左栏宽 */
#define SET_DIV_X         0.280            /* 竖分隔线 */
#define SET_DIV_Y0        0.120
#define SET_DIV_Y1        0.880
#define SET_RIGHT_X       0.320            /* 右栏左端 */
#define SET_RIGHT_W       0.620            /* 右栏宽，右端 0.940 */
#define SET_TAB_Y0        0.170
#define SET_TAB_DY        0.130
#define SET_TAB_H         0.100
#define SET_GROUP_Y       0.150            /* 「收起岛设置」组标题 */
#define SET_GROUP_H       0.090
#define SET_ROW_Y0        0.265
#define SET_ROW_DY        0.108
#define SET_ROW_H         0.092
#define SET_LINE_Y        0.710            /* 两组之间的极淡横线 */
#define SET_AUTO_Y        0.750            /* 「开机自启动」 */
#define SET_SW_W          0.065            /* 开关宽（占卡片宽） */
#define SET_SW_H          0.048            /* 开关高（占卡片高） */
#define SET_TAB_BAR_GAP   8.0              /* 选中页签左侧小竖条的偏移（逻辑像素） */

#define SET_SW_K          260.0
#define SET_SW_DAMP       26.0

#define SET_TAB_PX_RATIO   0.052
#define SET_TAB_PX_MIN     18
#define SET_TAB_PX_MAX     30
#define SET_GROUP_PX_RATIO 0.048
#define SET_GROUP_PX_MIN   18
#define SET_GROUP_PX_MAX   28
#define SET_ROW_PX_RATIO   0.042
#define SET_ROW_PX_MIN     16
#define SET_ROW_PX_MAX     24

#define SET_ON_R          86
#define SET_ON_G          186
#define SET_ON_B          244

#define ABOUT_ICON_R        0.130           /* 程序图标边长 / 卡片高 */
#define ABOUT_ICON_Y        0.075
#define ABOUT_HEAD_GAP      0.024           /* 图标与文字之间的 / 卡片宽 */
#define ABOUT_HR_Y          0.205           /* 头部下方横线 */
#define ABOUT_BODY_Y        0.235           /* 滚动区起点 */
#define ABOUT_BODY_H        0.625           /* 滚动区高，止于 0.860H */
#define ABOUT_SCROLL_GAP    0.028           /* 右缘留给滚动条（占卡片高） */
#define ABOUT_COL_GAP       0.030           /* 历史两列布局常量，现为单列，保留兼容 */
#define ABOUT_BLOCK_GAP     0.036           /* 单列板块之间的 / 卡片高 */

#define ABOUT_NAME_PX       0.072
#define ABOUT_NAME_PX_MIN   26
#define ABOUT_NAME_PX_MAX   40
#define ABOUT_REPO_PX       0.034
#define ABOUT_REPO_PX_MIN   14
#define ABOUT_REPO_PX_MAX   20
#define ABOUT_TITLE_PX      0.036           /* 四个板块小标题 */
#define ABOUT_TITLE_PX_MIN  15
#define ABOUT_TITLE_PX_MAX  22
#define ABOUT_CHIP_PX       0.040           /* 胶囊内文字 */
#define ABOUT_CHIP_PX_MIN   17
#define ABOUT_CHIP_PX_MAX   24

#define ABOUT_CHIP_BG       0.045           /* 胶囊淡底 / 描边的覆盖率 */
#define ABOUT_CHIP_BORDER   0.075
#define ABOUT_WHEEL_STEP    0.22            /* 每格滚轮走可视高的 22% */
#define ABOUT_ICON_GAIN     2.20            /* 暗色程序图标在深底上的覆盖率放大 */
#define ABOUT_ICON_TINT_R   0.86            /* 归一化色相压到冷灰白一档 */
#define ABOUT_ICON_TINT_G   0.90
#define ABOUT_ICON_TINT_B   0.98

#define ABOUT_TEXT_R        201
#define ABOUT_TEXT_G        206
#define ABOUT_TEXT_B        214
#define ABOUT_HEAD_R        228
#define ABOUT_HEAD_G        231
#define ABOUT_HEAD_B        236
#define ABOUT_LIC_R         154
#define ABOUT_LIC_G         162
#define ABOUT_LIC_B         176
#define ABOUT_LABEL_R       118
#define ABOUT_LABEL_G       123
#define ABOUT_LABEL_B       136
#define ABOUT_REPO_R        127
#define ABOUT_REPO_G        178
#define ABOUT_REPO_B        217

#define QISLAND_REPO_URL    L"https://github.com/Minecraftgoose/Qisland/"
#define QI_REPO_OWNER       "Minecraftgoose"
#define QI_REPO_NAME        "Qisland"
#define QI_GHPROXY_PREFIX   "https://v4.gh-proxy.org/"

#define UPD_TITLE_Y         0.170       /* 「当前版本」小标题 */
#define UPD_VER_Y           0.250       /* 版本号 */
#define UPD_BTN_Y           0.355       /* 按钮 */
#define UPD_BTN_H           0.110
#define UPD_BTN_W_RATIO     0.520       /* 占右栏宽 */
#define UPD_BODY_Y          0.510       /* 结果区起点 */
#define UPD_BODY_H          0.340       /* 止于 0.850H，避开 0.93H 横点 */
#define UPD_SCROLL_GAP      0.028       /* 右缘滚动条留白（占卡片高） */
#define UPD_LINE_GAP        1.55        /* 行高 / 字号 */

#define UPD_TITLE_PX_RATIO  0.044
#define UPD_TITLE_PX_MIN    17
#define UPD_TITLE_PX_MAX    26
#define UPD_VER_PX_RATIO    0.076
#define UPD_VER_PX_MIN      28
#define UPD_VER_PX_MAX      44
#define UPD_BTN_PX_RATIO    0.042
#define UPD_BTN_PX_MIN      16
#define UPD_BTN_PX_MAX      24
#define UPD_TEXT_PX_RATIO   0.030
#define UPD_TEXT_PX_MIN     13
#define UPD_TEXT_PX_MAX     18

#define UPD_BTN_R           86
#define UPD_BTN_G           186
#define UPD_BTN_B           244

#endif /* QISLAND_CONFIG_H */
