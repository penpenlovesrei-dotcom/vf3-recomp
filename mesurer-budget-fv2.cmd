@echo off
rem Fighting Vipers 2 : ce que coute une image aujourd'hui, avant de parler
rem d'ajouter quoi que ce soit. Question 3 de docs\fv2-ombres.md.
rem
rem --pc : la voie PC, celle ou START VA DROIT AU JEU. Sans elle, ce lanceur
rem herite du settings.ini (console_mode = 1), et dans la voie console Start est
rem le bouton de la console : il ouvre la liste des modes et n'atteint jamais le
rem jeu. C'est la liste, pas la mesure, qu'on verrait alors.
rem
rem Le jeu se joue normalement, en free play. La seule autre difference : une
rem ligne "pace:" toutes les DEUX secondes au lieu de dix, dans log.txt a cote de
rem ce fichier. Le journal est ecrase a chaque lancement.
rem
rem CE QU'IL FAUT FAIRE, et rien d'autre :
rem   1. laisser l'ecran titre et une demonstration tourner    ~15 secondes
rem   2. Start (touche 1, ou Start a la manette), choisir un
rem      combattant avec le POING, et jouer un round           ~30 secondes
rem   3. quitter.
rem Quinze secondes par endroit au moins : la ligne ne sort que toutes les deux
rem secondes, et le gouverneur ne juge qu'apres trente images.
rem
rem CE QUI SERA LU DANS log.txt :
rem   "a picture X ms"            le cout d'une image, tout compris
rem   "(the GPU's set-up / the CPU's 3D ..., uploads ...)"  lequel des deux
rem                               chemins dessine, et ce que coutent les envois
rem   "a picture's 3D: ..."       l'entre-deux, les triangles prepares, et le
rem                               temps passe a attendre la carte
rem   "NxN (w x h)"               l'echelle de rendu REELLE a ce moment
rem   toute ligne "governor:"     le gouverneur a bouge : le budget est deja
rem                               depense, et c'est le fait le plus important
rem
rem Un budget de reference : le jeu tourne a 57,524 images par seconde, donc une
rem image du jeu vaut 17,38 ms. Avec le mouvement lisse a 120 images par seconde
rem l'image affichee en vaut 8,33, moins la part de l'emulation.
setlocal
set PATH=C:\msys64\mingw64\bin;%PATH%
rem La ROM : celle de ce dossier si elle y est (roms\, hors depot), sinon celle du
rem dossier ou les autres lanceurs vont la chercher.
set "ROM=%~dp0roms\fvipers2.zip"
if not exist "%ROM%" set "ROM=C:\Users\frede\Documents\SEGA Model 3\ROMs\fvipers2.zip"
if not exist "%ROM%" (
  echo La ROM est introuvable, ni dans roms\ ni dans Documents\SEGA Model 3\ROMs.
  pause
  exit /b 1
)
set "VF3_DATA=%~dp0"
if "%VF3_DATA:~-1%"=="\" set "VF3_DATA=%VF3_DATA:~0,-1%"
cd /d "%~dp0"
if not exist "build\bin\vf3.exe" (
  echo build\bin\vf3.exe manque : lancer construire-hote.cmd d'abord.
  pause
  exit /b 1
)
set VF3_PACE_LOG=1
"build\bin\vf3.exe" "%ROM%" --pc %*
echo.
echo ---- fini. Les lignes "pace:" et "governor:" sont dans log.txt. ----
pause
