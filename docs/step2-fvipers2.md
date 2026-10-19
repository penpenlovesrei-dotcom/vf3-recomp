# Fighting Vipers 2 on this port: Model 3 Step 1.0 to Step 2.0

Virtua Fighter 3 runs on a Model 3 Step 1.0 board; Fighting Vipers 2 (Japan, Revision A, 1998) on a Step 2.0
one. This is every difference the port had to carry, what was measured to settle it, and what is still missing.
Both sets run from the same binary: which board to be is read from the zip.

Supermodel is the reference build, and was read here only as documentation of the hardware (its address map, its
stepping table, its ROM layout in `Config/Games.xml`). None of its code is in this one. The clock and PLL figures
below come from the tables it quotes from Motorola's manuals (MPC603E7TEC page 23, MPC105UM/MPC106UM).

## What was measured first

The brief for this work said the program is a gzip in the CROM and that a boot stub unpacks it to RAM 0. That is
right, and it had already been established in `fv2-arcade-notes` (`gzip_crom.py`, measurement of 2026-08-22).
Checked again here against the CROM as this port lays it out:

- the fixed CROM's **offset 0** is a whole gzip file: magic `1f 8b`, method 8 (deflate), flag FNAME, original name
  `foo.bin`, compression date 26 March 1998 16:01:08 UTC, Unix;
- 897,285 bytes of gzip hold **2,701,982 bytes** of program, and the CRC32 in the gzip footer checks out;
- the three canonical DEFLATE tables sit in the boot stub at `0xFFF03604`, `0xFFF03642` and `0xFFF03680`.

That last point is the one that matters for the design: **the decompressor is the game's own code**, zlib's
`huft_build` compiled into the boot stub. The board does not need to unpack anything for the game to run — the
PowerPC does it. So nothing was added to the boot path. The loader unpacks the gzip anyway, but only to hand the
recompiler an image to build from (`program_image()`), exactly as Virtua Fighter 3's boot copy does.

Measured after the change (`vf3run fvipers2.zip --frames 120 --ram-at 120`): RAM 0 is **99.986 %** identical to
the gzip's content (2,701,595 of 2,701,982 bytes), the difference being four 64 KB blocks the game has written
since boot. The PC is at `0x000074CC` after 120 frames and `0x0000A0B0` after 1800 — in RAM, in the game's own
program, not in the stub.

## 1. The ROM set (`src/machine/romset.{h,cpp}`)

The loader used to be Virtua Fighter 3's alone. It now carries a table of sets and picks the one most of whose
ROMs are in the zip by CRC, so the same binary takes any of them. Each set says its own region sizes, its
stepping, its security key and whether its program is compressed.

Three sets are known: `vf3`, `fvipers2` (Japan, Revision A) and `fvipers2o` (Japan, the first release). The last
two differ only in their four program ROMs, so each set names its program ROMs separately from the data ROMs the
revisions share, and a program ROM counts for a hundred data ROMs when the loader decides which set a zip is —
the program is what tells one revision from another. `fvipers2o` has not been run: there is no dump of it here to
try. Nothing about it should be special, since the loader reads whichever gzip is in the CROM.

The CRCs are checked, so a set with patched ROMs is refused and the loader names what doesn't match
(`fvipers2p.zip` here, with sixteen patched texture ROMs, is turned away for exactly that reason). That is the
behaviour Virtua Fighter 3 Recomp already had; taking patched sets would be a choice to add, not an oversight.

| | `vf3` (Step 1.0) | `fvipers2` (Step 2.0) |
| --- | --- | --- |
| Program EPROMs | 4 x 512 KB = 2 MB | 4 x 2 MB = **8 MB** |
| Fixed CROM below them | banked CROM 0's first 6 MB | **nothing: the EPROMs fill all 8 MB** |
| Banked CROM | 4 x 16 MB = 64 MB, mirrored to 128 MB | the same |
| VROM (Real3D textures) | 16 x 2 MB = 32 MB, mirrored to 64 MB | 16 x 4 MB = **64 MB, not mirrored** |
| Sound program (68000) | 1 x 512 KB | the same |
| SCSP samples | 2 x 4 MB = 8 MB, mirrored to 16 MB | 4 x 4 MB = **16 MB, not mirrored** |
| Program in the CROM | as it stands, copied by the boot | **a gzip at offset 0, unpacked by the boot** |
| Security chip | none | **315-5881, key `0x29260E96`** |

(The `fvipers2o` column would be the `fvipers2` one throughout, with four other program ROMs.)

The mirroring is now one loop for every region, which gives both layouts without a special case. Two details of
the Fighting Vipers 2 set are easy to get wrong and are written out in the table: the four sample ROMs are **not**
in the order of their numbers (`mpr-20576`, `mpr-20578`, `mpr-20577`, `mpr-20579` at 0, 4, 8 and 12 MB), and its
VROM and samples fill their regions on their own, so mirroring them would overwrite the second half with the
first.

## 2. The CPU's clock (`src/machine/model3.cpp`, `Model3::load`)

Both boards run a **603R, and the same PVR** (`0x00071202`) — Step 1.0 was already configured as a 603R here,
because that is what the reference uses for it. What differs is the bus multiplier, and everything that follows
from it:

