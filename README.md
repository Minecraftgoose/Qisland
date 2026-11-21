# Qisland

Windows 桌面「灵动岛」式小工具，纯 C + Win32，无第三方运行时库。
当前版本 **0.3.1**。

## 仓库结构

```
Qisland/
├─ .gitignore
├─ AGENTS.md
├─ README.md
├─ LICENSE                  # 项目自身：Apache-2.0，Copyright 2026 Minecraftgoose
├─ LICENSES/                # 全部第三方许可全文（集中存放，CI 复制进安装包）
│  ├─ ATTRIBUTION.txt
│  ├─ Apache-2.0.txt
│  ├─ MIT-lunar-javascript.txt
│  ├─ Font-Awesome-Free-License.txt
│  ├─ OPPO-Sans-4.0-License-Notice.txt
│  └─ NOTICE-lunar-calendar.txt
├─ src/                     # 22 个项目 .c + vendor/ 2 个 .c
│  ├─ build.bat  qisland.ico  qisland.rc  qisland.manifest
│  └─ vendor/
├─ nsis/Qisland.nsi         # NSIS 3.x 安装包脚本
├─ assets/                  # 纯运行时素材：字体 / 图标 / notice.wav
└─ .github/workflows/build.yml
```

> 为什么第三方目录叫 `LICENSES/` 而不是 `LICENSE/`：同一父目录里
> **`LICENSE` 文件与 `LICENSE/` 目录不能同名共存**（NTFS / Git tree 都冲突）。
> 根目录的 `LICENSE` 留给项目自身许可证，第三方按 REUSE/SPDX 惯例放 `LICENSES/`。
> GitHub 也能自动识别根目录 `LICENSE` 并在仓库页显示 Apache-2.0。

仓库里**不含任何 `.exe`、不含测试代码**：编译与打包全部交给 CI。
`assets/` 只放运行时素材；许可全文统一在 `LICENSES/`，组装安装包时由 CI
复制进 `assets/`，所以安装包里仍然带齐所有许可文件（OPPO Sans 许可明确要求
任何副本保留版权声明与协议全文）。`src/vendor/` 里的 LICENSE / NOTICE 是
上游快照自带，保持原样。

## 发版

```bash
git tag v0.3.1
git push origin main --tags
```

push tag（或 Actions 页手动 Run workflow）后，`.github/workflows/build.yml` 会：

1. 用 MSYS2 官方仓库的 `mingw-w64-x86_64-gcc` 编译 `Qisland.exe`；
2. 把 `LICENSES/` 与根目录 `LICENSE` 复制进打包目录的 `assets/` / 软件目录；
3. 用官方 `mingw-w64-x86_64-nsis` 运行 `nsis/Qisland.nsi` 生成
   `Qisland-0.3.1-Setup.exe`；
4. 把这个安装包发布到 GitHub Release（**Release 里只有安装包一份资产**）。

`Qisland.nsi` 的相对路径是按「源码/nsis + 软件/Qisland」的布局写的，所以 CI 会在
runner 的临时目录里组装出对应层级，仓库本身保持上面的扁平结构，不会被改动。

## 本地编译（可选）

Windows + MinGW-w64 下进 `src/` 跑 `build.bat`，产物 `Qisland.exe`；把它放到与
`assets/` 同级即可双击运行。`build.bat` 显式列出 24 个翻译单元，链接
user32 / gdi32 / shell32 / shlwapi / ole32 / oleaut32 / advapi32 / imm32 / winmm / m。
`winmm` 是通知音效 `PlaySoundW` 用的系统 DLL，不是第三方库。
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
工作副本里自动同步这两处，不回写仓库；三段版本号（如 `0.3.1`）会映射成
`0.3.1.0` / `0,3,1,0`。

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
