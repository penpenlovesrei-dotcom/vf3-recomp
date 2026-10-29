@echo off
rem The sound's differential harness: has the SAME scene played by this branch's vf3run and by a
rem reference's (by default the HEAD commit from before the changes, in a worktree of its own), then
rem compares the two WAVs byte by byte. A sound that moves by one sample is a regression to be
rem explained, not a tolerance to be widened.
rem
rem   tools\sound\compare-sound.cmd <game> <script> <frames> [ref-commit]
rem
rem   game    : fvipers2 or vf3 (the zip's name in Documents\SEGA Model 3\ROMs)
rem   script  : a file from tools\scripts
rem   frames  : how many frames to play
rem   ref     : the reference commit (default: HEAD)
rem
rem Examples:
rem   tools\sound\compare-sound.cmd fvipers2 tools\scripts\fv2-fight.txt 4200
rem   tools\sound\compare-sound.cmd vf3      tools\scripts\vf3-match.txt 4200
rem
rem Both runs write into %TEMP%\sound-cmp\<game>\, each in its own folder: the same place every
rem time, so ONE comparator at a time (verify-vf3.cmd's lesson).
setlocal
set MINGW=C:\msys64\mingw64\bin
set MAKE=C:\msys64\usr\bin\make.exe
set PATH=%MINGW%;C:\msys64\usr\bin;%PATH%
set SCRATCH=%LOCALAPPDATA%\Temp
set GAME=%1
set SCRIPT=%2
set FRAMES=%3
set REFC=%4
if "%GAME%"=="" set GAME=fvipers2
if "%SCRIPT%"=="" set SCRIPT=tools\scripts\fv2-fight.txt
if "%FRAMES%"=="" set FRAMES=4200
if "%REFC%"=="" set REFC=HEAD
set ROM=C:\Users\frede\Documents\SEGA Model 3\ROMs\%GAME%.zip
set REF=C:\Temp\fv2-sound-ref
set OUT=%TEMP%\sound-cmp\%GAME%
cd /d "%~dp0..\.."

if not exist "%ROM%" (
  echo The ROM %ROM% is not there.
  goto :done
)
if not exist "build\bin\vf3run.exe" (
  echo build\bin\vf3run.exe is missing: run build-tools.cmd first.
  goto :done
)

if not exist "%REF%\Makefile" (
  echo ---- the reference tree in %REF%, on %REFC% ----
  git worktree add --detach "%REF%" %REFC% || goto :done
)
if not exist "%REF%\build\bin\vf3run.exe" (
  echo ---- building the reference ----
  pushd "%REF%"
  "%MAKE%" CXX=g++ TMP=%SCRATCH% TEMP=%SCRATCH% TMPDIR=%SCRATCH% build/bin/vf3run -j8 || (popd & goto :done)
  popd
)

if not exist "%OUT%\ref" mkdir "%OUT%\ref" 2> nul
if not exist "%OUT%\mine" mkdir "%OUT%\mine" 2> nul

echo ---- %FRAMES% frames, the reference ----
pushd "%OUT%\ref"
set VF3_PROF=1
"%REF%\build\bin\vf3run.exe" "%ROM%" --script "%~dp0..\..\%SCRIPT%" --frames %FRAMES% --wav sound.wav --quiet
popd
echo ---- %FRAMES% frames, this branch ----
pushd "%OUT%\mine"
"%~dp0..\..\build\bin\vf3run.exe" "%ROM%" --script "%~dp0..\..\%SCRIPT%" --frames %FRAMES% --wav sound.wav --quiet
popd
set VF3_PROF=

echo.
echo ---- the sound, byte by byte ----
fc /b "%OUT%\ref\sound.wav" "%OUT%\mine\sound.wav" > nul && echo   IDENTICAL || echo   *** DIFFERENT ***
:done
endlocal
