@echo off
rem Fighting Vipers 2 : le second dump, celui des modeles SANS leur armure.
rem
rem   "dump-armures-fv2.cmd"       deux couleurs par combattant
rem   "dump-armures-fv2.cmd" 4     quatre
rem
rem Le premier dump (dump-complet-fv2.cmd) tenait la garde pour que l'armure ne
rem tombe pas : il n'a vu que les combattants habilles. Sous l'armure il y a un
rem autre modele et d'autres textures -- le chrome de la case 14 n'avait zero
rem texture propre que parce que tout son corps EST son armure.
rem
rem `flag0` du joueur 1 est a 0x00701444 ; 0x04 et 0x08 sont « la piece est
rem tombee », et ce sont ces bits-la que le modele lit (docs\fv2-honey.md).
rem 15 = 0x0F : les deux pieces a leur limite et les deux tombees. L'adresse
rem donnee a VF3_POKE est un indice dans le vidage de --ram-at, ou l'octet
rem d'adresse A est a l'indice A^3 : 701444 vise donc l'octet de poids faible
rem du mot de 32 bits. L'adresse est en hexadecimal, la valeur en decimal.
rem
rem Le poke est TENU, parce que le modele est choisi quand un round se charge
rem et pas pendant : c'est le round 2 qui porte le resultat, et c'est lui seul
rem que les scripts rendent.
rem
rem Les textures s'ajoutent a celles du premier dump, dans le meme dossier : le
rem dump n'ecrit jamais par-dessus un fichier deja la, donc seules les nouvelles
rem arrivent. Ne pas effacer Textures3D-dump avant de lancer ceci.
setlocal enabledelayedexpansion
set MINGW=C:\msys64\mingw64\bin
set PATH=%MINGW%;%PATH%
set ROM=C:\Users\frede\Documents\SEGA Model 3\ROMs\fvipers2.zip
cd /d "%~dp0"
set "VF3_DATA=%~dp0"
if "%VF3_DATA:~-1%"=="\" set "VF3_DATA=%VF3_DATA:~0,-1%"

set "COULEURS=%~1"
if "%COULEURS%"=="" set "COULEURS=2"

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

py -3 -I "tools\fv2\mk_dump_armures.py" %COULEURS% || goto :rate

set "SORTIE=%~dp0Textures3D-dump"
mkdir "%SORTIE%\fvipers2" 2>nul
mkdir "%SORTIE%\ram" 2>nul

set "VF3_POKE=701444=15"
set "VF3_POKE_FRAMES=3200-7500"

set /a N=0
for %%S in ("tools\scripts\dump-armures\*.txt") do set /a N+=1
echo.
echo   %N% courses, armure explosee, %COULEURS% couleur(s) par combattant.
echo   Comptez environ %N% x 3 minutes.
echo.

set /a I=0
for %%S in ("tools\scripts\dump-armures\*.txt") do (
  set /a I+=1
  echo.
  echo --- !I!/%N%  %%~nS ---
  "build\bin\vf3run.exe" "%ROM%" --script "%%S" --hd-dump "%SORTIE%\fvipers2" --full-roster --ram-at 5200 --rec --quiet
  echo     code !ERRORLEVEL!
  if exist "ram_5200.bin" move /y "ram_5200.bin" "%SORTIE%\ram\%%~nS.ram" >nul
)

set "VF3_POKE="
set "VF3_POKE_FRAMES="

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
if /i not "%1"=="-silence" if /i not "%2"=="-silence" pause
