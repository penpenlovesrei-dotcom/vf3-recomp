@echo off
rem Fighting Vipers 2, mode console, AVEC LA SONDE.
rem
rem Identique a jouer-fv2-console.cmd, sauf que la partie est enregistree :
rem
rem   log.txt        a la racine du projet. Pour chaque image dessinee : le
rem                  nombre de modeles 3D dans la scene, le nombre de vues, le
rem                  mode du jeu et son sous-etat ; et tout ce que le jeu dit a
rem                  sa carte son.
rem   Screenshots\   ce que la touche F12 y depose.
rem
rem A FAIRE QUAND LE DEFAUT APPARAIT : appuyer sur F12. L'image est ecrite dans
rem Screenshots\ avec un numero, et le journal garde l'image de jeu exacte. Dire
rem ensuite a Claude a quel moment c'etait ; le journal fera le reste.
rem
rem Le journal grossit vite (une ligne par image dessinee). C'est voulu : il est
rem ecrase a chaque lancement.
setlocal
set PATH=C:\msys64\mingw64\bin;%PATH%
set ROM=C:\Users\frede\Documents\SEGA Model 3\ROMs\fvipers2.zip
set "VF3_DATA=%~dp0"
if "%VF3_DATA:~-1%"=="\" set "VF3_DATA=%VF3_DATA:~0,-1%"
cd /d "%~dp0"
if not exist "build\bin\vf3.exe" (
  echo build\bin\vf3.exe manque : lancer construire-hote.cmd d'abord.
  pause
  exit /b 1
)
set VF3_PIC_LOG=1
set VF3_MIDI_LOG=60
rem La sonde du son du menu : pour chaque image de son fabriquee sous la liste
rem ou sous OPTIONS, la cadence, le pic, et surtout CE QUI ATTEND DANS LA FILE
rem DE SDL. La carte a ete mesuree fabriquant les bruitages sur l'image meme de
rem la touche, donc un bruitage entendu en retard est un bruitage qui attend
rem derriere quelque chose -- et c'est ce que cette ligne dit. Ne se mesure que
rem sur une vraie machine : une partie sans fenetre n'a pas de carte son.
set VF3_MENU_SOUND=1
"build\bin\vf3.exe" "%ROM%" --console %*
echo.
echo ---- fini. Le journal est dans log.txt, les captures dans Screenshots. ----
pause
