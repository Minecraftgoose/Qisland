# Qisland · 仅图标版（模块化）

Windows 桌面灵动岛。纯 Win32 + GDI 手写，**不依赖任何第三方运行库**
（唯一的第三方文件是 public domain 的 `stb_textedit.h` 单头库，用于便签选区）。
按钮全部是 Font Awesome 图标；计时器 / 便签页界面上唯一的文字是「倒计时」三个字；
**第三张中卡片（性能监测）按需求放开了文字政策**，允许中文标签与硬件信息；
三个圆环也有各自配色（这是界面上唯一的彩色区域）。
**第一个大卡片是粘贴板历史**：界面上出现的文字全部是「用户复制过的内容本身」
（数据，不是 UI 标签），没有标题、说明或 placeholder。
**横排圆点改为居中对称、密集排列**（间距取竖点 pitch），**新增即时通知系统**
（中卡片尺寸、左侧软件图标/FA、程序名 + 详细信息、不储存不写盘），
第一个接入方是倒计时归零提醒。
本轮收口三处：**旧 bell 常驻提示整段删除，归零只弹新版通知**；
通知文字与图标拉开到 29.4px；**双滑块模式隐藏 play/reset**（不画也不响应）。
本轮新增 **QQ / 微信即时消息监听**：微信走 Shell Hook（主线程）、QQ 走独立后台
轮询线程，两条链路都收敛到同一个 `notify_show_*()`，不新增任何 UI。
紫/绿/红框和标注文字都只是草图示意，没有进入实际 UI。

交付物是**整包 zip**，不再是单 exe —— 因为图标字体是资源文件，必须和 exe 放在一起。
整包按「软件 / 源码」两分：

```
zip
├── 软件/
│   └── Qisland/
│       ├── Qisland.exe          ← 双击运行
│       ├── assets/
│       │   ├── fa-solid-900.ttf     ← Font Awesome 6.7.2 Free Solid（必须存在）
│       │   ├── LICENSE.txt          ← FA Free 许可全文
│       │   └── ATTRIBUTION.txt      ← 来源与署名
│       ├── README.md            ← 运行说明（本目录自包含）
│       └── preview.png          ← QQ/微信通知四态 + 横点放大 + 官方依据 + 实测
└── 源码/
    ├── AGENTS.md                ← 项目协作规范（给 AI / 开发者，改代码前先读）
    ├── gitignore.txt            ← .gitignore 模板（重命名即可用）
    ├── README.md                ← 本文件：完整技术文档
    └── src/                     ← 模块化源码 + 编译脚本
        ├── common.h  config.h  app.h  icons.h
        ├── stb_textedit.h  textedit_glue.h  ← 文本编辑内核（public domain）
        ├── main.c  window.c  canvas.c  font.c
        ├── sys.c  anim.c  countdown.c  note.c  perf.c  clip.c  notify.c  im.c  cards.c
        ├── input.c  ui.c  tests.c
        └── build.bat            ← MinGW-w64 编译脚本（在 src 目录内运行）
```

**`assets/fa-solid-900.ttf` 必须留在 exe 旁边的 assets 目录**，程序通过
`GetModuleFileNameW` 定位它并用 `AddFontResourceExW(FR_PRIVATE)` 私有加载，
不需要（也不会）往系统里装字体。文件缺失时程序仍可启动，图标降级为自绘几何形状。

## 界面文字政策

| 位置 | 内容 |
|---|---|
| 卡片标题 | 「倒计时」三个字（唯一文字，走 Microsoft YaHei） |
| 三块读数 | `HH:MM:SS` 数字 + 两处冒号，是读数不是标签 |
| 冒号 | 每处上下两个 SDF 圆点，是符号不是文字；与岛态时钟的 `HH:MM` 一致 |
| 岛态 | `HH:MM` 时钟数字 |
| 便签正文 | 用户自己输入的文字，**不是界面标签**；空态只有一个极淡 `pencil` 图标，无 placeholder |
| 所有按钮 | Font Awesome 图标，**零文字** |
| 右键菜单 | `● ○ − ×` 几何符号，**零文字** |
| **性能监测页（view 2）** | **唯一放开文字政策的一页**：标题「性能监测」、标签「CPU / 内存 / 磁盘」、5 行硬件信息；三个圆环也是唯一彩色区域 |
| **粘贴板历史页（view 3）** | 条目文字 = **用户复制过的内容本身**（数据，不是 UI 标签）；无标题、无说明、无 placeholder；空态只有一个极淡 `clipboard` 图标 |
| **即时通知卡片** | 程序名 + 详细信息 = **通知内容本身**；左侧 FA 或软件 HICON，右上角 `xmark` 关闭；不占竖点/横点、不写盘 |
| 配色 | 计时器 / 便签页仍是完全中性的黑白灰；**性能页的圆环是彩色**（本页唯一例外）。状态差异用亮度 / 实心空心 + 环色区分 |

紫框、绿框、红框、`taskmgr`、`调节亮度`、`滚轮切换`、`隐藏灵动岛` 等草图
标注文字一律没有进入界面；界面与源码保持纯本地功能，不读取任何外部会话数据。

**文字政策的边界**：新增 UI 标签只作用于 view 2。计时器页（view 0 / 4）和便签页（view 1）
仍然维持「只有倒计时三个字」；view 3 显示的是剪贴板内容本身、通知卡片显示的是通知内容，
都属于数据而非新增标签。切换到其他档位时，性能页的标题与硬件信息、历史页的条目、
通知卡片的文字一起消失，不会渗到别的页面去。

## 卡片布局（按 3.html）

```
┌──────────────────────────────────────────────────────────┐
│ [滚轮竖条]  eye-slash   sun        「倒计时」             │
│             volume-high list-check 00:05:00  play reset │
└──────────────────────────────────────────────────────────┘
```

左侧 2x2 四格与 `3.html` 一一对应：

| 位置 | 图标 | 功能 |
|---|---|---|
| 左上 | `eye-slash` | 隐藏岛 |
| 右上 | `sun` / `moon` | 亮度 |
| 左下 | `volume-high` / `volume-off` / `volume-xmark` | 音量 |
| **右下** | `list-check` | **taskmgr（从标题区搬到红箭头指的位置）** |

原先占着右下那格的 `right-left` 切换按钮**不再单独占位**——它承担的
「计时器 ↔ 双滑块」功能改由**亮度或音量按钮单击触发**。四个按钮下方
**依然不画任何细滑轨**；滑块只出现在计时器区域，是功能主体而非装饰条。

## 计时器 ↔ 双滑块

| 状态 | 标题区 | 计时器区 |
|---|---|---|
| mode 0 | 「倒计时」三个字 | `HH:MM:SS` 三块读数 + 两处冒号 |
| mode 1 | `sliders` 图标（**无文字**） | 上：音量滑块 / 下：亮度滑块，各带左侧图标 + 圆角槽 + 填充 + 圆把手 |

- **单击** `sun` 或 `volume-high` 图标 → 进入 mode 1；再点一次（两个中任一个）→ 回到 mode 0。
- **按住左右拖动**这两个图标 → 直接调对应值，**不切换模式**（位移超过 4px 才算拖动）。
- mode 1 下在任一滑块上**点击或按住拖动** → 调对应值，按轨道真实范围映射
  （左侧图标占位不计入，所以点哪就是哪，不会偏移）。
- 切换模式时立即刷新命中表，不留一帧空窗。
- 为区分「单击」与「拖动」，输入改成按下记录、松开判定；纯点击用按下时的坐标触发，
  拖走则取消点击。这是标准控件行为，也避免了「想拖一下结果把模式切了」。

## 倒计时：冒号 / 悬浮调时 / 归零通知

**冒号**：`00:05:00` 的两处冒号各是上下两个圆点，用 SDF 绘制（同样无锯齿），
不是文字字符，所以界面中文字符串仍然只有 `L"倒计时"` 一处。

**悬浮调时**：鼠标停在 `HH` / `MM` / `SS` 任一段上，该段会浮出淡圆角底，
此时滚轮直接改这一段：

| 悬浮段 | 一格滚轮 | 上限 |
|---|---|---|
| `HH` | ±1 小时 | 总时长 ≤ 23:59:59 |
| `MM` | ±1 分钟 | 同上 |
| `SS` | ±5 秒 | 同上 |

- 调整走**墙钟推算**：运行中会从新的剩余值重新起一段，不累加定时器误差；
  暂停时直接改剩余值。运行中调整不会把计时停掉（`keeps_running=1`）。
- 滚轮 delta 按 `RAWMOUSE.usButtonData` 的带符号 short 累加到 `WHEEL_DELTA`（120）
  才走一步，兼容高分辨率滚轮 / 触控板的细碎增量。
- 数字段的淡底**只在跨段时置脏重绘**，鼠标在同一段内移动不产生任何重绘。
- 调时间会清掉内部一次性触发标记（`ui_clear_end`，幂等）。
- 滚轮仍不拦截：光标在数字上滚动时，下层窗口也会照常收到滚轮。

**归零通知**：倒计时自然归零时，`WM_TIMER` 检测到 `countdown_stop_if_zero`
置位 `ended`，`ui_countdown_ended()` 用 `endNotified` 保证只触发一次，然后
**只调用 `notify_show_fa()` 弹出新版即时通知卡片**（bell / Qisland / 倒计时已结束）。

- 显示、强制展开、隐藏时自动唤醒置顶，全部由通知模块负责；
- **不再强制切回计时器页、不改 `ui.mode`**：通知不打断用户当时正在看的页面，
  关闭后还原到弹出前的 view / mode / 显隐状态；
- **旧版「标题换 bell 图标 + 淡圆底 + `00:00:00` 变色 + 鼠标移开不收起」
  的常驻提示已整体删除**。点掉或过期时 `notify_dismiss()` 调 `ui_clear_end()`
  清掉 `ended` / `endNotified`，回到完全正常的计时器（标题仍是「倒计时」，
  数字仍是本色），`cd.ended=1` 与 `=0` 两次渲染逐像素一致（`diff=0`）；
- 归零那一帧之后不再置脏，动画收敛即回到静止零重绘，不会每 200ms 空转。