| | Step 1.0 | Step 2.0 |
| --- | --- | --- |
| Bus | 66 MHz | 66 MHz |
| Multiplier | 1.0 (passed doubled: 2) | **2.5 (passed doubled: 5)** |
| Core, as the core counts cycles | 66 MHz | **165 MHz** (2.5 x 66) |
| Core, as the frame is paced (`ppc_hz`) | 66 MHz | **166 MHz** |
| PLL configuration in HID1 | `0xF0000000` (no table entry for 1.0 at 66 MHz: -1) | **`0x60000000`** (0x6 for 2.5:1 at 66 MHz) |
| Decrementer ratio | 4 | **10** |

The two clocks are not a mistake. The reference paces a frame at a round 166 MHz while the core counts its
cycles at the multiplier's exact 165 MHz, and the decrementer's ratio comes from the two together
(`5 * 2 * 166 / 165` = 10, not 10.06). A new field `ppc_hz` carries the pacing clock, so `run_frame_start` and
`run_lines` pace a frame the way the reference paces it; on Step 1.0 the two numbers are the same and nothing
changes.

## 3. The PCI bridge: MPC105 to MPC106 (`src/machine/model3.{h,cpp}`)

Same registers, a different device ID, three more reset values, and its config ports somewhere else entirely.
`pci_model` holds which one the board has.

| | MPC105 (Step 1.x) | MPC106 (Step 2.x) |
| --- | --- | --- |
| Vendor / Device | 0x1057 / **0x0001** | 0x1057 / **0x0002** |
| CONFIG_ADDR | `F0800CF8` | **`FEC00000`-`FEDFFFFF`** |
| CONFIG_DATA | `F0C00CF8` | **`FEE00000`-`FEEFFFFF`** |
| Its own window at `F8FFF000` | writable | **ignored** (as the reference leaves it, at every access width) |
| Extra reset values | — | `0x0C` cache line size, `0x70` ODCR, `0xC0` error enabling 1, `0xE0`/`0xE8` ESCR1/2 |

The device ID is the point: the game's boot tells the two bridges apart by reading it. The ports are decoded from
the same `(addr >> 16) & 0xFF` the board already switched on (`0xC0` for the MPC105's data port, `0xE0`-`0xEF`
for the MPC106's), each gated on `pci_model`, so a Step 1.0 run sees exactly what it saw before.

`F8FFF000` is the one place where the reference and the brief for this work disagree: the brief said to keep it,
the reference ignores it on an MPC106 at all three access widths. This port follows the reference, because every
value the PowerPC can see here has to follow it for a run to be comparable frame by frame.

## 4. The rest of the memory map

| Address | Step 1.x | Step 2.x |
| --- | --- | --- |
| `C0000000`-`C00000FF` | the 53C810, when its base register points there | **nothing** (the net board's, which this port doesn't carry) |
| `C1000000`, `F9000000` | the 53C810's other two windows | **unchanged**: the reference leaves both on every stepping |
| `C2000000`-`C20000FF` | — | the Real3D's DMA registers |

Only the first of these was a change: `scsi_mapped()` now answers no above Step 1.5, which is the condition the
reference uses. The Real3D DMA port at `C2000000` was already here and needed nothing — Virtua Fighter 3's board
answers there too.

The Real3D's PCI device ID did need splitting in two. In config space it is Mercury's `0x16C311DB` on Step 1.x
and the DMA device's **`0x178611DB`** on Step 2.x; at the DMA port itself it is Mercury's `0x16C311DB` on every
stepping, which the reference notes games rely on even on a Step 2.x board.

## 5. The JTAG chain (`src/machine/model3.{h,cpp}`, `Jtag`)

Step 1.x has ten devices on the chain, Step 2.x **seventeen**, and every ID code differs. The chain is now built
by `Jtag::configure(stepping)` and every loop over it is bounded by `n_dev`, so TDO comes off the real last
device rather than a fixed slot 9.

| | Step 1.0 | Step 2.0 |
| --- | --- | --- |
| Devices | 10 | **17** |
| Jupiter | `0x116c7057` | `0x416c7057` |
| Mercury | `0x216c3057` | `0x416c3057` |
| Venus | `0x116c4057` | `0x316c4057` |
| Earth | `0x216c5057`, one of them | `0x416c5057`, **two** |
| Mars | `0x116c6057`, two of them | `0x316c6057`, **four** |
| 3D-RAMs | 4 | **8** |

Order on the chain: Jupiter, Mercury, Venus, Earth 0, a 3D-RAM, Mars 00, Mars 01, three 3D-RAMs, then — Step 2.x
only — Earth 1, a 3D-RAM, Mars 10, Mars 11, three 3D-RAMs.

## 6. What is Virtua Fighter 3's alone

Two things in the board are that game's own addresses, found in its test menu, and are now gated on the set so
another game can't trip over them: the coin settings in backup RAM (`coin_setting`, `set_coin_setting`,
`free_play`) and the FREE PLAY notices in the CROM (`hide_free_play_text`).

## 7. The 315-5881 security chip (`third_party/mame/315-5881/`)

Virtua Fighter 3's board has no security chip. Fighting Vipers 2's carries a 315-5881 keyed `0x29260E96`, and
measurement says the game uses it: **64 data reads and 3 register writes**, all in the first 120 frames and none
in the 1700 after — one block, read once at boot.

The chip's input is **not a ROM**: the game writes an encrypted block into the security board's RAM at
`0xF0180000`, points the chip at offset 0, writes its sub-key to `0xF01A0018`, and reads the plaintext a word at
a time from `0xF01A001C`. So the whole thing is self-contained, and `read_security_ram()` is all the wiring it
needs. The first read after an address write answers with a dummy word (`0xFFFF0000`), which is what the RAM-based
scheme expects and what the reference gives; every read after it takes the next word of the stream. The sub-key
is byte-swapped on the way in, the PowerPC being big-endian.

