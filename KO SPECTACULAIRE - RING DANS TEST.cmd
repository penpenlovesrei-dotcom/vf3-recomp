@echo off
rem Fighting Vipers 2, ARBRE ISOLE : la CINEMATIQUE SPECTACULAIRE (SKO) sur
rem TEST, avec le ring Dreamcast ajoute. C'est "RING DANS TEST.cmd" plus
rem VF3_SKO.
rem
rem CE QUE VF3_SKO FAIT. Les angles type replay qu'on voit sur Dreamcast juste
rem apres l'impact ne sont produits ni par le mur ni par l'armure : ils sont
rem produits par la NATURE DU COUP qui finit le round. La fonction de degats
rem (0x00043E84) n'arme la cinematique que si le coup fatal porte deux
rem proprietes a la fois. VF3_SKO neutralise ses deux echappements, donc
rem N'IMPORTE QUEL KO prend desormais le chemin de la cinematique.
rem
rem Ce que ca ne fait PAS : ca ne casse aucun mur, et ca ne cree pas de fin de
rem round. Il faut un vrai KO -- vide la barre de l'adversaire.
rem
rem A SURVEILLER SUR LES ECLATS. La mise a jour du systeme de parois sort
rem immediatement des que fda6 ou l'etape SKO est non nulle : pendant la
rem cinematique, le jeu CESSE d'animer les morceaux. Ils doivent donc se figer,
rem pas disparaitre. S'ils disparaissent, c'est notre dessin qu'il faut revoir.
rem
rem --pc : mode PC -- pas d'ecran d'amorce, acces libre, pas de menu console.
rem
rem LE DECOR N'APPARAIT QU'EN COMBAT : la demonstration d'attract ecrit son
rem decor en dur sans passer par le tireur. Appuie sur START.
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
set VF3_SKO=1
set VF3_SKO_LOG=1
cd /d "%~dp0"
if not exist "build\bin\vf3.exe" (
  echo build\bin\vf3.exe manque. Lance construire-hote.cmd.
  pause
  exit /b 1
)
del log.txt 2>nul
del "%VF3_SCENE%\injection.log" 2>nul
echo Decor force : index 0 = TEST.  Ring Dreamcast AJOUTE.
echo Tout KO prend la cinematique.
echo.
echo Mets la fenetre au premier plan, START, lance un combat, et VIDE LA BARRE
echo de l'adversaire -- c'est le KO qui declenche la cinematique.
echo.
"build\bin\vf3.exe" --pc "%ROM%" %*
echo.
echo ---- la chaine de la cinematique ----
findstr /B /C:"SKO :" log.txt
findstr /C:"VF3_SKO" log.txt
echo.
echo ---- decor et injection ----
findstr /C:"decor" log.txt
if exist "%VF3_SCENE%\injection.log" type "%VF3_SCENE%\injection.log"
echo.
pause
