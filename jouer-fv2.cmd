@echo off
rem Fighting Vipers 2 dans la fenetre, avec le son et les manettes.
rem Il faut SDL 3 : pacman -S mingw-w64-x86_64-sdl3 dans MSYS2, puis construire.cmd.
rem Les reglages et la NVRAM de ce jeu sont a part de ceux de Virtua Fighter 3.
setlocal
set PATH=C:\msys64\mingw64\bin;%PATH%
set ROM=C:\Users\frede\Documents\SEGA Model 3\ROMs\fvipers2.zip
cd /d "%~dp0"
if not exist "build\bin\vf3.exe" (
  echo build\bin\vf3.exe manque : installer SDL 3 puis lancer construire.cmd.
  pause
  exit /b 1
)
"build\bin\vf3.exe" "%ROM%" %*
