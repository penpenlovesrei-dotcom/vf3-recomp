# OPTIONS: the plan

Written on 2026-10-22, before a line of it had been written. This is not a description: it is the questions,
in order, and the measurement that settles each one.

`OPTIONS` is the last line of the list that means anything here. `NETWORK` has nothing behind it on the
cabinet; `OPTIONS` does: the game test menu's **thirteen settings**. The inventory said it already — "the
work is the page, not the mechanism". That is half true, and the false half is this document.


## What is settled

### The thirteen settings, measured in August

`MenuConsole.md` §9: a watcher printed every byte that changed in `0x120540`-`0x12055F` while **one setting at
a time** was modified in the test menu.

| setting | address | range | factory |
|---|---|---|---|
| `MATCH COUNT 1P` | `0x12054C` | 2-5 | 2 |
| `MATCH COUNT VS` | `0x12054D` | 2-5 | 3 |
| `ROUND TIME 1P` | `0x120555` | 20-60 by 5 | 30 |
| `ROUND TIME VS` | `0x120556` | 20-60 by 5 | 45 |
| `DIFFICULTY` | `0x12054E` | 0-4 | 2 `NORMAL` |
| `ENERGY MAX 1P` | `0x120540` | 160-360 by 10, **16-bit big-endian** | 210 |
| `ENERGY MAX VS` | `0x120542` | the same | 210 |
| `ADVERTISE SOUND` | `0x12054F` | 0/1 | `ON` |
| `SURVIVAL MODE` | `0x120550` | 0/1 | `OFF` |
| `COUNTRY` | `0x120557` | read-only | `JAPAN` |
| `VS FINISH` | `0x120558` | 0/1 | `OFF` |
| `SUPER K.O.` | `0x120554` | 0/1 | `ON` |
| `BIKE LOGO` | `0x12055A` | 0/1 | `ON` |

The order on screen is not the order of the addresses — `ROUND TIME` is nine bytes after `MATCH COUNT` while
it comes before it. And **nothing bounds a direct write**: the test menu would refuse a value out of range,
we would not. So the ranges above are the only guard.

`SURVIVAL MODE` (`0x120550`) is already used by this port, next to the Survival flag.

### The trap: this block is only a working copy

The game loads it from its **backup** at start-up. Changing it does not last: only the game's test menu copies
back to the backup, on the way out. August's menu worked round that with a file of its own, read back at
launch and put in place **at the instant the game has just loaded its block**. Question 1 below makes that
workaround unnecessary: the backup is reachable.

### But this port already knows how to do better, for the other game

For Virtua Fighter 3, the "Arcade settings" page does not write the working copy: it writes **the backup
itself**, one byte per setting from `0x1E000` on, found by changing each of them in the test menu. The game
reads them back when starting, so a change **restarts the game** when the menu is closed, and it holds.

And this port has already done exactly that work for Fighting Vipers 2 once: the backup's byte `0x17`, the
COIN/CREDIT setting, found by driving the game's test menu from an input script and then comparing the backup
before and after (`docs/fv2-console.md`).


## Question 1 — where those thirteen settings are in the backup: **closed on 2026-10-22**

`HANDOFF.md` §7 said that the backup "does not contain those values as they stand" and that nobody had
decoded it. It is done, and it did not take driving the test menu thirteen times: it was enough to look at
**who writes the working copy at start-up**.

`vf3run --watch 120540` gives `store32 00120540 = D200D2 at PC 0002026C (LR 00020F60)` — 210 and 210, both
`ENERGY MAX` at their factory value, in a single word. `0x00020240` is a `memcpy` by words (r3 destination, r4
source, r5 count): the information is at the caller, and there are two of them.

| | r3 (live) | r4 (source) | count |
|---|---|---|---|
| `0x00020D80` | `0x00120520` | **`0xFE0C0000`, the backup** | 30 half-words |
| `0x00020F38` | `0x00120520` | `0x00120D24`, **the factory table in the program** | 60 bytes |

And the arbiter is `0x00020F98`, which loads **four** blocks and then validates:

| block | live | backup | bytes |
|---|---|---|---|
| 1 | `0x0011F024` | `0xFE0C1000` | 1604 |
| 2 | `0x0011F668` | `0xFE0C2000` | 376 |
| 3 | `0x0011F7E0` | `0xFE0C3000` | 3392 |
| 4 | **`0x00120520`** | **`0xFE0C0000`** | **60 — the settings** |

The four are **contiguous in RAM** (`0x0011F024` to `0x0012057C`) and on **four separate pages** of the
backup. Validity is three fields at the head of block 1: the word `0x0011F024` equal to `0x53454741`
("SEGA"), the half-word `0x0011F028` equal to `0x8221` and `0x0011F02A` equal to `0x0312` — a version stamp.
If one is missing, `0x00020F74` restores the factory settings and `0x00021008` **writes them back into the
backup**.

### The correspondence

The loader reads one **word** of the backup per live **half-word** (`lwbrx` then `sthu`), so each 16-bit value
occupies four bytes of the backup and only two carry the data. For a live byte at `0x00120520 + k`:

```
index in board.backup = 2k + 2   if k is even
                        2k + 1   if k is odd
```

**An immediate cross-check**: the coin byte this port already knew, `0x17`, falls back on `k = 11`, that is
`0x0012052B` — inside the `COIN ASSIGNMENTS`, where it belongs. It had been found by trial and error; the
formula gives it again.

### The thirteen, measured

The "live" and "factory" columns are read from a RAM dump; the "backup" column is the index in
`board.backup`, and its value was read back from **your** `backup-fvipers2` of 21 October. All thirteen
agree, and the factory values are exactly those measured in August in the test menu — two independent
sources.

