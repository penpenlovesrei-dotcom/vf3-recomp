@echo off
rem replay.cmd : prend une trace d'acces au SCSP sur une scene, et la rejoue.
rem
rem Point 2 de l'etape 2 de scsp/docs/scsp_migration_plan.md. Deux controles en une commande :
rem   1. le rejeu retrouve-t-il le PCM que la carte a produit ? (la trace est-elle complete)
rem   2. deux coeurs rendent-ils la meme chose ? (la comparaison, quand il y en aura deux)
rem
rem   tools\son\replay.cmd [jeu] [script] [images] [coeur-a] [coeur-b]
rem
rem Sans arguments : fvipers2, fv2-fight.txt, 300 images, mame contre mame.
rem Rien n'est joue a l'ecran : vf3run est sans interface.
rem
rem Ce fichier est en ASCII pur : cmd lit les accents dans sa page de code ANSI et casse le decoupage des
rem lignes, ce qui donne des "'m' n'est pas reconnu" incomprehensibles.
setlocal
set JEU=%1
set SCRIPT=%2
set IMAGES=%3
set COEURA=%4
set COEURB=%5
if "%JEU%"=="" set JEU=fvipers2
if "%SCRIPT%"=="" set SCRIPT=tools\scripts\fv2-fight.txt
if "%IMAGES%"=="" set IMAGES=300
if "%COEURA%"=="" set COEURA=mame
if "%COEURB%"=="" set COEURB=mame
set ROM=C:\Users\frede\Documents\SEGA Model 3\ROMs\%JEU%.zip
set OUT=%TEMP%\son-rejeu
cd /d "%~dp0..\.."

if not exist "%ROM%" (
  echo La ROM %ROM% n'est pas la.
  goto :fin
)
if not exist "build\bin\vf3run.exe" (
  echo build\bin\vf3run.exe manque.
  goto :fin
)
if not exist "build\bin\replay.exe" (
  echo build\bin\replay.exe manque : make build/bin/replay
  goto :fin
)

if not exist "%OUT%" mkdir "%OUT%"
del /q "%OUT%\tr.*" 2>nul

echo ---- la trace, %IMAGES% images de %SCRIPT% ----
set VF3_SCSP_TRACE=%OUT%\tr
"build\bin\vf3run.exe" "%ROM%" --script "%SCRIPT%" --frames %IMAGES% --wav "%OUT%\son.wav" --quiet
set VF3_SCSP_TRACE=
if errorlevel 1 (
  echo vf3run a echoue.
  goto :fin
)

set FAUTES=0
for %%P in (0 1) do (
  echo.
  echo ---- puce %%P : la trace est-elle complete ? ----
  "build\bin\replay.exe" "%OUT%\tr.%%P" --against "%OUT%\tr.%%P.pcm"
  if errorlevel 1 set /a FAUTES+=1
  echo.
  echo ---- puce %%P : %COEURA% contre %COEURB% ----
  "build\bin\replay.exe" "%OUT%\tr.%%P" %COEURA% %COEURB%
  if errorlevel 1 set /a FAUTES+=1
)

echo.
if "%FAUTES%"=="0" (
  echo TOUT CONCORDE. Les traces sont dans %OUT%.
) else (
  echo %FAUTES% controle^(s^) en echec. Les traces sont dans %OUT%.
)

:fin
endlocal