On the licence: MAME's `sega/315-5881_crypt.cpp` is **BSD-3-Clause** (Andreas Naive, Olivier Galibert, David
Haywood), not GPL — the same licence as MAME's 68000 this project already carries in `third_party/mame/m68000`,
and the README already says code from MAME keeps its BSD-3-Clause licence and notices. So it is carried the same
way, in `third_party/mame/315-5881/`, tables and algorithm unchanged, with MAME's `device_t`, save state and
memory maps dropped and the read delegate made a `std::function`. Nothing GPL was copied.

**It is right, and this is how that is known.** With `VF3_SECURITY_LOG=1`, the 64 words the game reads come out
as exactly 128 bytes of ASCII:

```
************* [[ Fighting Vipers 2 ]] in <<MODEL-3>> Copyright (C)1998 SEGA Enterprises,LTD.   All right reserved. *************
```

That settles the key, the block cipher, the stream header, the sub-key's byte order and the dummy first read all
at once: a wrong key or a wrong byte order gives noise, and the stream ends exactly where the game stops reading.

And it goes all the way through. After 120 frames the string is in the game's own RAM at **`0x9EC5`**, whole and
the right way round (`vf3run fvipers2.zip --frames 120 --ram-at 120`), so the chip, the data register, the game's
copy loop and the byte order are all right together, not just the chip on its own.

## 8. Virtua Fighter 3 is untouched, and that is measured

Every Step 2.0 branch is gated, which is an argument. This is the measurement, against a pristine build of the
upstream commit (`git worktree add /c/Temp/vf3up main`) on the same Revision D set:

| Check | Result |
| --- | --- |
| 600 frames, per-frame CPU state (`--state`) | **byte-identical** to upstream |
| 3000 frames, per-frame CPU state | **byte-identical** (1.5 MB of dumps) |
| 3000 frames, the whole 8 MB of RAM (`--ram-at`) | **byte-identical** |
| 1200 frames on the recompiled code (`--rec`) | **byte-identical**, and the same 78,252 instructions recompiled, 77,027 native, with the same interpreter-fallback counts |
| `vf3lock` over `tools/scripts/vf3-match.txt` (boot, attract, a coin, the fighter select, a round of random play) | **2231 frames identical**, interpreter against recompiled |

So nothing in Step 1.0 moved: not the interpreter, not the board, not the image the recompiler is built from.

`tools/scripts/` is new, and holds the two input scripts those runs use (`vf3-match.txt`, `fv2-match.txt`); the
repository shipped none. Fighting Vipers 2 runs its script's 2231 frames without a fault too, but the frames in
it were copied from Virtua Fighter 3's and have not been checked against what that game is actually showing.

## 9. The Real3D's geometry: why nothing 3D was drawn (`src/video/r3d_scene.*`, `r3d_raster.cpp`)

With everything above working, Fighting Vipers 2 booted, showed its WARNING screen, its title, took a coin and
reached SELECT YOUR VIPER -- all of it **2D**. Measured with `VF3_VP=1`: across a 2400-frame run, 176 viewport
dumps, every one of them full-screen with sensible fog and lighting, and every one **`models 0`**. Virtua Fighter
3 on the same build gives 2 and 5 models on its select and fight screens. So the scene builder was finding the
viewports and no geometry at all.

The cause is the culling node's size. `r3d_scene.cpp` already said it in a comment at the top -- "Step 1.0: 8
words; the later steps' 10, less words 1 and 2" -- but the parser only ever read the 8-word layout. On a 10-word
node every field after the first is two words further on, so the matrix index, the child pointer, the sibling
pointer and the radii were all being read out of the wrong words, and the walk found nothing to draw.

| | Step 1.0 | Step 1.5 and later |
| --- | --- | --- |
| Culling node | 8 words | **10 words** (two more after word 0) |
| Vertices | 17.7 (`/128`) | **13.11** (`/2048`) |
| Texture NP | 12.12 (`/4096`) | **10.14** (`/16384`) |
| Word 1 | — | **a model scale, and a flag that turns culling off** |
| Word 2 | — | **a texture offset (X, Y) and which texture sheet** |
| Light intensity | clamped at full | **not clamped** (games drive brightness past 100 %) |

`node_shift` (0 or 2) now carries the layout, and `build()` sets it, the two scale factors and the viewport's
`intensity_clamp` from the board's stepping. The two extra words feed `attr.scale`, `attr.no_cull`, `attr.page`
and a new texture offset on the model, which the rasterizer adds to the mesh's place on the sheet -- the first
three were already there and simply never set, Step 1.0 having no word to set them from.

**What it gave:** `models 0` everywhere became up to **218** in a frame, and the attract's 3D demo, which had
been a black screen with INSERT COIN over it, draws its scene; the select screen draws its fighter and the
portrait strip.

**And Virtua Fighter 3 is untouched by it:** rendered side by side with the upstream build on the same three
scripted frames (select, and two in a fight), the PNGs are **byte-identical**. Everything above is behind
`node_shift`, which is 0 on Step 1.0.

## 10. The white veil on one frame: a flash, not a fault

The scene draws, but a milky white veil lies over it. Four things were measured on one frame (1800, the attract's
oil rig), and three of them cleared suspects rather than finding one:

