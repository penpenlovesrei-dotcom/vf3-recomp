# Fighting Vipers 2: the 3D textures replaceable from a pack

The Real3D draws from two pages of texture memory the game fills as it goes. This makes those textures
replaceable: a folder of PNGs, each named after the texture it stands in for, drawn in its place at whatever
resolution the image has. Off unless asked for.

What follows is what was read in this port's own code and what was measured here. The idea, the hashing of
content rather than addresses, and the refusal of flat textures come from the owner of this tree's earlier work
on the same problem inside Supermodel (`Docs\Tex3DHD.md` in his fork); the architecture below is this port's own
and is **not** his, because the two renderers are not built alike. Nothing of Supermodel's code is here.


## 1. Where a texture can be caught in this port, read in the code

This is the question the whole design turns on, and both rasterisers had to be read to answer it.

**The CPU rasteriser** (`src/video/r3d_raster.cpp`) samples through `Sampler`. Its `fetch` is the whole story:

```cpp
decode(format, tex[size_t(page * 1024 + mip_y[level] + y) * 2048 + mip_x[level] + x])
```

`tex` is the board's texture memory, 2048 x 2048 16-bit words -- two pages of 2048 x 1024. Nothing is decoded
before this point: the words are the game's, and the polygon's format says how to read them. A texture's whole
address here is six numbers, taken from the mesh and from the culling nodes' texture offset:

| | from |
| --- | --- |
| page | `mesh.page ^ model.page` |
| x, y | `(mesh.x + model.tex_x) & 2047`, `(mesh.y + model.tex_y) & 2047` |
| width, height | `mesh.width`, `mesh.height` |
| format | `mesh.format` |

**The GPU renderer** (`src/gpu/r3d_gpu.cpp`, `src/gpu/r3d.hlsl`) uploads that same texture memory into one raw
storage buffer and decodes it in the shader, by the same arithmetic, from the same six numbers -- which it puts
in its per-mesh `Draw` record (`GDraw` on the C++ side: `format`, `tx`, `ty`, `tw`, `th`, `page`).

So **neither rasteriser has anything like a texture object per texture of the game**, and both reach a texture
through one address of six numbers. (That much is as Supermodel is, and for the same reason: the hardware has a
texture sheet, not textures.)

**And they share more than a key.** `Raster::prepare` already builds the list of polygons that both rasterisers
draw -- `Raster::draws`, which the GPU renderer walks to fill its `Draw` records. One pass over that list, once
a picture, therefore serves both: that pass is `Raster::resolve_hd`, and its answers sit in `Raster::hd`, one
`TexPlace` per draw. The CPU rasteriser reads it in `draw_tri`; the GPU renderer reads it in `render`. There is
no second copy of the decision and no way for the two to disagree about which texture is being drawn.


## 2. What the replacement is: more pages of texture memory

Supermodel had to modify its shader, because there the raw texture RAM is a `R16UI` texture per page and the
decoding is in the shader; an RGBA8 atlas meant a new sampler, a new uniform and a new branch there. Here the
conclusion is different, and it is the point of this work.

**The replacements live in extra pages of texture memory.** `src/video/tex3d_hd.cpp` holds pages of the same
2048 x 1024 shape as the board's two, lays the pack's images out in them on shelves, and hands a polygon the
address of its replacement instead of its own. Then:

- the CPU rasteriser's `Sampler` is given those pages as its memory, and its page, place, size and format;
- the GPU renderer makes its one storage buffer that much bigger and uploads those pages once, after the
  board's two, so a replacement's page number is simply `2 + n`.

**No shader change at all**, which matters: `src/gpu/r3d_shaders.h` is generated, and rebuilding it needs a DXC
that can generate SPIR-V -- the one in the Windows Kits cannot (checked here: *"SPIR-V CodeGen not available"*).
And, more importantly, both rasterisers sample the very same words by the very same code, so their two pictures
stay comparable, which is what section 15 of `step2-fvipers2.md` pays for.

