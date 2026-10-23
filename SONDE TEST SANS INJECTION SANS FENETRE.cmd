@echo off
rem Le TEMOIN du bug d'affichage en haut a gauche : le meme script, le meme
rem decor force (TEST), mais SANS la moindre injection.
rem
rem   l'artefact est la aussi  -> il est a la borne, pas a nous
rem   il disparait             -> il vient de notre scene ajoutee
setlocal
set ROM=C:\Users\frede\Documents\SEGA Model 3\ROMs\fvipers2.zip
set VF3_STAGE=0
cd /d "%~dp0"
"build\bin\vf3run.exe" "%ROM%" --script tools\scripts\fv2-sol-blanc.txt --shots sonde_nue\ --modes --quiet > sonde_nue\journal.txt 2>&1
echo code %ERRORLEVEL%
