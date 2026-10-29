# Fighting Vipers 2: the select-screen portraits, read from a folder

The row of faces along the bottom of Fighting Vipers 2's select screen is two 256x256 textures in the VROM. This
port can put your own pictures there, read from a folder of PNG files at start-up. Nothing is written to your
zip, nothing is written to disk at all: the sheets are replaced in the VROM image the process holds in memory,
once, before the board starts.

This is what was measured to settle it, what the code does, and what it does not do.

Where it came from: that the portraits are VROM textures and roughly where, is the owner of this tree's own
earlier work on the game under Supermodel (`fv2mod`, HANDOFF section 9 ter, and his `portraits.py` / `vrom.py`).
Every figure below was taken again here, against this port's own VROM, before any of this was written -- and one
of them came out different from the note, which is the point of taking them again.


## Where the sheets are, measured here

The VROM is not compressed. Unlike the program -- a gzip at the fixed CROM's offset 0 that the game's own boot
stub inflates into RAM (`docs/step2-fvipers2.md`) -- it is in clear in the zip, `romset::load` lays it out, and
the game never writes a word of it. So there is nothing to wait for and nothing to keep re-applying: one write
into the loaded image is the whole of it.

A throwaway probe linked against this tree's own `src/machine/romset.cpp` read the player's `fvipers2.zip`,
decoded 256x256 at each of the two offsets the Supermodel-side notes name, and wrote the result out as PNG. Both
pictures are the eleven portraits and the empty white case. The first sixteen bytes at each:

```
light at 0x140a6a8: 25512551255125512551255125512551
dark  at 0x1435150: c340c340c340c340c340c340c340c340
```

Those two words -- `0x5125` and `0x40C3`, little-endian in the VROM -- are the fingerprint the code checks before
it writes anything. An image that does not have them there is not these sheets, and nothing is touched.

The spacing was **measured rather than carried over**: walking the mipmap pyramid of the first sheet, level by
level, ends at exactly `0x1435150`, where the second begins. So the pyramid is `0x2AAA8` bytes, the two sheets
are back to back, and the texture is one the Real3D uploads with its mipmaps already made (header type 0) rather
than one it builds them for. The two textures that follow at the same spacing were dumped too, to be sure
nothing here runs past the sheets into something else: `0x145FBF8` is the digits 0-9 and `0x148A6A0` the
FIGHTING VIPERS 2 logo. Neither is touched.


## The format

- **The texels are not stored row by row.** They are in tiles -- 8x8, or 8x4 and 8x2 for the smallest mipmap
  levels -- and the tiles sweep the **whole 256-wide sheet**, not one 64x64 case at a time. This is the trap: a
  search for a portrait through the VROM row by row finds the first tile and nothing more.
- Inside a tile the order is a fixed permutation. `src/machine/fv2_portraits.cpp` uses **the same three tables as
  `src/machine/model3.cpp`'s `r3d_store`**, on purpose and not by coincidence: that function is what the Real3D
  does with these very bytes, and two copies of the order that could drift apart would be worse than one that
  cannot.
- **T1RGB5, little-endian**: `T RRRRR GGGGG BBBBB`, with `T` set meaning the texel is the polygon's contour --
  what the renderer draws as nothing. Not one texel of either sheet has it set.
- **Mipmaps follow level 0 in the same stream**: 128, 64, 32, 16, 8, 4, then 2. Together with level 0 that is
  `0x2AAA8` bytes, which is the measurement above.


## The sheet's order, confirmed on screen

The sheet is 4x4 cases of 64x64: eleven portraits, then five free ones in flat white. The order is neither the
cursor's nor the fight identifiers' -- three numberings, and they are not the same.

```
00 BAHN    01 CHARLIE  02 GRACE   03 HONEY
04 JANE    05 MAHLER   06 PICKY   07 RAXEL
08 SANMAN  09 EMI      10 TOKIO   11-15 free (white)
```

That order came from the owner's notes, and the game itself confirmed it here. With sixteen numbered test cases
in place of the portraits, the select screen's row reads, left to right:

```
09 00 06 04 07 05 02 08 03 10 01
```

and the name above the cursor changes with it: on case `09` the screen says EMI, and two presses of right later
the cursor is on case `06` and the screen says PICKY. So the cursor's order is Emi, Bahn, Picky, Jane, Raxel,
Mahler, Grace, Sanman, Honey, Tokio, Charlie, and the sheet's order is the one above.

