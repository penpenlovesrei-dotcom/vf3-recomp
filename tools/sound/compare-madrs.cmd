@echo off
rem Les deux sons de la MEME scene, avec et sans le miroir de MADRS, SANS FENETRE, puis la mesure.
rem
rem   tools\sound\compare-madrs.cmd [jeu] [script] [images]
rem     defaut : vf3  tools\scripts\vf3-match.txt  2400
rem
rem Pourquoi : le miroir replie les lignes de retard du DSP sur une adresse (mesure du 2026-10-24,
rem docs/fv2-sound-program.md section 5 : le pilote ecrit 32 zeros dans 0x7C0-0x7FF APRES avoir
rem charge MADRS, ce qui ecrase 176 adresses sur Virtua Fighter 3). Le reglage a ete garde pour VF3
rem sur la seule LONGUEUR de la reverberation ; ceci donne les chiffres des deux cotes.
rem
rem Ne lance aucune fenetre : vf3run n'en a pas. Les WAV restent dans build\ pour etre ecoutes.
setlocal
set MINGW=C:\msys64\mingw64\bin
set PATH=%MINGW%;C:\msys64\usr\bin;%PATH%
set JEU=%1
set SCRIPT=%2
set IMAGES=%3
if "%JEU%"=="" set JEU=vf3
if "%SCRIPT%"=="" set SCRIPT=tools\scripts\vf3-match.txt
if "%IMAGES%"=="" set IMAGES=2400
set ROM=C:\Users\frede\Documents\SEGA Model 3\ROMs\%JEU%.zip
cd /d "%~dp0..\.."

if not exist "build\bin\vf3run.exe" (
  echo build\bin\vf3run.exe is missing: run build-tools.cmd first.
  goto :fin
)
if not exist "%ROM%" (
  echo La ROM %ROM% n'est pas la.
  goto :fin
)

echo ---- %IMAGES% images, le reglage ACTUEL (miroir pour VF3) ----
set VF3_SCSP_MADRS_SEPARATE=
set VF3_SCSP_MADRS_MIRROR=1
build\bin\vf3run.exe "%ROM%" --script "%SCRIPT%" --frames %IMAGES% --wav "build\son-%JEU%-miroir.wav" --quiet

echo ---- %IMAGES% images, les deux plages SEPAREES ----
set VF3_SCSP_MADRS_MIRROR=
set VF3_SCSP_MADRS_SEPARATE=1
build\bin\vf3run.exe "%ROM%" --script "%SCRIPT%" --frames %IMAGES% --wav "build\son-%JEU%-separe.wav" --quiet
set VF3_SCSP_MADRS_SEPARATE=

echo.
py -3 tools\sound\measure-madrs.py "build\son-%JEU%-miroir.wav" "build\son-%JEU%-separe.wav" --noms "miroir,separe" --scene %JEU%
echo.
echo ---- les deux WAV sont dans build\ : son-%JEU%-miroir.wav et son-%JEU%-separe.wav ----
:fin
endlocal
