Fighting Vipers 2 Recomp (Windows)
==================================

SEGA's 1998 Model 3 arcade game as a native program for your computer: the game's own PowerPC
program, recompiled to native code when it starts, with the rest of the board emulated.

  Fighting Vipers 2  (SEGA, 1998, Model 3 Step 2.0)  MAME's "fvipers2" set, Japan Revision A

No game files are included: no ROMs and nothing made from them. You need your own dump.
An unofficial fan project, not made, approved or supported by SEGA, and not affiliated with it.


Bugs, and anything you would like changed
-----------------------------------------

Please report bugs, or ask for an improvement, by opening an issue on GitHub:

  https://github.com/penpenlovesrei-dotcom/vf3-recomp/issues

Send log.txt from this folder with it. It says what the program found and did, and it is the
one file that usually answers the question.

ONLINE VERSUS HAS NEVER BEEN TESTED. The code is there and the menu will let you try it, but
nobody has yet played a match over the network in this package. Treat it as untested.


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


The modes
---------

Press Escape and the top of the page is the game's own modes. They are the arcade board's, not
this port's: a row hands the game a real press of Start, exactly as choosing a mode on a console
does, and the mode is over when that game is.

  ARCADE          the game as its own cabinet starts it: one player up the roster
  VERSUS          two players -- this row opens a page first (see below)
  RANDOM          the select screen in RANDOM MODE
  SURVIVAL        one opponent after another, on the life you have left
  ONLINE VERSUS   the same match with the second player somewhere else (untested, see above)
  TRAINING        see below

The rows are there for Fighting Vipers 2 Revision A only, and not while anything online is on.

VERSUS opens a page, because a match has a shape before it starts:

  TIME                how long a round is   (the test menu's own ROUND TIME VS)
  ROUNDS              how many to win it    (its MATCH COUNT VS)
  HIDDEN CHARACTERS   Del Sol, Kuhn and B.M. on the select row, or off
  START               the match

Both bytes are the game's own, in the live copy it reads as a match begins, with the test menu's
ranges and steps. They go back to your own operator settings at the next start.

A page of the fourteen arenas comes next, and the match runs in the one you picked. At the end of
it a MATCH OVER panel offers REMATCH, CHANGE STAGE, CHANGE YOUR VIPER and EXIT.


Training
--------

A two-player versus with four things on top:

  - Marble Arena, every time;
  - the clock stopped;
  - both lives held full, so a round never ends;
  - no round announcement: the fight starts at once.

One pad picks both fighters, so you can train alone. A column down the left reads back what you
pressed, with the damage each move did beside it.

Press Start in a training for a page with RESUME and EXIT. EXIT walks the game out to its own
title screen.


The hidden fighters
-------------------

Del Sol, Kuhn and B.M. are on the character select from the first start: push the cursor past
Charlie, the eleventh face. The row then runs 0 to 13 and wraps back to Emi.

The fifteenth cell is left shut on purpose. It is a second Kuhn, the same character under his
old development name and with three times the armour; on the select screen he wears Bahn's
name plate, the drawing routine stopping before his cell. The two bounds the patch moves are
what keeps him out of reach.


Honey's four costumes
---------------------

On the select screen, with the cursor on Honey, Up and Down turn a wheel of four: Sega's two,
and the two without the skirt. The second pair is Sega's own, written into the game and never
seen, because it asks for thirty straight wins with two players in the cabinet -- a pair of the
game's own instructions is what stands in the way, and this port stands them down.


Kuhn's two extra colours
------------------------

The "texture_replacement" folder here holds them, and they are read because they are there.
That folder is a 3D texture pack: the program reads the two folders it asks for by name and no
other, so anything else you keep in there costs nothing.


The picture
-----------

It can be enlarged well past the arcade's 496x384. Wider than 4:3 the 3D's view opens out to
the sides, the same height, rather than stretching the picture.

During a fight the 2D goes with it: the HUD's two bands are cut in three, the life bars and
names pushed out to the window's edges and the clock left exactly where the game drew it. Off
the fight -- menus, the select screen, the endings -- the 2D stays in the middle 4:3, which is
where the game drew it and where it reads right.

The select screen's row of faces can be Imai Toonz's drawings instead of Sega's: Display > Imai
Toonz Portraits. The pictures are in assets\fv2\portraits, sixteen 64x64 cells per sheet, and
they are yours to replace.


The port's own menu
-------------------

Press Escape, or View + Start on a pad, or L3 + R3. It has the modes above, the picture size and
shape, the renderer, the sound volumes, the play style, the arcade settings, online play and the
controls.

Keys:  player 1: arrows, Z guard, X punch, C kick, V escape, 1 start
       player 2: I J K L, B guard, N punch, M kick, comma escape, 2 start
       5 / 6 coins, F2 test, 9 service, Esc the menu, P pause, F3 reset, F5 render scale,
       F6 picture shape, F9 the timings, F11 fullscreen, F12 a screenshot


Where it keeps things
---------------------

In this folder, because portable.txt is here: your settings (settings.ini), the board's saved
memory, screenshots, and log.txt. Each game keeps its own saved memory, so one does not write
over the other's test-menu settings.

log.txt is kept over three starts: log.1.txt, log.2.txt and log.3.txt are the ones before it.


If it does not start
--------------------

- "no set this build knows in ...": the roms folder has no fvipers2.zip it recognises. The sets
  are checked by CRC, so a patched or incomplete set is refused; log.txt names the files that
  did not match.
- A black window, or the 3D missing: the program checks its GPU renderer against its own software
  one when it starts and falls back on its own if they disagree. log.txt says which it used.
- A pad that does nothing: click the game's window once. SDL holds every pad event back until the
  window has the keyboard focus, and Windows does not always give it to a program started from a
  shortcut.
