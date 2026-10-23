@echo off
rem Fighting Vipers 2, ARBRE ISOLE : le SHORT REPLAY sur TEST, avec le ring
rem Dreamcast ajoute. C'est CETTE camera-la -- les angles successifs juste
rem apres l'impact, pas celle du super KO.
rem
rem CE QU'IL FAUT FAIRE : CASSER UNE PIECE D'ARMURE. Rien d'autre. Pas de KO,
rem pas de fin de manche, aucun patch.
rem
rem Pourquoi. L'etape 0 de la tache du replay (0x0001EAE4) saute a l'armement
rem des que [r12+0x60] ou [r12+0x64] porte un des bits 0x30, SANS AUCUN SEUIL
rem (0x0001EB44 et 0x0001EB74). Et ces deux mots, c'est l'enregistreur
rem 0x0001E708 qui les remplit :
rem
rem     [r12+0x60] |= [J1 + 0xF44] & 0x30
rem     [r12+0x64] |= [J2 + 0xF44] & 0x30
rem
rem +0xF44 est le mot que le menu de test du jeu nomme "armor_flag", et 0x30 ses
rem deux bits de piece brisee.
rem
rem CE QUE SEGA EN DIT. Les 41 modes de camera ont leurs noms en clair dans le
rem binaire (mode 0 = FIX). Les trois qui comptent sont 7 S_REP_INIT,
rem 8 S_REPLAY et 9 SR2GAME_INIT -- short replay to game. Contrairement a
rem SUPER_KO, qui sort vers VIC_INIT, celui-la REND LA MAIN au combat : il
rem arrive donc en pleine manche.
rem
rem Ce lanceur ne pose AUCUN levier, expres : on regarde ce que fait le jeu.
rem Pour voir la camera sans casser d'armure, prends
rem "SHORT REPLAY - TOWER.cmd", qui porte VF3_SREP.
rem
rem A SURVEILLER SUR LES ECLATS. Les etapes impaires du replay rappellent
rem 0x00027904, l'init des parois -- le jeu remet le mur debout pour le
rem recasser devant la camera. Les morceaux doivent donc repartir a chaque
rem passe.
rem
rem --pc : mode PC -- pas d'ecran d'amorce, acces libre, pas de menu console.
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
cd /d "%~dp0"
if not exist "build\bin\vf3.exe" (
  echo build\bin\vf3.exe manque. Lance construire-hote.cmd.
  pause
  exit /b 1
)
del log.txt 2>nul
del "%VF3_SCENE%\injection.log" 2>nul
echo Decor force : index 0 = TEST.  Ring Dreamcast AJOUTE.
echo Aucun levier : le jeu tel quel.
echo.
echo START, lance un combat, et CASSE UNE PIECE D'ARMURE de l'adversaire.
echo Le short replay part a cet instant-la, et rend la main au combat.
echo.
"build\bin\vf3.exe" --pc "%ROM%" %*
echo.
echo ---- decor et injection ----
findstr /C:"decor" log.txt
if exist "%VF3_SCENE%\injection.log" type "%VF3_SCENE%\injection.log"
echo.
pause