**What it costs.** The pages are the hardware's 16-bit texels, so a replacement is **higher resolution at the
game's own colour depth**, not at 24 bits. The native formats are all 16-bit (T1RGB5, RGBA4, A4L4 and the rest),
so nothing is lost against what the game itself could ever have shown; a photographic gradient would band
exactly as the game's own textures band. A replacement is written in one of the two formats that carry colour:
RGBA4 (format 7) where the texture it stands in for has graded alpha, T1RGB5 (format 0) -- five bits a channel
and one bit of transparency -- otherwise.

The other route, an RGBA8 atlas of its own, is still open and would need the shader rebuilt with a release DXC.
It is not done here.


## 3. Identity is the content, never the place

The game uploads its textures where it likes and moves them between scenes, so an address is no name for one.

```
Identity = (FNV-1a 64 of the level-0 texels, width, height, format)
```

The format is in it because the same words read as T1RGB5 or as A4L4 are not the same picture, and it is the
polygon that says which -- so a texture is only knowable **when something draws it**, never when it is uploaded.
One function hashes, and the dump (which names the files) and the pack (which looks them up) both call it: they
cannot drift apart.

**A flat texture is refused**, in the dump and at the match. Its hash is the hash of every flat texture of that
size, so a pack entry for one would stand in for anything flat anywhere on the screen.

**A translator-map polygon is left alone.** It reads its texel as an index into its own colour times sixteen,
not as a colour, so a replacement image would be multiplied out of range.


## 4. The dump

```
dumper-textures-3d-fv2.cmd                      into Textures3D-dump\fvipers2
vf3.exe fvipers2.zip --hd-3d-dump [folder]
vf3run fvipers2.zip --script ... --hd-dump folder
```

Entirely passive: nothing to press, and every texture a polygon samples is written once, deduplicated by
content, as `<hash>_<width>x<height>_f<format>.png` -- RGBA8, so transparency survives into the file. A
`textures.csv` beside them says where each was found; it is **added to**, never started again, so two sessions of
capture describe every PNG in the folder.

### The alpha an image carries is the polygon's, not the texel's

A texel's top bit (T1RGB5) or its alpha nibble is **not** transparency on its own. Whether it is, is the polygon's
business, and `src/video/r3d_raster.cpp` settles it in three lines at the end of the texel fetch:

```cpp
if (mesh.alpha_test && t.a < 32.0f / 255.0f) continue;   // a cut-out: the texel is dropped
if (mesh.texture_alpha) { ... }                          // the alpha grades the blend
else if (!mesh.alpha_test) { t.a = 1; }                  // otherwise FORCED OPAQUE
```

Until 2026-10-21 the dump wrote the decoded alpha regardless, so it wrote transparency the screen never had. In
Fighting Vipers 2 it showed as a **one-pixel comb** -- every other column, sometimes every other row, fully
transparent -- because the game leaves the T bit alternating in texture memory wherever no polygon reads it.
Over Honey's costume, **42 of 53** images had at least one wholly transparent row or column; after the fix, **10**,
and every one of those ten is a polygon that really does use its alpha.

Supermodel has the same trouble in its own sheet dump and answers it by writing the whole of texture memory
twice -- `textures_t1rgb5_contour.bmp` and `textures_t1rgb5_opaque.bmp` (`Src/Model3/Real3D.cpp`, `DumpTextures`) --
leaving the choice to the eye. Here the choice is not needed: the polygon that drew the texture is in hand, so
the dump writes the alpha that polygon used and says which in a ninth CSV column:

| `alpha` | the polygon | what the image carries |
| --- | --- | --- |
| `opaque` | neither flag | alpha 255 everywhere |
| `contour` | `alpha_test` | a cut-out, 0 or 255 at the rasteriser's own 32/255 threshold |
| `graded` | `texture_alpha` | the decoded alpha, untouched |

Measured over one Honey capture: **145 opaque, 21 graded, 7 contour**. The identity of a texture does not move --
the hash is over the raw 16-bit texels, not over the image written -- so a pack made before the fix still matches.