| réglage | vivant | k | sauvegarde | usine |
|---|---|---|---|---|
| `ENERGY MAX 1P` | `0x120540` (16 bits) | 32 | `0x42`, `0x43` | `00D2` = 210 |
| `ENERGY MAX VS` | `0x120542` (16 bits) | 34 | `0x46`, `0x47` | `00D2` = 210 |
| `MATCH COUNT 1P` | `0x12054C` | 44 | `0x5A` | 2 |
| `MATCH COUNT VS` | `0x12054D` | 45 | `0x5B` | 3 |
| `DIFFICULTY` | `0x12054E` | 46 | `0x5E` | 2 `NORMAL` |
| `ADVERTISE SOUND` | `0x12054F` | 47 | `0x5F` | 1 `ON` |
| `SURVIVAL MODE` | `0x120550` | 48 | `0x62` | 0 `OFF` |
| `SUPER K.O.` | `0x120554` | 52 | `0x6A` | 1 `ON` |
| `ROUND TIME 1P` | `0x120555` | 53 | `0x6B` | 30 |
| `ROUND TIME VS` | `0x120556` | 54 | `0x6E` | 45 |
| `COUNTRY` | `0x120557` | 55 | `0x6F` | 0 `JAPAN` |
| `VS FINISH` | `0x120558` | 56 | `0x72` | 0 `OFF` |
| `BIKE LOGO` | `0x12055A` | 58 | `0x76` | 1 `ON` |

### The end-to-end check

Reading the table does not prove that writing works. Two runs, a control and a trial, the real backup laid
down byte by byte by `VF3_POKE` and five settings changed in the trial:

| setting | control | trial | expected |
|---|---|---|---|
| `MATCH COUNT 1P` | 02 | **05** | 05 |
| `ROUND TIME 1P` | 1E | **3C** | 3C |
| `ENERGY MAX 1P` | 00D2 | **0168** | 0168 |
| `DIFFICULTY` | 02 | **04** | 04 |
| `BIKE LOGO` | 01 | **00** | 00 |

Five out of five. **Writing the byte into the backup makes the game load the value**: `OPTIONS` will be
durable, by the same mechanism as Virtua Fighter 3's "Arcade settings" page, and the working copy need not be
touched.

A trap for next time: `VF3_POKE` reads its values in **decimal** (`%lu`), and `sscanf` **stops at the first
unreadable value** — a `D2` written in hexadecimal silently loses everything that follows. The first trial
concluded "the game ignores the backup" for that reason alone.

### What is left of the block

The block is 60 bytes and thirteen settings occupy only fifteen of them. `0x120544`, `0x120546`, `0x120548`
and `0x12054A` carry `0x0078`, `0x0078`, `0x0045`, `0x00B4` (120, 120, 69, 180) and are named by nothing:
other energies or other durations. We leave them alone.

### 2. What do we draw the page with?

**The question the image did not resolve.** `assets/menu-console/option_bg.png` is a **background** — the
darkened logo — without a single label. So the thirteen names and their values have to be **text**, and this
port has no text engine outside RmlUi, which is the Escape-key menu's.

Three ways were open: a RmlUi document of its own, the game's font through the tile generator, or the
Dreamcast's font to be got out of `TEX_DAT.AFS`.

**Settled on 2026-10-22, and by a file rather than by a judgement call**: Frédéric dropped `police.png`
**among the menu's images**, in `texture_remplacement_2d/fvipers2/`, and not among the tiles named by their
footprint. So it is a fourth way, and it is the right one: its own glyph sheet, drawn directly the way
`fv2_mode_list.h` already draws the list of modes. Neither RmlUi, nor the tile generator, nor the disc's
archive.

### What `police.png` carries, measured

| | |
|---|---|
| image | 1024 × 384, RGBA, **white glyphs on transparent** |
| grid | **16 columns × 6 rows**, cells of **64 × 64** |
| mapping | **ASCII `0x20` to `0x7F`**, in order, one cell per code |
| empty cells | exactly two: `0x20` (space) and `0x7F` — which confirms the mapping |
| position | the glyphs are **centred in their cell**, horizontally *and* vertically |
| capitals and digits | all exactly from `y = 16` to `y = 46`, the only exception being the `Q`'s tail (13 to 49) |
| ink widths | from 6 px (`'`) to 41 px (`W`), median 22 |

And the finding that counts, because it decides what can be written with this sheet: **it has no baseline**.
Every glyph is centred in its cell, vertically too. `O` occupies `y = 16..46`, `o` occupies `20..42`, the full
stop `29..33`, the comma `26..37`, the colon `21..41` — all with a midpoint of 31. A font puts those four on
one line; this one stacks them around a centre.

Three consequences, and they are clear-cut:

- proportional rendering has to go through **each cell's ink box**, not through the cell, since the glyphs are
  not flush left. Hence the table of 96 boxes in `src/host/fv2_texte.h`, measured by
  `tools/fv2/mesurer_police.py`;
- **vertically, there is nothing to compute**: the whole cell is drawn, and the glyphs keep the alignment the
  sheet already gives them. For a page in **capitals and digits** — which is what the thirteen settings and
  their values are — that is *exact*, since all 35 share the same box;
- the **lower case and the punctuation are wrong**, on the other hand, on an ordinary line of text: the `o`
  floats 4 px above the `O`, and the full stop reads as a middle dot — `SUPER K.O.` comes out as
  `SUPER K·O·`. That is not a flaw of the drawing, it is how the sheet is. Correcting it would mean
  **choosing** a baseline the sheet does not give, that is to say inventing: it is for Frédéric to say
  whether he wants one, and which.

Nothing in this port nor in the summer's work used this sheet: the port's only font is RmlUi's.

### 3. What should a change do? — **settled on 2026-10-22, and not as planned**

The plan said: "for Virtua Fighter 3, it restarts the game when the menu is closed. That is the behaviour to
take up, not to reinvent." **That was false**, and it is Frédéric who saw it while playing: the restart
blacks out the screen and leaves the window silent for several seconds while the board boots again. He first
took it for a crash, and he was right to take it for one — on a console options screen it makes no sense.

The right answer is to write **both copies**:

| | what it brings |
|---|---|
| the **backup** (`board.backup`, through the formula above) | the setting **lasts**: the game will read it again at the next start-up |
| the **working copy** (`0x00120520 + k`, directly in RAM) | the setting **takes effect at once**: that is the one, and the only one, the game reads while running |

Writing the backup alone forces a restart to be read; writing the working copy alone does not survive into
the next session. Both together, and there is nothing left to restart.

Measured: two `Right` on `ROUND TIME 1P` and the page left, with no restart — byte `0x6B` of the backup
written to disc goes from 40 to 50, and the page shows the new value because it reads the live copy.

**What remains to be checked**, and it takes playing: each setting only takes effect at the moment the game
reads it again. `ROUND TIME` and `MATCH COUNT` are presumably read at the start of a match, `ENERGY MAX` too;
but nothing says that none of the thirteen is locked in earlier. A setting that only moved after a restart
should be noted here rather than supposed.


