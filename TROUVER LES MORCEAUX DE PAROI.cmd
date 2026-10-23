@echo off
rem Fighting Vipers 2, ARBRE ISOLE : trouver OU vivent les trente-deux morceaux
rem d'une paroi qui casse.
rem
rem POURQUOI IL FAUT JOUER. La borne aligne les morceaux dans un bloc dont la
rem base est r12 -- le contexte d'une tache atteinte par pointeur depuis la
rem table 0x001150CC. Ce contexte est alloue a l'execution : il n'est pas dans
rem l'image, et r12 vaut zero a la frontiere d'image. On ne peut donc pas le
rem lire statiquement.
rem
rem Le port le cherche donc par la FORME des morceaux, des que le masque des
rem murs cesse d'etre nul : huit entrees de suite dont le x descend d'un pas
rem constant de 2*demi_cote/32 et dont le |z| vaut le demi-cote. Mais le masque
rem ne se pose que sur une VRAIE projection -- 26 000 images jouees au hasard
rem ne l'ont jamais pose.
rem
rem A FAIRE : lancer un combat sur TOWER et PROJETER l'adversaire contre un mur
rem assez fort pour le casser. Une fois suffit.
rem
rem Le journal ecrira alors :
rem    morceaux de paroi : base XXXXXXXX  pas N  demi-cote N
rem et les quatre premieres entrees.
setlocal
set PATH=C:\msys64\mingw64\bin;%PATH%
set ROM=C:\Users\frede\Documents\SEGA Model 3\ROMs\fvipers2.zip
set "VF3_DATA=%~dp0"
if "%VF3_DATA:~-1%"=="\" set "VF3_DATA=%VF3_DATA:~0,-1%"
set VF3_STAGE=10
set VF3_STAGE_LOG=1
cd /d "%~dp0"
if not exist "build\bin\vf3.exe" (
  echo build\bin\vf3.exe manque.
  pause
  exit /b 1
)
echo Decor force : index 10 = TOWER.  Aucune injection.
echo.
echo Lance un combat et PROJETTE l'adversaire contre un mur, assez fort pour
echo le casser. Une fois suffit, puis ferme le jeu.
echo.
"build\bin\vf3.exe" --pc "%ROM%" %*
echo.
echo ---- ce qu'on cherchait ----
findstr /C:"morceaux de paroi" /C:"murs :" log.txt
echo.
pause
