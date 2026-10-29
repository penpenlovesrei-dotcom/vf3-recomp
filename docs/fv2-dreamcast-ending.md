# The Dreamcast's ending, read in its own code

Frédéric, the 2026-10-29, on rebuilding the ending in this port: "tu devras analyser le code de la version
dreamcast pour savoir l'ordre d'affichage, les zones à afficher, et la vitesse de defilement". This is that
reading. Everything below comes out of `1ST_READ.BIN` (`C:\Users\frede\Documents\fv2-arcade-notes\dreamcast`),
with his own SH-4 disassembler, `desas_sh4.py`, and the resource mechanism his README already describes.

Nothing here was watched running. Where the code decides, the code is quoted; where a number is read out of
the binary, its address is given.

## The drawing pair, and what a "pattern" is

The Dreamcast port draws all of its 2D — logos, menus, font, **staff roll** — through one pair of functions:

| address | role |
|---|---|
| `0x8C02A23A` | `charge_ressource(record)` — makes a resource current |
| `0x8C029598` | `dessine(rank, x, y)` — rank in R4, x in R5, y in R6 |

A **pattern** is a 10-byte record — descriptor rank in the bank, x, y, width, height — so a rank names a
RECTANGLE CUT OUT OF A TEXTURE. That is the answer to "les zones à afficher": the zone is the pattern, and
the pattern list of the façade resource `0x8C0E5BC8` holds them.

## The staff roll's ten pages

`0x8C01CC28` is the drawer. Its branch for `param >= 16` copies three static 20-byte tables onto its stack
and indexes them with `i = (param + 0xEB) & 0xFF`:

| i | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 |
|---|---|---|---|---|---|---|---|---|---|---|
| rank (`0x8C0CCC74`) | 79 | 80 | 81 | 82 | 84 | 85 | 86 | 87 | 88 | 89 |
| x (`0x8C0CCC4C`) | 185 | 170 | 170 | 170 | 172 | 172 | 172 | 173 | 172 | 129 |
| y (`0x8C0CCC60`) | 212 | 187 | 158 | 137 | 152 | 152 | 152 | 168 | 57 | 212 |

On `i == 3` it draws a second pattern over the first: rank 83 at (378, 161).

**The x and y handed to the function are thrown away in this branch** — `mov.w @(r0,r12),r5` and
`mov.w @(r0,r11),r6` overwrite them from the tables just before the call. A page of the staff roll is drawn
where its table says and nowhere else.

The other branch, `param < 16`, is the one that USES the caller's x and y: it loads the resource of the
current illustration (a 16-byte record at `0x8C0E78D0 + index*16 + 16`) and draws patterns 0 and 1 side by
side, at x and x + 128. Two tiles of 128, placed anywhere.

## The script: the ending is a table of records

`0x8C01062A` is the interpreter. The scene it works on is 56 bytes, in a table at **`0x8C0E326C`**, indexed
by the byte at `0x8C15E00D`; there are fifteen scenes, the sixteenth being `-1`. A scene's `+0x18` is its
list of records and its `+0x1C` their count; `+0x1E` is how long the scene lasts, in frames.

A record is 12 bytes:

```
+0  u16  the page to draw (the param above)
+2  u16  x            +6  u16  n, the step of the slide
+4  u16  y            +8  u16  the frame it fires on
                      +A  u16  how many frames it stays
```

When the scene's frame counter (`0x8C15E068`) reaches `+8`, the interpreter copies the record into RAM —
`0x8C15E06C` x, `0x8C15E070` y, `0x8C15E07C` the page, `0x8C15E078` the countdown, `0x8C15E080` the step —
and from then on calls the drawer every frame until the countdown runs out.

## The speed, and it is horizontal

`0x8C0113AA`, called once a frame while a second counter (`0x8C15E074`, set to **15**) is positive:

```
r4 = n ; r4 <<= 2 ; r4 += n ; r4 <<= 1        ; r4 = n * 10
x += r4                                        ; 0x8C15E06C
```

So **x moves by 10 n per frame, for fifteen frames** — 150 n in all. Nothing anywhere writes `0x8C15E070`
except the interpreter itself, so y does not move while a page is up.

## The whole ending, scene by scene

Read out of the tables above: the slides start off-screen and come in at **40 pixels a frame** (n = ±4),
fifteen frames, 600 pixels in all; the staff roll's pages do not move at all.

| scene | frames | page | what | from x | step | stays |
|---|---|---|---|---|---|---|
| 0 | 204 | 3 | illustration | 620 | −40 | 100 |
| 1 | 204 | 10 | illustration | −280 | +40 | 100 |
| 2 | 204 | 5 | illustration | 620 | −40 | 100 |
| 3 | 204 | 7 | illustration | −280 | +40 | 100 |
| 4 | 613 | 22, 23, 24, 25 | **staff roll**, ranks 80, 81, 82, 84 at y 187, 158, 137, 152 | — | — | 140 each, at frames 0, 154, 307, 460 |
| 5 | 204 | 0 | illustration | 620 | −40 | 100 |
| 6 | 613 | 26, 27, 28, 29 | **staff roll**, ranks 85, 86, 87, 88 at y 152, 152, 168, 57 | — | — | 140 each, at frames 0, 154, 307, 460 |
| 7 | 204 | 11 | illustration | 620 | −40 | 100 |
| 8 | 102 | — | nothing: a pause | | | |
| 9 | 409 | 1 | illustration | −280 | +40 | 100 (at frame 307) |
| 10 | 204 | 8 | illustration | −280 | +40 | 100 |
| 11 | 204 | 4 | illustration | 620 | −40 | 100 |
| 12 | 409 | 2 | illustration | 620 | −40 | 100 (at frame 307) |
| 13 | 409 | 6 | illustration | −280 | +40 | 100 (at frame 307) |
| 14 | 102 | — | nothing: a pause | | | |

