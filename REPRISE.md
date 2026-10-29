# Resumption — Fighting Vipers 2, the console menu

State as of **2026-10-24**, branch `fv2`, pushed to `mine` up to `7f732ee`; **`origin` (the `mr-finster`
upstream) has no `fv2` branch** — none of this has gone there.

The day's journal is at the bottom. What follows is the state, not the story.


## The rule that takes precedence, laid down on 2026-10-22

**For any question of the form "why does the game do that", open `docs/fv2-flux-de-match.md` BEFORE
measuring.** It is the arcade program's decompilation: 759 lines, the 81 state handlers named by the game's
own tables, the variables under Sega's names. Only then does one measure, to confirm, or for what it does not
cover — it states its own limits (its section 7) and marks what was **read without being seen running**.

Why that rule exists: the evening of the 21st was spent finding by RAM difference what this document already
names.

| groped for on the 21st | already written in the flow |
|---|---|
| "the block `0x00700500` draws the demo" | `[0x0007511C] → 0x00700500`: **fighter 0**; `0x00700200` = `CPU0`, `0x00700300` = `CPU1`, 23 slots |
| the routine `0x0003F874` seen on the title screen | `FUN_0003F874` **is the fighter's task**, entered by the scheduler's `blrl` (`0x000391AC`) |
| "`0x0000B7C0` puts an end to the replay" | `fv2_adv_ADV_REPLAY_DSP`, **`0x0000B764`, 100 bytes** — the address falls inside it |
| — | `fv2_adv_ADV_DEMO_DSP` asks for sub-mode 2 **when `ADV_INFO SUBMODE` (`0x00580E08`) reaches 4** |

`SupermodelSrc\Docs\MenuConsole.md` remains the source of the **levers** — each one was measured there by
playing. The flow says **why** they are the levers, and how far they reach. It is the absence of that "how
far" that made the sequencer's lever inert in a replay without anyone being able to foresee it.


## What works today

Everything is behind a `.cmd` launcher at the root, and they all pin `VF3_DATA` to the project's folder.

| | launcher | where it is written |
|---|---|---|
| the game, sound and pads | `jouer-fv2.cmd` | |
| the hidden characters | `jouer-fv2-roster.cmd` | `docs/fv2-roster.md` |
| the select bar's portraits | `jouer-fv2-portraits.cmd` | `docs/fv2-portraits.md` |
| the 2D tile pack | `jouer-fv2-hd2d.cmd`, `releve-tuiles-fv2.cmd` | `docs/fv2-hd-2d.md` |
| the 3D textures | `jouer-fv2-hd3d.cmd`, `dumper-textures-3d-fv2.cmd` | `docs/fv2-hd-3d.md` |
| Honey's four costumes | `montrer-honey.cmd` | `docs/fv2-honey.md` |
| the colour of the untextured parts | | `docs/fv2-couleurs.md` |
| the two Kuhns, and what separates them | | `docs/fv2-deux-kuhn.md` |
| the name on the select screen | | `docs/fv2-noms-selection.md` |
| the console modes, and the list on the title screen | `jouer-fv2-console.cmd` | `docs/fv2-console.md` |
| measuring what Start does | `mesurer-fv2-start.cmd` | `docs/fv2-etats.md` |
| the list's captures | `capturer-fv2-liste.cmd` | `docs/fv2-console.md` |
| build the window too | `construire-hote.cmd` | |
| build | `construire.cmd` | |
| verify | `verifier-vf3.cmd` | |
| where the sound board's time goes | `VF3_SND_PROF=1` with `VF3_PROF=1` | `scsp/docs/scsp_audit.md` |
| the sound has not moved by a byte | `tools\son\compare-sound.cmd` | `scsp/docs/scsp_validation.md` |
| the DSP's two bodies, tested | `make test` | `scsp/docs/scsp_validation.md` |

And the game runs on **free play**: a single byte of the NVRAM, `backup[0x17] = 27`.

**Start now does both of the things asked for**, and the list of the seven modes is a surface of its own,
drawn on the game's title screen. The Escape menu's page still exists beside it, unchanged: it is not the
list.


## What was done on 2026-10-21

```
                           on the game's title screen ->  the list of seven modes opens over it
Start, console mode
                           everywhere else            ->  the game comes back to its title screen, by ITS road
```

In both cases the press is **withheld**: the game does not see it.

- The decision is `console_play::take_start` (`src/host/console_play.h`), so the host and `vf3run` take it the
  same way, and `vf3run --modes --console-start N` **measures** it without a window.
- The surface is `src/host/fv2_mode_list.h`. It does not go through `game_menu`: it draws
  `assets/menu-console/title_list.png` over the picture the game has just produced. The geometry comes
  entirely from the measurement (`docs/fv2-etats.md`), plus the two factors asked for at the time — grown by
  7/6, lifted by 50/1080. The chosen line **blinks** (alpha 1 → 0.5 over 45 frames): that is the owner's
  answer to the fact that only one version of each line exists. **The picture behind goes to half** while the
  list is there, as on the Dreamcast: measured on two captures of that version, 255 becomes 128 on all three
  channels, and the same measurement on the two captures from here gives 0.500.
- **There is a way out**: B on the pad (or the kick key, or Backspace) closes the list and leaves the game
  where it was. A, Start, Enter or the punch key choose.
- **The list is silent**, and the game's sound is paused behind it, as under the Escape menu. A first version
  of the sound effects was laid down and then **undone the same day**: see the journal for 2026-10-21 and
  `docs/fv2-console-son.md`, which puts the questions the next one will have to answer before being written.
- ARCADE and SURVIVAL act. The five other lines are drawn and do nothing: RANDOM and V.S. need the select
  screen's timing, never measured here; NETWORK, TRAINING and OPTIONS have no lever in this port.

### Start's road to the title screen, and the three attempts it took

It is the piece that cost the evening, and the result fits in two lines: **from the attract, Start asks the
loop to start over, then asks for the title screen once the tidying is done.**

```
MODE_REQUEST = 3, SUBMODE_REQUEST = 0   written ONCE : the loop restarts, the game tidies its scene
then, at the SEGA logo (sub 2 or 3)     the title screen request
```

