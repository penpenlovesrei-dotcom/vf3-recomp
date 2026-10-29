# Fighting Vipers 2's states, and the title screen

Nothing here is new. All of it was measured on the ROM between 2026-08-14 and 2026-08-20, and recorded in
`Documents\SEGA Model 3\SupermodelSrc\Docs\MenuConsole.md` (§6, 6 bis, 6 quater, 6 sexies); the addresses are
in `SupermodelSrc\Src\Menu\GameFV2.h`. This document brings them into this port so that they stop being
rediscovered.

Nothing is taken from Supermodel's code: these are facts about the game, not code.


## The game says itself where it has got to

Four contiguous bytes, and that is the whole machine:

```
MODE_MAIN       0x0058FD98   the mode
MODE_SUB        0x0058FD99   the current mode's sub-state
MODE_REQUEST    0x0058FD9A   the mode asked for
SUBMODE_REQUEST 0x0058FD9B   the sub-state asked for
```

The names are Sega's, left in RAM: a contiguous run of 80 strings at `0x00024494`, the first 24 of which are
the modes. **A contiguous table does not say where its slices begin** — slicing it by the names was caught out
three times, and every slice below ended up being measured on screen.

### The 24 modes

```
 0 WARNING_INT     1 WARNING_DSP     2 ADV_INT         3 ADV_DSP
 4 SEL_INT         5 SEL_DSP         6 GAME_INT        7 GAME_DSP
 8 CONTINUE_INT    9 CONTINUE_DSP   10 INTRUDE_INT    11 INTRUDE_DSP
12 ENDING_INT     13 ENDING_DSP     14 NAME_INT       15 NAME_DSP
16 RANKING_INT    17 RANKING_DSP    18 OVER_INT       19 OVER_DSP
20 TEST_INT       21 TEST_DSP       22 RESULT_INT     23 RESULT_DSP
```

`_INT` installs the screen, `_DSP` runs it.

**`MODE_SUB` only means anything for three modes: `GAME`, `SEL` and `ADV`.** All the others leave the byte at
whatever the previous mode had put there. For `CONTINUE`, `OVER`, `INTRUDE`, `ENDING` and `NAME`, reading the
mode is enough.

### The attract's twelve sub-states (ADV), `0x0002455C`

```
0 DONT_RECYCLE_INT   1 DONT_RECYCLE_DSP   2 ADV_SEGA_INT     3 ADV_SEGA_DSP
4 FV2_TITLE_INT      5 FV2_TITLE_DSP      6 ADV_RATING_INT   7 ADV_RATING_DSP
8 ADV_REPLAY_WAIT    9 ADV_REPLAY_INT    10 ADV_REPLAY_DSP  11 ADV_DEMO_DSP
```

Twelve, not ten: measured. And the slice begins at index 50 of the table, not at 52 — the two missing names
were **before**, not after, so nothing that follows slides. Verified by freezing the game sub-state by
sub-state and looking:

| frozen on | on screen |
|---|---|
| 3 (`ADV_SEGA_DSP`) | the SEGA logo, 192 frames |
| 5 (`FV2_TITLE_DSP`) | **the title screen** |

### A fight's twenty sub-states (GAME), `0x000244F4`

```
 0 ROUND_MASK_INT   1 ROUND_MASK_IN   2 ROUND_MASK_DSP  3 ROUND_MASK_OUT
 4 ROUND_INT        5 ROUND_DSP       6 SET_INT         7 SET_INT_POSE
 8 SET_DSP          9 READY_INT      10 READY_DSP      11 FIGHT_INT
12 FIGHT_DSP       13 JUDGE          14 JUDGE_DSP      15 REPLAY_INT
16 REPLAY_DSP      17 VIC_INT        18 VIC_DSP        19 NEXT_PROG_SEL
```

Confirmed end to end by measurement. The replay has states of its own, so **the game says itself when it has
finished showing**, whether the player cut it short or not — which no stopwatch could replace.

### The select's six sub-states (SEL), `0x00024544`

```
0 SEL_START        1 SELECT          2 STAGE_SEL_INT   3 STAGE_SEL_DSP
4 DECIDE_INT       5 DECIDE_DSP
```

### The ending's seven sub-states (ENDING), base 62

```
0 ending_trans_init    1 ending_trans_wait    2 ending_replay_init
3 ending_replay_ctrl   4 ending_replay_next   5 ending_final_mask1
6 ending_final_mask2
```

**Those names say what the ending is: REPLAYS.** Three of the seven speak of nothing else — starting one,
running it, moving to the next. So the credits are not a film: they are the credit roll alternating with
replays of the fights the player has just won, on their scenery, with the place's banner and the "v.s. NAME"
band every replay carries.

