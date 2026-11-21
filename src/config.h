/* config.h — 全部硬编码参数集中在这里。改完重编即可。 */
#ifndef QISLAND_CONFIG_H
#define QISLAND_CONFIG_H

#include "cards.h"      /* CARDS_VDOTS：菜单与竖点数量由卡片表推导 */

#define APP_NAME        L"Qisland"
#define WND_CLASS       L"QislandLayeredClass"
#define MUTEX_NAME      L"QislandSingleton"

/* 程序图标资源 ID：必须与 src/qisland.rc 的 `1 ICON` 一致。
   素材是用户图标包里「中间黑」那张，windres 编进 exe 后由
   window.c 的 WNDCLASSEXW.hIcon / hIconSm 加载。 */
#define IDI_QISLAND     1

/* 胶囊态右侧白色圆图标（无倒计时时显示）直径占岛高比例。
   素材是「中间白、边缘黑」那张，离线编成 64x64 灰度+alpha 数组。 */
#define CAPSULE_ICON_RATIO  0.56

/* 岛 */
#define ISLAND_W        240.0
#define ISLAND_H        40.0
#define ISLAND_R        20.0
#define CLOCK_PAD       24.0

/* 「倒计时」标题：原 0.10*H 太小，放大到 0.18*H（中卡约 23px）。
   「滚轮调整时间」与标题同字号，放在标题与读数之间的空行。 */
#define TITLE_PX_RATIO  0.18
#define TITLE_PX_MIN    18
#define TITLE_PX_MAX    34
#define CD_ISLAND_PX    18.0              /* 胶囊态右侧剩余倒计时的字号 */

/* 字体与抗锯齿 */
/* ---- 全局字体：OPPO Sans 4.0 ----
   用户本轮提供 OPPO_Sans_4.0.zip，要求「全局字体」。
   拉丁 / 数字 / 中文一律走同一个家族，不再 Segoe UI + 雅黑混排。
   字体以 AddFontResourceExW(FR_PRIVATE) 私有加载，不装系统；
   加载失败时由 font.c 回退到下面两个系统字体，程序不崩、不显示方框。
   许可要求「不得修改字体」，故 assets 里放的是原始 TTF（22MB，未子集化）；
   字体自身 OS/2.fsType=8 (No subsetting) 也禁止子集化。它带 fvar 变量字重轴
   (wght 100..700, 默认 400)，Win7 GDI 不认变量轴，FW_SEMIBOLD 会走默认 400
   + GDI 合成加粗；若想完全忠实字形，改 font.c 非 FA 分支字重为 FW_NORMAL。 */
#define OPPO_FACE       L"OPPO Sans 4.0"              /* TTF nameID=1/4 */
#define OPPO_FONT_FILE  L"assets\\OPPO Sans 4.0.ttf"
#define FONT_FACE       OPPO_FACE
#define FONT_FACE_CJK   OPPO_FACE
#define FONT_FACE_FALLBACK      L"Segoe UI"
#define FONT_FACE_FALLBACK_CJK  L"Microsoft YaHei"
#define FA_FACE_NAMEID1 L"Font Awesome 6 Free Solid"  /* TTF nameID=1/4 */
#define FA_FACE_TYPO    L"Font Awesome 6 Free"        /* 官方 CSS / nameID=16 */
#define FA_FONT_FILE    L"assets\\fa-solid-900.ttf"
#define GLYPH_SS        4                 /* 图标/文字的 4x4 面积覆盖率超采样 */
#define SOFT_PX         1.7               /* 形状边缘过渡带（输出像素） */
#define SHAPE_SS        4                 /* 形状过渡带子采样倍率 */

/* 卡片数量不再是固定宏：由 src/cards.c 的 cards[] 表推导（cards_count()）。
   竖向指示点由 CARDS_VDOTS 决定（当前 5 个），**加卡就加点，没有锁死 4 个**。
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

/* 卡片下标锚点：**只供历史诊断代码使用**，运行时判断一律走 cards_draw()。
   在 cards[] 中间插卡后必须同步这三个值，否则 tests.c 的锚点会错位。
   当前表：0 计时器 / 1 日期时钟 / 2 便签 / 3 性能 / 4 大卡片 / 5 超大卡片 */
