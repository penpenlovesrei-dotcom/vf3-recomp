@echo off
rem Fighting Vipers 2 in the window, with the sound and the pads.
rem SDL 3 is needed: pacman -S mingw-w64-x86_64-sdl3 in MSYS2, then build-tools.cmd.
rem This game's settings and NVRAM are kept apart from Virtua Fighter 3's.
setlocal
set PATH=C:\msys64\mingw64\bin;%PATH%
set ROM=C:\Users\frede\Documents\SEGA Model 3\ROMs\fvipers2.zip
rem This port files what belongs to it -- settings, NVRAM, log, portraits --
rem in the project's folder, the one this launcher sits in. All four launchers
rem pin the same one, or each would have its own.
set "VF3_DATA=%~dp0"
if "%VF3_DATA:~-1%"=="\" set "VF3_DATA=%VF3_DATA:~0,-1%"
cd /d "%~dp0"
if not exist "build\bin\vf3.exe" (
  echo build\bin\vf3.exe is missing: install SDL 3 then run build-tools.cmd.
  pause
  exit /b 1
)
"build\bin\vf3.exe" "%ROM%" %*
