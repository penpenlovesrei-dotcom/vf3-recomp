@echo off
rem Construit les outils sans fenetre (vf3run, vf3lock) avec le g++ de MSYS2.
rem make doit recevoir un dossier temporaire ou il peut ecrire, sinon le
rem compilateur tente C:\WINDOWS et s'arrete sur "Cannot create temporary file".
setlocal
set MINGW=C:\msys64\mingw64\bin
set MAKE=C:\msys64\usr\bin\make.exe
set PATH=%MINGW%;C:\msys64\usr\bin;%PATH%
set SCRATCH=%LOCALAPPDATA%\Temp
cd /d "%~dp0"
"%MAKE%" CXX=g++ TMP=%SCRATCH% TEMP=%SCRATCH% TMPDIR=%SCRATCH% build/bin/vf3run build/bin/vf3lock -j8
echo.
echo ---- fini (code %ERRORLEVEL%) ----
pause