#define NOTE_VIEW       2
#define PERF_VIEW       3
#define CLIP_VIEW       4

/* 第二张中卡片：带秒时钟 + 一整组历法日期（DRAW_DATE，idx 1）
   本页字符串全部是时间与历法信息，按用户本轮明确点名登记在 AGENTS.md
   第 4 节封闭清单第 9 条 —— 不是放开文字政策，别的页面一字不加。
   所有常量都是卡片 W/H 的比例；W/H 用目标态标称尺寸（不随动画抖）。 */
/* 用户给的 HTML 版式：容器 padding 16/24，grid 1.2fr | 1px | 1fr，
   gap 20px，两栏各自 flex-column + justify-content:center 垂直居中成组。
   映射到 480 宽中卡：左右各留 24px(0.050W)，左栏 ≈213px、右栏 ≈178px，
   分隔线在 0.536W，右栏起点 0.580W。不再把行钉死在 H 的固定比例上。 */
#define DATE_LEFT_X     0.082       /* 右移避开竖点：圆心 0.055W、半径 4.2px，
                                       0.082W 处离点右缘仍有 ~9px 余量 */
#define DATE_SEP_X      0.536
#define DATE_RIGHT_X    0.580
#define DATE_GREG_GAP   8.0         /* 公历与「星期·第N周」之间的间距(px) */

#define DATE_CLOCK_PX   0.262       /* 大时钟 ≈ 34px @ 中卡 */
#define DATE_GREG_PX    0.135       /* 公历 ≈ 17px */
#define DATE_LUNAR_PX   0.128       /* 右栏干支年 ≈ 16px */
#define DATE_LUNAR_BIG_PX 0.155     /* 农历日期 ≈ 20px（用户要求放大，未退回 18px） */
#define DATE_TERM_PX    0.140       /* 节气 ≈ 18px（用户要求放大） */
#define DATE_FESTIVAL_PX 0.155      /* 节日基准 ≈ 20px，行内放不下自动缩 */
#define DATE_YIJI_PX    0.128       /* 宜 / 忌 事项 ≈ 17px */
#define DATE_SMALL_PX   0.112       /* 其余辅助信息 ≈ 14px */

/* 垂直节奏：行高 = 字号 × 系数，组间距按 H 取比例；整组居中。 */
#define DATE_ROW_CLOCK  1.00
#define DATE_ROW_TEXT   1.25
#define DATE_GAP_A      0.030       /* 时钟 → 日期行 */
#define DATE_GAP_B      0.024       /* 日期行 → 宜忌组 */
#define DATE_GAP_YIJI   0.012       /* 宜 → 忌 */
#define DATE_GAP_RIGHT  0.015       /* 右栏 5 行等距 */

/* HTML：只有 .label 带色（宜 #4ade80 / 忌 #f87171），
   .almanac-text 是中性 #ced4da。上一轮整行带色显得乱，改回标签着色。 */
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

/* 性能监测（idx 3）：本页标签与硬件信息属封闭清单第 3 条。
   计时器页另有用户逐字点名的「滚轮调整时间」一行（与标题同字号）。 */
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

/* 第三张中卡片：便签（idx 2） */
#define NOTE_MAX        2048              /* UTF-16 字符上限 */
#define NOTE_MAX_LINES  192
#define NOTE_FILE       L"Qisland\\note.txt"   /* 相对 %APPDATA% */
#define NOTE_PAD        7.0               /* 文本区内边距（设备像素） */
#define NOTE_SCROLL_W   5.0               /* 美化滚动条预留宽度 */
/* 正文字号：按文本区高度取比例，下限 22、上限 30。
   中卡片约 480×130 时取到 26px（原来 12px 的两倍多）。 */
/* 内容区因顶部标题从 0.88H 收到 0.75H，比例同步补偿，字号不变 */
#define NOTE_FONT_RATIO 0.252
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

/* 第一个大卡片：剪贴板历史（DRAW_CLIP，idx 由 cards[] 决定）
   内容本身就是文字（用户复制的东西），但界面不新增任何标题/标签/提示文字；
   条目单行截断，滚动条与便签同款：无系统滚动条、无轨道、只有极淡圆头短滑块。 */

