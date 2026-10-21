# Fighting Vipers 2: the console way

The port has had two play styles: the PC way, which skips the boot and plays free, and the arcade way, which has
the boot screen, coins and credits. This adds a third for Fighting Vipers 2 -- the console way, which is the PC
way plus a console's own menu over the game. Start during the attract loop picks a mode, the way the Dreamcast
version does, instead of dropping you straight into the arcade's fixed road.

Where it came from: the design, the addresses and the reasoning are the owner of this tree's own earlier work on
the game under Supermodel (`SupermodelSrc\Docs\MenuConsole.md`, and `fv2mod\HANDOFF.md` behind it). None of that
code is here -- it is GPL and was read as documentation. What this port adds is its own implementation, and the
measurements below, which were taken here.


## The decision the rest rests on: never touch the credit

The menu does not read the credit counter and never writes to it. Choosing a mode hands the game a **real press
of Start**, and the game takes a credit, or none, by its own settings.

That is what makes it work on credits and on free play alike, without this port ever having had to find Fighting
Vipers 2's free-play byte -- which it has not (see the note in the commit that added the second game). It also
means opening the menu costs nothing and cancelling costs nothing.

That was true of the menu, and for a while it left the console way broken in a way only playing it showed: the
menu opened, a mode was chosen, and nothing happened. The Start was being handed over correctly; the game was
ignoring it, having no credit. A console has no coin slot, so asking the player for one was not an answer.

Feeding the game coins was tried and is not what a console does either. The answer was to put the board on
**FREE PLAY**, as the PC way already did for Virtua Fighter 3 -- which needed the one thing nobody had: where
this game keeps that setting.

It is one byte, `0x17` of the backup RAM, and it holds the COIN/CREDIT SETTING numbered as Virtua Fighter 3's
is, 1 to 27, with 27 being FREE PLAY. How it was found is in the next section. The console way is the PC way
plus a menu, so it now comes up on free play and a mode costs nothing.


## Free play: one byte, and how it was found

Neither this port nor the Supermodel-side work that preceded it had this. That work says so in as many words --
its console menu worked "without its byte ever having been found", by never deciding about credits at all.

What found it was the game's own test menu, driven from an input script. `tools/vf3run`'s scripts can press
`test` and `service`, which are the two cabinet buttons that menu is worked with, and `--shots` shows where the
cursor is, so the menu can be walked blind and then looked at:

```
TEST MENU             Test opens it, cursor on EXIT
                      Service moves, Test selects -- the screen says so
  9 x Service         down to COIN ASSIGNMENTS (the cursor wraps to the top and descends)
  Test                COIN/CREDIT SETTING #1 -- one coin, one credit
```

Then FREE PLAY was set there by hand, the game left, and the backup RAM compared with a copy taken before.
Seven bytes had moved:

```
0x0000B :  1 ->  4      the other lines of that page
0x0000F :  1 ->  3
0x00013 :  0 ->  6
0x00017 :  1 -> 27      COIN/CREDIT SETTING, #27 being FREE PLAY
0x01077 :  0 ->  2      bookkeeping
0x014CE : 77 -> 84
0x014CF : 228 -> 253
```

Seven bytes is not an answer. The one that matters was picked out by putting the other six back and leaving only
`0x17` at 27: the game still came up in FREE PLAY, and the picture was **byte-identical** to the run with all
seven changed. So there is no checksum to keep and no neighbour that has to agree -- `Model3::coin_offset`.

Two things fell out of this that are worth keeping:

- the numbering is the same as Virtua Fighter 3's, 1 to 27 with 27 being FREE PLAY, although the byte is at the
  other end of the backup RAM. The two games share their test menu's framework, not its addresses.
- `tools/vf3run` **never loads the backup RAM**. It starts every run on a board that has never been set, and
  the game writes its factory settings in the first frames. So nothing about an operator setting can be proved
  with it, and this was all done with the windowed host, which does load it. That cost a measurement before it
  was noticed.


## Where the game is: one comparison

```
in attract  =  [0x00700000] == 0
```

The state of the task Sega's own tables call `entry1`, the select screen's. Measured here through
`tools/scripts/fv2-fight.txt`, as a word:

| | `[0x00700000]` |
|---|---|
| the whole attract loop | `0` |
| the select screen | `5` |
| a fight | `0x2A` |

Only `0` or not is used, and the question it answers is "is a game running". **It is not a screen detector and
must not be made into one.** Which attract screen is up -- warning, logo, title, demo, rankings -- does not
matter, because the menu covers the screen itself.

When in doubt the safe side is "attract": a menu over a demonstration costs nothing, freezing a game nobody is
playing is a fault anyone can see.


## The four addresses, measured here

With `tools/vf3run --ram-at`. A byte of guest address `a` is `ram[a ^ 3]`: RAM here keeps the PowerPC's 32-bit
words in the host's order, and reading that wrong is the easiest way to measure nothing.

