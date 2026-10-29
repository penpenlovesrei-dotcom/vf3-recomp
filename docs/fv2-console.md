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
    elsewhere in the attract loop  ->  the game is asked for its title screen
    while a game is running        ->  NOTHING, and the press goes to the game

The decision is `console_play::take_start`, so the windowed host and `vf3run` make it the same way, and
`vf3run --modes --console-start N` measures it without a window. What that measured, with the game's own mode
log beside it, is the table in `docs/fv2-etats.md`: the title screen comes back one frame after the request
from the SEGA logo, from the demonstration, from the select screen and from inside a fight alike.

**But a request does not end a game.** Read back afterwards, the entry1 task is still in its playing state
(0x2A from a fight, 0x2D from the select screen). The title screen is back over a game that is still running,
so `fv2_in_attract` stays false. Leaving a fight properly is the game's own road, and it is not wired.

### And what that gave on screen, measured on 2026-10-23

This paragraph had been written since the 21st without anybody having looked at the result. Frédéric found it
in versus: **Start mid-fight, and the title screen comes back torn.** The tile layers hashed into one another,
the roster's names printed through the logo, the whole picture in stripes. Reproduced without a window on the
route `tools/scripts/fv2-console-vs-start.txt` — frame 3001 of a versus fight, the mode goes from
`GAME_DSP / FIGHT_DSP` to `ADV_DSP / FV2_TITLE_DSP` and the photograph is unreadable.

**So nothing is asked for from inside a game any more.** There is no good screen to ask for from inside a
fight: the request moves the screen without stopping the game, and it is that superposition that tears the
picture. The rule becomes `fv2_in_attract` — outside the attract, this layer does nothing.

**And it does not withhold the press either**: it goes to the game like the cabinet's Start, which is what
lets a second player invite himself into a match in progress. Verified: at the same frame 3001, the fight goes
on, the picture is intact, and the match runs to its `NEXT_PROG_SEL` and its next round.

What remains unresolved is what already was: **leaving a fight properly**, by the game's own road.

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

ARCADE, SURVIVAL, **RANDOM** and **V.S.** act, and that is every mode the arcade game has. **TRAINING** acts
too, as far as it has been built: it fights in Marble Arena. NETWORK and OPTIONS are drawn and leave the list
up rather than pretending something happened -- and they are not waiting for a lever to be found, because
there is none to find.

### TRAINING's arena, and the lever that is NOT the one from August

There is no training mode on this board, so there is nothing to switch on: it is a mode this port builds, and
what exists of it today is which arena it fights in. Frédéric's choice for now is **MARBLE ARENA**, scene 3.

The first lever tried was the stage picker at `0x0007111C`, the one the owner's `CHANGE STAGE` patches to
`li r3,<scene> ; blr`. **It does not command this.** Patched and held -- the words read back out of the
running game -- the game picked stage 9 all the same. That picker is reached only by a rematch going THROUGH
the select screen, which is how `CHANGE STAGE` uses it.

What commands a game's first arena is the game's own **route table**, found by watching who writes the scene
index and reading the dozen instructions above that write (`docs/fv2-etats.md`): eight bytes a match, four
(arena, opponent) pairs, the pair chosen by `0x0058145D`. One byte of it -- `0x000B4100`, route 0, first
match -- set to 3 puts the fight in Marble Arena, and it is put back when the game ends.

Measured both ways: the scene index reads 3 where it read 9, and the picture is the marble floor instead of
the dam.

### NETWORK, TRAINING and OPTIONS do not exist on the arcade board

This is not a gap in what has been read: it is what the game is. Sega's own twenty-four modes
(`docs/fv2-etats.md`) are WARNING, ADV, SEL, GAME, CONTINUE, INTRUDE, ENDING, NAME, RANKING, OVER, TEST and
RESULT. No training, no network, no options. And the owner's arcade notes say it in as many words:

> Versus does not exist as a mode of its own: in Fighting Vipers 2 it is the arcade mode with a second player
> inviting himself in. There is no Training mode.

Which also says that V.S., as this port does it -- a second player's Start into an arcade game -- is not an
approximation of a Versus mode. It **is** what Versus is on this board.

So the three lines are the Dreamcast's, and each would have to be BUILT rather than switched on:

| line | what the arcade actually has |
|---|---|
| `NETWORK` | nothing. The disc's `INTERNET` and `RANDOM NETWORK` have no counterpart in this program |
| `TRAINING` | a **stage**, not a mode. A training mode is this port's own, and its first half is built: it fights in **MARBLE ARENA**, by one byte of the game's route table (below). The rest -- a timer that does not run out, health that comes back, the dummy -- is still to be read |
| `OPTIONS` | the operator's own page, `GAME ASSIGNMENTS` at `0x00120540`-`0x0012055F`. This port already reads and writes that block for Virtua Fighter 3's Arcade settings page, so what is missing is the page, not the mechanism |

That settles what the list can be. It is **complete for the game underneath it**; what is left is work this
port would do itself, and it should be decided as such rather than listed as a hole.

### V.S., and the second player who had never been got in

