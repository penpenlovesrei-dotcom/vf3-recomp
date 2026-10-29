@echo off
rem PROBE: the crackle Frederic hears on the menus, measured in HIS session.
rem
rem Hidden runs do not reproduce it. Measured there, the device queue holds 2.5 to 3.4 frames with a
rem surface up, every core loaded, and the audio handed over has neither a repeated block nor a sample
rem discontinuity. So whatever it is lives in the real session: a window, vsync, a real device.
rem
rem This is jouer-fv2-console.cmd with two instruments on, and nothing else:
rem   VF3_MENU_SOUND  one line a pass while a surface is up -- sound frames made, their rate, the peak,
rem                   and HOW DEEP THE DEVICE QUEUE IS. A queue falling under one frame is a device
rem                   about to rasp, and that is the figure that would name the fault.
rem   VF3_SOUND_WAV   everything handed to the device, in order, as sound.wav in this folder. It holds
rem                   the menus too, which VF3_RECORD's own sound.wav does not.
rem
rem WHAT TO DO: play until a music is running, open a menu over it -- the list over a title screen that
rem came after a demonstration, or the pause menu in a fight -- and listen at the moment it opens. Say
rem WHEN it crackles: on opening, while it is up, on closing, or all the time.
rem
rem The log is log.txt and the sound is sound.wav, both in this folder. sound.wav grows about ten
rem megabytes a minute: it is a measurement, not a recording to keep.
setlocal
set PATH=C:\msys64\mingw64\bin;%PATH%
set ROM=C:\Users\frede\Documents\SEGA Model 3\ROMs\fvipers2.zip
set "VF3_DATA=%~dp0"
if "%VF3_DATA:~-1%"=="\" set "VF3_DATA=%VF3_DATA:~0,-1%"
set VF3_MENU_SOUND=1
set "VF3_SOUND_WAV=%VF3_DATA%\sound.wav"
cd /d "%~dp0"
rem LA FENETRE NOIRE NE RESTE PAS. Le jeu est bati avec -mwindows : il n ecrit rien dans cette
rem console, et tout son journal va dans log.txt du dossier de donnees. Elle restait donc ouverte,
rem vide et noire, les quarante secondes du demarrage -- Frederic l a photographiee le 2026-10-29.
rem `start` lance le jeu et rend la main tout de suite, donc la console se ferme.
start "" "build\bin\vf3.exe" "%ROM%" --console %*
