@echo off
rem Virtua Fighter 3 avec les ombres de ce portage : les vrais modeles des
rem combattants, a la place des pieces simplifiees que la ROM projette.
rem Rien dans ce code n'est propre a un jeu -- une ombre est reconnue a sa
rem matrice aplatie, et la direction de la lumiere est lue dans cette matrice.
rem docs/fv2-ombres.md.
rem
rem A regarder, sur le sable : la forme. L'ombre du jeu est une masse ; la
rem notre montre les deux bras et les deux jambes separes, le cone d'une jupe,
rem une manche levee. Et les grains du sol restent lisibles DANS l'ombre, ce
rem que l'ombre opaque du jeu efface. Les combattants s'ombrent aussi
rem eux-memes, du cote oppose a la lumiere.
rem
rem La lumiere de Virtua Fighter 3 est a 45 degres de la verticale : les
rem ombres y sont longues, bien plus que dans Fighting Vipers 2.
rem
rem Les premieres secondes sont l'image du jeu telle quelle : les ombres ne
rem s'allument qu'une fois le controle GPU/processeur passe.
rem
rem ATTENTION : par defaut ce jeu REFUSE les ombres du portage, et c'est
rem voulu. Ses matrices d'ombre ne sont pas des projections planes (residu
rem 0,0696 contre 0,000000 pour Fighting Vipers 2 ; son sol a du relief), donc
rem il n'y a pas de lumiere a y lire. Sans argument, tu verras donc les ombres
rem DU JEU, et l'image est identique a l'octet au jeu sans l'option.
rem
rem   jouer-vf3-ombres.cmd force     passe outre le refus
rem
rem Avec "force", tu verras les ombres du portage -- la forme est juste, les
rem bras et les jambes se separent -- mais la direction bouge de 1,15 degre par
rem image en moyenne et jusqu'a 62 : la lumiere tourne et l'image clignote.
rem C'est ce que le refus ecarte. docs/fv2-ombres.md.
rem
rem VF3_SHADOW_DARK, de 0 a 1, assombrit en plus l'ambiante qu'un pixel a
rem l'ombre garde entiere. 0 : la lumiere du jeu intacte.
setlocal
set PATH=C:\msys64\mingw64\bin;%PATH%
set ROM=%~dp0roms\vf3.zip
if not exist "%ROM%" set ROM=C:\Users\frede\Documents\SEGA Model 3\ROMs\vf3.zip
set "VF3_DATA=%~dp0"
if "%VF3_DATA:~-1%"=="\" set "VF3_DATA=%VF3_DATA:~0,-1%"
set VF3_SHADOWS=1
if /i "%~1"=="force" (set VF3_SHADOW_FORCE=1) else if not "%~1"=="" set VF3_SHADOW_DARK=%~1
if /i "%~1"=="force" if not "%~2"=="" set VF3_SHADOW_DARK=%~2
cd /d "%~dp0"
if not exist "build\bin\vf3.exe" (
  echo build\bin\vf3.exe manque : installer SDL 3 puis lancer construire-hote.cmd.
  pause
  exit /b 1
)
"build\bin\vf3.exe" "%ROM%"
