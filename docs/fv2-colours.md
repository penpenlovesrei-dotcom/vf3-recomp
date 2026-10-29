# Fighting Vipers 2: where a fighter's colour is, for the parts that have no texture

Kuhn's armour is chrome in one variant and gold in the other, and **no texture is involved**: his two
arcade entries bring zero textures of their own while the armour holds (`docs/fv2-hd-3d.md` and the
measurement below). A texture pack cannot touch him. This page says where that colour does live, read
in the code and checked at every step.

The short answer: a list of two colour pairs per fighter, in RAM, picked by `*1P COLOR`, copied into a
per-match table, and carried to polygon RAM by the Real3D's DMA. **Each fighter has exactly two
variants**, and that is a property of the data, not of the code that reads it.


## The chain

```
fighter record           0x00700500   (player 1; player 2 at 0x00707500)
   +184 = 0x007005B8     who he is
   +185 = 0x007005B9     *1P COLOR
        |
        v
fighter table            0x0007F9CC + id*4        120-byte records
   +116                                           -> his colour list
        |
        v
colour list + (*1P COLOR * 8)                     two 32-bit words
        |
        v                routine at 0x00022514, the store at 0x00022530
per-match table          0x003C2D04 / 0x003C2D08
        |
        v                Real3D DMA, bytes reversed
polygon RAM              words 0x400 / 0x401
        |
        v
the untextured polygons of his armour
```


## How each step was established

**The polygon RAM words.** Two runs of the same script differing only by the up press, polygon RAM dumped
at the same frame (`vf3run --poly-at`), and diffed. The first difference of the whole 4 MB is the answer:

```
word 0x00400   00FFFFFF -> 00CC9900
word 0x00401   00FFFFFF -> 00CC9900
```

Read as the rasteriser reads it -- R in bits 16-23, G in 8-15, B in 0-7 (`src/video/r3d_scene.cpp`) --
that is white against RGB(204,153,0), chrome against gold. Everything else that differs starts at word
0x801 and is geometry, because the two fights are not at the same instant.

**Why `--watch 98001000` saw nothing.** Polygon RAM is written through bus region 0x98, and the CPU's watch
only fires on CPU stores. The Real3D DMA writes it instead (`Model3::r3d_dma_write32`), which is also why
the value is stored **byte-reversed** upstream: the DMA may swap, and the bus write swaps again.

That reversal is what hid the colour at first. `0x00CC9900` appears nowhere -- not in main RAM, not in the
CROM, not in the VROM. `0x0099CC00` is in main RAM twice.

**The store.** `vf3run --watch 3C2D04`, on the interpreter, names it without ambiguity:

```
store32 003C2D04 = 99CC00 at PC 00022530 (LR 00022528)
```

**The routine**, disassembled from the RAM image:

```
0x022514  addis r18,r0,60        r18 = 0x003C0000
0x022518  addi  r18,r18,11524    r18 = 0x003C2D04
0x02251C  addis r3,r0,7
0x022520  lwz   r3,20764(r3)     r3 = [0x0007511C]   = 0x00700500, the fighter record
0x022524  bl    0x0225B8
0x022528  lwz   r4,0(r3)
0x02252C  lwz   r5,4(r3)
0x022530  stw   r4,0(r18)        [0x003C2D04] = first colour
0x022534  stw   r5,4(r18)        [0x003C2D08] = second colour
```

and the resolver it calls:

```
0x0225D0  mr     r18,r3          the fighter record
0x0225D4  addis  r3,r0,8
0x0225D8  addi   r3,r3,-1588     r3 = 0x0007F9CC
0x0225DC  lbz    r4,184(r18)     the fighter id
0x0225E0  rlwinm r4,r4,2,0,29    id * 4
0x0225E4  lwzx   r3,r3,r4        his record in the fighter table
0x0225E8  lwz    r3,116(r3)      his colour list
0x0225EC  lbz    r4,185(r18)     *1P COLOR
0x0225F0  rlwinm r4,r4,3,0,28    colour * 8
0x0225F4  add    r3,r3,r4
```

