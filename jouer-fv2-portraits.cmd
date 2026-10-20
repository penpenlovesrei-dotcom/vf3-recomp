@echo off
rem Fighting Vipers 2 avec les portraits de l'ecran de selection remplaces par
rem des PNG lus dans un dossier. Rien n'est ecrit dans la ROM ni dans le zip :
rem les deux planches sont reecrites dans l'image de la VROM que le processus
rem garde en memoire, une fois, avant que la carte demarre.
rem Voir docs\fv2-portraits.md.
rem
rem OU VONT LES FICHIERS. Ce portage range ce qui lui appartient -- reglages,
rem NVRAM, journal, portraits -- dans LE DOSSIER DU PROJET, celui ou se trouve
rem ce lanceur. Les quatre lanceurs epinglent le meme, sinon chacun aurait ses
rem reglages et sa NVRAM. Sans cet epinglage le portage choisit tout seul, et
rem son dernier recours est "a cote du zip" -- donc dans l'arbre Supermodel.
rem
rem LES PORTRAITS vont donc dans :
rem
rem     <ce dossier>\portraits-fvipers2\
rem         light\00.png ... 15.png    la planche claire : la case sous le curseur
rem         dark\00.png ... 15.png     la planche sombre : toutes les autres
rem
rem claire\ et sombre\ sont acceptes aussi, et un NN.png pose a la racine sert
rem pour les deux planches. Une case sans PNG garde celle de Sega. Le dossier
rem est cree au premier lancement, vide : il n'y a qu'a y deposer les images.
rem
rem Chaque image : exactement 64x64, PNG non entrelace (dans GIMP, decocher
rem l'entrelacement Adam7), 8 ou 16 bits.
rem
rem L'ordre des cases est celui de la PLANCHE, qui n'est ni celui du curseur ni
rem celui des identifiants de combat :
rem   00 BAHN  01 CHARLIE  02 GRACE  03 HONEY  04 JANE  05 MAHLER  06 PICKY
rem   07 RAXEL 08 SANMAN   09 EMI    10 TOKIO  11 a 15 libres (blanches)
rem
rem Pour essayer un autre dossier sans rien deplacer, le passer en argument :
rem   jouer-fv2-portraits.cmd "C:\mes portraits"
rem Pour revenir a ceux de Sega : jouer-fv2.cmd une fois avec --no-portraits.
setlocal
set PATH=C:\msys64\mingw64\bin;%PATH%
set ROM=C:\Users\frede\Documents\SEGA Model 3\ROMs\fvipers2.zip
set "VF3_DATA=%~dp0"
if "%VF3_DATA:~-1%"=="\" set "VF3_DATA=%VF3_DATA:~0,-1%"
cd /d "%~dp0"
if not exist "build\bin\vf3.exe" (
  echo build\bin\vf3.exe manque : installer SDL 3 puis lancer construire.cmd.
  pause
  exit /b 1
)
if not exist "%VF3_DATA%" mkdir "%VF3_DATA%"
set DOSSIER=%~1
if "%DOSSIER%"=="" (
  echo Portraits : %VF3_DATA%\portraits-fvipers2
  "build\bin\vf3.exe" "%ROM%"
) else (
  echo Portraits : %DOSSIER%
  "build\bin\vf3.exe" "%ROM%" --portraits "%DOSSIER%"
)
