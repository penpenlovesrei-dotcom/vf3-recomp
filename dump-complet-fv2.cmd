@echo off
rem Fighting Vipers 2 : capture de toutes les textures 3D que le jeu sait
rem montrer sans qu'on y joue -- l'attraction, puis un combat par case du
rem curseur, dans les deux couleurs de costume. Trente et une courses sans
rem fenetre, avec --full-roster pour atteindre les cases 11 a 14.
rem
rem   Textures3D-dump\fvipers2\   les PNG et leur textures.csv
rem   Textures3D-dump\ram\        l'etat lu a l'image 3300 de chaque course
rem   Textures3D-dump\couverture.txt   qui a ete joue dans chaque course
rem
rem COMPTEZ UNE HEURE. Rien a surveiller, rien a presser.
rem
rem Le dossier doit etre VIERGE pour que la correction de l'alpha serve : le
rem dump n'ecrit jamais par-dessus un fichier deja la, donc un PNG d'une
rem capture d'avant le 2026-10-21 survivrait avec son alpha faux. Ce lanceur
rem met de cote une capture precedente au lieu de l'effacer.
rem
rem L'alpha d'une image est celui du polygone qui l'a dessinee, pas celui du
rem texel : voir docs\fv2-hd-3d.md section 4.
setlocal enabledelayedexpansion
set MINGW=C:\msys64\mingw64\bin
set PATH=%MINGW%;%PATH%
set ROM=C:\Users\frede\Documents\SEGA Model 3\ROMs\fvipers2.zip
cd /d "%~dp0"
set "VF3_DATA=%~dp0"
if "%VF3_DATA:~-1%"=="\" set "VF3_DATA=%VF3_DATA:~0,-1%"

if not exist "build\bin\vf3run.exe" (
  echo build\bin\vf3run.exe manque : lancer construire.cmd d'abord.
  pause
  exit /b 1
)
if not exist "%ROM%" (
  echo ROM introuvable : %ROM%
  pause
  exit /b 1
)

py -3 -I "tools\fv2\mk_dump_complet.py" || goto :rate

set "SORTIE=%~dp0Textures3D-dump"
if exist "%SORTIE%\fvipers2\textures.csv" (
  for /f %%q in ('py -3 -I -c "import time;print(time.strftime(chr(37)+chr(89)+chr(37)+chr(109)+chr(37)+chr(100)+chr(45)+chr(37)+chr(72)+chr(37)+chr(77)))"') do set "QUAND=%%q"
  echo   Une capture est deja la : mise de cote sous Textures3D-dump-!QUAND!
  move /y "%SORTIE%" "%~dp0Textures3D-dump-!QUAND!" >nul
)
mkdir "%SORTIE%\fvipers2" 2>nul
mkdir "%SORTIE%\ram" 2>nul

set /a N=0
for %%S in ("tools\scripts\dump-complet\*.txt") do (
  set /a N+=1
  echo.
  echo --- !N!/31  %%~nS ---
  "build\bin\vf3run.exe" "%ROM%" --script "%%S" --hd-dump "%SORTIE%\fvipers2" --full-roster --ram-at 3300 --rec --quiet
  echo     code !ERRORLEVEL!
  if exist "ram_3300.bin" move /y "ram_3300.bin" "%SORTIE%\ram\%%~nS.ram" >nul
)

echo.
echo --- couverture ---
py -3 -I "tools\fv2\couverture_dump.py" "%SORTIE%" || goto :rate

echo.
echo   Textures  %SORTIE%\fvipers2
echo.
goto :fin

:rate
echo.
echo   ---- echec (code %ERRORLEVEL%) ----
:fin
if /i not "%1"=="-silence" pause