Measured: **the title screen in five frames, at 0 models in 3D**, from a demonstration as from a replay —
exactly the title screen the game arrives at on its own.

The two routes ruled out stay on file, because each taught something:

| route | what it gives |
|---|---|
| the mode request alone | a picture, but **397 models** left on the title screen: the demos that follow come out black or empty |
| the sequencer's lever (§6 bis) | clean from a demonstration, **inert from a replay**, and a 77 s replay behind it |
| **restarting the loop** | **five frames and 0 models, from anywhere** |

Why: the mode request changes what is **displayed**, it does not stop what **draws**. The game only turns its
drawing tasks off when its loop restarts (`0x0000B240`, between sub-states 1 and 2), and that is exactly what
the replay asks for when it finishes (`0x0000B7C0` writes `SUBMODE_REQUEST = 0`). The whole chain is recorded
in `docs/fv2-etats.md`, with `--watch`, without decompiling anything.

**What remains true and unresolved**: from a **fight**, the mode request brings the title screen back but does
not end the game — `entry1` (`0x00700000`) stays at `0x2A`. The title screen comes back over a game that is
still running.

### The captures, and what they prove

`capturer-fv2-liste.cmd`, three pictures in `build\`:

| picture | what it shows |
|---|---|
| `liste-1-arcade.png` | the seven lines on the real title screen, FREE PLAY readable under them |
| `liste-2-survival.png` | three times down then Enter: the game starts, and the select screen says SURVIVAL MODE |
| `liste-3-retour-au-titre.png` | Start during the demonstration: the title screen is back |

The second is the one that counts: it goes end to end, with no hand on the pad.

### The probe, and what it made possible

`jouer-fv2-console-sonde.cmd` launches the game with `VF3_PIC_LOG` and `VF3_MIDI_LOG`: **one line per frame
drawn** in `log.txt` — 3D models, views, mode, sub-state — and everything the game says to its sound board. It
is what named the flaw that four scripted trials had failed to reproduce:

```
title screen the game arrives at alone    0 models,  8 views
title screen asked for by Start         397 models, 10 views
```

Landmarks for next time: a sound title screen is at **0 models over 8 views**, a demonstration around **400 to
520**.

### Two test hooks, without which none of this could be verified

The list freezes the game, so anything that counts the game's frames no longer arrives — that was the previous
day's nine-minute hang. Hence, in the host:

```
VF3_QUIT_AFTER_S=N     finish after N seconds by the clock
VF3_KEYS_S=k@sec,...   send the key k at that second
```

And three others, laid down in the evening, that did all the rest of the work:

```
VF3_PIC_LOG=1          every frame drawn : models, views, mode, sub-state
VF3_MIDI_LOG=N         what the GAME says to its sound board, plus a line every N frames
VF3_MIDI_SEND=h@F      put a question to the sound board at frame F ; the answer reads in the peaks
```


## What remains open, and what it blocks

- **Really leaving a fight.** The screen comes back, the game does not (above). The game's road — ending the
  match without showing or hearing it, `0x00581455 = MATCH COUNT−1`, `0x00581406 = 64` once per round, player
  1's health at zero — is recorded in `docs/fv2-etats.md` and **is not wired**.

- **The second player has never been got in.** `0x0058145A == 3` commands the cut-in, the consecutive-wins
  counter and `NEXT_PROG_SEL`'s two-player half. It is a **test-harness** gap, not a knowledge one: it takes a
  coin and a player 2 Start at the right moment. Closing it would serve the Versus loop, the real condition
  for unlocking Honey, and `VERSUS` in the menu.
- **The ending, the name entry, the ranking and the Super K.O.** are named by Sega's table and never reached
  here.
- **What *decides* to rebuild a fighter** — the test at `0x00060288` — is unknown. What *commands* it is known
  and used (`fv2_redo_fighter_model`). That blocks nothing.
- **Survival's branches** have never been watched running, although the mode works.

The other menu lines' levers are measured and inventoried in `docs/fv2-menu-console-inventaire.md`:
`CHANGE YOUR VIPER`, `REMATCH`, `CHANGE STAGE` (`0x0007111C`), `OPTIONS` (`0x00120540`–`0x0012055F`), `STATS`,
`SOUNDS`, `DISPLAY`, `RANDOM`, `VERSUS`.


## Where the original sources are

They are not in this repository and must not come into it.

```
Documents\SEGA Model 3\SupermodelSrc\Docs\MenuConsole.md     3689 lines -- ALL the measurements on the ROM
Documents\SEGA Model 3\SupermodelSrc\Src\Menu\GameFV2.h      the addresses and the geometry
Documents\SEGA Model 3\SupermodelSrc\Docs\TuilesHD.md        the tile pack
Documents\SEGA Model 3\SupermodelSrc\Docs\Tex3DHD.md         the 3D textures
Documents\SEGA Model 3\ROMs\fvipers2.zip                     the ROM
C:\Users\frede\Desktop\Nouveau dossier (2)\fvipers2          the menu's pictures, where assets/ comes from
```

`MenuConsole.md` has a table of contents: `grep -n "^#"`. The sections that count for what follows are **§6**
(knowing where the game has got to), **§6 bis** (leaving a fight), **§6 quater** (the states named by Sega),
**§6 sexies** (the menu on the real title screen), **§13** (the menu's pages and their geometry).

**Nothing must be taken from Supermodel's code**: it is under the GPL. The addresses and the measurements are
facts about the game, not code — that is what was ported, and nothing else.


## The working rules, non-negotiable

- **No prediction.** Everything is read in the code or measured. If decompiling is needed, decompile.
- **The code says what CAN happen, the measurement says which one did.**
- **Show every capture, with no verdict.** Frédéric is the one who decides.
- **Always a `.cmd` launcher**, never a command line to copy out.
- **Never filter the build's output.**
- **Add alongside, never replace what works.** Read before writing.
- **No Sega binary, no ROM, no picture taken from the game in the repository.**
- **Push to `mine` only** (`penpenlovesrei-dotcom/vf3-recomp`), never to `origin`, which is `mr-finster`'s
  upstream.
- **Nothing is delivered without `verifier-vf3.cmd`**: five IDENTICAL checks and the step-by-step interpreter
  against recompiled code over 2231 frames.

Short answers. Say what is measured and what is not.


## This port's traps

- **A single frame of RAM differing from the instruction its native code was built from switches the whole
  recompiler off.** So patches must arrive before `rebuild_from_ram_if_stale()`, or force a rebuild.
- **FV2's program is a gzip at offset 0 of the CROM**, unpacked into RAM 0 by the game's boot. So every patch
  is a RAM patch, and each one reads before writing.
- **`board.ram` keeps the 32-bit words in the host's order**: the byte at address A is at index `A ^ 3`.
  `word(ram, a)` returns the big-endian value directly.
- **`tools/vf3run` NEVER loads the NVRAM**: it always starts from a board that has never been set. No operator
  setting can be proved with it.
- **`vf3run` only renders the picture on a script's `shot` marks** (and `--hd-dump`, which forces rendering).
- **Bash heredocs eat apostrophes and escapes.** Write the files with the Write tool.
- **The repository's files are CRLF.** An insertion script looking for `\n` finds nothing.
- **Linking fails with "Permission denied"** if the game or the verifier is still running.

---

# Journal for the evening of 2026-10-21

## Three sound stopgaps, and what they cost

The list's sound was laid down three times within the one hour, each time on a report from Frédéric — "no
sound effect", "the music does not stop", "it crackles" — and the fourth time he wrote: "lots of sound and
behaviour bugs, I could not list them".

All of it was **undone**. The crackle was indeed mine (the sound board advanced on one pass in two: thirty
frames of sound a second instead of 57.5), but the root was the method: one stopgap per symptom does not
converge, and **sound cannot be verified by looking at a picture**. `docs/fv2-console-son.md` holds the
sound's place: what is settled, the four questions to decide before writing, the table of the list's four
exits to be filled cell by cell, and the order of the steps. Nothing to do with sound is written.

Step 1 of that plan was done all the same, and it overturned three received ideas: **the game never sends
`FF`** (its silence command is `A0 00 01`), **the title screen has no music of its own** (what one hears there
is the demonstration's, left running), and **the select screen puts its own on** — so restoring the previous
one after a mode is chosen is harmful. It is all in `docs/fv2-console.md`.

## The real flaw was elsewhere, and the probe named it

"After leaving the menu, both demos are buggy: characters very dark or absent." Four scripted trials failed to
reproduce it — the demos there were **identical to the bit** with and without the list. It was by
instrumenting **his** session (`VF3_PIC_LOG`) that the answer fell out in two lines: the game's title screen
carries 0 models in 3D, the one Start asked for carried 397.

The mode request changes what is **displayed** and does not stop what **draws**. From there, three measured
trials, two dead ends, and the right road — restarting the loop — found by watching *how the game itself
leaves a replay*. No decompiler: `vf3run --watch` names the instruction that writes, and that was enough.

## What the evening confirms about method

- **Four scripted trials that reproduce nothing prove nothing**: they did not cover the path he was taking.
  Instrumenting his session cost ten minutes and gave the answer.
- **Ask the game for the lever rather than inventing it.** The two levers that work here are the ones the game
  sends itself. The two that failed were ours.
- **Do not launch the game before having said what to look at.** Done twice, objected to once. Since then the
  launchers are given, not fired.

---

# Journal for 2026-10-21

## Start, and the list

The two paths asked for yesterday are in place, and the list is at last a surface of its own. What changed
from the two failed attempts is not the intention, it is the **place**: `fv2_mode_list::render` draws between
`compose()` and the menu, over the game's picture, and never touches `game_menu`.

The rest of the day came down to three things.

**Read before writing, for real.** The geometry, the 7/6 factor, the 50/1080 lift and above all the way of
marking the chosen line were already in `SupermodelSrc\Src\Menu\GameFV2.h` and `Src\OSD\SDL\MenuOverlay.cpp`.
The first version here switched the six other lines off — an invention, when the answer existed: it is the
chosen line that **blinks**, the others stay full. Nothing of the code was taken; the numbers were.

**A positive trial must say how far it reaches.** The mode request brings the title screen back from a fight —
which was thought impossible. But reading `entry1` back says that the game, for its part, is still running. A
correct measurement can carry too wide a conclusion: "it works" meant "the screen comes back", not "we have
left the game".

**The verification must bear on what is in question.** The day before, the check did not tell the two menus
apart. Today `liste-2-survival.png` cannot be mistaken: the list opens on the game's title screen, its cursor
goes down three lines, the chosen line starts the game, and the select screen says SURVIVAL MODE. It took two
hooks for that (`VF3_QUIT_AFTER_S`, `VF3_KEYS_S`), because everything that counts the game's frames stops when
the list is there.

---

# Journal for 2026-10-20

## The same mistake twice, and what it teaches

Start was wired to `open_console()`, which calls `menu.open(console_page())` — **the Escape key menu's RmlUi
widget**, on another page. Frédéric had rejected it once a few hours earlier ("your menu is not integrated
into the game at all, it is in the escape key menu"), I had agreed, and I delivered it a second time under
another commit.

What let it through is more useful than the mistake: **the verification could not tell the two menus apart.**
It noted that console mode froze the game after 75 seconds, where PC mode reached the select screen. But both
menus freeze the game. The check bore on nothing that was in question, and it was presented as progress.

`f328e77` is reverted by `ceb9a8a`. Start takes nothing any more, in any mode. What had been measured in the
reverted commit is kept in its message: opening a menu on Start freezes the game at that frame, a script's
Start must be read as a player's, and a scripted run with the game frozen hangs with no time limit coming
from outside.

## The states were already analysed, and not here

Frédéric: "there are states that were already analysed during the work on the ROM, find it."

It is `Documents\SEGA Model 3\SupermodelSrc\Docs\MenuConsole.md`, 3689 lines, 194 KB, from 14 to 20 August.
This port had none of it. The day's question — "how to know that the title screen is on screen" — has a whole
section there, **§6 sexies**, and the answer is measured there, not deduced.

Brought into `docs/fv2-etats.md` (`e3c1938`), then read by the port (`d1e0ee8`):

- the state machine's four contiguous bytes, and the 24 modes Sega named in RAM, 80 strings in one run at
  `0x00024494`;
- the title screen is on screen when the mode is `ADV` and the sub-state `FV2_TITLE_INT` or `FV2_TITLE_DSP` —
  measured by freezing the game sub-state by sub-state and looking at the screen. **Not** the same question as
  `fv2_in_attract()`, which asks whether a game is running and which is true of the warning, the logo, the
  title, the presentations, the demos and the rankings;
- to get there, the mode and the sub-state are **asked for**, not forced: the dispatcher at `0x0000ABC4`
  copies `MODE_REQUEST` into the mode before jumping. Two frames. Written once — holding a request is what
  broke the select screen and its clock, twice;
- from a fight neither known lever commands anything, and the negative trial says which of the two it is:
  **the values were still intact a second later**, so the attract sequencer does not run during a match and
  nobody reads them.

Three lessons in method are written there in black and white, and each cost a session over there: a contiguous
table does not say where its slices begin (slicing by the names was caught out **three times**); a correct
measurement can carry a false explanation, and the explanation travels further than the measurement; a
negative trial must say which of the two — the game overwrote our values, or the game does not read them.

## The measurement, made here

`vf3run --modes` was added: the mode and its sub-state under Sega's names, at every change. It is the
equivalent of the other side's `-menu-mode-log`, and it is the instrument every question of *when* should be
hung on, instead of counting frames from some approximate event.

```
     1  WARNING_INT    0
   225  WARNING_DSP    0
   997  ADV_INT        0 DONT_RECYCLE_INT
   998  ADV_DSP        1 DONT_RECYCLE_DSP
   999  ADV_DSP        2 ADV_SEGA_INT
  1001  ADV_DSP        3 ADV_SEGA_DSP
  1193  ADV_DSP        4 FV2_TITLE_INT    <- TITLE SCREEN
  1194  ADV_DSP        5 FV2_TITLE_DSP    <- TITLE SCREEN
  1386  ADV_DSP       11 ADV_DEMO_DSP
