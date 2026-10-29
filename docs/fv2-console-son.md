# The mode list's sound — the plan, before writing

> This document is one of the nine on the sound. The front door, which says where everything is:
> [scsp/docs/sound.md](../scsp/docs/sound.md).

Written on 2026-10-21, after **undoing** a first version of the sound effects laid down that same day.

This document is not a description of what exists: none of it is written. It is the list of the questions
that need a **measured** answer before a line is typed, and in what order.


## Why the first version was undone

It worked on the test that came with it and not on the pad. Three patches in an hour, each one on a report
from Frédéric — "no sound effect", "the music does not stop", "it crackles" — and at the end "lots of sound
and behaviour bugs, I could not list them".

Four holes named on rereading, two of them certain:

1. **Escape over the list killed the music for good.** The list closes by four paths; three put the music
   back, the fourth (`main.cpp`, the menu opened over the top) did not and threw away the sound waiting to
   be played along the way.
2. **Start in mid fight cut all the sound, permanently.** The title-screen request carried an `FF`, and in
   `GAME` mode the attract sequencer does not run: nobody ever sends a block of sounds back.
3. **Start pressed repeatedly outside the title screen sent one `FF` per press**, and `FF` restarts a sound
   still in progress before cutting it (measured on the ROM in August): two presses close together make a
   hiccup.
4. And underneath: the list **freezes the PowerPC** while the sound board runs on its own, the reset goes
   behind the game's back, the music is put back by hand. A hybrid state this port was not built for.

The lesson is not "it should have been coded better": it is that **a sound is not checked by looking at a
picture**, and that one patch per symptom does not converge. Hence this document.


## What is settled, and will not be remeasured

All of this comes from the work on the ROM (`SupermodelSrc\Docs\MenuConsole.md` §5, `Src\Menu\GameFV2.h`)
and was rechecked here during the undone version:

| | |
|---|---|
| the protocol | `A0 <bank> <index>` into a single port; no DSB on `fvipers2` |
| the banks | `0x10` music, `0x2B` and `0x30` voices, `0x2E` effects |
| opening and choosing | `A0 2E 23` — 388 `sd_sign_16` |
| moving | `A0 2E 26` — 391 `sd_ketsu_00` |
| cancelling | `A0 2E 27` — 392 `sd_sonic00_11` |
| stopping | `FF`, the MIDI system reset, which the game sends itself at the head of every block |
| the reset's trap | `FF` **restarts** a sound still in progress before cutting it |
| the stop delay | 80 frames, so that the reset survives the opening beep |
| the music's return | 20 frames after closing, and **only on cancellation** |
| the last music | read by remembering the last `A0 10 nn` the game asked for |
| running the board on its own | possible: its 68000 and its SCSP are already on their own thread |
| the rate | 57.52 frames per second, on an accumulator — not "one per pass if it is due" |

Measured here and kept: the sound board does run under a frozen game (57.5 to 57.8 per second, a queue of 4
to 9.5 kB), the `FF` does stop the music (peak 6201 before, 0 after), and the music put back really was
`A0 10 07`.


## The questions to settle BEFORE writing

### 1. Should the game be frozen under the list?

That is the mother question, and the other three depend on it. Three possible answers, and none of them is
free:

- **freeze the PowerPC, run the sound board** (what was undone): the picture stays the title screen, but
  the game and its sound driver are in two different times;
- **freeze everything** (what is in place today): no hybrid, and the list is mute;
- **freeze nothing**: the sound becomes the port's normal path again, nothing special to write — but the
  title screen leaves after 192 frames and the list finds itself over a demonstration.

To be measured in order to settle it: *what is the game actually doing while the sound board runs on its
own?* The question has never been asked. A record of the MIDI bytes the game sends, frame by frame, before
and after a freeze, would say.

### 2. What should happen on each exit from the list?

There are four exits and each needs a line, written before being coded:

| exit | the music | the accompanying sound |
|---|---|---|
| a mode chosen (ARCADE, SURVIVAL) | ? | ? |
| a line with no lever | — (the list stays) | ? |
| cancellation (B) | ? | ? |
| Escape, the port's menu over the top | ? | ? |

Hole no. 1 above is exactly one empty cell of that table.

### 3. What should Start do **outside** the title screen?

Today: it asks for the title screen, and nothing else. The undone version added an `FF` to it, which
created hole no. 2. The cases to tell apart, because they do not resemble each other:

- from the attract (logo, demonstration, rankings): the sequencer is running, it will send a block of
  sounds back at the next screen change;
- from the selection screen: to be measured;
- **from a fight**: the sequencer is not running. If we cut, nobody turns it back on. And the match is
  still running under the title screen — the known hole, documented in `docs/fv2-etats.md`.

### 4. How is each step checked, with no ears?

The rule that was missing. For each step: **a measurement that tells the successful step from the failed
one**, and not just "the program did not crash".

The instruments already exist in the host: `VF3_QUIT_AFTER_S` (finish on the clock), `VF3_KEYS_S` (send a
key at a given second, the only way to drive something drawn over a frozen game). What is missing:

- a record of the MIDI bytes the **game** sends, frame by frame;
- a record of each frame's peak **on both sides** of the freeze;
- and above all a test that asks the question in a state where the answer is legible. The undone version's
  first test opened the list on the **first** title screen, the one after the SEGA logo, where there is only
  the announcer's voice — which stops by itself. The silence after the reset proved nothing there: a music
  stopped and a music finished look the same. The test had to be redone on a title screen coming from a
  **demonstration** for the measurement to mean anything.


## Step 1, done on 2026-10-21: what the game really says to its sound board

Recorded with `VF3_MIDI_LOG=N`: every block of bytes **the game** sends, with the mode it was in and the
previous frame's peak; plus one line every N frames, so that the silence is on file too. Nothing is modified
by this recording.

### It speaks very little, except in a fight

| state | blocks sent |
|---|---|
| warning | 1 (frame 82) |
| attract, logo, title, demo | 4 in 1250 frames |
| `SEL_INT` / `SEL_DSP`, the selection | 3 |
| `GAME_DSP`, a fight | **55 in 1700 frames** |

The whole attract loop, 73 seconds, fits in **six blocks**, one per screen change. A fight, on the other
hand, speaks constantly.

### What it sends in the loop

```
frame   82  mode 0            FF                                    the only FF of the whole run
frame  997  ADV               A0 00 01
frame  999  ADV_SEGA_INT      A0 00 01  A0 30 00                     the "SEGA" voice
frame 1193  FV2_TITLE_DSP     A0 2B 00                               the "Fighting" voice
frame 1247  FV2_TITLE_DSP     A0 2B 05                               the "Vipers 2" voice
frame 1385  ADV_DEMO_DSP      A0 10 07                               the demo's music
frame 1257  SEL_INT           A0 2E 24  A0 00 01  A0 10 00  A0 2B 02  effect, silence, music, voice
```

Three things follow from it, and none was expected:

1. **The game never sends `FF`**, except once at boot. Its own silence command is `A0 00 01`, and it puts
   it at the head of its blocks exactly where the August record saw `FF`. This document's line saying "the
   game sends it itself at the head of every block" was true of the August record, not of here.
2. **The title screen has no music of its own** — two voices, nothing else. The music one hears there is
   the demonstration's, left running. That is exactly what Frédéric reported.
3. **The selection screen sets its own music** (`A0 00 01` then `A0 10 00`, frame 1257). So putting the
   music back after a mode is chosen is not only useless, it is harmful — and it explains the August
   measurement where the selection screen inherited the demo's music.

### The two stop commands, measured side by side

Background: the demo's music at frame 2000, mean peak 6025 over the following 360 frames.

| sent at frame 2000 | mean peak afterwards |
|---|---|
| nothing (control) | **6025** |
| `A0 00 01` | **0** |
| `FF` | **0** |

And the reset's trap, taken up with the right instrument — an `A0 2E 23` beep over a silent background,
then the stop command 40 frames later, recorded every 4 frames:

```
frame   beep alone   beep + A0 00 01   beep + FF
 2040         3162              3162        3162
 2044         3119                 0           0
 2072          865                 0           0
 2076          100                 0           0
```

**Neither of them restarts the beep**: both cut it dead. The trap measured in August on Supermodel does not
reproduce on this port's sound board. So **the 80-frame delay is of no use here**, and the stop can go out
as soon as the list opens. (Measured on this port, not on the real board: it is this port that is shipped.)


## Step 2: the answer to the mother question

**Freezing the game and running the sound board is risk-free where the list opens.** Between frame 1247 and
frame 1385 — the whole title screen — the game says **nothing** to its board. So there is nothing to miss
during the freeze: it is not an interrupted dialogue, it is a prolonged silence.

So what broke in the undone version was not the principle, and it matters to say it exactly: it was **the
rate** (one sound frame every other pass, thirty a second instead of 57.5) and **the untreated cases** (four
exits, only three filled). Not the idea of running the board.

**Freezing during a fight, on the other hand, would be another matter**: 55 blocks in 1700 frames. A pause
menu that froze a fight would cut a dialogue in progress. That is noted for the day that menu is written;
the mode list, for its part, only opens on the title screen.


## Step 3: the table of the four exits, to be validated before writing

Every cell is justified by a measurement above, and none is an opinion.

| exit | the music | the sound |
|---|---|---|
| a mode chosen (ARCADE, SURVIVAL) | **nothing**: the selection screen sets its own, frame 1257 | the choice sound |
| a line with no lever | — (the list stays open) | **nothing**: nothing happened |
| cancellation (B) | put back what was playing, if we stopped it | the cancellation sound |
| Escape, the port's menu over the top | **put it back too** — that was hole no. 1 | nothing |

And the stop itself: `A0 00 01`, the game's own command, **on opening and with no delay**, since the reset's
trap does not reproduce here. No `FF`: the game does not use it.