The lever is the owner's, recorded in August: **one press of Start player 2, 30 frames after the select screen
appears**. Nothing else, and nothing written to the game's memory. What this port was missing was not the
lever but the two moments: the select screen appearing is the mode becoming SEL (4 or 5), and the thirty
frames clear the transition and the Start that opened the screen.

Measured: the select screen comes up with **two players on it**, 1P and 2P with their own cursors, and the
player-2 task `0x00700100` reads 5 -- the select screen's value -- where it reads 0 in every run before this
one. That closes a hole open since August: `docs/fv2-menu-console-inventaire.md` called the second player a
harness gap, not a knowledge one, and said it needed a coin and a Start at the right moment. It did.

### RANDOM: the game's own flag, after the hand manoeuvre refused to work

The lever recorded in August is Start **held** from 30 frames after the select screen until 90 past the
player's choice. Held here over exactly that window, it does **nothing at all**: the game's whole memory at
the frame of the choice is byte-identical to a run that held nothing, all 8 MB. And the control says it is
not this port's doing -- an input script holding Start over the same window gives the same zero. The hold
reaches the game, and the game ignores it.

So the question was asked the other way round, which is what the decompilation is for: **what does the game
set when it goes into that mode?** Section 5 of `docs/fv2-flux-de-match.md` lists the variables under Sega's
own names, and there it is, one line below Survival's flag:

```
0x0058145E   MODE_INFO   RANDOM_CHARA
```

Set, the select screen prints RANDOM MODE beside the fighter -- the same thing the hand manoeuvre produced
over there. The hold was a way of asking the game to set that byte; asking for the byte is shorter, and it is
measurable.

Two measurements decide how it is written, and both are the opposite of Survival's:

| | |
|---|---|
| does it have to be held? | **no** -- written once at frame 2000, RANDOM MODE is still up a hundred frames later |
| does writing it in the attract loop stop the game starting? | **no** -- written from frame 1800 the game still starts (`GAME_INT` at 2296), where Survival's flag there stops it dead |

It is armed on choosing and written once a game is running all the same: that shape costs nothing, it is the
one Survival already uses, and it is what gives a moment to put the flag back when the game ends.

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
- **The pause menu** — **built on 2026-10-24**, the modes being finished. See below.
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

## The pause menu

`src/host/fv2_pause.h`, built on 2026-10-24. Start during a match opens it over the fight, the game frozen
behind it; Start closes it. It is this port's third surface, after the list of modes and the OPTIONS page, and
it is made like them: it freezes the game, takes the keyboard and the pad, tells the host alone what it wants
to hear (`want_sound`), and knows nothing of RmlUi.

### The shape is the disc's

Frédéric showed the Dreamcast's screen, and it is the one followed, not a page of centred lines:

- a translucent purple **panel** under a **pale bar** carrying `1P PAUSE`, with a light upright along its left
  edge;
- **on the left for player 1, on the right for player 2** — the panel sits on the side of whoever paused, and
  the host knows which from the Start that arrived;
- **the screen darkened** behind, and the fight still readable under it;
- the lines **left-aligned** in the panel, the chosen one in yellow;
- the two hints **apart at the bottom**, left and right, as over there.

Every measurement is in fractions of the game's 4:3 screen, as everywhere else here, so the panel keeps its
place whatever the window.

### The four lines, and what they lack

They are August's (`docs/fv2-menu-console-inventaire.md` §2). Only one has a lever that works from here; the
three others are **drawn, quiet and stepped over**, the rule the OPTIONS page already follows.

| line | state |
|---|---|
| `TITLE SCREEN` | **measured inoperative from a fight.** August's lever is the one this port calls `fv2_attract_to_title` — pushing the attract sequencer to the end of its list, its timer expired. Tried from a fight: nothing. The sequencer is not running there, there is no list to leave; the round ended and the next one began |
| `CHANGE YOUR VIPER` | August's note gives three **addresses** — `0x00581455`, `0x00581406`, `0x0058FD56` — and not the three values |
| `REMATCH` | the note describes what was observed, not a lever |
| `CHANGE STAGE` | **wired**: the stage picker TRAINING already uses, held on the chosen scene. Left and right change the number on the line |

What blocks the three is the same thing `docs/fv2-etats.md` has been calling **leaving a fight properly**, by
the game's own road, from the start.

### Verified

In a hidden session, on `tools/scripts/fv2-console-vs-start.txt`: Start at frame 3000 of a versus fight
(`GAME_DSP`, `FIGHT_DSP`) opens the panel, the arrows do step over the lines with no lever, Start closes it,
and the fight resumes intact. The photographs are taken by the clock and not by the frame, the game being
frozen — a frame that does not advance never triggers a capture keyed to it.

### Start has a fourth fate: cutting the state short, in the presentation and the replay

Frédéric, on 2026-10-24: "the pause key during the fight's presentation state and during the replay should not
open the pause menu but just cut the current state short as in the original game".

