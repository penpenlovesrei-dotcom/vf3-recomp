@echo off
rem Imports the sound board's 68000 program into Ghidra, headless, and analyses it.
rem
rem   tools\sound\import-sound.cmd
rem
rem What it does, and why in this order:
rem   1. extracts epr-20600a.21 from fvipers2.zip and SWAPS every pair of bytes. The ROM's file
rem      stores the LOW byte first -- that is what SoundBoard::Bus::word does (v[a] | v[a+1]<<8) --
rem      where the 68000 is big-endian. Without the swap, the reset vector gives PC 60002001, an odd
rem      address off the board; with it, it gives SSP 00002000 and PC 00600120, and vectors 2 and 3
rem      fall at 6005AC and 6005B0. It is measurement that settles the byte order, not a supposition.
rem   2. imports the image into a Ghidra project as 68000:BE:32:default, based at 0x600000, where the
rem      board puts the program ROM (src/sound/soundboard.h).
rem   3. runs FV2SoundSetup.java BEFORE the analysis, so that it starts from the entry points the
rem      68000 carries itself in its vector table.
rem
rem The project is kept apart from the PowerPC's (FV2.rep): it is neither the same processor nor the
rem same program, and a project open in the interface is locked against the analyser.
setlocal
set GHIDRA=C:\Users\frede\Desktop\Nouveau dossier\ghidra_12.1.2_PUBLIC_20260605\ghidra_12.1.2_PUBLIC
set PROJDIR=C:\Temp\fv2-ghidra
set PROJ=FV2SON
set ROM=C:\Users\frede\Documents\SEGA Model 3\ROMs\fvipers2.zip
cd /d "%~dp0..\.."

if not exist "%GHIDRA%\support\analyzeHeadless.bat" (
  echo Ghidra is not at %GHIDRA%.
  goto :done
)
if not exist "%ROM%" (
  echo The ROM %ROM% is not there.
  goto :done
)
if not exist "build" mkdir build

echo ---- extraction and byte swap ----
py -3 tools\sound\extract-sound.py "%ROM%" build\fv2-sound-68000.bin || goto :done

echo ---- headless import and analysis ----
if not exist "%PROJDIR%" mkdir "%PROJDIR%"
call "%GHIDRA%\support\analyzeHeadless.bat" "%PROJDIR%" %PROJ% ^
  -import "%CD%\build\fv2-sound-68000.bin" ^
  -processor 68000:BE:32:default ^
  -loader BinaryLoader -loader-baseAddr 0x600000 ^
  -scriptPath "%CD%\tools\ghidra" ^
  -preScript FV2SoundSetup.java ^
  -overwrite

echo.
echo ---- finished. Open %PROJDIR%\%PROJ%.gpr in Ghidra to read it. ----
:done
endlocal
