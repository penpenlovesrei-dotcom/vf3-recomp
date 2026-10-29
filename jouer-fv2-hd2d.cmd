@echo off
rem Fighting Vipers 2 with the 2D graphics replaced: the tile plane is drawn at
rem 4 times its resolution and the pack's images take the place of the tiles and
rem the layers whose names they carry. See docs\fv2-hd-2d.md.
rem
rem The pack expected is assets-hd\pack-fvipers2, with tiles\ and layers\ as the
rem survey writes them (releve-tuiles-fv2.cmd). All of its images must be at
rem scale 4: a tile is 32x32, a layer 2048x2048 (the whole tilemap) or 1984x1536
rem (the visible screen). An image at any other size is refused by its name, and
rem the loading line says so.
rem
rem With no pack, the scale alone improves nothing: the original tiles are simply
rem repeated. All it does is make the room where the HD art goes.
rem
rem The scale and the pack are kept in settings.ini (tile_scale, tile_pack). To
rem go back to the original rendering: jouer-fv2.cmd with --tile-scale 1
rem --no-tile-pack.
setlocal
set PATH=C:\msys64\mingw64\bin;%PATH%
set ROM=C:\Users\frede\Documents\SEGA Model 3\ROMs\fvipers2.zip
cd /d "%~dp0"
if not exist "build\bin\vf3.exe" (
  echo build\bin\vf3.exe is missing: install SDL 3 then run construire.cmd.
  pause
  exit /b 1
)
if not exist "assets-hd\pack-fvipers2" (
  echo assets-hd\pack-fvipers2 does not exist: run releve-tuiles-fv2.cmd,
  echo then make the pack from the survey ^(docs\fv2-hd-2d.md^).
  pause
  exit /b 1
)
"build\bin\vf3.exe" "%ROM%" --tile-scale 4 --tile-pack "%~dp0assets-hd\pack-fvipers2" %*
