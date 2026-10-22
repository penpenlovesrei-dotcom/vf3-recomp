@echo off
rem Fighting Vipers 2 a la maniere d'une console : Start ouvre la liste des sept
rem modes sur l'ecran titre du jeu, comme sur la version Dreamcast, au lieu de
rem tomber directement sur la route fixe de l'arcade.
rem Voir docs\fv2-console.md.
rem
rem UNE SEULE TOUCHE, DEUX COMPORTEMENTS :
rem   sur l'ecran titre du jeu  -> Start ouvre la liste des sept modes
rem   partout ailleurs          -> Start ramene le jeu a son ecran titre
rem Dans les deux cas le jeu ne voit pas l'appui.
rem
rem Dans la liste : haut et bas deplacent, Start (ou Entree) choisit, B (ou
rem Retour arriere) ressort, et la ligne choisie clignote. Echap ouvre toujours
rem le menu de reglages de ce portage, et referme la liste au passage.
rem
rem SIX LIGNES SUR SEPT FONT QUELQUE CHOSE :
rem   ARCADE    la partie, telle quelle
rem   V.S.      la partie avec le deuxieme joueur deja dedans
rem   RANDOM    le drapeau RANDOM_CHARA du jeu : personnage tire au sort
rem   SURVIVAL  le mode survie
rem   TRAINING  pour l'instant : le combat dans MARBLE ARENA
rem   OPTIONS   une page a elle, par-dessus l'image du jeu (voir plus bas)
rem   NETWORK   n'existe pas sur la borne et ne fait rien
rem
rem LA PAGE OPTIONS : les treize reglages d'exploitation du jeu, ceux de son
rem menu de test. Haut et bas deplacent, GAUCHE et DROITE changent la valeur,
rem B ressort. INITIALIZE remet les valeurs d'usine. RIEN NE REDEMARRE : chaque
rem reglage est ecrit a la fois dans la sauvegarde du jeu, ou il dure, et dans
rem la copie que le jeu lit en tournant, ou il prend effet tout de suite. Il
rem prend effet au moment ou le jeu relit ce reglage -- en general au debut du
rem match suivant. COUNTRY est affiche en gris : il est en lecture seule.
rem Voir docs\fv2-console-options.md.
rem
rem PAS BESOIN DE PIECE : le mode console est en FREE PLAY, comme une console.
rem Le portage pose le reglage COIN/CREDIT du jeu lui-meme au demarrage (un
rem octet de la NVRAM, voir docs\fv2-console.md). La liste, elle, ne touche
rem jamais au compteur de credits : choisir un mode donne au jeu un vrai appui
rem sur Start, et c'est le jeu qui decide de ce que ca coute -- rien, ici.
rem
rem     1   Start joueur 1            2   Start joueur 2
rem     5   une piece joueur 1        6   une piece joueur 2
rem     F2  Test                      9   Service
rem
rem Les memes choix sont aussi sous Echap, ligne "Fighting Vipers 2" : c'est une
rem page de reglages de ce portage, et ce n'est pas la liste.
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
  echo build\bin\vf3.exe manque : installer SDL 3 puis lancer construire-hote.cmd.
  pause
  exit /b 1
)
"build\bin\vf3.exe" "%ROM%" --console %*
