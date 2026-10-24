@echo off
rem Non-regression de Virtua Fighter 3 : compare cette branche a la version
rem d'origine (le commit "main"), sur la meme ROM revision D. Doit sortir
rem IDENTIQUE partout, sinon une modification de la Step 2.0 a touche la 1.0.
rem Premier lancement : cree l'arbre de travail d'origine et le construit.
setlocal
set MINGW=C:\msys64\mingw64\bin
set MAKE=C:\msys64\usr\bin\make.exe
set PATH=%MINGW%;C:\msys64\usr\bin;%PATH%
set SCRATCH=%LOCALAPPDATA%\Temp
set ROM=C:\Users\frede\Documents\SEGA Model 3\ROMs\vf3.zip
set UP=C:\Temp\vf3up
rem Le dossier de comparaison porte le nom de CET arbre de travail. Partage, il est
rem ecrase par une autre session qui verifie en meme temps : mesure le 2026-10-23,
rem deux executions dans le meme dossier ont donne un etat tronque a 2000 images
rem contre 3000, donc un *** DIFFERENT *** qui n'en etait pas.
for %%I in ("%~dp0.") do set WT=%%~nxI
set CMP=C:\Temp\cmp-%WT%
set IMAGES=%1
if "%IMAGES%"=="" set IMAGES=3000
cd /d "%~dp0"

if not exist "%UP%\Makefile" (
  echo ---- creation de l'arbre d'origine dans %UP% ----
  git worktree add "%UP%" main || goto :fin
)
if not exist "%UP%\build\bin\vf3run.exe" (
  echo ---- construction de la version d'origine ----
  pushd "%UP%"
  "%MAKE%" CXX=g++ TMP=%SCRATCH% TEMP=%SCRATCH% TMPDIR=%SCRATCH% build/bin/vf3run -j8
  popd
)
if not exist "build\bin\vf3run.exe" (
  echo build\bin\vf3run.exe manque : lancer construire.cmd d'abord.
  goto :fin
)

if not exist "%CMP%\up" mkdir "%CMP%\up"
if not exist "%CMP%\fv2" mkdir "%CMP%\fv2"
echo ---- %IMAGES% images, version d'origine ----
pushd "%CMP%\up"
"%UP%\build\bin\vf3run.exe" "%ROM%" --frames %IMAGES% --state s.state --ram-at %IMAGES% --quiet
popd
echo ---- %IMAGES% images, cette branche ----
pushd "%CMP%\fv2"
"%~dp0build\bin\vf3run.exe" "%ROM%" --frames %IMAGES% --state s.state --ram-at %IMAGES% --quiet
popd

echo.
echo ---- etat du PowerPC, image par image ----
fc /b "%CMP%\up\s.state" "%CMP%\fv2\s.state" > nul && echo   IDENTIQUE || echo   *** DIFFERENT ***
echo ---- les 8 Mo de RAM ----
fc /b "%CMP%\up\ram_%IMAGES%.bin" "%CMP%\fv2\ram_%IMAGES%.bin" > nul && echo   IDENTIQUE || echo   *** DIFFERENT ***

echo.
echo ---- images rendues (vf3run --shots) ----
if exist "%CMP%\shup" rmdir /s /q "%CMP%\shup"
if exist "%CMP%\shnew" rmdir /s /q "%CMP%\shnew"
mkdir "%CMP%\shup" & mkdir "%CMP%\shnew"
"%UP%\build\bin\vf3run.exe" "%ROM%" --script "%~dp0tools\scripts\vf3-shots.txt" --shots "%CMP%\shup" --quiet > nul
"%~dp0build\bin\vf3run.exe" "%ROM%" --script "%~dp0tools\scripts\vf3-shots.txt" --shots "%CMP%\shnew" --quiet > nul
set SHOTS_OK=1
if not exist "%CMP%\shup\*.png" set SHOTS_OK=0
for %%F in ("%CMP%\shup\*.png") do fc /b "%%F" "%CMP%\shnew\%%~nxF" > nul || set SHOTS_OK=0
if "%SHOTS_OK%"=="1" (echo   IDENTIQUE) else (echo   *** DIFFERENT ***)

rem L'hote complet : l'image du jeu que la fenetre affiche. Elle est reproductible
rem (host_last.png), contrairement a host_window.png qui depend du rythme reel.
rem Sans SDL 3 installe, la cible build/bin/vf3 n'existe pas et on saute l'essai.
if not exist "build\bin\vf3.exe" goto :pashote
if not exist "%UP%\build\bin\vf3.exe" (
  echo ---- construction de l'hote d'origine ----
  pushd "%UP%"
  "%MAKE%" CXX=g++ TMP=%SCRATCH% TEMP=%SCRATCH% TMPDIR=%SCRATCH% build/bin/vf3 -j8
  popd
)
if not exist "%UP%\build\bin\vf3.exe" goto :pashote
echo.
echo ---- l'hote complet, image du jeu ----
if exist "%CMP%\hup" rmdir /s /q "%CMP%\hup"
if exist "%CMP%\hnew" rmdir /s /q "%CMP%\hnew"
mkdir "%CMP%\hup" & mkdir "%CMP%\hnew"
> "%CMP%\hup\settings.ini" echo settings_version = 3
>> "%CMP%\hup\settings.ini" echo picture_shape = 3
copy /y "%CMP%\hup\settings.ini" "%CMP%\hnew\settings.ini" > nul
set VF3_HIDDEN=1
set VF3_QUIT_AFTER=1500
set VF3_WINDOW=1280x720
set VF3_3D=cpu
set VF3_SCRIPT=%~dp0tools\scripts\vf3-match.txt
pushd "%UP%"
set VF3_DATA=%CMP%\hup
del /q build\host_last.png 2> nul
"%UP%\build\bin\vf3.exe" "%ROM%" > nul 2>&1
copy /y build\host_last.png "%CMP%\hup\last.png" > nul
popd
set VF3_DATA=%CMP%\hnew
del /q build\host_last.png 2> nul
"%~dp0build\bin\vf3.exe" "%ROM%" > nul 2>&1
copy /y build\host_last.png "%CMP%\hnew\last.png" > nul
fc /b "%CMP%\hup\last.png" "%CMP%\hnew\last.png" > nul && echo   IDENTIQUE || echo   *** DIFFERENT ***
set VF3_HIDDEN=
set VF3_QUIT_AFTER=
set VF3_SCRIPT=
set VF3_DATA=
set VF3_3D=
:pashote

echo.
echo ---- pas a pas interpreteur contre code recompile (vf3lock) ----
"build\bin\vf3lock.exe" "%ROM%" --script tools\scripts\vf3-match.txt

:fin
echo.
echo ---- fini (code %ERRORLEVEL%) ----
pause