## The order

1. ~~**Question 1**: the thirteen addresses in the backup.~~ **Closed on 2026-10-22**, recorded above,
   verified by a trial against a control.
2. ~~**Question 2**.~~ **Closed on 2026-10-22**: `police.png`, measured above.
3. The page, the thirteen lines, the ranges as the guard.
4. `verifier-vf3.cmd`, and a game played.

## What this plan does not do

- **It does not write the working copy** if the backup is reachable. A setting that does not survive a
  restart is not a setting.
- **It bounds nothing at random**: the ranges are §9's, measured.
- **It does not touch the Escape-key menu**, which keeps its page of port settings.


## What August's work had already learned — reread on 2026-10-22

Frédéric pointed out that "there was a sub-menu before that screen". The summer's work is in
`Documents/SEGA Model 3/Sauvegardes/2026-08-20_23h04/Docs/MenuConsole.md`, and it says a great deal more than
that.

### OPTIONS' shape in August

It was not a flat page of thirteen lines. `OPTIONS` was **a page of sixteen lines**: the thirteen
`GAME ASSIGNMENTS`, an `INITIALIZE`, and two entries leading elsewhere —

- **`SOUNDS`**: the sound effects test, the one for the twenty pieces of music, the two volumes and the choice
  of pack;
- **`STATS`**: the VS DIAGRAM, drawn from the backup's counters.

And the **Dreamcast has exactly that shape**: seven settings, then `SOUNDS`, `CONTROLLER`, `RESULT`, then
`INITIALIZE`. The three sources agree — the page carries settings *and* doors.

Our page has only the thirteen and `INITIALIZE`. What is missing is the doors, and they lead nowhere today:
the sound layer was undone (`docs/fv2-console-son.md`), and the VS DIAGRAM is not drawn here.

### The density, which the owner had measured before me

His list of outstanding things, word for word: "*`OPTIONS`' density: sixteen lines cannot be large. Two ways
out: put the thirteen `GAME ASSIGNMENTS` on their own page, or go to two columns.*" That is exactly the
problem met here, where the line pitch had to be tightened below the Dreamcast's because it holds seven and
we hold thirteen. **Putting the thirteen on their own page solves it from both sides at once**: the settings
page can breathe, and `OPTIONS` becomes a short list of doors again, as on the disc.

### Three rules of drawing, paid for in August

1. **The selection rule is anchored by its bottom.** `option_cursor.png` carries a vertical tab that rises
   *above* the line, so the visible line is at the bottom of the image. Placed by its top, it underlines the
   next row. **This port had exactly that flaw**, and I had "fixed" it by moving a constant until it fell
   right — which is not the same thing: at another size it would have come back. Corrected on 2026-10-22, the
   rule now hangs from the intended position of its line.
2. **A block is centred on the width it *can* occupy, never on the width it does occupy.** Otherwise the page
   slides under the eye as soon as a value gets longer — measured in August on `DIFFICULTY` going from `HARD`
   to `VERY HARD`. Hence its `SetValueRoom`, where each page declares its reserved width: nine characters for
   `OPTIONS` (`VERY HARD`), fourteen for `SOUNDS` (`BRAIN CLEANSER`). **This port does not do it**: each value
   is centred on its own width, so it recentres when it is changed. The Dreamcast, for its part, does centre
   its values — the tension is real and is not settled.
3. **A page that is too long looks for room before shrinking**: the band is widened (up to 0.20 at the top and
   0.96 at the bottom) before the text is reduced. Without that order, any increase of the font is cancelled
   by the tightening, "two effects that cancel exactly, and a correction that seems to do nothing".

### And one thing done better

His list also carried: "*an italic font: the Dreamcast reference is in italics, our atlas is not. A second
atlas would allow it.*" A second one was not needed: the slant is measured at 7.5° and applied by shearing at
draw time (`src/host/fv2_texte.h`).


## The tree, redone on August's — on 2026-10-22

Asked for by Frédéric: redo the tree of OPTIONS' sub-menus taking August 2026 as the example. It was reread
in `Src/OSD/SDL/Main.cpp` of the 20 August backup (`OptionsEntries` and `OptionLines`), not reconstructed
from memory.

```
OPTIONS
├─ the thirteen settings      name on the left, value centred
├─ DISPLAY                    -> a screen           (half a blank line before)
├─ SOUNDS                     -> a screen
├─ CONTROLLER                 -> a screen
├─ STATS                      -> a screen
└─ INITIALIZE                 an action, as an image  (1.2 blank line before)
```

The four doors are drawn **bold and centred**, as in August and for the reason he gave: they lead somewhere
rather than set something. The bold is the title's thickening without the condensing — the sheet has only one
weight, that is how it gains a second. Both blanks are his. `B` **goes up one screen** instead of aiming at
the list of modes in hard code: that is its return stack.

The Dreamcast has the same doors but for two names — `SOUNDS`, `CONTROLLER`, `RESULT`, then `INITIALIZE`.
`DISPLAY` and `STATS` are the fork's, `RESULT` is the disc's; we follow August, which is what was asked for.

**What is behind each door, honestly: nothing yet**, except `CONTROLLER`, which was already
a-title-and-nothing-else in August — "*THE CONTROLLER PAGE, EMPTY FOR NOW. Asked for on 2026-08-18: the title
and nothing else*". So for that one the tree is complete. The three others carried: `DISPLAY` `WIDESCREEN`,
`HD ART`, `FPS COUNTER`, `ENDING`; `SOUNDS` the sound effects test, the twenty pieces of music, the two
volumes and the choice of pack; `STATS` the VS DIAGRAM. Each one shows its title today and, quietly, what it
will hold.

### The density, announced in August and measured here

August's list said: "*sixteen lines cannot be large*". The page carries **eighteen**. Measured on a
1440 × 1080 capture:

| | the Dreamcast | this page |
|---|---|---|
| capital height | 0.0292 | **0.0176** — 60 % |
| line pitch | 0.0584 | **0.0361** — 62 % |

That is exactly what he had foreseen. **Settled the same day by Frédéric**: the first of his two ways out,
the thirteen settings on their own page.

### The tree as it stands

