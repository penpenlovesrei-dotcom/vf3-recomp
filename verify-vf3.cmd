@echo off
rem Virtua Fighter 3's non-regression: compares this branch with the original
rem version (the "main" commit), on the same revision D ROM. It must come out
rem IDENTICAL everywhere, or a change to Step 2.0 has touched 1.0.
rem First run: it creates the original worktree and builds it.
setlocal
set MINGW=C:\msys64\mingw64\bin
set MAKE=C:\msys64\usr\bin\make.exe
set PATH=%MINGW%;C:\msys64\usr\bin;%PATH%
set SCRATCH=%LOCALAPPDATA%\Temp
set ROM=C:\Users\frede\Documents\SEGA Model 3\ROMs\vf3.zip
set UP=C:\Temp\vf3up
set CMP=C:\Temp\cmp
set FRAMES=%1
if "%FRAMES%"=="" set FRAMES=3000
cd /d "%~dp0"

rem ONE RUN AT A TIME. Both runs write their states and their dumps into %CMP%, always in the same
rem place: two verifiers launched together mix their files up and the comparison comes out DIFFERENT
rem with no regression existing at all. That happened on 2026-10-23, one run at 2000 frames and one at
rem 3000: the s.state compared was 3000 frames on one side and 2000 on the other. The lock costs one
rem line; a false alarm on the fidelity contract costs far more.
if not exist "%CMP%" mkdir "%CMP%" 2> nul
if exist "%CMP%\running" (
  echo Another verifier is already running ^(%CMP%\running^).
  echo If it was interrupted, delete that file and start again.
  goto :done
)
echo %DATE% %TIME% > "%CMP%\running"

if not exist "%UP%\Makefile" (
  echo ---- creating the original worktree in %UP% ----
  git worktree add "%UP%" main || goto :done
)
if not exist "%UP%\build\bin\vf3run.exe" (
  echo ---- building the original version ----
  pushd "%UP%"
  "%MAKE%" CXX=g++ TMP=%SCRATCH% TEMP=%SCRATCH% TMPDIR=%SCRATCH% build/bin/vf3run -j8
  popd
)
if not exist "build\bin\vf3run.exe" (
  echo build\bin\vf3run.exe is missing: run build-tools.cmd first.
  goto :done
)

if not exist "%CMP%\up" mkdir "%CMP%\up"
if not exist "%CMP%\fv2" mkdir "%CMP%\fv2"
echo ---- %FRAMES% frames, the original version ----
pushd "%CMP%\up"
"%UP%\build\bin\vf3run.exe" "%ROM%" --frames %FRAMES% --state s.state --ram-at %FRAMES% --quiet
popd
echo ---- %FRAMES% frames, this branch ----
pushd "%CMP%\fv2"
"%~dp0build\bin\vf3run.exe" "%ROM%" --frames %FRAMES% --state s.state --ram-at %FRAMES% --quiet
popd

echo.
echo ---- the PowerPC's state, frame by frame ----
fc /b "%CMP%\up\s.state" "%CMP%\fv2\s.state" > nul && echo   IDENTICAL || echo   *** DIFFERENT ***
echo ---- the 8 MB of RAM ----
fc /b "%CMP%\up\ram_%FRAMES%.bin" "%CMP%\fv2\ram_%FRAMES%.bin" > nul && echo   IDENTICAL || echo   *** DIFFERENT ***

echo.
echo ---- frames rendered (vf3run --shots) ----
if exist "%CMP%\shup" rmdir /s /q "%CMP%\shup"
if exist "%CMP%\shnew" rmdir /s /q "%CMP%\shnew"
mkdir "%CMP%\shup" & mkdir "%CMP%\shnew"
"%UP%\build\bin\vf3run.exe" "%ROM%" --script "%~dp0tools\scripts\vf3-shots.txt" --shots "%CMP%\shup" --quiet > nul
"%~dp0build\bin\vf3run.exe" "%ROM%" --script "%~dp0tools\scripts\vf3-shots.txt" --shots "%CMP%\shnew" --quiet > nul
set SHOTS_OK=1
if not exist "%CMP%\shup\*.png" set SHOTS_OK=0
for %%F in ("%CMP%\shup\*.png") do fc /b "%%F" "%CMP%\shnew\%%~nxF" > nul || set SHOTS_OK=0
if "%SHOTS_OK%"=="1" (echo   IDENTICAL) else (echo   *** DIFFERENT ***)

rem The complete host: the game's picture as the window shows it. It is reproducible
rem (host_last.png), unlike host_window.png, which depends on the real pace.
rem With no SDL 3 installed, the target build/bin/vf3 does not exist and the trial is skipped.
if not exist "build\bin\vf3.exe" goto :nohost
if not exist "%UP%\build\bin\vf3.exe" (
  echo ---- building the original host ----
  pushd "%UP%"
  "%MAKE%" CXX=g++ TMP=%SCRATCH% TEMP=%SCRATCH% TMPDIR=%SCRATCH% build/bin/vf3 -j8
  popd
)
if not exist "%UP%\build\bin\vf3.exe" goto :nohost
echo.
echo ---- the complete host, the game's picture ----
if exist "%CMP%\hup" rmdir /s /q "%CMP%\hup"
if exist "%CMP%\hnew" rmdir /s /q "%CMP%\hnew"
mkdir "%CMP%\hup" & mkdir "%CMP%\hnew"
> "%CMP%\hup\settings.ini" echo settings_version = 3
>> "%CMP%\hup\settings.ini" echo picture_shape = 3
copy /y "%CMP%\hup\settings.ini" "%CMP%\hnew\settings.ini" > nul
set VF3_HIDDEN=1
set VF3_QUIT_AFTER=1500
set VF3_WINDOW=1280x720
set VF3_3D=cpu
set VF3_SCRIPT=%~dp0tools\scripts\vf3-match.txt
pushd "%UP%"
set VF3_DATA=%CMP%\hup
del /q build\host_last.png 2> nul
"%UP%\build\bin\vf3.exe" "%ROM%" > nul 2>&1
copy /y build\host_last.png "%CMP%\hup\last.png" > nul
popd
set VF3_DATA=%CMP%\hnew
del /q build\host_last.png 2> nul
"%~dp0build\bin\vf3.exe" "%ROM%" > nul 2>&1
copy /y build\host_last.png "%CMP%\hnew\last.png" > nul
fc /b "%CMP%\hup\last.png" "%CMP%\hnew\last.png" > nul && echo   IDENTICAL || echo   *** DIFFERENT ***
set VF3_HIDDEN=
set VF3_QUIT_AFTER=
set VF3_SCRIPT=
set VF3_DATA=
set VF3_3D=
:nohost

echo.
echo ---- step by step, interpreter against recompiled code (vf3lock) ----
"build\bin\vf3lock.exe" "%ROM%" --script tools\scripts\vf3-match.txt

:done
if exist "%CMP%\running" del "%CMP%\running"
echo.
echo ---- finished (code %ERRORLEVEL%) ----
pause
