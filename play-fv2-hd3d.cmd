@echo off
rem Fighting Vipers 2 with a pack of 3D textures: the PNGs in the folder
rem Textures3D\fvipers2 (beside this file) replace the game's textures whose
rem names they carry. See docs\fv2-hd-3d.md.
rem
rem The file's name IS the key, and it is the one the capture wrote
rem (dump-fv2-textures-3d.cmd): <hash>_<width>x<height>_f<format>.png. The size
rem in the name is the original texture's, and it only serves to recognise it;
rem the image itself may have whatever resolution it likes.
rem
rem The setting is kept in settings.ini (hd_3d = 1). To remove it: run
rem play-fv2.cmd once with --no-hd-3d.
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
if not exist "Textures3D\fvipers2" (
  echo The folder Textures3D\fvipers2 does not exist yet.
  echo Capture first with dump-fv2-textures-3d.cmd, then put the images in it.
  pause
)
"build\bin\vf3.exe" "%ROM%" --hd-3d %*
