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

## 8. What is not settled

- **The model's own format** was not read here, only its place. Nothing on this page parses a model; the port's
  renderer does (`src/video/r3d_scene.cpp`).
- **What a part brings with it.** A model's polygons name a place in the texture sheet, and the sheet is filled
  by the game. A part from elsewhere would need its textures there as well: not measured.
- **`0x00580E34`**, the fourth word that moves with a part id, a running total of some kind. Not chased.
- **The 1.14 MB the CROM copies into polygon RAM** (section 5). No node points there, and nothing here says
  what reads it.
- **A part from outside this machine.** Section 7 adds an id, a node and a slot; a model of its own in the
  VROM, and the textures it names, were not tried.
- **What the four 45-slot tables are.** Table 0 is what a fight draws; tables 1 to 3 were never reached, so
  "the armour states" is a guess, not a measurement. Their ids are in different bands from table 0's, which a
  plain armour variant would not need.
- **The four 24-slot tables.** Alone they move no geometry at all at frame 3300; on top of a body swap they move
  0.2 % to 1.7 % of the pixels. Something small, intermittent, and unnamed.
- **Whether a swapped body still fights the same.** The pose is identical and the opponent untouched, over
  sixteen pictures of one scripted fight. Reach, collision and armour live elsewhere and were not measured;
  a donor of another size may well not be fought the same way.
- **Both players.** Only player 1 was swapped.