That is what explains what is seen when the ending is asked for from the title screen: there is no match to
replay, so the floor stays bare. The detail is in `docs/fv2-console-options.md`.

### The name's three sub-states (NAME), base 69

```
0 NAME_ENTRY_INIT   1 NAME_ENTRY_CTRL   2 NAME_ENTRY_DISP
```

### The SUPER K.O.'s eight sub-states (SKO), base 72

```
0 SKO_REPLAY_WAIT      1 SKO_SET_IMAGE_STAGE   2 SKO_WAZA_WAIT    3 SKO_TRANS_WAIT
4 SKO_IMAGE_DISP       5 SKO_RESET_STAGE       6 SKO_TRANS_WAIT2  7 SKO_IMAGE_STAGE_END
```

`waza` is 技, the move. So a Super K.O. is a little staging of its own: wait for the move, change the stage
(`SET_IMAGE_STAGE`), show it, put the stage back (`RESET_STAGE`). Worth knowing for the VS DIAGRAM's two
cells `SKO 1` and `SKO 2`, which no match has filled yet.

## How those eighty names read

`0x00024494` is not a run of strings but a **table of pointers**: eighty words, each the address of a
zero-terminated name. The twin table `0x00024354` gives, at the same index, the handler's address. Read in a
RAM dump (`vf3run --ram-at`, the game's byte at address A being at index `A^3`), the two together give the
table above and, for each state, the function that runs it:

| state | handler | | state | handler |
|---|---|---|---|---|
| `ENDING_INT` | `0x0000E15C` | | `ending_trans_init` | `0x000725D0` |
| `ENDING_DSP` | `0x0000E270` | | `ending_replay_init` | `0x000727B8` |
| `NEXT_PROG_SEL` | `0x0000DB48` | | `ending_replay_ctrl` | `0x000728C8` |
| `OVER_INT` | `0x0000E2F0` | | `ending_replay_next` | `0x00072958` |

And those two, `0x00072FD0` and `0x00073238`, are the two places in the ending that
`docs/fv2-flux-de-match.md` had already seen rebuilding a fighter's model: they are the replays bringing their
actors back up.


## "Is the title screen on screen?"

```
MODE_MAIN ∈ { 2 ADV_INT, 3 ADV_DSP }   and   MODE_SUB ∈ { 4 FV2_TITLE_INT, 5 FV2_TITLE_DSP }
```

Measured on screen, not deduced from the names.

Not to be confused with `InAttract` (`[0x00700000] == 0`, the `entry1` task's state), which answers an
altogether different question — "is a game running" — and which is true during the warning, the logo, the
title, the presentations, the demos and the rankings. Both are useful, for different things.


## Getting there: ask, do not force

```
MODE_REQUEST    = 3   ADV_DSP
SUBMODE_REQUEST = 4   FV2_TITLE_INT
```

**Written once, never held.** Two frames. The dispatcher at `0x0000ABC4` copies `MODE_REQUEST` into the mode
*before* jumping: so the mode is not forced, it is asked for, and that is why it works. Holding a request is
what broke the select screen and its clock, twice.

Let **sixty frames** pass before freezing on it: the freeze cuts the announcer's voice off (at twenty frames
one hears "Fighting Vipers" without the "2"), and on the first frame of sub-state 5 the fade is not finished,
the SEGA logo still overlapping it. Three hundred is already too late, the screen has gone.

Those sixty frames are **not** a stopwatch in disguise: the state says *which* screen, and that is the signal;
the sixty only clear the fade.

### From a fight, this lever commands nothing

Nor does the other. The attract sequencer's two writes —

```
[0x00580DFC] = 14      the list's last real entry
[0x00580DD2] = 0x7FFF  its timer, expired
```

— work **from the attract** and only there. Tried from a fight, they do nothing, and reading them back says
why: they were still intact a second later. The attract sequencer does not run during a match, so nobody reads
them. *A negative trial must say which of the two: the game overwrote our values, or the game does not read
them.*

Forcing the `entry1` task's state derails the game — tried, measured.

What is left is the game's own road: **end the match without showing or hearing it**, then put the title
screen up once the attract has come back.

```
0x00581455 = MATCH COUNT − 1    the current round becomes the last
0x00581406 = 64                 the clock at the edge : one second displayed
player 1's health = 0           so that at TIME UP the outcome is beyond dispute
```

The clock is set **once per round, never held** — reloaded on every frame it never expires. The round number
`0x00581456` says when a new one begins, and that is the only moment it is rewritten. The health at zero
because at TIME UP it is the one with less who loses, and a draw probably does not count as a round lost: at
zero, there is nothing to suppose.

The K.O., the GAME OVER and the `CONTINUE` countdown run by the game's road, behind the menu's picture, the
sound cut.

### Measured here on 2026-10-21: the request, for its part, gets through from anywhere — but it does not leave the game

What precedes bears on the **attract sequencer**'s two writes. The mode request is another lever, and it had
never been tried anywhere but from the attract. It is now, with `vf3run --modes --console-start N`, which puts
a press of Start through the host's decision and says what it chose, frame by frame.

| Start at frame | the game was | what was decided | what happened |
|---|---|---|---|
| 1100 | `ADV_DSP` 3, the SEGA logo | ask for the title | title screen at **1101**, instead of 1193 |
| 1255 | `ADV_DSP` 5, the title screen | open the list | **nothing written**; what follows frame for frame like the control |
| 1450 | `ADV_DSP` 11, the demonstration | ask for the title | title screen at **1451** |
| 2100 | `SEL_DSP` 1, the select screen | ask for the title | title screen at **2101** |
| 3100 | `GAME_DSP` 12, mid-fight | ask for the title | title screen at **3101** |

The last three are new. So the request commands **the screen** from the attract, from the select screen and
from a fight.

**And it does not end the game.** That is the same trial pushed one notch further: the RAM read back
afterwards says that the `entry1` task (`0x00700000`, "is a game running") stayed at its playing value.

```
from a fight          entry1 = 0x2A before, 0x2A after   (read back at frame 4000)
from the select screen entry1 = 5 before,    0x2D after   (read back at frame 2600)
```

So the title screen comes back over a game that is still running. `fv2_in_attract()` stays false, and
everything that hangs on it — Survival's arming first of all — sees that. Really leaving a fight remains the
game's road described just above, which is not wired.

*A negative trial must say which of the two; a positive trial must say how far it reaches.*


## The tasks that draw the attract — measured on 2026-10-21

Measured because asking for the title screen left a dirty scene behind it. The task blocks are at
`0x00700000`, **one every `0x100`**; the scheduler reads two words of each sixty times a second: **bit 0 of
the first word** says whether the task is running, the next word is its routine.

Measured at the loop's three moments, by dumping the RAM (`vf3run --ram-at`):

| block | title screen | demonstration | replay |
|---|---|---|---|
| `0x00700200` | `00000000 00062144` | `00000000 00062144` | `00000001 00062390` |
| `0x00700300` | `00000000 000621B0` | `00000000 000621B0` | `00000001 00062404` |
| `0x00700400` | `00000000 00015A4C` | `00000000 00015A4C` | `00000001 00015A74` |
| `0x00700500` | `00000000 0003F874` | `01018001 00040134` | `01010041 00040134` |

**On the title screen, all of them are off.** The demonstration runs one, the replay four. The rest of the
region that differs is in floats — coordinates, not task blocks.

`0x00700000` and `0x00700100` are `entry1` and `entry2`, a game's tasks; forcing `entry1` derails the game
(tried in August). They are off in the attract anyway.

### Who turns those tasks off, and when — measured with `--watch`

`vf3run --watch 700200 --modes` names the instruction that writes, without decompiling anything:

```
store32 00700200 = 0 at PC FFF018DC (LR FFF0153C)     the boot, at start-up
store32 00700200 = 0 at PC 0000779C (LR 000076AC)     the same
store32 00700200 = 0 at PC 000390F0 (LR 00038C38)     the same
store32 00700200 = 0 at PC 0000B240 (LR 00039068)     between frames 998 and 999
```

The fourth is the only one that counts: it falls **between sub-state 1 (`DONT_RECYCLE_DSP`) and sub-state 2
(`ADV_SEGA_INT`)**, which is to say at the **restart of the attract loop**. `LR 0x00039068` is the mode
machine's call site (`0x00039064` is `bl 0x0000ABC4`).

**So the game never turns those tasks off during a demonstration or during a replay.** It waits for the loop
to start again. That is why clearing them by hand does not hold in mid-replay: what turns them back on is the
replay itself, which is not finished.

### How the loop starts again: the sequencer's list

`vf3run --watch 580DFC` shows the sequencer walking:

```
store8 00580DFC = 0 at PC 00072EB4 (LR 00072E7C)      set on the title screen
store8 00580DFC = 1..F at PC 00073178 (LR 00072E7C)   one step at a time, during the demonstration
   5866  ADV_SEGA_INT                                 out of the list : the loop starts again
```

The list goes from 0 to 15, and it is `0x00073178` that advances it. Writing 14 and emptying the timer makes
it cross the end on the next step — §6 bis' lever, and here one sees why it works.

**And why it does not work from the replay**: during the replay (`sub 10`), no write of `0x00580DFC` appears at
all. The sequencer no longer advances its list there; the replay runs on its own duration. So pushing the list
has nothing to push.

### What puts an end to the replay — and the clean road, at last

Measured by watching the state machine's word (`vf3run --watch 58FD98`). The mode and its sub-state are
rewritten on every frame by the mode machine, but **`SUBMODE_REQUEST` (`0x0058FD9B`) is only written a handful
of times**, and those writes *are* the attract loop:

```
at WARNING_DSP      = 0, = 1                    the loop starts
at DONT_RECYCLE_*   = 2, = 3                    on to the SEGA logo
at ADV_SEGA_DSP     = 4, = 5                    on to the title screen
at FV2_TITLE_DSP    = B  or  = 8                on to the demonstration, or to the replay
at ADV_REPLAY_DSP   = 0 by 0x0000B7C0, then 1   <- THE REPLAY ASKS FOR THE LOOP TO RESTART
```

**The way the game leaves a replay is not by asking for the title screen: it is by asking for the loop
to start over** (`DONT_RECYCLE_INT`). And that restart is precisely where it turns the drawing tasks off
(`0x0000B240`, between sub-states 1 and 2).

Hence the clean road, which works from anywhere in the attract and in two steps:

1. `MODE_REQUEST = 3`, `SUBMODE_REQUEST = 0` — written **once**: the loop restarts and the game does **its
   own** tidying;
2. once at the SEGA logo (sub-state 2 or 3), where the scene is empty, ask for the title screen.

Measured with the frame probe, from a demonstration **and** from a replay:

```
press at frame 1450 (demo)    sub 0, 1, 2 then TITLE SCREEN at frame 1455 : 0 models
press at frame 2100 (replay)  sub 0, 1, 2 then TITLE SCREEN at frame 2105 : 0 models
```

**Five frames, and 0 models** — the same title screen as the one the game arrives at on its own. Against 397
models for the direct request, and 400 frames of waiting for the sequencer's lever, which commanded nothing
from a replay.

### What that gives, and where it runs up

Turning bit 0 off before asking for the title screen **tidies nearly everything, from the demonstration**: the
title screen goes from **397 models to 4-19**, against 0 for the one the game arrives at on its own.

**But not from the replay**: read back ten frames after the request, the three tasks `0x00700200`,
`0x00700300` and `0x00700400` have been **turned back on by the game**. A clearing written once does not hold,
and holding it is precisely what must not be done.

So the real way out is not to turn those bits off by hand: it is to find **what the game runs** when its
sequencer installs the title screen, and that stops those tasks for good. That is the next measurement, and it
takes decompiling that branch of the sequencer.


## The arcade's fixed route: a table, measured on 2026-10-22

Looked for because Training mode needed to choose its arena, and found without a decompiler: `--watch` names
who writes the scene index, and the twelve instructions before it say where it comes from.

```
000716B8  stb r0,-609(r9)      r9 = 0x00590000, so 0x0058FD9F : the scene index
000716B4  mr  r0,r30           and r30 comes from :

000715F0  lbz    r9,5213(r11)  [0x0058145D], which chooses the route
000715F8  addi   r11,r11,16640 the table, 0x000B4100
000715FC  addi   r0,r8,-1      the fight's number, minus one
00071600  rlwinm r0,r0,3,0,28  times 8
00071604  rlwinm r9,r9,1,0,30  the route, times 2
0007160C  lbzx   r30,r11,r9    <- THE ARENA
00071614  lbz    r3,1(r9)      and the next byte : the opponent
```

**Eight bytes per fight, four (arena, opponent) pairs, the pair chosen by `0x0058145D`.** Measured in a
running game:

```
+00 : 09 0A 00 09 00 09 00 09      fight 1 : arena 9,  opponent 0x0A
+08 : 01 0B 00 09 00 09 00 09      fight 2 : arena 1,  opponent 0x0B
+10 : 07 00 0B 06 00 09 00 09
+18 : 02 02 0A 07 00 09 00 09
```

"9 then 1 for the second opponent": that is exactly what `docs/fv2-flux-de-match.md` had measured from the
other side, without knowing where it came from.

**And it is not the same lever as `CHANGE STAGE`.** The function `0x0007111C`, which August's menu patches to
`li r3,<scene> ; blr`, does **not** command the first fight's arena: patched and held, the game picked 9 all
the same. It is reached only by a rematch **going through the select screen**. For a fight that is starting,
it is the table.

The twelve arenas, numbered by the owner and measured on screen: 1 Old Armstone Town, 2 Deck of the Dark
Royal, **3 Marble Arena**, 4 The Great B.M, 5 Terror Furnace, 6 Serpent's Cage, 7 Hanger of the Dark Royal,
8 Secret Chemical Plant, 9 Cross River Damsite, 10 Neo Armstone City Tower, 11 The Skies of Armstone,
12 Great Museum.


## The list of modes, and its geometry

`assets/menu-console/title_list.png`, 709 x 897, RGBA, seven lines: ARCADE, V.S., RANDOM, SURVIVAL, NETWORK,
TRAINING, OPTIONS. **One version only of each line** — none of the 45 files is a "selected" variant, so the
chosen line has to be marked some other way.

The image was cropped to its ink (709 x 897 out of 1027 x 1024); without that crop the empty space counted in
the height and the whole block shifted.

The seven bands are cut **in the middles of the white**, not into seven equal parts: the labels are 114 to 118
pixels, and equal parts would clip the taller ones.

```
0, 124.5, 254, 381.5, 511.5, 640.5, 769, 897        (out of 897)
```

The labels are painted **flush left** — `V.S.` has its middle at 0.17 where `NETWORK` has it at 0.50 — so each
line has to be slid to bring its ink to the middle of the block.

| line | ink's middle | half-width | columns |
|---|---|---|---|
| ARCADE | 0.4006 | 0.3977 | 2..565 |
| V.S. | 0.1728 | 0.1643 | 6..238 |
| RANDOM | 0.4379 | 0.4320 | 4..616 |
| SURVIVAL | 0.4732 | 0.4673 | 4..666 |
| NETWORK | 0.5035 | 0.4908 | 9..704 |
| TRAINING | 0.4746 | 0.4718 | 2..670 |
| OPTIONS | 0.4281 | 0.4196 | 6..600 |

And its place on the title screen, in fractions of the height, measured on a 3840 x 2160 capture:

```
top    0.26157      height 0.35557      middle 0.5
```

Grown by `7/6` (the block occupies the height seven lines would take, for six drawn) and lifted by `50/1080`,
both on request. Kept as a factor applied to the measurement rather than replacing the measurement by the
figure obtained: both give the same picture, but one keeps the measurement legible and says what was added on
top of it.

The measured vertical scale is 1.0013 and the horizontal 1.1216: that screen is a 4:3 picture shown wide, and
the stretch is its display's, not the game's. **Only the vertical placement is taken from there**, the width
follows the picture's proportions.


## Three mistakes, already paid for over there

- **The style, not the content.** The menu laid over the title screen had been switched to a style that lets
  the game show through — which bypassed the whole branch where the painted labels live, and the lines fell
  back to the menu's font. The remedy is not one more style, it is a **subtraction**: the same style, minus
  its artwork.
- **The button mask.** That path does not open the menu at once, and it is the opening that was withholding
  the inputs. Without it, Start reached the game, which took its credit and opened the select screen while the
  journey waited for a title screen that would never come.
- **Two changes delivered at once**, on a mechanism the document itself said had never been tried. Frédéric
  had no way of knowing which of the two was breaking what, and everything had to be undone.


## The replay that follows Start: the fighters are dark — 2026-10-23

Reported by Frédéric: "*when the replay restarts, the characters' lighting is abnormally dark*". Reproduced
without a window, with a control, and **isolated**.

### It is not a lack of models

The frame log says so at once: the replay afterwards draws **457 to 532 models in 16 to 18 views**, as many as
the control. Nothing is missing — it is all there and badly lit. It is a flaw distinct from that of the models
left on the title screen, even though it comes from the same gesture.

### Nor is it the menu

Three captures, all in sub-state `0A`:

| | how it is reached | the fighters |
|---|---|---|
| control | the game alone, natural replay at frame 6284 | **lit** |
| trial | Start cuts the demo short, **the list opens and is left by B** | **dark** |
| trial with no list | Start cuts the demo short, **and nothing else** | **dark** |

The third is the one that settles it: with no menu having opened at all, the flaw is there. **The cause is
Start restarting the attract loop**, not the console's surface. The menu has nothing to do with it.

The scenery, for its part, is normally lit in all three. It is **the fighters' models alone** that are dark —
bluish silhouettes.

### Where this joins what was already written

The § "What that gives, and where it runs up" says that the clean road tidies everything from the
demonstration "but not from the replay", where the game turns back on the three tasks that had been turned
off, and concludes that what must be found is **what the game runs when its sequencer installs the title
screen**. This flaw adds a symptom to that same question: it is not only what stays on screen, it is also **a
light that is not put back**.

### The next measurement

Find who writes the models' lighting parameters, and compare what they are worth in the control and in the
trial at the same frame of the replay. The reproduction holds without a window, so it costs nothing to
repeat: `tools/scripts` does not have those three scenarios yet, they are in the session's scratchpad.


### What the views carry, control against trial

`VF3_VIEW_LOG=<frame>` prints every view of that frame with the light the game put on it. At the same
sub-state `0A` on both sides:

**Control — the game alone, 15 views, and one light only:**

```
view  0..14   ambient 0.800  sun 1.000 (0.02 0.85 -0.52)     (14 views out of 15)
view 11       ambient 0.227  sun 0.230 (-0.00 0.81 0.59)   73 models
```

**Trial — after Start, 17 views, and THREE lights mixed:**

```
view  0..16   ambient 0.710  sun 1.000 (-0.43 0.83 0.36)     the background
view  6       ambient 1.000  sun 0.020 (0.15 0.10 0.98)    113 models
view  5, 10   ambient 0.251  sun 1.000 (0.92 -0.38 -0.03)  104 models (view 10)
view 13, 14   ambient 0.710  sun 1.000 (-0.43 0.83 0.36)   162 and 171 models
```

**The control has one light and the trial has three.** That is where the flaw is, and it explains the picture:
the scenery is in the views at `0.710` and is well lit; a view of 104 models is at `ambient 0.251` with a sun
pointing sideways `(0.92 -0.38 -0.03)` — silhouettes.

The unevenness is the signal, and it does not depend on the arena: that two different replays have different
base ambients (0.800 against 0.710) is normal; that one scene carries three is not. The control has one
exception, the trial has three and two views more.

**The next measurement**: find out which view the fighters are in, and where the light node it cites comes
from. The hypothesis to check — not to believe — is that the display list keeps view nodes from before the
restart, and that the fighters fall into one of them.


### The fighters' view, and its sun the wrong way up

The log also prints the models' addresses, because a count does not say **what** is in a view. In the same
run, before the Start, the demonstration shows the fighters correctly lit:

```
frame 1400, sub-state 0B (the demonstration)
  view  6   ambient 0.098  sun 1.000 (-0.90  0.42  0.14)   89 models  7927A3 79149D 7911A5 791089
  view 10   ambient 0.800  sun 1.000 (-0.08  0.94  0.34)  226 models  A02F03 A02EEC A02DEF A02DD8
```

```
frame 1800, sub-state 0A (the replay, after Start)
  view 10   ambient 0.251  sun 1.000 ( 0.92 -0.38 -0.03)  104 models  7927A3 79149D 7911A5 791089
  view 13   ambient 0.710  sun 1.000 (-0.43  0.83  0.36)  162 models  83CF98 83CD3C 931B5F 931A5B
```

**The `0x79xxxx` family is the same on both sides** — `7927A3`, `79149D`, `7911A5`, `791089`, at the same
addresses. That is the fighters: models rebuilt frame by frame, where the scenery lives higher up (`A02Fxx`,
`83Cxxx`, `931xxx`) and does not move.

And **it is not the ambient**. In the demonstration it is `0.098`, lower still than the dark replay's `0.251`,
and the fighters are very well lit there. What changes is **the sun's direction**:

| | sun | its vertical component |
|---|---|---|
| the demonstration, fighters lit | `(-0.90  0.42  0.14)` | **+0.42, from above** |
| the control, natural replay | `( 0.02  0.85 -0.52)` | **+0.85, from above** |
| the replay after Start | `( 0.92 -0.38 -0.03)` | **−0.38, FROM BELOW** |

**The sun lights the fighters from underneath.** Their tops receive nothing, and with an ambient of 0.251 only
silhouettes are left — exactly the picture. In the control, every view without exception has a positive
vertical component.

### What remains to be found

Where that view node comes from. It is in the display list the game builds, so the question is whether
restarting the loop makes it cite a node from another scene, or whether it builds a new one with wrong
values. The two are told apart by watching who writes those words — and it is the same branch of the
sequencer this file has already been looking for since 2026-10-21.


### Tracing back what writes that node: three links, and the fourth open

**1. The log names the nodes.** `VF3_VIEW_LOG` also prints each view's node address (`Viewport`'s `node`
field, set in `SceneBuilder::viewport`). Within one run:

```
frame 1400, the demonstration  node 80015C  ambient 0.098  sun (-0.90  0.42  0.14)   89 models  7927A3...
                               node 800244  ambient 0.800  sun (-0.08  0.94  0.34)  226 models  A02F03...
frame 1800, the dark replay    node 800244  ambient 0.251  sun ( 0.92 -0.38 -0.03)  104 models  7927A3...
```

**The fighters have changed node**: `80015C` in the demonstration, `800244` in the replay — and `800244` was
the scenery's node in the demonstration.

**2. The culling RAM is filled by DMA, not by the processor.** `vf3run --watch 8E000928` reports **nothing**:
`r3d_dma_write32` copies blocks from the main RAM with `write32`, which is not `Cpu::w32`'s path. It is a trap
that costs an hour to anyone who does not know it. `VF3_DMA_LOG=1` says where each block comes from:

```
r3d dma: 007D8010 -> 8E000000, 464 words
r3d dma: 007E2A10 -> 8E00AA00,  24 words
r3d dma: 007EB6B0 -> 8E0136A0,   8 words
```

**3. The nodes are copied from a single template.** `--trace-pc 6348` gives `r5 = 0x005811C4` in both runs,
and `r6` the destination. The loop `0x00006350`–`0x00006364` copies **48 words** from that template to each
view. The template itself is written at `0x0000624C`, and its vertical is **positive on both sides**: so it is
not the culprit.

