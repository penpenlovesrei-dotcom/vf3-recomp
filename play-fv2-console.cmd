@echo off
rem Fighting Vipers 2 the way a console does it: Start opens the list of the
rem seven modes on the game's title screen, as on the Dreamcast version, instead
rem of dropping straight onto the arcade's fixed road.
rem See docs\fv2-console.md.
rem
rem ONE KEY, TWO BEHAVIOURS:
rem   on the game's title screen -> Start opens the list of the seven modes
rem   everywhere else            -> Start brings the game back to its title screen
rem In both cases the game does not see the press.
rem
rem In the list: up and down move, Start (or Enter) chooses, B (or Backspace)
rem leaves, and the chosen line blinks. Escape still opens this port's settings
rem menu, and closes the list on the way.
rem
rem SIX LINES OUT OF SEVEN DO SOMETHING:
rem   ARCADE    the game, as it stands
rem   V.S.      the game with the second player already in it
rem   RANDOM    the game's RANDOM_CHARA flag: the character drawn at random
rem   SURVIVAL  survival mode
rem   TRAINING  for now: the fight in MARBLE ARENA
rem   OPTIONS   a page of its own, over the game's picture (see below)
rem   NETWORK   does not exist on the cabinet and does nothing
rem
rem THE OPTIONS PAGE: the game's thirteen operator settings, the ones from its
rem test menu. Up and down move, LEFT and RIGHT change the value, B leaves.
rem INITIALIZE restores the factory values. NOTHING RESTARTS: each setting is
rem written both into the game's backup, where it lasts, and into the copy the
rem game reads while running, where it takes effect at once. It takes effect at
rem the moment the game reads that setting again -- generally at the start of the
rem next match. COUNTRY is shown in grey: it is read-only.
rem See docs\fv2-console-options.md.
rem
rem NO COIN NEEDED: console mode is on FREE PLAY, like a console. The port sets
rem the game's own COIN/CREDIT setting at start-up (one byte of the NVRAM, see
rem docs\fv2-console.md). The list, for its part, never touches the credit
rem counter: choosing a mode hands the game a real press of Start, and it is the
rem game that decides what it costs -- nothing, here.
rem
rem     1   Start player 1            2   Start player 2
rem     5   a coin, player 1          6   a coin, player 2
rem     F2  Test                      9   Service
rem
rem The same choices are also under Escape, line "Fighting Vipers 2": that is a
rem settings page of this port's, and it is not the list.
rem
rem The setting is kept in settings.ini (console_mode = 1). To go back to PC
rem mode: run play-fv2.cmd once with --pc.
setlocal
set PATH=C:\msys64\mingw64\bin;%PATH%
set ROM=C:\Users\frede\Documents\SEGA Model 3\ROMs\fvipers2.zip
rem This port files what belongs to it -- settings, NVRAM, log, portraits --
rem in the project's folder, the one this launcher sits in. All four launchers
rem pin the same one, or each would have its own.
set "VF3_DATA=%~dp0"
if "%VF3_DATA:~-1%"=="\" set "VF3_DATA=%VF3_DATA:~0,-1%"
cd /d "%~dp0"
if not exist "build\bin\vf3.exe" (
  echo build\bin\vf3.exe is missing: install SDL 3 then run build-host.cmd.
  pause
  exit /b 1
)
rem THE BLACK WINDOW DOES NOT STAY. The game is built with -mwindows: it writes nothing into this
rem console, and its whole log goes to log.txt in the data folder. So the window stayed open, empty
rem and black, for the forty seconds of start-up -- Frederic photographed it on 2026-10-29.
rem `start` launches the game and hands control straight back, so the console closes.
start "" "build\bin\vf3.exe" "%ROM%" --console %*
