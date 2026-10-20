@echo off
rem Fighting Vipers 2 avec les personnages caches : Del Sol, Kuhn, BM et Dural
rem accessibles a la fleche sur l'ecran de selection, le curseur reboucle sur 14
rem au lieu de 10. Voir docs\fv2-roster.md.
rem
rem Limite connue, et elle est du jeu : au-dela de la case 10 les noms et les
rem portraits sont faux ou absents. Les combattants se chargent et se jouent ;
rem c'est l'etiquette au-dessus qui est celle de quelqu'un d'autre.
rem
rem Le reglage est retenu dans settings.ini (full_roster = 1). Pour l'enlever :
rem lancer jouer-fv2.cmd une fois avec --no-full-roster.
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
"build\bin\vf3.exe" "%ROM%" --full-roster %*
