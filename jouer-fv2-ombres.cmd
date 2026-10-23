@echo off
rem Fighting Vipers 2 avec les ombres de ce portage : les vrais modeles des
rem combattants eclaires par la lumiere du decor, a la place des pieces
rem simplifiees que la ROM projette. docs/fv2-ombres.md.
rem
rem A regarder : la forme de l'ombre au sol (les deux chaussures se
rem distinguent, le BMX de Charlie aussi), l'ombre qui monte sur le corps du
rem cote oppose au soleil, et la texture du sol qui se lit DANS l'ombre --
rem ce que l'ombre opaque du jeu efface. Les premieres secondes sont l'image
rem du jeu telle quelle : les ombres ne s'allument qu'une fois le controle
rem GPU/processeur passe.
rem
rem VF3_SHADOW_DARK, de 0 a 1, assombrit en plus l'ambiante (0,6) qu'un pixel
rem a l'ombre garde entiere. 0 : la lumiere du jeu intacte. ~0,8 : la noirceur
rem de l'ombre du jeu, mais avec la texture conservee.
setlocal
set PATH=C:\msys64\mingw64\bin;%PATH%
set ROM=%~dp0roms\fvipers2.zip
if not exist "%ROM%" set ROM=C:\Users\frede\Documents\SEGA Model 3\ROMs\fvipers2.zip
set "VF3_DATA=%~dp0"
if "%VF3_DATA:~-1%"=="\" set "VF3_DATA=%VF3_DATA:~0,-1%"
set VF3_SHADOWS=1
if not "%~1"=="" set VF3_SHADOW_DARK=%~1
cd /d "%~dp0"
if not exist "build\bin\vf3.exe" (
  echo build\bin\vf3.exe manque : installer SDL 3 puis lancer construire-hote.cmd.
  pause
  exit /b 1
)
"build\bin\vf3.exe" "%ROM%"
