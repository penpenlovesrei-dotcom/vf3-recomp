# Fighting Vipers 2: the hidden fighters, made selectable

Fighting Vipers 2 ships with a select screen wider than the one it shows. Its cursor handler has a second mode
where the cursor wraps at 14 instead of 10, and that mode puts Del Sol, Kuhn, BM and Dural on the row with
everyone else. Sega left it in and gated it behind something no player can reach.

This is what the port does about it, what was measured to settle each point, and what it does not fix.

Where it came from: the gate, its address and the shape of the patch were not found here. They come from the
owner of this tree's own earlier work on the game under Supermodel — `fv2-arcade-notes` (`roster.py`, and the
section its README calls "Débloquer tout le roster : un octet et demi") and the `-menu-full-roster` option of
his patched Supermodel build. What this port adds is the port's side of it: where the write belongs when the
program is recompiled rather than interpreted, and the measurements below, which were taken here.


## The gate

One instruction decides it:

```
0005F3BC    cmpwi r0,192        the mode is on when r0 is 192
```

What is in `r0` is the operator input port -- both operator buttons, Service and Test, held at once. Not a flag
the game keeps, and not something saved: the raw state of the port, refreshed every frame. Writing to the port's
own address does nothing, because the next frame overwrites it. And the combination cannot be held by hand
either: pressing Test opens the test menu, so the select screen is gone before the second button matters.

The comparison is what gives:

```
0005F3BC    cmpwi r0,0          the mode is on when no operator button is held
```

which is always. Four bytes: `2C 80 00 C0` becomes `2C 80 00 00`.


## The address is in RAM, which is the whole difficulty

For Virtua Fighter 3, the program sits in the fixed CROM and the boot copies it to RAM, so a patch can go into
the ROM image before the game ever runs -- that is how the FREE PLAY notices are hidden
(`Model3::hide_free_play_text`). Fighting Vipers 2 is not like that. Its program is a gzip at the fixed CROM's
offset 0 which the game's own boot stub inflates into RAM at 0 (see `src/machine/romset.cpp`). There is nothing
at `0005F3BC` to patch until the boot has got that far, and there is no ROM copy of it to patch instead.

So the write has to happen at runtime, and it has to wait. Writing there before the inflate has reached that
address lands in a part of the program that is not decompressed yet; the PowerPC then leaves the rails some
frames later with nothing on screen to say why.

Hence the one rule the code keeps: **read before writing, every time**. Anything but the instruction Sega wrote
means this is not the moment, and nothing is written.

```cpp
if (word(ram, SITE) != AS_SEGA_WROTE_IT) return 0;
word(ram, SITE) = ALWAYS;
```


## How often it has to be written: measured, not assumed

The Supermodel-side notes say the game reloads that stretch of code and the patch has to be put back again and
again. That was worth checking here rather than carrying over, because a port that recompiles the program pays
for every write (below).

`tools/vf3run --watch 5F3BC` reports every store the CPU makes to that word. Over a whole run of
`tools/scripts/fv2-fight.txt` -- boot, attract loop, select screen, a fight to the end, 4200 frames:

```
store32 0005F3BC = 0  at PC FFF018DC (LR FFF0153C)
store8  0005F3BC = 2C at PC FFF02938 (LR FFF03350)
store8  0005F3BD = 80 at PC FFF02938 (LR FFF03350)
store8  0005F3BE = 0  at PC FFF02938 (LR FFF03350)
store8  0005F3BF = C0 at PC FFF0275C (LR FFF03350)
```

Five stores, and that is all of them. Every one is at a boot PC in the CROM -- the inflate clearing the word and
then writing the instruction into it. Nothing touches it afterwards: not entering the select screen, not
starting a fight, not the whole match.

So **one write, once the boot has unpacked that address, is enough** for everything this port has been through.
The check is still made every frame, because it costs one comparison and it covers the run this port has not
been through; if it ever does write a second time it says so in the log and rebuilds the native code.


## Why the write has to land before the recompiler rebuilds

This is the part that is the port's own problem and has no Supermodel equivalent.

The program is recompiled into native code at start-up, from the ROM image. A word of RAM that no longer agrees
with the instruction its native code was made from turns the **whole** recompiler off -- there is no per-word
fallback, because the native code is a straight-line translation and an instruction can be reached by falling
into it. That is why the port already rebuilds from RAM once the boot is over: Fighting Vipers 2 writes its own
handlers into three of the PowerPC's exception vectors, and without the rebuild the game runs entirely on the
interpreter (see `docs/step2-fvipers2.md` section 19 -- the three words that made it slow).

The roster patch is a fourth such word. So it goes in **before** that rebuild, and the rebuild then makes the
native code from the program as it will actually run. In the log:

```
recompiled 69716 instructions (68771 native) into 16014 KB in 105 ms
frame 64: the hidden fighters are selectable
recompiled from RAM after the boot (4 words the game had patched): 69801 instructions in 99 ms
```

Four words, not three. And afterwards:

```
interpreter instead: ... 0 outside the program, 0 without code
```

The recompiler stays on. Nothing is given up for this.


## What it does

Measured, from the select screen, one press of left from Emi (cursor 0):

| | cursor after | the row |
|---|---|---|
| as the game ships | 10 | wraps at the random slot |
| with the patch | 14 | Del Sol, Kuhn, BM and Dural are reachable |

This page first said Kuma, from an older note. It is Dural: the owner's own cursor-to-fighter table, measured
later, gives cell 14 as Dural and says Kuma is reachable by no cursor at all. What was seen here agrees -- the
fighter on cell 14 is the chrome one, not the bear. Who cells 11, 12 and 13 are was not checked here; the older
note gives Del Sol, Kuhn and BM, in that order.

Read at `00279668` (P1) and `00279669` (P2), the cursor positions named in Sega's own debug tables. The two
pictures differ, and the port's windowed host produces a picture byte-identical to the headless run's.

It is permanent: it is the cursor handler's own mode, not a one-shot.


## What it does not fix, and that is the game's doing

Past cursor 10 the names and portraits are wrong or missing. The routine that draws them stops there, and its
table of positions holds eleven entries. The fighters load and play; the label above them is somebody else's.
**Corrected 2026-10-23, see `docs/fv2-select-names.md`:** cursors 11 and 12 draw the RIGHT name,
the routine does not stop, and there is no table overrun. Cursor 13 is blank because of an explicit
`cmpwi cr1,r3,13` at `0x0006045C`; without it, it would read MAHLER.

Nothing here tries to paper over that -- it would mean building a name table Sega never wrote.


## Using it

```
vf3.exe fvipers2.zip --full-roster
```

or `full_roster = 1` in `settings.ini`, which is where `--full-roster` saves it. `--no-full-roster` turns it off
again. `play-fv2-roster.cmd` is the launcher.

`tools/vf3run --full-roster` does the same thing without a window, which is how the measurements above were
taken.


## The other Fighting Vipers 2 set

`fvipers2o` (Japan, no revision letter) is left alone. It is a different build; its own address for this gate was
never looked for, and patching `0005F3BC` there would be patching an address that means nothing. The code says so
and the game reports it rather than doing nothing silently.


## Verified

- `tools/scripts/fv2-fight.txt`, 4200 frames, `--full-roster` on, interpreter against recompiled: CPU state
  every frame and the 8 MB of RAM at the end, both byte-identical.
- The windowed host's picture at the select screen, byte-identical to the headless run's.
- `verify-vf3.cmd`: Virtua Fighter 3 untouched. It has to be -- `unlock_roster()` is never called unless the
  player asks for it, and it returns at once for any game but `fvipers2`.