```

**The SEGA logo holds from 1001 to 1193, that is 192 frames** — the figure measured two months ago on another
emulator. Two independent measurements landing on a number neither was looking for: it is the day's strongest
control.

The title screen holds from 1193 to 1385. The capture was taken at 1255 — sixty frames later, the rule
established over there to let the fade and the announcer's voice pass, the freeze cutting the voice off (at
twenty frames one hears "Fighting Vipers" without the "2").

## The list of modes, measured

`assets/menu-console/title_list.png`, 709 x 897, RGBA: ARCADE, V.S., RANDOM, SURVIVAL, NETWORK, TRAINING,
OPTIONS. The seven bands measured here fall within a pixel of August's measurement, and so do the ink's
columns. The labels are painted **flush left** — `V.S.` has its middle at 0.17 where `NETWORK` has it at 0.50
— so each one has to be slid to bring its ink to the middle of the block.

## Verification

`verifier-vf3.cmd`: the five IDENTICAL checks, and 2231 frames step by step between the interpreter and the
recompiled code.

## Journal for 2026-10-23 — the OPTIONS page finished, and the game's ending

It is all in `docs/fv2-console-options.md` and `docs/fv2-etats.md`; this is the thread, not the detail.

### `ROUND TIME` did nothing, and why

The match **does not read the setting**. It reads an already converted word — `0x00581418` for one player,
`0x0058141C` for two — and that conversion happens **only once, at boot**, value × 64. So writing the setting
afterwards could not show. `fv2_set_option` now writes all three: the backup, the working copy, and the cache
when there is one. Two boots compared over all of `0x00580000`-`0x00582000` say which ones are cached: both
`ROUND TIME`s and `ADVERTISE SOUND`, nothing else.

The verification round was played from the key to the clock, in a hidden session: two rounds started from 1280
frames, the twenty seconds the page set.

### `VS DIAGRAM` on real figures

The table already read the backup, but on nothing: with no match played to the end, everything is zero, and a
table of zeros does not say whether the right place is being read. Four matches played hidden, each against
the previous one's backup, filled it — and corrected the description:

- **`0x020` is `DRAW`**, which August's note gave as being nowhere;
- the encoding is **contiguous** (two big-endian bytes at `+0`/`+1` of a cell of eight), not spaced out like
  the settings';
- `SKO 1` and `SKO 2` almost certainly fall into the two holes at `0x008` and `0x010`, and **stay empty** for
  want of a Super K.O. played. An empty cell says what is known; a zero would pretend to know.

An ARCADE match writes none of this: it is a record of versus play.

### `DISPLAY`: the two stepped-over lines

`HD ART` is a real switch. No new lever was needed: `load()` empties what it holds before reading, so loading
from nowhere puts a pack down, and the GPU rendering already checked the number of pages. `NONE` when there is
nothing to load, and the line is then stepped over.

`ENDING` does two things. `ROLL NOW` gives the credits over the title screen; `AFTER A MATCH` arms the real
ending. And it took real research.

### What the game's ending really is

**The names Sega left in the clear said it.** `0x00024494` is not a run of strings but a table of eighty
pointers, with the handlers at `0x00024354` at the same index. The ending's seven sub-states:
`ending_trans_init`, `ending_trans_wait`, `ending_replay_init`, `ending_replay_ctrl`, `ending_replay_next`,
`ending_final_mask1`, `ending_final_mask2`. **Three of the seven are replays.** The credits alternate the
credit roll and replays of the matches won; the "place" and "v.s. NAME" banners are the replay's, not
leftovers.

The lever: `fv2_game_NEXT_PROG_SEL` branches to the ending when `ADV_INFO_STATUS` carries bit 3, "the ladder
is finished" — a bit the summer's notes gave as read in the code and never seen set. But it is the branch
point of **every round**, so setting it too early has it consumed by a round that has nothing to replay. The
rule fits in one line: **set it at the first frame where the word already carries `0x40000000`**, the flag by
which the game says the *match* is over.

Played end to end: the list, `OPTIONS`, `DISPLAY`, `ENDING` on `AFTER A MATCH`, `ARCADE`, a match — and the
ending comes out by the game's branch, with Cross River Damsite, the cage and the two fighters.

### Two mistakes of method, the same one twice

The ending **alternates** every 225 frames: credits, then a replay. I was caught by it twice.

1. By photographing **always frame 1900** while moving the moment of the request, I thought I had found a
   clean boundary at frame 1212 and laid a wait of forty frames on it. There was no boundary: I was only
   changing the phase frame 1900 fell into. The wait was removed.
2. By photographing the **second** replay passage, I thought the host was not loading the scenery while
   `vf3run` was, and I was about to isolate the processor rendering against the GPU one. There was no
   difference at all: `vf3run`'s picture was the first passage, mine the second — and the second is an
   opponent never faced, so a replay that does not exist.

**On a phenomenon that alternates, compare at an equal offset, never at the same absolute frame number.** And
the sweep to the frame (`VF3_KEYS=key@frame`, which writes the window three frames later) is what eventually
settled it, because it does not depend on the machine's speed.

### In passing

`F12` now goes through the console's surfaces, and the surfaces are drawn in a single place: a photograph of
the page is a photograph of the page. Before, it recomposed the window without them and gave the title screen.

### What is left

- `SKO 1` and `SKO 2`: **done**, by a Super K.O. played by hand. `0x010` goes to 1 in the winner's block
  without KO, JUDGE or DRAW moving. Neither the script, nor thirty thousand frames of random play, nor a whole
  fight against the processor had managed to produce it.
- `CONTROLLER`, empty, and `BGM SET`, the fork's music packs.
- The black fighters of 2026-10-23: the truncated submission after an attract restart, the cause still open.

## Journal for the night of 2026-10-24 — CONTROLLER, the pause menu, and three bugs of mine

### What is added to the game

- **`CONTROLLER` is no longer empty.** A bindings editor: nine lines — the four directions, punch, guard,
  kick, Start, `SELECT` — over two columns, `1P` and `2P`. `A` opens a line and the next key or button becomes
  the binding, written into `settings.ini`. A tenth line, `TEST`, opens the **mirror**: the eight directions
  and the three buttons lit as long as they are held, a diagonal lighting on its own. The icons are the game's,
  `inputs.png`, which was used nowhere.
- **The pause menu**, `src/host/fv2_pause.h`. Start during a match, in the disc's shape: a purple panel
  under its `1P PAUSE` bar, on the left for player 1 and on the right for player 2, the screen darkened. One of
  four lines has a lever that works from here; the three others are quiet and say what they are waiting for.
  See `docs/fv2-console.md`.
- **Start in a fight no longer tears the screen.** The warning had been written since the 21st without
  anybody having looked: the title screen request moved the screen without stopping the game, and the title
  came back hashed. Outside the attract, this layer now does nothing and leaves the press to the game.

### Three bugs, all of them mine

- **`ADVERTISE SOUND` was writing into a match's number of rounds.** `0x0058142C` is not its cache but the
  **match point**; my morning's measurement had taken a coincidence for a measurement (1 + 1 = 2 =
  `MATCH COUNT 1P`). Touching the setting shortened every match that followed. Removed, and nothing replaces
  it: the game recooks that word at the start of each match.
- **`CONTROLLER`'s mirror was swallowing what it was meant to reflect** — left and right changed player,
  punch confirmed, kick exited. Fixed by removing what had to be bound, not by rebinding it.
- **A binding capture shut the pad in**: only Backspace cancelled, so on a pad every button bound. Found
  because Frédéric's `p1_pad_kick` came back on `start`.

### What was read in Ghidra

- **Sega's eighty state names**, with their handlers, in `docs/fv2-etats.md` — it was Frédéric who said to go
  and read them. The ending's seven say what it is: **replays**.
- **The live statistics table** at `0x0011F7E0`, step `0xD4`, found through the cabinet's `VS DIAGRAM` screen;
  and `FUN_000701C8`, the routine that files a match away, with the kind of ending at `0x00581452` and the
  winner at `0x00581453`. In `docs/fv2-flux-de-match.md` §8.
- **`SKO 1` against `SKO 2`**: the reading is recorded **and marked as unconvincing**, Frédéric not agreeing
  and his objection holding. A Super K.O. played with the setting off would settle it.

### Tooling

`VF3_OPTION_LOG`, `VF3_TIMER_LOG`, `VF3_LIVE`, a dozen routes in `tools/scripts/`, and a **lock on
`verifier-vf3.cmd`**: two runs launched together shared their files and manufactured a false `DIFFERENT` on
the fidelity contract.

### What remains open

- **Leaving a fight properly**, by the game's road. That is what blocks three of the pause menu's four lines,
  and it is the real next piece of work.
- `CHANGE STAGE` is wired to the picker TRAINING uses; it has not been seen changing a stage mid-round, and it
  is probably the other lever that is needed.
- `BGM SET`, the fork's music packs.
- The black fighters of 2026-10-23.

## Journal for the morning of 2026-10-24 — the upstream merged

`mr-finster/vf3-recomp` was six commits and the tag `v0.1.6-alpha` ahead. All of it went into `fv2`.

### What the upstream brings

- **Vulkan first under Windows**, for every card, NVIDIA's Direct3D 12 being far slower too; and a
  `Display > Graphics driver` setting to go back to the other one.
- **The backups written off the game's thread** (`bg_saver`): each dump to a hard disc cost tens of
  milliseconds, so a hiccup every few seconds while the cabinet's bookkeeping stirred the backup RAM. The
  NVRAM now goes out once a minute, and everything at the moment of quitting.
- **A seventh shader, `FS_COLOUR`**: the whole picture put through an arcade monitor's colours.
- Three menu fixes (a click that closes the menu no longer crashes, `<` goes down one notch, the bounds wrap)
  and `docs/HANDOFF.md`.

### The three files that needed a hand

- **`.gitignore`** keeps both lists: ours, everything this port leaves beside itself when it is launched from
  here, and theirs, Wrangler's cache and the neighbouring ports.
- **`README.md`** keeps ours: the section the upstream adds points at this fork, and it has nothing to say
  here.
- **`src/gpu/r3d_shaders.h`** is a **generated** file, and DXC is not on this machine. The two sides were not
  treading on each other for all that: ours had recompiled the six shaders for the unclamped light
  (`FS_POLY`), the upstream adds a seventh whose code our change does not touch. So our header, the upstream's
  three `fs_colour` arrays grafted before the footprint line, and the sha1 recomputed on the merged
  `r3d.hlsl`. **To be regenerated in one go** by `tools/gpu/shaders.sh` on a machine that has DXC and
  SPIRV-Cross, so that the header becomes the generator's honest output again.

### Verified

`verifier-vf3.cmd 2000` against the new `main` (`d30b722`), the reference tree `C:\Temp\vf3up` advanced and
its binaries rebuilt: **four IDENTICAL** — the PowerPC's state frame by frame, the 8 MB of RAM, the frames
rendered, the complete host's picture — and **2231 identical frames** between the interpreter and the
recompiled code. Virtua Fighter 3 has not moved. And `vf3run` says at last what Start mid-fight means
(`start_for::pause`), which its `switch` had been leaving out since the pause menu was built.

## The branches, laid down on 2026-10-24

Frédéric tried the bare version and it runs at full speed, the working branch does not. So a **reference copy**
now exists, and it does not move any more:

| branch | what it is | what is done on it |
|---|---|---|
| `main` | the upstream, `mr-finster/vf3-recomp`, mirrored | it is advanced by merging from the upstream. **Never a commit of ours** |
| `fv2-stable` | the bare, fast version: `889a691`, "Fighting Vipers 2 runs", tagged `fv2-stable-1` | **nothing**. We do not push to it. It only advances the day a working version is both complete and fast, and then it is tagged `fv2-stable-2` |
| `fv2` | the work in progress, everything we add | the everyday branch |
| `fv2-<subject>` | one worktree per subject (`git worktree list`) | one piece of research at a time, merged into `fv2` when it comes off |

**A pact between branches, laid down on 2026-10-24.** `src/gpu/r3d_shaders.h` is a **generated** file, and no
machine here has a DXC capable of SPIR-V. So the header `fv2` and `decor-test-dreamcast` carry is a **graft**
— our six shaders recompiled with the light clamping `665b40c`, plus the upstream's three `fs_colour` arrays —
and not the generator's honest output. It verifies without anything being believed: the generator writes into
the header the sha1 of the `r3d.hlsl` that produced it, so

```
python -c "import hashlib;print(hashlib.sha1(open('src/gpu/r3d.hlsl','rb').read()).hexdigest())"
grep -o 'source_sha1\[\] = "[0-9a-f]*"' src/gpu/r3d_shaders.h
```

must agree, and `grep -c 'alignas(4) static const uint8_t r3d_'` must return **21**. The consequence: **the
day a branch modifies `r3d.hlsl`, the others no longer have valid blobs.** Whichever one touches it warns the
others. (That is how the Dreamcast scenery branch recovered the light clamping an upstream merge had taken
from it: it took our header, `2e8a285`.)

The reference copy is at **`C:\Users\frede\fv2-stable`**, no longer in `C:\Temp`: a temporary folder is no
place for what must not be lost. It has its own `settings.ini`, `backup-fvipers2` and `eeprom-fvipers2` (free
play), and a shortcut on the desktop, **Fighting Vipers 2 - version stable**, pointing at its
`jouer-fv2.cmd`.

### The pace, measured at an equal offset on 2026-10-24

The same route, no input, `--rec`, `VF3_PROF`, the same frame numbers. The attract's demonstration is a real
fight played by the machine, so it measures what a fight costs:

| frame window | `fv2-stable` (889a691) | `fv2` as of 2026-10-24 |
|---|---|---|
| 1500 | 2.64 ms | 4.27 ms |
| 2000 | 3.12 | 10.03 |
| 2500 | 2.66 | 5.96 |
| 3000 | 2.86 | 6.36 |
| 3500 | 2.55 | 4.19 |
| 4000 | 2.94 | 9.25 |
| **mean** | **2.78 ms** | **6.68 ms** |

And it is **not** the upstream merge: the two `vf3run` from before and after the merge give 16.74 and 16.24 ms
on the attract, so level. It is one of our own additions between `889a691` and today, and it is not named yet.
A bisection was attempted and **its verdict is false**: it judged the merge commit itself "fast", and that one
measures 6.68 ms, so its harness was measuring a stale binary. To be redone, making sure each step really
rebuilds and that the judge prints its figure.

In the window, a fight costs still more than those 6.68 ms — 15.5 ms with sound, 16.37 without — so a second
share will remain to be looked for in the host once the first is named. The sound's crackle follows from it:
SDL's queue falls to 1300 bytes, that is 7 ms of lead.

## The slow game: a `getenv` per CROM read

Found on 2026-10-24, after Frédéric reported fights under 50 frames/s and sound that crackled.

### The cause

`Model3::read32`, the memory read path, carried my `VF3_CROM_WATCH` probe **with no caching**:

```cpp
if (const char *w = getenv("VF3_CROM_WATCH"))      // on EVERY word read from the CROM
    if (a == uint32_t(strtoul(w, nullptr, 16)))