The states concerned, by the names Sega left (`docs/fv2-etats.md`, GAME's twenty sub-states):

```
 0 ROUND_MASK_INT   1 ROUND_MASK_IN   2 ROUND_MASK_DSP   3 ROUND_MASK_OUT    the STAGE n / VS name screen
 4 ROUND_INT        5 ROUND_DSP                                              the round's announcement
 6 SET_INT          7 SET_INT_POSE    8 SET_DSP                              the scenery and the fighters placed
 9 READY_INT       10 READY_DSP                                              "ready"
15 REPLAY_INT      16 REPLAY_DSP                                             the replay
```

`FIGHT_DSP`, `JUDGE` and `VIC` are **not** there: the pause menu belongs to them.

**What does not work, and it is the first thing measured.** Letting the press through to the game does
**nothing at all**. A real Start at frame 2360 of `tools/scripts/fv2-vic-banc.txt`, during `ROUND_MASK_DSP`
which runs from 2319 to 2450: the trace of the states is identical, frame by frame, with and without. The game
does not read that button there. So the hypothesis "it is enough to stop intercepting" is false, and it was
ruled out by measurement before a line of correction was written.

**What works, and it is the game's own road.** The sub-state handlers hand over to one another through a
**sub-state request** — `docs/fv2-flux-de-match.md` says so of `VIC_INT`: "simply asks for the next sub mode".
A single-frame write of `SUBMODE_REQUEST` (`0x0058FD9B`) to 3 during `ROUND_MASK_DSP`:

```
  2319  GAME_DSP   2 ROUND_MASK_DSP
  2361  GAME_DSP   3 ROUND_MASK_OUT      <- instead of 2450
  2362  GAME_DSP   4 ROUND_INT
  2364  GAME_DSP   6 SET_INT
  2366  GAME_DSP   8 SET_DSP
```

89 frames cut, and what follows runs on normally. That is `Model3::fv2_cut_submode`: the current sub-state
plus one, written **once only**, the mode left alone. And it is a correction to the note that goes with it:
the mode request "commands nothing from inside a fight", measured in `FIGHT_DSP` — but during the
presentation, it is read.

Wired into `console_play::take_start`, which therefore now has **four** fates instead of three: `list` on the
title screen, `cut` in those twelve sub-states, `pause` everywhere else in a game, `title` in the attract.

**Measured**: `vf3run --console-start 2360`, the trace above, and the log line "Start in the presentation or
the replay (mode 7, sub-state 2) -> the state is cut short".
**Not measured yet**: the replay itself (sub-state 16). The lever is the same and the table is the same, but
no out-of-window route reaches `REPLAY_DSP` for now — `fv2-vic-banc.txt` stops in `JUDGE_DSP`. So it is to be
checked by playing, and it is written here so that it does not pass for settled.

### A stage choice for VERSUS: the route table does not command it

Frédéric, on 2026-10-24: "in the work on the rom this summer, a stage select had been brought in for versus
mode". Before writing a page, measured — and the measurement says no, plainly.

The stage's two known levers, and what each one commands:

| lever | what it commands | measured |
|---|---|---|
| the picker `0x0007111C`, patched to `li r3,<scene> ; blr` | nothing for a match that starts: it is reached **only by a rematch going through the select screen**, and that is how August's `CHANGE STAGE` uses it | 2026-10: patched and held, the game picked stage 9 all the same |
| the route table, `0x000B4100` | the stage of the first fight **of a solo route** | the byte at 3 puts the fight in MARBLE ARENA, the scene index reads 3 where it read 9 |

**And it does not command a versus match.** On `tools/scripts/fv2-vic-banc.txt`, a versus match carried
through to the fight (`GAME_DSP`, `FIGHT_DSP`):

| | scene index `0x0058FD9F` | route byte `0x000B4100` |
|---|---|---|
| writing nothing | **8** | 09 |
| the byte held at 3 from frames 1990 to 2900 | **8** | 3 (the write did hold, read back in the dump) |

Two facts add up: with no write, the scene index (8) **already diverges** from the table's first byte (09),
where in solo play the two agreed; and held at 3 throughout the match's set-up — which begins at
`ROUND_MASK_INT`, frame 2316 — the stage does not move. Versus picks its stage elsewhere.

**Where, probably: in the states the game skips.** The select's six sub-states (`docs/fv2-etats.md`) are
`0 SEL_START  1 SELECT  2 STAGE_SEL_INT  3 STAGE_SEL_DSP  4 DECIDE_INT  5 DECIDE_DSP`. **The game has a stage
choice of its own**, and the versus trace shows it stepping over it:

```
  1963  SEL_DSP   0 SEL_START
  2035  SEL_DSP   1 SELECT
  2305  SEL_DSP   4 DECIDE_INT      <- 2 STAGE_SEL_INT and 3 STAGE_SEL_DSP skipped
  2306  SEL_DSP   5 DECIDE_DSP
  2316  GAME_INT  0 ROUND_MASK_INT
```

That is very probably what the summer's work "brought in": not a page to be drawn, but making the game
**enter `STAGE_SEL`**, which exists already and has pictures of its own. What remains to be found is what
decides, on the way out of `SELECT`, to go to 2 rather than to 4 — a flag, an operator setting, or the number
of players. That is a reading of `fv2_sel_SELECT` in Ghidra, not a measurement.

**A tooling trap met here, and it cost two runs**: in `VF3_POKE`, a leading `b` on an address means "the
backup RAM" (`tools/vf3run.cpp`, `const bool bk = *q == 'b'`). So `VF3_POKE=b4103=3` does **not** write at
index `0x0B4103` of the RAM: it writes into the backup at offset `0x4103`, without saying so. Addresses whose
first hexadecimal digit is `b` are written with a leading zero: `0b4103`.

#### Read in the code: `STAGE_SEL` is an EMPTY STUB on the cabinet

Ghidra, on 2026-10-24, and it closes the question. The mode table is at `0x00024354`; its index 5, `SEL_DSP`,
gives `0x0000BA74`, the select's dispatcher. It does three things:

```
0000ba88  lbz r6, 0x0058FD9B      SUBMODE_REQUEST
0000ba94  stb r6, 0x0058FD99      MODE_SUB = the request     <- it copies it every round
0000bac8  addi r2, r2, 0x4404     the select's handler table, 0x00024404
0000bad4  bctr                    it jumps into the sub-handler
```

(The second line is the direct proof of what `Model3::fv2_cut_submode` rests on: the dispatcher copies the
request into the sub-state, so writing the request is enough.)

And the table `0x00024404`:

| index | sub-state | handler |
|---|---|---|
| 0 | `SEL_START` | `0x0000BADC` |
| 1 | `SELECT` | `0x0000BB10` |
| 2 | `STAGE_SEL_INT` | **`0x0000BBB4`** |
| 3 | `STAGE_SEL_DSP` | **`0x0000BBB4`** — the same address |
| 4 | `DECIDE_INT` | `0x0000BBB8` — four bytes further on |
| 5 | `DECIDE_DSP` | `0x0000BC1C` |

And those four bytes are:

```
0000bbb4   b 0x0000bad8        the dispatcher's common exit
```

**The stage choice's two states are an empty stub.** A single instruction, shared, that only returns. The
cabinet never had a stage choice: Sega left both names in its debugging table and both slots in the handler
table, and nothing inside them.

So the summer's work did not "reach" a screen that existed: one had to be **written**, or something else
diverted — which explains why its `CHANGE STAGE` ends up on the picker `0x0007111C` by the rematch path, the
only place where the game asks for a stage again. And that doubles the first measurement: neither the route
table (measured here) nor the `STAGE_SEL` states (read here) give a stage choice in versus.

For the record, while we were there: `DECIDE_INT` ends with `li r0,5 ; stb r0,0x0058FD9B` — it asks for
`DECIDE_DSP` — and `DECIDE_DSP` reads `0x00581892` and, if it is zero, writes `6` into `MODE_REQUEST`
(`0x0058FD9A`), which is the move to GAME.

### The stage choice page: parking the game, not freezing it (2026-10-25)

The page exists, it opens after the fighters are chosen, and its first version **froze** the game like this
port's three other surfaces. Frédéric tried it on 2026-10-25 and noted three flaws that are only one:

> "the characters disappear, the music stops, no sound effect when moving or confirming the stage"

A frozen game draws nothing new any more: the select screen it had just made emptied of its two fighters, its
music stopped for want of a sound board being run, and the sound effects — which go through that same board —
had nobody to play them.

**August's method says the opposite, and it is in his own `.cmd`** (`FV2 - Choix du decor.cmd`,
`C:\Users\frede\Documents\SEGA Model 3`): hold `S-MODE_REQUEST` (`0x0058FD9B`) at **2**, that is to say park
the game in `STAGE_SEL_INT` — the state whose name Sega kept and whose body it emptied (read above: a single
instruction, `b 0x0000bad8`). The game runs its whole frame there, its drawing tasks included, and advances
nothing. "So the frozen screen is not a pause: it is the game itself in slow motion."

