@echo off
rem Fighting Vipers 2 avec un pack de textures 3D : les PNG du dossier
rem Textures3D\fvipers2 (a cote de ce fichier) remplacent les textures du jeu
rem qu'ils portent le nom. Voir docs\fv2-hd-3d.md.
rem
rem Le nom du fichier EST la cle, et c'est celui que la capture a ecrit
rem (dumper-textures-3d-fv2.cmd) : <hachage>_<largeur>x<hauteur>_f<format>.png.
rem La taille dans le nom est celle de la texture d'origine, elle ne sert qu'a
rem la reconnaitre ; l'image, elle, peut avoir la resolution qu'elle veut.
rem
rem Le reglage est retenu dans settings.ini (hd_3d = 1). Pour l'enlever :
rem lancer jouer-fv2.cmd une fois avec --no-hd-3d.
setlocal
set PATH=C:\msys64\mingw64\bin;%PATH%
set ROM=C:\Users\frede\Documents\SEGA Model 3\ROMs\fvipers2.zip
cd /d "%~dp0"
set "VF3_DATA=%~dp0"
if "%VF3_DATA:~-1%"=="\" set "VF3_DATA=%VF3_DATA:~0,-1%"
if not exist "build\bin\vf3.exe" (
  echo build\bin\vf3.exe manque : installer SDL 3 puis lancer construire.cmd.
  pause
  exit /b 1
)
if not exist "Textures3D\fvipers2" (
  echo Le dossier Textures3D\fvipers2 n'existe pas encore.
  echo Capturer d'abord avec dumper-textures-3d-fv2.cmd, puis y poser les images.
  pause
)
"build\bin\vf3.exe" "%ROM%" --hd-3d %*
