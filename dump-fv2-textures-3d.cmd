@echo off
rem Fighting Vipers 2: a passive capture of the 3D engine's textures. Nothing to
rem press: one goes through the screens, and every texture a polygon samples is
rem written once, as a PNG, named by the hash of its content.
rem See docs\fv2-hd-3d.md.
rem
rem Output: Textures3D-dump\fvipers2 beside this file, with a textures.csv that
rem says where each texture was found (it is added to from one session to the
rem next, it is not rewritten).
rem
rem What is not displayed is not captured: to have a fight's textures one has to
rem get as far as the fight. Plain-colour textures are never written: their hash
rem is that of any plain-colour texture of the same size.
setlocal
set PATH=C:\msys64\mingw64\bin;%PATH%
set ROM=C:\Users\frede\Documents\SEGA Model 3\ROMs\fvipers2.zip
cd /d "%~dp0"
set "VF3_DATA=%~dp0"
if "%VF3_DATA:~-1%"=="\" set "VF3_DATA=%VF3_DATA:~0,-1%"
if not exist "build\bin\vf3.exe" (
  echo build\bin\vf3.exe is missing: install SDL 3 then run build-tools.cmd.
  pause
  exit /b 1
)
"build\bin\vf3.exe" "%ROM%" --hd-3d-dump %*
