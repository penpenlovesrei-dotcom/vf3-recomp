# Fighting Vipers 2: the 2D replaced by a texture pack

The Model 3 draws its menus, its logos and its HUD on a tile generator: four 64x64 maps of 8x8 tiles, composed
line by line into two layers, one under the 3D and one over it. This is what the port does to let those tiles be
replaced by larger images, the way a texture pack replaces a game's textures, without touching the ROM.

Everything is behind an option and every option is off by default. With no option, the picture is not "probably
the same": it is the same code, and that is checked below byte for byte.

Where it came from: none of the ideas here were found in this tree. They come from the owner's own earlier work
on the same problem inside his fork of Supermodel, written up in
`SEGA Model 3\SupermodelSrc\Docs\TuilesHD.md` -- the identity of a tile, the colour offsets, the shape of the
layer images, the passive dump and its two-frame filter, the 496-column limit. That document was read as
documentation and nothing was copied from its code, which is GPL. What this port adds is its own side of it:
where the hook belongs in **our** rasteriser, which is not Supermodel's, and the measurements below, which were
taken here.


## 1. The one place a 2D pixel is born, in this port

This is a reading of `src/video/tilegen.{h,cpp}`, not a guess.

The chain is:

| | where | what happens |
|---|---|---|
| 1 | `src/host/main.cpp` `Game::load` and `tools/vf3run.cpp` | `board.on_line = [..](unsigned y) { if (y == 0) tiles.clear(); tiles.line(board, y); }` |
| 2 | `src/machine/model3.cpp` | the PowerPC runs between two lines, so a register written mid-frame is honoured line by line |
| 3 | `TileGen::line()`, the `pair` / `c0` loops | B and B' first, then A and A' over them, 32 pixels at a time -- the width one bit of the line's mask covers |
| 4 | **the `put` lambda inside `TileGen::line()`** | reads the pattern word out of the tile RAM, looks the texel up in the palette, writes the RGBA8 pixel |

**Step 4 is the whole interception point.** It is the equivalent of Supermodel's `Draw4BitTilePair` /
`Draw8BitTilePair`, with one difference worth knowing: ours is not a pair. `put(dst, x, e, four, fy, fx0, n)`
writes `n` texels (1 to 8) of **one** tile's row, because the loop above it walks the 32-pixel column in
tile-sized steps and stops at the column's edge. So the hook sees one tile at a time, with `fx0` saying where
in the tile the run starts. Everything a replacement needs -- the name-table entry, the depth, the row, the
destination -- is in its arguments already.

There is a second writer, `put_texel`, used only by the widescreen side code; and `put` is called again from
that side code and from the edge-tile fix-up. Those write `below_wide`, not the screen, and they are left
alone (section 4).

**Where the resolution is locked**, all of it in `src/video/tilegen.h`:

```cpp
const int W = 496, H = 384;
const int EXT = 256, WW = W + 2 * EXT;
std::vector<uint32_t> below = std::vector<uint32_t>(W * H), above = std::vector<uint32_t>(W * H);
std::vector<uint32_t> below_wide = std::vector<uint32_t>(WW * H);
```

`W` and `H` are compile-time constants and they are read in about forty places outside the tile generator: the
host's SDL textures (`make_2d`, `send_2d`), the quad the layers are drawn on, the stage test that counts 2D
rows at the 4:3 edges, the side colours, `TileGen::scrolled`, `compose()`, every `write_png` of a frame. Making
them variable would reach all of that.

So they were not made variable. The scale is **additive**: a second pair of surfaces beside the first, and the
native ones go on being written exactly as before. Everything that *measures* something from the layers --
smooth motion's scroll, the stage test, the sides' colours, the panorama memory -- keeps reading the native
surfaces, so none of those measurements change with the scale. The cost is that the rasteriser writes twice
when the scale is on; the gain is that with it off there is nothing to argue about.

**496 columns and not one more.** The hardware fills 496 of the map's 512-pixel pitch, and the name table is
written over 62 of its 64 columns. No display setting can show more of the tile plane than that, and nothing
here tries to. The widescreen sides this port draws (`below_wide`, `EXT` = 256 pixels each side) are the port's
own reconstruction of a stage's panorama for Virtua Fighter 3 and stay on the hardware's grid; at an HD scale
they are enlarged as blocks and the 496 real columns are then written over them at their own resolution
(`TileGen::hd_line_sides`).


## 2. The identity of a tile is never its index

A pattern index is a byte offset into the tile RAM. The game moves its patterns about as it streams them, and
on Fighting Vipers 2 the patterns are not in the ROM in the clear at all: the program is a gzip the boot
inflates into RAM, and the tiles only exist in the tile RAM at run time. So everything has to go through the
runtime, and the name is the content:

- **a tile** is `(hash of the pattern's bytes, palette block, depth)` -- 32 bytes hashed in 4-bit mode, 64 in
  8-bit. The palette is part of it because it comes from the name-table entry and not from the pattern: one
  pattern is commonly drawn in several colours, and in 4-bit mode bits of the pattern index are reused inside
  the palette index;
