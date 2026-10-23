@echo off
rem Fighting Vipers 2, ARBRE ISOLE : forcer le decor d'entrainement.
rem
rem CE QUE CA FAIT. VF3_STAGE=0 patche le TIREUR de decor en 0x0007111C pour
rem qu'il rende 0, l'index de TEST dans la table des 27 decors de 0x00075170.
rem Le choix est donc fait une fois, proprement : les modeles, l'arene et la
rem camera suivent ensemble. La version d'avant reposait l'octet 0x0058FD9F a
rem chaque image et desynchronisait tout.
rem
rem VF3_STAGE_LOG dit, sur la console, le decor que le jeu croit afficher --
rem lu dans la RAM, pas deduit de l'image.
rem
rem VF3_SCENE n'est PAS pose : la scene convertie n'est pas injectee. On voit
rem TEST tel que la ROM d'arcade le porte.
setlocal
set PATH=C:\msys64\mingw64\bin;%PATH%
set ROM=C:\Users\frede\Documents\SEGA Model 3\ROMs\fvipers2.zip
set "VF3_DATA=%~dp0"
if "%VF3_DATA:~-1%"=="\" set "VF3_DATA=%VF3_DATA:~0,-1%"
set VF3_STAGE=0
set VF3_STAGE_LOG=1
cd /d "%~dp0"
if not exist "build\bin\vf3.exe" (
  echo build\bin\vf3.exe manque.
  pause
  exit /b 1
)
echo Decor force : index 0 = TEST, par patch du tireur.
echo.
echo Mets la fenetre au premier plan : le port ne rend rien sans le focus.
echo Le decor courant s'ecrit dans log.txt a chaque changement.
echo.
"build\bin\vf3.exe" "%ROM%" %*
echo.
echo ---- les lignes de decor du journal ----
findstr /C:"decor" log.txt
echo.
pause