One texture can be drawn by two polygons that disagree. The file is written the first time it is drawn and cannot
carry both, so a line is printed rather than the picture being quietly wrong:

```
3D texture dump: <name> drawn both opaque and contour; the file keeps opaque
```

**What is not drawn is not captured** -- and in this port, what is not *drawn* is not even the same as what the
game puts on screen, because the headless runner renders only the frames its script asks a picture for, while
the windowed host renders every picture.

That cost a false conclusion once, and the trap is worth leaving marked. `--hd-dump` used to need `--shots`
beside it: without a folder to put pictures in, the runner drew nothing, so no texture was ever sampled and the
dump wrote its CSV header and not one file. Which looks exactly like a broken dump, and was read as one. It now
renders at its script's shot marks when the dump is on, whether or not anything is written, so `--hd-dump`
stands on its own -- measured, 322 textures with no `--shots` and no stray pictures. The 2D tile dump never had
this, because the tile path installs itself whenever that dump is open.

Measured:

| the capture | distinct textures |
| --- | --- |
| `vf3run --script fv2-shots.txt` (2421 frames, 22 pictures rendered) | 322 |
| `vf3run --script fv2-fight.txt` (4201 frames, 16 pictures rendered) | 220 |
| **the host over `fv2-fight.txt`** (4201 frames, every picture) | **541** |

So the capture to make a pack from is the launcher's, not the runner's: the launcher misses nothing that goes
past on screen. Of the 541, 309 of the first capture's were format 0 and 13 format 7, from 32 x 32 up to
1024 x 1024.

The capture is passive in the strict sense: over `fv2-shots.txt` the 22 pictures rendered with the dump on are
**byte-identical** to the ones the same build renders with no dump.


## 5. The pack

```
Textures3D\<game>\<hash>_<width>x<height>_f<format>.png
```

**No manifest**: the file name is the key, and it is the one the dump wrote. The gesture is to take a file from
the dump, enlarge it, redraw it, put it back under the same name.

**No global scale.** Each image may be whatever resolution it likes; the size in the name is the native one and
serves only to identify. A texture with no file keeps its own rendering. An image is left out, with a line
saying why, when it is not named that way, cannot be read, is bigger than a page (2048 x 1024), or when there is
no room left (16 pages, 64 MiB, is the ceiling).

```
3D texture pack: 319 textures loaded, 3 left out, in 6 pages of texture memory, from ...
3D texture pack: be4ceceed80b9ef3_64x32_f0.png drawn, from page 1 at 256,608
```

The second line comes the **first time that texture is drawn**, and it is the thing to look for when nothing
seems to happen: a pack indexed by content never says of itself that it matched.


## 6. What it costs, measured

The texels of a place in texture memory are hashed again only when the board has written to it since. The board
already counted its uploads (`Model3::tex_serial`); `Raster::tex_version` carries it, and while it does not
change, every picture reuses what the one before worked out.

Measured on the select screen at 2x (1366 x 768), the host's own pace log, the CPU rasteriser:

| | the CPU's 3D, a picture |
| --- | --- |
| no pack | **9.56 ms** |
| the pack on | **9.77 ms** |
| the pack on, hashing every picture (before `tex_version`) | 17.19 ms |

The pack's pages cost 4 MiB of memory each, on the CPU and again on the GPU; the proof pack below took six.


## 7. The two rasterisers against each other

The host's own start-up check draws one frame of the game on the GPU and on the CPU and counts the pixels that
differ (`VF3_GPU_CHECK_DIR` writes both pictures). On one frame of the attract, Direct3D 12 on an RTX 3070:

| | pixels differing by more than 8, of 1,049,087 | drawn on one only | worst channel |
| --- | --- | --- | --- |
| no pack | 6 | 1 | 40 |
| the pack on | **9** | **1** | **37** |

So the pack changes nothing about how far apart the two rasterisers are: what is left is a GPU and a software
rasteriser rounding and filtering differently, exactly as it was.