This also settles two addresses that were known only by Sega's debug names: `0x007005B8` and `0x007005B9`
are fields **+184 and +185 of the fighter record at `0x00700500`**, not loose variables.


## The colour lists: sixteen, contiguous, sixteen bytes each

| id | fighter | list | | id | fighter | list |
|---|---|---|---|---|---|---|
| 0 | GRACE | `0x0007FA0C` | | 8 | MAHLER | `0x0007FA8C` |
| 1 | BAHN | `0x0007FA1C` | | 9 | BM | `0x0007FA9C` |
| 2 | RAXEL | `0x0007FA2C` | | 10 | CHARLIE | `0x0007FAAC` |
| 3 | TOKIO | `0x0007FA3C` | | 11 | TECHNO (Emi) | `0x0007FABC` |
| 4 | SANMAN | `0x0007FA4C` | | 12 | DURAL | `0x0007FACC` |
| 5 | JANE | `0x0007FA5C` | | 13 | KUHN | `0x0007FACC` |
| 6 | HONEY | `0x0007FA6C` | | 14 | DEL SOL | `0x0007FADC` |
| 7 | PICKY | `0x0007FA7C` | | 15 | KUMA | `0x0007FAEC` |

**Ids 12 and 13 share one list**, which is the same fact as Dural being Kuhn's old name
(`fv2-arcade-notes/README.md`), seen from another side. What else those two records share, and the one
field where they differ, is `docs/fv2-two-kuhn.md`.

Kuhn's, at `0x0007FACC`, stored byte-reversed:

| `*1P COLOR` | address | first | second |
|---|---|---|---|
| 0 | `0x0007FACC` | RGB(255,255,255) | RGB(255,255,255) |
| 1 | `0x0007FAD4` | RGB(204,153,0) | RGB(204,153,0) |

Sixteen bytes per fighter, two pairs of eight. **There is no third pair**: `*1P COLOR = 2` reads Del Sol's
first pair, and beyond that whatever follows.

That is the explanation of a note in `docs/fv2-honey.md` that had been left as a curiosity -- poked to 2
the CPU stops at `0x00020908`, poked to 4 the whole screen changes. It was reading past the end of the
list, and far enough past it the pointer it builds is not a pointer.


## Poking it does not work, and that is not a detail

`VF3_POKE` writes before each frame; this routine runs during the frame. Measured: with the poke set over
frames 2500-4500, `0x003C2D04` still reads `FFFFFF00` at frame 3300 and polygon RAM word 0x400 with it.
The game wins every frame, by construction.

A value the game rewrites each frame is reached by **patching the code that writes it**, or by changing
what it reads -- not by writing the value.


## What a four-colour Kuhn would need

Two changes, both at addresses now known:

- **A list of his own.** The pointer at `[0x0007F7EC + 116]` -- and its twin `[0x0007F864 + 116]`, since
  ids 12 and 13 share the list -- repointed at a four-entry table placed somewhere free. Moving the colours
  in place is not possible: the next sixteen bytes are Del Sol's.
- **A `*1P COLOR` that reaches 2 and 3.** The select screen toggles it between 0 and 1 and nothing there
  produces more (26 runs, `docs/fv2-honey.md`).

The colours to put in them are measured: the Dreamcast gives Kuhn four skins, and its own flat 64x64
texture of rank 1 carries them -- RGB(49,48,65) grey, RGB(115,89,8) gold, RGB(0,20,106) blue, RGB(24,4,0)
near-black.


## What is not settled

- **Which polygons read which of the two words.** Both are the same colour in both of Kuhn's variants, so
  nothing here separates them. A fighter whose two words differ would.
- **The other fifteen fighters' lists were read but not looked at.** Nothing says their two pairs are used
  the way Kuhn's are; a textured fighter may not read the colour table at all.
- **The second pair at `0x003C2D0C`/`0x003C2D10`**, written by the same routine from `[0x00075120]`, is the
  other player's. Not checked.
