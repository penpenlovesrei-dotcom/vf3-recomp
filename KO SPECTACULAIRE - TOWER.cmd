@echo off
rem Fighting Vipers 2, ARBRE ISOLE : voir la CINEMATIQUE SPECTACULAIRE (SKO).
rem
rem SUR TOWER, un decor d'origine complet, SANS la moindre injection. C'est le
rem temoin : si les angles apparaissent ici, la chaine est bonne et il n'y a
rem plus qu'a la retrouver sur TEST avec le ring.
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
rem VF3_SKO_LOG ecrit dans log.txt une ligne a chaque changement de la chaine :
rem   sous-etat (13 = JUDGE, 14 = JUDGE_DSP), fda6 (l'octet qui arme),
rem   etape (0 a 7, les huit SKO_* de Sega), cause (3 = spectaculaire),
rem   5DD3 des deux joueurs, et qui est tombe.
rem
rem --pc : mode PC -- pas d'ecran d'amorce, acces libre, pas de menu console.
setlocal
set PATH=C:\msys64\mingw64\bin;%PATH%
set ROM=C:\Users\frede\Documents\SEGA Model 3\ROMs\fvipers2.zip
set "VF3_DATA=%~dp0"
if "%VF3_DATA:~-1%"=="\" set "VF3_DATA=%VF3_DATA:~0,-1%"
set VF3_STAGE=10
set VF3_SKO=1
set VF3_SKO_LOG=1
cd /d "%~dp0"
if not exist "build\bin\vf3.exe" (
  echo build\bin\vf3.exe manque. Lance construire-hote.cmd.
  pause
  exit /b 1
)
del log.txt 2>nul
echo Decor force : index 10 = TOWER.  Tout KO prend la cinematique.
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
pause