## 第二个中卡片：便签（view 1）

滚轮切到第二档（`view 1`，480×130，与计时器中卡片同尺寸）就是便签。

**不突兀是首要约束**：没有输入框边框、没有白纸块、没有"点此输入"之类的提示文字，
也没有标题栏。文字直接落在卡片本体上，配色沿用中性黑白灰；空态只有左上方一个
小巧极淡的 `pencil` 图标。

**第一页的 2x2 四按钮（隐藏 / 亮度 / 音量 / taskmgr）只属于第一页**，便签页一个都不画，
连命中区都不注册；整块卡片（除最左滚轮竖条）全是文本区，所以 `box=408×114`。
最左的 5 个档位指示圆点是全局的切档指示器，保留。

**字号**：正文按文本区高度取比例，中卡片上 **26px**（上一版 12px 的两倍多），
行距收紧到 1.28 保证 3 行可见。字形仍走 4×4 面积覆盖率，为此把临时 DIB
从 1024 宽放大到 4096 —— 否则 4 倍字号的整行会被截断，只剩前半行。

**选中指示（本轮新增，按「有库就用库」）**：之前只有一条光标竖线，压根没有
「选区」这个概念 —— 没有 anchor/caret 两端、没有拖选、没有把选中范围画成高亮，
所以字选不中，自然也就没有那种蓝色指示。

先按官方文档查了两条路，结论是**不能直接用真控件**：

