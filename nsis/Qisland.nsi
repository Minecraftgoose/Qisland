; Qisland.nsi — NSIS 3.x 安装包（含「识别旧版并处理」的脚本化升级）
;
; 位置：源码/nsis/Qisland.nsi
; 编译：makensis Qisland.nsi
;       makensis 默认把当前工作目录切到脚本所在目录（除非 /NOCD），
;       所以下面所有相对路径都以本文件所在目录为基准。
; 产出：软件/Qisland-0.2-Setup.exe
;
; 设计要点（用户要求：不做独立升级包，升级逻辑写进同一个安装包）：
;   .onInit 里检测旧版 → 比较版本 → 静默卸载旧版 → 清理旧卸载器与注册表
;   → 再走全新安装。这样能同时解决三件事：
;     1) 旧文件残留：旧卸载器删掉它记录的文件；CleanLegacyFiles 再补删
;        历史 zip 版遗留、旧卸载器不认识的废弃文件。
;     2) 注册表混乱：卸载后 DeleteRegKey 兜底，写新值前再删一次，
;        「程序和功能」不会出现重复条目。
;     3) 卸载程序不匹配：旧 uninst.exe 用 _?= 不会自删，这里显式 Delete，
;        再由本版 WriteUninstaller 重新生成，未来卸载用的是本版文件列表。
;
; 官方依据（已核对）：
;   · /S = 静默；_?= 设置 $INSTDIR，并阻止卸载器把自己复制到临时目录
;     （Chapter3：「It also stops the uninstaller from copying itself to the
;     temporary directory... can be used along with ExecWait to wait for the
;     uninstaller to finish. It must be the last parameter and must not
;     contain any quotes」）。
;   · 旧卸载器位于 $INSTDIR 内，直接 ExecWait 会过早返回 → 先 CopyFiles
;     到 $TEMP 再执行（NSIS 官方 wiki「Auto-uninstall old before installing new」）。
;   · VIProductVersion / VIFileVersion / VIAddVersionKey 写安装包版本资源
;     （Chapter4，格式必须是 X.X.X.X）。
;   · ${VersionCompare} 来自官方随附 WordFunc.nsh：0=相等 / 1=前者新 / 2=后者新。
;   · 官方明确警告「RMDir /r $INSTDIR is not safe」——用户可能在安装目录
;     放自己的文件，所以这里绝不整目录强删，只按清单 Delete。
;
; 只对 **NSIS 安装过的旧版** 做状态迁移；历史 zip 解压版没有卸载注册表项，
; 视为全新安装，旧解压目录不会被改动（需用户自行删除）。
;
; 0.2 是第一个会真正走升级分支的版本（0.1 装过 → 0.2 安装），
; 顺带修了三条只在真升级时才暴露的问题：
;   a) `Unicode true` 下 System::Call 的 `t` 是宽字符串，必须用
;      FindWindowW；原来的 FindWindowA 永远查不到运行中的窗口；
;   b) 旧卸载节会 DeleteRegValue ...\Run\Qisland，所以先在 .onInit
;      **卸载之前**把自启动值存进 $OldRunValue，安装完再恢复并改指新 exe；
;   c) 卸载后注册表被删，InstallDirRegKey 会退回默认路径，
;      显式 `StrCpy $INSTDIR $R2` 沿用旧安装目录，目录页预填原位置。

!include "MUI2.nsh"
!include "LogicLib.nsh"
!include "WordFunc.nsh"        ; ${VersionCompare}

!define APPNAME      "Qisland"
!define APPVERSION   "0.2"       ; 与 src/version.h 的 QI_VERSION_STR 一致
!define APPVERSION4  "0.2.0.0"   ; VS_FIXEDFILEINFO / VIProductVersion 要四段
!define UNINSTKEY    "Software\Microsoft\Windows\CurrentVersion\Uninstall\Qisland"
!define RUNKEY       "Software\Microsoft\Windows\CurrentVersion\Run"
!define WNDCLASS     "QislandLayeredClass"
!define SRCDIR       "..\..\软件\Qisland"

; 升级前先记下旧的开机自启动值：旧卸载器会 DeleteRegValue 把它删掉，
; 全新安装完成后再按这个记录恢复。必须是 /GLOBAL，跨 .onInit 与 Section 共享。
Var /GLOBAL OldRunValue

Name "${APPNAME} ${APPVERSION}"
OutFile "..\..\软件\Qisland-${APPVERSION}-Setup.exe"
InstallDir "$LOCALAPPDATA\Qisland"
InstallDirRegKey HKCU "${UNINSTKEY}" "InstallLocation"
RequestExecutionLevel user          ; 全程 HKCU，Win7 不弹 UAC
SetCompressor /SOLID lzma
Unicode true
BrandingText "${APPNAME} ${APPVERSION}"
ShowInstDetails show
ShowUninstDetails show