For "put back what was playing": the last `A0 10 nn` the game asked for, which is read in passing from the
bytes it sends. Measured: it really was `A0 10 07`, the demo's music.


## The order

One step, one measurement, one capture or record shown. Not two steps at once — that is what has been paid
for twice on this job.

1. **Record what the game sends**, frame by frame, in the states that count (attract, title, demo,
   selection, fight) and under a freeze. Nothing is written until that record exists.
2. **Answer question 1** with that record, and write it here.
3. **Fill in question 2's table**, cell by cell, before coding.
4. Only then: the three sound effects, **without touching the music**. Measurement: the peak rises on a
   cursor move where it was at zero.
5. The music, **if** step 2 says it is tenable. Measurement: the music plays before, no longer plays after,
   and comes back where the table says it comes back.
6. `verifier-vf3.cmd` at every step, and a game played by hand by Frédéric before moving to the next.


## Step 4, done on 2026-10-22: the three sound effects, without touching the music

Frédéric asked to **skip step 3** — the table of the four exits — and lay down the sound effects. That is
exactly the step that does not depend on it: it does not touch the music, so no cell of that table governs
it.

### What it took first: running the board under the frozen screen

A console surface freezes the PowerPC. A frozen PowerPC starts no sound frame, so **nothing comes out**, not
even a note handed to it. So the host had to run the board itself: `Game::sound_only_frame`, which is
`finish_frame`'s sound half without touching the board's state.

**Through an accumulator, and that is the whole point.** The undone version started one sound frame per
pass, conditional on 17.38 ms having elapsed, which gave thirty a second on a 60 Hz screen where the board
wants 57.5: the queue emptied and it crackled. Here the time is measured and the frames due are made,
whatever the number of passes — with a ceiling of four, so that a dragged window does not set off a flood.

And **only under the console's surfaces**: never under the Escape key's menu nor under a pause, which are
the port's and have always been silent.

### The three sounds

The game's own, found in August by sending notes and listening. Their numbers are `GameFV2.h`'s:

| what happens | the note | the sound |
|---|---|---|
| moving | `A0 2E 26` | `sd_ketsu_00`, no. 391 |
| choosing, and opening | `A0 2E 23` | `sd_sign_16`, no. 388 |
| cancelling | `A0 2E 27` | `sd_sonic00_11`, no. 392 |

Every surface says **what happened** (`want_sound`); the host, which owns the board, says **what it sounds
like**. Neither has to know the other's trade. The drain happens once per pass and not where the events are
distributed, because those blocks `continue` on every key: a note handed in inside would only have gone out
at the next event.

### The measurement, which is this job's rule

`VF3_MENU_SOUND=1` reports what the board produces under the surface. Three `Down`s sent on the clock, list
open:

```
menu sound: 11.45 s,  660 frames (57.6/s), peak 0
menu sound: 12.32 s,  710 frames (57.6/s), peak 2880
menu sound: 12.33 s,  711 frames (57.6/s), peak 4417
menu sound: 12.35 s,  712 frames (57.6/s), peak 4289
menu sound: 12.37 s,  713 frames (57.6/s), peak 3110
menu sound: 12.38 s,  714 frames (57.6/s), peak 678
menu sound: 12.40 s,  715 frames (57.6/s), peak 279
menu sound: 12.48 s,  720 frames (57.6/s), peak 0
```

The other two presses give **the same burst, to the same figure**. So:

- **the rate is 57.6/s**, the board's, and not the undone version's 30/s;
- **the peak rises on a cursor move where it was at zero**, which is word for word the measurement step 4
  required;
- **the silence comes back**, so nothing stays hooked.

### What is still not done

Steps 3 and 5: **the music is not touched**. It does not stop on opening, it is not put back on leaving, and
`A0 00 01` is sent nowhere. The table of exits remains to be filled, and it now needs six more cells than
when it was written: moving within `OPTIONS`, going through a door, going up one screen, changing a value,
`INITIALIZE`, and leaving a child screen.


## What Start really does to the demonstration — measured on 2026-10-23

Frédéric corrected this whole document's framing: "*it is not only the music that must be stopped when one
goes from the demo to the title screen by pressing Start, it is the whole demo that is cut, including the
music. On leaving the title screen, the demo and its music must start again from the beginning.*"

Measured, control included, with `VF3_MIDI_LOG` and `VF3_RAMLOG=580DFC:1,58FD99:1`.

### The first half is already true, and it is not us who do it

| frame | sub-state | what the game sends |
|---|---|---|
| 1385 | `0B` demo | `A0 10 07`, the demo's music |
| **1450** | | **Start: the port asks for the title screen** |
| 1453 | `02` | `A0 00 01  A0 30 00` — **its own silence**, then the "SEGA" voice |
| 1455 | `05` title | `A0 2B 00`, "Fighting" |
| 1523 | `05` title | `A0 2B 05`, "Vipers 2" |

The peak before frame 1453 is 6816 — the demo's music — and zero after. **The demo really is cut, music
included, three frames after Start, and it is the game itself that does it** by restarting its loop. So
there is nothing to write for that, and this plan's step 5 had put the question badly: it spoke of stopping
a music where the game cuts a demonstration.

