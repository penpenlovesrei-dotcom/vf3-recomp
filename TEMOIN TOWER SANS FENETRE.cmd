@echo off
rem Le TEMOIN de la sonde du sol blanc : le meme script, le meme round, mais sur
rem TOWER -- un decor d'origine, complet, SANS la moindre injection.
rem
rem A QUOI CA SERT. Sur TEST, une bande blanche a bord net couvre le bas de
rem l'ecran pendant ROUND_MASK et SET. Si elle est la sur TOWER aussi, c'est la
rem transition de round du jeu et il n'y a rien a corriger. Si elle n'y est pas,
rem c'est notre decor qui manque dans cette zone.
setlocal
set ROM=C:\Users\frede\Documents\SEGA Model 3\ROMs\fvipers2.zip
set VF3_STAGE=10
cd /d "%~dp0"
"build\bin\vf3run.exe" "%ROM%" --script tools\scripts\fv2-sol-blanc.txt --shots sonde_tower\ --modes --quiet > sonde_tower\journal.txt 2>&1
echo code %ERRORLEVEL%
