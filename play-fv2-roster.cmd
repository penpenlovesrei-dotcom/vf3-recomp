@echo off
rem Fighting Vipers 2 with the hidden characters: Del Sol, Kuhn, BM and Dural
rem reachable with the arrow on the select screen, the cursor wrapping at 14
rem instead of 10. See docs\fv2-roster.md.
rem
rem A known limit, and it is the game's: past cell 10 the names and the portraits
rem are wrong or absent. The fighters load and play; it is the label above them
rem that is somebody else's.
rem
rem The setting is kept in settings.ini (full_roster = 1). To remove it: run
rem play-fv2.cmd once with --no-full-roster.
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
"build\bin\vf3.exe" "%ROM%" --full-roster %*
