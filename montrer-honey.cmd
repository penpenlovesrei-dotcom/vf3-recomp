@echo off
rem Refait la mesure de docs\fv2-honey.md : le meme combat avec HONEY, trois
rem fois, et la seule chose qui change entre les trois est ecrite en RAM.
rem Sans fenetre, sur le code recompile ; comptez une dizaine de minutes.
rem
rem   captures-honey\temoin     le jeu tel quel
rem   captures-honey\armure     l'armure basse de Honey detruite
rem   captures-honey\trente     detruite, et trente victoires d'affilee
rem
rem Ce qu'il faut regarder, et c'est tout le sujet : les images 1 a 3 sont du
rem round ou l'armure tombe, les images 4 a 6 du round d'APRES. Pendant le
rem premier round, "armure" et "trente" donnent exactement la meme image.
rem C'est au chargement du round suivant que le jeu choisit le modele, et la
rem les deux se separent : la jupe reste dans "armure", elle a disparu dans
rem "trente".
rem
rem Rien n'est modifie dans le jeu : VF3_POKE ecrit en RAM avant chaque image.
rem L'adresse donnee a VF3_POKE est un index dans le vidage de --ram-at, ou
rem l'octet d'adresse invitee A est a l'index A^3 -- d'ou les octets a
rem l'envers ci-dessous.
setlocal
set ROM=C:\Users\frede\Documents\SEGA Model 3\ROMs\fvipers2.zip
cd /d "%~dp0"
if not exist "build\bin\vf3run.exe" (
  echo build\bin\vf3run.exe manque : lancer construire.cmd d'abord.
  pause
  exit /b 1
)

rem 0x00701444 (flag0, l'armure du joueur 1) : octet de poids faible a 0x0A,
rem soit le bit 1 "l'armure basse a encaisse sa limite" et le bit 3 "elle est
rem tombee". C'est ce second bit que le gestionnaire de tissu de Honey lit.
set ARMURE=701444=10
rem 0x00581438 (32 bits) = 40 : quarante victoires consecutives.
set TRENTE=58143B=0,58143A=0,581439=0,581438=40
set SCRIPT=tools\scripts\fv2-honey-rounds.txt

if exist "captures-honey" rmdir /s /q "captures-honey"
mkdir "captures-honey\temoin"
mkdir "captures-honey\armure"
mkdir "captures-honey\trente"

echo --- temoin : le jeu tel quel ---
"build\bin\vf3run.exe" "%ROM%" --script %SCRIPT% --shots "captures-honey\temoin" --rec --quiet

echo --- armure basse detruite ---
set "VF3_POKE=%ARMURE%"
set VF3_POKE_FRAMES=3200-5700
"build\bin\vf3run.exe" "%ROM%" --script %SCRIPT% --shots "captures-honey\armure" --rec --quiet

echo --- armure basse detruite, et trente victoires d'affilee ---
set "VF3_POKE=%ARMURE%,%TRENTE%"
"build\bin\vf3run.exe" "%ROM%" --script %SCRIPT% --shots "captures-honey\trente" --rec --quiet
set "VF3_POKE="
set "VF3_POKE_FRAMES="

echo.
echo ---- fini (code %ERRORLEVEL%) ----
start "" "%~dp0captures-honey\armure"
start "" "%~dp0captures-honey\trente"
pause
