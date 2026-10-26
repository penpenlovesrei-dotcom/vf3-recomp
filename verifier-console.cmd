@echo off
rem Fighting Vipers 2, le chemin complet du menu console, SANS FENETRE et sans les mains.
rem
rem Il joue tools\scripts\fv2-console-route.txt et imprime la trace des etats. Rien a regarder, rien a
rem cliquer : la partie tourne cachee et le journal dit ou elle est passee. A relancer apres tout
rem changement de la couche console (src\host\console_state.h, console_play.h, main.cpp).
rem
rem Ce que la trace DOIT dire, dans cet ordre (docs\fv2-console.md) :
rem
rem   PLAY -> MENU      Start sur l'ecran titre
rem   MENU -> PLAY      V.S. choisi
rem   PLAY -> STAGE     les combattants choisis, le carrousel s'ouvre
rem   STAGE -> PLAY     le decor repondu
rem   PLAY -> PAUSE     Start dans le combat
rem   PAUSE -> MENU_IN  MAIN MENU : le jeu rentre derriere la liste
rem   MENU_IN -> MENU   le jeu est rentre
rem
rem Une ligne qui manque ou qui arrive dans le desordre est une regression, pas un detail.
setlocal
set PATH=C:\msys64\mingw64\bin;%PATH%
set ROM=C:\Users\frede\Documents\SEGA Model 3\ROMs\fvipers2.zip
set "VF3_DATA=%~dp0build\console-route"
cd /d "%~dp0"
if not exist "build\bin\vf3.exe" (
  echo build\bin\vf3.exe manque : lancer construire-hote.cmd d'abord.
  pause
  exit /b 1
)
if not exist "%VF3_DATA%" mkdir "%VF3_DATA%"
set VF3_HIDDEN=1
set VF3_SCRIPT=%~dp0tools\scripts\fv2-console-route.txt
set VF3_CONSOLE_MODE=versus
set VF3_CONSOLE_STAGE=5
set VF3_CONSOLE_PAUSE=4
set VF3_QUIT_AFTER=7000
"build\bin\vf3.exe" "%ROM%" --console
echo.
echo ---- la trace des etats ----
findstr /C:"the console: " "%VF3_DATA%\log.txt"
echo.
echo ---- fini. Le journal entier est dans %VF3_DATA%\log.txt ----
pause