1. **Not the intensity clamp.** Taking it off above Step 1.0, which is what the reference does, moved 30 % of the
   pixels and the mean brightness only from 164.8 to 169.0. The scene was already too bright with it on. (It is
   nearly a no-op here anyway: the CPU rasterizer clamps the colour to 1 straight after the lighting.)
2. **Not the ambient or the sun.** Read side by side with the reference, the viewport node is parsed *identically*
   -- sun X from word 5, Y from `-word 6`, Z from `-word 4`, intensity clamped 0-1 from word 7, ambient from word
   0x24's bits 8-15. The values the game asks for, ambient 0.800 and sun 1.000, are what the reference would use
   too. Forcing the ambient to 0 (`VF3_AMBIENT=0`) darkens the scene and **the veil stays**.
3. **Not the fog.** At that frame the viewport asks for no fog at all: colour 0,0,0, density 0, start 0.
4. **It is the translucent polygons.** With the two translucent layers left out (`VF3_NO_TRANS=1`) the picture is
   crisp: deep blue sky, red-brown girders, the fighter clear. With *only* those layers (`VF3_ONLY_TRANS=1`) the
   sea's water polygons show, correctly textured -- and **88 % of the frame is a flat rgb(143,143,143)**. That is
   a white polygon covering the screen at 143/255, which is the alpha a translucency field of 18 gives
   (18 x 255 / 32 = 143).

So a full-screen white quad is being drawn at 56 % over everything. Its header is not being misread: the port's
polygon alpha and transparency are character for character the reference's (`PolyAlpha`, `Transparency`, both of
which take bit 23 as opaque and bits 18-23 as the amount).

**And the quad is meant to be there.** `VF3_TRANS_AUDIT=1` names it: viewport 1, model `DF0F6D`, one mesh of 24
vertices, **untextured**, colour `FFFFFF8F` -- alpha 0x8F = 143, exactly the grey measured -- and its box in the
camera's space is x -1..1, y -1..1, z about 0. A unit quad sitting on the camera, white, half opaque: a
full-screen flash. Frame 1800 is one of the attract demo's lighting flashes, and the sample was simply taken
during it.

Sampled every 40 to 60 frames across the demo instead, the colours are right: TOKIO on the platform has deep blue
sea and a clean red and white costume; Charlie's roll-call is a dark scene with a yellow spotlight cone and no
wash at all; the rig's girders keep their red-brown. The demo is a character roll-call built out of heavy
lighting effects, and this port is drawing them.

**So there is no washed-out bug.** What section 10 started out chasing was the renderer doing its job on a frame
that is genuinely overexposed. What remains true is only the last line below: none of it has been compared with
a reference.

Three switches were added for this and stay in, since the next person will want them:
`VF3_AMBIENT=<n>` forces the ambient light, `VF3_NO_TRANS=1` drops the translucent layers, `VF3_ONLY_TRANS=1`
shows them alone, and `VF3_TRANS_AUDIT=1` lists every translucent mesh with where it came from, whether it is
textured, its colour and alpha, and its box in the camera’s space -- which is what identified the flash.

## 11. The VROM texture port: the last thing drawing the wrong pictures

With the geometry right, the select screen still drew the wrong textures in its portrait strip -- stage walls and
doors where the fighters' faces belong -- and its countdown was missing. That is the texture port at
0x90000000-0x9000000B, and the two steppings do not use it alike at all:

| | Step 1.0 | Step 1.5 and later |
| --- | --- | --- |
| What a write means | a VROM word address | **three words: the address, the texture's header, then a write that sends it** |
| Where the length comes from | the first word at that address in the VROM | **the header** |
| How the data travels | the words are pushed into the texture FIFO and unpacked at the frame's start | **the Real3D fetches it itself, nothing goes through the FIFO** |
| Where it reads from | the VROM | **polygon RAM for the first 4 MB of the space, the VROM above it** |

The port was only ever doing the Step 1.0 thing, so every one of Fighting Vipers 2's uploads was read as an
address with a length taken from whatever happened to sit there. `vrom_fifo` and `vrom_fifo_n` were already
declared in the board and reset, and never used -- the same half-laid groundwork as `attr.scale` and
`attr.page`; they hold the two words now.

With that in, the strip shows the fighters' portraits and the countdown is back. Rendering the whole match
run again and comparing it frame by frame with the one before, **only the three select-screen pictures
changed**; the stage load, the round intro and every frame of the fight are byte-identical. So this game
uses the VROM port for its portraits and the direct texture FIFO at 0x94000000, which already worked, for
the stage and the fighters.

## 12. A match, start to finish

`tools/scripts/fv2-fight.txt` takes the game from the attract to a round: two coins, start, the cursor moved two
places along the portrait strip, the pick confirmed with **punch** (the select screen says PRESS START BUTTON,
but it is an attack button that takes the fighter), then random play through the round. The board's input
register is the Model 3's ordinary fighting layout, which is what this game declares -- punch on bit 0 of P1's
register at 0xF0040008, kick on 1, guard on 2, escape on 3, and the stick on bits 4-7, all active low.

It goes all the way. 4201 frames, no fault, and the pictures say the rest: Emi animating on the select screen,
the cursor moving to PICKY with the portrait lit, then **Cross River Damsite, ROUND 1, v.s. CHARLIE** -- the
stage with its water, fences, crates and red mat, both fighters in 3D with their armour, the health bars, the
round timer counting down, STAGE 1 and the clock, and shadows on the grating. Through the round the camera
tracks the two of them and the timer runs from 29 down.

