Fighting Vipers 2 Recomp (Windows)
==================================

SEGA's 1998 Model 3 arcade game as a native program for your computer: the game's own PowerPC
program, recompiled to native code when it starts, with the rest of the board emulated.

  Fighting Vipers 2  (SEGA, 1998, Model 3 Step 2.0)  MAME's "fvipers2" set, Japan Revision A

No game files are included: no ROMs and nothing made from them. You need your own dump.
An unofficial fan project, not made, approved or supported by SEGA, and not affiliated with it.

The game plays the arcade's own road, as the cabinet does.


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

You do not need a coin: the game's own COIN setting is on FREE PLAY. Press Start.


What this version adds
----------------------

  The hidden fighters.  Del Sol, Kuhn, BM and Dural on the character select, ON from the first
                        start: push the cursor past the tenth face. Game > Hidden fighters in
                        the port's menu turns them off again. Past the tenth cell the game's own
                        names and portraits are wrong or missing: that is the game, not the port.

  Kuhn's four colours.  Silver and gold are SEGA's; blue and black are the Dreamcast version's
                        third and fourth, and they are in this folder under
                        texture_replacement. On the character select, Up turns the wheel.

  Honey's four costumes. The game's own two, and the two without the skirt, on the same wheel of
                        Up presses on the character select.

  Online play.          Rollback netplay: the port's menu has an Online page with room codes, or
                        you can play by address. Both players need the same version and the same
                        setting for the hidden fighters -- the game says so if they differ.

  The victory screen is quiet. The sound board's DSP had its delay lines reading and writing one
                        buffer, and on that screen the loop fed itself and swelled about 24 dB.


Where it keeps things
---------------------

In this folder, because portable.txt is here: your settings (settings.ini), the board's saved
memory, screenshots, and log.txt. Each game keeps its own saved memory, so one does not write
over the other's test-menu settings.

log.txt says what the program found and did. It is the thing to send with a bug report.


The port's own menu
-------------------

Press Escape, or View + Start on a pad, or L3 + R3. It has the picture size and shape, the
renderer, the sound volumes, the arcade settings, the controls and the Online page.

The picture can be enlarged well past the arcade's 496x384, and widened to 16:9 or 21:9, which
shows more of the stage to the sides rather than stretching it.


If it does not start
--------------------

- "no set this build knows in ...": the roms folder has no fvipers2.zip it recognises. The sets
  are checked by CRC, so a patched or incomplete set is refused; log.txt names the files that
  did not match.
- A black window, or the 3D missing: the program checks its GPU renderer against its own software
  one when it starts and falls back on its own if they disagree. log.txt says which it used.
