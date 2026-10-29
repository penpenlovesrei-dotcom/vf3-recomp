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
rem LA FENETRE NOIRE NE RESTE PAS. Le jeu est bati avec -mwindows : il n ecrit rien dans cette
rem console, et tout son journal va dans log.txt du dossier de donnees. Elle restait donc ouverte,
rem vide et noire, les quarante secondes du demarrage -- Frederic l a photographiee le 2026-10-29.
rem `start` lance le jeu et rend la main tout de suite, donc la console se ferme.
start "" "build\bin\vf3.exe" "%ROM%" --console %*