- **a layout** is the hash of the whole name table, 2048 words at `0xF8000 + layer * 0x2000`. Not what is on
  screen: a layer that scrolls shows a different window of the same layout every frame.

Both hashes are in `src/video/tilehd.cpp` and nowhere else. The dump writes the names and the pack looks them
up; the two disagreeing by one bit would match nothing at all, in silence, which is why they share one
function rather than two that look alike.


## 3. The dump: what the game actually draws

```
vf3.exe fvipers2.zip --tile-dump <folder>
tools/vf3run fvipers2.zip --script tools/scripts/fv2-shots.txt --tile-dump <folder>
```

`releve-tuiles-fv2.cmd` is the launcher. It is **entirely passive**: nothing to press, no period. Play through
the screens you want to redraw and everything that is drawn is written once, deduplicated by content. A tile is
only taken if it was really drawn -- that is why it is the drawing hook and not a sweep of the tile RAM.

```
<folder>/tiles/<hash>_p<palette>_<4|8>bpp.png   one 8x8 picture per distinct tile
<folder>/tiles.csv                              their inventory
<folder>/layers/L<n>_<hash>.png                 the whole 512x512 tile map of one layout
<folder>/layers/L<n>_<hash>.txt                 its 64x64 grid, its registers, its legend
<folder>/layers/index.csv                       the table of contents, busiest screens first
```

**Start with `layers/index.csv`.** To get from something on screen to its tile file: cell `(x, y)` of the map is
at `(x*8, y*8)` pixels in the layout picture, the `.txt` grid gives that cell's short hash, and the legend at
the end of the `.txt` names the exact file.

Two filters, both taken from the owner's measurements rather than rediscovered:

1. a layout is only written once the **same name table has stood for two frames**. Without it the intermediate
   states are collected while the game is still building the screen;
2. an empty layer is not written, or the unused ones would be megabytes of black.

There is a ceiling of 256 layouts per layer, so a game that rewrites its name table every frame cannot fill the
disk. A layer that changes every single frame therefore never gets a layout picture; its tiles are still
captured.

Measured here, over `tools/scripts/fv2-shots.txt` (boot, attract, the select screen, a fight -- 2420 frames):
**4704 distinct tiles and 21 layer layouts**, in 50 s, which is the same wall-clock time as the same script
without the dump. The windowed host over the first 1020 frames wrote 238 tiles and 4 layouts, and gave the SEGA
logo the same name as the headless run did, `L2_835ee1da3ab98e02` -- so the two hosts agree.


## 4. The scale

```
--tile-scale <1..8>        or tile_scale in settings.ini, default 1
```

The tile plane is drawn at `scale` times its size each way. **Everything the generator computes stays in native
coordinates** -- the scroll, the mask, the tile numbering, the 32-pixel columns, the widescreen sides. Only the
writing of a pixel widens: where `put` writes one pixel, the HD path writes a `scale` by `scale` block of it.

Measured: at `--tile-scale 4`, with no pack, the picture is the scale-1 picture with every pixel repeated 4 by
4, **exactly** -- checked on two Fighting Vipers 2 frames (the SEGA logo and the title screen) by enlarging the
1984x1536 against the 496x384 with no smoothing and comparing. Not one pixel differs.

So on its own the scale improves nothing: the original tiles are simply repeated, and they lose the smoothing
the upscale filter gave them. All it does is make the room the HD art goes in.

Memory, at scale 4: the three HD surfaces are about 50 MB together (the widened one dominates).


## 5. The pack

```
--tile-pack <folder>       or tile_pack in settings.ini
--no-tile-pack             ignore the settings' pack, for comparing
```

`jouer-fv2-hd2d.cmd` is the launcher. The folder holds `tiles/` and `layers/` in exactly the shape the dump
wrote them.

**There is no manifest. The file name is the key**, and it is the name the dump wrote. The gesture is: take a
file the dump wrote, draw it larger, put it back under the same name.

Sizes are fixed, and one scale has to hold for the whole pack:

| | shape | at scale 4 |
|---|---|---|
| a tile | 8 x scale, square | 32 x 32 |
| a layer, the whole tile map | 512 x scale, square | 2048 x 2048 |
| a layer, the visible screen | 496 x scale by 384 x scale | 1984 x 1536 |

The tile-map shape scrolls and wraps as the hardware's map does, and is the one to use for a layer that moves.
The screen shape is the handy one for a screen that stands still, and by its shape it does not scroll at all.
Any other size is refused by name, and the loading line says what was expected.

What the lookup does:

- **a tile**: its own palette first, then an image written `_pANY` as a fallback for any palette;
- **a layer**: the image **is** the layer. Where it is transparent nothing is drawn, and the game's own tiles
  do not come back behind it.

Three things are handled that one would not expect to be:

- **fades.** The colour offset registers `0x40` and `0x44` are normally baked into the palette, which HD art
  goes round. They are put back on the pack's pixels, or the art would stay lit through a fade to black.
- **partial alpha.** The hardware has one bit of transparency; a pack pixel with an in-between alpha is
  composited, so an antialiased edge does not go back to a staircase.
- **the mask.** A replaced layer consumes exactly the same width as the tiles it stands for, so the per-column
  mask that chooses between a pair's two layers goes on working.

Whether it fired is in the log, and it is the first thing to look at when nothing happens -- a pack indexed by
content never says of itself that it found a match:

```
tile pack: 4681 tiles and 0 layers from <folder> at scale 4, 0 refused
tile pack: layer 2 drawn from L2_835ee1da3ab98e02.png
```


## 6. Making a test pack, and what it proved

`tools/hd-pack-test.py` makes a pack out of a dump this port wrote: it enlarges each picture by the scale with
no smoothing and swaps its red and blue channels. It invents no artwork. Anything the pack replaced comes out
with its colours exchanged and anything it did not is untouched, which is what makes it a proof.

```
py -3 tools/hd-pack-test.py <dump> <pack> --scale 4 --layers-only --layers L2_835ee1da3ab98e02
py -3 tools/hd-pack-test.py <dump> <pack> --scale 4 --tiles-only
```

Measured, `--tile-scale 4` with and without the pack, same frame, 1984x1536:

| | changed pixels | where | every change an exact red/blue swap |
|---|---|---|---|
| the SEGA logo, layer replaced whole | 116 416 of 3 047 424 | x 636..1335, y 660..871 | yes |
| the title screen, tiles replaced one by one | 51 296 of 3 047 424 | x 48..1267, y 1340..1495 | yes |

Nothing outside the 2D art moved: the 3D is pixel for pixel what it was.


## 7. Using it

```
vf3.exe fvipers2.zip --tile-scale 4 --tile-pack <folder>
vf3.exe fvipers2.zip --tile-dump <folder>
tools/vf3run fvipers2.zip --script ... --shots <dir> --tile-scale 4 --tile-pack <folder>
```

`--tile-scale` and `--tile-pack` are saved in `settings.ini` as `tile_scale` and `tile_pack`, and like
`full_roster` they are only written once they have been asked for. `--tile-scale 1 --no-tile-pack` puts it
back. With `--shots`, `vf3run` writes its pictures at the HD size.

`assets-hd/` is in `.gitignore`: a dump and a pack are made from the ROM and never belong in the repo.


## 8. Verified

- **`verifier-vf3.cmd`: all five checks IDENTIQUE** -- the PowerPC state frame by frame over 3000 frames, the
  8 MB of RAM, the rendered pictures, the windowed host's picture, and the interpreter against the recompiled
  code over 2231 frames.
- **With no option, byte for byte what it was.** 22 Fighting Vipers 2 pictures (`fv2-shots.txt`) and 3 Virtua
  Fighter 3 pictures (`vf3-shots.txt`) from `vf3run --shots`, against the same pictures from the binary built
  before any of this was written: all 25 identical. The windowed host on Fighting Vipers 2, both
  `host_last.png` (the game's frame) and `host_window.png` (the window as drawn): identical.
- **The scale alone changes nothing but the size**: scale 4 is scale 1 with each pixel repeated 4 by 4, exactly,
  on two frames.
- **The override fires, and only there**: the two diffs in section 6, from `vf3run --shots`, and the same two
  screens from the windowed host.


## 9. What is not done

- **Variants of a screen.** A layout whose name table differs by a few cells from one the pack knows is not
  recognised as the same screen; the override simply does not apply to it. On Fighting Vipers 2 the moving
  content is on layer 0 and the fixed art on layer 2, so replacing layer 2 is enough, and that is why this was
  left out rather than solved. It becomes necessary the day a screen refuses to match because a changing text
  is on the layer being replaced. The owner's Supermodel work has both an automatic form of this and a manual
  one (signature rectangles and holes); neither is here.
- **Cell patterns**, for HD art larger than 8x8 **that moves**. Not met on Fighting Vipers 2.
- **Hot reload** of the pack on a key.
- **Three paths are written and were never exercised**, so they are code that has not been seen to work: the
  `_pANY` fallback (on the screens dumped, the only tile appearing with more than one palette was the empty
  one), a layer image on a layer that actually scrolls (the layouts replaced here stand still), and the
  screen-shaped layer image -- only the 512-square tile-map shape was put on screen.
- **The cost was never timed.** Both surfaces are written every frame at the scale and sent whole to the GPU;
  no slowdown was noticed at scale 4 in the windowed host, but nothing was measured, and there is no frame
  cache like the one the owner added in Supermodel.
- **Virtua Fighter 3 was never run with any of this on.** It is not refused -- the option simply applies to
  whatever game is loaded -- but the only thing checked for that game is that with the options off it is
  untouched.
