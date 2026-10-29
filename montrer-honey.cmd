@echo off
rem Redoes docs\fv2-honey.md's measurement: the same fight with HONEY, three
rem times, and the only thing that changes between the three is written in RAM.
rem Without a window, on the recompiled code; allow some ten minutes.
rem
rem   captures-honey\temoin     the game as it stands
rem   captures-honey\armure     Honey's lower armour destroyed
rem   captures-honey\trente     destroyed, and thirty wins in a row
rem
rem What has to be looked at, and it is the whole subject: pictures 1 to 3 are
rem from the round where the armour falls, pictures 4 to 6 from the round AFTER.
rem During the first round, "armure" and "trente" give exactly the same picture.
rem It is when the next round loads that the game chooses the model, and there the
rem two part: the skirt stays in "armure", it has gone in "trente".
rem
rem Nothing is modified in the game: VF3_POKE writes into RAM before each frame.
rem The address given to VF3_POKE is an index into --ram-at's dump, where the
rem guest byte at address A is at index A^3 -- hence the bytes the wrong way round
rem below.
setlocal
set ROM=C:\Users\frede\Documents\SEGA Model 3\ROMs\fvipers2.zip
cd /d "%~dp0"
if not exist "build\bin\vf3run.exe" (
  echo build\bin\vf3run.exe is missing: run construire.cmd first.
  pause
  exit /b 1
)

rem 0x00701444 (flag0, player 1's armour): the low byte at 0x0A, that is bit 1
rem "the lower armour has taken its limit" and bit 3 "it has fallen". It is that
rem second bit that Honey's cloth handler reads.
set ARMURE=701444=10
rem 0x00581438 (32 bits) = 40: forty consecutive wins.
set TRENTE=58143B=0,58143A=0,581439=0,581438=40
set SCRIPT=tools\scripts\fv2-honey-rounds.txt

if exist "captures-honey" rmdir /s /q "captures-honey"
mkdir "captures-honey\temoin"
mkdir "captures-honey\armure"
mkdir "captures-honey\trente"

echo --- control: the game as it stands ---
"build\bin\vf3run.exe" "%ROM%" --script %SCRIPT% --shots "captures-honey\temoin" --rec --quiet

echo --- the lower armour destroyed ---
set "VF3_POKE=%ARMURE%"
set VF3_POKE_FRAMES=3200-5700
"build\bin\vf3run.exe" "%ROM%" --script %SCRIPT% --shots "captures-honey\armure" --rec --quiet

echo --- the lower armour destroyed, and thirty wins in a row ---
set "VF3_POKE=%ARMURE%,%TRENTE%"
"build\bin\vf3run.exe" "%ROM%" --script %SCRIPT% --shots "captures-honey\trente" --rec --quiet
set "VF3_POKE="
set "VF3_POKE_FRAMES="

echo.
echo ---- finished (code %ERRORLEVEL%) ----
start "" "%~dp0captures-honey\armure"
start "" "%~dp0captures-honey\trente"
pause
