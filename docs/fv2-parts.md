# Fighting Vipers 2: a fighter's body is a list of part ids, and another fighter can be given it

The question this page answers is whether one fighter can carry another's model. He can, and it takes four bytes:
the pointer to his part list, repointed at somebody else's. His pose does not move by a pixel while it happens.

Everything below was measured here, headless, with `tools/vf3run` on the Japan Revision A set. The fighter
table it starts from -- 120-byte records at `0x0007F9CC + id*4` -- is `docs/fv2-colours.md`'s, and the 0x450
shift between two of those records is `docs/fv2-two-kuhn.md`'s.


## 1. The twelve pointers are eight tables, and the eight are one block per fighter

A fighter's record opens with twelve pointers (`+0x00` to `+0x2C`). Read across the sixteen fighters they are
not twelve things but **eight**, and the last four repeat the middle four:

| pointer | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| Picky | `7CB7C` | `7CDA4` | `7CC90` | `7CEB8` | `7CC30` | `7CE58` | `7CD44` | `7CF6C` | `7CC30` | `7CE58` | `7CD44` | `7CF6C` |

The eight sit in one **0x450-byte block per fighter**, sixteen blocks contiguous from `0x0007AD4C`, each exactly
0x450 past the one before -- the same 0x450 that separates Dural's twelve tables from Kuhn's. Inside a block:

| at | size | what |
|---|---|---|
| +0x000, +0x114, +0x228, +0x33C | 0xB4 = **45 words** | pointers 0, 2, 1, 3 |
| +0x0B4, +0x1C8, +0x2DC, +0x3F0 | 0x60 = **24 words** | pointers 4, 6, 5, 7 |

So each fighter has four 45-slot tables and four 24-slot ones. This page is about the 45-slot ones.


## 2. A 45-slot table is a part list with holes

Each word is either a small id or `FFFFFFFF`. Picky's first table:

```
FFFFFFFF 000000FE FFFFFFFF 0000002E 00000105 00000107 0000010B FFFFFFFF 00000106 ...
```

**The slot layout is common to all sixteen fighters.** `#` is a part, `.` is `FFFFFFFF`:

```
         slot 0         10        20        30        40
GRACE    .#.####.###.######.####.#########..########..  35
BAHN     .#.#.##..##.###############..####..##########  35
RAXEL    .#.##############################..##########  41
TOKIO    .#.#.##..##.######.####.###....##..######....  27
SANMAN   .#.###############.####.#####..##..######....  33
JANE     .#.###############.####.#########..########..  37
HONEY    .#.##########################..##..######..##  37
PICKY    .#.####.###.#################..##..######..##  35
MAHLER   .#.####.###.#####################..##########  39
BM       .#.####.###.#####################..##########  39
CHARLIE  .#.####.###.#####################..##########  39
TECHNO   .#.####.###.#####################....########  37
DURAL    .#.####.###.#####################..##########  39
KUHN     .#.####.###.#####################..##########  39
DELSOL   .#.####.###.#####################..##########  39
KUMA     .#.#..##.###.#.#.#..#.#......................  12
```

Slots 0, 2, 33 and 34 are empty for everybody; slots 1, 3, 6, 9, 10, 13, 15, 17, 20 and 22 are filled by
everybody. Kuma is the exception at both ends: twelve parts in this table, and 92 of 96 in his small ones,
where the other fifteen carry 60 to 64.

Over the four big and four small tables of all sixteen fighters there are **3,298 ids, 1,392 of them distinct**.
Only 106 are used by more than one fighter, and 104 of those are Dural's and Kuhn's, who share everything.
The two sizes draw on two separate bands: `0x0001`-`0x05AB` for the 45-slot tables, `0x122A`-`0x14AB` for the
24-slot ones (Kuma inverted again: his 45-slot ids are `0x1203`-`0x12CF`).


## 3. The swap, and what it moves

`VF3_POKE` holds Picky's four pointers on Kuma's four tables from frame 2000 of `tools/scripts/fv2-fight.txt`,
and the pointers are **read back from the dump at frame 3300** to prove the poke landed before anything is
concluded from it:

```
baseline   0007CB7C 0007CDA4 0007CC90 0007CEB8   0007CC30 ...
swapped    0007EDFC 0007F024 0007EF10 0007F138   0007CC30 ...
```

Against the baseline, at the same frame of the same script:

| | polygon RAM (`--poly-at 3300`) | main RAM (`--ram-at 3300`) |
|---|---|---|
| the four 45-slot tables | **14,510 words of 1,048,576**, the first at word 0x800 | 69,637 bytes; 16,409 words of them in `0x003E0000`-`0x003FFFFF`, 416 in `0x003C0000` |
| the four 24-slot tables (and their repeats) | **0 words** | 4,605 bytes, none above `0x00400000` but for `0x007D0000`-`0x007EFFFF` |

Word 0x800 is where `docs/fv2-colours.md` already put the start of geometry; the 0x3C0000 words are the colour
table that page measured, which follows the fighter.

