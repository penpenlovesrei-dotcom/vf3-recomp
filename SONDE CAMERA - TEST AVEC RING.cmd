@echo off
rem Fighting Vipers 2, ARBRE ISOLE : LA SONDE DE LA CAMERA, sur TEST avec le
rem ring Dreamcast ajoute. C'est la moitie "a comparer" de la paire.
rem
rem L'AUTRE MOITIE est "SONDE CAMERA - TOWER.cmd" : TOWER d'origine, sans
rem injection. Joue la meme chose des deux cotes -- explose un mur, laisse
rem passer deux manches -- et les deux releves se comparent ligne a ligne.
rem
rem OU SONT LES RELEVES. La sonde ecrit son PROPRE fichier, dans releves\, nomme
rem par decor et horodate :
rem
rem     releves\cam-decor0-<heure>.txt     ce lanceur (TEST)
rem     releves\cam-decor10-<heure>.txt    le temoin (TOWER)
rem
rem Rien ne les efface. C'est le point : la sonde ecrivait avant dans log.txt,
rem que chaque lancement commence par effacer, et le releve du premier mur
rem explose sur TOWER a ete perdu comme ca.
rem
rem CE QUE LA LIGNE PORTE. Le mode de camera sous son nom de Sega (c'est
rem l'index de repartition de 0x00019078, donc la routine qui tourne), le
rem sous-etat, la cause de manche, le masque des parois et le compteur de
rem morceaux, les deux bits d'armure, la porte 0x00591D14, les trois tirages de
rem PROTO2_INIT (position de l'oeil, angle, cadrage), et le NUMERO DE MANCHE
rem avec les deux compteurs de manches gagnees.
rem
rem LE NUMERO DE MANCHE EST LA POUR LE CYLINDRE. Son sens doit s'inverser aux
rem manches paires ; r3d_scene.cpp lit pour ca ram[0x00581456], et le code de
rem la borne est net (ROUND_INT pose 1 en 0x0000BF90, 0x0000DD38 incremente).
rem Si le releve montre "manche 1" puis "manche 2", l'octet est bon et c'est
rem l'animation qu'il faut reprendre ; s'il reste a 0, c'est la lecture.
rem
rem --pc : mode PC -- pas d'ecran d'amorce, acces libre, pas de menu console.
rem
rem LE DECOR N'APPARAIT QU'EN COMBAT : l'attract ecrit son decor en dur.
rem Appuie sur START.
setlocal
set PATH=C:\msys64\mingw64\bin;%PATH%
set ROM=C:\Users\frede\Documents\SEGA Model 3\ROMs\fvipers2.zip
set "VF3_DATA=%~dp0"
if "%VF3_DATA:~-1%"=="\" set "VF3_DATA=%VF3_DATA:~0,-1%"
set "VF3_SCENE=C:\Users\frede\Documents\fv2-arcade-notes\real3d\TEST"
set VF3_STAGE=0
set VF3_STAGE_LOG=1
set VF3_SCENE_ADD=0
set VF3_SCENE_LOG=1
set VF3_CAM_LOG=1
cd /d "%~dp0"
if not exist "build\bin\vf3.exe" (
  echo build\bin\vf3.exe manque. Lance construire-hote.cmd.
  pause
  exit /b 1
)
del "%VF3_SCENE%\injection.log" 2>nul
echo Decor force : index 0 = TEST.  Ring Dreamcast AJOUTE.
echo.
echo START, un combat, EXPLOSE UN MUR, et laisse passer deux manches.
echo Fais ensuite la meme chose avec "SONDE CAMERA - TOWER.cmd".
echo.
"build\bin\vf3.exe" --pc "%ROM%" %*
echo.
echo ---- les releves, a comparer ----
dir /b /o-d releves\cam-decor*.txt
echo.
pause
