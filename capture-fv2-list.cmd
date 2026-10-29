@echo off
rem The three pictures of Fighting Vipers 2's list of modes, taken with no window
rem on screen and no hand on the pad. They arrive in build\:
rem
rem   build\liste-1-arcade.png          the list on the game's real title screen
rem   build\liste-2-survival.png        three times down, then Enter: the game
rem                                     starts, and the select screen says
rem                                     SURVIVAL MODE
rem   build\liste-3-retour-au-titre.png Start during the demonstration: the game
rem                                     comes back to its title screen
rem
rem The list freezes the game behind it, so nothing that counts the game's frames
rem arrives any more: these trials end by the clock (VF3_QUIT_AFTER_S) and the
rem list is driven by the clock too (VF3_KEYS_S). See docs\fv2-console.md.
rem
rem Allow a minute per picture. The seconds below have margin: the list opens
rem around the 31st second on this machine, and what is frozen does not move any
rem more, so a capture taken later is the same capture.
setlocal
set PATH=C:\msys64\mingw64\bin;%PATH%
set ROM=C:\Users\frede\Documents\SEGA Model 3\ROMs\fvipers2.zip
set "VF3_DATA=%~dp0"
if "%VF3_DATA:~-1%"=="\" set "VF3_DATA=%VF3_DATA:~0,-1%"
cd /d "%~dp0"
if not exist "build\bin\vf3.exe" (
  echo build\bin\vf3.exe is missing: run build-host.cmd first.
  pause
  exit /b 1
)
set VF3_HIDDEN=1
set VF3_WINDOW=1440x1080
set VF3_3D=cpu

echo ---- 1 of 3: the list on the title screen ----
set VF3_SCRIPT=%~dp0tools\scripts\fv2-console-list.txt
set VF3_QUIT_AFTER=999999
set VF3_QUIT_AFTER_S=45
set VF3_KEYS_S=
"build\bin\vf3.exe" "%ROM%" --console
copy /y build\host_window.png build\liste-1-arcade.png > nul

echo ---- 2 of 3: SURVIVAL chosen in the list ----
set VF3_QUIT_AFTER_S=58
set VF3_KEYS_S=Down@40,Down@41,Down@42,Return@44
"build\bin\vf3.exe" "%ROM%" --console
copy /y build\host_window.png build\liste-2-survival.png > nul

echo ---- 3 of 3: Start during the demonstration ----
set VF3_SCRIPT=%~dp0tools\scripts\fv2-console-title.txt
set VF3_QUIT_AFTER=1530
set VF3_QUIT_AFTER_S=
set VF3_KEYS_S=
"build\bin\vf3.exe" "%ROM%" --console
copy /y build\host_window.png build\liste-3-retour-au-titre.png > nul

echo.
echo ---- the three pictures ----
dir /b build\liste-*.png
echo.
echo ---- finished (code %ERRORLEVEL%) ----
pause
