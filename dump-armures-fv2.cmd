@echo off
rem Fighting Vipers 2: the second dump, the one of the models WITHOUT their
rem armour.
rem
rem   "dump-armures-fv2.cmd"       two colours per fighter
rem   "dump-armures-fv2.cmd" 4     four
rem
rem The first dump (dump-complet-fv2.cmd) held guard so that the armour would not
rem fall: it only saw the fighters clothed. Under the armour there is another
rem model and other textures -- cell 14's chrome had zero textures of its own only
rem because its whole body IS its armour.
rem
rem Player 1's `flag0` is at 0x00701444; 0x04 and 0x08 are "the piece has fallen",
rem and those are the bits the model reads (docs\fv2-honey.md). 15 = 0x0F: both
rem pieces at their limit and both fallen. The address given to VF3_POKE is an
rem index into --ram-at's dump, where the byte at address A is at index A^3: so
rem 701444 aims at the low byte of the 32-bit word. The address is in hexadecimal,
rem the value in decimal.
rem
rem The poke is HELD, because the model is chosen when a round loads and not
rem during it: it is round 2 that carries the result, and it alone is what the
rem scripts render.
rem
rem The textures are added to the first dump's, in the same folder: the dump never
rem writes over a file already there, so only the new ones arrive. Do not delete
rem Textures3D-dump before running this.
setlocal enabledelayedexpansion
set MINGW=C:\msys64\mingw64\bin
set PATH=%MINGW%;%PATH%
set ROM=C:\Users\frede\Documents\SEGA Model 3\ROMs\fvipers2.zip
cd /d "%~dp0"
set "VF3_DATA=%~dp0"
if "%VF3_DATA:~-1%"=="\" set "VF3_DATA=%VF3_DATA:~0,-1%"

set "COLOURS=%~1"
if "%COLOURS%"=="" set "COLOURS=2"

if not exist "build\bin\vf3run.exe" (
  echo build\bin\vf3run.exe is missing: run construire.cmd first.
  pause
  exit /b 1
)
if not exist "%ROM%" (
  echo ROM not found: %ROM%
  pause
  exit /b 1
)

py -3 -I "tools\fv2\mk_dump_armures.py" %COLOURS% || goto :failed

set "OUT=%~dp0Textures3D-dump"
mkdir "%OUT%\fvipers2" 2>nul
mkdir "%OUT%\ram" 2>nul

set "VF3_POKE=701444=15"
set "VF3_POKE_FRAMES=3200-7500"

set /a N=0
for %%S in ("tools\scripts\dump-armures\*.txt") do set /a N+=1
echo.
echo   %N% runs, the armour blown off, %COLOURS% colour(s) per fighter.
echo   Allow about %N% x 3 minutes.
echo.

set /a I=0
for %%S in ("tools\scripts\dump-armures\*.txt") do (
  set /a I+=1
  echo.
  echo --- !I!/%N%  %%~nS ---
  "build\bin\vf3run.exe" "%ROM%" --script "%%S" --hd-dump "%OUT%\fvipers2" --full-roster --ram-at 5200 --rec --quiet
  echo     code !ERRORLEVEL!
  if exist "ram_5200.bin" move /y "ram_5200.bin" "%OUT%\ram\%%~nS.ram" >nul
)

set "VF3_POKE="
set "VF3_POKE_FRAMES="

echo.
echo --- coverage ---
py -3 -I "tools\fv2\couverture_dump.py" "%OUT%" || goto :failed

echo.
echo   Textures  %OUT%\fvipers2
echo.
goto :done

:failed
echo.
echo   ---- failed (code %ERRORLEVEL%) ----
:done
if /i not "%1"=="-silence" if /i not "%2"=="-silence" pause