So the whole chain is working together on a Step 2.0 board: the gzip boot, the MPC106, the security chip, the
Step 2.x culling nodes, the tile generator over the Real3D, and the inputs.

## 13. The recompiler: an address map of its own

The recompiler walks the program from an address map -- the places code is entered at -- and compiles everything
the walk reaches; anything it has no code for falls back to the interpreter, so an incomplete map costs speed,
not correctness. Virtua Fighter 3's map (`src/rec/vf3_map.cpp`) was made by `tools/recomp/mkmap.py` from runs of
an instrumented Supermodel, and it reads the program straight out of the CROM. Neither helps here: Fighting
Vipers 2's program is the gzip the boot stub unpacks, and there is no instrumented reference build on this
machine.

It does not need one. A map only has to hold the words the program is **entered at other than by falling through
from the word before** -- call targets, the targets of indirect jumps and returns, interrupt entries. Everything
else runs in a straight line from one of those, and a straight line is exactly what the recompiler's `analyse()`
walks. That set is trivially collected by this port's own interpreter: in `Cpu::execute`, when the next word to
run is not the last one plus four, mark it. Nothing about the CPU's behaviour changes, and the collection is off
unless asked for.

- `vf3run --coverage FILE` turns it on and writes the addresses (the recompiler would hide most of them behind
  its native code, so it refuses to run with `--rec`).
- `tools/recomp/mkmap_run.py fv2 src/rec/fv2_map.cpp <coverage files...>` merges any number of runs into the
  map, so a later run can only add to it.
- `ppcrec::map_for(set)` picks a game's map, and `Rec::build` takes it; `vf3run`, `vf3lock` and the windowed
  host all pass it.

One coverage run of `tools/scripts/fv2-fight.txt` -- the attract, a coin, the fighter select and a round --
gave **7196 entry points**, between 0x100 and 0x74B70. From those the recompiler builds **69,716 instructions,
68,771 of them native**, in about 120 ms, and reports **nothing "without code"**: the map covers everything it
was asked to run.

**The lockstep passes.** `vf3lock` runs two boards side by side, one interpreting and one on the native code,
and compares the CPU state, RAM, backup RAM and the Real3D's and tile generator's memories after every frame:

| Script | Result |
| --- | --- |
| `fv2-match.txt` (attract, coin, select, random play) | **2231 frames identical** |
| `fv2-fight.txt` (the above, plus a fighter picked and a whole round against the CPU) | **4201 frames identical** |

That is the brief's criterion for this phase met on both the attract and a match.

## 14. The windowed host: the game as it is played

Everything above was measured with the headless tools. The windowed build (`build/bin/vf3`) had never run here
at all, SDL 3 not being installed; `pacman -S mingw-w64-x86_64-sdl3` in MSYS2 is all it needed. With it,
Fighting Vipers 2 runs in the real app: the window, the RmlUi menu, the sound board, the inputs, the upscaling,
the picture shapes and the 3D on the GPU.

**Direct3D 12 has now been tried on real Windows hardware**, which this project's README said it had not been.
On a GeForce RTX 3070 Laptop, SDL_GPU picks `direct3d12` and the host's own start-up check -- its GPU picture
against its CPU one, before the GPU is trusted -- keeps the GPU's for both games. For Virtua Fighter 3 it passes
without a word. For Fighting Vipers 2 it reports the difference and accepts it: of 1,047,551 pixels of 3D,
**73,299 differ, 3,640 of them by more than 8, by 97 at most**. `VF3_GPU_CHECK_DIR=<dir>` writes the two
pictures to compare by eye.

Measured on that machine, with the recompiler and the GPU:

| | Virtua Fighter 3 | Fighting Vipers 2 |
| --- | --- | --- |
| Recompiled | 78,252 instructions | 69,716 |
| The boot, unseen | 960 frames in **1.4 s** | 960 frames in **15.2 s** (its gzip unpacked by the game) |
| Running | full speed | **about 49-51 frames a second** against the arcade's 57.5 |
| A game frame | | 14-15 ms |
| A picture (GPU) | | 2.1-4.2 ms, of which the renderer 0.9-1.3 |
| Smooth motion's in-between | | 0.15-0.46 ms |

So this game does not quite reach full speed on this laptop, which is no surprise: its PowerPC is clocked at
2.5 times Virtua Fighter 3's and does that much more work a frame.

What had to change in the host, all of it for the second game and none of it touching the first:

- **The NVRAM is per game.** The backup RAM and EEPROM were saved as `backup` and `eeprom` in the data folder
  whatever was running, so Fighting Vipers 2 would have read Virtua Fighter 3's settings at the start and
  written over them at the end. Virtua Fighter 3 keeps those names, so saves made before this port took a second
  game still load; another game gets its own (`backup-fvipers2`).
- **The window says which game it is**, from the set's title rather than a fixed string.
- **The play style.** Where a game keeps its coin settings, and where its FREE PLAY notices are, were found in
  Virtua Fighter 3's test menu. For another game there is nothing to hide and nothing to set, so the PC way
  takes coins as the arcade does and says so once, and the 300-frame probe for a coin setting is skipped.
- **The stage sky's lead** (`sky_lead.h`) is how far Virtua Fighter 3's backdrop trails its camera -- 3 frames,
  1.11 across and 1.08 up -- measured on that game's stages. Another game scrolls its own way, so it is left
  alone and its backdrop simply moves as the game moves it.

