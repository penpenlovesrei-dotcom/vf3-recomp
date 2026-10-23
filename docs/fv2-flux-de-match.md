# Fighting Vipers 2: the match flow

What leads from one screen to the next -- the attract, the title, the start of a game, the
fighter select, the rounds, the end of a match, the way back to the attract. Written so that
another piece of work can hang a console menu off it without feeling around.

This is **not** a matching decompilation. Sega's compiler of 1998 is unknown and not to be had,
so C that rebuilds to the same bytes is out of reach. What is here is the region read and
documented, with the measurement that settles each non-obvious claim beside it.

Everything below was read out of the program image and measured on this port. Addresses that
were not seen are not here. Where something was read in the code but never watched happening,
it says so.


## How it was read

The program is a gzip at offset 0 of the fixed CROM which the game's own boot stub inflates
into RAM at 0 (`docs/step2-fvipers2.md`, section 1). `tools/fv2/README.md` is the whole chain;
the short of it:

- the image comes from `vf3run --ram-at`, re-swapped by groups of four, checked on one known
  instruction (`0x0005F3BC` must read `2C 80 00 C0`, `cmpwi r0,192`);
- it is loaded into Ghidra as raw PowerPC 32-bit big-endian at base 0;
- code is disassembled from the recompiler's 7196-address map, which is what this port's own
  interpreter measured the program entering;
- **functions are cut at the program's 1382 `bl`/`bcl` targets**, not at the map -- 6331 of the
  map's addresses are branch targets inside a function;
- Sega's own variable names come from the game's debug tables: 80 pages, 1035 named entries,
  745 distinct addresses.

**The Ghidra project is at `C:\Temp\fv2-ghidra` (`FV2.gpr`)**, outside the repository, because it
holds the game's program and no Sega binary belongs in git. In it: 1382 functions at the
program's own call targets, 745 of Sega's variable names, the 81 state handlers named from the
game's tables with a plate comment each, their bodies bounded, and the four dispatchers' jump
tables cut back to their real length. `tools/fv2/README.md` rebuilds it from scratch in two
commands.

The instruments are `vf3run`'s: `--trace-pc ADDR` (r3-r8 and LR at each pass), `--watch ADDR`
(every write to a word, with the PC and LR that did it), `--ram-at`, `--shots`, `VF3_POKE` with
`VF3_POKE_FRAMES`, and `VF3_RAMLOG` (added for this work: a frame-stamped line whenever any of a
list of RAM bytes changes, which is what a state machine's progress needs and the other two do
not give).

`tools/scripts/fv2-flow.txt` is the run most of the measurements below come from: a coin, Start,
two places along the portrait strip, the pick confirmed, then random play until the game is back
in the attract.


## 1. The machine

The whole flow is one state machine with a main mode and a sub mode, both bytes, both named by
Sega in the debug page `MODE_INFO`:

| address | Sega's name | what it is |
| --- | --- | --- |
| `0x0058FD98` | `B_MAIN_MODE` | the main mode now running |
| `0x0058FD99` | `B_SUB_MODE` | the sub mode now running |
| `0x0058FD9A` | `MODE_REQUEST` | the main mode asked for, taken up next tick |
| `0x0058FD9B` | `S-MODE_REQUEST` | the sub mode asked for, taken up next tick |
| `0x0058FD4C` | `M_MAIN_MODE` | `0x80000000 >> B_MAIN_MODE`, a mask of the same thing |
| `0x0058FD50` | `M_SUB_MODE` | `0x80000000 >> B_SUB_MODE` |
| `0x0058FD54` | -- (not named) | a tick-down counter a state sets and the next state waits on |

**Measured.** `vf3run --watch 58FD98` masks to the word, so it reports every write to all four
at once. Over `fv2-fight.txt` it names exactly one writer of `B_MAIN_MODE` -- `0x0000AC10`,
3513 times, always with `LR 0x00039068` -- and three of `B_SUB_MODE`: `0x0000B0F0`, `0x0000BA94`
and `0x0000BC88`, which are the three sub-dispatchers below. `MODE_REQUEST` and
`S-MODE_REQUEST` are written from about forty sites across `0x0000AFF4`-`0x0000C5C0`: the state
handlers asking for the next state.

### The two parallel tables

There is one jump table and one name table, **80 entries each**, and they run in parallel:

```
0x00024354   80 words: the handler of each state
0x00024494   80 words: a pointer to that state's name, as a string
```

Each dispatcher indexes both at the same base, which is what makes the pairing certain rather
than likely -- the four base offsets below are read straight out of the four dispatchers:

| dispatcher | at | jump table | name table | base | states |
| --- | --- | --- | --- | --- | --- |
| main mode | `0x0000ABF0` | `0x00024354` | `0x00024494` | 0 | 24 |
| `GAME_DSP`'s sub | `0x0000BCB4` | `0x000243B4` | `0x000244F4` | 24 | 20 |
| `SEL_DSP`'s sub | `0x0000BAC0` | `0x00024404` | `0x00024544` | 44 | 6 |
| `ADV_DSP`'s sub | `0x0000B124` | `0x0002441C` | `0x0002455C` | 50 | 12 |

and the rest of the 80 are the ENDING sub modes (base 62, 7), the NAME ENTRY sub modes
(base 69, 3) and the SKO sub modes (base 72, 8).

`0x000243B4 = 0x00024354 + 24*4` and `0x000244F4 = 0x00024494 + 24*4`, and so on for 44 and 50:
the same base in both tables, in all four dispatchers.

**This matters for reading the game's own debug menu.** `MODE_INFO`'s name arrays at `0x0007A5E8`
are a second, differently padded copy -- 24 names for the main mode but only 9 for the attract's
sub modes, the rest `********`. The table at `0x00024494` has **12** attract sub modes, and the
three the debug copy is missing are reached in an ordinary run: measured, `B_SUB_MODE` takes the
value 11 at frame 1386 of `fv2-flow.txt`, which is `ADV_DEMO_DSP`, the attract's demo itself.

### `ADV_INFO STATUS`, the other word the flow turns on

`0x005813F0`, which Sega's `ADV_INFO` page calls `STATUS`, is tested by nearly every handler
below. These are the bits this work saw used, and only those:

`vf3run --watch 5813F0` over `fv2-flow.txt` names every writer in the run, which is what the
"set at" column below rests on:

