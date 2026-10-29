# Resumption prompt — Fighting Vipers 2, the console menu, session of 2026-10-20

Paste this text at the beginning of a new session.

---

## The context

`C:\Users\frede\Desktop\fv2-recomp` — a port of Sega Model 3 games (Virtua Fighter 3 Step 1.0, **Fighting
Vipers 2** Step 2.0), derived from `mr-finster/vf3-recomp`. The PowerPC program is recompiled (SLJIT) with an
interpreter to fall back on; the board, the Real3D, the 68000/SCSP and the SDL3 host are the project's own
code. **Answer in French.**

Branch `fv2`, 36 commits ahead of `mine/fv2`, nothing pushed since the tag `fv2-v0.2.0-alpha`.

**Read `REPRISE.md` at the root before anything else.** It carries the complete state, the day's journal, and
where the original sources are. The project's documents are in `docs/`; the three that count for what follows
are `fv2-etats.md`, `fv2-menu-console-inventaire.md` and `fv2-console.md`.

## What is asked for

One key, two behaviours:

- **anywhere but the title screen** → Start brings the game back to its title screen;
- **on the title screen** → Start opens the sub-menu, the list of the seven modes (ARCADE, V.S., RANDOM,
  SURVIVAL, NETWORK, TRAINING, OPTIONS);
- in both cases Start is **withheld**, the game does not see it.

The Escape key menu can stay where it is.

## What is already in place and is of no use yet

`src/machine/model3.h`: `fv2_mode()`, `fv2_submode()`, `fv2_on_title()`, `fv2_request_title()`.
`tools/vf3run.cpp`: `--modes`, the log of the states under Sega's names.

```
MODE_MAIN 0x0058FD98   MODE_SUB 0x0058FD99   MODE_REQUEST 0x0058FD9A   SUBMODE_REQUEST 0x0058FD9B
```

Title screen = mode `ADV` (2 or 3) and sub-state 4 or 5. Getting there from the attract =
`MODE_REQUEST = 3`, `SUBMODE_REQUEST = 4`, **written once, never held**, two frames. From a fight those
levers command nothing: the game has to be left first by the game's own road (`docs/fv2-etats.md`).

## The mistake not to repeat, made TWICE on 20/10

**The console menu is not a page of the Escape key menu.** `menu.open(console_page())` opens this port's
settings RmlUi widget: whatever is put in it, it is the Escape menu that appears. Frédéric rejected it twice
in the same day.

The list of modes must be **a surface of its own**, drawn over the game's picture, on the original title
screen — not on artwork that covers it, not in the settings menu.

And the verification must bear on what is in question: noting that the game freezes does not tell the two
menus apart, since both of them freeze the game.

## The pictures

`assets/menu-console/`, 45 files, **outside the repository** (`.gitignore`). `title_list.png` is the list of
the seven modes, 709 x 897 RGBA; its bands, each line's ink and its placement on the title screen are
measured in `docs/fv2-etats.md`. **Only one version of each line exists**: no file is a "selected" variant, so
the chosen line will have to be marked some other way.

## The working rules, non-negotiable

- **No prediction.** Everything is read in the code or measured. If decompiling is needed, decompile.
- **The code says what CAN happen, the measurement says which one did.**
- **Show every capture, with no verdict.** Frédéric is the one who decides.
- **Always a `.cmd` launcher**, never a command line to copy out.
- **Never filter the build's output.**
- **Add alongside, never replace what works.** Read before writing.
- **No Sega binary, no ROM, no picture taken from the game in the repository.**
- **Take nothing from Supermodel's code nor from MAME's** — they are under the GPL. The addresses and the
  measurements are facts about the game, not code.
- **Push to `mine` only**, never to `origin` (`mr-finster`'s upstream).
- **Nothing is delivered without `verifier-vf3.cmd`**: five IDENTICAL checks and 2231 frames step by step.
- Short answers. Say what is **measured** and what is not.

## The traps, paid for

1. **A single frame of RAM differing from the instruction its native code was built from switches the
   recompiler off ENTIRELY.** Patches arrive before `rebuild_from_ram_if_stale()`, or force a rebuild.
2. **FV2's program is a gzip at offset 0 of the CROM**, unpacked into RAM 0 by the boot. Every patch is a RAM
   patch, and each one reads before writing.
3. **`board.ram` keeps the 32-bit words in the host's order**: the byte at address A is at index `A ^ 3`.
4. **`tools/vf3run` NEVER loads the NVRAM.** No operator setting can be proved with it.
5. **`vf3run` only renders the picture on a script's `shot` marks** (and `--hd-dump`).
6. **Holding a mode request breaks the game** — the select screen and its clock, twice. A request is written
   once.
7. **Bash heredocs eat apostrophes and escapes**: write the files with the Write tool.
8. **The repository's files are CRLF**: an insertion script looking for `\n` finds nothing.
9. **Linking fails with "Permission denied"** if the game or the verifier is still running.
10. **An identical PNG size proves nothing**, and two different footprints prove nothing either. The picture
    has to be looked at.

## The original sources, outside the repository

```
Documents\SEGA Model 3\SupermodelSrc\Docs\MenuConsole.md    3689 lines -- ALL the measurements on the ROM
Documents\SEGA Model 3\SupermodelSrc\Src\Menu\GameFV2.h     the addresses and the geometry
Documents\SEGA Model 3\ROMs\fvipers2.zip                    the ROM
```

`grep -n "^#" MenuConsole.md` gives its table of contents. §6 (knowing where the game has got to), §6 bis
(leaving a fight), §6 quater (the states named by Sega), §6 sexies (the menu on the real title screen), §13
(the menu's pages and their geometry).

## What remains open

- **The second player has never been got in** (`0x0058145A == 3`). A test-harness gap, not a knowledge one.
  Closing it would serve the Versus loop and the real condition for unlocking Honey.
- The ending, the name entry, the ranking and the Super K.O.: named, never reached.
- What *decides* to rebuild a fighter (`0x00060288`): unknown. What *commands* it is known and used.
- Survival's branches have never been watched running.

## Where to start

1. Read `REPRISE.md`, then `docs/fv2-etats.md`.
2. Wire Start to the two paths — `fv2_on_title()` decides which.
3. The surface: `title_list.png` over the game's picture. It is the one piece with no earlier work to take up.
