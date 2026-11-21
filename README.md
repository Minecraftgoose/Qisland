# Qisland

Windows 桌面「灵动岛」式小工具，纯 C + Win32，无第三方运行时库。
当前版本 **0.3.2**。

## 仓库结构

```
Qisland/
├─ .gitignore
├─ AGENTS.md
├─ README.md
├─ LICENSE                  # 项目自身：Apache-2.0，Copyright 2026 Minecraftgoose
├─ LICENSES/                # 全部第三方许可全文（集中存放，CI 复制到安装包 LICENSES/）
│  ├─ ATTRIBUTION.txt
│  ├─ Apache-2.0.txt
│  ├─ MIT-lunar-javascript.txt
│  ├─ Font-Awesome-Free-License.txt
│  ├─ OPPO-Sans-4.0-License-Notice.txt
│  └─ NOTICE-lunar-calendar.txt
├─ src/                     # 24 个项目 .c + vendor/ 2 个 .c
│  ├─ build.bat  qisland.ico  qisland.rc  qisland.manifest
│  ├─ update.c  update_winhttp.c   # 检查更新（WinINet + WinHTTP 兜底）
│  └─ vendor/
├─ nsis/Qisland.nsi         # NSIS 3.x 安装包脚本
├─ assets/                  # 仓库素材：字体 / 图标 / notice.wav（安装包只挑 3 个）
└─ .github/workflows/build.yml
```

> 为什么第三方目录叫 `LICENSES/` 而不是 `LICENSE/`：同一父目录里
> **`LICENSE` 文件与 `LICENSE/` 目录不能同名共存**（NTFS / Git tree 都冲突）。
> 根目录的 `LICENSE` 留给项目自身许可证，第三方按 REUSE/SPDX 惯例放 `LICENSES/`。
> GitHub 也能自动识别根目录 `LICENSE` 并在仓库页显示 Apache-2.0。

仓库里**不含任何 `.exe`、不含测试代码**：编译与打包全部交给 CI。
仓库的 `assets/` 是素材库（字体、图标原图、音效），**安装包只挑程序真正会读的
三个文件**；许可全文统一在 `LICENSES/`，组装时复制到安装包的 `LICENSES/` 子目录，
不混进 `assets/`（OPPO Sans 许可明确要求任何副本保留版权声明与协议全文）。
`src/vendor/` 里的 LICENSE / NOTICE 是上游快照自带，保持原样。

## 发版

```bash
git tag v0.3.2
git push origin main --tags
```

push tag（或 Actions 页手动 Run workflow）后，`.github/workflows/build.yml` 会：

1. 用 MSYS2 官方仓库的 `mingw-w64-x86_64-gcc` 编译 `Qisland.exe`；
2. 只把 3 个运行时素材复制进打包目录的 `assets/`，把 `LICENSES/` 与根目录
   `LICENSE` 复制到软件目录（**不带 `README.md` / `preview.png` / 图标原图**）；
3. 用官方 `mingw-w64-x86_64-nsis` 运行 `nsis/Qisland.nsi` 生成
   `Qisland-0.3.2-Setup.exe`；
4. 把这个安装包发布到 GitHub Release（**Release 里只有安装包一份资产**）。

`Qisland.nsi` 的相对路径是按「源码/nsis + 软件/Qisland」的布局写的，所以 CI 会在
runner 的临时目录里组装出对应层级，仓库本身保持上面的扁平结构，不会被改动。

## 安装包内容

装完之后安装目录里只有这些：

```
%LOCALAPPDATA%\Qisland\
├─ Qisland.exe
├─ LICENSE                       # 项目自身 Apache-2.0 全文
├─ assets\
│  ├─ fa-solid-900.ttf           # 私有加载，程序读
│  ├─ OPPO Sans 4.0.ttf          # 私有加载，程序读
│  └─ notice.wav                 # 通知音效，程序读
└─ LICENSES\                     # 第三方许可全文，分发合规要求
   ├─ ATTRIBUTION.txt
   ├─ Apache-2.0.txt
   ├─ Font-Awesome-Free-License.txt
   ├─ MIT-lunar-javascript.txt
   ├─ NOTICE-lunar-calendar.txt
   └─ OPPO-Sans-4.0-License-Notice.txt
```