| bit | where it is read | set at | what the measurement says |
| --- | --- | --- | --- |
| `0x00000008` | `NEXT_PROG_SEL` | -- | the ladder is finished: go to the ending. Read in the code; **never seen set** |
| `0x04000000` | the attract states set it, `DECIDE_INT` clears it | `0x0000B15C`, then `0x0005EFA8` (318 times) | going to the select screen. **Measured**: it appears on the Start frame and the select screen keeps re-asserting it |
| `0x20000000` | inside the `0x30000000` mask the attract tests | `0x0000B15C` | **Measured**: `0x0000B15C` writes `0x24000020` once, with `LR 0x0000B7D8` -- that is `fv2_adv_start_pressed` reached from `ADV_DEMO_DSP`, on frame 1961 |
| `0x30000000` | the attract states, `intrude_check`, `FIGHT_DSP` | | "Start has been pressed" |
| `0x40000000` | `intrude_check`, `VIC_INT`, `NEXT_PROG_SEL`; `ROUND_MASK_INT` clears it | `0x0000CD78` (`LR 0x0000CCB0`) | **Measured**: written in the round-decision code, between the `JUDGE` handler and `JUDGE_DSP`, and it is the bit that sent this run to `CONTINUE` instead of another round. In this run player 2 pressed nothing, so it is **not** player 2's Start; what it means exactly was not run down |
| `0x80000000` | set by `FIGHT_DSP` | `0x0000C678` (`LR 0x0000C600`), and `0x0000CD78` | the round timed out. **Measured twice over**: `0x0000C678` is inside `FIGHT_DSP`, which is the line read above, and the word reads `0x80000000` from frame 4640, the frame `0x00581404` reaches 0 |

One more writer accounts for most of the traffic and was not run down: `0x000122C4`
(`LR 0x000120F4`) writes the whole word **4293 times** in the run -- 3510 of them 0 -- so there is
a per-frame routine maintaining it that this page has not read.

Measured values of the whole word across the run: `0x00000000` (most of it), `0x00000020`,
`0x04000000`, `0x20000020`, `0x24000000`, `0x24000020`, `0x40000000`, `0x80000000`.

### The tick

`fv2_mode_tick`, **`0x0000ABC4`**, 44 bytes. Called once a frame from `0x00039064`.

```
if (B_MAIN_MODE != 0x15 && (*(uint*)0x00591D14 & 0x04000000))   /* 0x15 is TEST_DSP */
        return;                       /* the machine is suspended, but the test menu still runs */
/* falls into fv2_mode_dispatch */
```

Measured: `0x00591D14` was **0 for every frame** of `fv2-flow.txt`
(`VF3_RAMLOG=591D14:4`), so this early return never fired in that run and what sets the bit was
not established.

`fv2_mode_dispatch`, **`0x0000ABF0`**, 116 bytes. Every state handler ends by branching back
here or to the shared epilogue, so it is both the head of the tick and the re-entry point:

```
--*(int*)0x0058FD54;                               /* the screen counter ticks down here, once */
B_MAIN_MODE  = MODE_REQUEST;                       /* 0x0000AC10 -- the only writer */
M_MAIN_MODE  = 0x80000000 >> B_MAIN_MODE;
*(char**)0x002742F0 = name_table[B_MAIN_MODE];     /* for the debug display only */
*(void**)0x002742F4 = &DAT_00274374;
goto jump_table_0x00024354[B_MAIN_MODE];           /* bctr, no bounds check */
```

Two things worth having in hand for a console menu: **the request is taken up on the next tick,
not at once** -- a handler that sets `MODE_REQUEST` keeps running to its end -- and
**the `bctr` has no bounds check**, so a main mode outside 0..23 jumps into whatever the words
past the table hold, which are the name pointers.

`fv2_mode_tick_return`, **`0x0000AC64`**, 16 bytes: restore LR, pop the frame, return. Every
handler that is finished for this frame branches here.


## 2. The flow, as it was measured

One run of `tools/scripts/fv2-flow.txt`, `VF3_RAMLOG` on the four mode bytes and the counters,
frame numbers as they came out. This is the whole arc, boot to boot:

| frame | main mode | sub mode | what it is |
| --- | --- | --- | --- |
| 223 | `WARNING_INT` | | the health-and-safety screen is set up |
| 225 | `WARNING_DSP` | | and shown |
| 997 | `ADV_INT` | `DONT_RECYCLE_INT` | the attract begins |
| 998-1001 | `ADV_DSP` | `DONT_RECYCLE_DSP`, `ADV_SEGA_INT`, `ADV_SEGA_DSP` | the notices, then the SEGA logo |
| 1193 | `ADV_DSP` | `FV2_TITLE_INT` | the title screen |
| 1386 | `ADV_DSP` | `ADV_DEMO_DSP` | the character roll-call demo |
| 1961 | `SEL_INT` | | **Start, pressed at 1960, is taken** |
| 1963 | `SEL_DSP` | `SEL_START` | the select screen comes up |
| 2031 | `SEL_DSP` | `SELECT` | the cursor is live |
| 2285-2286 | `SEL_DSP` | `DECIDE_INT`, `DECIDE_DSP` | a fighter is taken (punch at 2220) |
| 2296 | `GAME_INT` | `ROUND_MASK_INT` | the match is set up; `0x0058145A` becomes 1 |
| 2298-2418 | `GAME_DSP` | `ROUND_MASK_IN`, `_DSP`, `_OUT` | the STAGE / VS screen |
| 2419-2420 | `GAME_DSP` | `ROUND_INT`, `ROUND_DSP` | the opponent's match is set up |
| 2421-2423 | `GAME_DSP` | `SET_INT`, `SET_INT_POSE`, `SET_DSP` | the stage and the two fighters are placed |
| 2624-2625 | `GAME_DSP` | `READY_INT`, `READY_DSP` | READY |
| 2721-2722 | `GAME_DSP` | `FIGHT_INT`, `FIGHT_DSP` | **the round is being fought** |
| 4640 | `GAME_DSP` | `JUDGE` | the round is over |
| 4641-4737 | `GAME_DSP` | `JUDGE_DSP`, `REPLAY_INT` | the decision and the replay |
| 4738-4740 | `GAME_DSP` | `VIC_INT`, `NEXT_PROG_SEL` | the winner's pose, then what comes next |
| 4741 | `GAME_DSP` | `SET_INT` | **the next round of the same match**, no VS screen |
| 7235-7236 | `GAME_DSP` | `NEXT_PROG_SEL`, `ROUND_MASK_INT` | the match is decided; `ROUND_NO` goes 1 -> 2 |
| ... | | | the second opponent, three more rounds |
| 14921 | `GAME_DSP` | `NEXT_PROG_SEL` | the player has lost the match |
| 14922-14923 | `CONTINUE_INT`, `CONTINUE_DSP` | | the continue countdown |
| 15295-15296 | `OVER_INT`, `OVER_DSP` | | GAME OVER |
| 15487 | `OVER_DSP` -> `ADV_INT` | | **back to the attract**; `0x0058145A` goes to 0, `ROUND_NO` to 1 |

Two shapes to notice, both of them measured rather than assumed:

- **a round loop and a match loop.** Within one opponent the cycle is
  `SET_INT` -> `READY` -> `FIGHT` -> `JUDGE` -> `REPLAY` -> `VIC` -> `NEXT_PROG_SEL` -> `SET_INT`,
  with no `ROUND_MASK`. `ROUND_MASK_*` -- the STAGE *n* / VS *name* screen -- runs only when the
  opponent changes, and `ROUND_NO` (`0x0058145C`) is incremented at the same moment.
- **`_INT` falls through into `_DSP`.** `GAME_INT` ends at `0x0000BC6C` and `GAME_DSP` begins at
  `0x0000BC70` with no branch between them, so on the `GAME_INT` frame `GAME_DSP` and its first
  sub handler run as well. The measurement agrees: at frame 2296 `B_MAIN_MODE` is 6 (`GAME_INT`)
  and `B_SUB_MODE` is already 0 (`ROUND_MASK_INT`) with `S-MODE_REQUEST` already 1.


## 3. The states, one at a time

Names are Sega's, out of the table at `0x00024494`. "Reads" and "writes" list the RAM this work
actually saw the handler touch, not everything it reaches through a call.

### The warning screen

**`fv2_mode_WARNING_INT`** -- `0x0000AC74`, 160 bytes, main mode 0.
Calls `FUN_0003B658`, puts `0x80400000` at `[0x0007512C]+0x50`, then clears `0x0058145A`
(who is in the machine) and `0x0058149C`. Sets the screen counter `0x0058FD54` to **0x140**
when `0x0011F06E != -1` and `!(*(uint*)0x00591D14 & 0x00400000)` and the region byte
`0x00120557` is 0; otherwise to **3**. Then `MODE_REQUEST++`.
*Measured*: in this port's run the counter was set to `0x140` and the screen ran from frame 224
to 997 -- 773 frames for 321 ticks, so the machine is not ticking once a video frame over that
stretch. The suspend bit at `0x00591D14` was 0 throughout, so that is not the reason; what is
was not run down.

**`fv2_mode_WARNING_DSP`** -- `0x0000AD14`, 44 bytes, main mode 1.
`if (*(int*)0x0058FD54 < 1) { FUN_00067248(); MODE_REQUEST++; }` and nothing else.

### The attract

**`fv2_mode_ADV_INT`** -- `0x0000AD40`, 716 bytes, main mode 2. The attract's setup.

**`fv2_mode_ADV_DSP`** -- `0x0000B00C`, 304 bytes, main mode 3. Two things before it dispatches:

1. a **secret code**. It reads `0x00580EA0` and `0x00580EA4`, and on two separate sequences --
   one walked through the table at `0x00110B24` with its position in `0x00110B21`, the other
   through `0x00110B6C` with its position in `0x00110B69` -- a run of 0x40 matching entries ends
   with, for the first, `0x00110B20 = 1`, and for the second, **`CONFIG *PLAY KUHN`
   (`0x0011F060`) and `*PLAY DELSOL` (`0x0011F061`) both set to 1**. Those are the two hidden
   fighters. Read in the code; **not watched happening** -- no run here entered either sequence.
2. then `B_SUB_MODE = S-MODE_REQUEST`, also copied to `0x0058149C`, `M_SUB_MODE` set, and the
   jump through `0x0002441C`.

**`fv2_adv_start_pressed`** -- `0x0000B13C`, 100 bytes. Not a state: the tail of `ADV_DSP`, and
the same five lines are compiled into `ADV_DEMO_DSP` and `ADV_REPLAY_DSP` as well.

```
if (ADV_INFO_STATUS & 0x30000000) {       /* 0x005813F0 -- Start has been pressed */
        ADV_INFO_STATUS |= 0x04000000;    /* going to the select screen */
        *(int*)[0x0007514C] = 1;
        FUN_0001FEA0();
        fighter0[0xB8] = 0;               /* 0x007005B8, *1P CHARA */
        fighter1[0xB8] = 0;               /* 0x007075B8, *2P CHARA */
        MODE_REQUEST = 4;                 /* SEL_INT */
        goto fv2_mode_dispatch;
}
```

*Measured*, and this is the clearest confirmation in the page: Start is pressed at frame 1960;
at 1960 `0x005813F0` reads `0x00000020`, at **1961** it reads `0x24000000` -- `0x20000000`, which
is inside the `0x30000000` mask, plus the `0x04000000` this handler sets -- and `B_MAIN_MODE`
becomes 4 on the same frame. At 1965 it is back to `0x04000000` alone.
Note it **zeroes both fighters' character id**, so the select screen always opens on fighter 0.

**`fv2_adv_ADV_DEMO_DSP`** -- `0x0000B7C8`, 48 bytes, attract sub mode 11. Sets `0x0058FDBB` to
1, calls `FUN_00072E50`, and when `ADV_INFO SUBMODE` (`0x00580E08`) reaches 4 asks for attract
sub mode 2. Then the Start test above.

**`fv2_adv_ADV_REPLAY_DSP`** -- `0x0000B764`, 100 bytes, attract sub mode 10. Puts `0x2000` at
`fighter+0x66` for both and zeroes both fighters' `+0xF38` and `+0xF3C` -- which `ROB0_ARMOR`
names `damage_u` and `damage_d`. Starts a sound through `FUN_000074E0(0xA0037F)` when the screen
counter is exactly `0x7F`. Then the Start test.

### The select screen

**`fv2_mode_SEL_INT`** -- `0x0000B7F8`, 636 bytes, main mode 4. The select screen's setup.
*Measured*: it sets the screen counter to **0x40** (frame 1962).

**`fv2_mode_SEL_DSP`** -- `0x0000BA74`, 104 bytes, main mode 5.

```
if (*(int*)0x0058FD54 <= 0x3E)  FUN_0005EF68();    /* the screen's per-frame work */
B_SUB_MODE = S-MODE_REQUEST;  M_SUB_MODE = 0x80000000 >> B_SUB_MODE;
goto jump_table_0x00024404[B_SUB_MODE];
```

*Measured*: the counter is `0x40` at 1962 and `0x3F` at 1963, so `FUN_0005EF68` first runs on the
third tick of the screen.

**`fv2_sel_SEL_START`** -- `0x0000BADC`, 52 bytes, sel sub mode 0.
`if (counter != 0x32 && counter < 1) S-MODE_REQUEST++`.

**`fv2_sel_SELECT`** -- `0x0000BB10`, 164 bytes, sel sub mode 1. The cursor is live here. It
works on two words reached through `[0x00075108]` and `[0x0007510C]` -- which the RAM dump says
are **`0x00700000` and `0x00700100`**, the two entry task blocks, Sega's `*ENTRY 1P` and
`*ENTRY 2P`. For each player: if bit 1 is clear and bit 3 set, set bit 1 and clear bit 2, and if
the *other* player has not got bit 2, reload the select countdown `0x00581408` with **0x3F**.
Then the countdown ticks down, and at 0 it asks for sel sub mode 4 (`DECIDE_INT`).

