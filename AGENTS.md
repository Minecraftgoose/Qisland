# AGENTS.md · Qisland 项目说明（给接手的 AI / 开发者）

> 本文件是项目级协作规范。改代码前先读完，改完按第 6 节自检。

## 1. 项目是什么

Windows 桌面「灵动岛」：**仅支持 Windows 7 SP1 及以上**，纯 Win32 + GDI 手写 C，
不依赖任何第三方运行库。唯一第三方文件是 public domain 单头库
`src/stb_textedit.h`（便签选区状态机，无窗口、无渲染、无运行时分配）。

- 界面：分层窗口 + 逐像素 alpha；圆角 SDF 面积覆盖率抗锯齿；图标用
  Font Awesome Free Solid 私有字体；**除「倒计时」三个字外，计时器/便签页
  不出现任何 UI 文字**；性能监测页是唯一放开文字政策的页面。
- 交互：悬停展开、滚轮切卡片、点击零弹簧反馈、展开弹簧保留、通知即时内存态。
- 卡片：表驱动（`src/cards.c` 的 `cards[]`），5 张卡，4 个竖点，
  唯一横向组 3 个点（性能 / 大卡片 / 超大卡片），横向最后一点是超大卡片。
- 已集成：倒计时、便签（含选中/IME/持久化）、性能监测、粘贴板历史、
  QQ/微信即时消息监听、即时通知系统。
- **项目范围严格限定为本文第 1 节列出的功能。任何超出该范围的历史适配
  均已彻底清除，禁止以任何形式重新引入，连注释和文档都不要提。**

## 2. 交付形态与目录

交付是整包 zip，顶层两个目录：

```
软件/Qisland/       ← 可直接运行：Qisland.exe + assets/（字体必须同目录）
源码/               ← AGENTS.md、gitignore.txt、README.md、src/
源码/src/           ← 全部 .c / .h + build.bat（编译脚本在这里，不在根目录）
```

`assets/fa-solid-900.ttf` 由 `GetModuleFileNameW` 定位、`AddFontResourceExW(FR_PRIVATE)`
私有加载，**不装系统字体**；文件缺失时图标降级为自绘几何形状，不会崩溃。
重命名 `gitignore.txt` 为 `.gitignore` 即可直接用于 git。

## 3. 构建

MinGW-w64（x86_64），在 `源码/src/` 目录内运行 `build.bat`，或手动：

```
gcc -O2 -s -mwindows -municode -Wall -Wextra -Wno-unused-parameter ^
    *.c -o Qisland.exe ^
    -luser32 -lgdi32 -lshell32 -lshlwapi -lole32 -loleaut32 -limm32 -lm
```

Windows cmd 不展开 `*.c`，所以 `build.bat` 里**显式列出全部 16 个 .c 文件**；
新增模块必须同步更新该列表和 `README.md` 的模块表。
编译目标：0 warning。只硬依赖系统 DLL：user32 / gdi32 / shell32 / kernel32 /
msvcrt / ole32 / oleaut32 / imm32；`dxva2.dll` 是运行时动态加载。

## 4. 架构不变量（改代码不能破）

1. **分层窗口**：`UpdateLayeredWindow(ULW_ALPHA)` 合成整块 alpha 位图，
   `WS_CHILD` 控件不会被合成 → 所有 UI 必须自绘进 `Canvas`，
   不能用 EDIT / RichEdit / 系统滚动条。
2. **GDI 在 32bpp DIB 上不保证写 alpha**：文字、FA 字形、HICON 都必须先画进
   临时 DIB（黑底白字 / 不透明底），读回灰度当覆盖率，再合成预乘 BGRA。
   字体用灰度 AA（`ANTIALIASED_QUALITY`），ClearType 会在透明通道出彩边。
3. **禁止 `SetWindowRgn`**：整数像素硬边会把抗锯齿过渡一刀切掉。
   命中判定走 `WM_NCHITTEST` 查最终 alpha。
4. **禁止全局钩子**（`WH_MOUSE_LL` 曾导致卡死）。`RegisterShellHookWindow`
   不是全局钩子，只 post shell 消息，允许使用。滚轮走 Raw Input
   `RIDEV_INPUTSINK`（`WM_MOUSEWHEEL` 只发焦点窗口，NOACTIVATE 收不到）。
5. **静止零重绘**：3 秒 + 消息洪泛下 `render_count=1`。不置脏就不重绘；
   动画限帧 16ms；后台线程不置脏、不碰 UI 状态，只 `PostMessage(kind,pid)`。
   新增任何轮询/定时器前，先想清楚它会不会每帧置脏。
6. **通知完全内存态**：不写盘、不排队、不建文件，新的顶掉旧的，
   5 秒过期或点掉即消失。`HICON` 由 `Im` 缓存，在 `im_shutdown()` 统一销毁
   （通知 active 期间每帧重绘都要用，不能弹完就 `DestroyIcon`）。
7. **卡片导航表驱动**：加卡片只改 `cards[]` 一行，点数/尺寸/滚轮序列/菜单/诊断
   全部从表推导，不要在 `ui.c` / `anim.c` / `window.c` 里硬编码组名。
8. **文字政策**：除「倒计时」、性能页硬件信息、通知的程序名+详细信息、
   以及用户自己输入的便签/剪贴板数据外，不加 UI 标签、说明、placeholder。