### The second half is not a defect — **corrected on 2026-10-23**

I had recorded a "real defect" here. There is none: Frédéric answered that **the alternation between the
replay and the demo on leaving the title screen is normal**. It is what the cabinet does, and what the game
already did before we touched it.

So what the measurement saw is accurate but was misinterpreted:

```
control (no Start)    01 02 03 04 05(title) 0B(demo)
with Start at 1450    01 02 03 04 05(title) 0B(demo) 00 01 02 03 04 05(title) 08 09 0A(replay)
```

The second pass through the title screen gives the replay because it is **the replay's turn**, not because
our restart disturbed anything. And `0x00580DFC`, the list's index, really is 0 in both cases: it had
nothing to do with that choice either.

### What decides, since we found it on the way

This document's table already said where to look — "*`at FV2_TITLE_DSP = B or = 8`: towards the
demonstration, or towards the replay*" — and the rule of opening the documents before measuring held: it
took finding two instructions, not an evening.

`vf3run --watch 58FD98` gives the two sites: `0x0000B480` writes `0B`, `0x0000B4C4` writes `08`. And the
test that chooses is just before:

```
0000B450  lis   r3,0x0029
0000B454  lbz   r3,10517(r3)      ; the byte 0x00292915
0000B458  cmpwi r3,1
0000B45C  bne   0x0000B4B8        ; different from 1 -> REPLAY, which writes 08
                                  ; equal to 1      -> DEMO,   which writes 0B
```

**A single byte, `0x00292915`, governs the alternation.** At 1 it is the demonstration, otherwise the
replay.

And it **flips by itself, on entering the title screen** (sub-state 4), which is the alternation itself.
`VF3_RAMLOG=292915:1,58FD99:1`, the two passes of one run:

```
frame 1193  byte = 01  sub-state 04   first title    -> 1386  sub-state 0B  the demo
frame 1647  byte = 00  sub-state 04   second title   -> 1850  sub-state 08  the replay
```

The game flips it itself every time it arrives at its title. Our restart does not touch it and does not have
to.

And a neighbour, for the file: just above, `0x0000B444` reads byte 55 of the settings block — that is
`0x00120557`, **`COUNTRY`** — and if it is 1 (`U.S.A.`) goes to sub-state 6, `ADV_RATING_INT`, the
classification screen. So the region governs one more attract screen, which was written nowhere.

### What that leaves to do

**Nothing on that path.** Start cuts the demo, the game does its housekeeping, and what starts again
afterwards is what was supposed to start again. Step 5 keeps its only real subject: **the music that goes on
playing under the open list**.


## Two defects heard, and why the question was badly put — 2026-10-23

Frédéric played and reported two things: "*if one interrupts the demo or the replay by pressing start, the
title screen appears but with the SEGA logo's sound*", and "*the menu's sound effects do not play live but
delayed*". Both are real, and neither was in my measurements — because my measurements read what the board
**produces**, not what comes out of the speakers.

### 1. The logo's voice was arriving on the title screen

It had been in my own record since the day before, without my hearing it: at frame 1453, three frames after
Start, the game sends `A0 00 01  A0 30 00` — its silence, **then the "SEGA" voice**. The port skips the
logo's screen and asks for the title; it was not skipping its sound. **Skipping a screen and keeping its
sound is skipping by half.**

Corrected by dropping bank `0x30`'s notes **on the one route where the logo is skipped** — `skip_logo > 0`
in `console_play`, that is to say exactly there and nowhere else. The title's voices (`A0 2B 00`
"Fighting", `A0 2B 05` "Vipers 2") are not touched: that screen really is shown.

Measured, list opened just afterwards: the peak goes from **8190, 10588, 10396, 8128… sustained** to **2585
falling back to 200 in 0.13 s** — what remains being the end of "Fighting", which must remain.

### 2. The sound effects were late, and it was not the board

The board was making them on time: measured at 57.6 frames per second, the peak rises on the key's frame.
What was late is **SDL's audio queue**, in which the game had got ahead before freezing. A beep placed
behind a second of lateness comes out a second later, and nothing drains that lateness during the freeze,
since the pump only makes what real time asks for.

Corrected by **draining the queue on entering the surface**: the menu starts from silence, and its sounds
are immediate.

### 3. And the music: the question really was badly put

Measured on the route Frédéric describes — Start cuts the demo, the list opens on the title screen that
follows:

```
no "the console's music" line: nothing was playing, so nothing was stopped
```

**There is no music to stop under the list.** The game has already cut it by restarting its loop, three
frames after Start. The whole scaffolding of "stop, then put back on leaving" answers a question that does
not arise on that path.

It is written and measured, and it **keeps quiet**: it follows the music in the game's bytes (`A0 10 nn`
sets it, `A0 00 01` clears it), and if nothing is playing it does nothing and says so in the log. It would
still be useful the day the list opened on a screen where music is running. On the attract loop as it
stands, it never fires.