**`fv2_sel_DECIDE_INT`** -- `0x0000BBB8`, 100 bytes, sel sub mode 4.
Clears bit 0 of both entry task blocks (which stops those tasks -- see section 4), starts a
sound with `FUN_00007510(0xA00001)`, **clears `ADV_INFO_STATUS` bit `0x04000000`** -- the one
`fv2_adv_start_pressed` set -- asks for sel sub mode 5, and calls `FUN_0005FD94`.

**`fv2_sel_DECIDE_DSP`** -- `0x0000BC1C`, 32 bytes, sel sub mode 5.
`if (*(char*)0x00581892 == 0) MODE_REQUEST = 6;` -- the one line that leaves the select screen
for the match.

### Starting the match

**`fv2_mode_GAME_INT`** -- `0x0000BC3C`, 52 bytes, main mode 6.

```
*(char*)0x00581697 = 0;
FUN_000603D4();              /* builds 0x0058145A: who is in the machine */
FUN_0006FC94();
S-MODE_REQUEST = 0;
MODE_REQUEST++;              /* GAME_DSP */
/* and then falls straight through into GAME_DSP */
```

*Measured*: `0x0058145A` is 0 up to frame 2295 and **1 at frame 2296**, the `GAME_INT` frame, and
goes back to 0 at 15487 when the game returns to the attract. `FUN_000603D4` is the routine
`docs/fv2-honey.md` already identified as the builder of that byte; this says when it is called.

**`fv2_mode_GAME_DSP`** -- `0x0000BC70`, 92 bytes, main mode 7. Clears `0x0058FDAB`, then
`B_SUB_MODE = S-MODE_REQUEST`, `M_SUB_MODE`, and the jump through `0x000243B4`.

**`fv2_game_intrude_check`** -- `0x0000BCCC`, 96 bytes. Not a state: the cut-in test, called from
several of the game states.

```
if (ROUND_NO == 8)                        return;        /* 0x0058145C */
if (!(ADV_INFO_STATUS & 0x30000000))      return;        /* no Start pressed */
if ((ADV_INFO_STATUS & 0x40000000) && 0x0058145A != 3) { /* see the bit table below */
        FUN_0000DD70();
        ROUND_NO++;
}
MODE_REQUEST = 10;                                       /* INTRUDE_INT */
goto fv2_mode_dispatch;
```

This is 乱入 -- a second player cutting into a match in progress. It is the piece
`docs/fv2-honey.md` wanted and could not drive: that page's consecutive-win counter only
advances while `0x0058145A == 3`, and this is where the 3 comes from. **Not watched happening**:
no run here got a second player in.

### The rounds

**`fv2_game_ROUND_MASK_INT`** -- `0x0000BD2C`, 384 bytes, game sub mode 0. The STAGE *n* / VS
screen, and where a round's health comes from:

```
if (SURVIVAL_MODE == 0) {                 /* 0x0058145F */
        fighter0[0] &= ~1;  fighter1[0] &= ~1;  [0x0007515C][0] &= ~1;   /* stop three tasks */
}
ADV_INFO_STATUS &= ~0x40000000;
*(int*)0x0058FD54 = 1;
energy = (0x0058145A == 3) ? CONFIG_VS_ENERGY : CONFIG_1P_ENERGY;   /* 0x00120542 : 0x00120540 */
if (SURVIVAL_MODE == 0) { fighter0[0x68] = energy;  fighter1[0x68] = energy; }
else { /* the player who is in keeps energy<<2, the other energy - energy/4 */ }
```

So **`fighter+0x68` is the health** a round starts with, and the two settings it comes from are
Sega's own `CONFIG *1P ENERGY` and `*VS ENERGY`. Read in the code; the Survival branch was not
exercised here.

**`fv2_game_ROUND_INT`** -- `0x0000BF68`, 200 bytes, game sub mode 4. The per-opponent reset:

```
if (SURVIVAL_MODE == 0)  FUN_00007510(0xA00001);
FUN_000182CC();
*(char*)0x00581456 = 1;     /* the round number within this match */
*(char*)0x00581454 = 0;     /* the two round-win counters */
*(char*)0x00581455 = 0;
*(char*)0x00581462 = 0;  *(char*)0x00581463 = 0;
*(int*)0x00581414  = 0;     /* frames played */
FUN_0005CFDC();
POINT vipers_point_s (0x00581444) = 0;
fighter0[0x607E] = 0;  fighter1[0x607E] = 0;
POINT perfect (0x00581450) = 0;   POINT straight (0x00581451) = 1;
FUN_00067BE4(); FUN_00067BE4(); FUN_0006FDC4(); FUN_00021928();
S-MODE_REQUEST++;
intrude check
```

*Measured*: over `fv2-flow.txt`, `0x00581455` reads 0 through the first round and **1** from the
first `JUDGE` onwards, then 2 -- the player was losing -- which is consistent with
`0x00581454`/`0x00581455` being the two sides' rounds won. `0x00581454` itself was not logged in
that run, so which byte is which side is **not settled here**. `0x00581456` ends the match at
**3** with `0x00581455` at 2: three rounds played, two of them lost, which is what a best-of-three
looks like and is the clearest thing said about either byte here.

**`fv2_game_SET_INT`** -- `0x0000C048`, 800 bytes, game sub mode 6. Places the stage and the two
fighters. It arms three tasks by the same two writes the scheduler reads (section 4): the
collision task at `[0x00075124]` at `FUN_00044BD4`, the one at `[0x00075158]` at `0x0001E628`,
and the one at `[0x00075130]` at `FUN_00022EC0`; and it stops `[0x00075118]`.
Of note for a stage: when `STAGE_TEST *STAGE` (`0x0058FD9F`) is **not** 9 it works out an angle
as `*(ushort*)(ROB_LIGHT+100) + (round_number - 1) * 0x4000` -- **the round number turns the
stage** -- and stage 9 is special-cased out of it.

**`fv2_game_READY_INT`** -- `0x0000C48C`, 92 bytes, game sub mode 9.

