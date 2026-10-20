@echo off
rem Refait la mesure de docs\fv2-honey.md : le meme combat avec HONEY, deux
rem fois, et la seule difference entre les deux est le compteur de victoires
rem consecutives. Sans fenetre, sur l'interpreteur, environ trois minutes.
rem
rem Les deux dossiers d'images sont montres a la fin ; c'est a la lecture des
rem deux qu'on voit ce que le compteur fait a la jupe.
rem
rem   captures-honey\temoin   le jeu tel quel
rem   captures-honey\trente   le compteur force a 40 et le test d'armure saute
rem
rem Les deux ecritures sont faites en RAM avant chaque image par VF3_POKE,
rem rien n'est recompile et le jeu n'est pas modifie. L'adresse donnee a
rem VF3_POKE est un index dans le vidage de --ram-at, ou l'octet d'adresse
rem invitee A est a l'index A^3 -- d'ou les octets a l'envers ci-dessous.
setlocal
set ROM=C:\Users\frede\Documents\SEGA Model 3\ROMs\fvipers2.zip
cd /d "%~dp0"
if not exist "build\bin\vf3run.exe" (
  echo build\bin\vf3run.exe manque : lancer construire.cmd d'abord.
  pause
  exit /b 1
)

rem 0x00057A24 beq -> nop : le test "son armure basse est tombee" saute, sinon
rem il faudrait casser l'armure de Honey a la main dans un combat sans clavier.
set NOP=57A27=96,57A26=0,57A25=0,57A24=0
rem 0x00581438 (32 bits) = 40 : quarante victoires consecutives.
set TRENTE=58143B=0,58143A=0,581439=0,581438=40

if exist "captures-honey" rmdir /s /q "captures-honey"
mkdir "captures-honey\temoin"
mkdir "captures-honey\trente"

echo --- temoin : le jeu tel quel ---
"build\bin\vf3run.exe" "%ROM%" --script tools\scripts\fv2-honey.txt --shots "captures-honey\temoin" --quiet

echo --- trente victoires d'affilee ---
set "VF3_POKE=%NOP%,%TRENTE%"
set VF3_POKE_FRAMES=2400-4500
"build\bin\vf3run.exe" "%ROM%" --script tools\scripts\fv2-honey.txt --shots "captures-honey\trente" --quiet
set "VF3_POKE="
set "VF3_POKE_FRAMES="

echo.
echo ---- fini (code %ERRORLEVEL%) ----
start "" "%~dp0captures-honey\temoin"
start "" "%~dp0captures-honey\trente"
pause
