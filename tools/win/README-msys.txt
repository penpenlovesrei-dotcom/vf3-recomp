Model 3 Recomp (Windows)
========================

Two SEGA Model 3 arcade games as a native program for your computer: the game's own PowerPC
program, recompiled to native code when it starts, with the rest of the board emulated.

  Virtua Fighter 3   (SEGA, 1996, Model 3 Step 1.0)   MAME's "vf3" set, Revision D
  Fighting Vipers 2  (SEGA, 1998, Model 3 Step 2.0)   MAME's "fvipers2" set, Japan Revision A

No game files are included: no ROMs and nothing made from them. You need your own dump.
An unofficial fan project, not made, approved or supported by SEGA, and not affiliated with it.


Starting it
-----------

1. Unzip this folder wherever you like.
2. Run Model3Recomp.exe.
3. It asks for your ROM set the first time. Choose vf3.zip or fvipers2.zip. It is copied into
   the "roms" folder here, so you only do this once.

You can also put the zip in the "roms" folder yourself before the first start, or drop it on
the program, or pass it: Model3Recomp.exe D:\ROMs\fvipers2.zip

Both games can live in "roms" side by side. With both there it starts Virtua Fighter 3; to play
the other, pass it on the command line or keep only the one you want in the folder.


Where it keeps things
---------------------

In this folder, because portable.txt is here: your settings (settings.ini), the board's saved
memory, screenshots, and log.txt. Each game keeps its own saved memory, so one does not write
over the other's test-menu settings.

log.txt says what the program found and did. It is the thing to send with a bug report.


The menu
--------

Press Escape, or View + Start on a pad, or L3 + R3. It has the picture size and shape, the
renderer, the sound volumes, the arcade settings and the controls.

The picture can be enlarged well past the arcade's 496x384, and widened to 16:9 or 21:9, which
shows more of the stage to the sides rather than stretching it.


If it does not start
--------------------

- "no set this build knows in ...": the roms folder has no vf3.zip or fvipers2.zip it recognises.
  The sets are checked by CRC, so a patched or incomplete set is refused; log.txt names the files
  that did not match.
- A black window, or the 3D missing: the program checks its GPU renderer against its own software
  one when it starts and falls back on its own if they disagree. log.txt says which it used.
