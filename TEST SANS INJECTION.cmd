@echo off
rem Fighting Vipers 2, ARBRE ISOLE : le decor TEST force, SANS notre geometrie.
rem
rem A QUOI CA SERT. Le jeu se fige pendant la destruction du ring. Ce lanceur
rem dit si ce gel vient de nous ou de la borne : VF3_STAGE=0 force le decor,
rem mais VF3_SCENE n'est pas pose, donc rien n'est injecte.
rem
rem   si ca gele quand meme  -> c'est la borne, sur un decor dont la geometrie
rem                             est vide (le creneau 3 de TEST vaut zero)
rem   si ca ne gele pas      -> c'est notre injection, et on cherche chez nous
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
echo Mode PC.  Decor force : index 0 = TEST.  AUCUNE injection.
echo.
echo Lance un combat, puis va jusqu'a la destruction du ring.
echo.
"build\bin\vf3.exe" --pc "%ROM%" %*
echo.
findstr /C:"decor" log.txt
echo.
pause
