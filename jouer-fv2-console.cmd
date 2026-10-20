@echo off
rem Fighting Vipers 2 a la maniere d'une console : le menu des modes s'ouvre
rem avec Start pendant la demonstration, comme sur la version Dreamcast, au
rem lieu de tomber directement sur la route fixe de l'arcade.
rem Voir docs\fv2-console.md.
rem
rem IL FAUT QUAND MEME UNE PIECE. Le menu ne touche jamais au compteur de
rem credits : choisir un mode donne au jeu un vrai appui sur Start, et c'est
rem le jeu qui decide de ce que ca coute. Le freeplay du mode PC n'est connu
rem que pour Virtua Fighter 3. Donc :
rem
rem     5   une piece joueur 1        6   une piece joueur 2
rem     1   Start joueur 1            2   Start joueur 2
rem     F2  Test                      9   Service
rem
rem Deux fois 5, puis 1 : le menu des modes s'ouvre. ARCADE ou SURVIVAL
rem lancent la partie. Le menu est aussi sous Echap, ligne "Fighting Vipers 2".
rem
rem Le reglage est retenu dans settings.ini (console_mode = 1). Pour revenir
rem au mode PC : lancer jouer-fv2.cmd une fois avec --pc.
setlocal
set PATH=C:\msys64\mingw64\bin;%PATH%
set ROM=C:\Users\frede\Documents\SEGA Model 3\ROMs\fvipers2.zip
cd /d "%~dp0"
if not exist "build\bin\vf3.exe" (
  echo build\bin\vf3.exe manque : installer SDL 3 puis lancer construire.cmd.
  pause
  exit /b 1
)
"build\bin\vf3.exe" "%ROM%" --console %*
