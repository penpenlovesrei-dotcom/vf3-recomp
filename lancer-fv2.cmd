@echo off
rem Fait tourner Fighting Vipers 2 sur l'interpreteur, sans fenetre, et releve
rem l'etat : le set reconnu, le stepping, le PC, l'empreinte de la RAM et ce que
rem le jeu a demande a la puce de securite.
rem   lancer-fv2.cmd            -> 120 images, et un vidage de la RAM
rem   lancer-fv2.cmd 1800       -> 1800 images, sans vidage
setlocal
set ROM=C:\Users\frede\Documents\SEGA Model 3\ROMs\fvipers2.zip
set IMAGES=%1
if "%IMAGES%"=="" set IMAGES=120
cd /d "%~dp0"
if not exist "build\bin\vf3run.exe" (
  echo build\bin\vf3run.exe manque : lancer construire.cmd d'abord.
  pause
  exit /b 1
)
if "%1"=="" (
  "build\bin\vf3run.exe" "%ROM%" --frames %IMAGES% --ram-at %IMAGES%
) else (
  "build\bin\vf3run.exe" "%ROM%" --frames %IMAGES%
)
echo.
echo ---- fini (code %ERRORLEVEL%) ----
pause
