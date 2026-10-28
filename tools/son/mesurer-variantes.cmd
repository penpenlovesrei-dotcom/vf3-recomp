@echo off
rem mesurer-variantes.cmd : compte les INSTRUCTIONS RETIREES de chaque variante du DSP, au compteur materiel.
rem
rem POURQUOI PAS UN CHRONOMETRE. docs/scsp_audit.md S4 octies et S4 nonies racontent les deux fois ou le temps
rem a menti : le socle de `mame` a derive de 2,70 a 3,21 ns pendant une seance d'essais, et un biais d'ordre a
rem failli faire garder un gain de 6,9 pour cent qui n'existait pas. Le compteur d'instructions retirees, lui,
rem ne derive pas avec la temperature et ne depend pas de l'ordre des mesures. Une capture par variante, un
rem chiffre dur.
rem
rem IL FAUT UNE FENETRE ADMINISTRATEUR : xperf ne peut pas programmer les compteurs autrement, et il le dit
rem ("Failed to configure counters"). Rien d'autre n'est demande -- les exes sont deja construits et deja
rem verifies par les dix tests : python tools\son\batir-variantes.py les fabrique.
rem
rem   Win+X, "Terminal (administrateur)", puis :
rem     cd /d C:\Users\frede\Desktop\fv2-recomp
rem     tools\son\mesurer-variantes.cmd jeu       (ou `socle`, qui est le defaut)
rem
rem Ferme le jeu avant : une capture a deja ete perdue parce que vf3.exe prenait les cycles.
rem
rem Ce fichier est en ASCII pur : cmd lit les accents dans sa page de code ANSI et casse le decoupage des
rem lignes, ce qui donne des "'m' n'est pas reconnu" incomprehensibles.
setlocal enabledelayedexpansion
set XPERF=C:\Program Files (x86)\Windows Kits\10\Windows Performance Toolkit\xperf.exe
set VAR=%~dp0..\..\build\bin\variantes
set OUT=C:\Temp\instr
rem LE MODE : `socle` (defaut) ou `jeu`. Le socle est un programme ou les 112 pas font tous la meme
rem chose : il situe un cout. Le programme de la forme du jeu est ce qui compte, et le 2026-10-28 une
rem combinaison de variantes a gagne 29 pour cent sur le premier et rien sur le second.
set MODE=%1
if "%MODE%"=="" set MODE=socle
if "%MODE%"=="socle" (set ECHANT=2000000) else (set ECHANT=600000)
if not "%MODE%"=="socle" if not "%MODE%"=="jeu" (
  echo Mode inconnu : %MODE%. Les deux sont `socle` et `jeu`.
  goto :fin
)
cd /d "%~dp0..\.."
rem LES DLL DE MinGW SONT DANS build\bin ET LES EXES DANS build\bin\variantes : sans cette ligne,
rem Windows ne trouve ni libstdc++-6.dll ni libgcc_s_seh-1.dll et sort une boite par variante. Une
rem fenetre administrateur n'a pas MSYS2 dans son PATH, et elle n'a pas a l'avoir.
set PATH=%~dp0..\..\build\bin;%PATH%

if not exist "%XPERF%" (
  echo Le Windows Performance Toolkit n'est pas la : "%XPERF%"
  goto :fin
)
if not exist "%VAR%" (
  echo Les exes des variantes manquent : %VAR%
  echo Les fabriquer : python tools\son\batir-variantes.py
  goto :fin
)
if not exist "%OUT%" mkdir "%OUT%" 2> nul

rem UN ARRET DEFENSIF D'ABORD : si une capture precedente a ete interrompue, le traceur noyau tourne
rem encore et le -on suivant echouerait sur "already started". Sans objet le reste du temps.
"%XPERF%" -stop > nul 2>&1

rem Le compteur est-il programmable ? Un essai franc avant la boucle, pour ne pas decouvrir au troisieme
rem tour qu'on n'est pas administrateur.
"%XPERF%" -on PROC_THREAD+LOADER+PMC_PROFILE -PmcProfile InstructionRetired 2> nul
if errorlevel 1 (
  echo.
  echo xperf n'a pas pu programmer les compteurs. Cette fenetre n'est probablement pas
  echo administrateur : Win+X, "Terminal ^(administrateur^)", puis relancer.
  goto :fin
)
"%XPERF%" -stop > nul 2>&1

for %%E in ("%VAR%\mc-*.exe") do (
  set NOM=%%~nE
  echo.
  echo ---- !NOM! ----
  "%XPERF%" -on PROC_THREAD+LOADER+PMC_PROFILE -PmcProfile InstructionRetired
  "%%~fE" --%MODE% maison %ECHANT%
  "%XPERF%" -stop -d "%OUT%\!NOM!-%MODE%.etl" > nul
  echo     trace : %OUT%\!NOM!-%MODE%.etl
)

echo.
echo ---- fini ----
echo Les traces sont dans %OUT%. L'analyse ne demande pas d'elevation.
:fin
echo.
pause