#define CLIP_FILE       L"Qisland\\clipboard.bin"   /* 相对 %APPDATA% */
#define CLIP_TEST_FILE  L"Qisland\\clipboard_test.bin" /* 诊断/演示隔离，不碰真实历史 */
#define CLIP_PAD        7.0               /* 内容区内边距（设备像素） */
#define CLIP_SCROLL_W   8.0               /* 右缘滚动指示预留宽度 */
#define CLIP_CHECK_W    28.0              /* 行右端「已回填」图标预留宽度 */
#define CLIP_FONT_RATIO 0.145             /* 内容区 0.80H→0.69H 后补偿，字号仍 26 */
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

/* 悬停提示气泡：时序按官方 TTM_SETDELAYTIME 口径由 GetDoubleClickTime() 推导
   （默认 initial 500ms / reshow 100ms / autopop 5000ms），这里只放外观常量。 */
#define TIP_PX          14.0
#define TIP_PAD_X       11.0
#define TIP_PAD_Y       7.0
#define TIP_GAP         9.0            /* 气泡与按钮之间的间隙（尖角占这段） */
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

/* 横向指示圆点：进入最后一个竖点（横向组）后，竖列圆点整列消失，
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

/* 软件版本：唯一来源在 version.h（C 代码 + windres 资源 + NSIS 脚本共用 0.2）。
   「关于」页按用户要求先留空，不在界面上写版本号。 */

/* 系统托盘图标（窗口本身的右键菜单已删除，菜单只在托盘图标上）。
   uCallbackMessage 用 WM_APP+3，避开 WM_APP_SHOW(+1) 与气泡信号(+2)。 */
#define WM_APP_TRAY       (WM_APP + 3)
#define TRAY_UID          1
#define TRAY_MENU_SHOW    100
#define TRAY_MENU_EXIT    101

/* ---- Win7 托盘气泡消息监听（替代原即时消息专属监听）----
   官方：NIF_INFO 的 NIN_BALLOON* 只发给图标拥有者，不广播；用
   SetWinEventHook(CREATE/SHOW, WINEVENT_OUTOFCONTEXT) 旁观 explorer 的
   tooltips_class32 + TTS_BALLOON 窗口。
   正文 szInfo 用 WM_GETTEXT 读；标题 szInfoTitle + 标题图标用
   TTM_GETTITLE(WM_USER+35) 读（该消息 >= WM_USER，跨进程不封送指针，
   必须在 explorer 进程里 VirtualAllocEx 分配 TTGETTITLE + 缓冲）。 */
#define WM_APP_BALLOON_SIGNAL   (WM_APP + 2)   /* WinEvent 回调 -> 主线程 */
#define BALLOON_COOLDOWN_MS     3000           /* 同一气泡窗口最小间隔 */
#define BALLOON_GLOBAL_GAP_MS   1200           /* 全局最小间隔（气泡排队连发） */
#define BALLOON_SEND_TIMEOUT_MS 200            /* SendMessageTimeout 防 explorer 卡住 */
#define BALLOON_TEXT_MAX        256            /* 气泡文本上限（NOTIFYICONDATA szInfo=256） */
#define BALLOON_TITLE_MAX       100            /* 官方：szInfoTitle <= 100 TCHAR 含 NULL */
#define BALLOON_TTS_BALLOON     0x40           /* 官方 Tooltip Styles，版本 5.80 */
#define BALLOON_CLASS           L"tooltips_class32"
#define BALLOON_APP_NAME        L"托盘消息"    /* 仅当标题读不到时的兜底，非来源 exe 名 */
#define BALLOON_NO_TEXT         L"有新消息"    /* 读不到文本时只给信号，不编造正文 */
/* 灵动岛本身的右键菜单已删除：不再有 MENU_VIEW0 / MENU_VIEW_LAST，
   跳卡只靠滚轮、竖/横圆点与右键菜单以外的入口。cards_vdot/cards_anchor
   仍被 ui.c 的竖点绘制和 tests.c 的历史诊断使用，函数本身保留。 */

