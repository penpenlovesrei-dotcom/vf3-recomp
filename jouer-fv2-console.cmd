@echo off
rem Fighting Vipers 2 a la maniere d'une console : le menu des modes s'ouvre
rem avec Start pendant la demonstration, comme sur la version Dreamcast, au
rem lieu de tomber directement sur la route fixe de l'arcade.
rem Voir docs\fv2-console.md.
rem
rem PAS BESOIN DE PIECE : le mode console est en FREE PLAY, comme une console.
rem Le portage pose le reglage COIN/CREDIT du jeu lui-meme au demarrage (un
rem octet de la NVRAM, voir docs\fv2-console.md). Le menu, lui, ne touche
rem jamais au compteur de credits : choisir un mode donne au jeu un vrai appui
rem sur Start, et c'est le jeu qui decide de ce que ca coute -- rien, ici.
rem
rem     1   Start joueur 1            2   Start joueur 2
rem     5   une piece joueur 1        6   une piece joueur 2
rem     F2  Test                      9   Service
rem
rem Un appui sur 1 pendant la demonstration ouvre le menu des modes. ARCADE ou
rem SURVIVAL lancent la partie. Le menu est aussi sous Echap, ligne
rem "Fighting Vipers 2".
rem
rem Le reglage est retenu dans settings.ini (console_mode = 1). Pour revenir
rem au mode PC : lancer jouer-fv2.cmd une fois avec --pc.
setlocal
set PATH=C:\msys64\mingw64\bin;%PATH%
set ROM=C:\Users\frede\Documents\SEGA Model 3\ROMs\fvipers2.zip
rem Ce portage range ce qui lui appartient -- reglages, NVRAM, journal,
rem portraits -- dans le dossier du projet, celui ou est ce lanceur. Les
rem quatre lanceurs epinglent le meme, sinon chacun aurait les siens.
set "VF3_DATA=%~dp0"
if "%VF3_DATA:~-1%"=="\" set "VF3_DATA=%VF3_DATA:~0,-1%"
cd /d "%~dp0"
if not exist "build\bin\vf3.exe" (
  echo build\bin\vf3.exe manque : installer SDL 3 puis lancer construire.cmd.
  pause
  exit /b 1
)
"build\bin\vf3.exe" "%ROM%" --console %*
