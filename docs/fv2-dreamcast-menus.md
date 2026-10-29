# Rereading our menus against the Dreamcast's — the plan

Written on 2026-10-22, before having read anything seriously. Nothing in this document is a conclusion:
these are the questions to settle, in what order, and by which measurement each one is checked.

Fighting Vipers 2's Dreamcast version (2001, Sega / CRI) **is** the console menu this port is trying to
remake. So far it has been used as a **source of images**. It is above all a source of **code**: its
screens, their sequence, what each line triggers, and what it does with the arcade game it carries.


## Why this plan exists

Two reasons, and the second is recent.

1. **The rule laid down on 2026-10-22**: for any "why does the game do that" question, read what is already
   decompiled first, measure afterwards. It was paid for with a whole evening spent recovering by RAM
   differencing what `docs/fv2-match-flow.md` already named.
2. **A first probe into the Dreamcast binary gave a false conclusion**, on 2026-10-22: I read there "the
   selected line is drawn with a different pattern", which was a misreading on two counts — the `jsr`'s
   delay slot (see below) and the identity of rows 34-44, which are the **font** and not labels. A binary
   probed with no plan answers whatever one wants to hear.


## What is settled, and will not be remeasured

Everything comes from `Documents\fv2-arcade-notes\README.md`, section "Les ressources 2D de la Dreamcast".

| | |
|---|---|
| the binary | `Documents\fv2-arcade-notes\dreamcast\1ST_READ.BIN`, 1.3 MB, SH-4, base `0x8C010000` |
| setting the resource | `charge_ressource(record)` — `0x8C02A23A` |
| drawing | `dessine(row, x, y)` — `0x8C029598`, row in R4, x in R5, y in R6 |
| the nuance that governs everything | `dessine` does not receive the resource: it works on the one the last `charge_ressource` set. **Every reading follows the order of execution, not the call graph.** |
| the façade's patterns | TEX entry 16, list at `0x8C0E5808`, 96 records of 10 bytes: `desc, x, y, width, height` |
| the menu's labels | **rows 10 to 18**, descriptor 18 |
| the title screen | rows 3 to 8, a 3×2 mosaic of 256×256, set by `0x8C019CBC` |
| the tools | `desas_sh4.py` (Ghidra disassembles only 32.4% of this binary), `tables_sh4.py`, `retrouver.py`, and Flycast driven by GDB (`FV2 - Flycast avec GDB.cmd`) |

**The SH-4's trap, paid for that same day**: `jsr` has a **delay slot**. The instruction that follows the
`jsr` in the listing executes **before** the jump. So the `mov #N,r4` carrying the row is written *after*
the call and belongs *to* that call. Reading the listing top to bottom without that shifts every call's
arguments by one.

And the reading, read correctly, of the first sequence probed (`0x8C02C388`):

```
row 10  (ARCADE, 144x32)  at (99, 217)
row 11  (V.S.,    64x32)  at (355, 217)
```

Two labels **side by side on the same line**: that is not the vertical menu, it is something else — a
versus screen, or a banner. Which is already a useful fact: **the same labels serve several screens.**


## The divergence to settle first

The Dreamcast's labels, read in its pattern list:

```
row 10  ARCADE            144x32
row 11  V.S.               64x32
row 12  INTERNET          160x32
row 13  RANDOM NETWORK    176x32
row 14  SURVIVAL          176x32
row 15  TRAINING VS       152x32
row 16                    176x32
row 17                    104x48
row 18                    256x32
```

Our `title_list.png` has **seven bands**: ARCADE, V.S., RANDOM, SURVIVAL, NETWORK, TRAINING, OPTIONS. The
Dreamcast has **nine patterns**, and composite lines (`RANDOM NETWORK`, `TRAINING VS`) that do not exist in
ours. Two readings are possible, and only one is true:

- our image comes from **another** version or another screen than the one those rows serve;
- or rows 10-18 are not all from the same menu, and the vertical list uses only part of them.

**Part of the question fell on 2026-10-22**, and it narrows the divergence: `NETWORK`, `TRAINING` and
`OPTIONS` **do not exist on the cabinet**. The 24 modes Sega names contain none of them, and the arcade
notes write it: "there is no Training mode". So those lines are the Dreamcast's, and they cannot be "wired
up" — there is nothing to wire. Either this port **builds** them itself (a training area out of the `TEST`
set and the game's levers, an options page over the `GAME ASSIGNMENTS`), or they come off the list.