; ---------- 安装包自身的版本资源 ----------
VIProductVersion "${APPVERSION4}"
VIFileVersion    "${APPVERSION4}"
VIAddVersionKey /LANG=1033 "ProductName"      "${APPNAME}"
VIAddVersionKey /LANG=1033 "ProductVersion"   "${APPVERSION}"
VIAddVersionKey /LANG=1033 "FileVersion"      "${APPVERSION}"
VIAddVersionKey /LANG=1033 "FileDescription"  "${APPNAME} Setup"
VIAddVersionKey /LANG=1033 "CompanyName"      "Qisland"
VIAddVersionKey /LANG=1033 "LegalCopyright"   "See LICENSE files in assets"

; ---------- MUI2 页面 ----------
!define MUI_ABORTWARNING
!define MUI_FINISHPAGE_RUN "$INSTDIR\Qisland.exe"
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "SimpChinese"
!insertmacro MUI_LANGUAGE "English"

; ============================================================
; .onInit —— 升级逻辑全部在这里，一个安装包兼顾全新装 / 重装 / 升级
; ============================================================
Function .onInit

  ; ① 程序正在运行会占用 Qisland.exe，覆盖必然失败。
  ;    System.dll 是 NSIS 官方自带插件。本脚本 `Unicode true`，
  ;    System::Call 的 `t` 是宽字符串，必须配 FindWindowW；
  ;    用 FindWindowA 会把 UTF-16 传给 ANSI API 而永远查不到窗口。
  System::Call 'user32::FindWindowW(t "${WNDCLASS}", i 0) i .r0'
  ${If} $0 != 0
    MessageBox MB_OK|MB_ICONSTOP \
      "Qisland 正在运行，请先通过托盘图标退出，再安装或升级。"
    Abort
  ${EndIf}

  ; ② 读旧版版本号；空 = 全新安装，什么都不用处理。
  ReadRegStr $R0 HKCU "${UNINSTKEY}" "DisplayVersion"
  ${If} $R0 == ""
    Return
  ${EndIf}

  ; ③ 版本比较：0 相同 / 1 本包更新 / 2 已装版本更新（禁止降级）。
  ${VersionCompare} "${APPVERSION}" "$R0" $R1
  ${If} $R1 == 2
    MessageBox MB_OK|MB_ICONSTOP \
      "已安装更新的版本 $R0，不能降级安装 ${APPVERSION}。"
    Abort
  ${ElseIf} $R1 == 0
    MessageBox MB_YESNO|MB_ICONQUESTION \
      "已安装相同版本 Qisland ${APPVERSION}，是否重新安装？" IDYES reinstall
    Abort
  ${EndIf}

reinstall:
  ; ④ 定位旧安装目录与旧卸载器。
  ;    **必须在卸载之前**读开机自启动值：0.1 的卸载节里有
  ;    DeleteRegValue ...\Run\Qisland，卸载完再读就永远是空，
  ;    会把用户原本开着的自启动悄悄关掉。
  StrCpy $OldRunValue ""
  ReadRegStr $OldRunValue HKCU "${RUNKEY}" "Qisland"
  ReadRegStr $R2 HKCU "${UNINSTKEY}" "InstallLocation"
  StrCmp $R2 "" legacy_done
  IfFileExists "$R2\uninst.exe" 0 legacy_done

  ; ⑤ 官方 wiki 的关键一步：旧卸载器在 $INSTDIR 内，直接跑会把自己
  ;    复制到 temp 并自我删除，ExecWait 等不到真正卸载完成。
  ;    先复制到独立的临时文件，再用 /S + _?= 静默原地卸载。
  Delete "$TEMP\QislandOldUninst.exe"
  CopyFiles /SILENT "$R2\uninst.exe" "$TEMP\QislandOldUninst.exe"
  IfErrors legacy_fail 0

  ; _?= 必须是最后一个参数、不能带引号（官方明文要求），
  ; 值传**旧安装目录**，让卸载器把 $INSTDIR 指向它并原地删文件。
  ExecWait '"$TEMP\QislandOldUninst.exe" /S _?=$R2' $R3
  Delete "$TEMP\QislandOldUninst.exe"

  ${If} $R3 != 0
    MessageBox MB_OK|MB_ICONSTOP \
      "旧版本卸载失败（退出码 $R3）。为避免新旧文件混在一起，安装已中止；请手动卸载后重试。"
    Abort
  ${EndIf}

  ; ⑥ _?= 会阻止卸载器删除自己 → 旧 uninst.exe 还在，显式删掉，
  ;    否则「程序和功能」里的卸载程序与新版文件列表不匹配。
  Delete "$R2\uninst.exe"
  ; 兜底清注册表：旧卸载器通常已删，这里再删一次防止重复条目。
  DeleteRegKey HKCU "${UNINSTKEY}"
  ; 注册表刚被删掉，InstallDirRegKey 下次求值会退回默认路径；
  ; 显式沿用旧目录，让目录页预填原位置、不会把升级变成「另装一份」。
  StrCpy $INSTDIR $R2
  Goto legacy_done

legacy_fail:
  MessageBox MB_OK|MB_ICONSTOP \
    "无法准备旧版本卸载程序，安装已中止；请手动卸载旧版后重试。"
  Abort