### The open link, and the hypothesis to check

The DMA block covers words **0 to 463**. The node `0x800244` is word **580**, so **outside the block**. If no
other transfer covers it, it is **not rewritten for the frame** and what the scene reads there comes from a
previous scene — which would explain everything: a node left there from before the restart, and fighters hung
on it.

**To be checked before being believed**: count all of a frame's transfers and see whether one covers word 580.
The same check on the control will say whether that node is written there or whether it is a leftover there
too — in which case the explanation is elsewhere. Nothing should be corrected before that answer.


### The fourth link, closed: the display list is truncated

`VF3_DMA_LOG=1` dates each transfer by its frame. All those that go into the culling RAM (`0x8E...`), for one
replay frame on each side:

```
CONTROL, natural replay, frame 6400      TRIAL, replay after Start, frame 1800
  8E000000,  870 words                     8E000000,  464 words
  8E00AA00,   24 words                      8E00AA00,   24 words
  8E00B030, 3480 words                      8E0136A0,    8 words
  8E002D00, 3240 words
  8E0136A0,  300 words
  ---------------------                    ---------------------
  7 914 words in all                          496 words in all
```

**The fighters' node is word 580.** The control writes 870 of them at a stroke: it is inside, rewritten on
every frame. The trial writes 464: it is **outside**, and nothing else reaches it. So what the scene reads
there is what a previous scene left there — including a sun that lights from underneath.

