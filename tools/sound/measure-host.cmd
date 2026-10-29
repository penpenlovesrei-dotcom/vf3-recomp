@echo off
rem What the sound costs THE HOST, where the board runs on a thread of its own. One command, no window.
rem
rem   tools\sound\measure-host.cmd [seconds]      (120 by default)
rem
rem WHY THIS IS NOT vf3run's FIGURE. In the launcher the sound board is INSIDE the frame loop, so its
rem time is the frame's: 30.5 % under --rec (scsp/docs/scsp_audit.md sect. 4 bis). In the host it is on a
rem thread of its own, one frame behind, and its cost splits in two:
rem
rem   the CPU cost    -- the thread's work, which has to be done somewhere;
rem   the FRAME cost  -- the time the game's thread spends BLOCKED waiting for it, in finish_frame. If
rem                      that is zero, the sound costs the frame rate NOTHING, whatever CPU it burns.
rem
rem Measured on 2026-10-26 on Frederic's machine: the thread works 1.49 ms per frame, the game waits for
rem it 0.000 ms (maximum 0.016), period 17.38 ms -- so 8.5 % of a core and 0.00 % of the frame (sect. 4 ter).
rem
rem WHAT THAT MEASUREMENT DOES NOT ESTABLISH, and that is why this script exists: ONE MACHINE, with cores
rem to spare. On TWO CORES -- an AMD laptop, a Steam Deck -- the sound's thread competes with the game's
rem and with the pictures', and the wait could stop being nil. It is the only measurement in the
rem specification that needs another machine.
rem
rem NO WINDOW (VF3_HIDDEN=1): nothing is displayed, no sound comes out, the sound board runs all the same.
rem The pacing is still the Model 3's -- the probe prints the period, and it is at 17.38 ms that one sees
rem it is right. The first few hundred frames do not count, while it settles.
rem IT WORKS FLAT. To measure on another machine the repository is not carried over: one copies a folder
rem holding vf3.exe, its DLLs, the ROM and this script, and clicks on it. The script looks for the exe
rem beside itself first, then in build\bin; and for the ROM beside itself, then at the usual path.
setlocal
set SECONDS=%1
if "%SECONDS%"=="" set SECONDS=120

set EXE=
if exist "%~dp0vf3.exe" set EXE=%~dp0vf3.exe
if "%EXE%"=="" if exist "%~dp0..\..\build\bin\vf3.exe" set EXE=%~dp0..\..\build\bin\vf3.exe
if "%EXE%"=="" (
  echo vf3.exe not found, neither beside this script nor in build\bin.
  echo Build it ^(make build/bin/vf3^), or copy the exe and its DLLs beside this script.
  goto :done
)

set ROM=
if exist "%~dp0fvipers2.zip" set ROM=%~dp0fvipers2.zip
if "%ROM%"=="" if exist "C:\Users\frede\Documents\SEGA Model 3\ROMs\fvipers2.zip" set ROM=C:\Users\frede\Documents\SEGA Model 3\ROMs\fvipers2.zip
if "%ROM%"=="" (
  echo fvipers2.zip not found, neither beside this script nor in Documents\SEGA Model 3\ROMs.
  goto :done
)

rem The data folder, so that the log lands somewhere we know to read. Without it, it follows the zip and
rem then has to be looked for.
set VF3_DATA=%~dp0measure-host
if not exist "%VF3_DATA%" mkdir "%VF3_DATA%"

set VF3_HIDDEN=1
set VF3_SND_PROF=1
set VF3_SND_HOST=500
set VF3_QUIT_AFTER_S=%SECONDS%

echo ---- %SECONDS% seconds with no window, please wait ----
"%EXE%" "%ROM%"
echo.
echo ---- what the sound cost the host ----
findstr /b "sndhost" "%VF3_DATA%\log.txt"
echo.
echo ---- and the split inside the board ----
findstr /b "sndprof" "%VF3_DATA%\log.txt"
echo.
echo The whole log: "%VF3_DATA%\log.txt"
echo.
echo WHAT TO READ: the "the game waits" column. If it is at 0.000 the sound costs the frame rate
echo nothing. And the "period" must be at 17.38 ms once settled -- otherwise the machine is not keeping
echo the Model 3's pace and the rest of the line no longer means the same thing.
:done
endlocal