9. **纯 C、无 C++ 异常/RAII、无 STL**；`common.h` 统一系统头与 `WINVER=0x0601`。
   模块头文件只前向声明 `App`（见 `common.h`），避免循环包含。

## 5. 模块职责

| 文件 | 职责 |
|---|---|
| `main.c` | 参数、单实例、初始化/清理、消息循环、渲染调度、测试帧驱动 |
| `window.c/.h` | 分层窗口、DPI、Raw Input、`WM_NCHITTEST`/`WM_SETCURSOR`、热键、菜单、WndProc |
| `canvas.c/.h` | 圆角 SDF 覆盖率、逐像素合成、alpha DIB、裁切、dump bmp |
| `font.c/.h` | GDI 字体、FA 私有加载、灰度覆盖率文字/图标渲染 |
| `anim.c/.h` | 展开/收起二阶弹簧、尺寸插值、目标态命中几何、窗口余量 |
| `ui.c/.h` | 布局、命中矩形、滚轮路由、各页绘制分支、竖/横圆点互斥 |
| `cards.c/.h` | 卡片表、分组/默认落点/越界夹取/导航查询 |
| `countdown.c` | 墙钟推算、悬浮调时、归零触发 |
| `note.c` + `stb_textedit.h` | 便签文本、IME、选区、折行、持久化、自绘滚动指示 |
| `perf.c` | `GetSystemTimes` / `GlobalMemoryStatusEx` / 硬件信息 / 彩色圆环 |
| `clip.c` | `AddClipboardFormatListener` 历史、去重、写回、持久化 |
| `notify.c/.h` | 即时通知卡片、`overrideMid`、HICON 光栅化、点击/过期关闭 |
| `im.c/.h` | 微信 Shell Hook（主线程）+ QQ/微信后台轮询线程、进程名校验、去抖 |
| `sys.c` | Core Audio 音量、WMI / DDC-CI 亮度、taskmgr |
| `input.c` | 便签键盘/IME 消息、焦点与保存 |
| `tests.c` | 所有 `--xxx` 自测、`--diag`、`--dump`、像素断言 |
| `config.h` | 全部硬编码尺寸/颜色/时间常量与消息 ID |
| `icons.h` | FA 码点（取自官方 `metadata/icons.yml`，勿手抄） |

## 6. 改完必须跑的自检

```
Qisland.exe --diag        # 导航表、横点对称、四格、通知/IM 全部断言
Qisland.exe --bench       # 静止 3 秒 + 消息洪泛 → render_count=1
Qisland.exe --wheel       # 滚轮序列 0->1->2->3->4->4->3->2->3->4
Qisland.exe --cdtest      # 悬浮调时 / 归零只弹新版通知
Qisland.exe --notetest    # 输入 / 选区 / 持久化
Qisland.exe --perftest    # 采样 / 范围 / 硬件信息 / 隐藏时不采样
Qisland.exe --cliptest    # 去重 / 写回 / 持久化
Qisland.exe --notifytest  # 通知尺寸 / 图标 / 关闭 / 过期 / 不写盘
Qisland.exe --imtest      # 微信+QQ 两条信号链路 / 去抖 / 线程干净退出
Qisland.exe --imoff --diag  # 监听可整体关闭
```

验收线：编译 0 warning；上述断言全绿；`--bench` 仍是 1 次重绘；
解压整包复测；全包禁用词扫描命中 0；`--dump x` 出的 bmp
逐张看过没有溢出/裁切/锯齿。改了布局常量必须同时更新像素断言。

## 7. 已知坑

- `RegisterShellHookWindow` 的 `HSHELL_FLASH` 只对应任务栏按钮闪烁；
  `Shell_NotifyIcon(NIM_MODIFY)` **不向其他应用广播**，所以托盘图标只能轮询，
  Win7 托盘 toolbar 是未文档化的 explorer 内部结构，必须 best-effort + 窗口枚举兜底。
- 跨进程 `TB_GETBUTTONINFO` 要 `VirtualAllocEx` 把结构放进 explorer 地址空间，
  并用 `SendMessageTimeout(SMTO_ABORTIFHUNG)` 防线程挂死。
- `QueryFullProcessImageNameW` 需 Vista+；句柄开
  `PROCESS_QUERY_LIMITED_INFORMATION` 就够。
- FA 的 CSS 家族 `'Font Awesome 6 Free'` 是 nameID=16；GDI `CreateFontW` 认
  nameID=1/4 的 `Font Awesome 6 Free Solid`，代码里两个候选都试并做图标自检。
- 测试环境是 wine：Core Audio 写入被 stub 拒绝、WMI 不可用、无真实托盘、
  `RegisterShellHookWindow` 返回 FALSE、无窗口管理器（IME 焦点测不了）。
  这些是**预期降级**，不是代码坏；真机 Win7 结论以 `--diag` 为准。
- 预览图中文字体来自 wine 的 Noto CJK 替换，不随包分发；Win7 自带雅黑。

## 8. 沟通约定

用户偏好：直接给可用产物（整包 zip + 效果图），中文回复，改动点列清楚，
明确说出「哪些是真机才能确认的」和「哪些局限无法绕过」，不把示意草图
当成实际 UI 实现。涉及第三方库的方案先查官方文档再动手。
