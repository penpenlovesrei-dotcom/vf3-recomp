@echo off
rem Runs Fighting Vipers 2 on the interpreter, without a window, and records the
rem state: the set recognised, the stepping, the PC, the RAM's footprint and what
rem the game asked the security chip for.
rem   lancer-fv2.cmd            -> 120 frames, and a dump of the RAM
rem   lancer-fv2.cmd 1800       -> 1800 frames, no dump
setlocal
set ROM=C:\Users\frede\Documents\SEGA Model 3\ROMs\fvipers2.zip
set FRAMES=%1
if "%FRAMES%"=="" set FRAMES=120
cd /d "%~dp0"
if not exist "build\bin\vf3run.exe" (
  echo build\bin\vf3run.exe is missing: run construire.cmd first.
  pause
  exit /b 1
)
if "%1"=="" (
  "build\bin\vf3run.exe" "%ROM%" --frames %FRAMES% --ram-at %FRAMES%
) else (
  "build\bin\vf3run.exe" "%ROM%" --frames %FRAMES%
)
echo.
echo ---- finished (code %ERRORLEVEL%) ----
pause