| address | what | as the board comes |
|---|---|---|
| `0x00700000` | the `entry1` task's state (a word) | `0` in attract |
| `0x0058145F` | Survival's flag | `0` |
| `0x00120550` | the operator setting Survival needs | `0` |
| `0x00120557` | the region (0 Japan, 1 U.S.A., 2 Export, 3 Asia) | `0`, this being the Japanese revision |

The region byte was tried with `VF3_POKE=120554=2` and held at `2`: the game does not write it back.


## Starting a mode is an injection

Through the same mixing point the input scripts use (`frame_inputs` in `src/host/main.cpp`). An injected press
lasts **8 frames**: a human press of Start was measured at 7 to 12 frames, and the game samples its inputs at
least once a frame, so 8 is indistinguishable from a real one.

The injections go on **last**, after the script's and the player's, because they have to get through on the very
frame the menu is still holding the player's own Start back.

And the press that worked the menu must not reach the game. Controls are handed back **when the player lets go**,
not when the menu closes -- otherwise the button that chose Arcade would still be down on the next frame and
would pick a fighter the instant the select screen appeared.

### What was measured

Two runs of the same script -- two coins at frames 1900 and 1930, and **no Start at all** -- differing only in
whether the console mode chose Arcade at frame 1960:

| | `[0x00700000]` at frame 2150 |
|---|---|
| no console mode | `0` -- still in the attract loop, nothing started |
| console mode, Arcade | `5` -- the select screen |

The control is the point: the coins alone start nothing. It is the injected Start that does.


## Survival: two bytes, and the order matters

`0x0058145F` is the flag, `0x00120550` the operator setting that has to agree with it.

**The flag must not be written during the attract loop.** Set there, it stops the game starting at all: the
injected Start is ignored and no credit is ever taken. So the menu works in three steps:

1. **armed** when it is chosen -- nothing is written, and it waits for a game to actually be running (given up
   after 300 frames rather than guessing);
2. **held** once `[0x00700000]` is no longer `0` -- both bytes written every frame, because nothing says the
   game does not clear them when a fight begins, and the roster patch had to be held for exactly that reason;
3. **put back** when the game ends. Left on, it blocks the next game and Arcade stops working.

Measured, with the same coins-and-no-Start script and Survival chosen at frame 1960, read at frame 2150 with a
game running:

```
0x0058145F = 1      the flag, which was 0
0x00120550 = 1      the operator setting, which was 0
```

and the screen says `SURVIVAL MODE` next to the fighter.


## Start: one button, two jobs

Added the 2026-10-21. In the console way Start is the console's button and the game never sees it. What it does
is decided by one question -- is the game's OWN title screen on screen? -- and `Model3::fv2_on_title` answers
it from the game's state machine (`docs/fv2-etats.md`), which is not the question `fv2_in_attract` answers.

    on the title screen   ->  the list of modes opens over that screen
    anywhere else         ->  the game is asked for its title screen

The decision is `console_play::take_start`, so the windowed host and `vf3run` make it the same way, and
`vf3run --modes --console-start N` measures it without a window. What that measured, with the game's own mode
log beside it, is the table in `docs/fv2-etats.md`: the title screen comes back one frame after the request
from the SEGA logo, from the demonstration, from the select screen and from inside a fight alike.

**But a request does not end a game.** Read back afterwards, the entry1 task is still in its playing state
(0x2A from a fight, 0x2D from the select screen). The title screen is back over a game that is still running,
so `fv2_in_attract` stays false. Leaving a fight properly is the game's own road, and it is not wired.

The press is withheld whichever way it went, and the next one is not read until this one has been let go --
otherwise the button that opened the list would still be down on the frame the list hands the game a Start of
its own, and would pick a fighter the instant the select screen appeared.


## The list of modes is a surface of its own

`src/host/fv2_mode_list.h`, and the thing to say first is what it is NOT: it is not a page of this port's menu.
That was built twice, and rejected twice, and the reason is not a matter of taste -- the menu under Escape is
this port's settings, an RmlUi document with its own backdrop, and a page added to it is a page of settings
whatever is written in it. This draws the owner's own picture of the seven lines straight over the picture the
game made, on the game's real title screen, with nothing else on it.

The picture is `assets/menu-console/title_list.png` (709 x 897, outside the repository), and every number in
the drawing is the measurement written down in `docs/fv2-etats.md`: the seven bands cut in the middles of the
white, each slid sideways to bring its own left-aligned ink to the middle, the block at 0.26157 of the
picture's height and 0.35557 of it tall, the width following the picture's proportions. On top of the
measurement are the two things the owner asked for, kept as factors applied to it rather than as the figures
they work out to: grown by 7/6 about its own middle, lifted by 50/1080.

