@echo off
rem Fighting Vipers 2 : les deux routes du menu console, SANS FENETRE, sans les mains, et verifiees.
rem
rem Il joue deux parties cachees -- une en VERSUS, une en ARCADE -- et compare la suite des etats
rem traverses a celle qui est attendue. Il ne demande pas de lire une trace : il dit si elle est bonne.
rem A relancer apres tout changement de la couche console (console_state.h, console_play.h, main.cpp).
rem
rem   VERSUS : PLAY MENU PLAY STAGE PLAY PAUSE MENU_IN MENU
rem   ARCADE : PLAY MENU PLAY       PLAY PAUSE MENU_IN MENU  (sans le carrousel, qui est a la ligne V.S.)
rem   TRAINING : la meme suite qu-ARCADE, mais elle met en marche l-horloge arretee, les vies tenues,
rem              les sept etats d-annonce coupes, l-annonceur jete et la sortie d-un match a deux.
rem
rem Compter un quart d-heure : trois parties de sept mille images chacune, sans fenetre.
setlocal
set PATH=C:\msys64\mingw64\bin;%PATH%
set ROM=C:\Users\frede\Documents\SEGA Model 3\ROMs\fvipers2.zip
cd /d "%~dp0"
if not exist "build\bin\vf3.exe" (
  echo build\bin\vf3.exe manque : lancer construire-hote.cmd d'abord.
  pause
  exit /b 1
)
set VF3_HIDDEN=1
set VF3_CONSOLE_PAUSE=4
set VF3_QUIT_AFTER=7000
rem ET UNE SORTIE A LA MONTRE AUSSI, parce que celle-la se compte en IMAGES DU JEU : la route finit sur
rem MENU, qui fige le PowerPC, donc les images restantes ne tournent jamais et la partie ne se termine
rem pas d'elle-meme. Mesure du 2026-10-27 : la route versus etait finie et juste a l'heure, et le
rem processus tournait encore vingt-six minutes plus tard. Trois minutes suffisent aux deux routes.
set VF3_QUIT_AFTER_S=180

echo ---- la route VERSUS ----
set "VF3_DATA=%~dp0build\console-route"
if not exist "%VF3_DATA%" mkdir "%VF3_DATA%"
set VF3_SCRIPT=%~dp0tools\scripts\fv2-console-route.txt
set VF3_CONSOLE_MODE=versus
set VF3_CONSOLE_STAGE=5
"build\bin\vf3.exe" "%ROM%" --console
py -3 "%~dp0tools\fv2\verifier_console.py" "%VF3_DATA%\log.txt" PLAY MENU PLAY STAGE PLAY PAUSE MENU_IN MENU
set VERSUS=%ERRORLEVEL%

echo.
echo ---- la route ARCADE ----
set "VF3_DATA=%~dp0build\console-route-arcade"
if not exist "%VF3_DATA%" mkdir "%VF3_DATA%"
set VF3_SCRIPT=%~dp0tools\scripts\fv2-console-route-arcade.txt
set VF3_CONSOLE_MODE=arcade
set VF3_CONSOLE_STAGE=
"build\bin\vf3.exe" "%ROM%" --console
py -3 "%~dp0tools\fv2\verifier_console.py" "%VF3_DATA%\log.txt" PLAY MENU PLAY PAUSE MENU_IN MENU
set ARCADE=%ERRORLEVEL%

echo.
echo ---- la route TRAINING ----
set "VF3_DATA=%~dp0build\console-route-training"
if not exist "%VF3_DATA%" mkdir "%VF3_DATA%"
set VF3_SCRIPT=%~dp0tools\scripts\fv2-console-route-training.txt
set VF3_CONSOLE_MODE=training
"build\bin\vf3.exe" "%ROM%" --console
py -3 "%~dp0tools\fv2\verifier_console.py" "%VF3_DATA%\log.txt" PLAY MENU PLAY PAUSE MENU_IN MENU
set TRAINING=%ERRORLEVEL%
rem et les trois lignes que cette route doit dire en plus (fv2-console-route-training.txt)
findstr /C:"is held, the game having started" /C:"the announcer dropped" "%VF3_DATA%\log.txt"

echo.
if "%VERSUS%%ARCADE%%TRAINING%"=="000" (echo ---- les trois routes sont bonnes ----) else (echo ---- VERSUS=%VERSUS% ARCADE=%ARCADE% TRAINING=%TRAINING% : zero veut dire bon ----)
echo Les journaux entiers sont dans build\console-route, -arcade et -training.
pause