That is what `Model3::fv2_park_stage_select` does now:

| | frozen (the first version) | parked (this one) |
|---|---|---|
| the PowerPC | stopped | runs |
| the chosen fighters | disappear | stay drawn by `entry1` |
| the music | stops | goes on |
| the carousel's sound effects | silent | played by the board, as in play |
| the pads | taken by the surface | taken by the surface (cleared before they reach the game) |

**And it is the request that is read, not the state.** `SELECT` writes `DECIDE_INT` into `SUBMODE_REQUEST` and
the dispatcher `0x0000BA74` only applies it on the next tick. So the page opens on `fv2_submode_request`, in
the one tick where `DECIDE_INT` is requested without having run, and overwrites the request with 2:
`DECIDE_INT` then runs **once only**, when the page hands back. Reading `MODE_SUB` caught it one tick too late
and made it run twice.

**Holding a request is deliberate here**, contrary to `fv2_request_title`'s warning: a request written once
is read once and the game moves on; here the point is precisely that it should not move on, so it is renewed
on every frame while the page is up.

#### The shape, after the trial of 2026-10-25

Taken from `CMenuOverlay::DrawStages` (August's modified Supermodel), with three changes asked for:

* the **arrows at the window's two edges**, and not centred on the row: in August's the third thumbnail on
  each side passed under an arrow, which then read as a label stuck on a stage. The band is widened to the
  whole window so that they have a background;
* **two side thumbnails instead of three**, and a little narrower, so that the row clears the arrows;
* **no instruction line any more**: August's two sentences ("USE THE DIRECTION BUTTON TO SELECT", "A TO
  DECIDE, START TO LET THE GAME CHOOSE") are removed.

### The stage draw, read in the code — and the stopgap replaced (2026-10-25)

The stage choice page had been holding August's lever until now: the function `0x0007111C` patched to
`li r3,<stage> ; blr`. **That is the right lever for Supermodel and the wrong one for this port** — this port
is a recompilation, and a word of RAM disagreeing with what the native code was made from switches the
recompiler off entirely, and the whole PowerPC falls back to the interpreter. Measured: **48.8 frames/s
against 57.5** for the same versus match with nothing held, and Frédéric noted 60 against under 50 in his
window.

So the function was read, with `tools/fv2/desas_ppc.py` on a RAM dump of a game in progress — twenty
instructions:

```
0007111C  bl 0x000083BC            a random number
00071140  r3 = random % 27         by the high multiply on 0x2F684BDB
00071160  r29 = 0x004836FF         a table of 27 bytes
00071170  if table[r3] == 0 : table[r3] = 1 ; return r3      the first free one, and it is taken
          otherwise r3 = (r3 + 1) mod 27, 27 tries at most
000711A0  then 0x00071098 rebuilds the table
```

And `0x00071098` says **which arenas can come out**: index 0 locked, 1 to 3 free, 4 free only if a setting
byte and a word at `0x00580E28` agree, 5 to 12 free, 13 to 26 locked. **Read back in three dumps**, the table
says exactly that: `01 00 00 00 01 00 00 00 00 00 00 00 00` then fourteen `01`. So the free indices are the
owner's twelve arenas less *The Great B.M.*, which has its condition.

**So the lever is the table**: leave one index free and the draw returns it, whatever the random number. A
write into **data**. No more disagreement with the native code, no more 100 ms rebuild, no more hole in the
sound. The table is held — the draw marks its choice as taken in taking it — and **given back as it was
found** rather than rebuilt from the rule: index 4 carries a condition this port does not evaluate, and a
table written from a half-known rule is wrong the day that half matters.

### Leaving a match: by the game's own branch point (2026-10-25)

`TITLE SCREEN` and `CHANGE YOUR VIPER` were waiting for a way to **end** a match. August did it from the
outside — the last round armed, the clock at the edge, player 1's health held at zero — because his fork did
not have the decompilation. This port does (`docs/fv2-flux-de-match.md`), so it asks instead of pushing. Two
words, and both are conditions the game itself tests on every frame:

| word | what the game does with it |
|---|---|
| `GAME_TIMER` `0x00581404` ← 1 | `fv2_game_FIGHT_DSP`: `if (... \|\| --GAME_TIMER > 0) return;` — on the next frame it sets its own TIME OVER flag and asks for `JUDGE` |
| `ADV_INFO_STATUS` `0x005813F0` \|= `0x40000000` | `fv2_game_NEXT_PROG_SEL`: `if (!(STATUS & 0x40000000)) { ++ROUND_NO; S-MODE_REQUEST = 6; return; }` — the bit makes the difference between "one more round" and "this match is settled" |

Then the judgement, the replay and the winner's pose are each cut short **once** with `fv2_cut_submode`,
which only asks for the next sub-state — what the game's handlers do among themselves. `NEXT_PROG_SEL` is
left alone: it is the decision that has just been written. All that is left of August's is the screen counter
`0x0058FD56`, and for a precise reason: `CONTINUE` and `GAME OVER` are **modes** and not sub-states, so the
sub-state request does not reach them.

**Measured on 2026-10-25**, chosen at frame 2100 of a round in progress: the match is over in **203 frames
(3.5 s)**, the end of the session passes in **12 frames (0.2 s)**, and the game arrives — the title screen
for one, a two-player select screen and a new fight for the other, photographed.

## The console layer has a state, like the game (2026-10-26)

Frédéric, on 2026-10-25: "*of the whole game, no state or sub-state encroaches on the others, except the one
we brought in. It still looks like patchwork, as on the rom.*" He was right, and the flaw can be named: the
board holds **one state at a time** — twenty-four modes, each with its INT that installs it and its DSP that
runs it, a handler table, a byte that says where one is (`docs/fv2-etats.md`). Nothing encroaches because
nothing coexists. Our console layer, for its part, had no state at all: a dozen booleans (`swallow_start`,
`want_list`, `want_stage_page`, `mode_wanted`, `versus_trip`, `stage_page_done`, `seen_select`,
`stage_from_pause`, `training_on`, `leaving`, `leave_for`) that together encoded a state machine nobody had
written, and a freeze decided by a boolean `stopped` with exceptions added one by one.

### The table (`src/host/console_state.h`)

Eight states, and each one **declares four things**. Nothing else decides them.

| state | the game runs | the screen | the sound | the controls |
|---|---|---|---|---|
| `PLAY` | yes | the game's | the game's | the game's |
| `PAUSE` | no | over the game's picture | both | the port's |
| `STAGE` | yes, parked in `STAGE_SEL` | over it | both | the port's |
| `STAGE_HELD` | no | over it | both | the port's |
| `GOING` | yes | the port's (the curtain) | **the port's** | the port's |
| `MENU_IN` | yes | the port's (the list's backdrop) | **the port's** | the port's |
| `MENU` | no | the port's | both | the port's |
| `OPTIONS` | no | the port's | both | the port's |

