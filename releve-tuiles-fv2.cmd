@echo off
rem Fighting Vipers 2: a survey of everything the tile generator draws. Entirely
rem passive -- nothing to press. One plays, one goes through the screens one wants
rem to redo, and every tile and every layer arrangement is written once,
rem deduplicated by its content. See docs\fv2-hd-2d.md.
rem
rem Output in assets-hd\dump-fvipers2:
rem   tiles\<hash>_p<palette>_<4^|8>bpp.png    one 8x8 image per distinct tile
rem   tiles.csv                                their inventory
rem   layers\L<n>_<hash>.png                   an arrangement's 512x512 tilemap
rem   layers\L<n>_<hash>.txt                   its 64x64 grid, its registers, its legend
rem   layers\index.csv                         the table of contents, the loaded screens first
rem
rem Start with layers\index.csv. The file's name IS the key: one takes a file,
rem redraws it larger, and puts it back under the same name in a pack folder.
rem
rem The full roster is set so that Del Sol's, Kuhn's, BM's and Kuma's screens go
rem through as well (docs\fv2-roster.md).
setlocal
set PATH=C:\msys64\mingw64\bin;%PATH%
set ROM=C:\Users\frede\Documents\SEGA Model 3\ROMs\fvipers2.zip
cd /d "%~dp0"
if not exist "build\bin\vf3.exe" (
  echo build\bin\vf3.exe is missing: install SDL 3 then run construire.cmd.
  pause
  exit /b 1
)
if not exist "assets-hd" mkdir "assets-hd"
"build\bin\vf3.exe" "%ROM%" --tile-dump "%~dp0assets-hd\dump-fvipers2" --full-roster %*
echo.
echo ---- the survey written into %~dp0assets-hd\dump-fvipers2 ----
pause