**Which sheet is which, also measured on screen.** The test cases carry a corner mark -- one square in the light
folder, two in the dark one. On the select screen the case under the cursor shows one square and every other
case shows two. So the light sheet is the selected fighter and the dark sheet is everyone else, and both are
live at the same time. Replacing only the first would leave ten faces out of eleven as Sega drew them.


## What the code does

`fv2::replace_portraits` (`src/machine/fv2_portraits.cpp`), called once after the ROM set is loaded and before
the board runs:

1. refuses any set but `fvipers2`, and refuses an image whose two fingerprints are not there;
2. reads the folder's PNGs -- a case with no file is simply not replaced, and a file it cannot read is reported
   and not replaced, so a bad folder costs nothing;
3. **checks its own codec before writing**: the sheet's own bytes are run through the decoder and back through
   the encoder, at every level, and compared with what was there. `detile` and `tile` are two halves of one
   permutation, and a permutation wrong in both halves the same way would still read the sheet back correctly
   while writing nonsense into the VROM. Anything but an exact match and the sheet is left alone;
4. writes each level from 256 down to 4, putting the replaced case into its own rectangle at that level's scale
   (64, 32, 16, 8, 4, 2, then 1 texel a case at the 4x4 level) by box-filtering the PNG down.

The 2x2 level is left exactly as Sega wrote it. At that size the sheet's 4x4 cases are half a texel each, so one
cannot be written without touching the three beside it -- and a 256x256 texture is only sampled there when it is
two pixels on screen.

Measured, with three PNGs in a folder and thirteen cases missing: two cases changed in the light sheet, one in
the dark one, **and every other case came back with not one texel changed at any level**. Nothing outside the two
`0x2AAA8`-byte pyramids changed either -- 32,067 bytes in all, every one of them inside them.

A sideways check of the quantisation: the owner's own tool exported the dark sheet to twelve PNGs months ago,
with a decoder written independently of this one. Fed back in here, those twelve files reproduce Sega's texels
**bit for bit** -- 0 of 4096 texels different in each of the twelve cases.


## The folder

```
<folder>/light/00.png ... 15.png     the sheet drawn for the selected fighter
<folder>/dark/00.png ... 15.png      the sheet drawn for everyone else
```

`claire/` and `sombre/` are read as well, so a folder made by the owner's Supermodel-side `portraits.py` is taken
as it stands. A `NN.png` sitting at the folder's root stands in for either sheet when that sheet has no file of
its own.

Each file is one **64x64** image. Any other size is reported and the case keeps Sega's. The reader
(`src/host/png.h`) takes bit depth 8 or 16 and colour types grey, RGB, palette, grey+alpha and RGBA, without
interlacing; anything else it says plainly rather than guessing.

`assets/fv2-portraits-test/` is a set of test cases -- flat colours with the slot number on them -- made by
`tools/fv2/portraits_test.py` and used for the pictures above. Nothing in it comes from the game.


## Using it

```
vf3.exe fvipers2.zip --portraits "C:\my portraits"
```

or `portraits_folder = C:\my portraits` in `settings.ini`, which is where `--portraits` saves it.
`--no-portraits` turns it off again and forgets the folder. `play-fv2-portraits.cmd` is the launcher: with no
argument it uses the test cases, and with a folder as its first argument it uses that one.

`tools/vf3run --portraits DIR` does the same thing without a window, which is how the measurements above were
taken.

The other Fighting Vipers 2 set, `fvipers2o`, is left alone: it is a different build, nothing says its VROM is
laid out the same, and the code says so rather than writing at an address that means nothing there.


## Verified

- `verify-vf3.cmd`: Virtua Fighter 3 untouched, all five checks IDENTICAL. It has to be -- `replace_portraits`
  is never called unless the player names a folder, and it returns at once for any set but `fvipers2`.
- Fighting Vipers 2 **without** the option, against this branch's base commit, on `tools/scripts/fv2-fight.txt`,
  4200 frames: the CPU state every frame, the 8 MB of RAM at the end, and all 16 rendered pictures, byte-identical.
- Fighting Vipers 2 **with** the option, interpreter against recompiled, same script: state and RAM byte-identical.
  They have to be: the VROM is not the program, and the PowerPC never reads a byte of it.
- With the option against without, same script: **only the three select-screen pictures differ.** The other
  thirteen -- the loading screen, the intro, the whole fight, the end -- are byte-identical, which is the measure
  of where those two sheets are used.
- The windowed host's picture at the select screen, byte-identical to the headless run's, for all four pictures
  the script takes there.
