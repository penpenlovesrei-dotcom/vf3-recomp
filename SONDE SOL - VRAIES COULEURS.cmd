@echo off
rem La meme passe que SONDE SOL BLANC SANS FENETRE.cmd, mais SANS le rouge :
rem les vraies couleurs, pour juger a l'oeil.
setlocal
set ROM=C:\Users\frede\Documents\SEGA Model 3\ROMs\fvipers2.zip
set "VF3_SCENE=C:\Users\frede\Documents\fv2-arcade-notes\real3d\TEST"
set VF3_STAGE=0
set VF3_SCENE_ADD=0
set VF3_SCENE_LOG=1
cd /d "%~dp0"
"build\bin\vf3run.exe" "%ROM%" --script tools\scripts\fv2-sol-blanc.txt --shots sonde_sol\ --modes --quiet > sonde_sol\journal.txt 2>&1
echo code %ERRORLEVEL%
