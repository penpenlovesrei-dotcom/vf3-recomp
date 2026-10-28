@echo off
rem SONDE : pourquoi l'ecran CONTINUE n'apparait pas apres un combat perdu.
rem
rem C'est jouer-fv2-console.cmd avec deux traces allumees, et rien d'autre :
rem   VF3_VS_TRACE  une ligne a chaque changement de mode a partir du jugement,
rem                 avec ce que le jeu demande ensuite et ce qu'il dit du match ;
rem   VF3_MIDI_LOG  une ligne a chaque octet envoye a la carte son, avec l'image
rem                 et le mode -- donc quand la musique du combat est coupee,
rem                 quand celle du CONTINUE part, quand celle du GAME OVER part.
rem
rem A FAIRE : mode ARCADE, perdre un combat, laisser tourner jusqu'a l'ecran titre.
rem Le journal est log.txt, dans ce dossier.
setlocal
set PATH=C:\msys64\mingw64\bin;%PATH%
set ROM=C:\Users\frede\Documents\SEGA Model 3\ROMs\fvipers2.zip
set "VF3_DATA=%~dp0"
if "%VF3_DATA:~-1%"=="\" set "VF3_DATA=%VF3_DATA:~0,-1%"
set VF3_VS_TRACE=1
set VF3_MIDI_LOG=600
cd /d "%~dp0"
"build\bin\vf3.exe" "%ROM%" --console %*