```

Millions of calls per frame. The rest of the port puts `static const bool ... = getenv(...)` everywhere —
`VF3_DMA_LOG` does it in the same file — and I did not follow it at the hottest place. The address is now read
once into a `static`. In passing, `VF3_BANK_LOG` in `set_crom_bank` was also paying for its `getenv` before
testing whether the bank had changed.

### The measurement, at an equal offset

The same route, no input, `VF3_PROF`, the same frame numbers. The mean of the demonstration's six windows:

| | one frame |
|---|---|
| `fv2-stable` (889a691) | **2.78 ms** |
| `fv2` before the fix | **6.68 ms** |
| `fv2` after | **2.81 ms** |

The working branch has come back to the bare version to within 0.03 ms. **The 3.9 ms were that one
`getenv`.**

### What that corrects in the reading we were making of it

- The bare version was not faster **because it has fewer things**. The portraits, the HD, Honey's costume and
  the console menu cost nothing per frame. It was faster because it did not have the probe. Bringing the
  additions back one by one from `fv2-stable` would have cost weeks to arrive at one line.
- The sound's crackle followed from it: SDL's queue was falling to 1300 bytes, that is 7 ms of lead, because
  the game was not keeping the pace. The sound bench costs only 1.6 ms per frame, and the upstream's SCSP has
  nothing to do with it — its WAV dumps show neither clipping nor a skipped sample.
- The upstream merge had nothing to do with it either: 16.74 against 16.24 ms on the attract, level.

### The lesson for next time

**An environment probe in a hot path is read once.** And: look for the cause in the code rather than in the
history. The bisection launched that day returned a false verdict — it judged the merge commit "fast", and
that one measured 6.68 ms — where a diff of the hot path's files between `fv2-stable` and `fv2`, then
`git diff -U0 | grep '^@@'` to see **which functions** the added lines fell into, gave `read32` in two
commands.


## Journal for the afternoon of 2026-10-24 — the sound: where its time goes, and a quarter less of it

Two commits, `1757e83` and `7f732ee`. Six documents in `scsp/docs/scsp_*.md`; what follows is what one needs
to know without opening them.

### What the measurement said, and that nobody had asked for

`vf3run`'s profiler already gave the sound board's cost **as a block**. What was missing was the breakdown
inside it, and that was the whole subject: `scsp/src/sound/sound_prof.h` (`VF3_SND_PROF=1`) gives it, taking
out the nested time, which is not trivial — **the samples are not made in a phase of their own**, the 68000
triggers them from inside its own slices every time it touches a chip register.

On `fv2-fight.txt`, 4200 frames, `fvipers2`:

| | the sound board's own share |
|---|---|
| **the effects DSP** | **62-69 %** |
| the 68000 | 22-24 % |
| the voices | 6-13 % |
| the loop and the timers | ~1.2 % |
| the mixing | ~1 % |

**Two thirds are the effects DSP.** The voices, which one would suspect first, are a tenth. And the DSP
program itself, read by the `dspprog` probe: over its 112 steps, `FRCL`, `ADRL`, `YRL` and `NEGB` are set
**zero times**, and the 33 memory accesses fall **all** on an odd step — so porting MAME's `step & 1` test to
the whole block, which looks like the obvious gain, would have given nothing. Measured before touching it, and
that is exactly why one measures.

### Three batches, and one undone

| | ms per frame | cumulative |
|---|---|---|
| base `335c050` | 1.85 | — |
| the DSP body specialised on the controls used (`Run<U>`) | 1.64 | −11.4 % |
| local copies + `__restrict`, against sound RAM / DSP object aliasing | 1.59 | −14.1 % |
| the eight one-bit controls grouped into a word (`Inst::f`) | **1.40** | **−24.3 %** |

The DSP step goes from 16.3 to 10.0 cycles, the frame's share from 10.6 % to 8.0 %.

**The fourth batch was undone.** `UpdateSlot`'s two-address loop unrolled: identical to the bit, clearer code,
**1.399 ms against 1.400** — zero. A change in a fidelity file that buys nothing is not kept. The negative
result, for its part, is recorded: the voices are not won there.

**And no frame gain is claimed.** In the host the sound board is on a thread of its own, one frame behind:
those 0.45 ms are processor time taken away — heat, battery, cores left to the rendering — not a frame
arriving earlier. It is in `vf3run`, everything on one thread, that the gain reads.

The 1.6 ms of the `getenv` entry above do not contradict the 1.85 here: it was not the same scene. This one
contains a fight, and the voices' share goes up in it.

### The sound has not moved by a sample

That is the condition for keeping a batch, not a bonus. `tools\son\compare-sound.cmd` builds a worktree of a
reference commit in `C:\Temp\fv2-son-ref`, has both `vf3run` play the same scene and compares the WAVs **byte
by byte**:

| game | scene | frames | verdict |
|---|---|---|---|
| `fvipers2` | `fv2-fight.txt` | 4200 (12 879 504 bytes) | **identical to the byte** |
| `vf3` | `vf3-match.txt` | 2400 (6 841 492 bytes) | **identical to the byte** |

### The hole the first batch dug, and that is plugged

Specialising the DSP body cut it into two paths. Both games take the lean one, verified to the byte;
**the other was verified by nothing**. `make test` (`tools/sound/test_scspdsp.cpp`) exercises it: a reference
that decodes the MPRO **inside** the loop, as MAME and Supermodel still do, and sixteen cases that compare
`EFREG`, `TEMP`, `MEMS`, `DEC` and the delay line's 524 288 words after **each** of 64 samples. Eleven cases
reach `Run<USES_ALL>`, including the one that counts: the games' own program with `uses` forced, so **both
bodies must agree on it**.

**And the test has shown that it knows how to fail** — four faults injected and then undone: `NEGB` that no
longer negates (5 cases fall), the loop reading `TWT` where it wants `XSEL` (12), `SHIFT` read one bit too far
(13), `Start` that does not put `uses` back to zero (1, the case written for it). A fifth, swapping two
enumeration values, brought nothing down — and rightly: it is a rename, not a fault. Noted so that nobody
reads it as blindness.

### What was read at the source, and the warning that comes out of it

- **MAME `master` and Supermodel both decode the MPRO inside the loop**, at every step of every sample. This
  repository's decoded form was already ahead of both **before** these batches.
- **MAME's SCSP and Supermodel's come from the same ElSemi.** They are not two witnesses, they are one
  witness read twice: their agreement establishes nothing about a real YMF292-F. Any fidelity question settled
  by "MAME and Supermodel do the same" is an **unsettled** question.
- **MAME has stopped mirroring MADRS** (`#16527`, merged on 2026-10-19) and has joined this tree's "separate"
  mode on the write side, by its own road and on `doa`. But its **reads** of 0x7C0-0x7FF return MADRS where
  the separate mode returns what was written, **and nobody has looked at whether either game reads that range
  back.** That is decidable here, and it is not decided yet.
