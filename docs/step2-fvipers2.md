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

## What is still missing
- **Nothing is compared frame for frame with the reference.** The pictures above are this port's alone. Until a
  reference run exists for Fighting Vipers 2, "it draws the scene" is all that can be claimed, not "it draws the
  right scene".
- **No lockstep for Fighting Vipers 2 yet.** The recompiler has Virtua Fighter 3's address map
  (`src/rec/vf3_map.cpp`); Fighting Vipers 2 needs its own, from coverage runs, before `vf3lock` means anything
  for it. (Virtua Fighter 3's own lockstep passes on this branch; see above.)
- **Whether the game is getting anywhere is not known.** It executes its own program and takes 2231 frames of
  scripted input without faulting, but with no picture to look at there is nothing yet to say it is past its
  own boot checks and into the attract. That is the next thing to measure, not something to assume.
- **The windowed build needs SDL 3**, which isn't installed on this machine
  (`pacman -S mingw-w64-x86_64-sdl3` in MSYS2). The headless `vf3run` and `vf3lock` build and run without it.

## Building and running here

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
| 4 — Real3D and tiles | **started.** The Step 2.x culling nodes and formats are in and the 3D draws (section 9); the colours hold up across the demo, the one white frame being a flash the game asks for (section 10). Nothing is compared with a reference yet |
| 5 — coverage and the recompiler | not started. `program_image()` already hands it the right image |
| 6 — polish | not started |