- **the screen being "the port's"** means that the game's picture is **not drawn**: not covered, not
  darkened, not drawn. That is the difference between a surface that hides what is behind it and a state that
  owns the screen;
- **the sound being "the port's"** means that what the **game** says to its board is thrown away at the one
  place where the game says anything (`Model3::mute_game_midi`, the only `midi.push_back`). What the port
  puts into the same queue goes through: the menu beeps over a game gone silent;
- **where the game is frozen and the port is not muted**, it is the port that runs the sound board — without
  which no frame of sound is made. The pause menu and the carousel were not in that set: their sound effects
  waited in the queue for the game to resume, and the fight's music stopped under them.

`MENU_IN` and `GOING` are Sega's INT/DSP shape where it makes sense: the surface is installed while the game
finishes what it was doing **behind it**, neither seen nor heard. That is what settled at a stroke the three
flaws of 25 October — `GAME OVER` heard, the black picture under the list, the glitched title — which were
three symptoms of one missing rule.

### One place opens, one place writes

`enter(state, why)` is the only place that opens or closes a surface, and it carries the three lines that
must always accompany such a change (withhold the player's Start, note it as not released, set the sound
device). Fourteen places did it by hand; the ones that forgot one of them were the flaws.

`console_play::ask_for()` is the only place that writes into the game's RAM on the port's behalf: a surface
says what it wants (end the match, a rematch, the title screen, let go of the select screen) and
`console_play` carries it out at the head of its `step` — after the frame that has run, before the one that
is going to run, which is to say at the moment the game's dispatcher reads a request. **One exception,
written down rather than left to be discovered**: the OPTIONS page writes the cabinet's settings, because it
*is* the settings, and its state declares that the game is not running underneath.