```
OPTIONS
├─ GAME ASSIGNMENTS  -> the thirteen settings, then INITIALIZE
├─ DISPLAY           -> a screen
├─ SOUNDS            -> a screen
├─ CONTROLLER        -> a screen : the bindings, with their indicator
└─ STATS             -> a screen
```

`OPTIONS` is five lines, `GAME ASSIGNMENTS` is fourteen. **`INITIALIZE` followed the settings**, and that is
a deliberate divergence from August and from the disc, which both keep it at the foot of `OPTIONS`: it
restores only those thirteen values, so on a page that no longer shows them it would be a button whose effect
cannot be seen. They could leave it there, the settings were there.

And **a short page does not stretch**: the pitch is the disc's as long as the lines allow, and the block is
centred in its band. Otherwise `OPTIONS`, with its five lines, would have had a pitch three times
`GAME ASSIGNMENTS`', and the two screens would not have looked like the same menu.

### What that gave, measured

| | capital | pitch | % of the disc |
|---|---|---|---|
| eighteen lines | 0.0176 | 0.0361 | 60 % |
| fourteen lines | **0.0231** | **0.0472** | **79 %** |
| the Dreamcast | 0.0292 | 0.0584 | 100 % |

21 % of difference remains, and it comes from the disc showing only **seven** settings where the cabinet has
thirteen. Reducing them would mean hiding some, which is not the same thing as arranging them.

## The pages behind the doors, ported — on 2026-10-22

Frédéric had to ask twice: the first delivery was the framework, with four doors that led nowhere. Here they
are filled, line for line after `Src/OSD/SDL/Main.cpp` of 20 August.

### `DISPLAY` — `DISPLAY_LINES`

| line | here |
|---|---|
| `WIDESCREEN` | **the image's shape**, which cycles through the port's five (`FIT`, `4:3`, `16:10`, `16:9`, `21:9`). August had only two, hence its `ON`/`OFF`; the line is the same, it does more |
| `HD ART` | `ON` or `NONE`. **Stepped over**: the 3D texture pack is read once at start-up and nothing knows how to release it |
| `FPS COUNTER` | **real**, the port's counter |
| `ENDING` | **stepped over**: the Dreamcast-style ending is the fork's, there is nothing like it here |

### `SOUNDS` — `SOUND_LINES`

| line | here |
|---|---|
| `SE` | **real since 2026-10-23**: the 256 indices of bank `0x2E`, played at every step. The only three that are named show by their name, the others by the game's SOUND TEST number (index + 353) |
| `SE VOLUME` | **real**, a gauge of twenty segments |
| `BGM` | **real since 2026-10-23**: the twenty pieces of music by name, played at every step, stopped on leaving the page. August showed it as `BGM` and routed it to `BGM TEST`, two `VOLUME` lines being indistinguishable |
| `BGM SET` | **stepped over** |
| `BGM VOLUME` | **real**, a gauge of twenty segments |
| `INITIALIZE` | **real**, puts both volumes back to full |

`SOUNDS`' four stepped-over lines all require **handing a note to the sound board**, which is to say the layer
undone on 2026-10-21 and waiting for `docs/fv2-console-son.md` to be approved. They are drawn and skipped,
never removed: "*a line with nothing to toggle is stepped over rather than removed*".

The gauges are `option_vol_up_on.png` and `option_vol_up_off.png`, twenty segments, **in his own colours**:
tinting them with the selected line's yellow said nothing, the lit segment being yellow already.

### `CONTROLLER` — the bindings

A title and nothing else in August, where it was asked for that way on the 18th. It now sets the controls:
**nine lines** — the four directions, punch, guard, kick, Start and `SELECT` — **over two columns**, `1P` and
`2P`. The selected line carries `GAME ASSIGNMENTS`' **pink rule**, the same drawing, placed by its bottom as
over there, over this grid's width.
Up and down walk the lines, left and right move from one player to the other, `A` opens a line and **the next
key or button becomes the binding**. Backspace **and Start** cancel, which makes them the only two controls
that cannot be bound here. Start also exits, outside a capture.

**Why Start is not bindable, and what that cost**: in the first version, only Backspace cancelled a capture.
On a gamepad there was therefore **no way out**: every button bound. The first hand to try it proved it in one
session —  went from  to , which is not a setting anyone asks for but a Start pressed to get out. A
capture that swallows everything shuts you in.

**And a ninth line that binds nothing**: `TEST`, one under each column, which opens that player's **mirror** —
the eight directions and the three buttons, large, lit as long as they are held, with their binding under
each. A diagonal lights on its own, the two straight ones staying dark, which makes a corner of the stick
indisputable. Start returns to the grid.

The split is the right one, and it is Frédéric who asked for it that way: **the grid says what is bound, the
mirror says that it arrives**. The mirror alone set nothing; attached to the editor, it earns its place.

Each line carries the game's corresponding icon, **always lit**: here it says which control is meant, not
whether it is held. It is the mirror, behind `TEST`, that answers that question — and it answers it better,
large.

**`SELECT` is this port's coin.** The cabinet has Start, Coin, Test and Service and no Select at all: the name
is the console pad's, and the lever is the one it falls on over there — the button a hand goes to in order to
put a credit in. It carries no icon, the sheet drawing only what a fighter does.

The icons come from `assets/menu-console/inputs.png`, 512 × 512, which was used nowhere. Its eleven cells were
**measured on the ink**: the arrows are not all the same width — 71 for the straight ones, 59 for the
diagonals, 70 for up and down — and forcing them into a single cell would fatten the diagonals. Each is drawn
at a common height. The unlit one is the same image at alpha 70.

### Two versions, and why the first was worth nothing

The first version was **a mirror only**: the eight directions and the three buttons lit, nothing to set. It
already had a controls flaw — Frédéric tried it on a gamepad and saw in under a minute that **testing left
and right changed player**; looking closer, **punch confirmed** and **kick exited**, kick being the pad's `B`.
Every control the page was meant to reflect, the menu was reading.

I fixed that by drawing both players together, so as to have nothing left to switch and to keep only Start.
Technically right, and **beside the point**: his next verdict was *"as it stands this page is of no use, you
can't set anything, just check that the keys work"*. A mirror is a diagnostic, not an options screen.

**And making an editor of it dissolves the controls problem instead of working round it.** A mirror has to
leave every control free — hence the dead end. An editor **wants** to swallow what is pressed, since that
press is the answer to its question. The very keys that got in the mirror's way are exactly what the capture
needs.

