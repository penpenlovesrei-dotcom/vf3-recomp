# Fighting Vipers 2: the console way

The port has had two play styles: the PC way, which skips the boot and plays free, and the arcade way, which has
the boot screen, coins and credits. This adds a third for Fighting Vipers 2 -- the console way, which is the PC
way plus a console's own menu over the game. Start during the attract loop picks a mode, the way the Dreamcast
version does, instead of dropping you straight into the arcade's fixed road.

Where it came from: the design, the addresses and the reasoning are the owner of this tree's own earlier work on
the game under Supermodel (`SupermodelSrc\Docs\MenuConsole.md`, and `fv2mod\HANDOFF.md` behind it). None of that
code is here -- it is GPL and was read as documentation. What this port adds is its own implementation, and the
measurements below, which were taken here.


## The decision the rest rests on: never touch the credit

The menu does not read the credit counter and never writes to it. Choosing a mode hands the game a **real press
of Start**, and the game takes a credit, or none, by its own settings.

That is what makes it work on credits and on free play alike, without this port ever having had to find Fighting
Vipers 2's free-play byte -- which it has not (see the note in the commit that added the second game). It also
means opening the menu costs nothing and cancelling costs nothing.

The practical consequence here, and it is worth saying plainly: **this port still needs a coin in Fighting
Vipers 2.** The PC way's FREE PLAY is only set for Virtua Fighter 3, so in the console way you still press 5 (or
6 for player 2) before the menu's Start can start anything. That is the known gap, not a new one.


## Where the game is: one comparison

```
in attract  =  [0x00700000] == 0
```

The state of the task Sega's own tables call `entry1`, the select screen's. Measured here through
`tools/scripts/fv2-fight.txt`, as a word:

| | `[0x00700000]` |
|---|---|
| the whole attract loop | `0` |
| the select screen | `5` |
| a fight | `0x2A` |

Only `0` or not is used, and the question it answers is "is a game running". **It is not a screen detector and
must not be made into one.** Which attract screen is up -- warning, logo, title, demo, rankings -- does not
matter, because the menu covers the screen itself.

When in doubt the safe side is "attract": a menu over a demonstration costs nothing, freezing a game nobody is
playing is a fault anyone can see.


## The four addresses, measured here

With `tools/vf3run --ram-at`. A byte of guest address `a` is `ram[a ^ 3]`: RAM here keeps the PowerPC's 32-bit
words in the host's order, and reading that wrong is the easiest way to measure nothing.

| address | what | as the board comes |
|---|---|---|
| `0x00700000` | the `entry1` task's state (a word) | `0` in attract |
| `0x0058145F` | Survival's flag | `0` |
| `0x00120550` | the operator setting Survival needs | `0` |
| `0x00120557` | the region (0 Japan, 1 U.S.A., 2 Export, 3 Asia) | `0`, this being the Japanese revision |

The region byte was tried with `VF3_POKE=120554=2` and held at `2`: the game does not write it back.


## Starting a mode is an injection

Through the same mixing point the input scripts use (`frame_inputs` in `src/host/main.cpp`). An injected press
lasts **8 frames**: a human press of Start was measured at 7 to 12 frames, and the game samples its inputs at
least once a frame, so 8 is indistinguishable from a real one.

The injections go on **last**, after the script's and the player's, because they have to get through on the very
frame the menu is still holding the player's own Start back.

And the press that worked the menu must not reach the game. Controls are handed back **when the player lets go**,
not when the menu closes -- otherwise the button that chose Arcade would still be down on the next frame and
would pick a fighter the instant the select screen appeared.

### What was measured

Two runs of the same script -- two coins at frames 1900 and 1930, and **no Start at all** -- differing only in
whether the console mode chose Arcade at frame 1960:

| | `[0x00700000]` at frame 2150 |
|---|---|
| no console mode | `0` -- still in the attract loop, nothing started |
| console mode, Arcade | `5` -- the select screen |

The control is the point: the coins alone start nothing. It is the injected Start that does.


## Survival: two bytes, and the order matters

`0x0058145F` is the flag, `0x00120550` the operator setting that has to agree with it.

**The flag must not be written during the attract loop.** Set there, it stops the game starting at all: the
injected Start is ignored and no credit is ever taken. So the menu works in three steps:

1. **armed** when it is chosen -- nothing is written, and it waits for a game to actually be running (given up
   after 300 frames rather than guessing);
2. **held** once `[0x00700000]` is no longer `0` -- both bytes written every frame, because nothing says the
   game does not clear them when a fight begins, and the roster patch had to be held for exactly that reason;
3. **put back** when the game ends. Left on, it blocks the next game and Arcade stops working.

Measured, with the same coins-and-no-Start script and Survival chosen at frame 1960, read at frame 2150 with a
game running:

```
0x0058145F = 1      the flag, which was 0
0x00120550 = 1      the operator setting, which was 0
```

and the screen says `SURVIVAL MODE` next to the fighter.


## Using it

```
vf3.exe fvipers2.zip --console
```

or Escape > Game > Play style, which now goes round three ways: PC, Arcade, Console. The third is only offered
for `fvipers2` -- everything above was measured on the Japan Revision A set, and offering it for another game
would be offering nothing. It is kept in `settings.ini` as `console_mode`; `--pc` puts it back.
`jouer-fv2-console.cmd` is the launcher.

In the console way, Start during the attract loop opens the console's page, and it is also under Escape as
*Fighting Vipers 2*. The game is paused behind it, as behind any page of this menu -- that machinery already
existed here, which is most of why this was short.

`tools/vf3run --console-mode arcade|survival --console-at N` drives the same mode machinery without a menu or a
window, which is how the measurements above were taken.


## What is not done

- **Random and Versus.** The owner's work has both, measured: Random is Start *held* from 30 frames after the
  select screen appears until 90 frames after the player's choice, and Versus is one press of Start player 2,
  30 frames after that screen appears. Both need the select screen's own timing, which this page has not
  measured here, so neither is wired up. The hooks are in `console_play` (`inject_start_p2` is there and
  unused); what is missing is the measurement, not the plumbing.
- **The pause menu.** Start during a fight should open a console's pause menu. Here Escape already does
  everything that menu would, so this was left until the modes are finished.
- **The region.** The byte is readable and writable through `Model3::fv2_region`, and holds, but nothing calls
  it yet: it has to be written at the moment the settings have just been loaded from the NVRAM, or the NVRAM
  read overwrites it, and that moment has not been pinned down in this port.
- **The Dreamcast menus' own look.** What is here is this port's menu, with the console's choices in it. The
  owner's work draws its own full-screen artwork; nothing of that is here.


## Verified

- `verifier-vf3.cmd`: Virtua Fighter 3 untouched on all five counts. It has to be -- every call is gated on the
  console way being set *and* the set being `fvipers2`.
- Fighting Vipers 2 without `--console`: unchanged, interpreter against recompiled on
  `tools/scripts/fv2-fight.txt`.
- The two measurements above, each against a control run that differs in one thing only.