- On the licence side, nothing was missing: MAME is BSD-3-Clause, compatible with the repository's MIT, and
  every file carries its notice. Supermodel, Mednafen, Yabause and Kronos are GPL — **observation only**,
  never a copy nor a translation.

### What remains open

- **The sound's share under `--rec`**: everything was measured on the interpreter. With the recompiler the
  board goes down, so the sound's share goes up, and these percentages do not transpose there. It is the
  measurement that says whether what follows is worth anything.
- **VF3's time**: never measured properly (the harness coughed some up in passing, on windows where the
  machine was moving — not kept).
- **The scenes that are missing**: silence, music only, effects only, the sound test. The two in the middle
  are one word (`VF3_VOLUMES`), with the reservation that they switch the effects leveller on.
- **One test per block out of eight.** The other seven are listed in `scsp/docs/scsp_validation.md` §3b, and
  they, with the two-core replayer, are the precondition for any replacement of MAME's SCSP. Without them,
  writing a standalone core is a gamble — and nothing is pressing: MAME is BSD, and **its code is this port's
  fidelity reference today**.
- **The MADRS mirror kept for VF3**, which now diverges from MAME: undecidable here, there is no Model 3 on
  this desk. Recorded as a hypothesis.
- And the reminder of scale: in `vf3run` on the interpreter, the Model 3 board is 13.8 ms against 1.40 for the
  sound. **If the aim is frames, the sound is not where to look.**

