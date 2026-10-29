@echo off
rem Fighting Vipers 2, console mode, WITH THE PROBE.
rem
rem The same as jouer-fv2-console.cmd, except that the session is recorded:
rem
rem   log.txt        at the project's root. For each frame drawn: the number of 3D
rem                  models in the scene, the number of views, the game's mode and
rem                  its sub-state; and everything the game says to its sound
rem                  board.
rem   Screenshots\   what the F12 key puts there.
rem
rem WHAT TO DO WHEN THE FLAW APPEARS: press F12. The picture is written into
rem Screenshots\ with a number, and the log keeps the exact game frame. Then tell
rem Claude when it was; the log will do the rest.
rem
rem The log grows fast (one line per frame drawn). That is intended: it is
rem overwritten at every launch.
setlocal
set PATH=C:\msys64\mingw64\bin;%PATH%
set ROM=C:\Users\frede\Documents\SEGA Model 3\ROMs\fvipers2.zip
set "VF3_DATA=%~dp0"
if "%VF3_DATA:~-1%"=="\" set "VF3_DATA=%VF3_DATA:~0,-1%"
cd /d "%~dp0"
if not exist "build\bin\vf3.exe" (
  echo build\bin\vf3.exe is missing: run construire-hote.cmd first.
  pause
  exit /b 1
)
set VF3_PIC_LOG=1
set VF3_MIDI_LOG=60
rem The menu's sound probe: for each frame of sound made under the list or under
rem OPTIONS, the pace, the peak, and above all WHAT IS WAITING IN SDL'S QUEUE. The
rem board was measured making the sound effects on the very frame of the key, so a
rem sound effect heard late is a sound effect waiting behind something -- and that
rem is what this line says. It can only be measured on a real machine: a session
rem without a window has no sound board.
set VF3_MENU_SOUND=1
"build\bin\vf3.exe" "%ROM%" --console %*
echo.
echo ---- finished. The log is in log.txt, the captures in Screenshots. ----
pause
