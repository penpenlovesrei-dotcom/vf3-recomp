@echo off
rem Fighting Vipers 2 : la sonde du sol blanc, SANS FENETRE et sans joueur.
rem
rem vf3run joue tools\scripts\fv2-sol-blanc.txt -- une piece, start, un
rem combattant, puis un round joue au hasard -- et ecrit une capture toutes les
rem vingt images dans sonde_sol\.
rem
rem   --modes            l'etat du jeu a chaque changement, sous le nom de Sega
rem   VF3_DIAG_ROUGE=1   nos maillages injectes sortent en ROUGE PUR
rem   VF3_VP=1           a chaque capture, tous les viewports avec leur
rem                      lumiere, leur brouillard et leur projecteur
setlocal
set ROM=C:\Users\frede\Documents\SEGA Model 3\ROMs\fvipers2.zip
set "VF3_SCENE=C:\Users\frede\Documents\fv2-arcade-notes\real3d\TEST"
set VF3_STAGE=0
set VF3_SCENE_ADD=0
set VF3_SCENE_LOG=1
set VF3_DIAG_ROUGE=1
set VF3_VP=1
cd /d "%~dp0"
"build\bin\vf3run.exe" "%ROM%" --script tools\scripts\fv2-sol-blanc.txt --shots sonde_sol\ --modes --quiet > sonde_sol\journal.txt 2>&1
echo code %ERRORLEVEL%
