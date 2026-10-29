@echo off
rem The two sounds of the SAME scene, with and without the MADRS mirror, WITHOUT A WINDOW, then the
rem measurement.
rem
rem   tools\sound\compare-madrs.cmd [game] [script] [frames]
rem     default: vf3  tools\scripts\vf3-match.txt  2400
rem
rem Why: the mirror folds the DSP's delay lines onto one address (measured on 2026-10-24,
rem docs/fv2-sound-program.md section 5: the driver writes 32 zeros into 0x7C0-0x7FF AFTER loading
rem MADRS, which overwrites 176 addresses on Virtua Fighter 3). The setting was kept for VF3 on the
rem reverberation's LENGTH alone; this gives the figures on both sides.
rem
rem It opens no window: vf3run has none. The WAVs stay in build\ to be listened to.
setlocal
set MINGW=C:\msys64\mingw64\bin
set PATH=%MINGW%;C:\msys64\usr\bin;%PATH%
set GAME=%1
set SCRIPT=%2
set FRAMES=%3
if "%GAME%"=="" set GAME=vf3
if "%SCRIPT%"=="" set SCRIPT=tools\scripts\vf3-match.txt
if "%FRAMES%"=="" set FRAMES=2400
set ROM=C:\Users\frede\Documents\SEGA Model 3\ROMs\%GAME%.zip
cd /d "%~dp0..\.."

if not exist "build\bin\vf3run.exe" (
  echo build\bin\vf3run.exe is missing: run build-tools.cmd first.
  goto :done
)
if not exist "%ROM%" (
  echo The ROM %ROM% is not there.
  goto :done
)

echo ---- %FRAMES% frames, the CURRENT setting (the mirror, for VF3) ----
set VF3_SCSP_MADRS_SEPARATE=
set VF3_SCSP_MADRS_MIRROR=1
build\bin\vf3run.exe "%ROM%" --script "%SCRIPT%" --frames %FRAMES% --wav "build\sound-%GAME%-mirror.wav" --quiet

echo ---- %FRAMES% frames, the two ranges SEPARATE ----
set VF3_SCSP_MADRS_MIRROR=
set VF3_SCSP_MADRS_SEPARATE=1
build\bin\vf3run.exe "%ROM%" --script "%SCRIPT%" --frames %FRAMES% --wav "build\sound-%GAME%-separate.wav" --quiet
set VF3_SCSP_MADRS_SEPARATE=

echo.
py -3 tools\sound\measure-madrs.py "build\sound-%GAME%-mirror.wav" "build\sound-%GAME%-separate.wav" --names "mirror,separate" --scene %GAME%
echo.
echo ---- both WAVs are in build\ : sound-%GAME%-mirror.wav and sound-%GAME%-separate.wav ----
:done
endlocal
