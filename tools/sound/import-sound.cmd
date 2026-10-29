@echo off
rem Importe le programme 68000 de la carte son dans Ghidra, sans interface, et l'analyse.
rem
rem   tools\sound\import-sound.cmd
rem
rem Ce que ca fait, et pourquoi dans cet ordre :
rem   1. extrait epr-20600a.21 de fvipers2.zip et ECHANGE chaque paire d'octets. Le fichier de
rem      la ROM stocke l'octet BAS en premier -- c'est ce que fait SoundBoard::Bus::word
rem      (v[a] | v[a+1]<<8) -- alors que le 68000 est big-endian. Sans l'echange, le vecteur de
rem      reset donne PC 60002001, une adresse impaire hors de la carte ; avec, il donne
rem      SSP 00002000 et PC 00600120, et les vecteurs 2 et 3 tombent a 6005AC et 6005B0.
rem      C'est la mesure qui tranche l'ordre des octets, pas une supposition.
rem   2. importe l'image dans un projet Ghidra en 68000:BE:32:default, basee a 0x600000, ou la
rem      carte met la ROM programme (src/sound/soundboard.h).
rem   3. lance FV2SoundSetup.java AVANT l'analyse, pour que celle-ci parte des points d'entree que
rem      le 68000 porte lui-meme dans sa table de vecteurs.
rem
rem Le projet est a part de celui du PowerPC (FV2.rep) : ce n'est ni le meme processeur ni le
rem meme programme, et un projet ouvert dans l'interface est verrouille pour l'analyseur.
setlocal
set GHIDRA=C:\Users\frede\Desktop\Nouveau dossier\ghidra_12.1.2_PUBLIC_20260605\ghidra_12.1.2_PUBLIC
set PROJDIR=C:\Temp\fv2-ghidra
set PROJ=FV2SON
set ROM=C:\Users\frede\Documents\SEGA Model 3\ROMs\fvipers2.zip
cd /d "%~dp0..\.."

if not exist "%GHIDRA%\support\analyzeHeadless.bat" (
  echo Ghidra n'est pas a %GHIDRA%.
  goto :fin
)
if not exist "%ROM%" (
  echo La ROM %ROM% n'est pas la.
  goto :fin
)
if not exist "build" mkdir build

echo ---- extraction et echange des octets ----
py -3 tools\sound\extract-sound.py "%ROM%" build\fv2-son-68000.bin || goto :fin

echo ---- import et analyse sans interface ----
if not exist "%PROJDIR%" mkdir "%PROJDIR%"
call "%GHIDRA%\support\analyzeHeadless.bat" "%PROJDIR%" %PROJ% ^
  -import "%CD%\build\fv2-son-68000.bin" ^
  -processor 68000:BE:32:default ^
  -loader BinaryLoader -loader-baseAddr 0x600000 ^
  -scriptPath "%CD%\tools\ghidra" ^
  -preScript FV2SoundSetup.java ^
  -overwrite

echo.
echo ---- fini. Ouvrir %PROJDIR%\%PROJ%.gpr dans Ghidra pour le lire. ----
:fin
endlocal