**On screen**: the body is replaced and nothing else is. Same pose, same frame of the animation, same clock
(17"65 at frame 3851), the opponent untouched to the pixel. Where the donor's table has a hole the part simply
does not appear -- Kuma fills twelve slots of 45, so Picky loses his head and his jacket and keeps the pose.

**Only table 0 is drawn in this fight.** Pointer 0 alone, poked by itself, gives a picture **byte-identical** to
the four poked together, on all sixteen screenshots, and polygon RAM identical to the word. Pointer 1 alone
changes **nothing at all**: 0 pixels, all sixteen. The other three tables are other states, not reached here.

**It is not the donor.** Grace in Kuma's place: 16,185 words of polygon RAM, and a complete body -- head, torso,
arms, legs in their places, in Picky's pose, with her own textures.

**And the slots mean the same part for everyone.** Ten of Grace's parts poked one id at a time into Picky's own
table -- the ten slots every fighter fills -- give a chimera whose every piece is anatomically where it belongs:
his helmet and shorts, her arms, her knee pads and boots. 3,005 to 6,133 pixels a picture. A single part is
180 to 724 pixels and is likewise in its place. Nothing scrambles.


## 4. What this corrects

`docs/fv2-two-kuhn.md` lists, among the 81 words that separate the two Kuhns, "a resource id, repeated" at
`0x005949F4` and every 0x168, not identified. **It is not a resource id.**

`vf3run --watch 5949F4` names the store, and the code around it says what it is:

```
0001E95C  lhz r6,24000(r14)      r14 = [0x0007511C] = 0x00700500, the fighter's working record
0001E960  lhz r7,16(r9)
0001E964  sth r3,68(r10)         <- 0x005949F4, from lhz r3,12(r14)
0001E968  sth r4,70(r10)         <- 0x005949F6, from lhz r4,92(r14)
```

`r10` walks a table at `0x00591E18` in records of 180 bytes, player 2's 180 past player 1's (`addi r10,r10,180`
at `0x0001E854`) -- which is the 0x168 the page saw. The routine at `0x0001E8F0` fills each record from the
fighter's working record: three floats at `+0x10`/`+0x14`/`+0x18`, the words at `+0`, `+108`, `+112`, `+232`,
`+3908`, then these halfwords. So `0x005949F4` is **field +12 of the fighter's working record, copied every
frame beside his position**, and it changes every frame:

```
store16 005949F4 = 4000    store16 005949F4 = 439A    store16 005949F4 = 5844
store16 005949F4 = 4000    store16 005949F4 = 4859    store16 005949F4 = C718
```

A number that takes six values in six frames is not an identity. What field +12 is was not settled.


## 5. What a part id resolves to

A part id is not an address. It is an index into one flat directory, and the directory says where that part's
**culling nodes** are. The models themselves are further down, in the VROM. The chain was measured end to end,
and one turn of it was measured wrong first -- section 7 keeps the mistake with what corrected it.

### The directory: 5,298 words at `0x003003CC`

Three runs differing by **one part id** -- slot 1 of Picky's first table, given Grace's `0x02BC` and Bahn's
`0x005B` against his own `0x00FE` -- differ in exactly **four words of the 8 MB**:

| | |
|---|---|
| `0x0007CB80` | the poked id itself |
| `0x007065AC` | the same id, in the fighter's working record |
| `0x007DB04C` | `00041B18` -> `0004426E` / `00040F06`, written byte-reversed (`stwbrx`) at PC `0x00006C20` |
| `0x00580E34` | a running total, not identified |

The three values are each in **one** place in all of RAM, and that place is the same table:

```
0x003003CC + 0x00FE*4 = 0x003007C4 : 00041B18
0x003003CC + 0x02BC*4 = 0x00300EBC : 0004426E
0x003003CC + 0x005B*4 = 0x00300538 : 00040F06
```

A sweep of the 8 MB for a table `T` with `T[id] = value`, at every stride from 4 to 48, returns **one**
candidate: base `0x003003CC`, stride 4. It holds **5,298 non-zero entries** (ids `0x0000` to `0x14B1`),
strictly increasing from `0x00040000` to `0x0007978E`.

### What that address is: a place in the culling RAM

`--cull-at` was added for this (`tools/vf3run.cpp`), because nothing in this port showed that memory. At frame
3300 the culling RAM holds **145,638 non-zero words, the first at `0x040000` and the last at `0x079797`** --
and the directory's values run from `0x00040000` to `0x0007978E`. The two are the same range, and at every one
of those addresses there is the head of a node (`n[0] & 3` set).

A node is ten words, and the word the game takes from the directory is read as a child pointer
(`n[5 + node_shift]`, `src/video/r3d_scene.cpp`). A part is a small tree of them:

```
041B18  800AD412 ... 00041B22 01000000     child -> a node
041B22  800AD812 ... 00041B2C 01000000     child -> a node
041B2C  800ADC12 ... 0166261B 00041B36     child -> a MODEL, sibling -> 041B36
041B36  800AE012 ... 016627FC 00041B40     another model
041B40  800AE412 ... 01662CC6 01000000     another, and the chain ends
```

Walked that way, Picky's slot 1 is **3 models** and Grace's slot 1 is **8**. Swept over the whole filled
region, the nodes' type-1 children are **21,457 into the VROM** and 1,567 into the low polygon RAM
(`0x800`-`0xD7E`: the joints the game builds each frame, which the port's renderer already names).

### How the two memories are filled

The culling node tree is **not** the Real3D DMA's doing. Over a whole run its blocks are 16,163 into the other
half of that memory -- the 48-byte records of the table above, from main RAM `0x007DAD10` -- and 450 from the
CROM into polygon RAM words `0x10000`-`0x58FC6`, every one of the 450 at the same constant offset,
`source - destination = 0x67A42000`. **No node seen points into those words**, and what that 1.14 MB is for
was not established here.

### The directory itself comes from the CROM

`vf3run --watch 3007C4` catches **one** store, at PC `0x0003312C`, and the routine around it reads its source
with `lwbrx` and masks it to 24 bits:

```
0003310C  addi  r8,r9,972        r8 = 0x003003CC, the directory
00033120  lwbrx r0,0,r0          a word, byte-reversed, from r31
00033128  rlwinm r0,r0,0,8,31    kept to 24 bits
0003312C  stwx  r0,r9,r8         directory[first id + k] = it
```

and its caller walks a list in the CROM at **`0xFFF10000`**, terminated by `FFFFFFFF`, whose entries are
offsets from that same address:

```
00038C88  lis   r9,0xFFF1
00038C94  cmpwi cr1,r0,-1        FFFFFFFF ends it
00038CAC  add   r3,r3,r27        0xFFF10000 + the entry
00038CB0  bl    0x000330D0       fill a run of ids from there
```

### Kuma, read again

An earlier pass of this page said Kuma's twelve parts pointed past anything the machine loads, and read the
first picture of section 3 as his parts resolving to nothing. **That was wrong**, and it came of taking the
directory's addresses for polygon RAM. Read in the culling RAM, all twelve have a node and would be drawn.
What is true of him is what section 2 already says: his first table fills **12 slots of 45**, where the other
fifteen fill 27 to 41. The picture is twelve of his parts drawn and thirty-three absent.

## 6. Repeating it

```
set VF3_POKE=7F597=0,7F596=7,7F595=237,7F594=252   (Picky's pointer 0 -> Kuma's table 0)
set VF3_POKE_FRAMES=2000-4200
vf3run fvipers2.zip --script tools\scripts\fv2-fight.txt --shots DIR --poly-at 3300 --ram-at 3300
```

A poke address is an index into the dump, so the byte of address A is written at `A^3`, and values are decimal
(`docs/fv2-honey.md`). The fighter table is program data the boot inflates once and never rewrites, so the poke
holds -- unlike the colour table of `docs/fv2-colours.md`, which the game wins back every frame.

Section 7's part, added whole, with the three levers written for this page:

```
set VF3_POKE=3057CF=0,3057CE=8,3057CD=0,3057CC=0,7CB83=0,7CB82=0,7CB81=21,7CB80=0
set VF3_POKE_FRAMES=2000-4200
set VF3_CULL_COPY=80000=4426E:A
vf3run fvipers2.zip --script tools\scripts\fv2-fight.txt --shots DIR --cull-at 3300
```

`--cull-at FRAME` writes both halves of the culling RAM beside `--poly-at`'s dump; `VF3_CULL_COPY=dst=src:words`
and `VF3_POLY_COPY` copy inside the culling RAM and inside polygon RAM, held before every frame, in words and
in hexadecimal. All three are `tools/vf3run.cpp` only: the game's own path has nothing of them.


## 7. Adding a part, and the mistake that came first

### The id goes in

The directory's last entry is id `0x14B1`, and **4,702 words of zero follow it** (`0x305694` to `0x30A00C`).
Id `0x1500` lands at `0x003057CC`, in that empty run. Poked there: Grace's address `0x0004426E`, with slot 1
of Picky's table poked to `0x1500`, an id Fighting Vipers 2 never names.

**Sixteen pictures of sixteen, byte-identical to giving the slot Grace's own id `0x02BC`.** The directory is
read live, for any id, and an id the game has never heard of is as good as one of its own.

### The mistake: a model copied into free polygon RAM draws nothing

Her model was copied -- `VF3_POLY_COPY`, added for this -- into polygon RAM at word `0x60000`, the copy
checked word for word in the dump afterwards, and the entry for `0x1500` set to `0x00060000`. **Nothing was
drawn**, on all sixteen pictures, exactly as if the entry pointed at zeros. The same at `0x59000`. Writing
over a model inside the polygon-RAM region did not draw it either, and sent the fight off its rails by frame
3251 (45 % of the pixels), so that run says nothing at all.

The game was not what refused it. Two runs differing only in that entry differ, over the whole 8 MB, in
**exactly two words**: the directory entry, and the node word at `0x007DB04C`, which carries the invented
address faithfully (`00000600`, which is `0x00060000` byte-reversed).

**What was wrong was the memory.** The directory addresses the culling RAM, not polygon RAM. Word `0x60000`
there is the middle of a live node (`BFA24000`, whose low two bits are zero) and `0x59000` is zero: in both
cases the renderer reads a head that says "not drawn" and stops. The trial aimed at the wrong half of the
Real3D's memory, and the port behaved correctly throughout.

### Done properly, it works

`VF3_CULL_COPY` puts Grace's first node -- ten words -- at culling RAM word `0x80000`, which the game never
touches (zero in the baseline, and its filled region ends at `0x79797`). The directory entry for the invented
id `0x1500` is set to `0x00080000`, and slot 1 of Picky's table names `0x1500`.

**Sixteen pictures of sixteen, byte-identical to Grace's own id.** The part is drawn from a node of its own,
in memory the game never wrote, reached by an id the game never had.

The three doors are open, and none of them is in the ROM:

| | where | what it takes |
|---|---|---|
| an id of its own | the directory, main RAM `0x003003CC` | one word, above id `0x14B1` |
| a node of its own | the culling RAM, above word `0x79797` | 2.6 MB free, nothing writes there |
| a slot that names it | a 45-slot part table | one word |

What this does **not** say is that a part from somewhere else would look right. Its nodes carry a model's
address in the VROM and its polygons name a place in the texture sheet; a part that is not already in this
machine needs both, and neither was tried here.

## 8. A model out of Virtua Fighter 3, drawn in Fighting Vipers 2

Section 7 adds a part the game does not have, out of parts the game does have. This section takes the geometry
from the other game on this port, and it works.

### Choosing one, measured rather than guessed

Virtua Fighter 3 runs on the same binary, so its culling RAM is dumped the same way. Its board is **Step 1.0,
so a node has no shift**: the child is `n[5]`, not `n[7]` (`src/video/r3d_scene.cpp`, `node_shift`). Read with
the wrong shift the sweep returns bytes like `0x1B1B1B`, which is how the mistake announces itself.

Read right, at frame 1500 of `tools/scripts/vf3-match.txt`: **22,196 distinct models in the VROM** under its
nodes, and 3,292 children into polygon RAM. Walking each one by the polygon rule the port uses -- seven words
a polygon plus `((w0 & 0x40) ? 4 : 3) - shared[w0 & 0xF]` vertices of four, until `w1 & 4` -- the biggest is
**1,429 polygons, 18,939 words, at VROM word `0x18ED02`**.

(Two frames of the fight were also dumped and diffed, which separates what moves -- the fighters -- from the
stage. Under the nodes that moved: 10 models in the VROM against 169 in polygon RAM, so **Virtua Fighter 3
builds most of a fighter each frame** where Fighting Vipers 2 reads his parts from the VROM. The model used
below is not one of those ten; it is a big piece of scenery, and it was chosen for being visible.)

### Putting it in the other game

Three levers, all in `tools/vf3run.cpp`: `--vrom-out` writes the model ROM so one game's can be read while the
other runs, `VF3_POLY_LOAD=word@file` puts a file of words into polygon RAM before every frame, and
`VF3_CULL_SET=addr=value,...` writes whole words into the culling RAM.

```
VF3_POLY_LOAD=60000@vf3_big_18ED02.bin          the model, in free polygon RAM
VF3_CULL_SET=80000=801A9012,...,80007=1060000,80008=1000000,80009=F819F819
VF3_POKE=3057CF=0,3057CE=8,3057CD=0,3057CC=0,   directory[0x1500] = 0x00080000
         7CB83=0,7CB82=0,7CB81=21,7CB80=0       Picky's slot 1 = 0x1500
```

The node at culling word `0x80000` is one of Fighting Vipers 2's own, with one word changed: its child is
`0x01060000`, type 1, the model at polygon RAM word `0x60000`.

**It is drawn.** Against the baseline the pictures move by 399 to 9,076 pixels, and the geometry is plainly
there: a long grey walkway across the arena at frame 4151, a panel in Picky's hands at 3401, in a fight that
is otherwise identical.

### Finding the fighters, and what happens to their models

The scenery model above was the biggest in the dump, not a fighter's. The fighters are found by structure
rather than by guessing. Virtua Fighter 3's scene is a chain of **five viewports** in the upper culling RAM
(`0x800070` onwards, each naming the next at `v[1]` and its tree at `v[2]`), and walking each tree separates
them at once:

| root | models in the VROM | models in polygon RAM | nodes |
|---|---|---|---|
| `8000A0` | 25 | **87** | 250 |
| `8000D8` | 80 | **82** | 329 |
| `800110` | 100 | 0 | 192 |
| `800180` | 317 | 0 | 490 |

Models in polygon RAM are the ones the game builds each frame -- the joints between a fighter's limbs. The two
roots that have them are the fighters; the two that have none are the stage. (The same split, counted another
way, is in the note above: the port already names those built models in `src/video/r3d_scene.cpp`.)

The biggest model under each: **586 polygons** at VROM `0xB15994`, and **93 polygons** at `0x82228C`. Both were
put into Fighting Vipers 2 exactly as the scenery model was.

**Neither comes out.**

- The 586-polygon one draws **nothing at all**: its pictures are byte-identical to the control that points the
  directory entry at zeros.
- The 93-polygon one leaves **the two fighters untouched to the pixel** and changes almost everything else --
  70 % to 87 % of the picture: the stage's girders, walls and water go dark, in a fight that is otherwise
  identical.

Neither is what it looks like, and the next section is the answer rather than the guess that came first.

### Why one draws and the others do not

Two leads were followed and closed before the answer came, and both are kept here because each cost a
measurement.

**Not the shared vertices.** Fighting Vipers 2's own scene was walked the same way -- sixteen viewports, Step
2.0 so the child is `n[7]` -- and every model it draws read in its own VROM (`--vrom-out` on this set):
**not one of its 249 models shares a vertex with the model before it**, fighters and stage alike. Nothing here
inherits anything from a foreign model.

**Not the vertex scale, though the scale is real.** This port reads a vertex differently on the two boards
(`src/video/r3d_scene.cpp`):

```cpp
vertex_scale = board.stepping > 0x10 ? 1.0f / 2048.0f : 1.0f / 128.0f;
```

Both fighter models were written again with every vertex multiplied by sixteen; the pictures did not move by
one pixel. (Why not is in the answer below.)

**Not the node either.** The same hand-made node, with its child pointed at one of *this* game's models in the
VROM, gives the baseline back to 683 pixels. So what changes the picture is the foreign model's own content.

**The answer is the models' own numbers.** Read in the units this board uses, with the span of each model's
vertices:

| model | vertices | size | textured | translucent | centred on its origin |
|---|---|---|---|---|---|
| VF3 `18ED02` | 2,234 | **2,193** | yes | no | yes |
| VF3 `B15994` | 903 | 25 | yes | no | **no: x runs 1,311 to 30,121** |
| VF3 `82228C` | 200 | 35 | **no** | **yes** | yes |
| FV2 `6D8CB9`, one of its own parts | 130 | **0.3** | yes | no | yes |

Three facts, and each explains one of the three results.

- **A Virtua Fighter 3 model is enormous here.** This game's own parts span 0.3 of a unit, because a Step 2.0
  node carries a scale word for what hangs below it; Step 1.0 has no such word, so Virtua Fighter 3's models
  carry their full size. `18ED02` therefore crosses the whole arena -- which is exactly the walkway in the
  picture.
- **`B15994` is not centred on its origin**: its x runs from 1,311 to 30,121, fifteen units to one side of the
  node it was given. It is drawn outside the picture, and multiplying its vertices by sixteen only sends it
  sixteen times further out. That is why the rescaling changed nothing.
- **`82228C` is the only untextured, translucent one of the three.** Enormous and centred, it is a tinted
  sheet over everything behind it: the stage goes dark, and the fighters, which are in front of it, do not
  change by a pixel. A uniform tint over the whole background is the same tint whatever the size, which is the
  other half of why the rescaling changed nothing. (`VF3_TRANS_AUDIT` exists in this port for exactly this
  shape of bug -- "finding the one that covers the picture".)

So the two games' models are compatible, and what a foreign one needs is not a format but **the size and the
place its own game's nodes gave it** -- which the next part builds.

### A node of our own, and a limb of Virtua Fighter 3 on a fighter of this one

The answer above says what a foreign model needs. Building it took one more thing out of the code, and a
false start worth keeping.

**`attr.scale` does not scale a model.** The node word that looks like a scale (`n[1]`, bit 0, read as
`as_float(n[1] & ~3u)`) is used in three places and **none of them is a vertex**: the culling radius
`float16(w7) * attr.scale`, the level-of-detail radius beside it, and dividing the normal matrix
(`src/video/r3d_raster.cpp`). Three runs at scales 0.004, 0.012 and 0.04 therefore drew **nothing at all** --
not because the model was small but because a shrunken sphere fails

```cpp
if (attr.no_cull || (!outside && lod_scale >= lod_del(3))) { ... }
```

**So the node must say "never cull me", and the size must go into the vertices.** The node is the same ten
words with `n[1] = 2` -- bit 1, `attr.no_cull` -- and the model is rewritten: every vertex centred on the
model's own middle and multiplied so that its span matches one of this game's parts (600 in the raw units,
which is the 0.3 of the table above).

| what was drawn | pixels against the baseline, frame 3851 |
|---|---|
| the model as it comes, never culled | 973 -- the baseline's own "part missing" |
| centred, span 600 | 1,007 -- there, a few dozen pixels of it |
| centred, span 1,800 | **6,179** |

At span 1,800 it is plainly on screen: a shape of Virtua Fighter 3's, 586 polygons, sitting on Picky's chest,
carried by his pose frame for frame, with Charlie and the clock untouched. It is **white and cream**, because
its polygons name places in a texture sheet that is this game's, not its own -- which is the limit section 8
already stated, now visible.

So a part of the other game can be put on a fighter here, and what it takes is: its words in polygon RAM, a
node that is not culled, its vertices fitted, a directory entry, and a slot that names it. Five things, none
of them in the ROM, and the textures still wrong.

### What that says, and what it does not

**The model format is shared.** The port parses both games' models with the same code and the same rule, and a
model lifted whole from one and handed to the other draws. What differs between the two boards is the **node**
(Step 1.0 against Step 2.0, the two extra words), not the polygon.

What this does **not** show:

- **Its textures.** The model names places in a texture sheet, and the sheet here is Fighting Vipers 2's. What
  is drawn wears whatever is at those coordinates.
- **Its place and its size.** Both come from the node it was given, which is Grace's; nothing here fits a
  foreign model to a fighter's skeleton.
- **A square of the sheet this game leaves alone.** Section 9 pays for a foreign part's colours with the
  host's at the same coordinates; which squares are free was not looked for.
- **The joint a part should hang from.** The placement above is by hand, not from the game's own skeleton.
- **The arithmetic inside each step.** The chain is named end to end; which matrix comes from which float of
  the ROM block, and what the blend's weight is weighted by, are not read.
- **What the byte blocks index**, and how the floats of `0281`/`0282` attach to them.
- **The conversion** from this to Real3D polygons, which is what carrying a character across means.
- **The conversion.** Nothing here turns those floats into Real3D polygons, which is what carrying a
  character across now means.
- **Virtua Fighter 3's own part tables** -- its equivalent of section 5's directory -- were not looked for.

### A whole character does not come out of the ROM half

Carrying one model across is section 8. Carrying a whole fighter was tried next, and it says something about
Virtua Fighter 3 that one model did not.

Its five viewports, counted at frame 1500 of a fight -- models in the ROM against models the game builds in
polygon RAM each frame:

| root | | models in the ROM | polygons | textured | built in polygon RAM | polygons | textured |
|---|---|---|---|---|---|---|---|
| `8000A0` | a fighter | 25 | 385 | **0** | **87** | **2,988** | 2,656 |
| `8000D8` | a fighter | 80 | 1,431 | 949 | **82** | **1,955** | 1,887 |
| `800110` | stage | 100 | 250 | 139 | 0 | 0 | 0 |
| `800180` | stage | 317 | 6,565 | 6,331 | 0 | 0 | 0 |

**Virtua Fighter 3 deforms its fighters every frame and leaves the result in polygon RAM.** The stage is all
ROM; a fighter is half ROM and half built, and one of the two is almost entirely built. So the ROM half alone
is not a character -- which is exactly what the first attempt looked like.

Taking both halves works as a transplant: walking the tree with the matrices from the game's own matrix table
(the viewport's word `0x16` says where it is), every vertex carried into the world and written back, gives
**175 models, 53,724 words, 33 squares of the sheet**, and they draw. But what it is, is **a statue in the
pose of that one frame**: the deformed half is a snapshot, not geometry a skeleton could move again. And 33
squares laid in the host's sheet take 33 squares of the host's own, which is most of what the picture then
shows.

So the honest conclusion of this section is a direction, not a result: **a character should be read out of
Virtua Fighter 3's ROM**, through its own part tables -- the thing section 5 found for this game -- and its
textures from wherever the sheet is uploaded from. Neither was looked for.

## 9. Its own colours, and what they cost

Section 8's part is drawn in white and cream because its polygons name places in a texture sheet that is this
game's. The places are in the polygons, so they can be read: **one 256x256 square of page 1**, at (384, 256)
and (512, 256), 128x256 each, format 0 (T1RGB5).

Read out of **Virtua Fighter 3's own sheet** at those coordinates (`--tex-out`, added for this), that square
is **a face: a fighter's head from the front and from the side**. So the 586-polygon model of section 8 is not
a limb at all. It is a head, which is why it carries that many polygons.

Read out of **Fighting Vipers 2's sheet** at the very same coordinates: rust and orange, the stage's own
materials.

`VF3_TEX_LOAD=page,x,y,w,h@file` lays a rectangle of 16-bit texels into the board's sheet before every frame.
With Virtua Fighter 3's square laid at `1,384,256,256,256`, the part stops being white and **takes its own
skin and hair**: 214 to 2,373 pixels a picture against the same run without it.

**And the rest of that difference is the cost.** The sheet is shared **by place**, not by object. Everything
of this game that was reading the same square now reads a face: in the pictures, the bike's frame and a knee
pad turn skin-coloured. Giving a foreign part its colours takes the colours of whatever lives at those
coordinates. Two ways out, neither tried: find a square this game does not use and rewrite the model's
polygons to name it, or hand the sheet to the pack machinery of `docs/fv2-hd-3d.md`, which already replaces a
texture by the content it holds.

### Size and place

The first fitting made the head **three times** a part of this game (span 1,800 against 600) and it swallowed
the fighter's chest. At **500** it is the size of one of his own parts and sits where a part sits.

Where it sits is still not where a head goes: it is on the shoulder. The node's translation is Grace's slot-1
offset with a lift added by hand (0.8, then 1.2 and 1.6 along the axis that raises it), not the joint the game
would use. **Fitting a foreign part to a skeleton is not done here**: what is done is putting it in, at the
right size, with its own colours.

So the whole of it, for a part that is not this game's: **its words in polygon RAM, a node that is not culled,
its vertices fitted, its texels in the sheet, a directory entry, and a slot that names it.** Six things, none
of them in the ROM.

## 10. Where Virtua Fighter 3 keeps its own, in the ROM

Section 9 ends on a snapshot. The question it leaves -- what the ROM holds, rather than what one frame holds
-- is answered here, and the answer has the same shape as section 5's.

### Step 1.0 has no Real3D DMA: the SCSI controller moves everything

`VF3_DMA_LOG` prints nothing on Virtua Fighter 3. What moves its data is the 53C810's SCRIPTS memory move
(`Scsi::exec`), which this port already implements and which `VF3_SCSI_LOG=1` now reports. Over a run to a
fight, **45,021 moves**, and every one of them reads main RAM:

| to | moves | bytes |
|---|---|---|
| `8C……` culling RAM, low | 1,984 | 974 KB |
| `8E……` culling RAM, high | 8,077 | 30.7 MB |
| `94……` the texture port | 85 | 14.7 MB |
| `98……` polygon RAM | 34,873 | 69.3 MB |

So the CPU stages everything in main RAM -- the big moves all come from `0x000A0000` -- and the SCSI pushes
it to the three memories of the Real3D.

### Who fills the staging buffer, and from where

`vf3run --watch A0000` names the writers; the one that matters is at PC `0x00054934`, and the loop around it
is a copy straight out of the ROM:

```
00054920  lis   r10,0xFF00
00054924  addi  r10,r10,-4        r10 = 0xFEFFFFFC, the banked ROM less a word
0005490C  lis   r8,0xFFA1         the table
00054910  rlwinm r5,r16,3,0,28    the id, times eight
00054914  lwzux r6,r8,r5          the entry's first word: how many words    (and r8 now points at it)
00054918  lwz   r7,4(r8)          its second: where
0005492C  add   r7,r7,r10
00054930  lwzu  r8,4(r7)          a word of the ROM
00054934  stwu  r8,4(r17)         into the staging buffer
```

`0x7C82286E` is **`lwzux`**, not `lwzx`: it advances the pointer, which is what makes the `lwz r7,4(r8)` after
it read the same entry's second word. Read as `lwzx` the table looks like one length and one fixed offset,
which is how that mistake announces itself.

### The directory, read in the ROM

**7,310 entries at banked CROM `0xFFA10000`**, eight bytes each -- a length in words and an offset -- with
the blocks laid end to end from `0xFF002000` to `0xFF7FA3CC`: **8 MB of resources**. 3,595 of the entries
carry a length; the smallest block is 23 words, the median 310, the biggest 20,183.

A second table of the same shape sits at `0xFFFFE000` in the fixed ROM, with its blocks at
`0xFFFE0000 + offset * 4`; in this set every one of its entries is `FFFFFFFF`.

### Which blocks are one fighter's: Akira is id 0x1057

The port's own `--trace-pc` lists what the program asks for, without any new code: the id is in `r3` at the
loader's door, and a run to a fight makes **1,434 calls for 1,240 distinct ids, from `0x0000` to `0x1C88`** --
the table's own range.

Crossing the ids with the move log's destinations says less than it promises: of 45,021 moves only 11,331 are
fed straight from a ROM block, and they carry **46 ids, all geometry**. The rest of the traffic comes from
buffers the CPU fills itself. So the ids are mostly **data the program reads**, which is what a deformed
fighter needs.

The ids a fighter costs are found the way `docs/fv2-two-kuhn.md` finds anything: by changing one thing.

**And the first attempt measured nothing**, which is worth keeping: `tools/scripts/vf3-match.txt` puts its
coin in at frame 600, and at frame 600 Virtua Fighter 3 is still showing its export warning. Its "select" and
"fight" marks are labels, not places; the run reaches the **attract mode**, and the two fighters found in its
culling RAM at frame 1500 are the demo's. Paid at frame 1600 instead, the select screen is there at 1890 with
the cursor on Akira.

From there, one cursor move at a time, with `--trace-pc 54890` naming every id the program asks the ROM for:

| the cursor | ids asked for |
|---|---|
| one cell right | `1051` (13,609 words), `1052` (2,980) |
| two cells right | those, and `104F` (20,183), `1050` (3,354) |
| three cells right | those, and `1053` (17,751), `1054` (1,971) |
| **two right, then two left, back on Akira** | **`1057` (15,547 words)** |

A fighter is **a pair of blocks**, and the pairs sit together in the directory -- `1049`/`104A`, `104B`/`104C`
and so on, each a big block and a small one. The cursor's order is not the ROM's, which is why the pairs come
out of order.

**Akira Yuki is id `0x1057`: 15,547 words, 60.7 KB, at banked CROM `0xFF48A7F0`** (his partner `1058` is
empty). The picture taken at that frame has him under the cursor, named on screen.

That is the thing section 9 said was missing: a character, in the ROM, at an address. What his 60.7 KB are
made of -- limbs, skeleton, the skinning the program applies -- is not read here.

### What is inside Akira's block, and the window that decides it

**Read through the wrong window, a block is a mirage.** The banked ROM is 128 MB seen through an 8 MB
window, `crom_bank = banked.data() + ((~v) & 0xF) * 0x800000`, and a dump read at window 0 gave id `0x1057`
the shape of a tidy tree: 6,691 addresses into one 56 KB range, records of {count, four addresses, a unit
axis}, and under them 1,581 points spanning 38 by 11 by 37 with most normals pointing up. It read as terrain,
and it was written down here as such.

`VF3_BANK_LOG` crossed with the trace says the program was never in that window: of **1,244 asks, 1,238 were
made with window 6**. The directory itself reads the same through either (it is duplicated), which is exactly
what made the mistake comfortable.

Read through window 6, Akira's nine blocks are:

| id | words | what the words are |
|---|---|---|
| `1057` | 15,547 | **53 % plausible floats, 32 % zero**, no addresses -- `-0.010`, `-0.060`, `-0.140` |
| `0281`, `0282` | 912 each | **74 % floats**, all small: `0.077`, `0.004`, `-0.012` |
| `0285`, `0286` | 128 each | packed bytes, not floats |
| `10C8`-`10CB` | 23 each | packed bytes: `2A 06 0C 11 | 19 20 2A 11 | 19 1C 20 2A` -- small, repeating, **indices** |

Floats for positions and normals, bytes for indices: the shape of a **skinned mesh**, which is what the
census above would expect of a fighter the program deforms every frame. Read as plain triples the big block
does not come out as a point cloud (its box runs to ±9,600), so it is not a flat array of vertices: something
-- a header, a stride, a per-limb division -- is in there that this page has not read.

**The lesson is the one that cost the terrain.** A resource's address is not enough; it takes the window the
program had chosen when it asked. Anything read out of this ROM without `VF3_BANK_LOG` beside it is a guess.

### The shape of the block: metres and degrees, and nothing else

There is no header and no table of offsets: swept for a word that could be an offset into itself, the first
256 give **none**, and there is no increasing run anywhere in them. Swept for a small integer -- a count, an
index, a length -- the whole 15,547 words give **not one** below 4,096.

What they are is **floats, and only floats**. 5,074 zeros, and of the 10,473 words left:

- **7,163 are small**, between -98.8 and 99.8, and read in threes they are body-scale points: the head of the
  block climbs `1.2997, 1.3739, 1.3800, 1.4400, 1.5100, 1.5300` with its other two components near zero,
  which is a column going up. Swept in stretches of 300 words, most of the block reads as such points, with
  y reaching 2.6 and never leaving ±4.
- **3,280 are big**, and they are **angles**: a third of them, 1,067, are a whole number of degrees at 65,536
  to the turn, and the commonest are **90°, -90°, 10°, 360°, 180°, -5°, -45°, -30°**. The values that stood
  out as odd constants -- 16384, 32768, 65536, 1820.44 -- are a quarter turn, a half, a whole, and ten
  degrees.

So Akira's 60.7 KB are **positions in metres and rotations in sixteen-bit degrees, written as floats**: the
numbers that move a skeleton, not a mesh and not polygons. Which agrees with the census that opened this
section -- the program turns these into geometry every frame, and the ROM holds no geometry of him for the
renderer to read.

What this does **not** say is which float belongs to which joint. There is no structure inside the block to
tell: the order is known only to the code that reads it, and that code was not followed here. The byte blocks
of his set (`0285`, `0286`, `10C8`-`10CB`) are the only indices in sight, and what they index was not read
either.

### Following the code: as far as the record, not yet as far as the joint

The block says nothing about its own order, so the order has to come from the code that reads it. Walked from
the other end -- from what the renderer is given, back towards the data -- the chain is:

**The matrix table is not where the animation is.** The viewport's word `0x16` points at it, and the SCSI log
says it is 144 bytes moved from main RAM `0x000EEC90`, once a frame, 1,563 times in a run. But
`vf3run --watch EEC90` catches **two stores in the whole run**: the table is written at startup and never
again. Twelve floats a matrix, 144 bytes, three matrices -- a fixed frame of reference, not a pose.

**What moves every frame is the node tree.** Five fixed addresses in main RAM feed the culling RAM, each
moved exactly once a frame: `000E0010`, `000EEC90`, `000EF680`, `000E3D90`, `000FA810`. So a fighter's pose
reaches the Real3D as nodes, rebuilt from nothing each frame, which is the same thing the census said from
the other side.

**And the builder is at PC `0x00020408`**, caught 1,563 times by `--watch E0010` -- once a frame, like the
move:

```
00020400  li   r9,48            forty-eight words
00020404  lwz  r0,0(r10)        from wherever r5 pointed
00020408  stw  r0,0(r2)         into the staging area
0002040C  addi r10,r10,4
00020410  addi r2,r2,4
00020418  bne  0x00020404
0002041C  addi r6,r6,48         and the next record forty-eight on
```

A record is **48 words**, which is six of this board's eight-word nodes. What the loop copies is already made;
what makes it sits above, in a dispatcher at `0x00020380` whose conditional calls are `0x0001FEB0`,
`0x00020224`, `0x00020268` and `0x0002027C`.

**That is where the floats become a pose, and this page stops at its door.** The chain is named end to end --
ROM block, directory, loader, staging, builder, record, node -- with one link unread: which of those four
routines takes which float, and for which joint.

### The four routines, and the one instrument that is missing

Opened, the four do not compute a pose. They **move floats out of the small-data area into the record**:

| | what it does |
|---|---|
| `0x00020268` | `lfs f1,-31900(r13)` then `stfs f1,0(r19)`, and a word at `+28` of another record -- one float, copied |
| `0x0002027C` | `lfs f5,-31896(r13)`, `lfs f6,-31892(r13)`, `lfs f7,-31888(r13)` -- **three consecutive floats**, then float arithmetic |
| `0x0001FEB0` | saves five floating registers on entry (`stfs f12`..`f16`): the heavy one |
| `0x00020224` | two helper calls (`0x2C20`, `0x2C8C`) and more of the same globals |

Three consecutive floats at `-31896`, `-31892`, `-31888` is an *x, y, z*. So a joint's value is a triple in a
**block of globals addressed from `r13`**, the small-data pointer, and these routines carry triples from there
into the 48-word records the builder then copies.

Which closes the shape of it: the ROM block holds metres and degrees; something fills the globals from it;
these four write the globals into records; `0x00020408` copies the records; the SCSI moves them to the culling
RAM; the Real3D draws them.

**And `r13` did not need an instrument after all**: the program sets it once, and the instruction is in the
image. Swept for `addis r13,r0` followed by `addi r13,r13`, the whole program gives **one** place, at
`0x001540`: **`r13 = 0x00108000`**. So the triple those routines read is at **`0x00100368`, `0x0010036C`,
`0x00100370`**.

Watched, that address is written **7,817 times in a run, by three writers through seven call sites, with five
distinct values**, 1,563 times each -- once a frame, five times over. It is **a scratch slot**, and the five
sites that fill it (`0x00015738`, `0x00015850`, `0x00015A08`, `0x00015AA0`, `0x00015ABC`) all do the same
thing:

```
li    r3,2
lwz   r2,-31960(r13)
oris  r2,r2,0x2000
stw   r2,-31960(r13)
stw   r3,-31956(r13)
lis   r3,0x0014
addi  r3,r3,4352        (and 4608, and 6144, one constant each)
bl    0x0001BE44
```

Each hands the same routine **an address in `0x0014xxxx`**, one per site. And those addresses hold, all three
of them, the same six floats: `0.0, 1.0, 0.7957, -0.5556, -0.2414, 0.55`, where
`0.7957² + 0.5556² + 0.2414² = 1.000` -- **a unit direction**. The whole 64 KB around them holds 88 non-zero
words, 85 of them floats.

**So this path is the lighting, and the 48-word records are viewports, not joints.** Five sites, five
viewports -- the same five the census at the top of this section walked from (`8000A0`, `8000D8`, `800110`,
`800148`, `800180`). The chain followed from the renderer backwards reaches the scene's furniture and stops
there; a fighter's joints are built on another path, which this page has not found.

### The other path: where a fighter becomes polygons

The viewport path above is the furniture. The fighter's own is found the same way, from the other end.

The census named the models a fighter has built in polygon RAM -- word `0x1A800` and its neighbours, bus
`0x9806A000`. The move log says what fills that address: **922 moves**, from main RAM `0x000ABC70` (449
times), `0x000A9210` (320) and `0x000AC0D0` (149). So a fighter's geometry is assembled in main RAM, in three
buffers, and pushed over.

`vf3run --watch ABC70` names the assembler: **PC `0x00035B68`, 321 times** (the ROM loader shows up there
too, 20 times, because the same area is also loaded into). And the instructions around it say what it is:

```
00035B44  stbx   r15,r31,r26
00035B58  stwbrx r3,r31,r26      a word, byte-reversed
00035B5C  addi   r26,r26,4
00035B60  stwbrx r4,r31,r26
00035B68  stwbrx r5,r31,r26
00035B6C  addi   r26,r26,8
```

A run of **`stwbrx`** -- store word byte-reversed -- stepping by 4, 8 and 16 through a buffer. Byte-reversed is
how everything bound for the Real3D is written (`docs/fv2-colours.md` found the same on the colour table), and
the strides are a polygon's.

And what feeds it, immediately above, is **floating-point arithmetic**: `0x00035A80` onwards is an unbroken
run of `fmadds`, `fmuls`, `fadds` (`EDED05B2`, `EC2F03F2`, `EC300C3A`...), ending in a call to `0x00034DA0`.

**So the conversion is at `0x00034DA0`-`0x00035B80`: floats in, Real3D polygons out, three buffers, once a
frame.** That is the routine a port would have to understand to carry a Virtua Fighter 3 character anywhere --
not a copy, as section 8 could do with one model, but this.

### The conversion, read

The floating-point instructions this port's disassembler prints as `.long` were decoded for this (opcodes 48
to 55, 59 and 63, and group 31's `stfiwx`). Read that way, `0x00035A50` onwards is not obscure at all:

```
lfs     f12,0(r27)   f13,4(r27)   f14,8(r27)     three floats: a vertex
lfs     f15,0(r28)   f16,4(r28)   f17,8(r28)     another
lfs     f20,0(r29)   f21,4(r29)   f22,8(r29)     a third

fsubs   f13,f13,f16     f22,f22,f17     f14,f14,f17      two edges, A-B and C-B
fsubs   f20,f20,f15     f12,f12,f15     f21,f21,f16

fmuls   f15,f13,f22     f16,f14,f20     f17,f12,f21      and their cross product
fnmsubs f15,f14,f21,f15
fnmsubs f16,f12,f22,f16
fnmsubs f17,f13,f20,f17

fmuls   f1,f15,f15                                        its length, squared
fmadds  f1,f16,f16,f1
fmadds  f1,f17,f17,f1
bl      0x00034DA0                                        and the reciprocal root

fmuls   f2,f2,f18                                         scaled
fmadds  f15,f15,f2,f29                                    and biased
fmadds  f16,f16,f2,f29
fnmadds f17,f17,f2,f29                                    (z negated: the board's handedness)

fctiw   f15 f16 f17                                       rounded to integers
stfiwx  f15,r19,r27     at +72, +76, +80
```

**It is a face normal**, taken from three vertices by cross product, normalised, scaled, biased and rounded.

Then the packing, and it names the rest:

```
lwz    r3,12(r27)  r4,16(r27)  r5,20(r27)      three words of vertex A
lwz    r6,12(r28)  ...                          of B
lwz    r19,12(r29) ...                          of C
lwz    r22,12(r30) ...                          of D          -- four vertices: a quad
lwz    r28,72(r27) r29,76 r30,80                the normal just computed

stwbrx r28,r31,r26                              the normal's x, byte-reversed
stbx   r14,r31,r26                              a byte put back over its low eight bits
addi   r26,r26,4
stwbrx r29,r31,r26                              y, and the same
```

Which is **the Real3D polygon header, word for word**: the port reads its words 1 to 3 as
`float(int32(h[k]) >> 8) / 4194304` -- a 24-bit normal with a low byte that carries flags, and here is the
code that writes exactly that, normal in the top 24 bits and the byte put back underneath.

So the answer to "which float becomes which vertex" is: **the floats are vertex positions, taken four at a
time, and the first thing the conversion makes of them is the polygon's normal**; the vertices' own packed
words are carried from `+12` to `+20` of each vertex record and written after it.

What is read here is the head of the routine. The rest of `0x00034DA0`-`0x00035B80` -- what fills those vertex
records in the first place, and where the ROM's metres and degrees enter -- is not.

### What feeds it: a record, an index list, and vertices of 224 bytes

The head of the same routine, at `0x000359F8`, says where its four vertices come from:

```
lhz   r3,42(r12)        how many polygons, from a record at r12
mtctr r3
lwz   r31,24(r12)       where the polygons go
lwz   r16,20(r12)       where the vertices are
lwz   r18,64(r12)       and the list of indices
lfs   f18,32000(r2)     a scale, from 0x00077D00
lfs   f29,5376(r0)      a bias, from 0x00001500
lhzu  r22,2(r18)        four sixteen-bit indices, one after the other
lhzu  r21,2(r18)
lhzu  r20,2(r18)
lhzu  r19,2(r18)
mulli r27,r19,224       each one times 224
mulli r28,r20,224
mulli r29,r21,224
mulli r30,r22,224
add   r27,r27,r16       added to the vertices' base
```

So the whole shape of it:

| | |
|---|---|
| a record at `r12` | `+20` the vertices, `+24` the output, `+42` the polygon count, `+64` the index list |
| the index list | **sixteen-bit, four per polygon** |
| a vertex | **224 bytes**, indexed by multiplication |
| two constants | a scale at `0x00077D00`, a bias at `0x00001500`, both read as floats |

Two hundred and twenty-four bytes is not a position: it is a **working record** -- the three floats the
cross product reads at `+0`, the packed words carried at `+12` to `+20`, the normal parked at `+72`, and
whatever else the game keeps per vertex.

Which leaves exactly one question where this page stops: **what fills those 224-byte records**, and where
the ROM's metres and degrees enter them. That is the skinning, one floor above, and it is not read here.

### One floor up: the vertices are blended, not placed

The vertex records had to be found before they could be watched. Swept for runs of 224-byte records whose
first three words are body-scale floats, main RAM gives **293 records at `0x00150FF8`, 277 at `0x001A5AD8`**,
and smaller runs after them. Watched:

- `0x00150FF8` and `0x001BDE9C` are written **once**, at the startup clear. They are rest geometry, left
  alone.
- `0x001A5AE4` is written **900 times in a run**, 447 by PC `0x00035F8C` and 447 by `0x000357E8`. That one is
  a fighter.

And the two writers say what the deformation is:

```
00035F50  fmadds f15,f16,f17        a matrix times a vector, row by row
00035F64  lfs    f12,0(r17)         where the vertex is now
00035F70  fsubs  f15,f15,f12        how far that is from where the matrix puts it
00035F7C  fmadds f12,f18,f15,f12    position += f18 x (target - position)
00035F88  stfs   f12,0(r17)
```

**A weighted blend**, not a placement: each vertex is moved a fraction `f18` of the way towards a
matrix-transformed target. The other writer does the same shape of thing and keeps more of it:

```
000357A0  lfs   f0,92(r12)          a scale out of the object record
000357A8  fsubs f20,f12,f15         the difference between two positions
000357B4  fmuls f20,f31,f20         scaled
000357C0  fmadds f1,f7,f8,f29       and its two sides, plus and minus a bias
000357CC  fnmadds f4,f7,f8,f29
000357E4  stfs  f12,0(r18)          the position at +0
000357F0  stfs  f20,24(r17)         the difference kept at +24
```

So a 224-byte record carries a position at `+0`, a difference at `+24`, the packed words the polygon takes at
`+12`-`+20`, the normal's scratch at `+72`, and more at `+108`-`+116`.

**Which closes the chain, floor by floor:** the ROM block's metres and degrees, the joints, a matrix, a
weighted blend into the vertex records, the cross product that makes each polygon's normal, the `stwbrx` run
that writes the Real3D header, the SCSI move into polygon RAM, and the renderer. Every step is named, with
the address and the measurement that named it.

What is still not read is the arithmetic inside each step -- which matrix comes from which float of the ROM
block, and what `f18` is weighted by.

## 11. What is not settled

- **The model's own format** was not read here, only its place. Nothing on this page parses a model; the port's
  renderer does (`src/video/r3d_scene.cpp`).
- **What a part brings with it.** A model's polygons name a place in the texture sheet, and the sheet is filled
  by the game. A part from elsewhere would need its textures there as well: not measured.
- **`0x00580E34`**, the fourth word that moves with a part id, a running total of some kind. Not chased.
- **The 1.14 MB the CROM copies into polygon RAM** (section 5). No node points there, and nothing here says
  what reads it.
- **A part from outside this machine, with its textures.** Section 8 draws a Virtua Fighter 3 model here; the
  textures it names are this game's, and nothing was done about that.
- **Virtua Fighter 3's own part tables.** Not looked for. Its fighters are largely built in polygon RAM each
  frame, which is not where Fighting Vipers 2 keeps its parts.
- **What the four 45-slot tables are.** Table 0 is what a fight draws; tables 1 to 3 were never reached, so
  "the armour states" is a guess, not a measurement. Their ids are in different bands from table 0's, which a
  plain armour variant would not need.
- **The four 24-slot tables.** Alone they move no geometry at all at frame 3300; on top of a body swap they move
  0.2 % to 1.7 % of the pixels. Something small, intermittent, and unnamed.
- **Whether a swapped body still fights the same.** The pose is identical and the opponent untouched, over
  sixteen pictures of one scripted fight. Reach, collision and armour live elsewhere and were not measured;
  a donor of another size may well not be fought the same way.
- **Both players.** Only player 1 was swapped.