## Journal for the evening of 2026-10-24 — Start, the stage choice, and a slowness with no owner

### What is added to the game

- **Start only opens the menu in a fight now.** The rule is Frédéric's, laid down in two steps, the second
  correcting the first: `FIGHT_INT` and `FIGHT_DSP` open the pause menu, **and they alone**. The
  presentation
  (`ROUND_MASK_*`, `ROUND_*`, `SET_*`, `READY_*`) sees the state **cut short**, by the sub-state request the
  game's handlers pass between themselves. Everywhere else — `JUDGE`, the replay, `VIC`, `NEXT_PROG_SEL`, the
  select screen — the key goes to the game, intact. `7e4f6a3`.
- **Versus has a stage choice**, `src/host/fv2_stage_list.h`, the port's fourth surface. `V.S.` opens a page
  of thirteen entries instead of starting, and it is the chosen stage that starts the match. `ef4c2c7`.

### What was read in Sega's code

- **`STAGE_SEL` is an empty stub on the cabinet.** The two states Sega names `STAGE_SEL_INT` and
  `STAGE_SEL_DSP` share one address, `0x0000BBB4`, which holds a single instruction: `b 0x0000bad8`, the
  dispatcher's exit. The arcade game never had a stage choice. `83f443b`.
- **The dispatcher copies `SUBMODE_REQUEST` into `MODE_SUB` every round**, which is the direct proof of what
  the state cut rests on.