And it is not a little that is missing: the trial writes **496 words** where the control writes **7 914**, and
the control's two large transfers (`8E00B030` and `8E002D00`) have no equivalent at all. Yet the trial's scene
cites **17 views** and draws **457 to 532 models**, as many as the control: so it needs those nodes and reads
beyond what the game wrote.

**It is the same cause as the models left on the title screen**, recorded on 2026-10-21: after the loop
restarts, the game no longer writes its list in full, and what is lying about is read as if it had written it.
One flaw, two symptoms.

**What must not be done**: patch the sun. The value read is not wrong, it is stale. The way out is the one
this file has been looking for since 2026-10-21 — what the game runs when its sequencer installs the title
screen, and that puts its list sender back in order.


### Who sends the list: the same one, called three times less often

The DMA's length register is at `0xC2000008`. `--watch` on it names each send, and counted over 1805 frames:

| sender | control | trial |
|---|---|---|
| `0x00009C64`, called from `0x00003CEC` | **9 642** | **3 428** |
| `0x00009B04`, from `0x00033404` | 2 487 | 2 296 |
| `0x00009B04`, from `0x00033468` | 2 461 | 1 918 |
| `0x00009BCC`, from `0x00005214` | 1 128 | 1 118 |
| `0x00009BCC`, from `0x00021F5C` | 419 | 67 |

