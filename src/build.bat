@echo off
setlocal
cd /d "%~dp0"
set CC=gcc
set WINDRES=windres
set SRC=main.c window.c canvas.c font.c sys.c anim.c countdown.c note.c perf.c date.c lunar.c clip.c notify.c balloon.c tooltip.c settings.c about.c capsuleicon.c tray.c cards.c input.c ui.c update.c update_dl.c update_winhttp.c vendor\lunar_date.c vendor\solar_term.c
set LIBS=-luser32 -lgdi32 -lshell32 -lshlwapi -lole32 -loleaut32 -ladvapi32 -limm32 -lwinmm -lwininet -lwinhttp -lm
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