## 8. Known limits, each for a reason read in the code

- **No mipmaps for a replacement.** A pack's image has none, so level 0 is sampled whatever the distance (the
  per-mesh `levels` is set to 0, which is what both rasterisers clamp against). Of no consequence on a quad seen
  face on; to revisit for a stage texture seen in perspective. Supermodel's pack has the same limit.
- **No flipbook cross-fade on a replaced texture.** A flipbook's two pictures share one page, one size and one
  format in the GPU's per-mesh record, so a replaced one is shown without the fade between steps rather than
  shown differently by the two rasterisers.
- **16-bit colour** (section 2).
- **A replacement must fit in a page**: 2048 x 1024 at most, and 16 pages in all.
- **The pack is read once, at the start.** There is no reloading it while the game runs.
- **The pack and anything that rewrites the game's textures exclude each other**, for the same reason as in
  Supermodel: the pack recognises a texture by its *original* content.


## 9. Using it

```
vf3.exe fvipers2.zip --hd-3d                  the pack in Textures3D\<game> in the data folder
vf3.exe fvipers2.zip --hd-3d-pack <folder>    a folder of its own
vf3.exe fvipers2.zip --no-hd-3d               off again
vf3.exe fvipers2.zip --hd-3d-dump [<folder>]  capture
```

`--hd-3d` saves itself in `settings.ini` as `hd_3d = 1`, as `--full-roster` does. `jouer-fv2-hd3d.cmd` and
`dumper-textures-3d-fv2.cmd` are the launchers. `tools/vf3run --hd-pack <folder>` and `--hd-dump <folder>` do
the same without a window, which is how everything above was measured.

`tools/tex3d_proof_pack.py` makes a pack out of a dump -- every image twice the size with its red and blue
channels swapped -- so that what is replaced can be seen at a glance without anything being drawn by hand.
Nothing it makes belongs in the repository: it is the game's own pictures, altered.


## 10. Verified

- `verifier-vf3.cmd`: the five checks IDENTIQUE. Virtua Fighter 3 cannot feel any of this -- with no pack and no
  dump, `Raster::hd` stays empty and both rasterisers take the paths they always took.
- With the option off, Fighting Vipers 2's 22 pictures over `fv2-shots.txt` and Virtua Fighter 3's three over
  `vf3-shots.txt` are **byte-identical** to the build before this change.
- With the dump on, the same 22 pictures are byte-identical too.
- The match run too (`tools/scripts/fv2-fight.txt`, 4201 frames, 16 pictures): byte-identical to the build
  before this change, with the option off.
- With a proof pack of 319 textures over `fv2-shots.txt`: the 14 pictures before any 3D is drawn are
  byte-identical, and every one of the 8 with 3D in it changed.
- With a proof pack of 538 textures, made from the host's capture of a whole match, over `fv2-fight.txt`:
  **15 of the 16 pictures changed** (the one that did not is the stage loading, which draws no 3D), and in a
  round the stage, the sky, both fighters and their bikes are all replaced while the tile generator's 2D --
  the health bars, the names, STAGE 1, the timer -- is not, which is exactly the line between the two layers.
- That 538-texture pack takes **10 pages**, so the GPU's storage buffer is 8 MiB + 40 MiB; it draws, and the
  GPU and the CPU still agree (section 7).


## 11. What has not been done

- **The RGBA8 route** (section 2): a replacement at 24-bit colour needs a new resource and a new branch in
  `src/gpu/r3d.hlsl`, and `src/gpu/r3d_shaders.h` rebuilt with a DXC release that generates SPIR-V.
- **No pack of real artwork exists.** What is proven is the mechanism, with the game's own pictures altered.
- **Mipmaps, hot reloading, and the size of the pages** are all left where section 8 leaves them.
- **Nothing was tried on Virtua Fighter 3**, on purpose: the mechanism is not game-specific and would work
  there, but no pack was made for it and no picture of it was taken with one.