legacy_done:
FunctionEnd

; ============================================================
; 清理历史版本遗留的废弃文件（旧卸载器不认识、也不会删的那些）
; 绝不 RMDir /r $INSTDIR —— 官方警告会连用户自己放的文件一起删掉。
; ============================================================
Function CleanLegacyFiles
  Delete "$INSTDIR\uninst.exe"                 ; 旧卸载器（_?= 不自删）
  Delete "$INSTDIR\ffmpeg.exe"                 ; 录屏方案撤掉后遗留
  Delete "$INSTDIR\FFmpeg-许可与来源.txt"
  Delete "$INSTDIR\GPL-3.0.txt"
  Delete "$INSTDIR\录屏资料.md"
  Delete "$INSTDIR\Qisland_old.exe"
FunctionEnd

; ============================================================
; 主安装节
; ============================================================
Section "!${APPNAME}" SEC_MAIN
  SectionIn RO
  SetOutPath "$INSTDIR"
  Call CleanLegacyFiles
  SetOverwrite on

  File "${SRCDIR}\Qisland.exe"
  File "${SRCDIR}\README.md"
  File "${SRCDIR}\preview.png"
  File /r "${SRCDIR}\assets"

  ; 卸载器永远是**本版**生成的，文件列表与本版一致。
  WriteUninstaller "$INSTDIR\uninst.exe"

  ; 写注册表前再删一次，杜绝「程序和功能」重复条目。
  DeleteRegKey HKCU "${UNINSTKEY}"
  WriteRegStr   HKCU "${UNINSTKEY}" "DisplayName"          "${APPNAME}"
  WriteRegStr   HKCU "${UNINSTKEY}" "DisplayVersion"       "${APPVERSION}"
  WriteRegStr   HKCU "${UNINSTKEY}" "Publisher"            "Qisland"
  WriteRegStr   HKCU "${UNINSTKEY}" "DisplayIcon"          "$INSTDIR\Qisland.exe,0"
  WriteRegStr   HKCU "${UNINSTKEY}" "InstallLocation"      "$INSTDIR"
  WriteRegStr   HKCU "${UNINSTKEY}" "UninstallString"      '"$INSTDIR\uninst.exe"'
  WriteRegStr   HKCU "${UNINSTKEY}" "QuietUninstallString" '"$INSTDIR\uninst.exe" /S'
  WriteRegDWORD HKCU "${UNINSTKEY}" "NoModify"             1
  WriteRegDWORD HKCU "${UNINSTKEY}" "NoRepair"             1

  ; 状态迁移：旧版若开过开机自启动，Run 值指向旧 exe 路径，
  ; 升级后必须改指新路径；原本没开的绝不擅自打开。
  ; 用 .onInit 里**卸载之前**存下的 $OldRunValue 判断，
  ; 不能在这里现读（旧卸载器已经把 Run 值删了）。
  ${If} $OldRunValue != ""
    WriteRegStr HKCU "${RUNKEY}" "Qisland" '"$INSTDIR\Qisland.exe"'
  ${EndIf}

  CreateDirectory "$SMPROGRAMS\Qisland"
  CreateShortcut  "$SMPROGRAMS\Qisland\Qisland.lnk"        "$INSTDIR\Qisland.exe"
  CreateShortcut  "$SMPROGRAMS\Qisland\卸载 Qisland.lnk"   "$INSTDIR\uninst.exe"
SectionEnd

; ============================================================
; 卸载节：只删自己释放过的文件，用户数据保留
; %APPDATA%\Qisland\ 下的 note.txt / clipboard.bin / settings.bin 不删。
; ============================================================
Section "Uninstall"
  Delete "$INSTDIR\Qisland.exe"
  Delete "$INSTDIR\README.md"
  Delete "$INSTDIR\preview.png"
  Delete "$INSTDIR\uninst.exe"
  Delete "$INSTDIR\ffmpeg.exe"
  Delete "$INSTDIR\FFmpeg-许可与来源.txt"
  Delete "$INSTDIR\GPL-3.0.txt"
  Delete "$INSTDIR\录屏资料.md"
  Delete "$INSTDIR\assets\*.*"
  RMDir "$INSTDIR\assets"
  RMDir "$INSTDIR"

  Delete "$SMPROGRAMS\Qisland\Qisland.lnk"
  Delete "$SMPROGRAMS\Qisland\卸载 Qisland.lnk"
  RMDir "$SMPROGRAMS\Qisland"

  DeleteRegKey   HKCU "${UNINSTKEY}"
  DeleteRegValue HKCU "${RUNKEY}" "Qisland"
SectionEnd

Function un.onInit
  ; 同 .onInit：Unicode 脚本必须用 FindWindowW 配 System::Call 的 `t`
  System::Call 'user32::FindWindowW(t "${WNDCLASS}", i 0) i .r0'
  ${If} $0 != 0
    MessageBox MB_OK|MB_ICONSTOP \
      "Qisland 正在运行，请先通过托盘图标退出，再卸载。"
    Abort
  ${EndIf}
FunctionEnd