**Verified** in a hidden session: go down to `PUNCH`, `A`, then `Q` — and `p1_key_punch` goes from `X` to `Q`
in `settings.ini`, the line showing `Q / A`. The setting survives a restart. `VF3_LIVE=n,m` lights each
player's bits, since a pushed event does not change the keyboard state SDL reports.

### `STATS` — the VS DIAGRAM

The game's table, `vs_global.png`, with the figures laid in its cells: fourteen characters, a `TOTAL` in
yellow, and eight columns. The grid is measured on the image by its white rules — `x` from 327 to 2219 in
eight equal parts, `y` from 99 to 1391 — which falls exactly on August's measurement.

The counters come from the backup, by August's §12 layout: `block = base + n × 0x1A8`, `KO` at +0x000,
`JUDGE` at +0x018, then `FIGHT`, `WIN` and `LOSE` per opponent every 8 bytes — **sixteen opponents and not
fourteen**, because the row is 0x80 and Del Sol is opponent 14. Two points of that description were corrected
by measurement, below: **`DRAW` is filed**, at +0x020, and the encoding is **contiguous** and not spaced.

**Only one thing had to be found again**: his offsets are in *his* NVRAM dump, not in this port's `backup`.
The footprint he prints links them — at his `0x188` he reads `210 210 120 120 69 180`, which is the
`GAME ASSIGNMENTS` block, and the same run is at `0x42` here. So **ours is his minus `0x146`**, which puts
the first block at `0x3006` — a few bytes into the backup's third page, which is precisely where the third
block begins. Two independent checks that agree.

**Not confirmed on live figures**: every counter is zero in a backup where no match has ever been played. One
will have to be played to say.

`SKO 1` and `SKO 2` stay **empty**, and that is intended: August's list of outstanding points gives them as
"*shown as zero without anyone knowing where they live — the only figure in the table that cannot be
justified*". An empty cell says so; a zero would pretend.


## `SOUNDS` filled — on 2026-10-23

The page's two tests work. What was blocking them was not the set-aside table of outputs — they do not touch
the game's music — but the fact that a note handed to the board **under a frozen screen** could not be heard.
That has been done since the day before, so they are feasible.

| line | what it does |
|---|---|
| `SE` | walks the 256 indices of bank `0x2E` and plays at every step |
| `BGM` | walks the twenty pieces of music by name and plays at every step |

The names and the numbering are August's measured tables: on bank `0x10` the SOUND TEST number is the index
plus ten, on `0x2E` the index plus 353. **Verified here twice** in the MIDI log: the attract sends
`A0 10 07`, which is no. 17 `ATTRACT`, and the replay `A0 10 11`, which is no. 27 `DEMO`.

And one decision of ours: **a test piece of music is stopped on leaving the page**, by the game's silence
command. Without that, a test followed the player into the game, and that is no longer a test.

`BGM SET` stays stepped over: the music packs are the fork's, and his own notes say the replacement was wired
in there without ever being heard.

`DISPLAY` is left with its four lines from August, two of them live. Adding more (render scale, choice of
rasteriser, smooth motion) would be inventing a page that is not his; the two stepped-over ones are so for a
reason stated at their line.


## Why `ROUND TIME` did not take, and what else had to be written — 2026-10-23

Frédéric set `ROUND TIME 1P` to 60 and the round lasted 30. Measured, and the answer is not the one
`docs/fv2-console-options.md` had been supposing since the day before.

**The match does not read the setting.** It reads an **already converted** word: `0x0000C2EC` chooses between
`0x00581418` (one player) and `0x0058141C` (two) according to `0x0058145A`, and puts the result into the clock
`0x00581404`.

**And the conversion happens only once, at boot.** `VF3_RAMLOG` shows `0x00581418` already equal to `0x780`
at **frame 223**, in mode 0, and it does not move again for the whole session. The computation is at
`0x00038DB8`: `rlwinm ...,6`, that is **the value times 64** — the game counts 64 frames a second for that
clock, not 60. 30 × 64 = 1920, and 60 × 64 = 3840.

So writing the setting alone could do **nothing** after boot, and that is exactly what he saw. The version
that restarted the game hid it without solving it.

### Which settings are cached that way

Not guessed: two boots were dumped, one with the settings pushed and the other not, and **all of
`0x00580000`–`0x00582000` compared**. Two places only move:

| setting | its cache | the form |
|---|---|---|
| `ROUND TIME 1P` | `0x00581418` | value × 64 |
| `ROUND TIME VS` | `0x0058141C` | value × 64 |

The **energies did not move**: they are read live and need nothing.

### And an error in that same measurement, found the same evening

That measurement also gave `ADVERTISE SOUND` as cached at `0x0058142C`, "kept as the setting plus one". **It
is not.** `0x0058142C` is the **number of rounds in a match**, and the first reading was a coincidence:
`ADVERTISE SOUND` was 1, so "plus one" made 2, and `MATCH COUNT 1P` was 2 as well.

Measured properly, one setting at a time:

| pushed | what moves in `0x00580000`–`0x00582000` |
|---|---|
| `MATCH COUNT 1P` to 5 | `0x0058142F` alone — the low byte of the word at `0x0058142C`, which then reads 5 |
| `MATCH COUNT VS` to 5 | nothing at boot |
| `ADVERTISE SOUND` to 0 | nothing at `0x0058142C`; a dozen bytes in the sound tables, at `0x00580E8D` and `0x0058135D`–`6F` |

And that word is not decorative: `FUN_000136B0`, which resolves a fatal blow, compares **the rounds already
won plus what the blow is worth** with `0x0058142C` to decide whether the match is over. Writing a wrong
number there therefore shortens or lengthens every match — which is what the page had been doing since the
morning, as soon as `ADVERTISE SOUND` was touched.

**The line was removed, and nothing replaces it**: the game **recooks** the number of rounds at the start of
each match. Pushed live during the attract, it reads 5 in the fight that follows. `ADVERTISE SOUND`, for its
part, is spread over sound tables at boot and has no word to rewrite; it needs a restart, which this page does
not do.

### The correction

`Model3::fv2_set_option` now writes **three** things: the backup, the working copy, and the cache when there
is one (`fv2_recook_option`).

