@echo off
rem Fighting Vipers 2: puts on the desktop every texture of Honey's basic
rem costume, in both of its colours.
rem
rem   Desktop\Honey-costume-de-base\couleur-1\    HONEY confirmed with punch
rem   Desktop\Honey-costume-de-base\couleur-2\    HONEY, one UP before the punch
rem   Desktop\Honey-costume-de-base\RELEVE.txt    how the sorting was done
rem
rem Three windowless runs, identical in everything but one thing each: Honey with
rem punch; Honey with one press of UP on her cell before the punch, which is what
rem changes the costume's colour (*1P COLOR 0x007005B9 goes from 0 to 1); and EMI
rem with punch as the control. The 3D engine's capture writes everything a polygon
rem samples -- the scenery, the sky, the opponent, the effects come with it. What
rem Honey's run has and the control does not is Honey. See docs\fv2-hd-3d.md.
rem
rem Nothing is deleted: each pass writes its captures into a timestamped folder of
rem its own, and the sorting refuses to overwrite a file already on the desktop.
rem
rem Allow some twenty minutes: three runs of 4500 frames, 167 frames rendered in
rem each.
setlocal
set PATH=C:\msys64\mingw64\bin;%PATH%
set ROM=C:\Users\frede\Documents\SEGA Model 3\ROMs\fvipers2.zip
cd /d "%~dp0"
set "VF3_DATA=%~dp0"
if "%VF3_DATA:~-1%"=="\" set "VF3_DATA=%VF3_DATA:~0,-1%"

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

rem The input scripts are generated, so that the three runs stay strictly
rem comparable.
py -3 -I "tools\fv2\mk_honey_tex_scripts.py" || goto :failed

rem One capture folder per pass: the previous one is never deleted, and a fresh
rem capture is indispensable, the sorting being a matter of sets. The timestamp
rem comes from Python and not from %DATE%, whose shape depends on Windows's
rem language.
for /f %%q in ('py -3 -I -c "import time;print(time.strftime(chr(37)+chr(89)+chr(37)+chr(109)+chr(37)+chr(100)+chr(45)+chr(37)+chr(72)+chr(37)+chr(77)))"') do set "WHEN=%%q"
if "%WHEN%"=="" set "WHEN=capture"
set "CAPT=%~dp0captures-textures-honey\%WHEN%"
mkdir "%CAPT%\couleur1" 2>nul
mkdir "%CAPT%\couleur2" 2>nul
mkdir "%CAPT%\temoin"   2>nul

echo.
echo --- 1/3  HONEY, the first colour (punch) ---
"build\bin\vf3run.exe" "%ROM%" --script "tools\scripts\fv2-honey-tex-c1.txt" --hd-dump "%CAPT%\couleur1" --rec --quiet
echo     code %ERRORLEVEL%

echo.
echo --- 2/3  HONEY, the second colour (UP then punch) ---
"build\bin\vf3run.exe" "%ROM%" --script "tools\scripts\fv2-honey-tex-c2.txt" --hd-dump "%CAPT%\couleur2" --rec --quiet
echo     code %ERRORLEVEL%

echo.
echo --- 3/3  EMI, the control (punch) ---
"build\bin\vf3run.exe" "%ROM%" --script "tools\scripts\fv2-emi-tex.txt" --hd-dump "%CAPT%\temoin" --rec --quiet
echo     code %ERRORLEVEL%

echo.
echo --- sorting ---
set "OUT=%USERPROFILE%\Desktop\Honey-costume-de-base"
py -3 -I "tools\fv2\honey_textures.py" "%CAPT%" "%OUT%" || goto :failed

echo.
echo   Raw captures    %CAPT%
echo   On the desktop  %OUT%
echo.
goto :done

:failed
echo.
echo   ---- failed (code %ERRORLEVEL%) ----
:done
if /i not "%1"=="-silence" pause