**`fv2_game_FIGHT_INT`** -- `0x0000C510`, 180 bytes, game sub mode 11. Arms the two CPU tasks --
`[0x00075110]` (Sega's `CPU0`) at `FUN_00062144` and `[0x00075114]` at `FUN_000621B0` -- plays
`0xA02B0F`, calls `FUN_00017D1C`, asks for the next sub mode, and **falls through into
`FIGHT_DSP`**.

**`fv2_game_FIGHT_DSP`** -- `0x0000C5C4`, 208 bytes, game sub mode 12. The round, every frame:

```
if (0x0058145A != 3 && CPU_level < 2)  ++*(int*)0x00581414;     /* frames played */
FUN_00017D8C();
if (!(fighter0[0] & 0x04000000) && !(fighter1[0] & 0x04000000)) {
        if ((CONFIG_CB_DISP & 1) || CPU_level > 1 || --GAME_TIMER > 0) {   /* 0x00581404 */
                intrude check;
                return;                       /* the round goes on */
        }
        ADV_INFO_STATUS |= 0x80000000;        /* time over */
}
S-MODE_REQUEST++;                             /* JUDGE */
goto fv2_mode_dispatch;
```

So **a round ends when either fighter's status word carries `0x04000000`, or the game timer
reaches 0**, and a time-out is marked by bit 31 of `ADV_INFO_STATUS`.

*Measured, and this one is exact.* Over `fv2-flow.txt` the first round ended on time:
`0x00581404` reads 5, 4, 3, 2, 1 on frames 4635 to 4639, and on frame **4640** it is 0, the sub
mode becomes `JUDGE`, and `0x005813F0` becomes **`0x80000000`** -- all three on the same frame.

**`fv2_game_JUDGE`** -- `0x0000C694`, 192 bytes, game sub mode 13. The decision.
**`fv2_game_JUDGE_DSP`** -- `0x0000CF8C`, game sub mode 14.
**`fv2_game_REPLAY_INT`** -- `0x0000D6E0`, 516 bytes, game sub mode 15.
**`fv2_game_VIC_INT`** -- `0x0000D98C`, 376 bytes, game sub mode 17. The winner's pose. Zeroes
three fields on both fighters, calls `FUN_000556A8` and `FUN_00045558`, sets `0x0058FDAF` to 10,
and when `ADV_INFO_STATUS` has no `0x40000000` simply asks for the next sub mode.

**`fv2_game_NEXT_PROG_SEL`** -- `0x0000DB48`, 136 bytes, game sub mode 19. **The branch point of
the whole match flow.**

```
if (ADV_INFO_STATUS & 8) {                 /* the ladder is finished */
        FUN_0000DD70();
        MODE_REQUEST = 12;                 /* ENDING_INT, entered inline here */
        ... stops seven tasks, clears the fog, sets the screen counter to 0x140 ...
}
if (!(ADV_INFO_STATUS & 0x40000000)) {
        ++*(char*)0x00581456;              /* the next round of the same match */
        S-MODE_REQUEST = 6;                /* SET_INT -- no VS screen */
        intrude check;
        return;
}
/* otherwise the two-player branch, on 0x0058145A == 3 ... */
```

This is the handler that decides another round, another opponent, the ending, or a continue, and
it is the one a console menu would most want to reach.

### Losing, and the way back

**`fv2_mode_CONTINUE_INT`** -- `0x0000DDA4`, 84 bytes, main mode 8.
**`fv2_mode_CONTINUE_DSP`** -- `0x0000DDF8`, 196 bytes, main mode 9. The countdown.
*Measured*: frames 14922 to 15294, 372 frames.
**`fv2_mode_OVER_INT`** -- `0x0000E2F0`, 192 bytes, main mode 18.
**`fv2_mode_OVER_DSP`** -- `0x0000E3B0`, 104 bytes, main mode 19.
*Measured*: frames 15295 to 15486, then at **15487** the request goes to `ADV_INT`, `0x0058145A`
drops to 0 and `ROUND_NO` is put back to 1. The game is in the attract again at 15488.

**`fv2_mode_INTRUDE_INT`** -- `0x0000DFF0`, 304 bytes, main mode 10. Plays `0xA00001` and
`0xA02E05`, then calls the per-fighter model setup `FUN_0003F874` twice, once with each fighter
block (`[0x0007511C]` and `[0x00075120]`). **Not watched happening.**


## 4. How a screen's work is actually run

This is the part a console menu needs most, and it is not the mode machine: it is a task
scheduler, and the mode handlers drive it.

**The scheduler** is at `0x00039174`, inside the same frame routine that calls `fv2_mode_tick`:

```
n    = *(int*)0x00075164;              /* measured: 23 slots */
slot = 0x00075108;                     /* an array of pointers to task blocks */
for (i = 0; i < n; i++, slot += 4) {
        FUN_0000A424();
        block = *slot;
        *(int*)0x0058FDBC = i;         /* which slot is running */
        block[0x28] = sp;              /* the scheduler's stack, kept in the block */
        *(void**)0x0058FDC0 = block;   /* the running task */
        block[0x2C] = *(int*)0x0058FDC4;
        if (block[0] & 1) {            /* bit 0 of word 0: this task runs */
                LR = block[1];         /* word 1: the resume address */
                blrl;                  /* enter it */
        }
        FUN_0000A448(block + 0x30);
}
```

So a task is **two words**: `[block+0]` whose bit 0 says whether it runs, and `[block+4]` the
address to enter. A task yields by planting its next resume address in `[block+4]` and
returning.

**Measured**, from the RAM dump at frame 1200:

| slot pointer | block | what it is |
| --- | --- | --- |
| `[0x00075108]` | `0x00700000` | the entry task, player 1 (Sega's `*ENTRY 1P`) |
| `[0x0007510C]` | `0x00700100` | the entry task, player 2 |
| `[0x00075110]` | `0x00700200` | `CPU0` |
| `[0x00075114]` | `0x00700300` | `CPU1` |
| `[0x0007511C]` | `0x00700500` | fighter 0 |
| `[0x00075120]` | `0x00707500` | fighter 1 |
| `[0x00075140]` | `0x0070EC00` | fighter 0's cloth task |
| `[0x00075144]` | `0x0070ED00` | fighter 1's cloth task |
| `[0x00075164]` | 23 | how many slots |

**The two primitives**, generic, used all over the match flow:

```
FUN_00067B80(block, entry)      0x00067B80   block[1] = entry;  block[0] |= 1;    /* start it */
FUN_00067B94(block)             0x00067B94   block[0] &= ~1;                      /* stop it */
```

Every `*(uint*)PTR_... &= 0xFFFFFFFE` in the handlers above is `FUN_00067B94` compiled inline:
`ROUND_MASK_INT` stopping three tasks, `DECIDE_INT` stopping the two entry tasks,
`NEXT_PROG_SEL` stopping seven of them before the ending.

**This is a command, not a leftover.** The scheduler reads exactly those two fields every frame
and jumps through the second one. Writing them is how the game itself starts a task, and it is
how anything else can.

**The select screen's own task** is the entry task for player 1, `0x00700000`.
`vf3run --watch 700004` over `fv2-fight.txt` reports **five writes in the whole run**, and two of
them are the brief's own anchors:

```
store32 00700004 = 5ED48 at PC 000390FC (LR 00038C38)   the boot installing the task table
store32 00700004 = 5ED48 at PC 0005EFE4 (LR 0005EF98)   <- the select screen armed
store32 00700004 = 5EDF8 at PC 0005EDD4 (LR 000391AC)   the screen planting its own next step
```

So `0x0005ED48` really is the select screen's task entry, and `0x0005EFE4` really is one of the
two writes Start leaves behind -- but what those writes *are* is now plain: they are
`FUN_00067B80`'s two fields, written inline.

### What re-arms the character preview, and under what condition

Asked by the session that commissioned this page, and in this perimeter because it is the
select screen's model setup. Answered by measurement, end to end.

The highlighted fighter's preview is built by **that fighter's cloth task**, and a cloth task is
an ordinary scheduler task like any other:

| | fighter 0 | fighter 1 |
| --- | --- | --- |
| block | `0x0070EC00` | `0x0070ED00` |
| entry | `0x0005756C` | `0x000575A0` |
| resume field | `0x0070EC04` | `0x0070ED04` |

`vf3run --watch 70EC04` over `fv2-fight.txt` names every writer of fighter 0's resume field:

```
7 x  store32 0070EC04 = 57C78 at PC 0005758C (LR 00057584)   the task yielding to its own next step
6 x  store32 0070EC04 = 5756C at PC 00067B84 (LR 0003FE74)   <- the re-arm
1 x  store32 0070EC04 = 5756C at PC 000390FC (LR 00038C38)   the boot installing the table
```

`0x00067B84` is the `block[1] = entry` line of **`FUN_00067B80`**, reached with `LR 0x0003FE74`,
which is the call at `0x0003FE70` at the end of **`FUN_0003F874`**, the per-fighter model setup:

```
if (fighter[8] == 0) { FUN_00022660(&DAT_003E51A8); entry = 0x0005756C; block = [0x00075140]; }
else                 { FUN_00022660(&DAT_003C43A8); entry = 0x000575A0; block = [0x00075144]; }
FUN_00067B80(block, entry);
```

The byte it branches on is **which fighter this is, not a mode**: measured over `fv2-fight.txt`,
`0x00700508` is 0 and `0x00707508` is 1 on every frame of the run, never changing. So
`FUN_0003F874` **always** re-arms that fighter's cloth task at its entry -- there is no further
condition inside it. Measured: `--trace-pc 67B80` over `fv2-fight.txt` reports eight calls and
nothing else, every one of them with `LR 0x0003FE74` -- six with `(0x0070EC00, 0x0005756C)` and
two with `(0x0070ED00, 0x000575A0)` -- and `--trace-pc 3F874` reports exactly eight entries. One
run of the model setup, one re-arm.

`FUN_0003F874` takes the fighter record in **r12**, which is what makes `fighter[8]` reachable:
six of its ten call sites load r12 from `[0x0007511C]` or `[0x00075120]`, the two fighter-block
pointers. (The other four do not: two move it from another register and two load it from
`0xDF8(rX)`. So the argument is a fighter-shaped record, established at six sites, not ten.)

### and one level up: what the select screen actually does

`FUN_0003F874` is **not called by the select screen**. Measured: of the eight entries to it,
five arrive with `LR 0x000391AC` -- the scheduler's `blrl` -- so it is itself a *task step*, the
fighter's own task, block `0x00700500` / `0x00707500`. (That is also why r12 holds the fighter
record: the scheduler leaves the task block there.)

So the chain has one more link, and `vf3run --watch 700504` -- fighter 0's own task resume --
gives it whole:

```
6 x  store32 00700504 = 40134 at PC 0003F8A4 (LR 0003F884)   the task yielding to its next step
3 x  store32 00700504 = 3F874 at PC 000440C0 (LR 000602C0)   <- the select screen
1 x  store32 00700504 = 3F874 at PC 000440C0 (LR 0000C1BC)   <- SET_INT, for the fight
1 x  store32 00700504 = 3F874 at PC 000440C0 (LR 00073238)   the ending
1 x  store32 00700504 = 3F874 at PC 000440C0 (LR 00072FD0)   the ending
1 x  store32 00700504 = 3F874 at PC 000390FC (LR 00038C38)   the boot installing the table
```

`0x000440C0` is inside **`FUN_000440A0`**, which is "rebuild this fighter's model":

```
FUN_000440A0(r3 = which fighter, 0 or 1, ...)       0x000440A0
        r3 = extsb(r3);
        r9  = *(void**)(0x002748C8 + r3*4);
        r11 = *(void**)r9;                   /* the fighter's task block */
        *(void**)(r11 + 4) = 0x0003F874;     /* 0x000440C0 -- arm it at the model setup */
        r11[0xC0] = r11[0xD4] = r11[0xCC] = r11[0xE0] = 0;
        ...
```

and the select screen calls it at **`0x000602BC`** with `r3 = 0` (`li r3,0x0` at `0x000602B8`),
behind a test of bit 1 at `0x00060288`. Three times in a run in which the cursor was moved twice
and a fighter confirmed.

**So the answer to "what re-arms the preview, and under what condition" is:**

1. the select screen, at `0x000602BC`, calls `FUN_000440A0(0)` -- *rebuild fighter 0's model*;
2. that arms the fighter's own task (`0x00700500`) at `FUN_0003F874`;
3. the scheduler runs it next frame, and its last act is `FUN_00067B80` on that fighter's **cloth**
   task (`0x0070EC00`) at `0x0005756C`, which is what builds the clothed model.

Nothing in any of the three watches the cursor or compares it with a previous value. The
condition is simply that the select screen decided to call `FUN_000440A0`; what that bit-1 test
at `0x00060288` reads was **not** run down, so "when the highlighted fighter changes" is the
behaviour, not something this page has read.

The cleanest lever is therefore `FUN_000440A0(0)`, one level above the cloth task, because it is
the game's own "rebuild this fighter" and it resets the four fields at `+0xC0`, `+0xCC`, `+0xD4`
and `+0xE0` that the step expects to find cleared. The cloth-task write below is the smaller
hammer and was the one measured.

**And it can be driven by hand, measured.** With the cursor never moved at all
(`1900 p1.coin / 1930 p1.coin / 1960 p1.start`, nothing after), poking `FUN_00067B80`'s two
fields for two frames:

```
VF3_POKE=70EC07=0,70EC06=5,70EC05=117,70EC04=108   /* 0x0070EC04 := 0x0005756C */
VF3_POKE_FRAMES=2150-2151
```

| run | entries to `0x0005756C` (`--trace-pc 5756C`) |
| --- | --- |
| untouched | **4** |
| the two frames poked | **6** |

Two extra entries, one per poked frame, with the cursor untouched; the run finished with no
fault and the select screen renders normally afterwards. Bit 0 of `0x0070EC00` is already set
while the screen is up (measured: the block's first word reads `0x80000005`, then `0x80000001`
from frame 2038), so on the select screen the resume word alone is enough; anywhere the task has
been stopped, bit 0 has to be set as well, which is the other half of `FUN_00067B80`.

This answers the owner's warning in `fv2mod\HANDOFF.md` -- that the writes Start leaves behind
are posted by the scene loader and do not command it -- in the opposite direction for *this*
field: `[block+4]` is read by the scheduler every frame, and writing it is exactly what the
game's own `FUN_00067B80` does.


## 5. The variables, by Sega's names

Every one of these is a debug-table entry; the page name is Sega's.

| address | page | Sega's label | measured |
| --- | --- | --- | --- |
| `0x0058FD98` | `MODE_INFO` | `B_MAIN_MODE` | yes |
| `0x0058FD99` | `MODE_INFO` | `B_SUB_MODE` | yes |
| `0x0058FD9A` | `MODE_INFO` | `MODE_REQUEST` | yes |
| `0x0058FD9B` | `MODE_INFO` | `S-MODE_REQUEST` | yes |
| `0x0058FD4C` | `MODE_INFO` | `M_MAIN_MODE` | read in the code |
| `0x0058FD50` | `MODE_INFO` | `M_SUB_MODE` | read in the code |
| `0x00581404` | `MODE_INFO` | `*GAME TIMER` | yes -- reached 0 at frame 4640 |
| `0x0058145F` | `MODE_INFO`, `CONFIG` | `SURVIVAL FLAG` / ` SURVIVAL MODE` | read in the code |
| `0x0058145E` | `MODE_INFO` | `RANDOM_CHARA` | no |
| `0x00581461` | `MODE_INFO`, `CONFIG` | `BONUS_STG_FLAG` / `*BONUS WIN` | no |
| `0x0058145C` | `CONFIG` | `*ROUND NO` | yes -- 1, then 2 at the second opponent |
| `0x005813F0` | `ADV_INFO` | `STATUS` | yes -- `0x24000000` on the Start frame, `0x80000000` on time over |
| `0x00580DFC` | `ADV_INFO` | `SCENE No.` | yes |
| `0x00580DD2` | `ADV_INFO` | `FRAME` | no |
| `0x00580E08` | `ADV_INFO` | `SUBMODE` | read in the code |
| `0x0058FD9F` | `STAGE_TEST` | `*STAGE` | yes -- 0x09 then 0x01 for the second opponent |
| `0x007005B8` | `CONFIG` | `*1P CHARA` | yes |
| `0x007075B8` | `CONFIG` | `*2P CHARA` | yes |
| `0x007005B9` | `CONFIG` | `*1P COLOR` | no |
| `0x00700000` | `CONFIG` | `*ENTRY 1P` | yes -- the task block |
| `0x00700100` | `CONFIG` | `*ENTRY 2P` | yes -- the task block |
| `0x00120540` | `CONFIG` | `*1P ENERGY` | read in the code |
| `0x00120542` | `CONFIG` | `*VS ENERGY` | read in the code |
| `0x00120550` | `CONFIG` | `*SURVIVAL KANOU` | no |
| `0x0011F060` | `CONFIG` | `*PLAY KUHN` | read in the code |
| `0x0011F061` | `CONFIG` | `*PLAY DELSOL` | read in the code |
| `0x00701444` | `ROB0_ARMOR`, `ARMOR_INFO` | `armor_flag` / `flag0` | (see `docs/fv2-honey.md`) |
| `0x00581450` | `POINT` | `perfect` | read in the code |
| `0x00581451` | `POINT` | `straight` | read in the code |
| `0x00581444` | `POINT` | `vipers_point_s` | read in the code |

And the ones this work named itself, because the debug tables do not:

| address | what the measurement says it is |
| --- | --- |
| `0x0058FD54` | the screen counter: 0x140 at `WARNING_INT`, 0x40 at `SEL_INT`, 0x60 at `JUDGE`, ticked down once per dispatch |
| `0x0058145A` | who is in: 0 in the attract, 1 from `GAME_INT`, back to 0 on the return. 3 is both |
| `0x00581456` | the round number within a match: set to 1 at `ROUND_INT`, incremented at `NEXT_PROG_SEL` |
| `0x00581454`, `0x00581455` | the two round-win counters, both zeroed at `ROUND_INT`; `0x00581455` counted the side that was winning |
| `0x00581408` | the select screen's countdown, reloaded with 0x3F on a cursor move |
| `0x00581414` | frames played in the round, zeroed at `ROUND_INT` |
| `0x00075108`..`0x00075164` | the scheduler's table of task-block pointers, 23 slots |
| `0x0058FDBC`, `0x0058FDC0` | the slot and the block the scheduler is running |


## 6. Nothing in the port moved

The only change this work made to the port is `VF3_RAMLOG`, which is off unless the environment
variable is set and only reads RAM. Measured against the binary as it stood before
(`build/bin/vf3run` at `421f342`), same ROMs, same scripts:

| check | result |
| --- | --- |
| Virtua Fighter 3, the PowerPC's state frame by frame, 600 frames | **byte-identical** (307,200 bytes) |
| Fighting Vipers 2, the same over all of `fv2-fight.txt` | **byte-identical** (2,150,912 bytes) |


## 7. What is not settled

- **The second player was never got in.** `0x0058145A == 3` gates the cut-in, the consecutive-win
  counter and the two-player branch of `NEXT_PROG_SEL`, and no run here reached it. The whole
  `INTRUDE_*` pair and the two-player half of `NEXT_PROG_SEL` are read, not watched. This is the
  same gap `docs/fv2-honey.md` ends on.
- **The ending, the name entry, the ranking and the SKO states were not reached.** Their handlers
  are named here from the game's own table -- that naming is as good as the rest -- but nothing
  below `0x0000E15C`, nor `0x00069714`-`0x00069BCC`, nor `0x000725D0`-`0x00072AF8`, was read or run.
- **Why the mode machine does not tick once a video frame during the warning screen** was not run
  down. 321 ticks took 773 frames; the suspend bit at `0x00591D14` was 0 throughout, so it is
  something else.
- **Which of `0x00581454` / `0x00581455` is which side** was not established: only `0x00581455`
  was logged, and only in a run the player was losing.
- **The hidden-fighter code in `ADV_DSP`** is read in the code and was never entered, so the two
  sequences at `0x00110B24` and `0x00110B6C` were not decoded and the claim that they set
  `*PLAY KUHN` and `*PLAY DELSOL` rests on reading alone.
- **Survival mode** was never run: its branches in `ROUND_MASK_INT`, `ROUND_INT` and `FIGHT_INT`
  are read only.
- **`FUN_0005EF68`, `FUN_0005FD94` and `FUN_000603D4`** are named as the select screen's per-frame
  work, its confirm, and the builder of `0x0058145A`, from where they are called and from
  `docs/fv2-honey.md`. Their insides were not read: they are past this page's perimeter.
- **What the select screen tests before rebuilding a fighter** (the `andi. r9,r0,2` at
  `0x00060288`, whose word was not traced back) is not established, so section 4 says what
  commands the rebuild but not what decides to command it.
- **`0x0000ABC4` is not what an earlier note called it.** `fv2-arcade-notes/symbols.py` lists
  `0x0000ABC4` as `fn_ecrit_progression`. Measured here, it is the main mode machine's tick:
  `--watch 58FD98` reports `B_MAIN_MODE` written at `0x0000AC10` with `LR 0x00039068`, and
  `0x00039064` is `bl 0x0000ABC4`.

## 8. Le relevé des combats : `FUN_000701C8`

Lu dans Ghidra le 2026-10-23, à la demande de Frédéric, pour savoir ce qui sépare `SKO 1` de `SKO 2`.
C'est **la routine qui range un match dans le tableau des statistiques**, celui que la borne montre sous
`VS DIAGRAM` dans son menu de test et que la Dreamcast reprend en plus large.

### La table vivante

Trouvée par l'écran de la borne lui-même (`FUN_00070924`, qui l'affiche) :

