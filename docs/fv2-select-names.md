# Fighting Vipers 2: the name on the select screen, and why BM has none

With the roster patch the select row reaches cursor 14. `docs/fv2-roster.md` says of the labels above the
fighters: *"Past cursor 10 the names and portraits are wrong or missing. The routine that draws them stops
there, and its table of positions holds eleven entries."*

**All three of those are wrong.** Cursors 11 and 12 draw the right name, the routine runs for all four, and
there is no table overrun. What there is, is an instruction Sega wrote on purpose.


## What each cursor actually shows

| cursor | fighter | label | |
|---|---|---|---|
| 8 | 6 HONEY | HONEY | right |
| 10 | 10 CHARLIE | CHARLIE | right |
| 11 | 14 DEL SOL | DELSOL | right |
| 12 | 13 KUHN | **KUHN** | right -- and the sprite is chrome-lettered, Sega drew it |
| 13 | 9 BM | **nothing** | |
| 14 | 12 DURAL | **BAHN** | borrowed |


## The name is tiles, and cursor 13 lays an empty strip

Not a 3D texture: cursor 14 adds **no 3D texture of its own** over the other cursors, so its BAHN cannot be
one. It is the tile layer, and the layouts the dump writes say it plainly -- each cursor produces one layout
of its own, and cursor 13's is blank:

```
cursor 8   L0_57a8179792369ed6   197 filled cells   HONEY
cursor 12  L0_5e9cddfea568d29b                      KUHN
cursor 13  L0_f5c71ae842c15cfe    20 filled cells   nothing
cursor 14  three layouts                            BAHN, sliding in
```

The name occupies **rows 28 to 35** of layer 0's name table, columns 2 to 54. The 20 cells cursor 13 does
fill are at rows 45-46 and are present in every cursor: something else.


## The path from a name to the screen

```
the name table is composed in RAM     0x0030EDCC   (double-buffered with 0x003111CC)
   cleared with the background cell   PC 0x0002036C, writing 0x00200020
   then the name's tiles are laid     PC 0x0003BB08, called from 0x000604B4
copied whole, 2048 words              PC 0x000202A4 loop, caller 0x0003B308
   to the tile VRAM                   0xF10F8000  (0xF10FC000 for the other buffer)
```

The buffer chosen alternates on the flag at `0x00274640`. None of this was written down in this port before;
the RAM-side name table is worth knowing for the HD tile work as well.


## The special case, read in the code

At `0x0006045C`, on the player-1 path:

```
0x060454  addis r9,r0,40          r9 = 0x00280000
0x060458  lbz   r3,-27032(r9)     r3 = [0x00279668], the player 1 cursor
0x06045C  cmpwi cr1,r3,13         is the cursor 13?
0x060460  bf    6,0x06047C        if not -> the ordinary path: look the name up and lay it
0x060464  addi  r3,r0,190         if so, item 190 instead
0x060468  addi  r4,r0,2           column 2
0x06046C  addi  r5,r0,28          row 28 -- the name's own place
0x060470  addi  r6,r0,0
0x060474  bl    0x03BDB8          a different routine from the name's (0x03BA30)
0x060478  b     0x0604B4          and the name is skipped
```

The same test again at `0x000604CC` for player 2, reading `0x00279669` and using column 34.

`0x00279668` and `0x00279669` are the cursor positions `docs/fv2-roster.md` already names, from Sega's own
debug tables.


## Why Sega wrote it: the ordinary path would have lied

Patch the comparison so it can never hold -- `cmpwi cr1,r3,13` becomes `cmpwi cr1,r3,-1`, the halfword at
`0x0006045E` going from `000D` to `FFFF` -- and cursor 13 draws **MAHLER**.

So BM is not missing a name by omission. The lookup hands him somebody else's, and Sega added four
instructions to stop it being shown. Between a wrong label and no label they chose none.

They did not do it for cursor 14, which is why Dural still goes by BAHN.


## What is not settled

- **Item 190**, drawn by `0x03BDB8` at the name's own position when the cursor is 13. Whatever it is, it
  leaves no visible pixels. Not chased.
- **How the ordinary path turns a cursor into a name**: `0x060184` returns an index, which is then scaled
  and read through `lhzx r3,r3,r9` at `0x0604A8` with a second table based at `0x00279670`. Which table
  holds the names was not established -- a candidate at `0x000EDFD8`, sixteen words with a zero exactly on
  BM, was **tested and ruled out**: poking BM's entry to Honey's changed nothing on screen.
- **Why cursor 13 resolves to Mahler** in particular, and cursor 14 to Bahn.