One version of each line exists -- none of the 45 files is a "selected" variant -- so the chosen line is marked
by what it does rather than by what it is: it blinks, its alpha falling to 0.5 and back over 45 frames, which
is the owner's own answer to the same problem.

**The picture behind goes to half** while the list is up, which is what the Dreamcast version does and what
makes the lines readable over the logo. The figure is measured, not chosen: two captures of that version, the
same screen with its menu and without, give 255 -> 128 on all three channels (median 0.502 over 56000 pixels
outside the menu's own block). This port draws black at alpha 128 over the whole picture, under the list, and
the same measurement on its own two captures gives 0.500.

The game is frozen behind it, as behind this port's menu, and its sound with it: the title screen lasts 192
frames of its own, and a list that let the game walk on would end up over a demonstration. Up and down move;
Start, the pad's A, Enter or the space bar choose; **the pad's B or Backspace back out** and leave the game
where it was; and Escape still opens this port's menu, which closes the list. On the keyboard, choosing and
backing out are the player's own punch and kick bindings, because that is where A and B sit on the pad here.

**The list has no sounds of its own, and that is on purpose for now.** A first attempt at them was taken out
the same day it was written (see the journal in REPRISE.md): it ran the sound board under a frozen game, sent
the MIDI reset behind the game's back and put the music back by hand, and it was put together in three patches
on three reports rather than from a plan. What a second attempt has to answer, before a line of it is written,
is in `docs/fv2-console-son.md`.

ARCADE and SURVIVAL act; the five other lines are drawn and do nothing yet, and leave the list up rather than
pretending something happened. RANDOM and V.S. want the select screen's own timing, which this port has not
measured; NETWORK, TRAINING and OPTIONS have no lever here at all.

### What was measured, and how

Driving something drawn over a frozen game needs hooks that do not go by the game's frames, because those
stop. Two were added to the host, and they are what made this verifiable at all:

```
VF3_QUIT_AFTER_S=N    end the run after N seconds of real time
VF3_KEYS_S=k@sec,...  press key k at that many seconds
```

With those, `capturer-fv2-liste.cmd` takes the three pictures in `build\`, hidden, without a hand on a pad:

| picture | what it shows |
|---|---|
| `liste-1-arcade.png` | the seven lines over the game's own title screen, FREE PLAY still readable under them |
| `liste-2-survival.png` | three presses of down, then Enter: the game starts, and the select screen says SURVIVAL MODE |
| `liste-3-retour-au-titre.png` | Start during the demonstration: the title screen is back |

The second is the one that matters most, because it is end to end: the list opened on the real title screen,
its own cursor moved, the line chosen handed the game a Start of its own, and the game began a match on free
play without a coin.


## Using it

```
vf3.exe fvipers2.zip --console
```

or Escape > Game > Play style, which now goes round three ways: PC, Arcade, Console. The third is only offered
for `fvipers2` -- everything above was measured on the Japan Revision A set, and offering it for another game
would be offering nothing. It is kept in `settings.ini` as `console_mode`; `--pc` puts it back.
`jouer-fv2-console.cmd` is the launcher.

In the console way, Start opens the console's own list of modes on the game's title screen, and brings that
title screen back from anywhere else (above). The same choices are also under Escape as *Fighting Vipers 2*,
as a page of this port's menu -- that page stays where it is, and it is not the list.

`tools/vf3run --console-mode arcade|survival --console-at N` drives the same mode machinery without a menu or a
window, which is how the measurements above were taken.


## What is not done

- **Random and Versus.** The owner's work has both, measured: Random is Start *held* from 30 frames after the
  select screen appears until 90 frames after the player's choice, and Versus is one press of Start player 2,
  30 frames after that screen appears. Both need the select screen's own timing, which this page has not
  measured here, so neither is wired up. The hooks are in `console_play` (`inject_start_p2` is there and
  unused); what is missing is the measurement, not the plumbing.
- **The pause menu.** Start during a fight brings the title screen back rather than opening a console's pause
  menu, and it leaves the game running underneath (above). Here Escape already does everything that menu
  would, so this was left until the modes are finished.
- **The region.** The byte is readable and writable through `Model3::fv2_region`, and holds, but nothing calls
  it yet: it has to be written at the moment the settings have just been loaded from the NVRAM, or the NVRAM
  read overwrites it, and that moment has not been pinned down in this port.
- **The Dreamcast menus' own look.** What is here is this port's menu, with the console's choices in it. The
  owner's work draws its own full-screen artwork; nothing of that is here.


## Verified

- `verifier-vf3.cmd`: Virtua Fighter 3 untouched on all five counts. It has to be -- every call is gated on the
  console way being set *and* the set being `fvipers2`.
- Fighting Vipers 2 without `--console`: unchanged, interpreter against recompiled on
  `tools/scripts/fv2-fight.txt`.
- The two measurements above, each against a control run that differs in one thing only.
