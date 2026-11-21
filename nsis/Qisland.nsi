; Qisland.nsi — NSIS 3.x 安装包（含「识别旧版并处理」的脚本化升级）

!include "MUI2.nsh"
!include "LogicLib.nsh"
!include "WordFunc.nsh"        ; ${VersionCompare}

!define MUI_ICON    "..\src\qisland.ico"
!define MUI_UNICON  "..\src\qisland.ico"

!define APPNAME      "Qisland"
!define APPVERSION   "0.3.4"     ; 与 src/version.h 的 QI_VERSION_STR 一致
!define APPVERSION4  "0.3.4.0"   ; VS_FIXEDFILEINFO / VIProductVersion 要四段
!define UNINSTKEY    "Software\Microsoft\Windows\CurrentVersion\Uninstall\Qisland"
!define RUNKEY       "Software\Microsoft\Windows\CurrentVersion\Run"
!define WNDCLASS     "QislandLayeredClass"
!define SRCDIR       "..\..\软件\Qisland"

; 升级前先记下旧的开机自启动值：旧卸载器会 DeleteRegValue 把它删掉， 全新安装完成后再按这个记录恢复。 必须是 /GLOBAL，跨 .onInit 与 Section 共享。
Var /GLOBAL OldRunValue

Name "${APPNAME} v${APPVERSION}"
OutFile "..\..\软件\Qisland-${APPVERSION}-Setup.exe"
InstallDir "$LOCALAPPDATA\Qisland"
InstallDirRegKey HKCU "${UNINSTKEY}" "InstallLocation"
RequestExecutionLevel user          ; 全程 HKCU，Win7 不弹 UAC
SetCompressor /SOLID lzma
Unicode true
BrandingText "${APPNAME} v${APPVERSION}"
ShowInstDetails show
ShowUninstDetails show

; 安装包自身的版本资源
VIProductVersion "${APPVERSION4}"
VIFileVersion    "${APPVERSION4}"
VIAddVersionKey /LANG=1033 "ProductName"      "${APPNAME}"
VIAddVersionKey /LANG=1033 "ProductVersion"   "${APPVERSION}"
VIAddVersionKey /LANG=1033 "FileVersion"      "${APPVERSION}"
VIAddVersionKey /LANG=1033 "FileDescription"  "${APPNAME} Setup"
VIAddVersionKey /LANG=1033 "CompanyName"      "Qisland"
VIAddVersionKey /LANG=1033 "LegalCopyright"   "See LICENSE and LICENSES"

; MUI2 页面
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

; .onInit —— 升级逻辑全部在这里，一个安装包兼顾全新装 / 重装 / 升级
Function .onInit

  System::Call 'user32::FindWindowW(t "${WNDCLASS}", i 0) i .r0'
  ${If} $0 != 0
    MessageBox MB_OK|MB_ICONSTOP \
      "Qisland 正在运行，请先通过托盘图标退出，再安装或升级。"
    Abort
  ${EndIf}

  ; ② 读旧版版本号； 空 = 全新安装，什么都不用处理。
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
  StrCpy $OldRunValue ""
  ReadRegStr $OldRunValue HKCU "${RUNKEY}" "Qisland"
  ReadRegStr $R2 HKCU "${UNINSTKEY}" "InstallLocation"
  StrCmp $R2 "" legacy_done
  IfFileExists "$R2\uninst.exe" 0 legacy_done

  Delete "$TEMP\QislandOldUninst.exe"
  CopyFiles /SILENT "$R2\uninst.exe" "$TEMP\QislandOldUninst.exe"
  IfErrors legacy_fail 0

  ; _?= 必须是最后一个参数、不能带引号（官方明文要求）， 值传**旧安装目录**，让卸载器把 $INSTDIR 指向它并原地删文件。
  ExecWait '"$TEMP\QislandOldUninst.exe" /S _?=$R2' $R3
  Delete "$TEMP\QislandOldUninst.exe"

  ${If} $R3 != 0
    MessageBox MB_OK|MB_ICONSTOP \
      "旧版本卸载失败（退出码 $R3）。为避免新旧文件混在一起，安装已中止；请手动卸载后重试。"
    Abort
  ${EndIf}

  ; ⑥ _?= 会阻止卸载器删除自己 → 旧 uninst.exe 还在，显式删掉， 否则「程序和功能」里的卸载程序与新版文件列表不匹配。
  Delete "$R2\uninst.exe"
  DeleteRegKey HKCU "${UNINSTKEY}"
  ; 注册表刚被删掉，InstallDirRegKey 下次求值会退回默认路径； 显式沿用旧目录，让目录页预填原位置、不会把升级变成「另装一份」。
  StrCpy $INSTDIR $R2
  Goto legacy_done

