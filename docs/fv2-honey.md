# Fighting Vipers 2: Honey without her skirt

The question put to this page: the owner reports that Honey has a "no skirt" model, obtained by thirty or more
straight wins in versus with a second player cutting in (乱入対戦), with her lower armour destroyed during those
matches or on the thirty-first. That was player folklore when this started.

**It is in the code, it is Honey's alone, and the number is thirty.** What follows is what was measured, in the
order it was measured, and at the end what is still not settled.

Nothing here comes from Supermodel. The game's own debug tables did most of the work; the rest is
`tools/vf3run` and the ROM.


## The short answer

It is **not** a separate model, and it is **not** merely an Armor Break state. It is the ordinary Honey, with
three halfwords of her cloth model's working copy zeroed, and the game only does that when **both** of these
hold **at the moment her model is set up, which is when a round loads**:

- `armor_flag & 8` -- her **lower** armour has been knocked off, not merely damaged past its limit;
- the word at `0x00581438` is **greater than 29** -- the current winner's run of consecutive wins.

That second word only ever advances when `0x0058145A == 3`, and `0x0058145A` is 1 when player 1 has entered and
`+2` when player 2 has. **Three means both players are in** -- which is what 乱入対戦 is.

So the folklore is right on every point: the thirty, the lower armour, and the skirt going **at the next
round**, because the handler that decides it does not run while a round is being played.


## The instruments

`tools/scripts/fv2-honey.txt` picks Honey and fights the CPU without a window. Honey is cell **8** of the select
row -- eight presses of right from Emi -- and her in-fight identifier is **6**, which is what `0x007005B8`
holds once the fight starts. Both read back from `--ram-at`'s dump.

`VF3_POKE=addr=value` with `VF3_POKE_FRAMES=from-to` writes RAM before every frame, so an effect can be tried
without compiling anything. The address is an index into `--ram-at`'s dump, which keeps 32-bit words in the
host's order: **the byte of guest address A is at index A^3**. Every address on this page is a *guest* address;
every address in a `VF3_POKE` is the index, which is why the bytes look reversed there.

`montrer-honey.cmd` runs the whole measurement twice and leaves the two folders of pictures side by side.


## Sega named these variables themselves, and nobody had read that table yet

`fv2-arcade-notes/symbols.py` pulls Sega's own debug-table names out of the program, but it scans two narrow
ranges and keeps only the entries whose variable field is an absolute address -- 84 names. Scanning the whole
debug data region instead finds **97 pages and 1035 named entries**, and the layout is this:

- a *page* record is three words: pointer to the page's name, pointer to its first entry, pointer to its
  terminator;
- an *entry* is `0x20` bytes: `+00` pointer to the label, `+08` pointer to a name array (for enum and bit
  fields), `+0C` the variable, `+1C` the type;
- **the terminator's first word is the base** the page's `+0C` fields are offsets from. For the per-fighter
  pages that base is `0x00700500` (player 1) and `0x00707500` (player 2).

That base is what the older script was missing, and it is why the armour fields had never been read: they are
stored as offsets, not addresses. Among the pages are `ARMOR_INFO`, `ROB0_ARMOR`, `ROB_OBJ_CHANGE`,
`ROB0_rob_flag` -- and among the bit names, `hadaka`.


## The armour, and where it lives

`ARMOR_INFO` names them with absolute addresses, which agree with the offsets on `ROB0_ARMOR`:

| Sega's name | player 1 | player 2 | size |
|---|---|---|---|
| `flag0` / `flag1` | `0x00701444` | `0x00708444` | 32-bit |
| `damage0(u)` / `damage1(u)` | `0x00701438` | `0x00708438` | 32-bit |
| ` limit0(u)` / ` limit1(u)` | `0x00701440` | `0x00708440` | 16-bit |
| `damage0(d)` / `damage1(d)` | `0x0070143C` | `0x0070843C` | 32-bit |
| ` limit0(d)` / ` limit1(d)` | `0x00701442` | `0x00708442` | 16-bit |

`u` is the upper armour, `d` the lower one. Read mid-fight, Honey against the CPU's Charlie, frame 4350:
`dmgU=124 limU=350 dmgD=119 limD=300`, `flag0=00000000`. **Honey's upper armour takes 350 points and her lower
one 300.**

### The bits of `flag0`

Measured, three runs of the same script differing only by a poke, RAM read at the same frame:

| poke | `flag0` |
|---|---|
| none | `00000000` |
| `limit_d` forced to 1 | `00000002` |
| both limits forced to 1 | `00000003` |

The three runs' `rob_flag` came out identical (`3B610001`), so the poke did not disturb the simulation: clean
A/B, same camera, same pose.

The code says the rest. At `0x00067E90`, every frame: if `!(flag & 5)` and `damage_u >= limit_u`, set bit 0; if
`!(flag & 0xA)` and `damage_d >= limit_d`, set bit 1. And at `0x00067FC4` a routine dispatched on which body
part was hit:

```
part 0 (upper)  if !(flag & 4) and (flag & 1)   subtract weight_u, flag |= 0x14
part 1 (lower)  if !(flag & 8) and (flag & 2)   subtract weight_d, flag |= 0x28
part 2 (both)                                   subtract both,     flag |= 0x3C
```

So:

| bit | meaning |
|---|---|
| `0x01` / `0x02` | the upper / lower armour's damage has reached its limit |
| `0x04` / `0x08` | that piece has actually come **off** (its weight is out of the fighter's) |
| `0x10` / `0x20` | it came off just now; cleared again at `0x00067DC0` |

**`0x08` is the lower armour gone, on its own** -- that is the branch at `0x0006800C`, and it is reached
without the upper one. That is exactly what the folklore asks for.

### Bits 0 and 1 on their own do nothing to the model

Forcing `flag0` to 2 or to 3 mid-fight changes the picture by **510 and 1061 pixels**, and every one of them
is inside `x 1..28, y 7..92` -- the little green figure in the corner, the armour indicator. The 3D model is
byte-identical, in that round and in the one after. Reaching the limit is not the same as the piece coming
off: bit 3 is, and that is the one the model reads.


## `hadaka` -- Sega's own word, and it is a dead end for this question

The per-fighter status word `rob_flag` (`0x00700500` / `0x00707500`) has a bit Sega named **`hadaka`**, 裸,
bare. It is `0x00000002`.

The bit order was settled twice. By plausibility first: read mid-fight `rob_flag` is `0x71010401`, and reading
the names most-significant-bit first gives `left_look`, `fix_hara`, `ypos_chk_pass`, `init_end`, `rob_calc`,
`kao_init`, `event` -- all sensible in the middle of a round -- where the other direction gives `ring_out` and
`hadaka` both set. Then by the code, which is not an opinion:

```
00067E6C  lwz  r0,0x0(r3)       the status word
00067E70  stw  r9,0xf44(r3)     the armour flag, with 0x14 and 0x28 just folded in
00067E7C  ori  r0,r0,0x802      <-- 0x002 is hadaka
00067E80  stw  r0,0x0(r3)
00067E84  stw  r9,0xf38(r3)     damage_u := limit_u
00067E88  stw  r11,0xf3c(r3)    damage_d := limit_d
```

`hadaka` is set when **all** of a fighter's armour is off (also at `0x00067DB0` and `0x00068074`, both guarded
by `flag & 4` *and* `flag & 8`). It is a status bit for every character, not Honey's skirt. Poking it alone, in
a fight, changes nothing about her.


## The cloth task, and the switch on who you are

The game has two tasks called `cloth0` and `cloth1` -- found in the task table at `0x0008D054`, with the work
areas `0x0070ED00` and `0x0070EE00`. `cloth0`'s function is at `0x000575A0`, and the body of it begins:

```
0005760C  lbz    r11,0xb8(r31)      r31 is the fighter; +0xB8 is the character id
00057610  cmplwi r11,11
0005761C  bgt    0x00057b9c         12..15 have no cloth: return
...
00057640  bctr                      through the table at 0x00057644
```

The table is twelve words, offsets from `0x00057644`:

| id | fighter | handler |
|---|---|---|
| 0 | GRACE | `0x000579D4` |
| 1 | BAHN | `0x00057674` |
| 2 | RAXEL | `0x000576E8` |
| 3 | TOKIO | `0x00057950` |
| 4 | SANMAN | `0x00057AE8` |
| 5 | JANE | -- returns |
| **6** | **HONEY** | **`0x00057A1C`** |
| 7-10 | PICKY, MAHLER, BM, CHARLIE | -- return |
| 11 | TECHNO (Emi) | `0x000577A8` |

