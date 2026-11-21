# Qisland

Windows 桌面「灵动岛」式小工具，纯 C + Win32，无第三方运行时库。
当前版本 0.2，尚未正式发布。

## 仓库结构

```
Qisland/
├─ .gitignore
├─ AGENTS.md
├─ README.md
├─ src/                 # 25 个 .c + 32 个 .h + vendor/，含 build.bat / qisland.ico / qisland.rc
├─ nsis/Qisland.nsi     # NSIS 3.x 安装包脚本
├─ assets/              # 运行时字体、图标、第三方许可全文
└─ .github/workflows/build.yml
```

仓库里**不含任何 .exe**：编译与打包全部交给 CI。

## 发版

```bash
git tag v0.2
git push origin main --tags
```

push tag（或 Actions 页手动 Run workflow）后，`.github/workflows/build.yml` 会：

1. 用 MSYS2 官方仓库的 `mingw-w64-x86_64-gcc` 编译 `Qisland.exe`；
2. 用官方 `mingw-w64-x86_64-nsis` 运行 `nsis/Qisland.nsi` 生成
   `Qisland-<版本>-Setup.exe`；
3. 把这个安装包发布到 GitHub Release（**Release 里只有安装包一份资产**）。

`Qisland.nsi` 的相对路径是按「源码/nsis + 软件/Qisland」的布局写的，所以 CI 会在
runner 的临时目录里组装出对应层级，仓库本身保持上面的扁平结构，不会被改动。

## 本地编译（可选）

Windows + MinGW-w64 下进 `src/` 跑 `build.bat`，产物 `Qisland.exe`；把它放到与
`assets/` 同级即可双击运行。`build.bat` 显式列出 25 个翻译单元，链接
user32 / gdi32 / shell32 / shlwapi / ole32 / oleaut32 / advapi32 / imm32 / m。

字体以 `AddFontResourceExW(FR_PRIVATE)` 私有加载、退出时卸载，不写入系统字体目录；
缺少 `assets` 时 OPPO Sans 回退到 Segoe UI / 微软雅黑、Font Awesome 回退到几何图标，
不会崩溃。

安装包：`makensis nsis/Qisland.nsi`（需自备 NSIS 3.x 与运行时目录布局）。

## 版本号

唯一来源是 `src/version.h`（`QI_VERSION_STR` / `QI_VERSION_NUM` / `QI_VERSION_W`），
`nsis/Qisland.nsi` 顶部的 `APPVERSION` / `APPVERSION4` 需同步。CI 解析 tag 后会在
工作副本里自动同步这两处，不回写仓库。

## 许可

第三方字体与数据库的署名、许可全文见 `assets/ATTRIBUTION.txt` 及同目录 `LICENSE*`。

- **OPPO Sans 4.0**：未修改、不得单独分发或售卖，需在软件里显著声明使用，
  任何副本保留版权声明与协议全文。
- **Font Awesome Free 6.7.2**：字体 SIL OFL 1.1（保留字体名 "Font Awesome"），
  图标 CC BY 4.0，代码 / 元数据 MIT。
- **lunar-calendar**（`src/vendor/lunar_date.c`、`solar_term.c`）：Apache 2.0，
  修改清单见 `src/vendor/NOTICE.txt`。
- **lunar-javascript v1.7.7**（`src/vendor/huangli_data.h`）：MIT，仅数据表。

二次分发请保留 `assets/` 全部许可文件与 `src/vendor/NOTICE.txt`。
