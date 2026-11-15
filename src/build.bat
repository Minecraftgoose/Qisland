@echo off
rem Qisland build (MinGW-w64, pure C, no third-party runtime libs)
rem Run this script from this src\ folder (it cd's here automatically).
rem Output: Qisland.exe in this folder. Copy it next to the assets\ folder
rem (the runtime folder that contains assets\) before running.
rem Requires: assets\fa-solid-900.ttf next to Qisland.exe at runtime.
setlocal
cd /d "%~dp0"
set CC=gcc
rem cmd.exe does NOT expand *.c, so list every translation unit explicitly.
rem Keep this list in sync when adding a new module.
set SRC=main.c window.c canvas.c font.c sys.c anim.c countdown.c note.c perf.c clip.c notify.c im.c cards.c input.c ui.c tests.c
set LIBS=-luser32 -lgdi32 -lshell32 -lshlwapi -lole32 -loleaut32 -limm32 -lm
%CC% -O2 -s -mwindows -municode -Wall -Wextra -Wno-unused-parameter %SRC% -o Qisland.exe %LIBS%
if errorlevel 1 (
  echo Build failed.
  pause
  exit /b 1
)
echo Build OK: Qisland.exe (in this src folder)
echo Copy it into the runtime folder, next to the assets\ folder, to run.
pause
