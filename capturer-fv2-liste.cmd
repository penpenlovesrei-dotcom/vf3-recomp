@echo off
rem Les trois images du menu des modes de Fighting Vipers 2, prises sans fenetre
rem a l'ecran et sans une main sur la manette. Elles arrivent dans build\ :
rem
rem   build\liste-1-arcade.png          la liste sur le vrai ecran titre du jeu
rem   build\liste-2-survival.png        trois fois bas, puis Entree : la partie
rem                                     demarre, et l'ecran de selection dit
rem                                     SURVIVAL MODE
rem   build\liste-3-retour-au-titre.png Start pendant la demonstration : le jeu
rem                                     revient a son ecran titre
rem
rem La liste gele le jeu derriere elle, donc rien de ce qui compte les images du
rem jeu n'arrive plus : ces essais se terminent a l'horloge (VF3_QUIT_AFTER_S) et
rem la liste se pilote a l'horloge aussi (VF3_KEYS_S). Voir docs\fv2-console.md.
rem
rem Compter une minute par image. Les secondes ci-dessous ont de la marge : la
rem liste s'ouvre vers la 31e seconde sur cette machine, et ce qui est gele ne
rem bouge plus, donc une capture plus tard est la meme capture.
setlocal
set PATH=C:\msys64\mingw64\bin;%PATH%
set ROM=C:\Users\frede\Documents\SEGA Model 3\ROMs\fvipers2.zip
set "VF3_DATA=%~dp0"
if "%VF3_DATA:~-1%"=="\" set "VF3_DATA=%VF3_DATA:~0,-1%"
cd /d "%~dp0"
if not exist "build\bin\vf3.exe" (
  echo build\bin\vf3.exe manque : lancer construire-hote.cmd d'abord.
  pause
  exit /b 1
)
set VF3_HIDDEN=1
set VF3_WINDOW=1440x1080
set VF3_3D=cpu

echo ---- 1 sur 3 : la liste sur l'ecran titre ----
set VF3_SCRIPT=%~dp0tools\scripts\fv2-console-liste.txt
set VF3_QUIT_AFTER=999999
set VF3_QUIT_AFTER_S=45
set VF3_KEYS_S=
"build\bin\vf3.exe" "%ROM%" --console
copy /y build\host_window.png build\liste-1-arcade.png > nul

echo ---- 2 sur 3 : SURVIVAL choisi dans la liste ----
set VF3_QUIT_AFTER_S=58
set VF3_KEYS_S=Down@40,Down@41,Down@42,Return@44
"build\bin\vf3.exe" "%ROM%" --console
copy /y build\host_window.png build\liste-2-survival.png > nul

echo ---- 3 sur 3 : Start pendant la demonstration ----
set VF3_SCRIPT=%~dp0tools\scripts\fv2-console-titre.txt
set VF3_QUIT_AFTER=1530
set VF3_QUIT_AFTER_S=
set VF3_KEYS_S=
"build\bin\vf3.exe" "%ROM%" --console
copy /y build\host_window.png build\liste-3-retour-au-titre.png > nul

echo.
echo ---- les trois images ----
dir /b build\liste-*.png
echo.
echo ---- fini (code %ERRORLEVEL%) ----
pause