```
base      0x0011F7E0
pas       0xD4 par personnage        (index = fighter[0xB8], le *1P CHARA du flux)
  +0x00   KO          entier 32 bits
  +0x04   SKO 1       entier 32 bits
  +0x08   SKO 2       entier 32 bits
  +0x0C   JUDGE       entier 32 bits
  +0x10   DRAW        entier 32 bits
  +0x14   FIGHT[16]   un entier par adversaire, de 4 en 4
  +0x54   WIN[16]
  +0x94   LOSE[16]
```

**Ce sont des entiers 32 bits**, `lwz`/`stw`, et non des mots de 16 bits. Dans la sauvegarde, où tout est
espacé à deux octets utiles sur quatre, chaque entier occupe huit octets et **la valeur se lit dans les deux
premiers** — mesuré : `00 01 00 00 00 00 00 00` pour un compteur à 1. Une lecture 16 bits au décalage du
champ donne donc le bon compte tant qu'il reste sous 65536 ; au-delà il faudrait la moitié haute, quatre
octets plus loin.

Les décalages de la sauvegarde sont exactement le double de ceux-ci : `0x1A8` pour `0xD4`, `0x28` pour
`0x14`, `0xA8` pour `0x54`, `0x128` pour `0x94`, et 8 entre adversaires pour 4. La sauvegarde est une image
espacée de cette table, rien de plus.

