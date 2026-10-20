@echo off
rem Fighting Vipers 2 : releve de tout ce que le generateur de tuiles dessine.
rem Entierement passif -- rien a presser. On joue, on traverse les ecrans que
rem l'on veut refaire, et chaque tuile et chaque disposition de couche est
rem ecrite une fois, dedupliquee par son contenu. Voir docs\fv2-hd-2d.md.
rem
rem Sortie dans assets-hd\dump-fvipers2 :
rem   tiles\<hachage>_p<palette>_<4^|8>bpp.png  une image 8x8 par tuile distincte
rem   tiles.csv                                leur inventaire
rem   layers\L<n>_<hachage>.png                la tuilemap 512x512 d'une disposition
rem   layers\L<n>_<hachage>.txt                sa grille 64x64, ses registres, sa legende
rem   layers\index.csv                         la table des matieres, les ecrans charges en tete
rem
rem Commencer par layers\index.csv. Le nom de fichier EST la cle : on reprend
rem un fichier, on le redessine plus grand, on le remet sous le meme nom dans
rem un dossier de pack.
rem
rem Le roster complet est mis pour que les ecrans de Del Sol, Kuhn, BM et Kuma
rem passent aussi (docs\fv2-roster.md).
setlocal
set PATH=C:\msys64\mingw64\bin;%PATH%
set ROM=C:\Users\frede\Documents\SEGA Model 3\ROMs\fvipers2.zip
cd /d "%~dp0"
if not exist "build\bin\vf3.exe" (
  echo build\bin\vf3.exe manque : installer SDL 3 puis lancer construire.cmd.
  pause
  exit /b 1
)
if not exist "assets-hd" mkdir "assets-hd"
"build\bin\vf3.exe" "%ROM%" --tile-dump "%~dp0assets-hd\dump-fvipers2" --full-roster %*
echo.
echo ---- releve ecrit dans %~dp0assets-hd\dump-fvipers2 ----
pause