**These are not two different senders** — that was my first reading, and it was wrong: both sides use the same
four. What changes is the **number of calls** to the main one, `0x00009C64` from `0x00003CEC`: a little over a
third. The three others barely move.

So the game does not change the way it writes its list: **it has less to write**. What feeds `0x00003CEC`'s
loop is shorter after the restart.

**The next measurement**: what `0x00003CEC` walks through — its list and its count — and why it is shorter.
`--trace-pc` on its entry will give the pointer and the length, and comparing control and trial at the same
sub-state will say what is missing.


### The proof by the same models, and the cause isolated

The doubt that remained: the two replays do not play the same arena, and a low sun could be the stage's. Ruled
out by comparing **the same models**, recognised by their addresses:

| | node | ambient | sun | models |
|---|---|---|---|---|
| control, frame 6400 | `8000E8` | 0.800 | `(0.02 **0.85** -0.52)` | 139: `6DA8A9 6DA4BD 6DA0D1 6D9E28` |
| trial, frame 2400 | `800122` | 0.251 | `(0.92 **-0.39** 0.08)` | 139: `6DA8A9 6DA4BD 6DA0D1 6D9E28` |

**The same 139 models, at the same addresses**, lit from above on one side and from below on the other. It is
not the arena. And a second family, `85243F 851AC7 A02796 A0273A`, is found on both sides too, at different
nodes (`80027E` against `8002F2`).

