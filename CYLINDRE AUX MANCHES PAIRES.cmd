@echo off
rem Fighting Vipers 2, ARBRE ISOLE : VOIR LE CYLINDRE, et son changement a la
rem manche 2. C'est "RING DANS TEST.cmd" plus le journal du cylindre.
rem
rem CE QU'IL Y A A VOIR, ET CE QUE LE CODE DREAMCAST EN DIT.
rem
rem decor_TEST_maj (0x8C048878) est une BRANCHE sur le drapeau de manche
rem [0x8C110FD0], qui est le bit 0 du numero de manche [0x8C1E6354] :
rem
rem   manche IMPAIRE : translation seule, (0, -hauteur, 0).  Le cylindre DESCEND.
rem   manche PAIRE   : deux rotations EN PLUS, puis (0, +hauteur, 0).
rem                      0x8C0BAED0( 14564 )          axe Z, 80,00 degres pile
rem                      0x8C0BAE10( [0x8C288528] )   axe Y, +46 par image
rem                    Le cylindre MONTE, incline de 80 degres, et tourne sur
rem                    lui-meme : 0,2527 degre par image, un tour en 23,75 s.
rem
rem Les deux valeurs sont lues, pas estimees : 14564 est la constante du bassin
rem litteral en 0x8C04890E, et 46 l'increment pose en 0x8C048B26. L'angle est
rem binaire (fsca, donc 65536 = 360 degres).
rem
rem La borne porte le meme compteur de manche en 0x00581456 (0x0000BF90 le pose
rem a 1 au debut du match, 0x0000DD38 l'incremente), et c'est lui que
rem r3d_scene.cpp lit.
rem
rem CE QUE LE JOURNAL DIT. VF3_CYL_LOG ecrit UNE ligne sur la console a chaque
rem changement de manche, de cette forme :
rem
rem   CYL : manche 1  signe +1  periode 500  pas 500  copies 5  d (0, -1, 0)
rem
rem   manche  le numero lu dans la RAM de la borne
rem   signe   +1 aux manches impaires, -1 aux paires
rem
rem Si "manche" passe de 1 a 2, la lecture est bonne et ce qui est a l'ecran est
rem bien ce que le code pose. Si elle reste a 0, c'est la lecture qu'il faut
rem reprendre, pas l'animation.
rem
rem UNE DIFFERENCE CONNUE, pour ne pas la prendre pour un defaut : anime.txt
rem porte "copies 5", donc onze exemplaires en tout. La liste d'affichage
rem Dreamcast en dessine QUINZE. Le compte n'a pas encore ete remis.
rem
rem CE QUI A CHANGE DEPUIS LA PREMIERE VERSION DE CE LANCEUR. Trois corrections,
rem a juger ensemble :
rem
rem   1. L'INCLINAISON. Les trois fonctions de rotation de la Dreamcast
rem      POST-multiplient XMTRX, donc le produit est M . Rz(80) . Ry(angle) . T,
rem      et le port calculait Rz . Ry . T . M -- l'ordre inverse. La rotation
rem      tournait autour de l'origine de la scene au lieu de l'axe propre du
rem      cylindre, ce qui est INVISIBLE aux manches impaires (il n'y a qu'une
rem      translation, et l'axe du cylindre est deja Y) et faux aux paires.
rem
rem   2. LE FLOU. Les sept textures de TEST sont en VQ SANS mipmaps -- 6144
rem      octets pour une 128x128, soit 2048 + 128*128/4 pile. La console
rem      echantillonne donc toujours le niveau 0. On garde la chaine de mips
rem      dans la planche (sans elle, du vide au loin) mais on ne la selectionne
rem      plus : base de LOD a 0xFFFFFF sur les 3840 en-tetes.
rem
rem   3. LE BROUILLARD. La console en a un, le port n'en avait aucun. Couleur
rem      FOG_COL_RAM = 0x00101010, lue dans le banc de registres du PowerVR2
rem      d'une sauvegarde Flycast.
rem
rem      VF3_FOG=<start>,<densite> le regle, VF3_FOG=<densite> ne change que la
rem      densite. Le defaut est 0 et 0,05 : sature au bord de l'arene. La FORME
rem      de la courbe ne se transpose pas -- la Dreamcast lit une table indexee
rem      par 1/W, lineaire en log2(distance), le port est lineaire en distance.
rem      C'est l'oeil qui tranche, pas un calcul. Si c'est trop charge, essaie
rem      0,02 ; si c'est trop faible, 0,10.
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
rem LE RASTERISEUR LOGICIEL, OBLIGATOIRE ICI.
rem
rem L'injection de scene n'existe QUE dans le rasteriseur logiciel :
rem r3d_gpu.cpp ne contient aucune occurrence de injected, inj_tex ni
rem injected_scene. Le drapeau mesh.injected et la seconde planche de textures
rem sont des notions du seul r3d_raster.cpp.
rem
rem Tant que le moteur GPU a echoue a sa sonde, ca ne se voyait pas : le port
rem tombait sur le logiciel et l'injection marchait. Des que Vulkan repasse, la
rem geometrie du jeu se retrouve habillee de la planche du ring -- des textures
rem etrangeres plaquees sur les parois.
rem
rem VF3_3D=cpu force le logiciel pour CE lanceur seulement, sans toucher au
rem settings.ini : les autres lanceurs gardent le GPU, qui fonctionne.
rem C'est plus lent (environ 45 images/s au lieu du plein regime), et c'est le
rem prix a payer tant que le moteur GPU ne connait pas la scene injectee.
set VF3_3D=cpu
set VF3_STAGE=0
set VF3_STAGE_LOG=1
set VF3_SCENE_ADD=0
set VF3_SCENE_LOG=1
set VF3_CYL_LOG=1
cd /d "%~dp0"
if not exist "build\bin\vf3.exe" (
  echo build\bin\vf3.exe manque. Lance construire-hote.cmd.
  pause
  exit /b 1
)
del "%VF3_SCENE%\injection.log" 2>nul
echo Decor force : index 0 = TEST.  Ring Dreamcast AJOUTE.
echo Journal du cylindre : une ligne CYL par changement de manche.
echo.
echo START, lance un combat, puis LAISSE LA MANCHE 1 SE TERMINER et regarde le
echo cylindre a la manche 2 : il doit monter au lieu de descendre, etre incline
echo de 80 degres, et tourner lentement sur lui-meme.
echo.
"build\bin\vf3.exe" --pc "%ROM%" %*
echo.
echo ---- decor et injection ----
findstr /C:"decor" log.txt
if exist "%VF3_SCENE%\injection.log" type "%VF3_SCENE%\injection.log"
echo.
echo ---- les lignes CYL sont au-dessus, sur cette console ----
echo.
pause