**Eight pages of credits**, not ten: the roll uses i = 1 to 8. The two left over, i = 0 (rank 79) and
i = 9 (rank 89), both at y 212, belong to params 21 and 30, which no scene of this table asks for.

## What rises, and what does not

Within a page, nothing moves: n is 0 on all eight records of scenes 4 and 6. What climbs is the SEQUENCE of
pages — y 187, then 158, then 137 — so the text does walk up the screen, a page at a time, and the eye reads
that as a roll. The movement the code animates is the ILLUSTRATIONS', and it is sideways: in from one edge,
40 pixels a frame, a quarter of a second.

## The zones, read out of the pattern list

The façade's resource record at `0x8C0E5BC8` says: TEX entry **16**, its descriptor array at `0x8C0E56C8`,
its pattern list at `0x8C0E5808`. Descriptors 3, 4 and 5 are three 512 × 512 VQ textures at `0x0A1000`,
`0x0B1800` and `0x0C2000` of that entry -- the three sheets the credits are painted on.

The eleven patterns the staff roll draws from, each a rectangle in its sheet:

| rank | sheet | x | y | w | h | what it holds |
|---|---|---|---|---|---|---|
| 79 | 3 | 0 | 0 | 272 | 64 | FightingVipers2 STAFF, the heading |
| 80 | 3 | 0 | 64 | 304 | 112 | Director, Game Coordinator |
| 81 | 3 | 0 | 184 | 304 | 168 | Chief Programmer, Chief Designer, Sound Director |
| 82 | 3 | 304 | 0 | 208 | 192 | Programmers -- the LEFT half of those six names |
| 83 | 3 | 304 | 216 | 144 | 168 | their RIGHT half, drawn beside it at (378, 161) |
| 84 | 4 | 0 | 336 | 360 | 168 | Motion Designers |
| 85 | 4 | 0 | 0 | 384 | 168 | Character Designers, Original Character Designer |
| 86 | 4 | 0 | 168 | 328 | 168 | Stage Designers, Visual Effect Designer |
| 87 | 5 | 0 | 368 | 328 | 144 | Special Thanks to |
| 88 | 5 | 0 | 0 | 360 | 312 | Publicity by, Assistant Companies, the two studios |
| 89 | 5 | 0 | 312 | 408 | 56 | Presented by Original Game, SEGA ENTERPRISES, LTD. |

THE PROGRAMMERS' NAMES ARE CUT IN TWO BY THE SHEET'S EDGE, and that is not damage: pattern 82 ends at
x = 512, the sheet's right edge, and pattern 83 holds the rest of the same six names. The page draws 82 at
its own place and 83 at (378, 161) -- 208 to the right, which is exactly 82's width, and 24 lower, which is
the height of the Programmers heading. Side by side they read as one block.

Cut out and looked at, every one of the eleven is a clean block of credits with nothing of its neighbours in
it, which is what says the rectangles are read right. **The sheets are stored first row at the bottom** (as
`textures_dc.py` says of this format), so a rectangle at y in the table is at `512 - y - h` in the file.

The script of the two roll scenes asks for pages i = 1 to 8, which is ranks 80 to 88. Ranks 79 and 89 are
drawn by params 21 and 30, which no scene in this table uses.

## Cutting the replays: not through MODE_SUB

Frédéric asked for the fight passages to go: "est il possible de supprimer proprement les phases de combat
du ending ?". Three of the ending's seven sub-states are the replay and nothing else — `ending_replay_init`,
`ending_replay_ctrl`, `ending_replay_next` — and this port already knows how to ask a sub-state to move on:
`Model3::fv2_cut_submode` writes `SUBMODE_REQUEST` and Sega's dispatcher copies it into `MODE_SUB` on the
next tick, which is the door training uses for the round's announcement.

**It is not that door.** Watched byte by byte through a whole roll (`VF3_RAM_WATCH=0x0058FD90:16`),
`0x0058FD99` — `MODE_SUB` — reads **11** from the first frame of mode 12 to the last of mode 13 and never
moves; only `MODE_MAIN` steps, 12 then 13. The ending's seven sub-states are kept in its own task's block,
not in the game's mode pair, so cutting them means finding that block first.

Nothing of the replays is seen meanwhile: the port owns the screen for the whole ending (`fv2_roll.h`), so
they run under a picture that is not drawn. What they still cost is time — the ending lasts about 110
seconds whatever is on it, which is why the roll's own speed is set to fill that length.
