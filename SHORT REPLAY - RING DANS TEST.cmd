@echo off
rem Fighting Vipers 2, ARBRE ISOLE : le SHORT REPLAY sur TEST, avec le ring
rem Dreamcast ajoute. C'est CETTE camera-la -- les angles successifs juste
rem apres l'impact, pas celle du super KO.
rem
rem CE QUE SEGA EN DIT. Les 41 modes de camera ont leurs noms en clair dans le
rem binaire (mode 0 = FIX). Les trois qui comptent sont 7 S_REP_INIT,
rem 8 S_REPLAY et 9 SR2GAME_INIT -- short replay to game. Contrairement a
rem SUPER_KO, qui sort vers VIC_INIT, celui-la REND LA MAIN au combat : il
rem arrive donc en pleine manche.
rem
rem CE QUE LE JEU EXIGE NORMALEMENT. La tache du replay compte les images ou le
rem bit 0x00800000 du mot de proprietes du coup courant est pose. Ce mot est
rem recopie depuis la table des coups (0x000B4368) : c'est Sega qui marque, coup
rem par coup, ceux qui meritent un replay. Au bout de dix images, le replay
rem part : il desarme les quatre taches du combat, rembobine le tampon
rem circulaire de 320 images, puis rejoue l'instant plusieurs fois de suite en
rem remontant une image de moins a chaque passe -- d'ou les angles successifs.
rem
rem CE QUE VF3_SREP FAIT. Il neutralise le test de ce bit (une instruction, en
rem 0x0001EBE8), donc N'IMPORTE QUEL coup tenu dix images arme le replay. Le
rem test de l'autre bit, 0x00002000, est LAISSE en place : il interdit le replay
rem sur certains coups, et le lever aussi rendrait l'observation illisible.
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
set VF3_SREP=1
cd /d "%~dp0"
if not exist "build\bin\vf3.exe" (
  echo build\bin\vf3.exe manque. Lance construire-hote.cmd.
  pause
  exit /b 1
)
del log.txt 2>nul
del "%VF3_SCENE%\injection.log" 2>nul
echo Decor force : index 0 = TEST.  Ring Dreamcast AJOUTE.
echo Tout coup tenu dix images arme le short replay.
echo.
echo START, lance un combat, et frappe. Pas besoin de KO : le replay arrive
echo EN PLEINE MANCHE et rend la main au combat.
echo.
"build\bin\vf3.exe" --pc "%ROM%" %*
echo.
findstr /C:"VF3_SREP" log.txt
echo.
echo ---- decor et injection ----
findstr /C:"decor" log.txt
if exist "%VF3_SCENE%\injection.log" type "%VF3_SCENE%\injection.log"
echo.
pause
