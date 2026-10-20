@echo off
rem Fighting Vipers 2 avec les portraits de l'ecran de selection remplaces par
rem des PNG lus dans un dossier. Rien n'est ecrit dans la ROM ni dans le zip :
rem les deux planches sont reecrites dans l'image de la VROM que le processus
rem garde en memoire, une fois, avant que la carte demarre.
rem Voir docs\fv2-portraits.md.
rem
rem Le dossier par defaut est assets\fv2-portraits-test : seize cases de
rem couleur unie numerotees, faites par tools\fv2\portraits_test.py, pour voir
rem d'un coup d'oeil quelle case va ou. Pour les siennes, passer un autre
rem dossier en premier argument :
rem
rem   jouer-fv2-portraits.cmd "C:\mes portraits"
rem
rem Le dossier contient light\00.png ... 15.png (la planche claire, celle du
rem personnage sous le curseur) et dark\00.png ... 15.png (la sombre, les
rem autres). claire\ et sombre\ sont acceptes aussi, et un NN.png pose a la
rem racine sert pour les deux planches. Une case sans PNG garde celle de Sega.
rem
rem L'ordre des cases est celui de la planche, qui n'est ni celui du curseur ni
rem celui des identifiants de combat :
rem   00 BAHN  01 CHARLIE  02 GRACE  03 HONEY  04 JANE  05 MAHLER  06 PICKY
rem   07 RAXEL 08 SANMAN   09 EMI    10 TOKIO  11 a 15 libres (blanches)
rem
rem Le dossier est retenu dans settings.ini (portraits_folder). Pour revenir
rem aux portraits de Sega : lancer jouer-fv2.cmd une fois avec --no-portraits.
setlocal
set PATH=C:\msys64\mingw64\bin;%PATH%
set ROM=C:\Users\frede\Documents\SEGA Model 3\ROMs\fvipers2.zip
cd /d "%~dp0"
rem Premier argument : le dossier de portraits. S'il commence par -- c'est une
rem option pour vf3.exe, pas un dossier, et on garde celui d'essai.
set DOSSIER=%~1
set SUITE=%*
if "%DOSSIER%"=="" set DOSSIER=%~dp0assets\fv2-portraits-test
if "%DOSSIER:~0,2%"=="--" (set DOSSIER=%~dp0assets\fv2-portraits-test) else (set SUITE=)
if not exist "build\bin\vf3.exe" (
  echo build\bin\vf3.exe manque : installer SDL 3 puis lancer construire.cmd.
  pause
  exit /b 1
)
if not exist "%DOSSIER%" (
  echo Le dossier de portraits est introuvable : %DOSSIER%
  echo Pour fabriquer celui d'essai : py -3 tools\fv2\portraits_test.py assets\fv2-portraits-test
  pause
  exit /b 1
)
echo Portraits lus dans : %DOSSIER%
"build\bin\vf3.exe" "%ROM%" --portraits "%DOSSIER%" %SUITE%