### Ce que la routine fait

```
char0 = fighter0[0xB8]        /* [0x0007511C] + 0xB8 */
char1 = fighter1[0xB8]        /* [0x00075120] + 0xB8 */

FIGHT[char0][char1]++ ;  FIGHT[char1][char0]++        /* toujours, des deux cotes */

if (0x00581453 == 2) {                                 /* match nul */
        DRAW[char0]++ ;  DRAW[char1]++
} else {
        gagnant = (0x00581453 == 0) ? char0 : char1
        perdant = l'autre
        WIN[gagnant][perdant]++ ;  LOSE[perdant][gagnant]++

        switch (0x00581452) {                          /* LE GENRE DE FIN */
        case 2:   JUDGE[gagnant]++                     /* decision au juge */
        case 3:   SUPER_K_O ? SKO2[gagnant]++ : SKO1[gagnant]++
        default:  KO[gagnant]++
        }
}
```

### Deux variables nommées au passage

| adresse | ce que c'est |
|---|---|
| `0x00581452` | **le genre de fin** : 2 au juge, 3 Super K.O., le reste un KO ordinaire |
| `0x00581453` | **qui a gagné** : 0 le combattant 0, 2 un nul, sinon le combattant 1 |

La section 7 donnait `0x00581454` / `0x00581455` comme « on ne sait pas lequel est quel côté » ; ces deux-ci
sont juste avant et ne sont pas les mêmes.

