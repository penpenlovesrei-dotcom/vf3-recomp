@echo off
rem Ce que fait un appui sur Start dans le mode console, mesure sans fenetre :
rem cinq essais, chacun un seul appui, a un endroit different de la partie. Le
rem journal des etats (--modes) est imprime a cote, sous les noms de Sega, donc
rem on voit ce qui a ete decide ET ce que le jeu en a fait.
rem
rem Ecran titre = mode ADV, sous-etat 4 ou 5. Voir docs\fv2-etats.md.
rem Compter deux a trois minutes en tout.
setlocal
set ROM=C:\Users\frede\Documents\SEGA Model 3\ROMs\fvipers2.zip
set RUN=build\bin\vf3run.exe
set SCRIPT=tools\scripts\fv2-console-start.txt
cd /d "%~dp0"
if not exist "%RUN%" (
  echo %RUN% manque : lancer construire.cmd d'abord.
  pause
  exit /b 1
)

echo ######## temoin : aucun appui
"%RUN%" "%ROM%" --frames 1700 --modes

echo.
echo ######## 1 : Start pendant le logo SEGA (image 1100)
"%RUN%" "%ROM%" --frames 1500 --modes --console-start 1100

echo.
echo ######## 2 : Start sur l'ecran titre (image 1255) -- la liste, rien d'ecrit
"%RUN%" "%ROM%" --frames 1500 --modes --console-start 1255

echo.
echo ######## 3 : Start pendant la demonstration (image 1450)
"%RUN%" "%ROM%" --frames 1800 --modes --console-start 1450

echo.
echo ######## 4 : Start sur l'ecran de selection (image 2100)
"%RUN%" "%ROM%" --script "%SCRIPT%" --modes --console-mode arcade --console-at 1960 --console-start 2100 --frames 2600

echo.
echo ######## 5 : Start en plein combat (image 3100)
"%RUN%" "%ROM%" --script "%SCRIPT%" --modes --console-mode arcade --console-at 1960 --console-start 3100 --frames 3400

echo.
echo ---- fini (code %ERRORLEVEL%) ----
pause
