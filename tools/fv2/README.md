# Reading Fighting Vipers 2's program

The chain that turns a running game into a readable Ghidra project. Nothing here
holds any of the game's data: these are the scripts, and they take the image from
a run of this port.

## 1. The image

The program is a gzip at offset 0 of the fixed CROM which the game's own boot stub
inflates into RAM at 0 (`docs/step2-fvipers2.md`, section 1). To get it as the game
actually runs it:

```
build/bin/vf3run <fvipers2.zip> --frames 1200 --ram-at 1200 --quiet
python tools/fv2/mkimage.py ram_1200.bin fv2_prog.bin
```

`--ram-at` keeps 32-bit words in the **host's** order, so the byte of guest address
A sits at index A^3; `mkimage.py` re-swaps by groups of four and cuts the image at
the program's `0x29381E` bytes. It checks itself on one known instruction: at
`0x0005F3BC` the result must read `2C 80 00 C0` (`cmpwi r0,192`).

## 2. What to cut functions at

```
python tools/fv2/bltargets.py          # -> calls.txt, needs map.txt and fv2_prog.bin
```

`map.txt` is the recompiler's address map (`src/rec/fv2_map.cpp`, 7196 addresses) --
every word the program is *entered* at, measured by this port's own interpreter.
That is the right set to **disassemble** from and the wrong set to cut functions at:
6331 of the 7196 are branch targets inside a function, and a function cut at each of
those is a fragment. `bltargets.py` reads the image for the targets of `bl` and `bcl`
instead: **1382** addresses, which is what a function boundary is.

## 3. Sega's own names

```
python tools/fv2/dbgpages.py fv2_prog.bin dbg.json
python tools/fv2/mksyms.py             # -> symbols_dbg.csv
```

The game's internal test menu carries its debug tables, and Sega named its variables
in them: **80 pages, 1035 named entries, 745 distinct addresses**. `dbgpages.py`
documents the record layout it reads. The one rule worth repeating here: a page's
`+0C` field is an **offset** from the page's base when it is below `0x10000` and an
**absolute address** when it is at or above it, and the two ranges do not overlap
(largest offset `0x6060`, smallest absolute `0x114F3C`).

## 4. The state handlers

```
python tools/fv2/mkfnames.py           # -> fnames.csv
python tools/fv2/mkbodies.py           # -> bodies.csv, needs fnames.csv and calls.txt
```

The match flow is a switch, not a set of functions. `mkfnames.py` names all 81 of its
state handlers out of the game's own two parallel 80-entry tables -- the jump table at
`0x00024354` and the name table at `0x00024494` -- and `mkbodies.py` works out where
each handler ends. `docs/fv2-match-flow.md` is what came of reading them.

## 5. Into Ghidra

```
analyzeHeadless <dir> FV2 -import fv2_prog.bin \
    -processor "PowerPC:BE:32:default" -loader BinaryLoader -loader-baseAddr 0x0 \
    -scriptPath tools/ghidra \
    -postScript FV2Setup.java map.txt calls.txt symbols_dbg.csv
analyzeHeadless <dir> FV2 -process fv2_prog.bin -noanalysis -scriptPath tools/ghidra \
    -postScript FV2Names.java fnames.csv \
    -postScript FV2Fix.java bodies.csv \
    -postScript FV2Body.java bodies.csv
```

`tools/ghidra/` says what each script is for. `FV2Fix.java` is the one that is not
obvious: without it a 208-byte state handler decompiles as 4451 lines of the whole
switch, because a handler ends by branching back to the dispatcher's head and Ghidra
reads that as a jump to inline. With it, every handler decompiles as itself.

Then `FV2Decomp.java` writes the C of whatever functions you ask for, and
`FV2Dis.java` the disassembly with each instruction's resolved references beside it.