**The models have slipped node.** The control files them on `800074, 8000E8, 800196, 80020A, 80027E, 8002B8`;
the trial on `800122, 80020A, 80027E, 8002F2, 80032C`. Each group ends up hung on a neighbouring node — the one
belonging to another part of the scene, with that part's light.

### It is not the title screen shortcut

`VF3_NO_SKIP=1` restarts the loop and **lets the game walk to its own title screen**, logo included, without
asking it for anything. The replay that follows is **just as dark**: the same `0.251`, the same sun at `-0.39`.

So it is neither the menu nor the title screen request: **it is the restart of the attract loop itself**,
`MODE_REQUEST = 3, SUBMODE_REQUEST = 0`.

### The hypothesis that remains, and why it holds

The game restarts its loop by itself all the time — this file notes it above: *"the replay asks for the loop
to restart"*. And it has no dark fighters. What differs with us is **the moment**: the game does it at the end
of a replay, we do it **in the middle of a demonstration**.

To be checked: press Start **during a replay** rather than during a demonstration, and look at the next scene.
If it is sound, the way out is not to cut a demonstration off in the middle — either by waiting for it to
finish, or by ending it the way the game ends it.


### Correction: it is the attachment, not the sun's direction

One more trial — Start during a replay — showed **sound** scenes carrying suns with a negative vertical too:
`(-0.32 -0.74 0.59)`, `(-0.27 -0.30 0.91)`. The sun is **particular to each view**, and each view has its
camera: comparing two vectors taken at two moments proves nothing. **My phrase "the sun lights from
underneath" was an over-reading**, and it is withdrawn.