Widescreen works in the attract: at 16:9 and 21:9 the frustum really widens, showing more of the rig and the sea
to the sides rather than stretching. **In a fight it does not engage** -- the picture stays 4:3 with bars. The
host widens only on what it recognises as a stage, and that recognition is tuned to Virtua Fighter 3; Fighting
Vipers 2's fights, with their health bars above and their caption below, do not match it. That is the next thing
to look at, and nothing is wrong with the picture meanwhile.

### What the regression check now covers

Writing the host changes above broke Virtua Fighter 3, badly: an `else if` added to the coin block swallowed the
`game.reset()` that followed it, so the board was never reset after its coin settings were applied, and the
picture came out in torn bands. **None of the checks that existed caught it** -- the state, the RAM, the
rendered shots and the lockstep were all still identical, because none of them runs the host. `verifier-vf3.cmd`
now runs five checks, the last two new:

| Check | |
| --- | --- |
| The PowerPC's state, frame by frame | 3000 frames |
| All 8 MB of RAM | at the last frame |
| The pictures `vf3run` renders | `tools/scripts/vf3-shots.txt` |
| **The game frame the windowed host draws** | `host_last.png`, which is reproducible run to run (`host_window.png` is not: it follows the real-time pacing) |
| The lockstep, interpreter against recompiled | 2231 frames |

All five say IDENTIQUE. `jouer-fv2.cmd` opens the game in a window.

## 15. Widescreen in a fight, and two things the GPU was not doing

**Widescreen in a fight.** The host widens the picture only on what it recognises as a stage, and that test is
Virtua Fighter 3's. `VF3_STAGE_AUDIT=1` prints what it saw, and on a Fighting Vipers 2 round the answer was
exact: the 3D is drawn full width, the 2D covers 8-9 % (the limit is 50 %), and the one failing test is
**26 rows with 2D at the 4:3 edges against a limit of 19** -- the player icons this game puts hard against each
edge above the health bars. Virtua Fighter 3's HUD never reaches the edges, which is why 19 was enough.

That allowance is now per game (`stage_edge_divisor`): 20 for Virtua Fighter 3, which leaves its test written
exactly as it was, and 8 for anything else. What really tells a stage from a picture is the other two tests, the
3D full width and the 2D under half. With it, a round is drawn across the whole 16:9 or 21:9 picture.

**The texture offset was only on the CPU.** The culling nodes' texture offset (section 9) was applied in the CPU
rasterizer and not in the GPU renderer, which read the mesh's place on the sheet raw. In a fight that gave one
fighter the other's textures -- Charlie wearing Picky's blue armour on the GPU while the CPU drew him in his own
green and purple with his bike.

**The light's clamp was only on the CPU too.** `r3d.hlsl` clamped the light reaching a surface at full whatever
the board, while the CPU stopped clamping above Step 1.0 (section 9), so the GPU's picture came out duller --
which is how it was noticed. The shader takes the flag in `light.z` now.

Measured on one frame of a match, the GPU against the CPU:

| | Before | After |
| --- | --- | --- |
| Mean brightness (CPU 99.3) | 84.7 | **99.3** |
| Sampled pixels differing by more than 8 | 69,571 | **5,158** |

What is left differs by 8 or less: a GPU and a software rasterizer rounding and filtering differently.

Rebuilding `src/gpu/r3d_shaders.h` needs `tools/gpu/shaders.sh` with SPIRV-Cross (MSYS2 has it) and a DXC that
can generate SPIR-V -- the one in the Windows Kits cannot, the release from
`microsoft/DirectXShaderCompiler` can.

Virtua Fighter 3 is untouched by all of it, measured: the five checks of `verifier-vf3.cmd` and, since the
shader changed, its picture on the GPU as well -- all byte-identical to the upstream build.

## 16. The sound board

Nobody had listened to this game on the sound board (MAME's 68000 and two SCSPs, as Virtua Fighter 2 Recomp has
them). `vf3run --wav FILE` writes what it plays, and over the whole fight script it plays: 73 seconds of stereo
at 44.1 kHz, silent for the first 17 while the boot unpacks the program, then steady from the attract through
the coin, the select and the round.

Nothing in the board needed changing. The two things that could have:

- **The samples are banked, and this game fills them.** The 68000 sees 8 MB at a time at 0x800000-0xFFFFFF, with
  bit 4 of the register at 0x400001 choosing the upper half. Virtua Fighter 3 has 8 MB of samples mirrored into
  16; Fighting Vipers 2 has four ROMs filling all 16, so it is the first set here that uses the bank. It was
  already right.
- **The MIDI path**, the program ROM's size and the board's timing are the same on both steppings.

What did need a word is the menu's split between music and effects. The two volumes are applied per voice, and
which group a voice belongs to is read out of **Virtua Fighter 3's sound driver**: a record per slot at 0x400 in
each SCSP's RAM, the music on channels 0x00-0x0E and the effects, voices and announcer above. Another game's
driver puts something else there, and reading it could sort voices into the wrong group by coincidence, so
`SoundBoard::driver_known` is now set from the ROM set: off for anything but Virtua Fighter 3, where every voice
counts as the music and the two sliders move together. The sound itself is unaffected.

Measured side by side over each game's own script:

| | Virtua Fighter 3 | Fighting Vipers 2 |
| --- | --- | --- |
| Peak (of 32767) | 8313 / 11221 | 4004 / 3666 |
| RMS | 909 / 954 | 497 / 486 |

So this game plays about 6 dB quieter than the other. Whether that is its own mix or something this port gets
wrong is not settled: there is no reference recording of it on this machine to compare with, and the levels are
well within what two games can differ by. Worth a listen against the real thing before anything is changed.

## 17. Against the reference, frame for frame

Everything up to here was this port against itself. `tools/ref` is the piece that settles it: a patch that adds
coverage and scripted input to Supermodel, and `SM_STATE`, which writes the CPU's state and a hash of RAM after
every frame in exactly the format `vf3run --state` writes, so `tools/statecmp.py` compares the two.

It builds on this machine. Supermodel at trzy/Supermodel **c65edb1** (the commit the patch is made against),
the patch applied clean, built with `Makefiles/Makefile.Win32` against MSYS2's SDL2, GLEW, zlib, libpng and
SDL2_net -- all already installed. `runcov.sh` needed one line: it ran the game under Xvfb on anything that
wasn't a Mac, and Windows has no Xvfb but does have `SM_HIDDEN`, which is what the Mac branch uses.

**Both scripts match the reference exactly:**

| Script | Frames | Result |
| --- | --- | --- |
| `fv2-match.txt` -- boot, attract, a coin, the fighter select, random play | 2231 | **identical** |
| `fv2-fight.txt` -- the above, plus a fighter picked and a whole round against the CPU | 4201 | **identical** |

Every one of the 128 words a frame: all 32 general registers, all 32 floating-point registers, CR, XER, LR, CTR,
MSR, the next PC, SRR0/1, the decrementer, the time base, FPSCR, the four SPRGs, HID0/1, the interrupt state,
and a hash of all 8 MB of RAM. Over 6432 frames between them, not one word differs.

That is the whole chain checked at once -- the gzip boot, the 166 MHz clock, the MPC106, the security chip, the
Real3D's registers, the tile generator, the inputs and the interrupt timing -- against a build that is not this
one. It does not check the pictures or the sound, which are this port's own code and have no counterpart in the
reference's state dump.

The reference's own count of the code this game runs, `00000100-00074B7C`, also sits right on top of the address
map built from this port's coverage (section 13): 7196 entry points between 0x100 and 0x74B70.

Nothing from the reference build is in this port. The patch is GPL and lives in `tools/ref` for making
measurements with; Supermodel's source was read as documentation of the hardware and nothing was copied from it.

## 18. A package that stands on its own

The last piece of phase 6. Two things were in the way.

**The program only ever looked for `vf3.zip`.** Its first-launch search, the ROM folder it reads, the copy it
keeps for the next update and the file picker all named that one file, so a package for the other game could
never find it. The loader already knows its sets, so it now says which file names they go under
(`romset::known_zip_names()`), and the host tries them in that order -- Virtua Fighter 3's first, which leaves a
folder holding `vf3.zip` behaving exactly as it did. A set the player picks is filed under its own set's name,
and the dialogs name both games. Measured: with nothing but `fvipers2.zip` in its ROM folder and no argument on
the command line, the program starts Fighting Vipers 2.

**There was no way to build a package here.** `tools/win/build.sh` cross-compiles from a Mac and rebuilds SDL3,
FreeType and zlib from tarballs; this machine has MSYS2, which has them already.
`tools/win/package-msys.sh` is the native equivalent: it builds, copies the program, then copies every DLL the
program loads from `/mingw64/bin` and everything those need in turn until nothing new turns up, adds the assets,
a README naming both games, the licences and a `portable.txt`, and zips it.

**It was tried the only way that proves anything:** unzipped into a folder that had never seen it, with
`fvipers2.zip` dropped in its `roms`, and started in an **empty environment** -- `env -i`, nothing on PATH but
Windows' own system32, no MSYS2 anywhere. It ran: portable mode, the ROM folder found, 69,716 instructions
recompiled, Direct3D 12 on the GPU, first picture 1.06 s after the start, 1300 frames and a clean quit.

The package is 24 MB unzipped, 9.3 MB zipped: the program (9.9 MB), SDL3 and fifteen support DLLs, the assets,
README.txt, LICENSES.txt.

One thing it does not do: the banner in `log.txt` still says "Virtua Fighter 3 Recomp", the name of the project
this is a fork of. Renaming the project is not this work's to do, so the package is called Model3Recomp and its
README says which two games it runs.

## 19. Why it ran slowly: three words

Played from the package, the game was slow. The log said why, once the right number was looked at: a game frame
took 18.9 to 20.9 ms against the 17.4 a 57.5 Hz frame allows, while a picture took 3 to 4 ms. The GPU was not
the problem; the emulation was.

`vf3run --rec` put it beyond doubt:

| | Virtua Fighter 3 | Fighting Vipers 2 |
| --- | --- | --- |
| 1500 frames | 1.6 s (913 a second) | **24.7 s (61 a second)** |
| instructions the interpreter ran "before the boot copy" | 67 million | **4,284 million** |

The recompiler was never running. It is turned off whenever a word it compiled no longer matches RAM, and
`enabled()` is all-or-nothing on purpose: the native code is a straight-line translation, so an instruction can
be reached by falling into it from the one before, and refusing just that one word would not stop it running.

