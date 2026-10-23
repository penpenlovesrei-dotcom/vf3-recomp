@echo off
rem Fighting Vipers 2, ARBRE ISOLE : le ring Dreamcast AJOUTE au decor TEST.
rem
rem CE QUE CA FAIT, en trois morceaux.
rem
rem   VF3_STAGE=0       patche le tireur de decor en 0x0007111C pour qu'il
rem                     rende 0, l'index de TEST. Le choix se fait une fois,
rem                     donc modeles, arene et camera suivent ensemble.
rem   VF3_SCENE=...     la scene convertie depuis la Dreamcast.
rem   VF3_SCENE_ADD=0   l'AJOUTER a la scene du jeu -- et seulement quand le
rem                     decor courant est 0. Le jeu garde ses combattants, ses
rem                     ombres et son interface ; on ne lui ajoute que ce qui
rem                     lui manque.
rem
rem Pourquoi ajouter plutot que remplacer : la borne porte tout TEST sauf le
rem creneau 3 de ses huit identifiants de ressource, celui de la geometrie
rem propre, qui vaut zero. Il ne manque que le ring.
rem
rem --pc : mode PC -- pas d'ecran d'amorce, acces libre, pas de menu console.
rem
rem LE DECOR N'APPARAIT QU'EN COMBAT : la demonstration d'attract ecrit son
rem decor en dur (0x000B3C4 pose 21, ADV_TOWER) sans passer par le tireur.
rem Appuie sur START et lance un combat.
setlocal
set PATH=C:\msys64\mingw64\bin;%PATH%
set ROM=C:\Users\frede\Documents\SEGA Model 3\ROMs\fvipers2.zip
set "VF3_DATA=%~dp0"
if "%VF3_DATA:~-1%"=="\" set "VF3_DATA=%VF3_DATA:~0,-1%"
set "VF3_SCENE=C:\Users\frede\Documents\fv2-arcade-notes\real3d\TEST"
set VF3_STAGE=0
set VF3_STAGE_LOG=1
set VF3_SCENE_ADD=0
set VF3_SCENE_LOG=1
cd /d "%~dp0"
if not exist "build\bin\vf3.exe" (
  echo build\bin\vf3.exe manque.
  pause
  exit /b 1
)
del "%VF3_SCENE%\injection.log" 2>nul
echo Decor force : index 0 = TEST.  Ring Dreamcast AJOUTE.
echo.
echo Mets la fenetre au premier plan, puis START et lance un combat.
echo.
"build\bin\vf3.exe" --pc "%ROM%" %*
echo.
echo ---- decor et injection ----
findstr /C:"decor" log.txt
if exist "%VF3_SCENE%\injection.log" type "%VF3_SCENE%\injection.log"
echo.
pause
