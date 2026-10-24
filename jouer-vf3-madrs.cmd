@echo off
rem Virtua Fighter 3 dans la fenetre, pour ECOUTER le miroir de MADRS d'un cote puis de l'autre.
rem
rem   jouer-vf3-madrs.cmd miroir     le reglage ACTUEL du portage pour VF3
rem   jouer-vf3-madrs.cmd separe     les deux plages separees (ce que MAME fait depuis #16527)
rem
rem Pourquoi ce lanceur existe (docs/fv2-son-programme.md section 5) : mesure du 2026-10-24, le
rem pilote son ecrit 32 zeros dans 0x7C0-0x7FF APRES avoir charge MADRS. Avec le miroir ces zeros
rem ecrasent MADRS, donc 176 adresses de ligne de retard sur VF3 tombent a zero et toutes les lignes
rem du DSP lisent le meme tampon. Le reglage avait ete garde pour VF3 sur la seule LONGUEUR de la
rem reverberation -- et cette longueur venait de l'effondrement.
rem
rem Ce que ce lanceur NE dit pas : lequel des deux est le son de la borne. Il n'y a pas de Model 3
rem ici. C'est une decision d'ecoute, pas une correction.
setlocal
set MODE=%1
if "%MODE%"=="" (
  echo Usage : jouer-vf3-madrs.cmd miroir ^| separe
  pause
  exit /b 1
)
set PATH=C:\msys64\mingw64\bin;%PATH%
set ROM=C:\Users\frede\Documents\SEGA Model 3\ROMs\vf3.zip
set "VF3_DATA=%~dp0"
if "%VF3_DATA:~-1%"=="\" set "VF3_DATA=%VF3_DATA:~0,-1%"
cd /d "%~dp0"

if /i "%MODE%"=="miroir" (
  set VF3_SCSP_MADRS_MIRROR=1
  set VF3_SCSP_MADRS_SEPARATE=
  echo ---- MIROIR : le reglage actuel du portage pour VF3 ----
) else if /i "%MODE%"=="separe" (
  set VF3_SCSP_MADRS_MIRROR=
  set VF3_SCSP_MADRS_SEPARATE=1
  echo ---- SEPARE : les deux plages gardees a part ----
) else (
  echo Mode inconnu : %MODE%. Attendu "miroir" ou "separe".
  pause
  exit /b 1
)

if not exist "build\bin\vf3.exe" (
  echo build\bin\vf3.exe manque : installer SDL 3 puis lancer construire-hote.cmd.
  pause
  exit /b 1
)
if not exist "%ROM%" (
  echo La ROM %ROM% n'est pas la.
  pause
  exit /b 1
)
"build\bin\vf3.exe" "%ROM%" %2 %3 %4 %5
endlocal
