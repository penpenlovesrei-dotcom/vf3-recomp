@echo off
rem replay.cmd : takes a trace of the SCSP's accesses over a scene, and replays it.
rem
rem Point 2 of step 2 of scsp/docs/scsp_migration_plan.md. Two checks in one command:
rem   1. does the replay find the PCM the board produced again? (is the trace complete)
rem   2. do two cores render the same thing? (the comparison, once there are two)
rem
rem   tools\sound\replay.cmd [game] [script] [frames] [core-a] [core-b]
rem
rem With no arguments: fvipers2, fv2-fight.txt, 300 frames, mame against mame.
rem Nothing is played on screen: vf3run has no interface.
rem
rem This file is pure ASCII: cmd reads accents in its ANSI code page and breaks the line splitting,
rem which gives incomprehensible "'m' is not recognized" errors.
setlocal
set GAME=%1
set SCRIPT=%2
set FRAMES=%3
set COREA=%4
set COREB=%5
if "%GAME%"=="" set GAME=fvipers2
if "%SCRIPT%"=="" set SCRIPT=tools\scripts\fv2-fight.txt
if "%FRAMES%"=="" set FRAMES=300
if "%COREA%"=="" set COREA=mame
if "%COREB%"=="" set COREB=mame
set ROM=C:\Users\frede\Documents\SEGA Model 3\ROMs\%GAME%.zip
set OUT=%TEMP%\sound-replay
cd /d "%~dp0..\.."

if not exist "%ROM%" (
  echo The ROM %ROM% is not there.
  goto :done
)
if not exist "build\bin\vf3run.exe" (
  echo build\bin\vf3run.exe is missing.
  goto :done
)
if not exist "build\bin\replay.exe" (
  echo build\bin\replay.exe is missing: make build/bin/replay
  goto :done
)

if not exist "%OUT%" mkdir "%OUT%"
del /q "%OUT%\tr.*" 2>nul

echo ---- the trace, %FRAMES% frames of %SCRIPT% ----
set VF3_SCSP_TRACE=%OUT%\tr
"build\bin\vf3run.exe" "%ROM%" --script "%SCRIPT%" --frames %FRAMES% --wav "%OUT%\sound.wav" --quiet
set VF3_SCSP_TRACE=
if errorlevel 1 (
  echo vf3run failed.
  goto :done
)

set FAULTS=0
for %%P in (0 1) do (
  echo.
  echo ---- chip %%P : is the trace complete? ----
  "build\bin\replay.exe" "%OUT%\tr.%%P" --against "%OUT%\tr.%%P.pcm"
  if errorlevel 1 set /a FAULTS+=1
  echo.
  echo ---- chip %%P : %COREA% against %COREB% ----
  "build\bin\replay.exe" "%OUT%\tr.%%P" %COREA% %COREB%
  if errorlevel 1 set /a FAULTS+=1
)

echo.
if "%FAULTS%"=="0" (
  echo EVERYTHING AGREES. The traces are in %OUT%.
) else (
  echo %FAULTS% check^(s^) failed. The traces are in %OUT%.
)

:done
endlocal
