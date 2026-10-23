@echo off
rem Fighting Vipers 2, ARBRE ISOLE : pourquoi le sol est blanc dans trois sous-etats.
rem
rem CE QUE CETTE SONDE REPOND, et pourquoi elle est faite comme ca.
rem
rem Rien dans le chemin d'eclairage ne depend de l'etat du jeu pour un maillage
rem a ombrage fixe : notre viewport pose soleil 1, ambiante 0, et efface le
rem brouillard et le projecteur. L'eclairage ne PEUT donc pas blanchir d'un
rem sous-etat a l'autre. Deux hypotheses restent, et une couleur les separe :
rem
rem   VF3_DIAG_ROUGE=1   nos maillages injectes sortent en rouge pur.
rem                      le sol blanc devient rouge -> c'est le notre
rem                      le sol blanc reste blanc   -> il est peint par autre
rem                                                    chose, et notre sol est
rem                                                    absent ou recouvert
rem   VF3_SCENE_LOG=1    le SOUS-ETAT DU COMBAT puis la forme de la scene,
rem                      imprimes seulement quand ils CHANGENT, SOUS LE NOM DE
rem                      SEGA : le mode est en 0x0058FD98 et le sous-etat en
rem                      0x0058FD99, et les quatre-vingts noms sont en clair
rem                      dans la ROM, table de pointeurs en 0x00024494. Les
rem                      trois sous-etats ou le sol est blanc s'y appellent
rem                      ROUND_MASK_*, SET_*/READY_* et REPLAY_* ; le seul ou
rem                      il est correct est FIGHT_*. La ligne porte aussi
rem                      l'index de decor
rem                      (0x0058FD9F), dont depend l'injection, et par viewport
rem                      la priorite, les modeles, le rectangle, le soleil,
rem                      l'ambiante, le projecteur et le brouillard.
rem   VF3_TRANS_AUDIT=1  tout maillage translucide de la scene, avec sa taille
rem                      dans l'espace de la camera : pour trouver celui qui
rem                      couvre l'image.
rem
rem A FAIRE PENDANT L'ESSAI : lancer un combat, laisser passer la presentation
rem des combattants et le debut de round, puis jouer. Les trois sous-etats
rem doivent etre traverses au moins une fois.
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
set VF3_TRANS_AUDIT=1
set VF3_DIAG_ROUGE=1
cd /d "%~dp0"
if not exist "build\bin\vf3.exe" (
  echo build\bin\vf3.exe manque.
  pause
  exit /b 1
)
echo Decor force : index 0 = TEST.  Ring Dreamcast AJOUTE, EN ROUGE.
echo.
echo Lance un combat et traverse la presentation, le debut de round et un replay.
echo.
"build\bin\vf3.exe" --pc "%ROM%" %*
echo.
echo ---- la scene, a chaque changement ----
findstr /C:"scene :" log.txt
echo.
pause
