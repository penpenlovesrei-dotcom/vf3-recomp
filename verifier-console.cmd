@echo off
rem Fighting Vipers 2 : les deux routes du menu console, SANS FENETRE, sans les mains, et verifiees.
rem
rem Il joue deux parties cachees -- une en VERSUS, une en ARCADE -- et compare la suite des etats
rem traverses a celle qui est attendue. Il ne demande pas de lire une trace : il dit si elle est bonne.
rem A relancer apres tout changement de la couche console (console_state.h, console_play.h, main.cpp).
rem
rem   VERSUS : PLAY MENU PLAY STAGE PLAY PAUSE MENU_IN MENU
rem   ARCADE : PLAY MENU PLAY       PLAY PAUSE MENU_IN MENU  (sans le carrousel, qui est a la ligne V.S.)
rem
rem Compter une dizaine de minutes : deux parties de sept mille images chacune, sans fenetre.
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
if "%VERSUS%%ARCADE%"=="00" (echo ---- les deux routes sont bonnes ----) else (echo ---- VERSUS=%VERSUS% ARCADE=%ARCADE% : zero veut dire bon ----)
echo Les journaux entiers sont dans build\console-route et build\console-route-arcade.
pause
