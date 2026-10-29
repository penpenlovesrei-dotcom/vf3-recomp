# Fighting Vipers 2: the two Kuhns, and what actually separates them

With the roster patch the select row offers **two Kuhns** -- cursor 12 and cursor 14, fighter ids 13 and
12. `fv2-arcade-notes/README.md` says Dural is Kuhn's old name and that "the two ids load the same
character". That is nearly right and worth making exact, because one thing does differ and it is not
cosmetic.

**Same moves, same animations, same reach. Armour three times tougher on id 12.**


## The record says it first

The fighter table found in `docs/fv2-colours.md` -- 120-byte records at `0x0007F9CC + id*4` -- gives one
record each:

```
id 12  DURAL  0x0007F7EC
id 13  KUHN   0x0007F864
```

Sixteen of their thirty words differ, which looks like a lot until the words are read:

| words | what | |
|---|---|---|
| +0x00..+0x2C | twelve table pointers | all shifted by exactly `0x450` |
| +0x30 | `FFFFFFFF` | same |
| +0x34, +0x38 | two sub-tables | shifted by `0x58` and `0x18` |
| +0x3C..+0x5C | five floats: 3.0, 45.0, 20.0, 20.0, 1.5 | **same** |
| +0x60, +0x64 | **1200 / 1200 against 400 / 400** | **different** |
| +0x74 | the colour list, `0x0007FACC` | **same** |

**The twelve tables are byte-identical copies**: the 0x450 bytes at `0x0007E10C` and at `0x0007E55C`
differ in **0 bytes out of 1104**, and the sub-tables they point at have the same relative offsets on both
sides (8, 16, 24, 40, 56, 72). Everything that makes a fighter move is duplicated verbatim.


## `+0x60` / `+0x64` is the armour, and Honey proves it

The same field across the roster:

All sixteen, the stored field and the limit the fight reads (its double):

| id | | `+0x60` | `+0x64` | upper | lower |
|---|---|---|---|---|---|
| 14 | DEL SOL | 120 | 150 | **240** | **300** |
| 0 | GRACE | 150 | 175 | 300 | 350 |
| 2 | RAXEL | 150 | 200 | 300 | 400 |
| 3 | TOKIO | 150 | 200 | 300 | 400 |
| 4 | SANMAN | 150 | 225 | 300 | 450 |
| 5 | JANE | 150 | 300 | 300 | **600** |
| 7 | PICKY | 150 | 175 | 300 | 350 |
| 8 | MAHLER | 150 | 200 | 300 | 400 |
| 11 | TECHNO (Emi) | 150 | 200 | 300 | 400 |
| 15 | KUMA | 150 | 200 | 300 | 400 |
| 6 | HONEY | 175 | 150 | **350** | **300** |
| 10 | CHARLIE | 180 | 200 | 360 | 400 |
| 1 | BAHN | 200 | 200 | 400 | 400 |
| 9 | BM | 400 | 400 | **800** | **800** |
| 13 | KUHN | 400 | 400 | **800** | **800** |
| 12 | DURAL | 1200 | 1200 | **2400** | **2400** |

Three things the column makes plain.

**300 is the default upper**: nine of the twelve regulars carry it, so the ones that do not are
deliberate. The regulars span 240 to 400 above and 300 to 600 below -- a narrow band.

**Honey is the only fighter whose upper armour outlasts her lower one** (350 against 300). Everyone
else is equal or the other way round. That fits what is known of her: it is the *lower* piece that
comes off, and the skirt goes with it (`docs/fv2-honey.md`).

**The three hidden fighters are off the scale.** BM and Kuhn at 800/800, Dural at 2400/2400 -- eight
times a regular. They were not balanced to be played, which agrees with BM having no displayable name
at all (`docs/fv2-roster.md`).

The third halfword of `+0x64` (13 to 38, one per fighter) is **not identified**. It follows neither
limit, nor any obvious weight.

`docs/fv2-honey.md` measured, mid-fight, that **Honey's upper armour takes 350 and her lower one 300**.
Here she carries 175 and 150: exactly half, on both. The field is the armour limit before the game doubles
it, and the reading is checked against a number that was obtained another way entirely.

Read back in a fight (`0x00701440` upper, `0x00701442` lower) the two Kuhns give **800/800 and 2400/2400**
-- the double, as predicted.


## The in-game check, and the two ways it lied first

Fifty pictures, same stage, same opponent, the same fixed sequence of buttons at the same frames (punch,
kick, guard, punch, kick, punch), no random play at all:

**All fifty are identical to the bit.**

Getting there took three attempts, and the two failures are the useful part.

**First attempt**, same seed and random play: 91 pictures out of 91 differed, 87 % of the pixels, from the
first frame. **Second attempt**, random play removed: the same 87 %. Both looked like proof that the two
fighters behave differently. They were not.

The cause was the protocol. **Reaching cursor 12 takes twelve presses and cursor 14 takes fourteen.** Two
extra inputs before the match are enough to leave the game in a different state, so the CPU's Charlie plays
differently -- and Charlie is most of what is on screen. The pictures were measuring the opponent.

**Third attempt**: fourteen direction presses on both sides, at the same frames -- thirteen rights and one
left to land on 12, fourteen rights to land on 14. Everything falls into line.

The lesson is general enough to be worth keeping: **two runs are only comparable if their inputs are
comparable in number, not just in effect.**


## What the 81 differing words of RAM are

Over 8 MB, at frame 3300, with the inputs equalised:

| address | | |
|---|---|---|
| `0x00279668` | 12 against 14 | the cursor, the address `docs/fv2-roster.md` already names |
| `0x0030E088` | 14 against 13 | the id |
| `0x005949F4` and on, every 0x168 | `0x0A6F` against `0x0183` | **not a resource id**: field +12 of the fighter's working record, copied every frame beside his position (`docs/fv2-parts.md` section 4) |
| `0x00701440` / `0x00701442` | 800/800 against 2400/2400 | the armour |

Nothing else. No difference in position, in animation frame, or in the twelve tables' working copies.


## What is not settled

- **The repeated `0x0A6F` / `0x0183`.** Something per-fighter, written in a table of 0x168-byte entries at
  `0x005949F4`. Not chased. **Chased 2026-10-29, see `docs/fv2-parts.md` section 4:** the table is at
  `0x00591E18`, its entries are 180 bytes a player, and the halfword is field +12 of the fighter's working
  record, which changes every frame. What that field is remains unidentified; that it is no identity is now
  measured.
- **Whether the armour difference is reachable in play.** Fifty frames of light punching damage nothing, so
  the pictures stay identical; the difference only shows in a fight long enough to break armour. Not filmed.
- **The two sub-tables at +0x34 and +0x38.** Their contents were not compared beyond the first words; one
  of the two has entries of different lengths on each side, which the twelve big tables do not.
