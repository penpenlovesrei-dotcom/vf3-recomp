@echo off
rem Virtua Fighter 3 in the window, to LISTEN to the MADRS mirror one way and then the other.
rem
rem   jouer-vf3-madrs.cmd mirror     the port's CURRENT setting for VF3
rem   jouer-vf3-madrs.cmd separate   the two ranges kept apart (what MAME does since #16527)
rem
rem Why this launcher exists (docs/fv2-son-programme.md section 5): measured on 2026-10-24, the sound
rem driver writes 32 zeros into 0x7C0-0x7FF AFTER loading MADRS. With the mirror those zeros overwrite
rem MADRS, so 176 delay-line addresses on VF3 fall to zero and every line of the DSP reads the same
rem buffer. The setting had been kept for VF3 on the reverberation's LENGTH alone -- and that length
rem came from the collapse.
rem
rem What this launcher does NOT say: which of the two is the cabinet's sound. There is no Model 3 here.
rem It is a decision to be made by listening, not a correction.
setlocal
set MODE=%1
if "%MODE%"=="" (
  echo Usage: jouer-vf3-madrs.cmd mirror ^| separate
  pause
  exit /b 1
)
set PATH=C:\msys64\mingw64\bin;%PATH%
set ROM=C:\Users\frede\Documents\SEGA Model 3\ROMs\vf3.zip
set "VF3_DATA=%~dp0"
if "%VF3_DATA:~-1%"=="\" set "VF3_DATA=%VF3_DATA:~0,-1%"
cd /d "%~dp0"

if /i "%MODE%"=="mirror" (
  set VF3_SCSP_MADRS_MIRROR=1
  set VF3_SCSP_MADRS_SEPARATE=
  echo ---- MIRROR: the port's current setting for VF3 ----
) else if /i "%MODE%"=="separate" (
  set VF3_SCSP_MADRS_MIRROR=
  set VF3_SCSP_MADRS_SEPARATE=1
  echo ---- SEPARATE: the two ranges kept apart ----
) else (
  echo Unknown mode: %MODE%. Expected "mirror" or "separate".
  pause
  exit /b 1
)

if not exist "build\bin\vf3.exe" (
  echo build\bin\vf3.exe is missing: install SDL 3 then run construire-hote.cmd.
  pause
  exit /b 1
)
if not exist "%ROM%" (
  echo The ROM %ROM% is not there.
  pause
  exit /b 1
)
"build\bin\vf3.exe" "%ROM%" %2 %3 %4 %5
endlocal
