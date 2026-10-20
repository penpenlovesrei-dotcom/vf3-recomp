@echo off
rem Fighting Vipers 2 avec les graphismes 2D remplaces : le plan de tuiles est
rem dessine a 4 fois sa resolution et les images du pack prennent la place des
rem tuiles et des couches dont elles portent le nom. Voir docs\fv2-hd-2d.md.
rem
rem Le pack attendu est assets-hd\pack-fvipers2, avec tiles\ et layers\ comme
rem le releve les ecrit (releve-tuiles-fv2.cmd). Toutes ses images doivent etre
rem a l'echelle 4 : une tuile fait 32x32, une couche 2048x2048 (la tuilemap
rem entiere) ou 1984x1536 (l'ecran visible). Une image a une autre taille est
rem refusee par son nom, et la ligne de chargement le dit.
rem
rem Sans pack, l'echelle seule n'ameliore rien : les tuiles d'origine sont
rem simplement repetees. Elle ne fait que creer la place ou l'art HD se pose.
rem
rem L'echelle et le pack sont retenus dans settings.ini (tile_scale, tile_pack).
rem Pour revenir au rendu d'origine : jouer-fv2.cmd avec --tile-scale 1
rem --no-tile-pack.
setlocal
set PATH=C:\msys64\mingw64\bin;%PATH%
set ROM=C:\Users\frede\Documents\SEGA Model 3\ROMs\fvipers2.zip
cd /d "%~dp0"
if not exist "build\bin\vf3.exe" (
  echo build\bin\vf3.exe manque : installer SDL 3 puis lancer construire.cmd.
  pause
  exit /b 1
)
if not exist "assets-hd\pack-fvipers2" (
  echo assets-hd\pack-fvipers2 n'existe pas : lancer releve-tuiles-fv2.cmd,
  echo puis fabriquer le pack a partir du releve ^(docs\fv2-hd-2d.md^).
  pause
  exit /b 1
)
"build\bin\vf3.exe" "%ROM%" --tile-scale 4 --tile-pack "%~dp0assets-hd\pack-fvipers2" %*