Measured, not deduced: `tools/vf3run --trace-pc 57A1C` fires during a Honey fight with `r3 = 0x00700500`, and
it fires **twice in a 4500-frame run**. The handler is not per-frame work; it runs when her model is set up.

That one fact is why the first attempt at this came back negative and why the owner's "at the next round" is
the hinge: forcing the armour flag in the middle of a round and comparing the same frames compares two
pictures built from a model that was chosen before the poke existed. Nothing can change there. The comparison
has to straddle a round end.


## Honey's handler, which is the whole answer

```
00057A1C  lwz    r0,0xf44(r31)      her armour flag
00057A20  andi.  r9,r0,0x8          the lower armour is off
00057A24  beq    0x00057a64         no -> the ordinary skirt
00057A28  lis    r9,0x58
00057A2C  lwz    r0,0x1438(r9)      0x00581438
00057A30  cmpwi  r0,29
00057A34  ble    0x00057a64         29 or fewer -> the ordinary skirt
00057A38  ...                       otherwise: zero three halfwords of the cloth model
00057A60  b      0x00057b9c
00057A64  ...                       the ordinary skirt
```

The loop at `0x00057A38` writes zero to the halfwords at `0x00471C48 + n*2560 + {0x28, 0xC8, 0x168}`, where
`n` is the fighter's index -- three records of 160 bytes in the **working copy** of the cloth object, the one
the ordinary path at `0x00057A64` builds from the master at `0x00431C48`. Three numbers, and the skirt is not
drawn.

**There is no second Honey in the ROM.** There is one Honey, and a counter the game blanks when you have
earned it.

### Shown, not argued -- and the round end is the whole of it

`tools/scripts/fv2-honey-rounds.txt` is the same fight carried far enough to cross a round end; with this seed
the first round ends between frames 4900 and 5100. Three runs of it, nothing in the game touched, only RAM
written before each frame:

| run | what was written | |
|---|---|---|
| **témoin** | nothing | |
| **armure** | `0x00701444` low byte = `0x0A` | her lower armour has reached its limit and come off |
| **trente** | the same, and `0x00581438 = 40` | and forty straight wins |

Diffed frame by frame, **armure** against **trente**:

```
3900  round 1        0 pixels
4300  round 1        0 pixels
4700  round 1        0 pixels
------------------------------ the round ends here
5100  round after    541 pixels
5300  round after   2252 pixels
5500  round after    932 pixels
```

**Nothing while the round runs, and a different Honey from the first picture of the round after it.** At frame
5300 the two are the same pose in the same camera: one still in her skirt, one not. That is the whole claim,
and it needed no instruction patched -- only the flag Sega's own `ARMOR_INFO` page names, and the counter.

`montrer-honey.cmd` runs the three and leaves the two folders that matter open.

Two more, taken before the round end was understood, which pin the mechanism rather than the behaviour:

| run | what was poked | against the untouched run |
|---|---|---|
| A | the `beq` at `0x00057A24` turned into a `nop` | **0 pixels**, every frame |
| B | the same `nop`, and `0x00581438 = 40` | 900-2700 pixels, moving with Honey |
| C | the same `nop`, and `cmpwi r0,29` turned into `cmpwi r0,-1` | **identical to B**, every frame, byte for byte |

A says the `nop` on its own does nothing -- the counter was 0, so the test still sent her down the ordinary
path. C says one word at `0x00057A30` does exactly what thirty wins does, which is the lever this port would
use.


## The counter: what it counts, and what gates it

`0x00581438` is written in three places and nowhere else (`tools/vf3run --watch 581438` over a whole game
confirms it):

```
0000AFBC  stw r3,0x1438(r2)        r3 = 0: the reset, with 0x581414, 0x58145A, 0x58145B,
                                   0x5814A9, 0x581592, 0x581593, 0x5814AA, 0x581434
0000CA10  stw r0,0x1438(r2)        = 1   (player 1 has just won, and did not win last time)
0000CA28  stw r3,0x1438(r2)        += 1  (player 1 has won again)
0000CC10  stw r3,0x1438(r2)        the same pair for player 2, at 0x0000CBF8 and 0x0000CC10
```

Around them:

