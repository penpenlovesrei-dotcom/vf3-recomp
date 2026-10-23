@echo off
rem TOWER, un match long joue au hasard par les deux cotes, sans fenetre.
rem On cherche le moment ou un mur casse : le masque [[0x00075124]+0xC0] change,
rem et l'octet d'etat +0xC d'un des trente-deux objets passe a 2.
setlocal
set ROM=C:\Users\frede\Documents\SEGA Model 3\ROMs\fvipers2.zip
set VF3_STAGE=10
set VF3_STAGE_LOG=1
cd /d "%~dp0"
"build\bin\vf3run.exe" "%ROM%" --script tools\scripts\fv2-casser-un-mur.txt --modes --quiet > mur_tower.txt 2>&1
echo code %ERRORLEVEL%
