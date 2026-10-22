@echo off
rem Fighting Vipers 2 : ce qu'est une ombre dans ce jeu, et combien il y en a.
rem Sans fenetre, sans manette -- tout est scripte. Voir docs\fv2-ombres.md.
rem
rem Trois passes, environ huit minutes en tout :
rem   1. un combat, image 3101, telle que le jeu la dessine    -> build\ombres\
rem   2. la meme, les maillages de couleur 051A26 peints en
rem      magenta, et l'audit de TOUS les maillages             -> build\ombres-marque\
rem   3. la boucle d'attraction, une image toutes les 200 de
rem      1400 a 9000 : plusieurs decors, plusieurs combattants -> build\ombres-attract\
rem
rem A regarder, dans cet ordre :
rem   build\ombres\shot_003101_9_fight.png           l'ombre sous chaque combattant
rem   build\ombres-marque\shot_003101_9_fight.png    le magenta tombe-t-il sur les
rem                                                  ombres, et SEULEMENT sur elles ?
rem   build\ombres\audit-3101.txt                    485 lignes, une par maillage
rem   build\ombres-attract\*.png                     39 images : la couleur de l'ombre
rem                                                  change-t-elle avec le decor ?
rem   build\ombres-attract\audit-attract.txt         19 968 lignes, avec le decor
setlocal
set PATH=C:\msys64\mingw64\bin;%PATH%
rem La ROM : celle de ce dossier si elle y est (roms\, hors depot), sinon celle du
rem dossier ou les autres lanceurs vont la chercher.
set "ROM=%~dp0roms\fvipers2.zip"
if not exist "%ROM%" set "ROM=C:\Users\frede\Documents\SEGA Model 3\ROMs\fvipers2.zip"
if not exist "%ROM%" (
  echo La ROM est introuvable, ni dans roms\ ni dans Documents\SEGA Model 3\ROMs.
  pause
  exit /b 1
)
cd /d "%~dp0"
if not exist "build\bin\vf3run.exe" (
  echo build\bin\vf3run.exe manque : lancer construire.cmd.
  pause
  exit /b 1
)
if not exist "build\ombres" mkdir "build\ombres"
if not exist "build\ombres-marque" mkdir "build\ombres-marque"
if not exist "build\ombres-attract" mkdir "build\ombres-attract"

echo.
echo ---- 1 sur 3 : un combat, l'image telle quelle ----
"build\bin\vf3run.exe" "%ROM%" --script tools\scripts\fv2-fight.txt --shots build\ombres --frames 3110

echo.
echo ---- 2 sur 3 : la marque, et l'audit de cette image ----
set VF3_MARK_RGB=051A26
set VF3_MESH_AUDIT=3101
"build\bin\vf3run.exe" "%ROM%" --script tools\scripts\fv2-fight.txt --shots build\ombres-marque --frames 3110 2> build\ombres\audit-3101.txt
set VF3_MARK_RGB=
set VF3_MESH_AUDIT=

echo.
echo ---- 3 sur 3 : l'attraction, 39 images et leurs decors ----
set VF3_MESH_AUDIT=1401-9001
"build\bin\vf3run.exe" "%ROM%" --script tools\scripts\fv2-ombres-attract.txt --shots build\ombres-attract --frames 9100 2> build\ombres-attract\audit-attract.txt
set VF3_MESH_AUDIT=

echo.
echo ---- fini (code %ERRORLEVEL%) ----
echo Les images sont dans build\ombres, build\ombres-marque et build\ombres-attract.
pause
