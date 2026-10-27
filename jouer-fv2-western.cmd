@echo off
rem Fighting Vipers 2, premier combat a WESTERN, avec le journal des ombres.
rem
rem A quoi ca sert : reproduire un RING OUT sur une arene qui a du sol hors du
rem ring, pour savoir ce qui prive l'ejecte de son ombre. Le numero d'arene du
rem premier combat vient d'une table d'octets a 0x000B4100, dont la premiere
rem entree vaut 09 (le barrage) ; VF3_STAGE l'ecrit, et le jeu charge l'arene
rem entiere par son propre chemin -- fiche, lumiere, geometrie ET textures
rem (docs/fv2-ombres.md).
rem
rem CE QU'IL FAUT FAIRE : une partie, et mettre l'adversaire KO avec un coup
rem qui l'ejecte hors du ring.
rem
rem CE QUE J'IRAI LIRE ENSUITE, dans log.txt :
rem   decor N, nom "..."      le decor au moment de l'ejection. S'il devient
rem                           FINISH00..FINISH05, c'est le jeu qui coupe ses
rem                           ombres, pas nous.
rem   places : j1 [...] j2    ou sont les deux combattants, toutes les 30 images
rem   sous le sol : ...       de combien l'ejecte passe sous le plan du sol, et
rem                           combien de pieces le jeu aplatit encore
rem
rem   jouer-fv2-western.cmd            Western, journal, ombres de ce portage
rem   jouer-fv2-western.cmd 11         une autre arene (0..12 ; 11 = BUILDING)
setlocal
set PATH=C:\msys64\mingw64\bin;%PATH%
set ROM=%~dp0roms\fvipers2.zip
if not exist "%ROM%" set ROM=C:\Users\frede\Documents\SEGA Model 3\ROMs\fvipers2.zip
set "VF3_DATA=%~dp0"
if "%VF3_DATA:~-1%"=="\" set "VF3_DATA=%VF3_DATA:~0,-1%"
set VF3_SHADOWS=1
set VF3_SHADOW_WHY=1
if "%~1"=="" (set VF3_STAGE=1) else set VF3_STAGE=%~1
cd /d "%~dp0"
if not exist "build\bin\vf3.exe" (
  echo build\bin\vf3.exe manque : installer SDL 3 puis lancer construire-hote.cmd.
  pause
  exit /b 1
)
"build\bin\vf3.exe" "%ROM%"