### What the table brought out by itself

- the pause menu and the carousel held over a fight made neither their own sound effects nor the fight's
  music heard (measured afterwards: **1714 frames of sound at 57.6/s** under the pause menu, the peak never
  at zero);
- bringing them into that set revealed a **heap corruption**: the board is the game's, and running it while
  the emulation thread is finishing the frame in flight means writing it from two places. A wait at the place
  where it runs covers all four surfaces;
- `CHANGE YOUR VIPER` had the same flaw as `MAIN MENU` and no answer, for want of a picture to put in front:
  it is the curtain (`loading.png`, the Dreamcast's loading picture), measured at 52 frames end to end;
- a training session that ended by itself left its mode switched on;
- a **script**'s kicks and guards were not routed in training, where a hand's were.

### What putting it in a table led to next (2026-10-27)

**A total machine.** `enter()` assumed that the surface the state names exists. A picture that had failed to
load would have left the carousel's state up with no carousel — the game parked in the empty `STAGE_SEL` for
ever. It now asks first: refused, the reason in the log, back to `PLAY`, and for the carousel alone the
select screen is let go of (the game was parked there on the very tick of the request). Measured with
`VF3_CONSOLE_STAGE=none`: "*STAGE was asked for (the fighters are chosen) and its page is not there --
staying in play*", then "*the select screen let go of*", and the versus match goes on.

**A state is asked for, like a write.** `console_play::ask_state()`: the layer says which state it wants, the
host enters it. That is what made the last two signal flags disappear (`want_list`, `want_stage_page`) along
with the two `if`s by which the host re-guessed the end of an exit.

**What the game knows, we no longer keep.** "Is this a versus match" was two booleans, one of whose only job
was to delay clearing the other. The game says it in the byte its own branch point reads first:
`Model3::fv2_players`, **1 for one player, 3 for two**, measured on all three routes. That also answers a
case this port could not have known: a game joined mid-way by the cabinet's INTRUDE rule is a versus match,
and nothing here had chosen it.

**What is borrowed is given back.** The state writes four things that live outside it — the board's mute, the
logo's voice, the announcer's, `VS FINISH` — and they are only written while the console way is on. Changing
play style at the wrong moment left them in place: the game would have stayed silent for the rest of the
session. All four are given back where the style changes, and `VS FINISH` **before** the borrower is
replaced.

### The net: `verifier-console.cmd`

Three routes played without a window and without hands, and **compared by machine** with the expected
sequence of states (`tools/fv2/verifier_console.py`):

| route | the expected sequence |
|---|---|
| VERSUS | `PLAY MENU PLAY STAGE PLAY PAUSE MENU_IN MENU` |
| ARCADE | `PLAY MENU PLAY PAUSE MENU_IN MENU` |
| TRAINING | the same as ARCADE, plus the lines of its levers |

TRAINING is there because it is the route that starts up what no other touches: the clock stopped, the
health held, the seven announcement states cut short, the announcer thrown away, and the exit from a
two-player match. Its three lines are looked for in the log next to the states.

Two traps, paid for and corrected: the exit was counted in **the game's frames**, and the route ends on
`MENU`, which freezes the PowerPC — the process was still running twenty-six minutes after finishing (there
is a clock-based exit beside it now); and a printed trace is not a verification, because a line that has
disappeared in the middle does not leap out at you the next day.

### A VERSUS match's way out: `VS FINISH`, the cabinet's setting (2026-10-27)

`verifier-console.cmd` plays the whole path without hands and prints the trace of the states. It was going
all the way **in ARCADE** and **getting stuck in VERSUS**: four hidden sessions, the game reloading a round
twenty frames after `MAIN MENU`, the probe saying "the match says decided" on every frame. Here is the
reading that found the way out, and the two false trails it ruled out.

#### What the branch point really does

`NEXT_PROG_SEL` (`0x0000DB48`), disassembled from a RAM dump with `tools/fv2/desas_ppc.py`:

```
lis r10,0x0058 ; lwz r10,5104(r10)      ADV_INFO_STATUS
rlwinm. r0,r10,28,0,0 ; bne 0x0000DD4C  bit 0x00000008 : the ladder is finished -> mode 12, the game's end
rlwinm. r0,r10,1,0,0  ; beq 0x0000DD28  bit 0x40000000 ABSENT -> one more round
lbz r3,[0x0058145A]                     how many are playing
  == 3 -> 0x0000DBD0                    TWO PLAYERS : the versus route
  == 0 -> 0x0000DC9C
  != [0x00581457] + 1 -> 0x0000DC9C     "all but one have dropped out" ? otherwise the last one carries the ladder on
```

and the four modes those routes ask for: `8` CONTINUE, `12` ENDING, `18` GAME OVER, `22` RESULT.

#### The versus route, and the way out

`0x0000DBD0` is the two-player route, and its **first question** is a setting:

```
0000DBE0  lis r3,0x0012 ; addi r3,r3,1312      0x00120520, the live block of settings
0000DBE8  lbz r4,56(r3)                        0x00120558 = VS FINISH
0000DBEC  cmplwi r4,0 ; bne 0x0000DD60         if it is ON -> mode 18 : GAME OVER
```

**`VS FINISH` is the way out**, and it is the cabinet that provides it: with the setting ON, the end of a
two-player match **ends the session**; OFF, the winner inherits the ladder and the game goes on — exactly
what the four hidden sessions were showing. It is already in this port's OPTIONS page (`fv2_options.h`, the
`VS FINISH` line).