So what remains of the divergence is sharper: **our image carries seven lines of which three the game
underneath cannot do**, and the Dreamcast has nine, composed differently. The question is no longer "which
lever for those lines" but "what should the list of a console menu laid over a cabinet contain".


## The questions, in order

### 1. What is the Dreamcast menus' state machine?

On the arcade side we have the machine Sega names — `MODE_MAIN` / `MODE_SUB` and the 24 modes
(`docs/fv2-states.md`). The question is whether the Dreamcast port has one of its own, on top, or whether it
reuses the game's.

**The measurement that settles it**: set a GDB breakpoint on `dessine` in Flycast, navigate the menu by
hand, and record who calls, with which row, and from which stack. The distinct callers are the screens.

### 2. How its screens follow one another

What Frédéric asked for explicitly, and what our menu lacks most: **the transitions**. Title → mode list →
selection → fight → back; and the return paths (the B button), which are precisely what this port has had
the most trouble laying down.

**Measurement**: the same Flycast session, noting for each transition the order of `charge_ressource` /
`dessine`, and what the embedded arcade game is doing at the same moment (its mode, its sub-state).

### 3. Who draws the vertical list, and how the selected line is marked

A return to the question I misread. The criterion: a routine that draws **several rows from the 10-18 range
at increasing `y` and a constant `x`**. That is what a vertical list is — not two rows side by side.

**Measurement**: the same sweep as the first probe, but respecting the delay slot, and confirmed under GDB.

And the question that depends on it, the one that has occupied us for two days: **how is the selected line
marked?** A second pattern, a colour, a blink, a frame? Our port makes the line blink (alpha 1 → 0.5 over
45 frames), which comes from the owner's Supermodel menu, not from the Dreamcast.

### 3bis. The Dreamcast's OPTIONS screen: **recorded on 2026-10-22**

Frédéric supplied a Flycast capture of that screen. It is measured below, not commented on by eye. The
game's image extracted from the window: 1878 × 1405, ratio 1.3367 — 4:3. Every fraction is relative to that
image.

#### The background: **no veil**

That was the question asked. The medians of four areas with no interface, compared with `option_bg.png`
brought to the same size:

| area | Dreamcast capture | `option_bg.png` as it is | ratio |
|---|---|---|---|
| top left | (39, 38, 37) | (35, 33, 32) | 1.11 — 1.16 |
| left edge | (41, 39, 38) | (36, 34, 33) | 1.15 |
| bottom left | (44, 41, 40) | (38, 36, 35) | 1.14 — 1.16 |
| top right | (49, 47, 46) | (39, 37, 36) | 1.26 — 1.28 |

The disc's background **is** `option_bg.png`, up to the emulator's compression and gamma. It is not
darkened at all. Not to be confused with the mode list, where the measured halving applies to the game's
live image: here the background is a prepared image, already dark by construction.

#### The two fonts, and there are two

Measured by shearing each line and keeping the angle that makes the stems most upright:

| | slant |
|---|---|
| `OPTIONS`, the title | **0.00°** |
| `SOUNDS`, `CONTROLLER` | **0.00°** |
| `MATCH COUNT`, `ROUND TIME`, `SURVIVAL TIME`, `ENERGY MAX -1P-`, `ENERGY MAX -2P-` | **−7.0° to −8.25°**, median ≈ 7.5° |

So: a narrow bold **upright** for the title and the three sub-screens, and an **oblique** of ~7.5° for the
seven setting lines.

And the weight, measured as the median of ink runs along a row at mid-height, relative to the capital
height:

| | weight |
|---|---|
| `police.png` (the `I`, which is a pure stem: 6 px out of 31) | **0.194** |
| the disc's setting lines (8 px out of 41) | **0.195** |
| the disc's title (16 px out of 63) | **0.254** |

`police.png` and the disc's lines agree to the thousandth. **It is the same font**, without the slant —
which the shapes already suggested, and which the weight confirms.

The title, for its part, is a narrow bold: the width of the word `OPTIONS` = **4.143** capitals, when
`police.png`'s is **6.000**. It is approached by condensing and then thickening **horizontally** — not with
a square dilation, tried first and rejected because it rounds the corners and closes the counters, when the
disc's are crisp and open; a narrow bold carries its weight on the vertical stems in any case. Horizontal
thickening does not touch the height, hence a system with two unknowns:

```
6.000*cap*c + s = 4.143*cap      (the width)
0.194*cap*c + s = 0.254*cap      (the weight)
```

that is `c = 0.6698` and `s = 0.1241*cap`. Rendered that way, the port's title measures 4.129 capitals wide
for a weight of 0.258, against 4.143 and 0.254 on the disc. `tools/fv2/options_mockup.py` does it.

