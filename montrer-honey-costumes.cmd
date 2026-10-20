@echo off
rem Les quatre Honey, cote a cote. Sans fenetre ; comptez un quart d'heure.
rem
rem   captures-honey\couleur1              valide avec un bouton d'attaque
rem   captures-honey\couleur1-sans-jupe    le meme, sans la jupe
rem   captures-honey\couleur2              valide en tenant HAUT
rem   captures-honey\couleur2-sans-jupe    le meme, sans la jupe
rem
rem Le choix de costume est celui du jeu, et il tient au MANCHE, pas au bouton :
rem mesure, punch, kick, guard et escape donnent tous *1P COLOR = 0, et haut ou
rem bas avec n'importe lequel d'entre eux donne 1. Voir docs\fv2-honey.md.
rem
rem Les deux sans-jupe sont obtenues en ecrivant en RAM, rien n'est modifie dans
rem le jeu. Et comme le modele est choisi au chargement d'un round, les trois
rem premieres images de chaque dossier (le round ou l'armure tombe) sont
rem identiques deux a deux ; ce sont les trois dernieres, le round d'apres, qui
rem se separent.
setlocal
set ROM=C:\Users\frede\Documents\SEGA Model 3\ROMs\fvipers2.zip
cd /d "%~dp0"
if not exist "build\bin\vf3run.exe" (
  echo build\bin\vf3run.exe manque : lancer construire.cmd d'abord.
  pause
  exit /b 1
)

rem 0x00701444 octet de poids faible a 0x0A : l'armure basse a encaisse sa
rem limite (bit 1) et elle est tombee (bit 3). 0x00581438 = 40 victoires.
set ARMURE=701444=10
set TRENTE=58143B=0,58143A=0,581439=0,581438=40
set C1=tools\scripts\fv2-honey-rounds.txt
set C2=tools\scripts\fv2-honey-rounds-c2.txt

if exist "captures-honey" rmdir /s /q "captures-honey"
mkdir "captures-honey\couleur1"
mkdir "captures-honey\couleur1-sans-jupe"
mkdir "captures-honey\couleur2"
mkdir "captures-honey\couleur2-sans-jupe"

set VF3_POKE_FRAMES=3200-5700

echo --- couleur 1 ---
set "VF3_POKE=%ARMURE%"
"build\bin\vf3run.exe" "%ROM%" --script %C1% --shots "captures-honey\couleur1" --rec --quiet

echo --- couleur 1, sans jupe ---
set "VF3_POKE=%ARMURE%,%TRENTE%"
"build\bin\vf3run.exe" "%ROM%" --script %C1% --shots "captures-honey\couleur1-sans-jupe" --rec --quiet

echo --- couleur 2 ---
set "VF3_POKE=%ARMURE%"
"build\bin\vf3run.exe" "%ROM%" --script %C2% --shots "captures-honey\couleur2" --rec --quiet

echo --- couleur 2, sans jupe ---
set "VF3_POKE=%ARMURE%,%TRENTE%"
"build\bin\vf3run.exe" "%ROM%" --script %C2% --shots "captures-honey\couleur2-sans-jupe" --rec --quiet

set "VF3_POKE="
set "VF3_POKE_FRAMES="
echo.
echo ---- fini (code %ERRORLEVEL%) ----
start "" "%~dp0captures-honey"
pause
