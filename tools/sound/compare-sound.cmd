@echo off
rem Harnais differentiel du son : fait jouer la MEME scene au vf3run de cette branche et a celui
rem d'une reference (par defaut le commit HEAD d'avant les modifications, dans un arbre de travail
rem a part), puis compare les deux WAV octet par octet. Un son qui bouge d'un seul echantillon est
rem une regression a expliquer, pas une tolerance a elargir.
rem
rem   tools\son\compare-sound.cmd <jeu> <script> <images> [ref-commit]
rem
rem   jeu     : fvipers2 ou vf3 (le nom du zip dans Documents\SEGA Model 3\ROMs)
rem   script  : un fichier de tools\scripts
rem   images  : combien d'images jouer
rem   ref     : le commit de reference (defaut : HEAD)
rem
rem Exemples :
rem   tools\son\compare-sound.cmd fvipers2 tools\scripts\fv2-fight.txt 4200
rem   tools\son\compare-sound.cmd vf3      tools\scripts\vf3-match.txt 4200
rem
rem Les deux runs ecrivent dans %TEMP%\son-cmp\<jeu>\, chacun dans son dossier : le meme endroit a
rem chaque fois, donc UN SEUL comparateur a la fois (la lecon de verifier-vf3.cmd).
setlocal
set MINGW=C:\msys64\mingw64\bin
set MAKE=C:\msys64\usr\bin\make.exe
set PATH=%MINGW%;C:\msys64\usr\bin;%PATH%
set SCRATCH=%LOCALAPPDATA%\Temp
set JEU=%1
set SCRIPT=%2
set IMAGES=%3
set REFC=%4
if "%JEU%"=="" set JEU=fvipers2
if "%SCRIPT%"=="" set SCRIPT=tools\scripts\fv2-fight.txt
if "%IMAGES%"=="" set IMAGES=4200
if "%REFC%"=="" set REFC=HEAD
set ROM=C:\Users\frede\Documents\SEGA Model 3\ROMs\%JEU%.zip
set REF=C:\Temp\fv2-son-ref
set OUT=%TEMP%\son-cmp\%JEU%
cd /d "%~dp0..\.."

if not exist "%ROM%" (
  echo La ROM %ROM% n'est pas la.
  goto :fin
)
if not exist "build\bin\vf3run.exe" (
  echo build\bin\vf3run.exe is missing: run construire.cmd first.
  goto :fin
)

if not exist "%REF%\Makefile" (
  echo ---- arbre de reference dans %REF%, sur %REFC% ----
  git worktree add --detach "%REF%" %REFC% || goto :fin
)
if not exist "%REF%\build\bin\vf3run.exe" (
  echo ---- construction de la reference ----
  pushd "%REF%"
  "%MAKE%" CXX=g++ TMP=%SCRATCH% TEMP=%SCRATCH% TMPDIR=%SCRATCH% build/bin/vf3run -j8 || (popd & goto :fin)
  popd
)

if not exist "%OUT%\ref" mkdir "%OUT%\ref" 2> nul
if not exist "%OUT%\moi" mkdir "%OUT%\moi" 2> nul

echo ---- %IMAGES% images, reference ----
pushd "%OUT%\ref"
set VF3_PROF=1
"%REF%\build\bin\vf3run.exe" "%ROM%" --script "%~dp0..\..\%SCRIPT%" --frames %IMAGES% --wav son.wav --quiet
popd
echo ---- %IMAGES% images, cette branche ----
pushd "%OUT%\moi"
"%~dp0..\..\build\bin\vf3run.exe" "%ROM%" --script "%~dp0..\..\%SCRIPT%" --frames %IMAGES% --wav son.wav --quiet
popd
set VF3_PROF=

echo.
echo ---- le son, octet par octet ----
fc /b "%OUT%\ref\son.wav" "%OUT%\moi\son.wav" > nul && echo   IDENTICAL || echo   *** DIFFERENT ***
:fin
endlocal