**Verified**, in two halves that meet.

*That the cache is read again*: by setting `0x00581418` to `0xF00` from frame 1500 on — so well after boot —
the next round starts at **3840 frames, sixty seconds**, against 1920 for the control. So the cache is read
again at every match, and writing it is enough.

*That the page does write that word*: `VF3_OPTION_LOG=1` makes `fv2_recook_option` say what it sets and
where. The route is `tools/scripts/fv2-console-options-timer.txt` — Start at frame 1200 falls on the title
screen and opens the list; from there the game is frozen, so the rest is driven by the clock (`VF3_KEYS_S`),
the only thing that reaches a surface drawn over a stopped game:

```
VF3_HIDDEN=1 VF3_OPTION_LOG=1 VF3_SCRIPT=tools/scripts/fv2-console-options-timer.txt
VF3_KEYS_S=Down@30,Down@31,Down@32,Down@33,Down@34,Down@35,Return@37,Return@39,Down@41,Down@42,Left@44,Left@45,Right@47,Right@48
```

Six *downs* lead to OPTIONS, two *confirms* open the page and then GAME ASSIGNMENTS, two *downs* fall on
`ROUND TIME 1P`, and the steps give:

```
option 120555 = 55, cooked 00581418 = 3520 (55 s)
option 120555 = 50, cooked 00581418 = 3200 (50 s)
option 120555 = 55, cooked 00581418 = 3520 (55 s)
option 120555 = 60, cooked 00581418 = 3840 (60 s)
```

The two halves overlap on `3840`: the page sets the word the match reads again.

**Worth knowing for the next trial**: if the setting is already at its maximum, a step to the right changes
nothing and writes nothing — that is what happened on the first trial, where the backup already carried 60.
Bring the value down before bringing it back up, or a silence gets read as a failure.

### The verification round

The two halves overlap, but neither of them is a match. That one was played, in a hidden session, from the
first press to the clock: `tools/scripts/fv2-console-options-round.txt` opens the list at frame 1200,
`VF3_KEYS_S` leads to `ROUND TIME 1P` and brings it **down from 60 to 20**, leaves the page with two
backspaces, the press at frame 1260 reopens the list, *confirm* takes ARCADE, and the punch at frame 1500
picks the fighter. `VF3_TIMER_LOG=1` then follows `0x00581404`.

The backup was copied into a separate data folder, so that the owner's would not move: it carried 60, so the
boot had cooked `3840`. The log gives:

```
option 120555 = 20, cooked 00581418 = 1280 (20 s)
...
timer f 1701: 1280 left (20.0 s), the round starts from 20 (1P) / 45 (2P)
timer f 3401: 1280 left (20.0 s), the round starts from 20 (1P) / 45 (2P)
timer f 4842: 0 left (0.0 s)
```

**Two rounds started from 1280**, the twenty seconds the page set, and not the boot's 3840. The first ended
before time was up (reset to 1280 at frame 3401), the second expired at frame 4842.

Two things read out in passing:

- **`0x00581404` counts frames, not seconds.** 1280 for twenty seconds: sixty-four a second, the same count
  as the conversion. The `5, 4, 3, 2, 1` noted in `fv2-flux-de-match.md` were its last five frames, not
  seconds.
- **`ROUND TIME VS` stayed at 45**, the backup's value, which nothing touched. The write does not spill over
  onto the neighbouring setting.

## VS DIAGRAM: the real figures

The table was built and already read the backup, but on nothing: in a backup where no match has been played
to the end, every counter is zero, and a table of zeros does not say whether the right place is being read.
Four matches played in hidden sessions, each against the backup the previous one had written, filled it — and
corrected the description along the way.

### An ARCADE match writes none of this

The previous section's verification round moved **fourteen bytes**, all elsewhere. The table is a record of
**versus** play: a versus match is what is needed. The routes are in `tools/scripts/`:
`fv2-console-vs-diagram.txt` (nobody moves), `fv2-console-vs-decide.txt` (player 1 hits without stopping),
`fv2-console-vs-juge.txt` (he hits for eight seconds then lets the clock run out) and
`fv2-console-vs-sko.txt` (punch and kick alternating). All go through `ROUND TIME VS` at its minimum, so that
the rounds expire quickly.

### What each match moved

| match | what happened | what moved in the table |
|---|---|---|
| 1 | four rounds, nobody moves → four draws | `FIGHT` 0 → 4 and `0x020` 0 → 4, **in both blocks** |
| 2 | three rounds settled by KO | `FIGHT` 4 → 7, `LOSE` 0 → 3 on one side, `WIN` 0 → 3 on the other, `KO` 0 → 3; **`0x020` stays at 4** |
| 3 | hits then the clock, at unequal health | `FIGHT` 7 → 10, one `WIN` and one `LOSE`, **`0x018` 0 → 1**, `0x020` 4 → 6 |
| 4 | punch and kick alternating | `FIGHT` 10 → 13, `LOSE`/`WIN` +3, `KO` 3 → 6; **nothing at `0x008` or `0x010`** |

Five bytes move per match, never one more: block 10 at `0x4096` and block 11 at `0x423E`, each carrying the
other as its opponent, and each pair symmetrical — CHARLIE's `LOSE` against EMI facing EMI's `WIN` against
CHARLIE. The base, the step and the per-opponent cell all fall right **at the same time**, which no single
counter could have shown.

### Three things learned

**`0x020` is DRAW**, which August's note gave as being nowhere. Match 1 put it at 4 with four draws; match 2
left it at 4 while adding three settled fights — a total of fights would have followed `FIGHT` up to 7. And
it agrees with `FIGHT - WIN - LOSE` on every line measured: 10 − 0 − 4 = 6, 10 − 4 − 0 = 6. It is the stored
figure that is read now, being the game's own count rather than a subtraction among three of its others.

**The encoding is contiguous**, not spaced like the settings': two big-endian bytes at `+0` and `+1` of a
cell of eight, so six bytes out of eight unused. A count of 4 moved only the byte at `+1`.

**SKO 1 and SKO 2, found through a Super K.O. played by hand.** The scalars go in eights: `0x000` KO,
`0x008`, `0x010`, `0x018` JUDGE, `0x020` DRAW. Three of the five were measured, which fixed the step, and the
two holes could hardly be anything but the SKO pair. It remained to be proved, and **no machine managed it**:
not the script, at six Armor Flash cadences, not thirty thousand frames of random play, not a whole fight
against the processor — both armour icons stay green from start to finish, measured over thirteen frames.

