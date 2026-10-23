@echo off
rem Pousse le combattant 1 dans le mur de droite pendant 700 images, puis vide
rem la RAM a l'image 3399 dans ram_3399.bin, sans fenetre.
setlocal
set ROM=C:\Users\frede\Documents\SEGA Model 3\ROMs\fvipers2.zip
set "VF3_SCENE=C:\Users\frede\Documents\fv2-arcade-notes\real3d\TEST"
set VF3_STAGE=0
set VF3_SCENE_ADD=0
cd /d "%~dp0"
"build\bin\vf3run.exe" "%ROM%" --script tools\scripts\fv2-contre-le-mur.txt --ram-at 3599 --modes --quiet > mur_journal.txt 2>&1
echo code %ERRORLEVEL%
