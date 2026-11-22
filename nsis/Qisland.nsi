; Qisland.nsi — NSIS 3.x 安装包

!include "MUI2.nsh"
!include "LogicLib.nsh"
!include "WordFunc.nsh"        ; ${VersionCompare}

!define MUI_ICON    "..\src\qisland.ico"
!define MUI_UNICON  "..\src\qisland.ico"

!define APPNAME      "Qisland"
!define APPVERSION   "0.3.5.1"
!define APPVERSION4  "0.3.5.1"
!define UNINSTKEY    "Software\Microsoft\Windows\CurrentVersion\Uninstall\Qisland"
!define RUNKEY       "Software\Microsoft\Windows\CurrentVersion\Run"
!define WNDCLASS     "QislandLayeredClass"
!define SRCDIR       "..\..\软件\Qisland"

Var /GLOBAL OldRunValue

Name "${APPNAME} v${APPVERSION}"
OutFile "..\..\软件\Qisland-${APPVERSION}-Setup.exe"
InstallDir "$LOCALAPPDATA\Qisland"
InstallDirRegKey HKCU "${UNINSTKEY}" "InstallLocation"
RequestExecutionLevel user
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

; .onInit —— 升级逻辑
Function .onInit

  System::Call 'user32::FindWindowW(t "${WNDCLASS}", i 0) i .r0'
  ${If} $0 != 0
    MessageBox MB_OK|MB_ICONSTOP \
      "Qisland 正在运行，请先通过托盘图标退出，再安装或升级。"
    Abort
  ${EndIf}

  ; 空 = 全新安装
  ReadRegStr $R0 HKCU "${UNINSTKEY}" "DisplayVersion"
  ${If} $R0 == ""
    Return
  ${EndIf}

  ; 版本比较
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

  ExecWait '"$TEMP\QislandOldUninst.exe" /S _?=$R2' $R3
  Delete "$TEMP\QislandOldUninst.exe"

  ${If} $R3 != 0
    MessageBox MB_OK|MB_ICONSTOP \
      "旧版本卸载失败（退出码 $R3）。为避免新旧文件混在一起，安装已中止；请手动卸载后重试。"
    Abort
  ${EndIf}

  ; 显式删掉旧 uninst.exe 
  Delete "$R2\uninst.exe"
  DeleteRegKey HKCU "${UNINSTKEY}"
  StrCpy $INSTDIR $R2
  Goto legacy_done

legacy_fail:
  MessageBox MB_OK|MB_ICONSTOP \
    "无法准备旧版本卸载程序，安装已中止；请手动卸载旧版后重试。"
  Abort

legacy_done:
FunctionEnd

; 清理历史版本遗留的废弃文件
Function CleanLegacyFiles
  Delete "$INSTDIR\uninst.exe"                 ; 旧卸载器
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
  File "${SRCDIR}\LICENSE"           ; Apache-2.0 

  SetOutPath "$INSTDIR\assets"
  File "${SRCDIR}\assets\fa-solid-900.ttf"
  File "${SRCDIR}\assets\OPPO Sans 4.0.ttf"
  File "${SRCDIR}\assets\notice.wav"

  SetOutPath "$INSTDIR\LICENSES"
  File "${SRCDIR}\LICENSES\ATTRIBUTION.txt"
  File "${SRCDIR}\LICENSES\Apache-2.0.txt"
  File "${SRCDIR}\LICENSES\Font-Awesome-Free-License.txt"
  File "${SRCDIR}\LICENSES\MIT-lunar-javascript.txt"
  File "${SRCDIR}\LICENSES\NOTICE-lunar-calendar.txt"
  File "${SRCDIR}\LICENSES\OPPO-Sans-4.0-License-Notice.txt"

  WriteUninstaller "$INSTDIR\uninst.exe"

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

Section "Uninstall"
  Delete "$INSTDIR\Qisland.exe"
  Delete "$INSTDIR\LICENSE"
  Delete "$INSTDIR\README.md"
  Delete "$INSTDIR\preview.png"
  Delete "$INSTDIR\uninst.exe"
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
  System::Call 'user32::FindWindowW(t "${WNDCLASS}", i 0) i .r0'
  ${If} $0 != 0
    MessageBox MB_OK|MB_ICONSTOP \
      "Qisland 正在运行，请先通过托盘图标退出，再卸载。"
    Abort
  ${EndIf}
FunctionEnd