- `UpdateLayeredWindow` 把整块逐像素 alpha 位图交给系统合成，所有绘制必须在
  内存 DC 完成；MSDN「分层窗口」说明它不使用系统重定向机制，实测与资料都确认
  **`WS_EX_LAYERED` 窗口上的 `WS_CHILD` 子控件不会被合成、直接不可见**
  （[Microsoft Learn: Layered Windows](https://learn.microsoft.com/en-us/windows/win32/winmsg/window-features)、
  [UpdateLayeredWindow](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-updatelayeredwindow)）。
  所以 EDIT / RichEdit / Scintilla 放上来就是一片空白。
- 微软有 [Windowless Rich Edit（`CreateTextServices` / `ITextServices`）](https://learn.microsoft.com/en-us/windows/win32/api/textserv/nf-textserv-createtextservices)，
  Vista+ 支持、无需子窗口，但要自己实现整套 `ITextHost` vtable，且 `TxDraw` 走 GDI
  在 32bpp DIB 上不写 alpha，选区底色与文字会混成同一片灰度，无法分离。

最终选 **stb_textedit.h v1.16**（Sean Barrett，public domain，单头文件、无窗口、
无渲染、不做运行时分配）：它只维护 `cursor / select_start / select_end / undo`，
换行布局与字宽由 `note.c` 回调提供，选中矩形由 `canvas` 画进 alpha 画布。
既有库的标准选区交互，又完整保留圆角裁切、4×4 字形抗锯齿和不突兀的观感。

| 选区操作 | 效果 |
|---|---|
| 左键拖过文字 | 框选，淡蓝灰圆角高亮；跨行时逐行画，首末行只框半行 |
| `Shift` + 方向键 / Home / End / PgUp / PgDn | 键盘扩展选区 |
| `Ctrl+A` | 全选 |
| `Ctrl+C` / `Ctrl+X` | 复制 / 剪切**选中段**（不再是整篇） |
| `Ctrl+V` | 粘贴并**替换当前选区** |
| 直接打字 / IME 上屏 | 替换当前选区 |
| `Ctrl+Z` / `Ctrl+Y` | 撤销 / 重做 |
| `Ctrl+←` / `Ctrl+→` | 按词移动 / 选词 |

高亮色是暗蓝灰（`NOTE_SEL_*`，合成系数 0.62），白字保持原色；只在编辑态画，
退出编辑自动清除，且不画出内容区（实测 `note_sel_outside_px=0`）。
UTF-16 代理对也做了处理，光标/选区不会落在 emoji 两个 code unit 中间。

**为什么不用 EDIT / RichEdit 控件**：系统控件的直角边框、焦点框和自带滚动条
在分层窗口的逐像素 alpha 画布上会很突兀，也没法被圆角裁切。全部自绘：

| 环节 | 做法 |
|---|---|
| 输入 | 点击文本区临时去掉 `WS_EX_NOACTIVATE` 取得焦点，收起时加回；`WM_CHAR` 接收 ASCII，`WM_IME_COMPOSITION` 的 `GCS_RESULTSTR` 接收中文，候选框用 `ImmSetCompositionWindow` 定到光标处 |
| 光标 / 选区 | 由 stb_textedit 维护；淡竖线光标 + 淡蓝灰圆角选中块，见上表全部组合键 |
| 换行 | 按测得像素宽度自动折行；**中文禁则**：优先在逗号句号后断行，标点绝不落行首；英文不在单词中间断开 |
| 裁剪 | `canvas_set_clip` 把文本卡死在内容区，超出的行不会画到卡片外（实测溢出像素 0） |
| 滚动 | 滚轮在文本区滚内容（累加到 `WHEEL_DELTA` 才走一行），**不切视图**、不拦截输入 |
| 持久化 | UTF-8 存到 `%APPDATA%\Qisland\note.txt`，输入后 700ms 节流保存，切档 / Esc / 点卡片外 / 退出都强制保存；打开时从第一行开始 |

**滚动条的处理**：没有系统滚动条，没有箭头，没有方块，**连整条轨道都不画**。
只有内容溢出时，右缘出现一小段极淡的圆头滑块（固定短长度，不按比例撑满，
避免 8 行 / 3 行可见时变成一根竖线），按进度上下移动。空态 / 未溢出时完全不显示。
实测溢出态该区域亮像素 63，空态 0。

编辑中鼠标移开**不会收起卡片**（和通知显示期间一样），点卡片外或按 Esc 才结束编辑并保存。

## 第三个中卡片：性能监测（view 2）

**这一页按需求放开了文字政策**：允许中文标签与硬件信息文字。其余页面不受影响。

| 区域 | 内容 |
|---|---|
| 左侧三个圆环 | CPU / 内存 / 磁盘，环心是百分比数字，环下是中文标签 |
| 右上角标题 | 「性能监测」 |
| 右侧 5 行 | CPU 型号 · 核心数/内存 · 显卡 · 系统版本 · 系统盘容量与可用量 |
| 最左竖条 | 竖向阶段（性能页属于此阶段）：滚轮热区 + 4 个竖列指示圆点（无 grip、无连线） |

**实时值**（每秒采样一次，只在性能页展开时采样）：

| 指标 | 官方 API | 口径 |
|---|---|---|
| CPU | `GetSystemTimes` | `total = kernel + user`（kernel 含 idle），`busy = total - idle`；两次采样差分。间隔 1000ms，避免首次噪声 |
| 内存 | `GlobalMemoryStatusEx` | `MEMORYSTATUSEX.dwMemoryLoad` 直接是 0–100；`ullTotalPhys / ullAvailPhys` 换算 GB，**不截 32 位** |
| 磁盘 | `GetDiskFreeSpaceExW` | 系统盘（取 Windows 目录盘符）`ULARGE_INTEGER` 总量 / 空闲量算占用，**不截 32 位** |

**硬件信息**（启动时采一次）：`RegQueryValueExW` 读
`HKLM\HARDWARE\DESCRIPTION\System\CentralProcessor\0\ProcessorNameString`（顺手压掉
`(R)` `(TM)` 和连续空格）、`GetSystemInfo` 取逻辑核心数、`EnumDisplayDevicesW(NULL,0,...)`
的 `DISPLAY_DEVICE.DeviceString` 取显卡名、`GetVersionExW` 取 6.1 → Windows 7 SP1、
`GetNativeSystemInfo` 判 x64 / x86。所有硬件行按右区实际像素宽度用 `font_measure`
截断并加省略号，绝不画出卡片外。

**为什么不用 PDH**：`PdhAddCounter` 的计数器名随系统语言本地化（得改用
`PdhAddEnglishCounter`），速率型计数器官方要求两次采样间隔 ≥1 秒，还要链 `pdh.lib`
并走 COM。`GetSystemTimes` 是等价且更轻的官方路径，MSDN 社区给的算法也是它，
Win7 上零额外依赖。

**圆环渲染（彩色）**：不走任何控件，逐像素 2×2 子采样求环带面积覆盖率，扇形用
`atan2(dx, -dy)` 从顶部顺时针展开，端点做 `PERF_ARC_SOFT` 角度软化。
**颜色只作为 `canvas_blend_px` 的源色传入，覆盖率算法一行没改**，所以彩色化没有
带来任何锯齿：实测放大后 171 级亮度层次，边缘仍是连续过渡。

| 环 | 基色 RGB | 说明 |
|---|---|---|
| CPU | `86, 186, 244` | 冷蓝 |
| 内存 | `214, 128, 232` | 紫粉 |
| 磁盘 | `104, 214, 158` | 青绿 |
| ≥85% | `242, 138, 96` | 暖橙，功能性警示 |

- `perf_ring_rgb()` 是**唯一色源**，绘制与诊断共用同一套插值，不会各算一套。
- 85% 以下保持纯基色；超过后用 **smoothstep** 向暖橙插值，避免阈值处硬跳。
- 沿弧有明暗层次：起点亮度 `PERF_ARC_SHADE=0.78`，顺时针渐增到 1.0。
- 环心百分比数字跟随环色、再向白提亮 `PERF_NUM_MIX=0.38` 保证黑底可读；
  标签与硬件信息保持中性灰，底环仍是中性 `TRACK`，彩环才有对比。
- 三色实测采样两两色差 `101 / 102 / 82`（阈值 40），确实可辨识而不是「都上了色但看不出区别」。
- 数字与文字继续走 font 模块的 4×4 字形超采样。

**只读页**：性能页不注册任何可点击控件（`hitN=1`，只有滚轮竖条）。
滚轮在卡片其他位置不滚内容、不调时间，也不拦截输入；倒计时归零时会切回计时器页显示提示。

## 卡片导航（表驱动，可持续加卡片）

卡片不是硬编码的档位，而是 `src/cards.c` 里一张 `cards[]` 表。每张卡带
`tier`（MID/BIG/HUGE 尺寸档）、`draw`（TIMER/NOTE/PERF/EMPTY 渲染器）、
`group`（**V 竖向 / H 唯一的横向组**）、`slot`（组内序号）、`vdot`（对应哪个竖点）、
`isDefault`（跨组进入时的落点）。**加卡片 = 往表里插一行**，竖点数量、尺寸、滚轮序列、
右键菜单、`--diag` 导航表全部自动推导，不用改 `ui.c` / `anim.c` / `window.c` 里任何数字。

> **最终定稿：大卡片要，超大卡片也要，但仍然只有一个横向组。**
> 前两版各走了极端（只留大卡 / 只留超大卡），本版把两个尺寸档都放回同一排横点：
> 横1 性能 → 横2 大卡片 → 横3 超大卡片（最后一点）。
> 仍满足「只有一个横向」「横向最后一个点是超大卡片」「竖向第四个就是横向第二个」：
> 竖④ 的锚点是 slot1 的大卡片，超大卡片是同一排的第 3 个点。
> `big2/3/4`、`huge0/2`、第二个横向组 `GRP_U`、所有留空卡全部删除。
> 全表 **5 张卡**，只有 `GRP_V / GRP_H` 两个组，没有跨组跳转。

当前表（5 张卡、4 个竖点、唯一横向 3 个点）：

| idx | 卡 | 尺寸 | 内容 | 组 / slot | 竖点 | 横向 |
|---|---|---|---|---|---|---|
| 0 | timer | 中 480×130 | 计时器 | V / 0 | ① | — |
| 1 | note | 中 480×130 | 便签 | V / 0 | ② | — |
| 2 | perf | 中 480×130 | 性能监测 | H / 0 | ③ | **横 1** |
| 3 | big1 | **大 480×260** | **粘贴板历史**（最新在顶部） | H / 1 **默认** | ④ | **横 2** |
| 4 | huge1 | **超大 911.8×491.8** | 计时器内容 | H / 2 | ④ | **横 3 = 最后一点** |

③ 同时是横1、④ 同时是横2（大卡片）；超大卡片与它共用第 4 个竖点，
所以竖点仍是 4 个。超大尺寸取 `halfScreen − 2×HUGE_GAP`（1080p 实测圆角 28），
窗口按它 + 展开过冲 1.0825 动态算（1027×565），实测 `overshoot_fit -> OK` 没被裁。

- **方向已纠正**：往下滚（`delta < 0`）= 下一个卡片，往上滚 = 上一个。
  `0→1→2→3→4` 全程线性邻接，到 4 后夹住停住，不再跨到别的组。
- **横向滚动**：滚到第 4 个竖点后，`Shift + 滚轮` 或**硬件水平滚轮**
  （`WM_MOUSEHWHEEL` / `RI_MOUSE_HWHEEL`）调用 `cards_hmove()`，
  在唯一横向组的 3 个点之间左右移动（横1 性能 ↔ 横2 大卡片 ↔ 横3 超大卡片）。
  主竖向滚轮沿整条序列推进，两种手势互不冲突。
- **不再有跨组落点**：`cards_next()` 里「横向组 ↔ 横向组落 default」那段当前
  不会触发（只有一组），保留它只为以后真拆成多组时不用改代码。
  `V→H` 保持线性邻接，性能页不会被 default 跳掉。
- **互斥显示（关键）**：竖向阶段（vdot 0..2）只画左侧那列 4 个圆点；
  **进入横向阶段（vdot 3）竖列点整列不画**，改在卡片底部画一排圆点。
- **横排圆点居中对称、密集**：和竖列点同款 —— 同一个 `canvas_disc`、
  同样半径（当前 4.2 / 非当前 2.6）、同样 ACCENT / DIM 配色。
  但**不再按 `0.30W..0.85W` 铺满整张卡**：改成以卡片水平中心为对称中点，
  间距直接取竖点的 pitch = `0.80H × 0.55 / (CARDS_VDOTS−1)`，
  再钳到 `HDOTS_PITCH_MAX`（38 逻辑像素），免得超大卡片上被拉得过疏。
  实测 `hdots_centered midpoint == card_center symmetric=1 dense=1`，
  命中区也只有几十像素宽（不再横跨半张卡）。
  **没有轨道、长条、分段、文字、箭头、grip**。点数 = 3；停在大卡片时第 2 点亮，
  停在超大卡片时第 3 点（最后一点）亮。参数在 `config.h` 的 `HDOTS_*` 区。
- **连滚不断**：左侧竖条热区在横向阶段**保留但不画点**，仍接受滚轮，
  从第 3 张连滚到第 4、5 张不用挪鼠标。横排点上往下滚到最后一点会停住，
  往上滚组内横切回大卡片，再往上退回性能页。

## 交互

| 操作 | 效果 |
|---|---|
| 鼠标悬停到岛上 | 展开卡片；移开自动收起 |
| **滚轮（卡片最左侧竖条）** | 往下滚 = 下一张卡（共 5 张：0→1→2→3→4），往上滚 = 上一张，到 4 停住 |
| **Shift+滚轮 / 水平滚轮（竖条上）** | 唯一横向组内横向切换（横1 性能 ↔ 横2 大卡片 ↔ 横3 超大卡片） |
| **滚轮（悬浮在底部横排圆点上）** | 组内横向切换；到最后一点就停住，不再跨组；不用 Shift、不用移到竖条 |
| **点击横排某个圆点** | 直接跳到该组对应 slot 的卡片（横1 → 性能 / 横2 → 大卡片 / 横3 → 超大卡片），`hdots_slot()` 找最近点，与绘制同源 |
| **滚轮（悬浮在 HH/MM/SS 上）** | 按段调时间：±1h / ±1m / ±5s；不与切视图冲突 |
| **滚轮（悬浮在便签文本区）** | 滚便签内容，不切视图 |
| **性能监测页** | 只读展示；每秒刷新 CPU / 内存 / 磁盘圆环与硬件信息，环心百分比数字随之更新 |
| **滚轮（悬浮在粘贴板历史列表上）** | 滚条目（不是切卡片），累加到 `WHEEL_DELTA` 才走一行；不拦截输入 |
| **点击粘贴板历史某一条** | 写回系统剪贴板，可直接粘贴；该行淡底 + 右端 check 标记 |
| **trash-can 图标（历史页右上角）** | 清空历史记录（**不动系统剪贴板**）；空态不显示 |
| **滚轮（性能页 / 超大卡片其他位置）** | 不滚内容、不调时间、不拦截输入；只有最左竖条切档 |
| **通知期间滚轮** | 完全忽略（不切卡片、不滚内容、不拦截），因为通知不是导航卡片 |
| 点击便签文本区 | 开始输入（含中文 IME），点卡片外 / Esc 结束并保存 |
| **鼠标悬停便签文字区** | 指针变 **工字形 `IDC_IBEAM`**；移出文本区 / 切到计时器页 / 收起成岛立即变回箭头 `IDC_ARROW` |
| 最左竖条 | 竖向阶段的滚轮热区 + **4 个**竖列圆点（当前点实心）；**无 grip 图标、无连接线**。横向阶段**整列点不画**，热区仍接受滚轮 |
| 底部横排圆点 | 横向阶段出现（与竖列点互斥）；**3 个**圆点**居中对称、密集**（间距 = 竖点 pitch）；第 2 个亮 = 大卡片 / 第 3 个亮 = 超大卡片（最后一点），同时是滚轮 / 点击热区 |
| **即时通知卡片** | 中卡片尺寸；左侧 FA 或软件 HICON，右侧程序名 + 详细信息；点卡片本体 / 右上 `xmark` 关闭，5 秒后自动消失；**不占竖点横点、不写盘、不排队** |
| 倒计时自然归零 | **只弹即时通知卡片**（`bell` / Qisland / 倒计时已结束）；不切页、不改模式；点掉或过期后是**正常计时器**（无 bell / 淡圆底 / 变色数字）；点通知即确认提醒 |
| eye-slash 图标（左上） | 隐藏岛，`Ctrl+Alt+N` 唤回 |
| sun 图标（右上） | 单击切双滑块 / 按住左右拖动调亮度；低亮度自动换 moon 图标 |
| volume 图标（左下） | 单击切双滑块 / 按住左右拖动调音量；低音量自动换低音量 / 静音图标 |
| list-check 图标（右下） | 启动任务管理器 |
| 两条滑块（mode 1） | 点击或拖动调值；**启动 / 重置图标同时隐藏**（不绘制、不注册命中），2x2 四按钮与两条滑块保留 |
| play / pause 图标 | 开始 / 暂停倒计时（默认 5 分钟） |
| clock-rotate-left 图标 | 重新倒计时（重置为 5 分钟） |
| 单击空白 / 双击 | 无任何弹簧形变；点击只改变功能状态 |

展开弹簧保留（8.25% 过冲），收起接近临界阻尼（回弹 0.10%）。

**关于工字形指针**：它和「插入点」是两件东西——`IDC_IBEAM` 是鼠标指针形状，
插入点（caret）仍是便签里那条淡竖线，Windows 标准文本编辑器本来就是这样。
窗口类注册的是 `IDC_ARROW`，所以之前整块卡片（包括便签）都是箭头；现在在
`WM_SETCURSOR` 里按实时几何判定，命中便签文本区就 `SetCursor(IDC_IBEAM)`，
其余 `SetCursor(IDC_ARROW)`，并返回 `TRUE`。判定函数 `window_text_cursor()`
用 `ui_note_box()` 的动画几何，不依赖 `ui_render` 构建的命中表，切档当帧也不滞后。
这是纯事件驱动：不设定时器、不装钩子、**一次重绘都不产生**。

## 官方文档（本轮读过的）

### Font Awesome

- **自托管 Web Fonts**：官方说明 `/webfonts` 放字体文件，Solid Free 对应
  `fa-solid-900.ttf`，CSS 家族 `'Font Awesome 6 Free'`，font-weight 900。
  https://fontawesome.com/v6/docs/web/setup/host-yourself/webfonts
- **图标码点**：取自官方 npm 包内 `metadata/icons.yml` 的 `unicode` 字段，
  生成 `src/icons.h`。没有手抄任何码点。
- **许可**（`assets/LICENSE.txt`）：图标 CC BY 4.0、字体 SIL OFL 1.1、
  代码/元数据 MIT；保留字体名 "Font Awesome"。
- 字体来源：`https://mirrors.tencent.com/npm/@fortawesome/fontawesome-free/-/fontawesome-free-6.7.2.tgz`
  （腾讯 npm 镜像，其余 CDN 在本环境被 403）。

**一个容易踩的坑**：官方 CSS 用的 `'Font Awesome 6 Free'` 是 TTF 的
`nameID=16`（typographic family）；GDI 的 `CreateFontW` 匹配 `nameID=1/4`，
也就是 **`Font Awesome 6 Free Solid`**。程序按这个顺序试两个候选，并用
`f04b (play)` 渲染自检（统计覆盖率分布，排除 fallback 导致的空白/豆腐块），
`--diag` 里的 `fa_selfcheck=1` 就是这项结果。

### Microsoft Learn

本轮新增（QQ / 微信监听）：

| API | 官方口径（已核） |
|---|---|
| `RegisterShellHookWindow` | 文档表格列有 `HSHELL_FLASH`，`lParam` = 需闪烁的窗口 `HWND`；最低 Win2000，SDK XP SP1+ 才提供声明 |
| `RegisterWindowMessageW` | 两个应用注册同一字符串返回同一消息值，注册持续到会话结束；Win2000+ |
| `QueryFullProcessImageNameW` | Vista+，句柄需 `PROCESS_QUERY_INFORMATION` 或 `PROCESS_QUERY_LIMITED_INFORMATION` |
| `GetWindowThreadProcessId` | 返回创建窗口的线程 ID 并可取 pid；Win2000+ |
| `Shell_NotifyIcon` / `NIM_MODIFY` | 只修改已有图标，`uCallbackMessage` 发给**添加图标的那个应用**；**不向其他应用广播**，Win XP+ |
| `TB_GETBUTTONINFO` / `TBBUTTONINFO` | Vista+，`cbSize` / `dwMask` 必须先填；跨进程需把结构放到目标进程地址空间 |
| `FlashWindowEx` / `FLASHWINFO` | 任务栏按钮闪烁（`FLASHW_TRAY`），XP+；这正是 `HSHELL_FLASH` 对应的场景 |


| API | 用途 |
|---|---|
| `AddFontResourceExW` + `FR_PRIVATE` | 仅本进程可用的私有字体，进程退出自动移除；无需系统安装 |
| `RemoveFontResourceExW` | 卸载时须以相同标志配对调用 |
| `CreateFontW` + `ANTIALIASED_QUALITY` | 灰度抗锯齿；不用 ClearType（次像素渲染在透明通道上会出彩边） |
| `IMMDeviceEnumerator::GetDefaultAudioEndpoint` → `IAudioEndpointVolume::Get/SetMasterVolumeLevelScalar` | 音量，归一化 0.0–1.0，Vista+ |
| `WmiMonitorBrightnessMethods::WmiSetBrightness(Timeout 秒, Brightness 百分比)` | 亮度，`Root\wmi`，返回 0 成功，Vista+ |
| `WmiMonitorBrightness::CurrentBrightness` | 读亮度初值 |
| `IWbemServices::ExecMethod` | `GetMethod → SpawnInstance → Put → ExecMethod` |
| `SetMonitorBrightness` / `GetMonitorBrightness` | 仅作 DDC/CI 兜底；**官方明确警告**很多显示器不完整实现 VESA MCCS I2C |
| `ShellExecuteW("open","taskmgr.exe")` | 启动任务管理器，成功返回 >32 |
| `SetTimer` / `WM_TIMER` | `lpTimerFunc=NULL` 时投递 `WM_TIMER` 到窗口队列；本程序 200ms 只做归零判定与运行中重绘触发 |
| `RAWMOUSE.usButtonFlags` / `usButtonData` | `RI_MOUSE_WHEEL` 时 `usButtonData` 是带符号 short，delta 为 `WHEEL_DELTA`(120) 的倍数；官方建议累加细碎增量直到达到一个 delta 再执行一步 |
| `RegisterRawInputDevices(RIDEV_INPUTSINK)` + `WM_INPUT` / `GetRawInputData` | `WM_MOUSEWHEEL` 只发给焦点窗口，`NOACTIVATE` 窗口收不到；`RIDEV_INPUTSINK` 允许后台接收且必须指定 `hwndTarget` |
| `GET_X_LPARAM` / `GET_Y_LPARAM` / `GET_WHEEL_DELTA_WPARAM` | 官方要求，多显示器下 `LOWORD/HIWORD` 会把负坐标弄错 |
| `ImmGetCompositionStringW(GCS_RESULTSTR)` | 中文 IME 上屏字串；处理过的结果不再交给 `DefWindowProc`，避免 `WM_IME_CHAR` 重复插入 |
| `ImmSetCompositionWindow` / `ImmSetCandidateWindow` | 把候选框定到便签光标处 |
| `CF_UNICODETEXT` 剪贴板 | 便签 Ctrl+C / Ctrl+V |
| `AddClipboardFormatListener` / `RemoveClipboardFormatListener` / `WM_CLIPBOARDUPDATE` | **Vista+**（Win7 可用），系统维护的 listener 列表，剪贴板每次变化 **post** 一条消息；不需要 viewer chain、不需要焦点 |
| `SetClipboardData` | 需先 `OpenClipboard` 且窗口是当前 owner；`GlobalAlloc(GMEM_MOVEABLE)`；**成功后系统拥有 hMem**，不得写入或 `GlobalFree`；响应 `WM_RENDERFORMAT` 时不得调用 |
| `GetClipboardSequenceNumber` / `GetClipboardOwner` | 序列号用于合并重复通知与轮询兜底（2000+）；owner 用于识别本程序自己的写回 |
| `GetClipboardData` + `GlobalLock` / `GlobalSize` | 读 `CF_UNICODETEXT` 按字节上限找 null terminator，不假定一定带终止符 |
| `SHGetFileInfoW(SHGFI_ICON)` / `LoadIconW` | 取软件图标 HICON；文件图标调用方 `DestroyIcon`，共享图标不销毁 |
| `DrawIconEx` + `CreateDIBSection` / `GlobalLock` | 把 HICON 画进独立 32bpp DIB；GDI 在 32bpp DIB 上不保证写 alpha，先给不透明黑底，再按亮度当覆盖率、反预乘还原色相叠进 alpha 画布（与文字/FA 同一套思路） |
| `WM_SETCURSOR` | 鼠标在窗口内移动且未捕获时发送；`lParam` 低字是 `WM_NCHITTEST` 结果。处理完**必须返回 `TRUE`**，否则 `DefWindowProc` 用类光标（箭头）覆盖。只设一次也不行，下一次鼠标移动会重置 |
| `SetCursor` / `LoadCursor(NULL, IDC_IBEAM)` | `SetCursor` 只在调用线程拥有鼠标输入时生效，可靠位置就是 `WM_SETCURSOR` 里；`IDC_IBEAM` 是系统预定义工字光标，`LoadCursor` 内部按 `LR_SHARED` 返回共享句柄，无需 `DestroyCursor` |
| `WM_NCHITTEST` → `HTCLIENT` / `HTTRANSPARENT` | 只有实心客户区由我们决定指针形状，透明区交给下层窗口 |
| `GetSystemTimes` | 返回 idle / kernel / user 的 `FILETIME` 累计 100ns tick；**kernel 含 idle**，故 `total=kernel+user`、`busy=total-idle`，两次差分得 CPU%。需 `_WIN32_WINNT ≥ 0x0501` |
| `GlobalMemoryStatusEx` / `MEMORYSTATUSEX` | `dwMemoryLoad` 是 0–100 的物理内存占用百分比；`ullTotalPhys` / `ullAvailPhys` 为字节，调用前须设 `dwLength` |
| `GetDiskFreeSpaceExW` | `ULARGE_INTEGER` 的总量 / 空闲量；官方示例明确**不能截成 32 位**；`lpDirectoryName` 可以是任意目录，这里传系统盘根目录 |
| `GetSystemInfo` / `SYSTEM_INFO` | `dwNumberOfProcessors` 是当前处理器组的逻辑处理器数；`wProcessorArchitecture` 判架构 |
| `GetNativeSystemInfo` | 取原生架构判 x64 / x86 |
| `EnumDisplayDevicesW(lpDevice=NULL)` | 枚举显示适配器，`DISPLAY_DEVICE.DeviceString` 即显卡名（第二次调用传 `DeviceName` 才是显示器名） |
| `RegQueryValueExW` | 读 `HKLM\HARDWARE\DESCRIPTION\System\CentralProcessor\0\ProcessorNameString`；缓冲区不足返回 `ERROR_MORE_DATA` 并给出所需字节数 |
| `GetVersionExW` / `OSVERSIONINFOEXW` | 官方版本对照表：6.1 + `VER_NT_WORKSTATION` = Windows 7，6.0 = Vista，10.0 = Windows 10 |
| PDH（`PdhOpenQuery` / `PdhAddEnglishCounter` / `PdhCollectQueryData`） | **读过但没采用**：计数器名会随系统语言本地化，速率型计数器官方要求两次采样间隔 ≥1 秒，还要链 `pdh.lib` |

对应文档：
[设置光标图像](https://learn.microsoft.com/zh-CN/windows/win32/learnwin32/setting-the-cursor-image)、
[WM_SETCURSOR](https://learn.microsoft.com/en-us/windows/win32/menurc/wm-setcursor)、
[关于游标](https://learn.microsoft.com/zh-cn/windows/win32/menurc/about-cursors)、
[WM_NCHITTEST](https://learn.microsoft.com/en-us/windows/win32/inputdev/wm-nchittest)、
[LoadCursor 标准光标表](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-loadcursorw)。
另外 [The Old New Thing: 光标属于线程输入状态](https://devblogs.microsoft.com/oldnewthing/20090618-00/?p=11563)
解释了为什么「改类光标」是错的做法、必须按窗口/按区域在 `WM_SETCURSOR` 里定制。

性能采集相关：
[GetSystemTimes](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-getsystemtimes)、
[GlobalMemoryStatusEx](https://learn.microsoft.com/en-us/windows/win32/api/sysinfoapi/nf-sysinfoapi-globalmemorystatusex)、
[MEMORYSTATUSEX](https://learn.microsoft.com/en-us/windows/win32/api/sysinfoapi/ns-sysinfoapi-memorystatusex)、
[GetDiskFreeSpaceExW](https://learn.microsoft.com/zh-cn/windows/win32/api/fileapi/nf-fileapi-getdiskfreespaceexw)、
[获取卷信息](https://learn.microsoft.com/zh-cn/windows/win32/fileio/volume-management-functions)、
[SYSTEM_INFO](https://learn.microsoft.com/en-us/windows/win32/api/sysinfoapi/ns-sysinfoapi-system_info)、
[EnumDisplayDevicesW](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-enumdisplaydevicesw)、
[OSVERSIONINFOW 版本对照表](https://learn.microsoft.com/zh-cn/windows/win32/api/winnt/ns-winnt-osversioninfow)、
[使用 PDH 函数取用计数器资料](https://learn.microsoft.com/zh-tw/windows/win32/perfctrs/using-the-pdh-functions-to-consume-counter-data)、
[Collecting Performance Data（两次采样）](https://learn.microsoft.com/en-us/windows/win32/perfctrs/collecting-performance-data)。

所有接口最低版本 ≤ Windows Vista（`WM_SETCURSOR` 自 Windows 2000，`GetSystemTimes` 自 XP SP1），
Win7 SP1 全覆盖。

## 图标为什么没有锯齿

`src/font.c` 的渲染管线：

1. 字形以 **4 倍字号** 用 GDI 灰度抗锯齿画进临时 32bpp DIB（黑底白字）。
   GDI 在 32bpp DIB 上不保证写 alpha，所以只读回 RGB 灰度当覆盖率。
2. 对放大图做 **4×4 box-downsample**，得到的是**真实面积覆盖率**，不是
   单像素灰度近似 —— 边缘连续多级灰阶，斜线不会出台阶。
3. 覆盖率按 `(码点/文本哈希, 字号)` 缓存，动画帧里只做颜色合成，不重复光栅化。
4. 叠加到主画布时只在 alpha > 0 的像素上做 src-over，因此天然被圆角裁切，
   **不使用 `SetWindowRgn`**（整数区域会把抗锯齿切成硬边，之前踩过）。

实测导出位图：**225 级亮度、3600+ 中间灰阶像素**，图标是真实 FA 字形，
不是豆腐块、不是位图。启动时自检字形，失败才降级为 SDF 几何图标。

## 第一个大卡片：粘贴板历史（view 3）

滚到横排第 2 个点就是它。程序常驻时自动记录系统剪贴板里的**文本**，
在这张卡片上列出；点某一条会把它**写回系统剪贴板**，可以直接 `Ctrl+V` 粘贴。

### 先读过的官方文档

| API | 文档口径 | 代码里的处理 |
|---|---|---|
| `AddClipboardFormatListener` / `WM_CLIPBOARDUPDATE` | 最低 **Windows Vista**（desktop apps），Win7 SP1 可用；窗口加入系统维护的 listener 列表后，剪贴板内容每次变化都会 **post** 一条 `WM_CLIPBOARDUPDATE` | 动态 `GetProcAddress` 加载，不用老的 `SetClipboardViewer` 链（那条要转发 `WM_DRAWCLIPBOARD`、处理 `WM_CHANGECBCHAIN`，断了会拖累别的程序）。消息与焦点无关，`WS_EX_NOACTIVATE` 分层窗口照样收得到 |
| `RemoveClipboardFormatListener` | 反注册要传注册时的原 `HWND` | `WM_DESTROY` 里先反注册再销毁窗口，退出清理再兜一次（幂等） |
| `SetClipboardData(CF_UNICODETEXT, hMem)` | 必须先 `OpenClipboard` 且窗口是当前 owner；`hMem` 必须 `GlobalAlloc(GMEM_MOVEABLE)`；**成功后内存所有权转移给系统**，不能再写入或 `GlobalFree`；响应 `WM_RENDERFORMAT` 时不得调用 | 只在失败分支 `GlobalFree`；不使用延迟呈现；`OpenClipboard` 失败重试 5×10ms，不长时间阻塞主线程 |
| `GetClipboardData` / `GlobalLock` / `GlobalSize` | 读 `CF_UNICODETEXT` 不能假定一定带 null terminator | 按 `GlobalSize/2` 个字符为上限找终止符（微软官方示例做法） |
| `GetClipboardSequenceNumber` | Windows 2000+；内容变化或清空时递增 | 合并重复投递的通知；listener 万一不可用时按 400ms 序列号轮询兜底 |
| `GetClipboardOwner` | `EmptyClipboard` 把 owner 设为当前 `OpenClipboard` 的窗口 | 识别「本程序自己写回」的通知 |

### 行为

- 只记 `CF_UNICODETEXT` 文本；图片 / 自定义格式不进历史。
- **最新在顶部**；与顶部完全相同则跳过；重新复制旧条目则提到顶部；全空白跳过。
- 上限 **32 条**，单条 **1024 UTF-16 字符**。原文完整保存（换行、制表符都保留），
  卡片上只压成一行显示，按宽度截断并补 `…`（二分找前缀，不是硬切）。
- **点条目 = 写回系统剪贴板**：淡圆底 + 右端 `check` 标记该条已回填；
  `trash-can` 图标清空历史记录（**不清空系统剪贴板**，空态连它都不显示）。
- 自写回的通知用三重判据抑制（序列号相同 / owner 是本窗口 /
  内容与刚写回相同且在 500ms 内），历史不会自己增殖。
- 滚轮在列表上滚条目，累加到 `WHEEL_DELTA` 才走一行；不拦截输入。
  溢出时右缘只有一条极淡圆头短滑块，**没有系统滚动条、没有轨道、没有箭头**。
- 字号 **26px**，与便签一致；文字用 4×4 面积覆盖率渲染，无锯齿；
  `canvas_set_clip` 把条目卡死在内容区（实测溢出像素 0）。
- 历史存 `%APPDATA%\Qisland\clipboard.bin`，二进制长度前缀格式，每次变化落盘。
  `--dump` / `--diag` / `--cliptest` 的演示数据走 `clipboard_test.bin`，**不碰真实历史**。

> **隐私提醒**：程序会记录你复制过的所有文本，包括可能的密码、令牌等敏感内容，
> 且以明文写在 `%APPDATA%`。不想留存时点右上角 `trash-can` 清空，或直接删除
> `clipboard.bin`。这是剪贴板历史工具的固有行为，用之前请知悉。

后台捕获到新内容时，只有「展开且当前正是历史页」才会置脏重绘；
收起、岛态、其他页面一律零重绘（`--bench` 静止 3 秒仍只有 1 次）。

## 即时通知系统

通知卡片和**中卡片一样大（480×130）**，但**不是导航卡片**：不进 `cards[]`、
不占竖点 / 横点、滚轮对它无效。布局是左侧**软件图标或 FA 字形**，右侧
**程序名**换行**详细信息**，右上角一个 `xmark` 关闭。

图标与文字的间距由 `config.h` 的 `NOTIFY_ICON_X / ICON_S / TEXT_X / TEXT_W`
统一给出（绘制与测试共用同一组宏）：中卡片下图标右缘 78.6px、
文字起点 108px，**间隙 29.4px**，不再贴在一起；文本区右端 0.905W 正好
避开右上角关闭图标。

| 项 | 做法 |
|---|---|
| 尺寸 | `Anim.overrideMid` 让所有 `anim_view_size()` 调用统一返回 `MID_W/MID_H/MID_R`；展开弹簧、目标态命中、窗口余量自动跟上，导航表一行不用改 |
| 图标 | `notify_show_fa()` 走 4×4 FA 字形；`notify_show_icon()` 收 HICON，`SHGetFileInfoW` 可从 exe / ico 取软件图标；取不到自动回退 FA，绝不显示空白 |
| HICON 光栅化 | 独立 32bpp DIB + 不透明黑底 → `DrawIconEx(DI_NORMAL)` → 读回 RGB，按 `max(R,G,B)` 当覆盖率、反预乘还原色相，再用 `canvas_blend_px` 叠进 alpha 画布 |
| 文字 | 程序名 22px、详细信息 16px 灰；过长按文本区宽度二分截断补 `…`（与粘贴板历史同一套） |
| 生命周期 | **内存里只有当前一条**：新通知直接顶掉旧的，`NOTIFY_MS`(5000ms) 到点自动消失；点卡片本体 / `xmark` 立即关闭；关闭时调 `ui_clear_end()` 清内部触发标记，并还原弹出前的 view / mode / 显隐状态 |
| 持久化 | **完全没有**。不写盘、不排队、不建文件；退出进程什么都不剩（`--notifytest` 里 `no_storage_file=1`） |
| 显示期间 | 强制展开到过期或被点掉；`ui_layout` 只注册两个关闭命中区，`wheelRect` / `hdotsRect` 归零；不画任何竖点 / 横点；`window_text_cursor()` 也避开，不会冒出工字形光标 |
| 性能 | `notify_tick()` 只在 active 时检查时间，静止零开销、不置脏；`--bench` 静止 3 秒仍只有 1 次重绘 |

**第一个接入方：倒计时归零**。`ui_countdown_ended()` 现在只做一件事：
调用 `notify_show_fa(FA_BELL, L"Qisland", L"倒计时已结束 · 00:00:00", NOTIFY_MS)`。
不再强制切回计时器页、不再画 bell 常驻提示 —— **新版通知是唯一的提醒形态**。
点掉或过期都走 `notify_dismiss() → ui_clear_end()`，回到正常计时器，不留第二套 UI。
以后任何模块要弹通知，调 `notify_show_*()` 一行即可。

> 通知期间 `ui_draw` 直接走 `notify_draw()` 并 return，所以计时器 / 便签 / 圆环 /
> 粘贴板历史的内容一个像素都不会渗进来；`canvas_shape` 仍用同一套圆角 SDF +
> 4×4 面积覆盖率，边缘无锯齿。

## QQ / 微信即时消息监听（Win7）

按「分而治之」落地，**不装任何全局钩子**（`WH_MOUSE_LL` 那次卡死的教训）：

| | 微信 | QQ |
|---|---|---|
| 主路径 | `RegisterShellHookWindow` + `HSHELL_FLASH`，**主 UI 线程**消息循环 | **独立后台线程**每 500ms 轮询托盘图标 / 进程窗口状态 |
| 回传 | 直接在 `WndProc` 里判定 | `PostMessageW(WM_APP_IM_SIGNAL, kind, pid)` 回主线程 |
| 兜底 | 后台线程同时枚举微信可见窗口（数量增加 + 标题 hash 变化） | 托盘 toolbar 指纹 + QQ 聊天窗口数量增加 |
| 补充 | — | QQ 前台 / 任务栏按钮闪烁同样产生 `HSHELL_FLASH`，Shell Hook 分支也接受 QQ |

### 主线程链路

1. `RegisterWindowMessageW(L"SHELLHOOK")` 拿运行时消息 ID（同字符串返回同一 ID）。
2. `RegisterShellHookWindow(hwnd)` 注册窗口 —— 它只是把 shell 事件 post 到该窗口，
   **不进钩子链、不拦截输入**。
3. `WndProc` 在 `switch` 之前识别该 ID；`wParam == HSHELL_FLASH (0x8006)` 时，
   `lParam` 是需闪烁的 `HWND` → `GetWindowThreadProcessId` → pid。
4. `QueryFullProcessImageNameW` 反查 exe（Vista+，只需
   `PROCESS_QUERY_LIMITED_INFORMATION`），比对
   `Weixin.exe / WeChat.exe / WeChatAppEx.exe` 或 `QQ.exe / QQNT.exe / TIM.exe`。
5. `im_on_signal()` 去抖 → 由 pid 取软件 `HICON`（`SHGetFileInfoW`）→
   `notify_show_icon(..., L"微信", L"有新消息", NOTIFY_MS)`。
   取不到图标回退 `FA_COMMENT_DOTS` / `FA_COMMENT`。

### 后台线程链路

线程函数 `im_poll_thread()` **不读也不写** `App / Canvas / Ui / Notify` 的任何
可变字段，快照全在自己的栈上，诊断计数用 `Interlocked*` 累加：

- `EnumWindows` 分别统计 QQ / 微信进程的可见窗口数与标题 hash，**只在窗口数
  增加时**判定（减少用户自己切换会话造成的误报）；
- Win7 托盘结构 `Shell_TrayWnd → TrayNotifyWnd → SysPager → ToolbarWindow32`，
  跨进程 `TB_GETBUTTONINFO` 必须把 `TBBUTTONINFO` 用 `VirtualAllocEx` +
  `WriteProcessMemory` 放进 explorer 地址空间再 `ReadProcessMemory` 读回；
  用 `SendMessageTimeoutW(..., SMTO_ABORTIFHUNG, 200ms)` 防止 explorer 卡住
  把线程挂死；
- 状态变化就 `PostMessage` 一个 `(kind, pid)`，**不跨线程传指针或字符串**；
- 500ms 的 Sleep 拆成 10×50ms 检查 `stop`，退出时 `WaitForSingleObject`，
  实测 `thread_clean_exit=1 / thread_running=0`，不留野线程。

### 去抖与生命周期

每来源独立冷却 3 秒、全局最小间隔 1.2 秒，闪烁连发不会把通知刷成一片
（`cooldown_suppressed=1`）。`HICON` 由 `Im` 缓存并在 `im_shutdown()` 统一
销毁 —— 通知 active 期间每帧重绘都要用它，不能弹完就 `DestroyIcon`。
`--imoff` 可整体关闭监听（`im_enabled=0 / shell_msg=0 / thread_running=0`）。

### 必须说清的局限

- **只有信号，没有内容**：这几条 Win32 路径都不提供发送者或消息正文，
  所以通知只写「有新消息」，不编造。要正文得叠加 UI Automation（COM，
  复杂度显著更高），本轮没做。
- **`Shell_NotifyIcon(NIM_MODIFY)` 官方明确不向其他应用广播任何通知**，
  所以 QQ 托盘图标只能轮询；Win7 托盘 toolbar 是**未文档化的 explorer
  内部结构**，这条 best-effort，取不到就自动由窗口枚举兜底。
- **微信也不保证触发 `HSHELL_FLASH`**：它对应任务栏按钮闪烁，若某版本
  只闪托盘图标就会漏掉 —— 所以补了微信窗口轮询兜底。
- 轮询判定的语义是「状态变化」而非严格的「新消息」，用户自己打开聊天
  窗口也可能触发；3 秒冷却 + `--imoff` 是目前的缓解手段。

## 模块化结构

| 文件 | 职责 |
|---|---|
| `common.h` | 系统头、`App` 前向声明、公共工具 |
| `config.h` | 全部硬编码参数（尺寸、字号、配色、弹簧、节流） |
| `app.h` | 运行时状态聚合；模块头只用 `App*`，**无循环依赖** |
| `icons.h` | 由官方 metadata 生成的 FA 码点 |
| `canvas.h/.c` | SDF 圆角矩形、预乘 BGRA 画布、覆盖比叠加原语、分层上屏 |
| `font.h/.c` | FA 私有字体加载、自检、4×4 字形覆盖率、文字/图标/中文标签绘制 |
| `sys.h/.c` | Core Audio / WMI / DDC-CI / 启动进程 |
| `cards.h/.c` | **卡片注册表 + 导航**：`cards[]` 定义全部 5 张卡（tier/draw/group/slot/vdot/default）；`cards_next()` / `cards_hmove()` / `cards_anchor()` / `cards_group_count()` / `cards_group_card()` / **`cards_is_hgroup()` / `cards_hgroup_count()` / `cards_group_default()` / `cards_group_first/last()` / `cards_group_by_order()` / `cards_last_card()`**；唯一的横向组由这些通用接口驱动，横排点数、当前点、点击跳卡、最后一点全从这里查；加卡片只改这一张表 |
| `anim.h/.c` | 视图尺寸（按 `cards_tier()` 查档，**通知态 `overrideMid` 覆盖为中卡**）、窗口余量（容纳过冲）、展开弹簧、目标态命中 |
| `countdown.h/.c` | 倒计时状态机（墙钟推算、按段调时、自然归零 ended） |
| `stb_textedit.h` | 第三方文本编辑内核（public domain，v1.16）：光标 / 选区 / 撤销；无窗口、无渲染、不分配 |
| `textedit_glue.h` | 统一 `STB_TEXTEDIT_CHARTYPE=WCHAR` 等 header/implementation 必须一致的宏 |
| `note.h/.c` | stb 宿主适配（LAYOUTROW / GETWIDTH / 增删回调）、中文禁则换行、选中高亮自绘、裁剪、美化滚动指示、UTF-8 持久化 |
| `input.h/.c` | 便签焦点切换（临时去掉 `NOACTIVATE`）、键盘 / IME / 选区剪贴板与组合键 |
| `im.h/.c` | **QQ / 微信监听**：Shell Hook（主线程）+ 后台轮询线程、跨进程托盘 toolbar 读取、进程名校验、去抖、软件 HICON 缓存与销毁、`--imoff` / `--imtest` |
| `notify.h/.c` | **即时通知**：内存态覆盖视图（`Anim.overrideMid`）、FA / HICON 图标光栅化、程序名+详细信息两行、过期 / 点击关闭、零持久化、倒计时归零接入 |
| `clip.h/.c` | **第一个大卡片 · 粘贴板历史**：`AddClipboardFormatListener` + `WM_CLIPBOARDUPDATE` 监听、`CF_UNICODETEXT` 读写（严格遵守 `SetClipboardData` 内存所有权）、三重自写回抑制、去重/上限/持久化（二进制长度前缀）、单行截断、自绘列表与美化滚动指示 |
| `perf.h/.c` | **性能监测页**：`GetSystemTimes` 差分 / `GlobalMemoryStatusEx` / `GetDiskFreeSpaceExW` 实时采样，硬件信息一次性采集，2×2 子采样圆环、角度软化、硬件行按像素宽度截断 |
| `ui.h/.c` | 仅图标布局、冒号、悬浮段高亮、命中矩形、滚轮路由、归零通知触发、mode 1 隐藏 play/reset、性能页分支、**竖列点 / 居中密集横排点互斥绘制 `draw_hdots()` / `ui_dots_pitch()` / `ui_hdots_center()`**、**通知分支 `notify_draw()` 与两个关闭热区**、整帧渲染 |
| `window.h/.c` | 分层窗口、DPI、Raw Input、热键、无文字右键菜单、WndProc（WM_TIMER 归零、输入消息转发）、**`WM_SETCURSOR` 工字形/箭头判定与 `window_text_cursor()`** |
| `tests.h/.c` | `--dump` / `--diag` / `--bench` / `--wheel` / `--gesture` / `--cdtest` / `--notetest` / **`--perftest`** |
| `main.c` | 入口、装配、消息循环、清理 |

## 输入与性能（延续之前的修复）

- **不装全局钩子**。悬停与点击由主循环轮询 `GetCursorPos` + `GetAsyncKeyState`；
  `RegisterShellHookWindow` 不是全局钩子，只是把 shell 事件 post 到注册窗口。
- QQ 后台线程每 500ms 轮询，但**不置脏、不触发 `UpdateLayeredWindow`**：
  `--bench` 静止 3 秒仍是 1 次重绘。线程只通过 `PostMessage` 送 `(kind,pid)`。
- 高频 `WM_MOUSEMOVE` **绝不置脏标记**，否则 `UpdateLayeredWindow` 风暴会在 Win7 上拖垮主线程。
- `WM_NCHITTEST` 先 `ScreenToClient` 再查最终 alpha；透明区 `HTTRANSPARENT`。
- 滚轮有四个互斥热区：卡片最左侧竖条（走整条 5 卡序列）、底部横排圆点（唯一横向组
  内 3 点之间切）、`HH/MM/SS` 悬浮段调时间、便签文本区滚内容；四者都不拦截输入，
  其余区域滚轮照常作用于下层窗口。
  横点热区按「中心 ± (n−1)/2×pitch」构造，左右各扩 `HDOTS_HIT_PAD_X`、
  上下各扩 `HDOTS_HIT_PAD`；非横向阶段 `hdotsRect` 归零，不会误判。
  绘制、命中、点圆跳卡、`--diag` 的对称断言全部共用 `ui_dots_pitch()` /
  `ui_hdots_center()`，不会出现两处算得不一样。
- `WM_MOUSEHWHEEL` 与 `RI_MOUSE_HWHEEL` 走 `ui_wheel_h()` → 纯组内 `cards_hmove()`；
  `Shift + 竖向滚轮` 等价。主竖向滚轮方向为**往下 = 下一个**。
- **只有一个横向组**：`hdots_on()` 用 `cards_is_hgroup()` 判定，横排点 / 滚轮 /
  点圆跳卡全部由 `draw_hdots()` / `hdots_wheel()` / `hdots_slot()` 一套代码驱动。
  到最后一点（超大卡片）时组内 `cards_hmove()` 与 `cards_next()` 都到头，停住不动。
- 便签编辑时临时去掉 `WS_EX_NOACTIVATE` 取得焦点（否则 `NOACTIVATE` 窗口
  收不到 `WM_CHAR` / IME），结束编辑时加回，全程不进任务栏、不抢常态焦点。
- 最左竖条**只画 4 个竖列指示圆点**（5 → 4），且**只在竖向阶段画**：顶部的
  `grip-vertical` 六点图标与贯穿点的细连接线已删除（对应码点与降级绘制分支也一并移除），
  热区矩形本身在横向阶段仍保留（不画点）以保证连滚不断。
  第 4 点是大卡片（横2），超大卡片与它共用这个竖点，都由 `cards_vdot()` 自动推导。
  横向阶段改由底部 `HDOTS_*` 参数画 **3 个**横排圆点，两者互斥、同款圆点，
  但横排改为居中对称、间距取竖点 pitch（钳到 `HDOTS_PITCH_MAX`）。
- 归零只在那一帧置脏（旧常驻提示已删）；通知同理，
  `notify_tick()` 只在 active 时检查时间，过期那一帧才置脏一次。
- **性能页只在展开且当前视图是 view 2 时采样**：`perf_tick()` 每 1000ms 才返回 1 次，
  有新样本才置脏；收起 / 其他视图 / 隐藏时一次都不采，`no_sampling_when_hidden=1`。
  读取的是 kernel32 累计计数，没有后台线程、没有定时器、没有钩子。
- 静止零重绘、动画限帧 16ms、动画时轮询 8ms / 静止 24ms。

## 编译

在 `源码/src/` 目录内运行 `build.bat`（它会自动 `cd /d %~dp0`），或在 bash 里：

```
gcc -O2 -s -mwindows -municode -Wall -Wextra -Wno-unused-parameter *.c -o Qisland.exe ^
    -luser32 -lgdi32 -lshell32 -lshlwapi -lole32 -loleaut32 -limm32 -lm
```

Windows cmd 不展开 `*.c`，所以 `build.bat` 里**显式列出全部 16 个 .c 文件**；
新增模块要同步该列表与下方模块表。产物在本目录，拷到 `软件/Qisland/`
（与 `assets/` 同级）即可运行。只依赖系统 DLL：user32 / gdi32 / shell32 /
kernel32 / msvcrt / ole32 / oleaut32 / imm32；`dxva2.dll` 是运行时动态加载，
不是硬依赖。

项目级协作规范、架构不变量、自检清单见 `源码/AGENTS.md`；
git 忽略模板见 `源码/gitignore.txt`（重命名为 `.gitignore` 使用）。

## 内部验证命令

```
Qisland.exe --diag     → diag.txt（能力、四格、双滑块、三段命中、便签、调时归零、
                         弹簧、**彩色圆环/硬件信息**、**cards[] 导航表与方向断言**、
                         **竖列/横排圆点互斥像素、当前点、滚轮、点圆跳卡**、
                         **唯一横向组 / 大卡片+超大卡片共存 / 3 点 / 末端夹取断言**、
                         **粘贴板历史 listener / 去重 / 字号 / 像素裁切**、
                         **横点居中对称 / 密集 pitch 断言**、
                         **通知中卡尺寸 / 图标 / 文字 / 无点 / 无文件 / 文字间隙**、
                         **旧提示删除 diff=0 / 滑块模式 play-reset 隐藏**、
                         **IM 架构 / Shell API 可用性 / 后台线程轮询计数 / 官方局限说明**）
Qisland.exe --dump x   → island / mid / hover / end（归零后正常计时器，无 bell）/ slider / note_empty /
                         note_long / note_selected / note_sel_multi /
                         note_scrolled / **perf** / **perf_warn** / big / huge / last /
                         **clip_empty / clip_list / clip_selected / clip_scrolled** /
                         **notify_fa / notify_icon / im_wechat / im_qq** 二十三张 bmp
                         （clip_* = 粘贴板历史四态；notify_* = FA / 软件 HICON 通知；
                          im_* = 微信 / QQ 即时消息通知；
                          big = 横2 大卡片；huge = 横3 超大卡片；last = 它的滑块模式）
Qisland.exe --cliptest → cliptest.txt（去重 / 空白跳过 / 旧条目提顶 / 溢出滚动 /
                         真实写回并回读 / 自写回不增殖 / 点击回填 / 持久化 roundtrip /
                         像素裁切与滚动指示；演示数据走 clipboard_test.bin）
Qisland.exe --bench    → bench.txt（静止 3 秒 + 每帧 24 条消息洪泛）
Qisland.exe --wheel    → wheel.txt（9 步端到端：5 卡序列 0→1→2→3→4、
                         横排 3 点上到头停住与组内横切、双向与末端夹取）
Qisland.exe --gesture  → gesture.txt（SendInput 真实鼠标：单击切模式 / 拖动调值）
Qisland.exe --cdtest   → cdtest.txt（悬浮数字滚轮调时 / 自然归零弹通知 / 点通知确认）
Qisland.exe --imtest    → imtest.txt（微信 Shell Hook 信号 / QQ 后台线程信号 /
                         真实 SHELLHOOK 消息链路 / 去抖 / 软件图标 / 线程干净退出 / 不写盘）
Qisland.exe --imoff     → 整体关闭 QQ/微信监听（可与 --diag 同用验证 enabled=0）
Qisland.exe --notifytest → notifytest.txt（中卡尺寸覆盖 / FA 与 HICON 两种图标 /
                         程序名+详细信息 / 不画竖点横点 / 点击关闭与视图还原 /
                         过期自动关闭 / 不写盘 / 倒计时归零触发通知 /
                         文字间隙 ≥24px / 旧提示 diff=0 / 滑块模式 0/0/0）
Qisland.exe --notetest → notetest.txt（输入 / 中文折行 / 溢出 / 滚动 / 保存回读 / Esc）
Qisland.exe --perftest → perftest.txt（CPU 差分采样 / 内存 / 磁盘范围 /
                         硬件信息 / 圆环与文字像素 / 收起后不再采样）
```

本版实测：`wheel_grip_lit=0`、`wheel_line_lit=0`、`wheel_dots_lit=147`
（顶部 6 点 grip 与连接线确已删除、5 个指示点仍在、滚轮热区 `wheel_at_left=YES`）、
`fa_selfcheck=1`、`grid2x2 = 1/1/1/1`、`taskmgr_in_grid=YES`、
`num_hit_rects=3 valid=1 order=1`、`colon_gap_px=14/13`、
冒号两处上下点亮度 `217`、中间 `13`（确是分离圆点）、
悬浮段淡底 `13 → 34`、按钮下方亮像素 `0`（细滑轨确已删除）、
`adjust H/M/S = 3900 / 3960 / 3955`、`keeps_running=1`、
`natural_zero ended=1`、
`--cdtest`：`MM↑ 360 / MM↓ 300 / SS↑ 305`、`ended=1 expanded=1 visible=1`、
`click notify → cleared=1`（归零弹通知，点通知即确认提醒）、`render_count=25`、
便签 `note_no_first_page_buttons=0/0/0/0 hitN=2`（四按钮只在第一页）、
`grid2x2 = 1/1/1/1`（第一页四按钮仍在）、
`note_view=1 hit_valid=1 box=408×114`、正文 26px / 可视 3 行、
空态 `bar_lit=0`、长文本 `lines=8 vis=3 overflow=1 bar_lit=63`、
`note_clip_spill_px=0`、滚动夹取 `topLine=5 = lines-vis`、
`--notetest`：`typed_len=106`、`long_lines=6 overflow=1`、
双向滚轮 `wheel_up=2 from=3` / `wheel_down=3`（回到原位）、
`persist_roundtrip=1`、`editing_after_esc=0`、
光标 `cursor_note_center=1 wheel_strip=0 outside=0 timer_page=0`、
`cursor_setcursor_ibeam=1 arrow=1 handles_valid=1`（走真实
`WM_SETCURSOR → SetCursor → GetCursor` 链路）、`--bench` 加了
`WM_SETCURSOR` 洪泛后重绘仍为 **1 次**、
选中 `note_sel_lit none=0 / all=20815`、`note_sel_outside_px=0`、
`--notetest`：`select_all=1 shift_shrink=1 drag_select=1 sel=(3,17)`、
`selection_highlight_lit=25218`、`type_replaces_selection=1`、
`undo_restores=1`、`copy_chars=106`、
`cards_nav: count=5 vdots=4 hgroups=1 last_card=4 big_cards=1 huge_cards=1`、
`nav_assert` 与 `nav_expect` 逐项一致（`only_one_hgroup=1 one_big=1 one_huge=1`
`big_before_huge=1 last_is_huge=1 last_is_last_dot=1 hdotsN=3 end_clamped=1`）、
`single_hgroup: count=1 order=V/H ok=1 is_hgroup H=1 V=0`、
`single_hgroup_table: cards=5 first=2 last=4 default=3 big=1 huge=1 empty=0`、
`single_hgroup_dots=3 last_is_huge=1 last_is_last_dot=1`、
`single_hgroup_one_big=1 one_huge=1 big_before_huge=1 v4_is_h2=1 default_is_h2=1`、
`single_hgroup_linear 0->1->2->3->4->4 (end clamped) ok=1`、
`single_hgroup_verdict=1`、
`wheel_dots_lit=127`（4 竖点）、`view clamp high=4`、
滚轮 `0→1→2→3→4→4→3→2→3→4`（前 4 步竖条走到超大卡片、横排 3 点上
末端夹取与组内横切、后 3 步双向回走）、
`hdots_lit big1=118 huge1=118 timer=0`（横向阶段有 / 竖①无）、
`vstrip_lit timer=145 big1=0 huge1=0`（★ **竖列点整列消失**，互斥成立）、
`hdots_per_dot_big1=26/66/26`、`hdots_per_dot_huge1=26/26/66`、
`hdots_current_dot big1=1 huge1=2`（横2 / 横3 分别是当前点）、
`hdots_wheel big1_down=4 big1_up=2 huge1_down=4 huge1_up=3`、
`hdots_click_dots=2/3/4 expected=2/3/4 ok=1`、
`hdots_single_group_verdict=1`、
`hdots_style: CENTERED + SYMMETRIC / dense pitch / radii 4.2:2.6 / ACCENT:DIM`、
`hdots_centered midpoint=240.0 card_center=240.0 symmetric=1 dense=1`、
`hdots_click_dots=2/3/4 ok=1`（密集后点圆跳卡仍准）、
`view3 = 480.0 x 260.0 r=20.0`、`view4 = 911.8 x 491.8 r=28.0`、
`overshoot_fit need=987.0x532.4 window=1027.0x565.0 -> OK`、
`vol_direct_click_right=1.000`（滑块纯逻辑对照）、
静止 3 秒重绘 **1 次**、
彩色圆环 `perf_ring_colors CPU=(67,145,190) MEM=(143,88,156) DISK=(72,143,108)`、
`color_dist 101/102/82 distinguishable=1 saturated=1`、
`base_at_0=1 warn_at_100=1`（插值端点）、`perf_ring_lit_px=3049` /
`antialias_levels=171`（原中性版 64 级，彩色层次更多且无锯齿）、
`perf_hw_text_lit_px=1308` / `perf_hitN_on_page=1`、
`--perftest`：`samples 1→3`、`range cpu_ok=1 mem_ok=1 disk_ok=1`、
`hw cpu=1 gpu=1 os=1`、`ring_lit_px=947`、`hw_text_lit_px=1308`、
`no_sampling_when_hidden=1`、
**粘贴板历史（`--cliptest` / `--diag`）**：
`listener_ok=1`（Vista+ 注册成功）、`count_empty=0 count_after_inserts=5`、
`dup_skipped=1 self_skipped=1`、`scroll top 0 -> 2 overflow_ok=1`、
`set_clipboard_write_ok=1 readback_match=1`（真实写回系统剪贴板后回读一致）、
`self_update_no_grow=1`（自写回不当成新复制）、
`click_selected=0 click_writeback_match=1`（点条目写回与原文一致）、
`persist_roundtrip=1`、`empty_lit=373 list_lit=5133 spill_below=0 thumb_lit=139`、
`clip_layout vis_rows=5 font_px=26 line_h=38 overflow=1`、
`clip_pixels empty_lit=373 list_lit=8864 spill_below=0 thumb_lit=139`。
wine 里剪贴板 API 完整可用（未出现预期降级），真机 Win7 同样走这套接口。

**即时通知（`--notifytest` / `--diag`）**：
`active_before=0 active_after=1`、`size_is_mid=1`（480×130 与中卡片一致）、
`icon_lit=1155 text_lit=1198`（FA/HICON + 程序名 + 详细信息都画进 alpha 画布）、
`no_vertical_or_horizontal_dots=1 no_nav_hit_rects=1`（通知不占导航）、
`hicon_lit=2499`（软件图标链路）、
`click_dismiss=1 view_restored=1`、`expire_auto_dismiss=1`、
`countdown_hook=1`（倒计时归零确实弹了通知）、
`no_storage_file=1 shown=3`、`notify_pixels dots_lit=0`、
`notify_size=480.0x130.0 mid=480.0x130.0 match=1`。
`--cdtest` 改为「点通知关闭即确认提醒」后仍 `cleared=1`。

**本轮三项收口（`--notifytest` / `--diag`）**：
`notify_gap_px=29.4 min=24.0 ok=1`（图标右缘 → 程序名起点，文字不再贴图标）、
`old_end_hint_diff_px=0 old_end_hint_removed=1`（`cd.ended=1` 与 `=0` 逐像素一致，
旧 bell 常驻提示整段删除）、
`slider_mode play_hit=0 reset_hit=0 right_lit=0`（双滑块模式不画也不响应 play/reset）、
`countdown_notify_only`（归零 → 通知 → 点掉/过期 → `ui_clear_end` → 正常计时器）、
`natural_zero ended=1 expanded=1 visible=1`、`click notify → cleared=1`。

**QQ / 微信监听（`--imtest` / `--diag` / `--bench`）**：
`shell_api_exported=1 shell_msg=49193 thread_started=1`、
`wechat notify_active=1 appname_ok=1`、`qq notify_active=1 appname_ok=1`、
`cooldown_suppressed=1`、`shellhook_msgs=1 wechat_hits=2`、
`worker win_polls=6 tray_polls=6 tray_ok=0`（wine 无真实 explorer 托盘，预期）、
`thread_clean_exit=1 thread_exited=1 thread_running=0`、
`no_storage_file=1`、静止 3 秒重绘 **1 次**、`--imoff` 下
`im_enabled=0 shell_msg=0 hook_registered=0 thread_running=0`。
既有回归全过：导航表、横点居中密集、粘贴板历史、便签选中、彩色圆环、
工字形指针、旧提示 `diff=0`、滑块模式 `0/0/0`。

展开峰值 1.0825 / 收起 0.0010、静止 3 秒重绘 **1 次**、常驻无崩溃、编译 0 警告。

## 已知边界

- **QQ/微信监听在 wine 里无法端到端验证真实信号**：wine 导出了
  `RegisterShellHookWindow` 但调用返回 FALSE（`hook_registered=0`），
  也没有真实 QQ/微信进程与 explorer 托盘（`tray_ok=0`）。所以用
  `--imtest` 把本进程 pid 当作目标，走**完全相同**的
  `PostMessage → WndProc → im_on_signal → notify` 链路验证逻辑；
  真机 Win7 上请实际收一条消息确认。若 `hook_registered=0`，
  跑 `Qisland.exe --diag` 看 `im_api_exported` 与 `im_enabled`。
- **只有「有新消息」信号，没有发送者 / 正文**；轮询判定是「状态变化」，
  自己打开聊天窗口也可能弹通知，用 `--imoff` 可整体关闭。
- 预览图里微信/QQ 左侧是 wine 的共享图标 `IDI_APPLICATION`（白窗口方块），
  真机 Win7 上 `SHGetFileInfoW` 会取到来源程序自己的图标。
- 测试环境是 wine：Core Audio 可读但写入被 stub 拒绝、WMI 不可用、无真实显示器，
  所以 `--diag` 里 `set_volume/set_brightness` 返回失败，属**预期降级**；真机 Win7 上生效。
- 亮度主路 WMI（内置屏），失败才走 DDC/CI（外接屏）；两路都失败时滑块仍可拖动，
  程序不崩不卡。
- 测试环境原本没有中文字体，我在 wine 里把雅黑替换成 Noto CJK 才让预览图里的
  「倒计时」正常显示；这不随包分发，Win7 自带雅黑。
- 预览图里的硬件信息来自测试机（wine 报 `Wine Adapter` / `AMD EPYC 9K65`），
  CPU 空闲时 `cpu=0.0` 是 wine 的 `GetSystemTimes` 实现较粗，**真机会显示你自己的
  CPU / 显卡 / 内存 / 系统盘**，并按实际负载跳动。`RegQueryValueExW` 读不到
  `ProcessorNameString` 时会显示 `Unknown CPU`，不会崩。
- wine 里 `SHGetFileInfoW(L"notepad.exe")` 取不到文件图标时，预览图与测试会
  回退到系统共享图标 `IDI_APPLICATION`；真机 Win7 上能正常取到来源程序图标。
- 预览图里的剪贴板条目中文显示为方框，是 wine 测试环境没有中文字体；
  Win7 自带雅黑会正常显示，条目文本本身完整保存在 `clipboard.bin` 里。
- 剪贴板历史以明文保存所有复制过的文本（含敏感内容），见上面「隐私提醒」；
  不想留存就点 `trash-can` 或删除 `%APPDATA%\Qisland\clipboard.bin`。
- `--notetest` 里 `editing_after_click=0`：wine 没有窗口管理器，`SendInput` 的
  点击建立不了焦点，测试里用 `input_begin` 兜底走完输入链路（`typed_len=106`、
  `persist_roundtrip=1`）。真机 Win7 上点击便签会正常取得焦点并弹出 IME 候选框。

## 许可

Qisland 自身代码可自由使用。随包的 Font Awesome 6.7.2 Free 字体与图标受
CC BY 4.0 / SIL OFL 1.1 约束，详见 `assets/LICENSE.txt` 与 `assets/ATTRIBUTION.txt`。

`src/stb_textedit.h` 是 Sean Barrett 的 **v1.16 public domain** 单头库
（文件头与文件尾自带许可声明，无署名义务）。先按建议试了腾讯 npm 镜像的
几个路径均 404，最终从 jsdelivr 的 `nothings/stb` 镜像取得原文件，未改动内容；
项目内只在 `note.c` 里用 `#pragma GCC diagnostic push/pop` 局部屏蔽了它自身
两处「未使用」警告，方便 `-Wall -Wextra` 全开编译。
