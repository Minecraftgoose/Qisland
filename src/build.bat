@echo off
rem Qisland build (MinGW-w64, pure C, no third-party runtime libs)
rem Run this script from this src\ folder (it cd's here automatically).
rem Output: Qisland.exe in this folder. Copy it next to the assets\ folder
rem (the runtime folder that contains assets\) before running.
rem Requires at runtime, in assets\ next to Qisland.exe:
rem   fa-solid-900.ttf          (FA icons)
rem   OPPO Sans 4.0.ttf         (global UI font, unmodified per its license)
rem   OPPO Sans 4.0 License Notice.txt (license, must be kept)
setlocal
cd /d "%~dp0"
set CC=gcc
set WINDRES=windres
rem cmd.exe does NOT expand *.c, so list all 25 translation units explicitly
rem (23 project + 2 vendored lunar library .c). Keep this list in sync.
rem settings.c uses the registry (HKCU Run) -> needs -ladvapi32.
rem tray.c uses Shell_NotifyIconW -> shell32 already linked.
set SRC=main.c window.c canvas.c font.c sys.c anim.c countdown.c note.c perf.c date.c lunar.c clip.c notify.c balloon.c tooltip.c settings.c about.c capsuleicon.c tray.c cards.c input.c ui.c tests.c vendor\lunar_date.c vendor\solar_term.c
set LIBS=-luser32 -lgdi32 -lshell32 -lshlwapi -lole32 -loleaut32 -ladvapi32 -limm32 -lm
rem 程序图标：「中间黑」那张 qisland.ico 由 qisland.rc 编成 COFF 资源对象。
%WINDRES% qisland.rc -O coff -o qisland_res.o
if errorlevel 1 (
  echo Resource compile failed.
  pause
  exit /b 1
)
%CC% -O2 -s -mwindows -municode -Wall -Wextra -Wno-unused-parameter %SRC% qisland_res.o -o Qisland.exe %LIBS%
if errorlevel 1 (
  echo Build failed.
  pause
  exit /b 1
)
echo Build OK: Qisland.exe (in this src folder)
echo Copy it into the runtime folder, next to the assets\ folder, to run.
pause