What remains, and depends on no camera, is **the ambient**, a scalar:

| the same 139 models, `6DA8A9 6DA4BD 6DA0D1 6D9E28` | node | ambient |
|---|---|---|
| control, natural replay | `8000E8` | **0.800** |
| trial, after the restart | `800122` | **0.251** |

And the second family, `85243F 851AC7 A02796 A0273A`: node `80027E` at **0.227** in the control, node `8002F2`
at **0.478** in the trial.

**So the flaw is the attachment**: one group of models ends up on a neighbouring node and inherits a light
meant for something else — ambient included. The fighters, which should be on a view at full ambient, land on
a view at a quarter, and that is what is seen.

### Where it stands

- **Established**: the same models, different nodes, different ambients; and it is neither the menu nor the
  title screen request, since `VF3_NO_SKIP=1` gives the same flaw.
- **Established too**: after the restart the game sends only 496 words of list where it sends 7 914, because
  it builds only **three DMA descriptors instead of five**.
- **Not established**: why. The trail that remains is the **moment** of the restart — the game restarts its
  loop at the end of a replay, we cut it off in the middle of a demonstration. The trial "Start during a
  replay" did not settle it: it lands on a demonstration, not on a replay, so it does not compare the right
  thing.


### The complete chain, from the symptom to the counter

```
dark fighters
  └─ they are hung on a neighbouring view node, of ambient 0.251 instead of 0.800
      └─ because the display list sent does not cover their node
          └─ because the game builds only 3 DMA descriptors instead of 5
              └─ because two of them take their length from counters
                 (0x00580FAC, 0x00580FB0) multiplied by 12, at 0x00005184 and 0x0000519C
                  └─ and 0x00580FB0 is 33 instead of 323
                      └─ because 0x00006C34 and 0x0000688C, which increment it,
                         run 1 206 times instead of 69 151
```

Every step is measured with its control, and every address is taken, not supposed.

### Where it stops, and why

The last step says **that** the game submits fifty times less to be drawn, not **why**. The two incrementers'
callers are `0x0001277C`, `0x00012CB4`, `0x00018DA0`, `0x00022C10` and `0x00023C94`; the control uses all
five, the trial practically none. Knowing what commands them takes decompiling the scene submission — a job of
its own, not one more measurement.

What is settled and will not have to be done again:

- the flaw is **the restart of the attract loop**, neither the menu nor the title screen request
  (`VF3_NO_SKIP=1` reproduces it without asking the game for anything);
- it strikes **the submission**, not the rendering: the models are all there, 457 to 532, correctly placed;
- it explains **October's two symptoms and this one**: what is not rewritten is read as it was, hence the
  models left on the title screen and the fighters inheriting a foreign ambient;
- and the instruments now exist: `VF3_VIEW_LOG`, `VF3_DMA_LOG`, `VF3_NO_SKIP`.

**What must not be done**: forcing the ambient or the sun. That would be painting over a scene that was not
submitted, and the rest — what is lying about on screen — would remain whole.
