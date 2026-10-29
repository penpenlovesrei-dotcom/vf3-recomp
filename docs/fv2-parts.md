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
- **Virtua Fighter 3's own part tables and where its sheet is uploaded from.** Without them a character can
  only be lifted as the snapshot above.
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

## 10. What is not settled

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