#### The layout

| | |
|---|---|
| the `OPTIONS` title | ink from `y = 0.0996` to `0.1438`, centred |
| the seven settings | capital height **0.0292**, pitch **0.0584**, first line at `y = 0.2128` |
| labels | **left-aligned on `x = 0.3136`** |
| values | **centred on `x ≈ 0.665`** — and not right-aligned |
| `SOUNDS` / `CONTROLLER` / `RESULT` | taller capitals (0.0377), the same pitch of 0.0584, from `y = 0.644` |
| `INITIALIZE` | `y = 0.848..0.873`, the painted image |
| the footer | two lines, `y = 0.902` and `y = 0.940` |

#### How the selected line is marked

**By colour, and by the rule.** The selected line's ink is `(252, 252, 10)` — clear yellow — when the other
six are `(247, 246, 246)`, white. And the pink rule `(254, 170, 255)` runs under it, from `x = 0.2913` to
`x = 0.7295`: it barely overruns the labels and the values, **it does not cross the screen**. That is
exactly `option_cursor.png`, tab included.

Nothing blinks. `fv2_mode_list.h`'s blink comes from the owner's Supermodel menu, and this screen does not
do it.

#### And these are not the cabinet's thirteen settings

The point that matters for `docs/fv2-console-options.md`. The disc's screen shows **seven** settings —
`DIFFICULTY`, `MATCH COUNT`, `ROUND TIME`, `SURVIVAL TIME`, `ENERGY MAX -1P-`, `ENERGY MAX -2P-`,
`SUPER K.O.` — then three sub-screens (`SOUNDS`, `CONTROLLER`, `RESULT`), `INITIALIZE`, and a help panel at
the bottom right explaining the current entry. It is not the `GAME ASSIGNMENTS` list: the cabinet separates
`MATCH COUNT 1P` and `VS`, the disc shows only one; the cabinet has `ADVERTISE SOUND`, `VS FINISH`,
`BIKE LOGO` and `COUNTRY`, which the disc does not show.

A **hypothesis**, not a fact: `SURVIVAL TIME` is 180 sec. in the capture, and one of the unnamed bytes of
the cabinet's block, `0x12054A`, carries `0x00B4` = 180. To be checked before touching it.

And a hint about `DIFFICULTY`'s range: the capture shows `VERY EASY`, which supports 0 to 4 = `VERY EASY` …
`VERY HARD`, with 2 = `NORMAL` as the factory setting.

### 4. What each line triggers

The five lines of our menu that do nothing: `RANDOM`, `V.S.`, `NETWORK`, `TRAINING`, `OPTIONS`. The
Dreamcast does all of them. The question is what it writes into the arcade game for each — which is exactly
the nature of the levers `MenuConsole.md` found by trial and error.

**Measurement**: a breakpoint on writes to the settings area (`0x00120540`-`0x0012055F` on the arcade side)
and on the mode machine, while each line is chosen.

### 5. Where the Dreamcast differs from the arcade, and what that imposes on our port

The Dreamcast **emulates or ports** the arcade game; our levers are the arcade's. Where it does otherwise,
one has to know whether that is because its game is different, or because it found a better lever.

The case already known: the Dreamcast has `INTERNET` and `RANDOM NETWORK`, which do not exist on the
cabinet.


## The order, and what stops it

One step, one measurement shown, nothing written into the port's code before question 3.

1. **Read what already exists**: `fv2-arcade-notes\README.md` in full (1900 lines), and the 2D sections in
   particular. Probe nothing before that.
2. **Set up the Flycast + GDB session** and check that a breakpoint on `dessine` fires. Without that,
   nothing of what follows can be measured.
3. Questions 1 and 2, which go together: the screens and their transitions.
4. Question 3: the vertical list and the selected line's mark.
5. **Settle the labels' divergence** — and only then decide whether our image and our seven lines hold.
6. Question 4, line by line, from the simplest to the most obscure.
7. Only then: what we change in the port, and in what order.

## What this plan does not do

- **Not a line of Dreamcast code goes into the repository.** The addresses, the tables and the measurements
  are facts about the game; the code is not. The same rule as for Supermodel and MAME.
- **Nothing changes in the port before question 3 is closed.** Our menu works; we do not take it apart to
  follow a reading in progress.
- **We do not conclude from one probe.** The one of 2026-10-22 produced a false assertion in three minutes.
  A conclusion about this binary takes two independent readings, or one reading plus a measurement under
  GDB.
