@echo off
rem Ce que le son coute A L'HOTE, ou la carte tourne sur un fil a elle. Une commande, sans fenetre.
rem
rem   tools\son\mesurer-hote.cmd [secondes]      (120 par defaut)
rem
rem POURQUOI CE N'EST PAS LE CHIFFRE DE vf3run. Dans le lanceur la carte son est DANS la boucle d'image,
rem donc son temps est celui de l'image : 30,5 % sous --rec (docs/scsp_audit.md sect. 4 bis). Dans l'hote elle est
rem sur un fil a elle, une image en retard, et son cout se separe en deux :
rem
rem   le cout CPU    -- le travail du fil, qu'il faut bien faire quelque part ;
rem   le cout d'IMAGE -- le temps que le fil du jeu passe BLOQUE a l'attendre, dans finish_frame. Si c'est
rem                     zero, le son ne coute RIEN a la cadence, quel que soit le CPU qu'il brule.
rem
rem Mesure le 2026-10-26 sur la machine de Frederic : le fil travaille 1,49 ms par image, le jeu l'attend
rem 0,000 ms (maximum 0,016), periode 17,38 ms -- donc 8,5 % d'un coeur et 0,00 % de l'image (sect. 4 ter).
rem
rem CE QUE CETTE MESURE-LA N'ETABLIT PAS, et c'est pourquoi ce script existe : UNE SEULE MACHINE, avec des
rem coeurs a revendre. Sur DEUX COEURS -- un portable AMD, un Steam Deck -- le fil du son est en concurrence
rem avec celui du jeu et celui des images, et l'attente pourrait cesser d'etre nulle. C'est la seule mesure du
rem cahier des charges qui demande une autre machine.
rem
rem SANS FENETRE (VF3_HIDDEN=1) : rien ne s'affiche, aucun son ne sort, la carte son tourne quand meme. Le
rem cadencement reste celui du Model 3 -- la sonde imprime la periode, et c'est a 17,38 ms qu'on voit qu'elle
rem est juste. Les premieres centaines d'images ne comptent pas, le temps que ca se cale.
rem IL MARCHE A PLAT. Pour mesurer sur une autre machine on n'emporte pas le depot : on copie un dossier qui
rem contient vf3.exe, ses DLL, la ROM et ce script, et on clique dessus. Le script cherche l'exe a cote de
rem lui d'abord, puis dans build\bin ; et la ROM a cote de lui, puis au chemin habituel.
setlocal
set SECONDES=%1
if "%SECONDES%"=="" set SECONDES=120

set EXE=
if exist "%~dp0vf3.exe" set EXE=%~dp0vf3.exe
if "%EXE%"=="" if exist "%~dp0..\..\build\bin\vf3.exe" set EXE=%~dp0..\..\build\bin\vf3.exe
if "%EXE%"=="" (
  echo vf3.exe introuvable, ni a cote de ce script ni dans build\bin.
  echo Construire ^(make build/bin/vf3^), ou copier l'exe et ses DLL a cote de ce script.
  goto :fin
)

set ROM=
if exist "%~dp0fvipers2.zip" set ROM=%~dp0fvipers2.zip
if "%ROM%"=="" if exist "C:\Users\frede\Documents\SEGA Model 3\ROMs\fvipers2.zip" set ROM=C:\Users\frede\Documents\SEGA Model 3\ROMs\fvipers2.zip
if "%ROM%"=="" (
  echo fvipers2.zip introuvable, ni a cote de ce script ni dans Documents\SEGA Model 3\ROMs.
  goto :fin
)

rem Le dossier de donnees, pour que le journal atterrisse a un endroit qu'on sait lire. Sans ca il suit le
rem zip, et on le cherche.
set VF3_DATA=%~dp0mesure-hote
if not exist "%VF3_DATA%" mkdir "%VF3_DATA%"

set VF3_HIDDEN=1
set VF3_SND_PROF=1
set VF3_SND_HOST=500
set VF3_QUIT_AFTER_S=%SECONDES%

echo ---- %SECONDES% secondes sans fenetre, patienter ----
"%EXE%" "%ROM%"
echo.
echo ---- ce que le son a coute a l'hote ----
findstr /b "sndhost" "%VF3_DATA%\log.txt"
echo.
echo ---- et le partage interne a la carte ----
findstr /b "sndprof" "%VF3_DATA%\log.txt"
echo.
echo Le journal entier : "%VF3_DATA%\log.txt"
echo.
echo CE QU'IL FAUT LIRE : la colonne "le jeu l'attend". Si elle est a 0,000 le son ne coute rien a la
echo cadence. Et la "periode" doit etre a 17,38 ms une fois calee -- sinon la machine ne tient pas le
echo rythme du Model 3 et le reste de la ligne ne veut plus dire la meme chose.
:fin
endlocal
