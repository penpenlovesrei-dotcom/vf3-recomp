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
set CMP=C:\Temp\cmp
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
echo ---- pas a pas interpreteur contre code recompile (vf3lock) ----
"build\bin\vf3lock.exe" "%ROM%" --script tools\scripts\vf3-match.txt

:fin
echo.
echo ---- fini (code %ERRORLEVEL%) ----
pause