### Et `SKO 1` contre `SKO 2` : ce n'est pas deux coups

C'est ce que la lecture apporte de plus inattendu. Les deux colonnes comptent **la même fin** — `0x00581452`
vaut 3 dans les deux cas — et ce qui décide de la colonne est **le réglage `SUPER K.O.` lui-même** :

```
00070304  lis   r9, 0x12
00070308  lbz   r0, 0x554(r9)      ; le reglage SUPER K.O., 0x00120554
0007030c  cmpwi cr1, r0, 0x0
00070310  beq   cr1, 0x00070324
00070314  lwz   r0, 0x8(r10)       ; reglage ALLUME  -> +0x08, la seconde colonne
0007031c  stw   r0, 0x8(r10)
00070324  lwz   r0, 0x4(r10)       ; reglage ETEINT  -> +0x04, la premiere
0007032c  stw   r0, 0x4(r10)
```

Autrement dit l'exploitant qui éteint `SUPER K.O.` ne supprime pas la fin : il la fait compter dans l'autre
colonne. Les deux cases séparent donc **les Super K.O. obtenus réglage éteint de ceux obtenus réglage
allumé**, et non deux manières de les faire.

### Et ce que le réglage commande vraiment

Il ne décide pas si la fin peut arriver — le compteur monte dans les deux cas. C'est une option
**d'affichage**, et deux routines le montrent :

- **`FUN_00059E24`**, les marques de rounds gagnés. Éteint, le jeu dessine de simples marques, `0x00581454`
  d'un côté et `0x00581455` de l'autre. Allumé, il parcourt l'historique des rounds à `0x00581897` sur
  `0x00581456` entrées et donne à chacun un symbole **selon la façon dont il a été gagné** : 1 et 2 pour le
  côté gauche, 3 et 4 pour le droit, `0xFF` pour un double KO qui en pose un des deux côtés.
- **`FUN_0005C760`**, qui change la bannière affichée pendant le combat selon le même réglage.

Ce qui donne aussi un sens à la séparation des deux colonnes : réglage allumé, le joueur a été prévenu et la
fin lui est annoncée ; éteint, elle lui est tombée dessus sans rien dire. Deux statistiques différentes pour
l'exploitant.

Au passage, `0x00581454` et `0x00581455` sont **les rounds gagnés de chaque côté**, ce que la section 7
donnait pour non établi, et `0x00581456` est le nombre de rounds joués.