## The victory screen: a sound that is born and swells — RESOLVED on 2026-10-24

Frédéric: "this sound that starts at the WIN screen and grows to be very loud, **is not normal**". He is
right, and it is neither a loud jingle nor a badly set volume. Here is everything that is measured,
everything that is eliminated, and where it stops.

### The phenomenon, quantified

Measured with `VF3_MIDI_LOG=1`, which gives per frame the mode, the sub-state and the peak of the sound
made, over real games of Frédéric's played through to the end. The sub-state is **18, `VIC_DSP`**
(`docs/fv2-etats.md`) — the *match* victory, not the end of a round.

At `--volume 25`, hence gain exactly 1 and **the limiter out of circuit**, the sound board's raw level:

| state | frames | raw peak | % of full scale |
|---|---|---|---|
| **`VIC_DSP` (18)** | 256 | **13805** | **42.1%** |
| `JUDGE_DSP` (14) | 384 | 4107 | 12.5% |
| `FIGHT_DSP` (12) | 2003 | 3942 | 12.0% |
| `REPLAY_DSP` (16) | 510 | 3441 | 10.5% |
| attract | 1478 | 4117 | 12.6% |

The whole game lives between 10 and 13%; the victory is at 42%. And it **does not get there at once**: the
sound is born at 2.3% on entering the state and rises to 36% in under two seconds — **24 dB** — then levels
off around 33–41% until the game sends its own silence (`A0 00 01`). An `A0 11 0D` command arrives in the
middle of the rise; replayed on its own outside that state (`VF3_MIDI_SEND=A0110D@1500`), it produces
nothing.

### Why it sounds so bad

`LEVEL = 4.0` in `src/host/main.cpp` raises the board's output by **12 dB**, on the grounds written in its
comment that it "peaks well below full scale" — true of Virtua Fighter 3, for which it was chosen. At 42% of
raw, the victory asks for 168% of full scale, and the limiter (`CEILING = 0.891`, that is −1 dBFS =
**29196**, instant attack, 0.1 s release) takes 4.7 dB off continuously. Measured: **120 frames stuck
exactly on 29196**. A limiter working flat out for two seconds is what one hears.

So the limiter is not the defect, it is what reveals it. Lowering `LEVEL` for FV2 would **mask** the rise
instead of removing it: that is explicitly what Frédéric refused, and he was right.

### THE CULPRIT IS NAMED: two voices, c0/s18 and c0/s19

Both chips already carried the measurement (`scsp.h`: `m_probe_direct`, the energy each slot has sent) and
**nothing was turning it on**. It is now wired: `SoundBoard::energy_on` / `energy_take`, and
`VF3_ENERGY_LOG=N` in the host prints every N frames the voices that are sounding, their total energy, and
the six loudest with their share. On a game of Frédéric's:

```
 frame | voices | total     | the loudest
  4724 |     13 |  2.98e+09 | c0/s13 31% c0/s17 16% c1/s24 14%
  4748 |      5 |  2.12e+10 | c1/s12 39% c0/s18 32% c0/s19 20%
  4796 |      4 |  5.34e+10 | c0/s19 56% c0/s18 40%
  4892 |      4 |  7.28e+10 | c0/s19 61% c0/s18 37%
  4940 |      4 |  1.49e+11 | c0/s19 57% c0/s18 42%
```

**Slots 18 and 19 of the first chip carry 95 to 99% of the energy, and their total is multiplied by fifty —
17 dB — in a second and a half.** It is not voices stacking up: it is a **pair** running away while it
plays. Two voices whose output grows without being given another order is the signature of cross
modulation: the modulation reads `m_RINGBUF`, which holds the recent outputs, so one voice can modulate
itself with the other's sound — a feedback loop, whose gain is the depth.

### Four hypotheses measured and fallen

1. **The three changes to the SCSP from upstream** (FM depth doubled, `PLFOS`'s low bit removed, timers
   realigned). Two games compared, the board from before and the board from after: peak 29196 on both sides,
   182 against 181 frames above 20000. **That comparison is worth nothing** and it is a mistake not to
   repeat: both games were at full volume, hence **both against the limiter's ceiling**, where a 10 dB
   difference in the raw signal gives the same figure. Upstream is neither accused nor cleared; the
   comparison has to be redone at `--volume 25`. The binary is ready: `vf3-son-avant.exe` in the working
   folder.
2. **The accumulation in the DSP's effect registers.** `EFREG[EWA] += SHIFTED >> 8` in
   `src/sound/scspdsp.cpp`, and `EFREG` is only cleared at the chip's initialisation, never between two
   samples. Clearing them on every sample (a test switch) **changes nothing at all**: peak 13805 and mean
   8234, against 13805 and 8238 without. So not only is that not the cause, but **the DSP contributes
   practically nothing** to this sound. The switch was removed. (The code comes from MAME and the reference
   copy `fv2-stable` has the same line.)