The port **borrows** it for the duration of the exit: `borrow_vs_finish` reads it and sets it to 1 when a
match exit begins, `return_vs_finish` gives it back on arrival (and also if the exit gives up). **Only the
live copy is touched** — not the stored setting — so what the player chose in OPTIONS is not changed.

**Measured** over the complete path in versus: `VS FINISH was 0, lent as 1`, the match is over in **203
frames (3.5 s)**, the title screen reached in **12 frames**, `VS FINISH given back as 0`, `MENU_IN -> MENU`.
That is the same figure as the arcade route, and it is the same route.

#### The two false trails, ruled out by measurement

- **the count of players who dropped out** (`0x00581457` set to the number of players) and **a point of
  health's difference** so that the clock has a winner: the two together changed nothing a session could
  see. Removed; `fv2_end_match`'s comment says that they were and why;
- `fv2_restart_attract()` from the fight does put the game into `ADV`, but **the match's task stays alive**,
  so `fv2_in_attract()` goes on saying that a game is running;
- writing 0 into both health bars **is not a K.O.**: the game decides that in its damage handler.

## The end of a two-player match is a state, not a cabinet screen (2026-10-28)

Frédéric, on 2026-10-28: "VERSUS MODE: at the end of the fight, in place of the GAME OVER sub-state, create a
sub-state of versus mode's own offering REMATCH, CHANGE VIPERS, CHANGE STAGE, MAIN MENU."

### The game says itself when a match is settled

A route was written to watch it: `tools/scripts/fv2-vs-fin.txt`, a versus match where player 1 hits without
stopping and 2 does not move, so every round ends in a K.O. Played hidden with `VF3_VS_TRACE=1`, it prints
one line per sub-state change from the judgement on. The end of each round reads the same four states, then
**splits in two**:

```
a round that settles nothing  13 JUDGE  14 JUDGE_DSP  15 REPLAY_INT  16 REPLAY_DSP
                              17 VIC_INT -> asks for 19      (VIC_DSP is skipped)
                              19 NEXT_PROG_SEL -> asks for 6  (one more round)

the round that settles it     13 14 15 16
                              17 VIC_INT -> asks for 18
                              18 VIC_DSP runs
```

