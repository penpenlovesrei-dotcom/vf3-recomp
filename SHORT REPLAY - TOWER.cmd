@echo off
rem Fighting Vipers 2, ARBRE ISOLE : le SHORT REPLAY sur TOWER -- un decor
rem d'origine complet, SANS la moindre injection -- et AVEC le levier, pour le
rem voir sans avoir a casser d'armure.
rem
rem CE QUE VF3_SREP FAIT. Deux ecritures, et les deux sont necessaires :
rem
rem   0x00114F50 = 1        une DONNEE. L'etape 0 de la tache du replay
rem                         (0x0001EAE4) n'atteint la branche du bit 0x00800000
rem                         que si la cause de manche vaut 3 (super KO) OU si
rem                         cet octet vaut 1.
rem   0001EBE8  beq -> nop  pour que n'importe quel coup y passe, au lieu des
rem                         seuls coups que Sega a marques dans sa table.
rem
rem Poser le nop seul ne faisait rien du tout : la branche n'etait jamais
rem atteinte en combat. C'est l'erreur qui a coute un essai.
rem
rem Avec ce levier, tout coup tenu [0x00114F44] = dix images arme le replay.
rem
rem EFFET DE BORD, dit clairement : tant que le levier est la, l'etape 0 ne
rem passe plus par le chemin du combat courant, donc CASSER UNE ARMURE N'ARME
rem PLUS le replay. C'est l'un ou l'autre. Pour la voie fidele au jeu -- casser
rem une piece d'armure, sans aucun patch -- prends
rem "SHORT REPLAY - RING DANS TEST.cmd".
rem
rem Les trois modes sont 7 S_REP_INIT, 8 S_REPLAY, 9 SR2GAME_INIT : noms de
rem Sega, en clair dans le binaire.
rem
rem --pc : mode PC -- pas d'ecran d'amorce, acces libre, pas de menu console.
setlocal
set PATH=C:\msys64\mingw64\bin;%PATH%
set ROM=C:\Users\frede\Documents\SEGA Model 3\ROMs\fvipers2.zip
set "VF3_DATA=%~dp0"
if "%VF3_DATA:~-1%"=="\" set "VF3_DATA=%VF3_DATA:~0,-1%"
set VF3_STAGE=10
set VF3_SREP=1
cd /d "%~dp0"
if not exist "build\bin\vf3.exe" (
  echo build\bin\vf3.exe manque. Lance construire-hote.cmd.
  pause
  exit /b 1
)
del log.txt 2>nul
echo Decor force : index 10 = TOWER, d'origine, sans injection.
echo Levier VF3_SREP : tout coup tenu dix images arme le short replay.
echo (Et du coup, casser une armure ne l'arme plus -- voir le commentaire.)
echo.
echo START, lance un combat, et frappe. Pas besoin de KO.
echo.
"build\bin\vf3.exe" --pc "%ROM%" %*
echo.
findstr /C:"VF3_SREP" log.txt
echo.
pause
