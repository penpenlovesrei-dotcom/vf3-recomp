@echo off
rem Fighting Vipers 2 : capture passive des textures du moteur 3D. Rien a
rem presser : on traverse les ecrans, et chaque texture qu'un polygone
rem echantillonne est ecrite une fois, en PNG, nommee par le hachage de son
rem contenu. Voir docs\fv2-hd-3d.md.
rem
rem Sortie : Textures3D-dump\fvipers2 a cote de ce fichier, avec un
rem textures.csv qui dit ou chaque texture a ete trouvee (il s'ajoute d'une
rem seance a l'autre, il ne se reecrit pas).
rem
rem Ce qui n'est pas affiche n'est pas capture : pour avoir les textures d'un
rem combat il faut aller jusqu'au combat. Les textures unies ne sont jamais
rem ecrites : leur hachage est celui de toute texture unie de la meme taille.
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
"build\bin\vf3.exe" "%ROM%" --hd-3d-dump %*
