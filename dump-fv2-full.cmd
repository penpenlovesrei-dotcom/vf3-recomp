@echo off
rem Fighting Vipers 2: a capture of every 3D texture the game can show without
rem anybody playing it -- the attract, then one fight per cursor cell, in both
rem costume colours. Thirty-one windowless runs, with --full-roster to reach
rem cells 11 to 14.
rem
rem   Textures3D-dump\fvipers2\        the PNGs and their textures.csv
rem   Textures3D-dump\ram\             the state read at frame 3300 of each run
rem   Textures3D-dump\coverage.txt     who was played in each run
rem
rem ALLOW AN HOUR. Nothing to watch, nothing to press.
rem
rem The folder must be EMPTY for the alpha correction to serve: the dump never
rem writes over a file already there, so a PNG from a capture made before
rem 2026-10-21 would survive with its wrong alpha. This launcher puts a previous
rem capture aside instead of deleting it.
rem
rem A picture's alpha is that of the polygon that drew it, not the texel's: see
rem docs\fv2-hd-3d.md section 4.
setlocal enabledelayedexpansion
set MINGW=C:\msys64\mingw64\bin
set PATH=%MINGW%;%PATH%
set ROM=C:\Users\frede\Documents\SEGA Model 3\ROMs\fvipers2.zip
cd /d "%~dp0"
set "VF3_DATA=%~dp0"
if "%VF3_DATA:~-1%"=="\" set "VF3_DATA=%VF3_DATA:~0,-1%"

if not exist "build\bin\vf3run.exe" (
  echo build\bin\vf3run.exe is missing: run build-tools.cmd first.
  pause
  exit /b 1
)
if not exist "%ROM%" (
  echo ROM not found: %ROM%
  pause
  exit /b 1
)

py -3 -I "tools\fv2\mk_dump_full.py" || goto :failed

set "OUT=%~dp0Textures3D-dump"
if exist "%OUT%\fvipers2\textures.csv" (
  for /f %%q in ('py -3 -I -c "import time;print(time.strftime(chr(37)+chr(89)+chr(37)+chr(109)+chr(37)+chr(100)+chr(45)+chr(37)+chr(72)+chr(37)+chr(77)))"') do set "WHEN=%%q"
  echo   A capture is already there: put aside as Textures3D-dump-!WHEN!
  move /y "%OUT%" "%~dp0Textures3D-dump-!WHEN!" >nul
)
mkdir "%OUT%\fvipers2" 2>nul
mkdir "%OUT%\ram" 2>nul

set /a N=0
for %%S in ("tools\scripts\dump-full\*.txt") do (
  set /a N+=1
  echo.
  echo --- !N!/31  %%~nS ---
  "build\bin\vf3run.exe" "%ROM%" --script "%%S" --hd-dump "%OUT%\fvipers2" --full-roster --ram-at 3300 --rec --quiet
  echo     code !ERRORLEVEL!
  if exist "ram_3300.bin" move /y "ram_3300.bin" "%OUT%\ram\%%~nS.ram" >nul
)

echo.
echo --- coverage ---
py -3 -I "tools\fv2\dump_coverage.py" "%OUT%" || goto :failed

echo.
echo   Textures  %OUT%\fvipers2
echo.
goto :done

:failed
echo.
echo   ---- failed (code %ERRORLEVEL%) ----
:done
if /i not "%1"=="-silence" pause