- **August's picker does command a versus match**, contrary to what this port had written. The sentence
  "reached only by a rematch going through the select screen" was true of the first fight of a solo route and
  false of a versus match. Corrected in `console_play.h` with the three measurements.

### THE EVENING'S SLOWNESS IS NOT OURS, and it is not fixed

Frédéric: "it makes the game slow down to 45fps". It was **not** the stage choice page, and that is measured:

| | one frame, in a fight |
|---|---|
| `fv2` in the morning, after the `getenv` fix (`62a856c`) | **2.96 ms** — 57.5 frames/s |
| `fv2` in the evening | **15.0 to 19.6 ms** — 42 to 45 frames/s |
| `fv2` **minus two sound commits** | **60 frames/s** |

The two commits are **`1757e83`** and **`7f732ee`**, an audit of the SCSP's DSP: 149 lines in
`src/sound/scspdsp.cpp`, which runs 128 steps per sample at 44.1 kHz, that is 5.6 million steps a second.
This repository's reflog dates them to 2026-10-24 at **17:13:58** and **17:23:07**, made **in this folder**,
on `fv2`, between two commits of mine.

Checks made in passing, because the stage page was accused wrongly: holding the picker costs nothing (**zero
repatches over 1500 frames**), and the cost is the same with a held stage and with the game's own (15.0
against 15.7 ms).

