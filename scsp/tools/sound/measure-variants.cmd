@echo off
rem measure-variants.cmd : counts the INSTRUCTIONS RETIRED by each DSP variant, on the hardware counter.
rem
rem WHY NOT A STOPWATCH. scsp/docs/scsp_audit.md S4 octies and S4 nonies tell of the two times the clock
rem lied: `mame`'s baseline drifted from 2.70 to 3.21 ns during one session of trials, and an ordering bias
rem nearly had a gain of 6.9 per cent kept that did not exist. The counter of instructions retired does not
rem drift with the temperature and does not depend on the order of the measurements. One capture per
rem variant, one hard figure.
rem
rem IT NEEDS AN ADMINISTRATOR WINDOW: xperf cannot program the counters otherwise, and it says so
rem ("Failed to configure counters"). Nothing else is required -- the exes are already built and already
rem verified by the ten tests: python tools\sound\build-variants.py makes them.
rem
rem   Win+X, "Terminal (administrator)", then:
rem     cd /d C:\Users\frede\Desktop\fv2-recomp\scsp
rem     tools\sound\measure-variants.cmd game       (or `baseline`, which is the default)
rem
rem This file is pure ASCII: cmd reads accents in its ANSI code page and breaks the line splitting, which
rem gives incomprehensible "'m' is not recognized" errors.
setlocal enabledelayedexpansion
set XPERF=C:\Program Files (x86)\Windows Kits\10\Windows Performance Toolkit\xperf.exe
set VAR=%~dp0..\..\build\bin\variants
set OUT=C:\Temp\instr
rem THE MODE: `baseline` (default) or `game`. The baseline is a program whose 112 steps all do the same
rem thing: it locates a cost. The program of the game's shape is what counts, and on 2026-10-28 a
rem combination of variants won 29 per cent on the first and nothing on the second.
set MODE=%1
if "%MODE%"=="" set MODE=baseline
if "%MODE%"=="baseline" (set SAMPLES=2000000) else (set SAMPLES=600000)
if not "%MODE%"=="baseline" if not "%MODE%"=="game" (
  echo Unknown mode: %MODE%. The two are `baseline` and `game`.
  goto :done
)
cd /d "%~dp0..\.."
rem MinGW's DLLS ARE IN build\bin AND THE EXES IN build\bin\variants: without this line, Windows finds
rem neither libstdc++-6.dll nor libgcc_s_seh-1.dll and puts up one box per variant. An administrator
rem window does not have MSYS2 in its PATH, and it has no business having it.
set PATH=%~dp0..\..\build\bin;%PATH%

if not exist "%XPERF%" (
  echo The Windows Performance Toolkit is not there: "%XPERF%"
  goto :done
)
if not exist "%VAR%" (
  echo The variants' exes are missing: %VAR%
  echo Make them: python tools\sound\build-variants.py
  goto :done
)
if not exist "%OUT%" mkdir "%OUT%" 2> nul

rem A DEFENSIVE STOP FIRST: if a previous capture was interrupted, the kernel tracer is still running and
rem the next -on would fail on "already started". Harmless the rest of the time.
"%XPERF%" -stop > nul 2>&1

rem Is the counter programmable? An honest trial before the loop, so as not to discover on the third
rem round that this is not an administrator window.
"%XPERF%" -on PROC_THREAD+LOADER+PMC_PROFILE -PmcProfile InstructionRetired 2> nul
if errorlevel 1 (
  echo.
  echo xperf could not program the counters. This window is probably not an administrator
  echo one: Win+X, "Terminal ^(administrator^)", then start again.
  goto :done
)
"%XPERF%" -stop > nul 2>&1

for %%E in ("%VAR%\mc-*.exe") do (
  set NAME=%%~nE
  echo.
  echo ---- !NAME! ----
  "%XPERF%" -on PROC_THREAD+LOADER+PMC_PROFILE -PmcProfile InstructionRetired
  "%%~fE" --%MODE% own %SAMPLES%
  "%XPERF%" -stop -d "%OUT%\!NAME!-%MODE%.etl" > nul
  echo     trace: %OUT%\!NAME!-%MODE%.etl
)

echo.
echo ---- finished ----
echo The traces are in %OUT%. The analysis needs no elevation.
:done
echo.
pause
