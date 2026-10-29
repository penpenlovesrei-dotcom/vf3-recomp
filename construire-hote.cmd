@echo off
rem Builds the window (build\bin\vf3.exe) AND the windowless tool
rem (build\bin\vf3run.exe), with MSYS2's g++.
rem construire.cmd builds only the windowless tools; this one does both, because
rem Fighting Vipers 2's list of modes is drawn in the window.
rem make must be given a temporary folder it can write to, or the compiler tries
rem C:\WINDOWS and stops on "Cannot create temporary file".
setlocal
set MINGW=C:\msys64\mingw64\bin
set MAKE=C:\msys64\usr\bin\make.exe
set PATH=%MINGW%;C:\msys64\usr\bin;%PATH%
set SCRATCH=%LOCALAPPDATA%\Temp
cd /d "%~dp0"
"%MAKE%" CXX=g++ TMP=%SCRATCH% TEMP=%SCRATCH% TMPDIR=%SCRATCH% build/bin/vf3run build/bin/vf3 -j8
echo.
echo ---- finished (code %ERRORLEVEL%) ----
pause