**`VIC_DSP`, sub-state 18, only comes at the end of a settled match.** It is not a threshold nor a round
count kept by this port: it is a state Sega named and that it reaches nowhere else. That is the door, and it
is the game's.

### And the panel comes up at its end, not at its start

The moment taken is the one where `VIC_DSP` has **asked for** `NEXT_PROG_SEL` and the dispatcher has not
acted yet — the same tick as the carousel (`step_select`). So the winner's pose plays in full, and
`NEXT_PROG_SEL` does not run at all. One frame later it would already have decided: `GAME OVER` if the
cabinet's `VS FINISH` setting is ON, the winner taken into the arcade ladder if it is OFF — measured in that
same session, where it is OFF: 19 ran, the players byte fell from 3 to 1, and the game ran straight into a
ladder fight. **Neither of the two routes is taken now.**

Since nothing must be left pending behind the panel, the sub-state request is put back to 18 —
`Model3::fv2_hold_submode`, the exact inverse of `fv2_cut_submode`, and the value the game itself holds there
on every other frame.

### What the state declares

`VS_OVER` is one row of `console_state.h`'s table, exactly like `PAUSE`:

| state | runs | screen | sound | controls |
|---|---|---|---|---|
| `PAUSE` | no | over it | both | the port's |
| `VS_OVER` | no | over it | both | the port's |

The surface is **the same panel** (`fv2_pause.h`), because the four lines were already in it. Three things
differ, and not one more: `RESUME` is not shown (there is no fight to come back to), the player cannot leave
with Start or B (the same reason), and the bar says `VS` `BATTLE OVER` instead of who paused.

`chose_pause` serves both: the only difference is that the match is **already** over, so `MAIN MENU` and
`CHANGE YOUR VIPER` do not ask for `end_match` — `fv2_end_match` refuses anywhere but during a round being
played, which is right, its two words meaning nothing to `VIC_DSP`. All the rest of the exit reads the same
from there: the bit held, the states cut short, `VS FINISH` borrowed, the ending screens shortened behind the
port's picture.

### Only with two players, and never in training

`fv2_players()` reads 3 when two players are in contention: the arcade ladder's matches also end at
`VIC_DSP`, and there the end of a match is the ladder's business, not the player's. TRAINING is a two-player
versus match on this board and it is ruled out by its name — the dummy has no rematch to ask for, and that
mode's pause menu is the way out.

### Leaving this panel: a second loan, and it is measurement that imposed it

`REMATCH` worked first time (`VS_OVER -> PLAY`, `a rematch asked for, the game says 1`, and the next match is
played). `MAIN MENU` **did not**: the game went back into a fight and the exit gave up after its minute. The
whole branch point, read again from a dump taken at frame 5000 (`VF3_RAM_DUMP`, a new hook, and
`tools/fv2/desas_ppc.py`):

```
0000DB4C  lwz r10,[0x005813F0]        ADV_INFO_STATUS
0000DB50  bit 0x00000008              -> DD4C : mode 12, the game's end
0000DB58  bit 0x40000000 absent       -> DD28 : ++ROUND_NO, sub-mode 6, one more round
0000DB64  lbz r3,[0x0058145A]         HOW MANY ARE PLAYING
0000DB70    == 3                      -> DBD0 : the two-player route, which reads VS FINISH
0000DB74    == 0                      -> DC9C
0000DB88    != [0x00581457] + 1       -> DC9C : 145F -> mode 22 RESULT, otherwise mode 8 CONTINUE
0000DB8C    otherwise                 the last one standing carries the ladder on : ++145C, sub-mode 0
```

**`VS FINISH` is the second question, not the first.** And the byte that commands the first falls away under
our feet: `VF3_RAM_WATCH=00581454:12` on the panel's route gives

```
frame 6659  VS_OVER -> MENU_IN (MAIN MENU)
frame 6660  mode 7 sub 19 : 0058145A 03->01
frame 6661  mode 7 sub 19 : 0058145C 01->02      <- the ladder's route, not the versus one
```

The `3` holds for the whole match and falls to `1` **on the very frame** `NEXT_PROG_SEL` runs: it is the
cabinet's `INTRUDE` rule — the winner inherits the session — applied at the end of the winner's pose. So a
settled versus match has **stopped being a versus match** before the branch looks at it, and it takes the
ladder's route whatever is done with `VS FINISH`.

**Lending the 3 was tried and does not take**: the hand passes to the winner on the branch's own frame, so
after the loan's write, which is done at the top of the frame. Measured, not supposed.

What works is shorter and more honest: the two-player route ends in three instructions,
`li r0,18 ; stb r0,[0x0058FD9A]`, and **the port writes what it writes, where it writes it**
(`Model3::fv2_request_game_over`). From the panel, `MAIN MENU` and `CHANGE YOUR VIPER` therefore do not ask
for the end of the match — it is over — but for `GAME OVER`, and the exit goes straight to the ending
screens, which the port's curtain already covers. The arcade and survival routes and the pause menu
mid-fight are not touched: they keep theirs, which works.