legacy_fail:
  MessageBox MB_OK|MB_ICONSTOP \
    "无法准备旧版本卸载程序，安装已中止；请手动卸载旧版后重试。"
  Abort

legacy_done:
FunctionEnd

; 清理历史版本遗留的废弃文件（旧卸载器不认识、也不会删的那些） 绝不 RMDir /r $INSTDIR —— 官方警告会连用户自己放的文件一起删掉。
Function CleanLegacyFiles
  Delete "$INSTDIR\uninst.exe"                 ; 旧卸载器（_?= 不自删）
  Delete "$INSTDIR\ffmpeg.exe"                 ; 录屏方案撤掉后遗留
  Delete "$INSTDIR\FFmpeg-许可与来源.txt"
  Delete "$INSTDIR\GPL-3.0.txt"
  Delete "$INSTDIR\录屏资料.md"
  Delete "$INSTDIR\Qisland_old.exe"
  ; 0.3.1 之前随包释放过、现在不再带的文件（zip 版解压目录没有卸载器， 这里按名单补删，保证升级后目录里只剩运行必需项）
  Delete "$INSTDIR\README.md"
  Delete "$INSTDIR\preview.png"
  Delete "$INSTDIR\assets\圆形图标.ico"
  Delete "$INSTDIR\assets\白色黑边.ico"
  Delete "$INSTDIR\assets\白色黑边_透明.png"
  Delete "$INSTDIR\assets\透明圆形.png"
  Delete "$INSTDIR\assets\ATTRIBUTION.txt"
  Delete "$INSTDIR\assets\Apache-2.0.txt"
  Delete "$INSTDIR\assets\Font-Awesome-Free-License.txt"
  Delete "$INSTDIR\assets\MIT-lunar-javascript.txt"
  Delete "$INSTDIR\assets\NOTICE-lunar-calendar.txt"
  Delete "$INSTDIR\assets\OPPO-Sans-4.0-License-Notice.txt"
FunctionEnd

; 主安装节
Section "!${APPNAME}" SEC_MAIN
  SectionIn RO
  SetOutPath "$INSTDIR"
  Call CleanLegacyFiles
  SetOverwrite on

  File "${SRCDIR}\Qisland.exe"
  File "${SRCDIR}\LICENSE"           ; 项目自身 Apache-2.0 全文

  SetOutPath "$INSTDIR\assets"
  File "${SRCDIR}\assets\fa-solid-900.ttf"
  File "${SRCDIR}\assets\OPPO Sans 4.0.ttf"
  File "${SRCDIR}\assets\notice.wav"

  ; 第三方许可全文按分发合规要求保留，但单独放 LICENSES\，不混进 assets\。 OPPO Sans 许可要求任何副本都保留版权声明与协议全文。
  SetOutPath "$INSTDIR\LICENSES"
  File "${SRCDIR}\LICENSES\ATTRIBUTION.txt"
  File "${SRCDIR}\LICENSES\Apache-2.0.txt"
  File "${SRCDIR}\LICENSES\Font-Awesome-Free-License.txt"
  File "${SRCDIR}\LICENSES\MIT-lunar-javascript.txt"
  File "${SRCDIR}\LICENSES\NOTICE-lunar-calendar.txt"
  File "${SRCDIR}\LICENSES\OPPO-Sans-4.0-License-Notice.txt"

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

  ${If} $OldRunValue != ""
    WriteRegStr HKCU "${RUNKEY}" "Qisland" '"$INSTDIR\Qisland.exe"'
  ${EndIf}

  CreateDirectory "$SMPROGRAMS\Qisland"
  CreateShortcut  "$SMPROGRAMS\Qisland\Qisland.lnk"        "$INSTDIR\Qisland.exe"
  CreateShortcut  "$SMPROGRAMS\Qisland\卸载 Qisland.lnk"   "$INSTDIR\uninst.exe"
SectionEnd

; 卸载节：只删自己释放过的文件，用户数据保留 %APPDATA%\Qisland\ 下的 note.txt / clipboard.bin / settings.bin 不删。
Section "Uninstall"
  Delete "$INSTDIR\Qisland.exe"
  Delete "$INSTDIR\LICENSE"
  ; README.md / preview.png 本版已不释放，这里只补删旧版遗留
  Delete "$INSTDIR\README.md"
  Delete "$INSTDIR\preview.png"
  Delete "$INSTDIR\uninst.exe"
  Delete "$INSTDIR\ffmpeg.exe"
  Delete "$INSTDIR\FFmpeg-许可与来源.txt"
  Delete "$INSTDIR\GPL-3.0.txt"
  Delete "$INSTDIR\录屏资料.md"
  Delete "$INSTDIR\LICENSES\*.*"
  RMDir "$INSTDIR\LICENSES"
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
