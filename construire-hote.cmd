@echo off
rem Construit la fenetre (build\bin\vf3.exe) ET l'outil sans fenetre
rem (build\bin\vf3run.exe), avec le g++ de MSYS2.
rem construire.cmd ne batit que les outils sans fenetre ; celui-ci fait les deux,
rem parce que le menu des modes de Fighting Vipers 2 se dessine dans la fenetre.
rem make doit recevoir un dossier temporaire ou il peut ecrire, sinon le
rem compilateur tente C:\WINDOWS et s'arrete sur "Cannot create temporary file".
setlocal
set MINGW=C:\msys64\mingw64\bin
set MAKE=C:\msys64\usr\bin\make.exe
set PATH=%MINGW%;C:\msys64\usr\bin;%PATH%
set SCRATCH=%LOCALAPPDATA%\Temp
cd /d "%~dp0"
"%MAKE%" CXX=g++ TMP=%SCRATCH% TEMP=%SCRATCH% TMPDIR=%SCRATCH% build/bin/vf3run build/bin/vf3 -j8
echo.
echo ---- fini (code %ERRORLEVEL%) ----
pause
