@echo off
rem PROBE: why the CONTINUE screen does not appear after a fight is lost.
rem
rem It is jouer-fv2-console.cmd with two traces switched on, and nothing else:
rem   VF3_VS_TRACE  one line at every mode change from the judgement on, with what
rem                 the game asks for next and what it says about the match;
rem   VF3_MIDI_LOG  one line per byte sent to the sound board, with the frame and
rem                 the mode -- so when the fight's music is cut, when CONTINUE's
rem                 starts, when GAME OVER's starts.
rem
rem WHAT TO DO: ARCADE mode, lose a fight, let it run to the title screen.
rem The log is log.txt, in this folder.
setlocal
set PATH=C:\msys64\mingw64\bin;%PATH%
set ROM=C:\Users\frede\Documents\SEGA Model 3\ROMs\fvipers2.zip
set "VF3_DATA=%~dp0"
if "%VF3_DATA:~-1%"=="\" set "VF3_DATA=%VF3_DATA:~0,-1%"
set VF3_VS_TRACE=1
set VF3_MIDI_LOG=600
cd /d "%~dp0"
"build\bin\vf3.exe" "%ROM%" --console %*
