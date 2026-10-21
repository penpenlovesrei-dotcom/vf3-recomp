@echo off
rem Fighting Vipers 2 : sort sur le bureau toutes les textures du costume de
rem base de Honey, dans ses deux couleurs.
rem
rem   Bureau\Honey-costume-de-base\couleur-1\     HONEY confirmee au poing
rem   Bureau\Honey-costume-de-base\couleur-2\     HONEY, un HAUT avant le poing
rem   Bureau\Honey-costume-de-base\RELEVE.txt     comment le tri a ete fait
rem
rem Trois courses sans fenetre, identiques en tout sauf une chose chacune :
rem Honey au poing ; Honey avec une pression de HAUT sur sa case avant le poing,
rem qui est ce qui change la couleur du costume (*1P COLOR 0x007005B9 passe de 0
rem a 1) ; et EMI au poing pour temoin. La capture du
rem moteur 3D ecrit tout ce qu'un polygone echantillonne -- le decor, le ciel,
rem l'adversaire, les effets viennent avec. Ce que la course de Honey a et que
rem le temoin n'a pas, c'est Honey. Voir docs\fv2-hd-3d.md.
rem
rem Rien n'est efface : chaque passage ecrit ses captures dans un dossier
rem horodate a lui, et le tri refuse d'ecraser un fichier deja sur le bureau.
rem
rem Comptez une vingtaine de minutes : trois courses de 4500 images, dont 167
rem images rendues chacune.
setlocal
set PATH=C:\msys64\mingw64\bin;%PATH%
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

rem Les scripts d'entrees sont generes, pour que les trois courses restent
rem rigoureusement comparables.
py -3 -I "tools\fv2\mk_honey_tex_scripts.py" || goto :rate

rem Un dossier de captures par passage : on n'efface jamais le precedent, et
rem une capture fraiche est indispensable, le tri etant une affaire d'ensembles.
rem L'horodatage vient de Python et pas de %DATE%, dont la forme depend de la
rem langue de Windows.
for /f %%q in ('py -3 -I -c "import time;print(time.strftime(chr(37)+chr(89)+chr(37)+chr(109)+chr(37)+chr(100)+chr(45)+chr(37)+chr(72)+chr(37)+chr(77)))"') do set "QUAND=%%q"
if "%QUAND%"=="" set "QUAND=capture"
set "CAPT=%~dp0captures-textures-honey\%QUAND%"
mkdir "%CAPT%\couleur1" 2>nul
mkdir "%CAPT%\couleur2" 2>nul
mkdir "%CAPT%\temoin"   2>nul

echo.
echo --- 1/3  HONEY, premiere couleur (poing) ---
"build\bin\vf3run.exe" "%ROM%" --script "tools\scripts\fv2-honey-tex-c1.txt" --hd-dump "%CAPT%\couleur1" --rec --quiet
echo     code %ERRORLEVEL%

echo.
echo --- 2/3  HONEY, seconde couleur (HAUT puis poing) ---
"build\bin\vf3run.exe" "%ROM%" --script "tools\scripts\fv2-honey-tex-c2.txt" --hd-dump "%CAPT%\couleur2" --rec --quiet
echo     code %ERRORLEVEL%

echo.
echo --- 3/3  EMI, le temoin (poing) ---
"build\bin\vf3run.exe" "%ROM%" --script "tools\scripts\fv2-emi-tex.txt" --hd-dump "%CAPT%\temoin" --rec --quiet
echo     code %ERRORLEVEL%

echo.
echo --- tri ---
set "SORTIE=%USERPROFILE%\Desktop\Honey-costume-de-base"
py -3 -I "tools\fv2\honey_textures.py" "%CAPT%" "%SORTIE%" || goto :rate

echo.
echo   Captures brutes  %CAPT%
echo   Sur le bureau    %SORTIE%
echo.
goto :fin

:rate
echo.
echo   ---- echec (code %ERRORLEVEL%) ----
:fin
if /i not "%1"=="-silence" pause