The move takes a hand: the downed player's armour (Armor Flash — forward, back, forward, back with the three
buttons) and the opponent nearly dead, then forward + punch + kick + guard. **Frédéric played it.** One versus
match, and the comparison is beyond appeal:

```
0x010 goes from 0 to 1 in the winner's block
KO, JUDGE and DRAW do not move at all
```

A win that counts as none of the three ordinary endings and that falls into one of the two holes **is** the
Super K.O. So both cells are filled, and the table has none empty left.

**And what separates the two columns was read in the code** (`docs/fv2-flux-de-match.md`, section 8), because
Frédéric asked for it to be checked there rather than on screen. It is not two moves: `FUN_000701C8`, the
routine that files a match away, counts **the same ending** in both cases — `0x00581452` is 3 either way —
and what decides the column is **the `SUPER K.O.` setting itself**:

```
lbz r0, 0x554(r9)      the setting, at 0x00120554
beq -> +0x04           OFF : the first column    (backup 0x008, SKO 1 here)
    -> +0x08           ON  : the second          (backup 0x010, SKO 2 here)
```

So an operator who turns `SUPER K.O.` off does not stop the ending from happening: he makes it count
somewhere else. The two cells separate **the Super K.O.s won with the setting off from those won with it on**.
Frédéric's, played with the setting on, fell into the second — and the order drawn here, off first, is the
code's.

**The counters are 32-bit integers**, on a live table at `0x0011F7E0` with a step of `0xD4` — that is exactly
half the backup's `0x1A8`, the latter being only that table spaced out at two useful bytes in four. In the
backup each counter takes eight bytes and **the value reads in the first two**, measured:
`00 01 00 00 00 00 00 00` for a count of 1. So the page's 16-bit read is right up to 65535.

### What the page shows

Photographed on the four matches' backup (F12, which now goes through the console's surfaces):

```
CHARLIE   FIGHT 13   WIN 0   LOSE 7   DRAW 6   KO 0   JUDGE 0
EMI       FIGHT 13   WIN 7   LOSE 0   DRAW 6   KO 6   JUDGE 1
TOTAL           26         7        7       12      6         1
```

The rows fall on the right characters, and the total is the sum of the columns — each fight counts twice
there, once per participant, since both blocks record it.

## `DISPLAY`: `HD ART` and `ENDING`

This page's two stepped-over lines are wired in. They were not wired in the same way: one only needed a
lever, the other took a measurement.

### `HD ART`: a real switch

The line only reported, because the 3D texture pack was read once at start-up and nothing knew how to release
it. No new lever was needed: **`load()` empties what it holds before reading**, so loading from nowhere is the
way to put a pack down, and the GPU rendering already checks the number of pages instead of supposing it —
its own comment foresaw a pack arriving later.

- `NONE` when there is nothing to load, and the line is then stepped over like any line with nothing to set.
- `ON` / `OFF` otherwise, and the setting follows into `settings.ini`.

**Measured** with a pack of forty PNGs taken from the dump: switched on, the page says `ON` and the log
reports *40 textures loaded, 0 left out, in 1 page*. Switched off, *the 3D texture pack put down*, the page
closed, the game rendered seven hundred more frames without an error — that is where the GPU's buffer is
rebuilt, so that is where it had to break if it was going to break.

### `ENDING`: the game's ending, which can finally be watched

August made a switch of it for his own artwork, none of which exists here. But **the game has an ending** —
the credits, mode 12 — that nobody had ever seen run: getting there means finishing the ladder, and the notes
file it among the states never reached. So the line is not its switch but a way to watch it: *confirm* asks
the game for it and the page clears.

It ends by itself: the credits run about a hundred seconds, then the game goes back through its attract loop
and returns to its title screen. There is nothing to put back.

**Only half of it comes out, and the line is kept anyway**, because the half that comes out is the credits
themselves.

The ending **alternates**: about 225 frames of cards over the viper, which draw exactly as they should, and
about 225 of a 3D passage — 279 models over 16 views, 383 later — which draws as a **bare white floor with a
silhouette the size of a thumb**, under 2D banners left over from the demonstration ("Old Armstone Town",
"v.s. GRACE"). None of the scenery is there.

### A boundary that was not one

The mistake has to be told, because it is instructive and it held for half a day.

By pushing `MODE_REQUEST` one frame at a time and photographing **always at frame 1900**, the credits looked
broken when asked for from 1201 to 1210, whole from 1212 on, and partial at 1211 — a clean boundary eighteen
frames after `FV2_TITLE_DSP`, which could be explained by a title screen that has not finished settling. A
wait of forty frames was laid on top of that.

**It was an artefact of the sampling.** Since the ending alternates every 225 frames, moving the request
moves *the phase frame 1900 falls into*: at 1201 it fell on a 3D passage, at 1250 on a card. Photographed
**at the same offset from `ENDING_INT`** — +300 and +650 — a request at 1201 and one at 1250 give the same
two images, down to the same byte sizes. **The moment of the request changes nothing at all.** The wait was
removed.

The lesson, for next time: compare images taken at the same instant of a periodic phenomenon, and not at the
same absolute frame number. The "partial 1211" that seemed so telling was only a phase transition caught in
flight.

### Why there is no scenery: the ending is made of replays

Sega left its states' names in the clear, and it is they that answer. The ending's seven sub-states
(`docs/fv2-etats.md`, which also says how to read them):

```
ending_trans_init   ending_trans_wait   ending_replay_init
ending_replay_ctrl  ending_replay_next  ending_final_mask1  ending_final_mask2
```

**Three of the seven speak only of replays.** The credits are not a film: they are the credit roll
alternating with **replays of the fights the player has just won**. The banners I had taken for leftovers of
the demonstration — "Old Armstone Town", "v.s. GRACE" — are the replay's own overlays, announcing its place
and its opponent, exactly as in the attract.

So, asked for from the title screen, the ending has **no match to replay**. The floor stays bare because
there is no scenery to load: there is no fight.

### The three images that prove it

The same frame of the ending, at the same offset from `ENDING_INT`, asked for in three ways:

| how it is entered | what comes out | weight |
|---|---|---|
| `MODE_REQUEST = 12` from the title screen | a bare white floor, a silhouette the size of a thumb | 16 KB |
| `MODE_REQUEST = 12` **from a fight** | the fight's **two fighters**, textured, with their shadows — still on a bare floor | 28 KB |
| the **game's own branch** | the whole replay: *Cross River Damsite*, the cage, the two fighters, "v.s. PICKY" | 324 KB |

The second row is the hinge: asking for the mode from a fight makes the fighters appear, because they are
loaded — and not the scenery, because the replay was not prepared.

The third is done without forcing anything in the rendering. `fv2_game_NEXT_PROG_SEL` branches to the ending
when `ADV_INFO_STATUS` carries bit 3, "the ladder is finished" — a bit the summer's notes gave as *read in
the code, never seen set*. It is enough to set it: the low byte of `0x005813F0` is `00` just before the
branch, so nothing is overwritten. The game then takes its own route, with everything it does besides the
mode — `FUN_0000DD70`, seven tasks stopped, the fog cleared, the screen counter at `0x140` — and the ending
comes out whole.

```
VF3_POKE=5813f0=8 VF3_POKE_FRAMES=7170-7178
vf3run ... --script <a fight's route> --modes
   7178  ENDING_INT   19      instead of CONTINUE_INT
```

### Asked for from the title screen, it can only do the credits

It shows **the cards**, which are the ending's own content, and the game hands control back by itself. The
replays it cannot: there is nothing to replay when one arrives from the title screen, and a replay cannot be
invented.

**The game's real ending is within reach, and it is there now.**

### The lever: where to set bit 3

`fv2_game_NEXT_PROG_SEL` is the branch point of **every round**, not only of every match. That is the whole
difficulty: setting the bit too early makes an intermediate round take the branch, and it has recorded
nothing to replay yet. Measured on a fight whose real branch point falls at frame 7178:

| set at | what the game does |
|---|---|
| 7178 | **too late** — the branch has already read the bit, the game goes to `CONTINUE` |
| 7177, 7176 | the ending, whole |
| 6922, once only | the ending, whole — the bit **holds** for 256 frames, nothing needs to maintain it |
| 3000, mid-fight | the ending at frame 4740, **at the end of the first round**, and **bare** |
| 1250, on the title screen, before the match even | the same: bare, at 4740 |

The last two rows are the lesson. The bit survives everything — the selection screen, the start of the match
— but it is consumed by the first `NEXT_PROG_SEL` that comes along, and a round's, with no finished match, has
no replay.

**The game says itself when the match is over**: `0x40000000` in the same word, the bit its own question "one
more round?" is going to read. Hence the rule, and it is simple:

> set bit 3 at the first frame where `ADV_INFO_STATUS` already carries `0x40000000`.

One write, never held, like all of this port's requests. The game rewrites the whole word on every frame from
`0x000122C4`, preserving its value, so a write laid down before a frame counts for that frame's reads.

### What the `ENDING` line does now

Two things, which left and right choose between:

| value | what *confirm* does |
|---|---|
| `ROLL NOW` | the credits at once, over the title screen. The credits are whole, the replay passages are bare — there is no match to replay |
| `AFTER A MATCH` | **arms**. At the first match that ends, the game takes its own branch and gives its whole ending, replays on their scenery |

Armed, the line says so (`>`) and the page stays open: there is a game to go and play. The arming holds until
a match ends.

### The verification run

Played in the host, in a hidden session, end to end: the list, `OPTIONS`, `DISPLAY`, `ENDING` set to
`AFTER A MATCH`, *confirm*, out of the page, then `ARCADE` and a match. The log:

```
the console's list: OPTIONS chosen
the console's list: ARCADE chosen
the console ENDING: the match is over -- the game's own ending asked for     frame 6258
frame 6835 mode 12 sub 19     ENDING_INT, through the game's branch
frame 6847 mode 13 sub 19     ENDING_DSP
frame 7073 : 336 models       the replay passage, furnished
```

**336 models** in the 3D passage, where the bare request gave only the leftovers.

### And the scenery is indeed there — for the match that was played

Photographed in the host, to the frame, the ending shows **Cross River Damsite**: the dam, the water, the
cage, the two fighters mid-exchange, "v.s. EMI". It is the replay of the match just played, on its scenery,
in widescreen.

**But the ending replays the whole ladder, and the ladder has only one rung.** The passages follow each
other:

| passage | models | what is seen |
|---|---|---|
| the first | 334 to 338 | the match played, scenery included |
| the next | 279 | the bare floor, "Old Armstone Town", "v.s. GRACE" |

The second is not a flaw of the port: it is an opponent the player never faced, so a replay that does not
exist. A real arcade run would fill eight of them.

**And that is what explains my long misreading.** Four photographs in a row fell on the second passage or on
a card, and I concluded from them that the host was not loading the scenery while `vf3run` was — two engines,
two results, a fine false trail. There was no difference of engine at all: `vf3run`'s image was the first
passage, mine the second. **The same lesson as in the previous section, in the same place: on a phenomenon
that alternates, an image taken at random proves nothing.**

### The verification run

Played in the host, in a hidden session, from the first press to the credits. The list opens at frame 1200,
the page leads to `DISPLAY` then to `ENDING`, *confirm* asks for it, and the page clears. The game resumes at
frame 1200, and — this run having been played while the forty-frame wait was still in place, since removed:

```
frame 1241 mode 12 sub 5     ENDING_INT, forty-seven frames after FV2_TITLE_DSP
frame 1253 mode 13 sub 5     ENDING_DSP
```

The photographs show the cards whole and advancing — *Chief Programmer HIDEYA SHIBAZAKI* then, twenty seconds
later, *Character Designers KENTAROW NISHIMURA...* — over the viper, in widescreen. **Both fell on cards**,
which is precisely the trap described above: they say nothing about the 3D passages, which stay on the bare
floor.

And it hands control back by itself, which was the condition for having nothing to put back:

```
frame 7167 mode 18 sub 5     OVER_INT : the credits are finished
frame 7360 mode  3 sub 1     the attract loop resumes
frame 7558 mode  3 sub 5     the title screen
frame 7777 mode  3 sub 10    the replay, as if nothing had happened
```

So the game goes through its GAME OVER before coming back to the attract. Six thousand three hundred frames
between the request and the return, a little under two minutes.