3. **Voices stacking up without being freed.** `VF3_KEYON_LOG`: 81 voices started during `VIC_DSP`'s 256
   frames, spread over different slots, varied samples. Ordinary.
4. **The envelopes.** The voice log now prints their shape (`tl`, `ar`, `d1r`, `d2r`, `rr`, `dl`, `eghold`,
   `lpctl`, `lsa`, `lea`):

   | | voices | `rr`=0 | `d2r`=0 | looping | mean `tl` |
   |---|---|---|---|---|---|
   | `VIC_DSP` | 81 | 8 | 81 | 81 | 49 |
   | `FIGHT_DSP` | 1153 | 36 | 1153 | 1153 | 58 |
   | attract | 693 | 0 | 648 | 693 | 51 |

   The whole game plays looping with no sustain decay: that is its way. The victory's voices are slightly
   louder (`tl` 49 against 58, a few dB) and slightly more numerous in never releasing. Nothing that comes
   near 24 dB.

### What was tried as a fix, and removed

Giving Fighting Vipers 2 back MAME's depths, leaving Supermodel's only to Virtua Fighter 3 — they are, by
the comment's own admission, "as Supermodel's, **set by ear for VF3**", and doubling the depth of a feedback
loop is exactly what takes a bounded resonance to a rise all the way to saturation. Built, played: **the
defect is still there**. So the change was **removed** rather than kept — an unmeasured change to the sound
does not stay in the tree. The reasoning itself remains valid and deserves to be taken up again with the
right measurement (point 1 above).

### Where to resume

The question is now narrow: **two slots, 18 and 19 of the first chip, during `VIC_DSP`.**

1. Read their registers — `SoundBoard::reg_word(0, 0x000 + slot * 0x20 ...)` gives a slot's words — and
   look at `MDL`, `MDXSL`, `MDYSL`: do 18 and 19 modulate each other, or themselves?
2. Redo the before/after merge comparison **at `--volume 25`**, the only one that makes sense.
3. If it really is a pair in feedback, the loop's gain is in `smp >>= 0x18 - (MDL & 0xE)` and in
   `m_RINGBUF`'s indexing (`SCSP_FM_DELAY`): that is where to measure, not by ear.

### The tools left in place

- `VF3_ENERGY_LOG=N` — which voice makes the sound, per frame (`energy_on` / `energy_take`,
  `soundboard.h`).
- `VF3_KEYON_LOG=file` — every voice started, **with its envelope and loop shape**.
- `VF3_MIDI_LOG=N` — what the game sends the board, with its state and the previous frame's peak.
- `tools/scripts/fv2-vic-banc.txt` — a route that **does not get there**: it does enter a fight in `vf3run`
  (credits paid, V.S. chosen) and the round clock forced to zero (`VF3_POKE` on `0x00581404`) does end the
  round, but the game then stays in `JUDGE_DSP` and `VIC_DSP` is never reached. And `vf3run` runs at 14
  frames per second on this game: six minutes per attempt. **An out-of-game bench for that state remains to
  be found**; until then, the only route to it is a game played by hand.

### What MAME and Supermodel make of this defect (recorded on 2026-10-24)

Frédéric asked whether the defect already existed in the emulators and how they had solved it. **Yes, and on
this game precisely.**

- **Supermodel carried the same problem, and named it.** Its `LegacySoundDSP` option is documented as
  "legacy SCSP **DSP** implementation for games that do not play well with the newer one (**e.g. Fighting
  Vipers 2**)", and it is set **per game** — Fighting Vipers 2 and Sega Rally 2 are the examples cited. The
  "newer" engine is MAME's, ported there in r814; the old one is ElSemi's. So when MAME's DSP came in, this
  game needed a return to the old one.
- **Then they fixed it for good.** The current `Src/Sound/SCSP.cpp` has removed ElSemi's path, and its
  header says why: Fighting Vipers 2 "used to need ElSemi's older DSP register handling, which has been
  removed now that **DSP memory access follows the manual**".
- **Three differences from our modulation**, theirs taken from the manual (table 4.17) and not from the
  ear:

  | | Supermodel (the manual) | us (MAME + upstream) |
  |---|---|---|
  | when the modulation applies | `MDL` **5 to F** only; 0 to 4 = none | as soon as `MDL`, `MDXSL` **or** `MDYSL` is non-zero |
  | the shift | `>> (0x19 - MDL)` | `>> (0x18 - (MDL & 0xE))` |
  | the address offset obtained | **clamped to signed 11 bits** | **unclamped** |

  The third is the most suspect for what we measure: an unclamped offset reads outside the intended sample,
  and that is the kind of thing that makes a voice grow into noise.
