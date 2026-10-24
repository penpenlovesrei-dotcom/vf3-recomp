@echo off
rem Fighting Vipers 2, ARBRE ISOLE : LA SONDE DE LA CAMERA, sur TOWER.
rem
rem Decor d'ORIGINE, complet, sans la moindre injection, et SANS AUCUN LEVIER :
rem ni VF3_SKO ni VF3_SREP. On regarde ce que fait le jeu, point.
rem
rem CE QU'IL FAUT FAIRE : START, un combat, et EXPLOSER UN MUR.
rem
rem CE QUE LA SONDE RELEVE. Deux choses, et elles ne se valent pas.
rem
rem 1. "CAM : ..." -- EXACT. Une ligne a chaque changement d'etat. Le champ
rem    +0xE2 de la structure de camera n'est pas un compte rendu : c'est
rem    l'INDEX DE REPARTITION que 0x00019078 utilise pour sauter dans la table
rem    de 0x0001AD2C. Le relever, c'est relever la routine qui tourne -- et la
rem    sonde l'imprime sous son nom de Sega (les 41 noms sont en clair dans le
rem    binaire ; mode 0 = FIX, 6 = GAME, 7/8/9 = S_REP_INIT/S_REPLAY/
rem    SR2GAME_INIT, 26/27 = SUPER_KO).
rem
rem    La ligne porte aussi : le compteur de short replays, le combattant que
rem    la camera suit, le mode et le sous-etat, la cause de manche, le masque
rem    des parois, le compteur de morceaux, les deux bits d'armure, et la PORTE
rem    0x00591D14 -- dont le bit 0x04000000 fait sortir la tache du replay des
rem    sa premiere instruction. Si la sonde ecrit "REPLAY COUPE", tout le
rem    systeme est inerte dans la borne et c'est la reponse a tout.
rem
rem 2. "CAM PC : ..." -- ECHANTILLONNE, donc a lire avec prudence. Une prise
rem    du PC par tranche de ligne, soit quelques centaines par image, limitee
rem    a la fenetre 0x0001A000-0x0001F400 (les 41 routines de camera et la
rem    tache du replay) plus 0x0006AFE4-0x0006B180. Une adresse RELEVEE prouve
rem    qu'on y est passe ; une adresse ABSENTE ne prouve rien -- une routine de
rem    vingt instructions passe souvent entre deux prises.
rem
rem NE PAS REDIRIGER LA SORTIE VERS NUL : sous Windows, NUL est un peripherique
rem caractere, donc isatty() y repond vrai et le port n'ecrit PLUS log.txt. Une
rem partie entiere a deja ete perdue comme ca.
rem
rem --pc : mode PC -- pas d'ecran d'amorce, acces libre, pas de menu console.
setlocal
set PATH=C:\msys64\mingw64\bin;%PATH%
set ROM=C:\Users\frede\Documents\SEGA Model 3\ROMs\fvipers2.zip
set "VF3_DATA=%~dp0"
if "%VF3_DATA:~-1%"=="\" set "VF3_DATA=%VF3_DATA:~0,-1%"
set VF3_STAGE=10
set VF3_CAM_LOG=1
cd /d "%~dp0"
if not exist "build\bin\vf3.exe" (
  echo build\bin\vf3.exe manque. Lance construire-hote.cmd.
  pause
  exit /b 1
)
del log.txt 2>nul
echo Decor force : index 10 = TOWER, d'origine, sans injection ni levier.
echo.
echo START, un combat, et EXPLOSE UN MUR.
echo Quitte ensuite : le releve est dans log.txt, et je le lis.
echo.
"build\bin\vf3.exe" --pc "%ROM%" %*
echo.
echo ---- la porte du systeme de replay ----
findstr /C:"REPLAY COUPE" log.txt
echo.
echo ---- les adresses relevees dans le fichier camera/replay ----
findstr /B /C:"CAM PC :" log.txt
echo.
echo (le releve d'etat complet, lignes "CAM :", est dans log.txt)
pause
