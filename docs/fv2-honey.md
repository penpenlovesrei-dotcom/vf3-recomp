# Fighting Vipers 2: Honey without her skirt -- what the game actually has

The question put to this page: the owner reports that Honey has a "no skirt" model, obtained by thirty or more
straight wins in versus with a second player cutting in (乱入対戦), with her lower armour destroyed during those
matches or on the thirty-first. That is player folklore. This page says what was measured, what it rules out,
and what it does not settle.

**Status: in progress.** Everything below is a measurement taken in this tree with `tools/vf3run`; where
something is an inference it says so.


## The instruments

`tools/scripts/fv2-honey.txt` picks Honey and fights the CPU without a window. Honey is cell **8** of the select
row -- eight presses of right from Emi -- and her in-fight identifier is **6**, which is what
`0x007005B8` holds once the fight starts. Both read back from `--ram-at`'s dump.

`VF3_POKE=addr=value` with `VF3_POKE_FRAMES=from-to` writes RAM before every frame, so an effect can be tried
without compiling anything. The address is an index into `--ram-at`'s dump, which keeps 32-bit words in the
host's order: **the byte of guest address A is at index A^3**. Every address in this page is a *guest* address.


## Sega named these variables themselves, and nobody had read that table yet

`fv2-arcade-notes/symbols.py` pulls Sega's own debug-table names out of the program, but it only scans two narrow
ranges and only keeps the entries whose variable field is an absolute address. Scanning the whole debug data
region instead finds **97 pages and 1035 named entries**, and the layout is this:

- a *page* record is three words: pointer to the page's name, pointer to its first entry, pointer to its
  terminator;
- an *entry* is `0x20` bytes: `+00` pointer to the label, `+08` pointer to a name array (for enum and bit
  fields), `+0C` the variable, `+1C` the type;
- the terminator's first word is the **base** the page's `+0C` fields are offsets from. For the per-fighter
  pages that base is `0x00700500` (player 1) and `0x00707500` (player 2).

That base is what the older script was missing, and it is why the armour fields had never been read: they are
stored as offsets, not addresses.


## The armour is a combat state, and here is where it lives

Page `ARMOR_INFO` names them with absolute addresses, which agree with the offsets on page `ROB0_ARMOR`:

| Sega's name | player 1 | player 2 | size |
|---|---|---|---|
| `flag0` / `flag1` | `0x00701444` | `0x00708444` | 32-bit |
| `damage0(u)` / `damage1(u)` | `0x00701438` | `0x00708438` | 32-bit |
| ` limit0(u)` / ` limit1(u)` | `0x00701440` | `0x00708440` | 16-bit |
| `damage0(d)` / `damage1(d)` | `0x0070143C` | `0x0070843C` | 32-bit |
| ` limit0(d)` / ` limit1(d)` | `0x00701442` | `0x00708442` | 16-bit |

`u` is the upper armour, `d` the lower one. Read mid-fight with Honey against the CPU's Charlie, at frame 4350:

```
dmgU=124 limU=350  dmgD=119 limD=300   flag0=00000000
```

So Honey's upper armour takes 350 points and her lower one 300.

### The bits of `flag0`, measured by forcing the limits down

Three runs of the same script, differing only by a poke, RAM read at the same frame:

| poke | `flag0` |
|---|---|
| none | `00000000` |
| `limit_d` forced to 1 | `00000002` |
| both limits forced to 1 | `00000003` |

**Bit 0 is the upper armour broken, bit 1 the lower one.** The game's own code agrees: at `0x00067E90` it reads
`flag0`, and if `!(flag & 5)` and `damage_u >= limit_u` it sets bit 0; if `!(flag & 0xA)` and
`damage_d >= limit_d` it sets bit 1.

The three runs' `rob_flag` came out identical (`3B610001`), so the poke did not disturb the simulation: these
are clean A/B pairs, same camera, same pose.


## `hadaka` -- Sega's own word for it

Among the bit names Sega gave the per-fighter status word `rob_flag` (`0x00700500` for player 1,
`0x00707500` for player 2) there is one called **`hadaka`** -- 裸, bare. It is bit `0x00000002` of that word.

The bit order was settled twice over. First by plausibility: read mid-fight, `rob_flag` is `0x71010401`, and
reading the names most-significant-bit first gives `left_look`, `fix_hara`, `ypos_chk_pass`, `init_end`,
`rob_calc`, `kao_init`, `event` -- all sensible in the middle of a round -- where the other direction gives
`ring_out` and `hadaka` set at the same time. Then by the code, which is not an opinion:

```
00067E6C  lwz  r0,0x0(r3)       the status word
00067E70  stw  r9,0xf44(r3)     the armour flag, with 0x14 and 0x28 just folded in
00067E74  lha  r9,0xf40(r3)     limit_u
00067E78  lha  r11,0xf42(r3)    limit_d
00067E7C  ori  r0,r0,0x802      <-- 0x002 is hadaka
00067E80  stw  r0,0x0(r3)
00067E84  stw  r9,0xf38(r3)     damage_u := limit_u
00067E88  stw  r11,0xf3c(r3)    damage_d := limit_d
```

This routine destroys **both** armours at once and sets `hadaka` with them. So `hadaka` is the game's name for
a fighter who has lost all of their armour, and it is set on a path separate from the ordinary
damage-reaches-limit one above.


## What the armour flag alone does not do

Forcing `flag0` to 2 or to 3 mid-fight changes the picture by **510 and 1061 pixels** respectively, and every
one of them is inside `x 1..28, y 7..92` -- the little green figure in the top-left corner, the armour
indicator. **The 3D model does not change.** Diffed frame by frame against the untouched run, the fighter is
byte-identical.

So the flag records the break; it does not perform it. Whatever swaps the model is on the break event's path,
not in the flag.


## Still open

- what the break event does to the model, and whether Honey has a part the others do not;
- whether a straight-win counter exists, where, and what it is compared against;
- `*1P COLOR` (`0x007005B9`): poked to 1 and 3 during a fight it only repaints the name plate (382 and 816
  pixels, all in the name plate's rectangle); poked to 2 the CPU stops with an invalid instruction at
  `0x00020908`; poked to 4 the whole screen changes. None of that has been run down yet.
