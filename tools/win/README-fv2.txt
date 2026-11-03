Fighting Vipers 2 Recomp (Windows)
==================================

SEGA's 1998 Model 3 arcade game as a native program for your computer: the game's own PowerPC
program, recompiled to native code when it starts, with the rest of the board emulated.

  Fighting Vipers 2  (SEGA, 1998, Model 3 Step 2.0)  MAME's "fvipers2" set, Japan Revision A

No game files are included: no ROMs and nothing made from them. You need your own dump.
An unofficial fan project, not made, approved or supported by SEGA, and not affiliated with it.

This is the bare version: the board, the recompiler, the sound and the port's own menu, with
the hidden fighters open and nothing else added on top.


Starting it
-----------

1. Unzip this folder wherever you like.
2. Run FightingVipers2.exe.
3. It asks for your ROM set the first time. Choose fvipers2.zip. It is copied into the "roms"
   folder here, so you only do this once.

You can also put fvipers2.zip in the "roms" folder yourself before the first start, or drop it
on the program, or pass it: FightingVipers2.exe D:\ROMs\fvipers2.zip

The same program runs Virtua Fighter 3 (MAME's "vf3", Revision D) if you put that zip there
instead. With both in the folder it starts Virtua Fighter 3, so keep only the one you want, or
pass the zip you mean on the command line.

You do not need a coin: the game's own COIN setting is put on FREE PLAY and the boot runs
unseen, so Start starts. Game > Play style: Arcade gives the boot and the coin slot back.


The hidden fighters
-------------------

Del Sol, Kuhn and B.M. are on the character select from the first start: push the cursor past
Charlie, the eleventh face. The row then runs 0 to 13 and wraps back to Emi.

The fifteenth cell is left shut on purpose. It is a second Kuhn, the same character under his
old development name and with three times the armour; on the select screen he wears Bahn's
name plate, the drawing routine stopping before his cell. The two bounds the patch moves are
what keeps him out of reach.

Past Charlie the game's own names and portraits in the bottom row are the ones it always had:
that is the game, not the port.


What this version fixes
-----------------------

  The victory screen is quiet. The sound board's DSP had its delay lines reading and writing one
  buffer, and on that screen the loop fed itself and swelled about 24 dB into the limiter. The
  mirror that caused it now goes per game: it stays on for Virtua Fighter 3, where this port
  measured it right.


Where it keeps things
---------------------

In this folder, because portable.txt is here: your settings (settings.ini), the board's saved
memory, screenshots, and log.txt. Each game keeps its own saved memory, so one does not write
over the other's test-menu settings.

log.txt says what the program found and did. It is the thing to send with a bug report.


The port's own menu
-------------------

Press Escape, or View + Start on a pad, or L3 + R3. It has the picture size and shape, the
renderer, the sound volumes, the play style, the arcade settings and the controls.

The picture can be enlarged well past the arcade's 496x384. Wider than 4:3 the 3D's view opens
out to the sides, the same height, rather than stretching the picture; the 2D (the HUD, the
text) stays in the middle 4:3.

Keys:  player 1: arrows, Z guard, X punch, C kick, V escape, 1 start
       player 2: I J K L, B guard, N punch, M kick, comma escape, 2 start
       5 / 6 coins, F2 test, 9 service, Esc the menu, P pause, F3 reset, F5 render scale,
       F6 picture shape, F9 the timings, F11 fullscreen, F12 a screenshot


If it does not start
--------------------

- "no set this build knows in ...": the roms folder has no fvipers2.zip it recognises. The sets
  are checked by CRC, so a patched or incomplete set is refused; log.txt names the files that
  did not match.
- A black window, or the 3D missing: the program checks its GPU renderer against its own software
  one when it starts and falls back on its own if they disagree. log.txt says which it used.
