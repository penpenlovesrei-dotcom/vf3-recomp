@echo off
rem La taille de chaque modele injecte, telle que le MOTEUR la decode.
rem La regle : le plan bouche-trou de la borne fait x -6..6 et z -6..6 pour une
rem arene de +-5. Notre sol doit donc sortir a 4,78 de rayon.
setlocal
set ROM=C:\Users\frede\Documents\SEGA Model 3\ROMs\fvipers2.zip
set "VF3_SCENE=C:\Users\frede\Documents\fv2-arcade-notes\real3d\TEST"
set VF3_STAGE=0
set VF3_SCENE_ADD=0
set VF3_ECHELLE=1
cd /d "%~dp0"
"build\bin\vf3run.exe" "%ROM%" --script tools\scripts\fv2-contre-le-mur.txt --shots echelle_img\ --quiet > echelle.txt 2>&1
echo code %ERRORLEVEL%