**不带** `README.md`、`preview.png`，也不带 `assets/圆形图标.ico`、
`assets/白色黑边.ico`、`assets/白色黑边_透明.png`、`assets/透明圆形.png`——
程序图标由 `qisland.rc` 编进 exe 资源，胶囊态白图标是 `capsuleicon.c` 里的
64×64 数组自绘，托盘 / 关于页 / 通知图标都取 exe 资源或 `SHGetFileInfoW`，
不读任何外部图片文件。升级安装时 `CleanLegacyFiles` 会按名单补删旧版留下的
这些文件；卸载时 `LICENSES\` 与 `assets\` 一起清掉，用户数据
（`%APPDATA%\Qisland\` 下的便签 / 剪贴板 / 设置）保留。

## 本地编译（可选）

Windows + MinGW-w64 下进 `src/` 跑 `build.bat`，产物 `Qisland.exe`；把它放到与
`assets/` 同级即可双击运行。`build.bat` 显式列出 26 个翻译单元，链接
user32 / gdi32 / shell32 / shlwapi / ole32 / oleaut32 / advapi32 / imm32 / winmm /
wininet / winhttp / m。`winmm` 是通知音效 `PlaySoundW` 用的系统 DLL，
`wininet` / `winhttp` 是检查更新用的系统 HTTP 栈，都不是第三方库。
`qisland.rc` 会把 `qisland.manifest` 编进 exe（资源类型 24 / ID 1），windres
在 `src/` 目录下运行即可找到它。

字体以 `AddFontResourceExW(FR_PRIVATE)` 私有加载、退出时卸载，不写入系统字体目录；
缺少 `assets` 时 OPPO Sans 回退到 Segoe UI / 微软雅黑、Font Awesome 回退到几何图标，
不会崩溃。

## 通知音效

`assets/notice.wav` 是一段 0.48 秒的 PCM 16-bit / 44.1 kHz / 单声道提示音
（42 KB）。每次即时通知弹出时 `notify.c` 用
`PlaySoundW(SND_FILENAME | SND_ASYNC | SND_NODEFAULT)` 播一声；新通知顶掉旧通知会重播。

**为什么用 WAV 而不是 MP3**：短 UI 提示音用未压缩 PCM 最合适——没有 MP3 的
编码器首尾填充（原 `notice.mp3` 转 WAV 后从 0.522 s 变成 0.480 s，约 42 ms
白等），触发即响；Windows 原生播放，不依赖系统 MPEG 驱动（Server Core / N 版 /
精简系统常常没装）；不需要 `open/play/close` 那套有状态的 MCI 设备；无损且结构
最简单，所有 Windows 版本都支持。代价只是 9.5 KB → 42 KB，对含 22 MB 字体的
安装包可以忽略。OGG / FLAC 之类要自带解码库，与「纯 C 无第三方运行时库」冲突。

**缺文件 / 无音频设备一律静默跳过** —— 通知卡片照常显示，不弹错误框、不阻塞
主循环、不写盘、不建线程。`SND_NODEFAULT` 保证找不到文件时不会冒出系统默认「叮」。

## 检查更新（设置 → 更新）

设置页左栏现在是「通用 / 更新 / 关于」三个页签。「更新」右栏显示当前版本，
下面一个「检查更新」按钮，再下面是可滚轮滚动的结果区。

查询结果**一律用 Qisland 自己的即时通知**报一条（内存态、不排队、到点自动
消失），不弹系统对话框。工作线程查 `releases/latest`，主线程只收消息、写
结果区、弹通知，岛全程照常动。连点由模块自己挡掉，请求在途时按钮压暗。

- 主链路 WinINet，带连接 / 接收超时，先查状态码，4xx/5xx 也把错误页读回来；
- Win7 的 Schannel 常没开 TLS 1.2，WinINet 握手失败时由独立的 WinHTTP
  翻译单元显式请求 TLS 1.2 再试一次（`wininet.h` 与 `winhttp.h` 的 typedef
  冲突，所以必须分成两个编译单元）；
- 独立看门狗：超时直接关掉阻塞中的请求句柄，允许立刻重试；
- **检查出错时和结果区一起显示原始数据**：阶段、HTTP 状态、Win32 错误码、
  URL、WinINet 扩展文本，再原样附上服务器返回的字节，完整字节另存到
  `%TEMP%\Qisland_update_error.txt`。不写「可能是没网 / 代理拦截」这类猜测。

## 高 DPI（0.3.1 起）

- `src/qisland.manifest` 声明进程 DPI 感知：`PerMonitorV2,PerMonitor,System`
  逐级回退（Win10 1703+ / Win8.1+ / 老系统）。Win7 不认新字段，由
  `SetProcessDPIAware()` 兜底。
- 运行时按**窗口所在显示器**取 DPI：`GetDpiForWindow`（Win10 1607+）→
  `GetDpiForSystem`（Win8.1+）→ `GetDeviceCaps(LOGPIXELSX)`，全部
  `GetProcAddress` 动态取，不抬 `WINVER`、不引入链接期依赖，Win7 可跑。
- 缩放上限从 200% 放宽到 **300%**；高 DPI 下形状超采样降档到 2，省光栅化开销。
- 处理 `WM_DPICHANGED`：跨屏拖动或改系统缩放时，重建画布 / HFONT / 字形缓存 /
  命中表，并 `SetWindowPos` 到系统建议的新矩形；`dpiReady` 守卫避免初始化期间
  资源还没建好就被重建。
- 尺寸基准仍按主屏（`SM_CXSCREEN` / `SM_CYSCREEN`），未做多屏联合布局。

## 版本号

唯一来源是 `src/version.h`（`QI_VERSION_STR` / `QI_VERSION_NUM` / `QI_VERSION_W`），
`nsis/Qisland.nsi` 顶部的 `APPVERSION` / `APPVERSION4` 需同步。CI 解析 tag 后会在
工作副本里自动同步这两处，不回写仓库；三段版本号（如 `0.3.2`）会映射成
`0.3.2.0` / `0,3,2,0`。

## 许可

项目自身：**Apache License 2.0**，Copyright 2026 Minecraftgoose，全文见根目录 `LICENSE`。
第三方组件的署名、许可全文与修改清单见 `LICENSES/`（索引在 `LICENSES/ATTRIBUTION.txt`）。

- **OPPO Sans 4.0**：未修改、不得单独分发或售卖，需在软件里显著声明使用，
  任何副本保留版权声明与协议全文。
- **Font Awesome Free 6.7.2**：字体 SIL OFL 1.1（保留字体名 "Font Awesome"），
  图标 CC BY 4.0，代码 / 元数据 MIT。
- **lunar-calendar**（`src/vendor/lunar_date.c`、`solar_term.c`）：Apache 2.0，
  修改清单见 `LICENSES/NOTICE-lunar-calendar.txt`。
- **lunar-javascript v1.7.7**（`src/vendor/huangli_data.h`）：MIT，仅数据表。
- **notice.wav**：由项目维护者提供的 `notice.mp3` 无损转码而来，ID3 标注
  "Feather Tap Pip / noname / 2026"；**未随附单独许可文本**，公开发布前请确认
  分发授权。

二次分发请保留 `LICENSES/` 全部文件、`src/vendor/NOTICE.txt` 与根目录 `LICENSE`。