**What is proposed and not done**: revert those two commits' **code** — `scspdsp.cpp`, `scspdsp.h`,
`scsp.cpp`, `sound_prof.h` — and **keep all of their knowledge**, the six documents `scsp/docs/scsp_*.md` and
the test bench `tools/sound/test_scspdsp.cpp`, which cost nothing at run time. Frédéric asked for a pause
before deciding.

**And a mistake of mine, recorded because it accused somebody.** I named the "shadow handling" session as the
author of those two commits, on the sole strength of a launch folder and a neighbouring time. It checked and
showed the opposite: it works in `C:\Temp\fv2-ombres`, on the branch `fv2-ombres`, which does not contain
those commits, and it has never touched `src/sound/`. **The author remains unknown**, and Frédéric says
nobody is working on the sound at the moment. A coincidence taken for a measurement, on the very day he had
objected to it twice.

### The machine's state at the pause

- **The binary that is running is not the branch.** `build/bin/vf3.exe` from 20:06 is the trial built
  **without** the two sound commits, so fast. Any rebuild will bring the 45 frames per second back as long as
  the question is undecided.
- **The tree is clean** and another session's uncommitted work in `src/sound/soundboard.cpp` — a
  `VF3_SCSP_DSPLOG` probe — was copied before being touched and then put back intact.
- The Ghidra project `FV2-claude` is open on `foo.bin`.

### What remains open

- The slowness above, not fixed, with no owner.
- Fighting Vipers 2's reverberation, never compared with a reference (`docs/fv2-console-son.md`).
- Leaving a fight properly, which blocks three of the pause menu's four lines.
- `BGM SET`, and the black fighters of 2026-10-23.

> **A note on the commit `b0eadeb`.** It carried off, without my meaning it, lines of this journal that
> another session had written and not yet committed — the header "State as of 2026-10-24" and the table of
> sound tools (`VF3_SND_PROF`, `tools\son\compare-sound.cmd`, `scsp/docs/scsp_audit.md`,
> `scsp/docs/scsp_validation.md`). **They are its, not mine.** `git status` showed `REPRISE.md` modified
> before I touched it and I committed the whole file instead of committing only my addition. The guard exists
> and had already been used on the morning of 2026-10-24: `git update-index --cacheinfo`, to stage only one's
> own version of a shared file. The history is not rewritten — it is pushed — so this note is the
> correction.