```
0000C9BC  lbz    r3,0x14a9(r3)     who won last time: 1 or 2
0000C9C4  stb    r3,0x1592(r2)     kept
0000C9C8  cmplwi cr0,r19,3         r19 is the byte at 0x0058145A
0000C9D0  cmplwi cr2,r18,1         r18 is the byte at 0x0058145B
0000C9D4  bne    cr0,0x0000ca34    not 3 -> nothing is counted
0000C9EC  beq    cr2,0x0000ca18    the same side again -> increment
```

and `0x0058145A` is built at `0x000603D4`:

```
0x0058145A = 0
if (player 1's entry flags & 6)  0x0058145A  = 1
if (player 2's entry flags & 6)  0x0058145A += 2
```

So the counter is **the current winner's run of consecutive wins, counted only when both players are in the
machine**. A copy goes to `0x00581684` on the increment path only, so that one is the run when it is two or
more.

Measured against a one-player game: `--trace-pc C9C0` over a long run against the CPU fires three times -- three
matches won, `r3 = 1` each time -- and `--watch 581438` over the same run reports no write at all beyond the
reset. **The CPU ladder does not count.** The gate did its job.


## What is not settled

- **The counter was never seen to increase.** Both the increment sites and the gate were read in the code and
  the one-player case was measured to be excluded, but no run in this tree has produced a finished two-player
  match. The script that tries to cut a second player in (`p2.coin`, `p2.start` during player 1's fight) does
  not get there: the game is back at the title by the time it would matter. Driving a real cut-in needs the
  windowed host and a second pad, or a script written against the entry task rather than against the frame
  numbers.
- **The lower armour was never knocked off by play here.** Forcing `limit_d` to 1 reliably sets `flag0` bit 1
  (damage past the limit), but the random-input fights never reached the removal step that sets bit 3, so bit
  3 was written straight into RAM instead. The branch at `0x0006800C` that sets it for the lower armour alone
  was read, not watched.
- **`tools/vf3run` never loads the NVRAM** -- it starts from a blank card every time. That does not weaken
  anything here, because the counter is in RAM: `--watch 581438` over a whole game reports the program's own
  three writes and nothing else, and the reset at `0x0000AFBC` zeroes it. But it does mean that if there is a
  *second*, saved counter behind this, nothing on this page would have seen it.
- **What the three zeroed halfwords are** was not decoded. They sit at `+0x28` of three consecutive 160-byte
  records, and zeroing them stops the skirt being drawn; whether the field is a polygon count, a leaf count or
  something else was not established.
- Whether the thirtieth win has to be the one in which her armour goes, or whether any order does, is not
  settled either -- but the code says it cannot matter: the handler reads both conditions at the same instant,
  when a round loads, and nothing remembers the order. "During those matches, or on the thirty-first" is
  therefore two descriptions of one thing.
- `*1P COLOR` (`0x007005B9`) was swept as a possible second costume and is not one. Poked to 1 or 3 during a
  fight it only repaints the name plate (382 and 816 pixels, all inside the plate); poked to 2 the CPU stops
  with an invalid instruction at `0x00020908`; poked to 4 the whole screen changes. None of that was run down,
  and none of it touches the model.


## Reaching it from this port

Two ways, both measured above.

**Without changing the port.** `montrer-honey.cmd`, or by hand:

```
VF3_POKE=58143B=0,58143A=0,581439=0,581438=40   0x00581438 = 40 consecutive wins
VF3_POKE=701444=10                              0x00701444 low byte = 0x0A, her lower armour off
VF3_POKE_FRAMES=3200-5700
```

Both have to hold when a round loads, and the skirt goes at **that** round, not the one being played. In a
real game only the first of the two is ever written by hand: her lower armour comes off by being hit.

**As an option, the way `--full-roster` is done.** One word, at `0x00057A30`:

```
2C80001D   cmpwi r0,29      ->   2C80FFFF   cmpwi r0,-1
```

and the counter never matters again: knock Honey's lower armour off and the skirt goes. Run C above proves the
two are the same thing, byte for byte.

The same care as the roster patch applies and for the same reason (`docs/fv2-roster.md`): the program is a gzip
in the fixed CROM that the game's own boot stub inflates into RAM, so there is nothing at `0x00057A30` to patch
until the boot has got that far, the word must be **read back and checked against what Sega wrote** before
anything is written, and the write has to land before the recompiler rebuilds from RAM. This page does not add
that option; it says what it would be.

It is also `fvipers2` only. `fvipers2o` is a different build and `0x00057A30` means nothing there until
somebody looks.