/* ---- 设置页（唯一的超大卡片，DRAW_SETTINGS）----
   左右栏布局；所有比例相对**目标态标称尺寸** W0/H0，绘制与命中同源。
   文字是 AGENTS.md 封闭清单第 13 条的逐字点名字符串，不扩项。 */
#define SET_FILE          L"settings.bin"   /* %APPDATA%\Qisland\，只存 tab + 收起掩码 */
#define SET_RUN_KEY       L"Software\\Microsoft\\Windows\\CurrentVersion\\Run"
#define SET_RUN_VAL       L"Qisland"        /* HKCU，开机自启动唯一真值 */

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
#define SET_ROW_Y0        0.265            /* 四个收起条件（单选必选） */
#define SET_ROW_DY        0.108
#define SET_ROW_H         0.092
#define SET_LINE_Y        0.710            /* 两组之间的极淡横线 */
#define SET_AUTO_Y        0.750            /* 「开机自启动」 */
#define SET_SW_W          0.065            /* 开关宽（占卡片宽） */
#define SET_SW_H          0.048            /* 开关高（占卡片高） */
#define SET_TAB_BAR_GAP   8.0              /* 选中页签左侧小竖条的偏移（逻辑像素） */

/* 开关弹簧：ω=√260≈16.1、ζ=26/(2ω)≈0.81，轻微回弹，约 0.4s 收敛。
   收敛后 settings_tick() 返回 0，主循环恢复静止零重绘。 */
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

/* 开关打开时的冷蓝；关闭用 TRACK_* 中性灰。只是控件状态色，不是文字色。 */
#define SET_ON_R          86
#define SET_ON_G          186
#define SET_ON_B          244

/* ---- 关于页（设置页右栏，SET_TAB_ABOUT）----
   0.2 轮按用户确认的 HTML 效果稿落地：图标 + 名称 + 仓库 + 四个胶囊板块。
   内容比可视区高 → 可滚轮滚动，右缘一条淡圆条。所有比例相对 H0（设备像素）。 */
#define ABOUT_ICON_R        0.130           /* 程序图标边长 / 卡片高 */
#define ABOUT_ICON_Y        0.075
#define ABOUT_HEAD_GAP      0.024           /* 图标与文字之间的 / 卡片宽 */
#define ABOUT_HR_Y          0.205           /* 头部下方横线 */
#define ABOUT_BODY_Y        0.235           /* 滚动区起点 */
#define ABOUT_BODY_H        0.625           /* 滚动区高，止于 0.860H（避开 0.93H 横点） */
#define ABOUT_SCROLL_GAP    0.028           /* 右缘留给滚动条（占卡片高） */
#define ABOUT_COL_GAP       0.030           /* 两列之间的 / 卡片高 */
#define ABOUT_BLOCK_GAP     0.026           /* 板块之间的 / 卡片高 */

#define ABOUT_NAME_PX       0.062
#define ABOUT_NAME_PX_MIN   22
#define ABOUT_NAME_PX_MAX   34
#define ABOUT_REPO_PX       0.028
#define ABOUT_REPO_PX_MIN   11
#define ABOUT_REPO_PX_MAX   16
#define ABOUT_TITLE_PX      0.024           /* 四个板块小标题 */
#define ABOUT_TITLE_PX_MIN  10
#define ABOUT_TITLE_PX_MAX  14
#define ABOUT_CHIP_PX       0.027           /* 胶囊内文字 */
#define ABOUT_CHIP_PX_MIN   11
#define ABOUT_CHIP_PX_MAX   16

#define ABOUT_CHIP_BG       0.045           /* 胶囊淡底 / 描边的覆盖率 */
#define ABOUT_CHIP_BORDER   0.075
#define ABOUT_WHEEL_STEP    0.18            /* 每格滚轮走可视高的 18% */
#define ABOUT_ICON_GAIN     2.20            /* 暗色程序图标在深底上的覆盖率放大 */
#define ABOUT_ICON_TINT_R   0.86            /* 归一化色相压到冷灰白一档 */
#define ABOUT_ICON_TINT_G   0.90
#define ABOUT_ICON_TINT_B   0.98

/* 文字配色：胶囊常规 / 主体亮 / 许可淡 / 板块小标题 / 仓库蓝 */
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

#endif /* QISLAND_CONFIG_H */
