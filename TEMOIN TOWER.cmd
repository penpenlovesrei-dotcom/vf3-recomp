@echo off
rem Fighting Vipers 2, ARBRE ISOLE : le TEMOIN, sur un decor d'origine.
rem
rem A QUOI CA SERT. Sur TEST, le masque des murs lu en [[0x00075124] + 0xC0]
rem reste a zero pendant tout un combat : la routine qui installe les parois
rem (0x00027D2C) n'est donc jamais appelee. Mais il faut verifier que ce masque
rem bouge AILLEURS, sinon c'est l'adresse qui est fausse.
rem
rem   le masque change sur TOWER  -> TEST est bien depourvu, et c'est la qu'il
rem                                  faut agir
rem   il ne change pas non plus   -> mon adresse est fausse, et il faut
rem                                  chercher le vrai etat des murs
rem
rem Aucune injection : on observe le jeu tel quel.
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
echo Mode PC.  Decor force : index 10 = TOWER.  Aucune injection.
echo.
echo Lance un combat et pousse l'adversaire contre un mur.
echo.
"build\bin\vf3.exe" --pc "%ROM%" %*
echo.
findstr /C:"murs :" log.txt
echo.
pause
