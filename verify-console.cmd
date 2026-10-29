@echo off
rem Fighting Vipers 2: the console menu's two routes, WITHOUT A WINDOW, without hands, and verified.
rem
rem It plays two hidden sessions -- one in VERSUS, one in ARCADE -- and compares the sequence of states
rem passed through with the one expected. It does not ask anyone to read a trace: it says whether the
rem trace is right. To be run again after any change to the console layer (console_state.h,
rem console_play.h, main.cpp).
rem
rem   VERSUS : PLAY MENU PLAY STAGE PLAY PAUSE MENU_IN MENU
rem   ARCADE : PLAY MENU PLAY       PLAY PAUSE MENU_IN MENU  (no carousel, which belongs to the V.S. line)
rem   TRAINING : the same sequence as ARCADE, but it starts up the stopped clock, the held health, the
rem              seven announcement states cut short, the announcer thrown away and the exit from a
rem              two-player match.
rem
rem Allow a quarter of an hour: three sessions of seven thousand frames each, without a window.
setlocal
set PATH=C:\msys64\mingw64\bin;%PATH%
set ROM=C:\Users\frede\Documents\SEGA Model 3\ROMs\fvipers2.zip
cd /d "%~dp0"
if not exist "build\bin\vf3.exe" (
  echo build\bin\vf3.exe is missing: run build-host.cmd first.
  pause
  exit /b 1
)
set VF3_HIDDEN=1
set VF3_CONSOLE_PAUSE=4
set VF3_QUIT_AFTER=7000
rem AND A CLOCK-BASED EXIT TOO, because that one is counted in THE GAME'S FRAMES: the route ends on
rem MENU, which freezes the PowerPC, so the remaining frames never run and the session does not end by
rem itself. Measured on 2026-10-27: the versus route was finished and right on time, and the process
rem was still running twenty-six minutes later. Three minutes is enough for both routes.
set VF3_QUIT_AFTER_S=180

echo ---- the VERSUS route ----
set "VF3_DATA=%~dp0build\console-route"
if not exist "%VF3_DATA%" mkdir "%VF3_DATA%"
set VF3_SCRIPT=%~dp0tools\scripts\fv2-console-route.txt
set VF3_CONSOLE_MODE=versus
set VF3_CONSOLE_STAGE=5
"build\bin\vf3.exe" "%ROM%" --console
py -3 "%~dp0tools\fv2\verify_console.py" "%VF3_DATA%\log.txt" PLAY MENU PLAY STAGE PLAY PAUSE MENU_IN MENU
set VERSUS=%ERRORLEVEL%

echo.
echo ---- the ARCADE route ----
set "VF3_DATA=%~dp0build\console-route-arcade"
if not exist "%VF3_DATA%" mkdir "%VF3_DATA%"
set VF3_SCRIPT=%~dp0tools\scripts\fv2-console-route-arcade.txt
set VF3_CONSOLE_MODE=arcade
set VF3_CONSOLE_STAGE=
"build\bin\vf3.exe" "%ROM%" --console
py -3 "%~dp0tools\fv2\verify_console.py" "%VF3_DATA%\log.txt" PLAY MENU PLAY PAUSE MENU_IN MENU
set ARCADE=%ERRORLEVEL%

echo.
echo ---- the TRAINING route ----
set "VF3_DATA=%~dp0build\console-route-training"
if not exist "%VF3_DATA%" mkdir "%VF3_DATA%"
set VF3_SCRIPT=%~dp0tools\scripts\fv2-console-route-training.txt
set VF3_CONSOLE_MODE=training
"build\bin\vf3.exe" "%ROM%" --console
py -3 "%~dp0tools\fv2\verify_console.py" "%VF3_DATA%\log.txt" PLAY MENU PLAY PAUSE MENU_IN MENU
set TRAINING=%ERRORLEVEL%
rem and the three lines this route must say in addition (fv2-console-route-training.txt)
findstr /C:"is held, the game having started" /C:"the announcer dropped" "%VF3_DATA%\log.txt"

echo.
if "%VERSUS%%ARCADE%%TRAINING%"=="000" (echo ---- all three routes are right ----) else (echo ---- VERSUS=%VERSUS% ARCADE=%ARCADE% TRAINING=%TRAINING% : zero means right ----)
echo The whole logs are in build\console-route, -arcade and -training.
pause
