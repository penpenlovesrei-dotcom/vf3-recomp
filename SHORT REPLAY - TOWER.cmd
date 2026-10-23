@echo off
rem Fighting Vipers 2, ARBRE ISOLE : le SHORT REPLAY sur TOWER -- un decor
rem d'origine complet, SANS la moindre injection. C'est le temoin.
rem
rem A QUOI CA SERT. Si les angles successifs sortent ici, la chaine est bonne et
rem il ne reste qu'a les retrouver sur TEST avec le ring. Si elles n'y sortent
rem pas, c'est la chaine qu'il faut reprendre, pas l'injection.
rem
rem Les trois modes sont 7 S_REP_INIT, 8 S_REPLAY, 9 SR2GAME_INIT -- noms de
rem Sega, en clair dans le binaire. VF3_SREP neutralise, en 0x0001EBE8, le test
rem du bit 0x00800000 que le jeu exige dans le mot de proprietes du coup, donc
rem n'importe quel coup tenu dix images arme le replay.
rem
rem --pc : mode PC -- pas d'ecran d'amorce, acces libre, pas de menu console.
setlocal
set PATH=C:\msys64\mingw64\bin;%PATH%
set ROM=C:\Users\frede\Documents\SEGA Model 3\ROMs\fvipers2.zip
set "VF3_DATA=%~dp0"
if "%VF3_DATA:~-1%"=="\" set "VF3_DATA=%VF3_DATA:~0,-1%"
set VF3_STAGE=10
set VF3_SREP=1
cd /d "%~dp0"
if not exist "build\bin\vf3.exe" (
  echo build\bin\vf3.exe manque. Lance construire-hote.cmd.
  pause
  exit /b 1
)
del log.txt 2>nul
echo Decor force : index 10 = TOWER, d'origine, sans injection.
echo Tout coup tenu dix images arme le short replay.
echo.
echo START, lance un combat, et frappe. Pas besoin de KO.
echo.
"build\bin\vf3.exe" --pc "%ROM%" %*
echo.
findstr /C:"VF3_SREP" log.txt
echo.
pause
