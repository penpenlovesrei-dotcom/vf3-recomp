@echo off
rem Fighting Vipers 2, ARBRE ISOLE, avec la scene convertie et l'instrument.
rem
rem Cet arbre est un worktree git a part : il ne partage RIEN avec
rem Desktop\fv2-recomp, ni les sources, ni les objets, ni la NVRAM. L'autre
rem session peut travailler pendant ce temps.
rem
rem Ce que ca doit faire : afficher le decor d'entrainement converti au lieu
rem de la scene du jeu. Si ca ne marche pas, le journal dit pourquoi.
setlocal
set PATH=C:\msys64\mingw64\bin;%PATH%
set ROM=C:\Users\frede\Documents\SEGA Model 3\ROMs\fvipers2.zip
set "VF3_DATA=%~dp0"
if "%VF3_DATA:~-1%"=="\" set "VF3_DATA=%VF3_DATA:~0,-1%"
set "VF3_SCENE=C:\Users\frede\Documents\fv2-arcade-notes\real3d\TEST"
cd /d "%~dp0"
if not exist "build\bin\vf3.exe" (
  echo build\bin\vf3.exe manque.
  pause
  exit /b 1
)
del "%VF3_SCENE%\injection.log" 2>nul
del "%TEMP%\vf3_inject.log" 2>nul
echo Scene  : %VF3_SCENE%
echo Journal: %VF3_SCENE%\injection.log
echo.
echo Laisse tourner quelques secondes, puis ferme la fenetre.
echo.
"build\bin\vf3.exe" "%ROM%" %*
echo.
echo ---- journal d'injection ----
if exist "%VF3_SCENE%\injection.log" (
  type "%VF3_SCENE%\injection.log"
) else (
  if exist "%TEMP%\vf3_inject.log" (
    type "%TEMP%\vf3_inject.log"
  ) else (
    echo AUCUN journal : le crochet n'a jamais ete atteint.
  )
)
echo.
pause
