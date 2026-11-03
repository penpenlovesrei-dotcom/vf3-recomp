# Fighting Vipers 2's hidden fighters: the whole recipe

Everything needed to put Del Sol, Kuhn and B.M. on the character select again, so that nothing here has to be
searched for twice. Measured on MAME's `fvipers2` set, **Japan Revision A**, and on no other.

The code it describes is `Model3::unlock_roster` in `src/machine/model3.cpp` and `Game::hold_roster` in
`src/host/main.cpp`.


## 1. The three words

| address | as Sega wrote it | word | here | word | what it is |
| --- | --- | --- | --- | --- | --- |
| `0005F3BC` | `cmpwi r0,192` | `2C8000C0` | `cmpwi r0,0` | `2C800000` | the gate |
| `0005F444` | `li r0,14` | `3800000E` | `li r0,13` | `3800000D` | the left wrap |
| `0005F4B8` | `cmpwi cr1,r0,14` | `2C80000E` | `cmpwi cr1,r0,13` | `2C80000D` | the right bound |

The **gate**'s 192 is the operator input port: the arcade opened the hidden fighters when both operator buttons
were held at once. Turned always-true, it gives the row the game keeps for them.

The **bounds** are what makes the row the right width, and they are not in the gate's own code: they are in the
handler the gate opens. They were found by watching who writes the cursor --

    vf3run <fvipers2.zip> --watch 279668      # the address is read as hex; it names PC 0005F484

-- and reading around that PC. `0005F444` is where left from cell 0 lands; `0005F4B8` is the test, with the `ble`
at `0005FA28`, and failing it sends the cursor back to 0.


## 2. Why the bounds move, and not only the gate

The gate alone gives **fifteen** cells, 0 to 14, and the fifteenth is a **second Kuhn**: cursor 12 is id 13
(KUHN) and cursor 14 is id 12 (DURAL), the same character under his old development name, with three times the
armour. On the select screen he wears **Bahn's** name plate, the drawing routine stopping before his cell --
photographed at cursors 12, 13 and 14: KUHN, nothing, BAHN.

Taking both 14s down to 13 leaves the row 0 to 13 and wraps it back to Emi, so that cell cannot be reached.
Frederic asked for it to be shut, on the 2026-11-01.

What the row then is, measured cell by cell:

| cursor | who |
| --- | --- |
| 0 .. 10 | the game's own eleven, EMI first, CHARLIE last |
| 11 | DELSOL |
| 12 | KUHN |
| 13 | B.M. |
| 14 | does not exist: back to EMI |

Past CHARLIE the game's own names and portraits in the bottom row are wrong or missing. That is the game, not
the port: Sega never finished that part of the screen.


## 3. Why it goes in RAM, and why it is read first

This game's program is a **gzip in the CROM at offset 0**, which the game's own boot stub inflates into RAM 0.
There is nothing to patch until the boot has run. So each site is **read before it is written**: anything but
the word Sega wrote means this is not the moment (or the patch is already in), and nothing is written. Each
site stands on its own -- the gate may be in from an earlier call while a bound is not.


## 4. The part that is easy to forget: the rebuild

The recompiler makes native code from the program **as it was**. One word of RAM disagreeing with the
instruction its native code came from turns the **whole** recompiler off -- there is no per-word fallback,
because the native code is a straight-line translation and an instruction can be reached by falling into it.

So the patch goes in **just before** the rebuild after the boot, and `Game::hold_roster` builds again from RAM
if it ever writes after that:

    game.hold_roster();              // the patch
    game.rebuilt_after_boot = true;
    game.rebuild_from_ram_if_stale();   // the native code made from the patched program

Getting this wrong is what made the game run at a **sixth** of its speed once before: `docs/step2-fvipers2.md`,
section 19. The symptom in `log.txt` is the line about the interpreter running everything, and ~18 ms a frame
instead of ~5.5.

`hold_roster` is called every frame as well as at the boot. It costs three word reads when there is nothing to
do, and it does nothing at all for Virtua Fighter 3 (`unlock_roster` returns -1 for any other set).


## 5. How to check it in a minute, without touching the window

Three input scripts are kept in `tools/scripts/roster/`: `r10to12.txt`, `r13.txt` and `r14.txt`.

    set PATH=C:\msys64\mingw64\bin;%PATH%
    set VF3_HIDDEN=1
    set VF3_3D=cpu
    set VF3_DATA=%TEMP%\fv2probe
    set VF3_WINDOW=1024x768
    set VF3_SCRIPT=tools\scripts\roster\r10to12.txt
    build\win-msys\bin\vf3.exe "D:\ROMs\fvipers2.zip"

Pass the zip on the command line: a ROM folder holding both sets starts Virtua Fighter 3, and the probe then
measures the wrong game (that happened here).

The program is built `-mwindows`, so **nothing comes out on the console**. The run's report is `log.txt` in the
data folder, and the shots are `build/host_<name>.png`.

What a good run says and shows:

- `recompiled from RAM after the boot (6 words the game had patched)` -- the three exception vectors the game
  writes into itself, and these three. **Three** means the patch did not go in; a line about the interpreter
  means the rebuild did not settle it.
- `host_cur10.png` CHARLIE, `host_cur11.png` DELSOL, `host_cur12.png` KUHN, `host_r13_a.png` B.M.,
  `host_r14_a.png` EMI with the 1P marker on the first portrait.

From PowerShell, run the exe through `cmd /c`: a `-mwindows` program started with `&` is not waited for, and
the log read straight after is the previous run's.


## 6. What is deliberately not done

- **The other `fvipers2` set** is a different build and its own addresses were never looked for. It is left
  alone rather than patched at an address that means nothing there.
- **The names and portraits** past CHARLIE are the game's own. Drawing them would be a different piece of work.
- On the bare version (`fv2-stable`) there is **no setting**: the fighters are simply open. The `fv2` branch
  has them behind `full_roster` in `settings.ini`, with a menu row and `+roster` on the online version, because
  both sides of a match must patch the same thing or they never meet.