- **And MAME fixed a DSP addressing defect that we keep active.**
  [mamedev/mame#16527](https://github.com/mamedev/mame/pull/16527), "Don't mirror MADRS at 0x7c0-0x7ff,
  fixes corrupted and then silent sound in doa". But `src/sound/scsp.h` keeps that mirror **by default**:

  ```cpp
  const bool m_madrs_mirror = [] { const char *e = getenv("VF3_SCSP_MADRS_SEPARATE"); return !(e && *e == '1'); }();
  ```

  So MAME's corrected behaviour is only reachable through an environment variable, and this port runs by
  default what MAME repaired — in the very area Supermodel says it had to bring into line with the manual.

**So the first thing to try is `VF3_SCSP_MADRS_SEPARATE=1`**: one variable on the existing binary, and it is
MAME's fix. All of this is also in the issue opened on that occasion,
[penpenlovesrei-dotcom/vf3-recomp#1](https://github.com/penpenlovesrei-dotcom/vf3-recomp/issues/1) — the
fork's issues were disabled and were enabled for it.

Sources: the `LegacySoundDSP` of Supermodel's published configurations, the Libretro-Supermodel core's
documentation ("MAME-derived" and "ElSemi" engines), the header of `trzy/Supermodel`'s
`Src/Sound/SCSP.cpp`, and MAME's pull request above.

### RESOLVED: the mirroring of `MADRS` was crushing the DSP's delay lines onto one address

The cause is the mirroring of the DSP's `MADRS` table at `0x7C0-0x7FF`, and **the comment that accompanied
it in `scsp.h` already contained it without anyone having connected it to this defect**:

> "VF3's sound program sets its reverb's addresses, then clears that range, which **with the mirror leaves
> them all at one place: the reverb short** […] kept apart, VF2's way, they **rang on for several**"

With the mirror, **every entry of `MADRS` falls in the same place**, so each of the DSP's delay lines reads
and writes the same buffer. On the victory screen, that loop feeds on itself: the sound is born, grows by
24 dB and saturates. The two voices the energy probe named, `c0/s18` and `c0/s19`, are the two taps that
ended up at the same address.

**The measurement, at equal context** — the same code, the same volume, two games of Frédéric's, with the
mirror and without:

| state | peak WITH | peak WITHOUT | mean WITH | mean WITHOUT |
|---|---|---|---|---|
| **`VIC_DSP`** | **29196** | **16506** | **22831** | **6532** |
| `FIGHT_DSP` | 16980 | 16290 | 6714 | **6979** |
| `JUDGE_DSP` | 16913 | 18386 | 8109 | 7654 |
| `REPLAY_DSP` | 14023 | 14749 | 6575 | **6923** |
| `SELECT` | 11101 | 10708 | 5800 | 5858 |

The victory screen goes from a mean of 22831 to 6532 and leaves the limiter's ceiling; **all the other
states stay where they were**, the fight's mean even rising slightly. Frédéric, who played both: "the bug
has been fixed, even if I find the sound effects are lower". That is not a loss of level — the measurement
says the opposite — but the reverb, which is no longer crushed into one point and gets its real delay lines
back: the hits have a tail instead of a concentrated crack.

### The fix is PER GAME, because both measurements are good

Upstream measured that the mirror is **right for Virtua Fighter 3**: the credit's and Start's echoes die
there in a second, like the reference, whereas without it they ring on for several. We have just measured
that it is **wrong for Fighting Vipers 2**. Neither measurement cancels the other, so the setting follows
the game, as `driver_known` already does:

- `SoundBoard::madrs_mirror`, set from the set by the host and by `vf3run` (`board.set == "vf3"`), applied
  to the chip at its creation through `scsp_device::set_madrs_mirror`.
- `VF3_SCSP_MADRS_SEPARATE=1` and `VF3_SCSP_MADRS_MIRROR=1` force one or the other, for comparison.
- `verifier-vf3.cmd 2000`: four `IDENTIQUE` and 2231 identical frames — Virtua Fighter 3 does not move.

**A measurement trap met on the way, not to be repeated**: the first build failed (`m_madrs_mirror` was
private) and the checker then ran on the **old** binaries, returning four `IDENTIQUE` that bore on nothing.
The same defect as the morning's bisection: a harness that measures a stale binary always answers "all is
well". Since then, the script stops on a compilation error before checking anything at all.

### What remains open

- **Nobody has ever measured Fighting Vipers 2's right reverb length** against a reference. Upstream
  compared VF3 with Supermodel; we compared FV2 with itself. Supermodel says it brought its DSP's memory
  accesses into line with the manual, and that is the reference one would want to listen to.
- The three modulation differences from Supermodel (table 4.17) remain, and the **clamping of the address
  offset to 11 bits** remains a fix taken from the manual that this port does not have.
- `LEVEL = 4.0`, the 12 dB chosen for Virtua Fighter 3, still applies to this game too. They no longer
  saturate, but they have never been measured for it.

## The menu that crackles: 56 ms of music thrown away on every opening (2026-10-28)

Frédéric: "sometimes the main menu's sound crackles, or rather has an echo".

### What the measurements ruled out first

Three hidden runs played, with `VF3_MENU_SOUND=1`, which prints the device queue's depth frame by frame:

| route | queue (frames) | rate |
|---|---|---|
| the list on the title screen, 65 s | min 2.7 — median 3.0 — max 3.4 | 57.6/s |
| the same, **every core loaded** | min 3.0 — median 3.3 — max 3.6 | 58.1/s |
| the return to the menu from a match | median 3.0 | — |

**The pump does not drop out**, even under load, and the sound recording (`VF3_SOUND_WAV`, new) shows
neither a sample break nor a repeated block. So it was neither a queue starvation nor a doubling of the
audio.

### What they found

Opening a surface **drained the device's queue**, and that is sound the player has not heard yet. The line
had been there since the day the beeps were arriving "delayed", and the figure had never been looked at.
Measured on the arcade route:

```
the console: MENU_IN -> MENU (the game is home)
the console's sound: 3.2 frames (56 ms) were queued and are thrown away
```

**3.2 frames, 56 ms** — that is not accumulated lateness, it is the three frames of lead the game's governor
holds on purpose. In other words, **56 ms of music was being cut in the middle of itself every time a menu
opened**: a jump inside a held note, which sounds exactly like a crackle or an echo.

### The fix

Only the excess is drained — beyond twice what the device wants — and the usual three frames are left alone.
The pump now makes only what is really missing (`max(0, QUEUED - queue)`) instead of priming three frames on
top of an already full queue. The case the first version really did treat, a queue in which the game has run
far ahead, stays treated: that is fast-forward's, eight frames per pass.

So a beep arrives 56 ms after the key, that is three refreshes — nobody hears that.

**After**: not one "thrown away" line left, and the queue never goes down to zero on opening again (min 2.5
frames over 106 readings, against 0.0 at every opening before). `verifier-console.cmd`'s three routes still
pass.

## The menu's beep comes out with an echo (2026-10-29)

Frédéric: "parfois le son du menu principal gresille, ou plutot a un echo", and then, after a session
recorded with `VF3_SOUND_WAV`: "oui et c'est plutot un echo avec gresillement". He heard it in the
recording itself, so it is in what this port hands the card, not in his device.

### The measurement that finally isolated it

Six hidden runs had come out clean, and the reason is itself worth writing down: **in a fresh hidden run
the menu's beep does not sound at all**. The peak over a whole menu stay is 4421, which is the tail of the
SEGA voice; after that, nothing. Bank 0x2E is not loaded until a mode has been played, so every measurement
so far had been measuring the cleanliness of a sound that never played.

What worked was a timed gesture in his own session: open a menu, five seconds untouched, ONE move of the
cursor, five seconds untouched. That gives one transient with a known place and a control either side of it.

### What the isolated beep says

The floor around it is **a constant −492 on every sample, standard deviation zero**, identical on both
channels — so anything that moves is the beep. Its envelope, in 2 ms steps from the attack:

```
   78 - 128 ms   the beep        peak 3672
  284 - 328 ms   a repeat        peak  939
  450 - 484 ms   a third         peak  391
```

About **180 ms apart, each a quarter of the one before**. That is a delay line with feedback, which is the
SCSP's own DSP — the part another session is measuring variant by variant. An effect on bank 0x2E comes out
of this port with a 180 ms echo at about a quarter, and that is the handle.

### Two things read in passing

**The output carries a constant DC.** It creeps in — 0, then −209, −349, −424, −480 — and sticks at −492,
where it stays for minutes with a standard deviation of zero. A direct component is not heard in itself, but
it turns every interruption of the stream into a click of 492, about −36 dB: a queue that runs dry stops
being a gap and becomes a tick.

**Every sample is a multiple of four.** That is `LEVEL = 4.0`, the 12 dB chosen for Virtua Fighter 3: the
board's output is multiplied by four, so two bits of resolution go. Inaudible on its own, noted because it
shows in every figure above.

### And it is OURS: the same sound has no tail through the game's own loop

Frédéric, plainly: "le son doit se jouer une seule fois. Pas de reverb, echo ou autre."

The A/B that settles it. `A0 2E 26` — the menu's own move sound — sent by hand into the game's own frame loop
with `VF3_MIDI_SEND`, the music silenced first with the game's own `A0 00 01`, the board loaded by a match
already played, against the same sound as the pump plays it:

```
                         at the attack   +165 ms   +330 ms   +495 ms
  the game's own loop           1566           0         0         0
  the menu's pump               1564         259       119        56
```

Nothing at all through the game's loop. Three decaying repeats through the pump, 165.01 ms apart — 7277
samples, 9.49 sound frames, which is no round number of anything this port counts — and the repeats are the
SAME WAVEFORM, not an echo of the attack: cross-correlated, repeat two against repeat three gives 0.952 at
zero lag, and the pattern is found again at 265, 430, 595, 640 and 805 ms.

So the board itself produces this when the PUMP turns it and not when the game does. The two call paths
differ in one thing only — `sound_only_frame` does not run the PowerPC — so what the game stops sending
while it is frozen is the next place to look. The 68000 and its DSP go on running without their host.

Measured along the way, and worth keeping for whoever picks this up: of the seventeen indices of bank 0x2E,
0x26 is among the cleanest through the game's loop (no tail at all), so the fault is not in the choice of
sound.
