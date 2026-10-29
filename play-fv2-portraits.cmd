@echo off
rem Fighting Vipers 2 with the select screen's portraits replaced by PNGs read
rem from a folder. Nothing is written into the ROM nor into the zip: both sheets
rem are rewritten in the VROM image the process keeps in memory, once, before the
rem board starts. See docs\fv2-portraits.md.
rem
rem WHERE THE FILES GO. This port files what belongs to it -- settings, NVRAM,
rem log, portraits -- in THE PROJECT'S FOLDER, the one this launcher sits in. All
rem four launchers pin the same one, or each would have its own settings and its
rem own NVRAM. Without that pinning the port chooses by itself, and its last
rem resort is "beside the zip" -- so inside the Supermodel tree.
rem
rem SO THE PORTRAITS go into:
rem
rem     <this folder>\portraits-fvipers2\
rem         light\00.png ... 15.png    the light sheet: the cell under the cursor
rem         dark\00.png ... 15.png     the dark sheet: all the others
rem
rem claire\ and sombre\ are accepted as well, and an NN.png put at the root serves
rem for both sheets. A cell with no PNG keeps Sega's. The folder is created empty
rem at the first launch: the images only have to be dropped into it.
rem
rem Each image: exactly 64x64, a non-interlaced PNG (in GIMP, untick the Adam7
rem interlacing), 8 or 16 bits.
rem
rem The cells' order is the SHEET's, which is neither the cursor's nor that of the
rem fight identifiers:
rem   00 BAHN  01 CHARLIE  02 GRACE  03 HONEY  04 JANE  05 MAHLER  06 PICKY
rem   07 RAXEL 08 SANMAN   09 EMI    10 TOKIO  11 to 15 free (blank)
rem
rem To try another folder without moving anything, pass it as an argument:
rem   play-fv2-portraits.cmd "C:\my portraits"
rem To go back to Sega's: play-fv2.cmd once with --no-portraits.
setlocal
set PATH=C:\msys64\mingw64\bin;%PATH%
set ROM=C:\Users\frede\Documents\SEGA Model 3\ROMs\fvipers2.zip
set "VF3_DATA=%~dp0"
if "%VF3_DATA:~-1%"=="\" set "VF3_DATA=%VF3_DATA:~0,-1%"
cd /d "%~dp0"
if not exist "build\bin\vf3.exe" (
  echo build\bin\vf3.exe is missing: install SDL 3 then run build-tools.cmd.
  pause
  exit /b 1
)
if not exist "%VF3_DATA%" mkdir "%VF3_DATA%"
set DOSSIER=%~1
if "%DOSSIER%"=="" (
  echo Portraits : %VF3_DATA%\portraits-fvipers2
  "build\bin\vf3.exe" "%ROM%"
) else (
  echo Portraits : %DOSSIER%
  "build\bin\vf3.exe" "%ROM%" --portraits "%DOSSIER%"
)