A counter added to the recompiler (`mismatch_count`, `mismatch_addresses`) named the words: **three of them,
at 0x1000, 0x1100 and 0x1200** -- the 603e's three TLB-miss exception vectors, which this game fills with its
own handlers once the boot has unpacked it. `analyse()` seeds every exception vector, so those words were
compiled from the gzip image, which is the program as it is *before* the game patches it.

So once the boot is over, the recompiler is built again from **RAM as it stands**
(`Model3::ram_program_image`), which does agree with it. Virtua Fighter 3 patches nothing and never reaches
that line.

| Fighting Vipers 2, `vf3run --rec` | Before | After |
| --- | --- | --- |
| 1500 frames, in all | 24.7 s | 17.4 s |
| frames 2000 to 3000, after the boot | about 62 a second | **about 385 a second** |

The boot itself is unchanged and still takes about 15 s: it unpacks the gzip before any of its program is in
RAM to be recompiled, so it is interpreted, and 960 frames at 57.5 Hz is 16.7 s of the game's own time anyway.

Checked after the change: Virtua Fighter 3 still identical on all five counts, and Fighting Vipers 2 still
identical to the reference over the whole fight script's 4201 frames.

## What is still missing

- **The pictures are still not compared with the reference.** The CPU's state and RAM now are, over 6432 frames
  (section 17), but what gets drawn and played is this port's own code and does not appear in the reference's
  state dump. Upstream has 173,000 frames of state for Virtua Fighter 3; this game has 6432.
- **The sound has not been compared with a reference either.** It plays, and over each game's own script this
  one is about 6 dB quieter than Virtua Fighter 3 (section 16). Whether that is its own mix is not settled.
- **`fvipers2o` has never been run.** The set is in the loader and should need nothing special, the loader
  reading whichever gzip is in the CROM, but there is no dump of that revision here to try.
- **The menu's music/effects split does nothing for this game** (section 16): which voice is which is read out
  of Virtua Fighter 3's sound driver, so for anything else the two sliders move together.
- **The boot takes 16 seconds of game time** unpacking the gzip, and the host runs it unseen. That is the game's
  own boot, not slow emulation -- 960 frames at 57.5 Hz is 16.7 s, and this port does it in 15.2 -- but it is a
  long wait, and the recompiler does not cover it: the stub runs in the CROM, and only the program in RAM is
  recompiled.
- **Only the Windows package is done** (section 18). `tools/mac`, `tools/deck` and the cross-built
  `tools/win/build.sh` still name and ship one game.

## Building and running here

SDL 3 comes from MSYS2 (`pacman -S mingw-w64-x86_64-sdl3`); FreeType is already there. With it the windowed
`build/bin/vf3` builds too, and `jouer-fv2.cmd` opens the game. Note that SDL's pkg-config adds `-mwindows`, so
the binary has no console and its messages go to `log.txt` in the data folder, not the terminal.

MSYS2's g++ (16.1.0) builds `vf3run` and `vf3lock` unchanged, but `make` must be handed a writable temp directory
or the compiler tries `C:\WINDOWS` and stops with "Cannot create temporary file"; passing `TMP`, `TEMP` and
`TMPDIR` as make command-line variables is what gets through to the recipes. `construire.cmd` and
`lancer-fv2.cmd` in the repository root do that, and `verifier-vf3.cmd` runs the whole regression above in one go
(it makes the upstream worktree and builds it the first time, then compares the state dumps and the RAM and runs
the lockstep). All three take an optional frame count.

The three warnings the build prints (`px`/`py` possibly uninitialized in `r3d_scene.cpp`, parentheses in
`vf3run.cpp`) are the upstream code's, not this work's.

## Where the work stands against the brief's phases

| Phase | State |
| --- | --- |
| 0 — project, ROM set, gzip tools, toolchain | **done.** Branch `fv2`; three sets; the gzip read in C in the loader; `vf3run` and `vf3lock` build |
| 1 — boot and gzip | **done.** The criterion was "runs past the stub without an immediate crash": RAM 0 is 99.986 % the gzip's content, the PC is in the game's own program, 1800 frames with no fault. Virtua Fighter 3 is byte-identical to upstream over 3000 frames and passes its own lockstep |
| 2 — the Step 2.0 machine layer | **done.** Clocks, MPC106, the map, the JTAG chain, the Real3D IDs, and the 315-5881. Its criterion, "no more mapping or basic hardware crashes", holds for 1800 frames, and the security chip's output is verified text |
| 3 — a stable interpreter (attract, menus, inputs) | **done.** Boot, WARNING, title, the attract demo, a coin, the fighter select, and a whole round fought against the CPU: 4201 scripted frames, no fault |
| 4 — Real3D and tiles | **started.** The Step 2.x culling nodes and formats are in and the 3D draws (section 9); the colours hold up across the demo, the one white frame being a flash the game asks for (section 10). The board behind them matches the reference exactly over 6432 frames (section 17); the pictures themselves still have no reference to be compared with |
| 5 — coverage and the recompiler | **done.** Its own address map from this port's own coverage, 69,716 instructions recompiled, and the lockstep identical over both scripts — 2231 frames on the attract and menus, 4201 through a match (section 13) |
| 6 — polish | **started.** The windowed host runs the game with sound, inputs, the menu, upscaling and the 3D on the GPU (Direct3D 12, tried on real hardware at last); per-game NVRAM, window name and play style; widescreen in the attract and in a fight, two Step 2.x things the GPU renderer was not doing, and a Windows package that stands on its own (sections 14, 15 and 18) |
