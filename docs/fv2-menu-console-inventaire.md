# The console menu: everything it needs, and what is already here

An inventory, not a plan. For each thing the console menu has to do, this says which lever the game
answers to, where that lever was established, and whether this port already has it.

Two sources, and they are complementary. The owner's `SupermodelSrc\Docs\MenuConsole.md` is where every
lever below was first measured, one at a time, by playing the game and watching memory. `docs/fv2-flux-de-match.md`
is the decompilation of the match flow, which came later and explains *why* those levers are the levers:
the mode machine, its states under Sega's own names, and the scheduler that reads two words per task every
frame. Neither replaces the other -- the first says what to write, the second says what the game is doing
when you write it.

Nothing here is new work. It is a reading of what exists, so the next person does not rediscover it.


## 0. The spine, which this port already has

This is the part that would have been the longest, and it is done. Supermodel needed all of it built; here
it fell out of owning the host.

| | where |
|---|---|
| a menu that draws over the game, with pages, a cursor and a back stack | `src/host/menu.*`, RmlUi |
| the game frozen behind it | `stopped = paused \|\| menu.is_open()` in `src/host/main.cpp` |
| one place where every input is composed, so anything can be interposed or withheld | `frame_inputs` |
| injecting a press as the player's own | `console_play.h`, 8 frames, measured against a human's 7-12 |
| knowing whether a game is running | `Model3::fv2_in_attract`, the entry1 task's state at `0x00700000` |
| free play, so a menu that starts a match costs nothing | `Model3::coin_offset`, backup RAM `0x17` |
| reading and writing the operator settings | `Model3::backup`, and the Arcade settings page |
| reading a PNG from a folder | `src/host/png.h` |
| the owner's own artwork for the menu | `assets/menu-console/`, 45 files, outside the repository |


## 1. The modes

| line | lever | state |
|---|---|---|
| `ARCADE` | inject Start, 8 frames | **done** (`console_play::choose_arcade`) |
| `SURVIVAL` | `0x0058145F` the flag, `0x00120550` the operator setting; armed on choosing, written only once a game runs, held, put back at the end | **done** (`Model3::fv2_set_survival`) |
| `RANDOM` | Start *held* from 30 frames after the select screen appears until 90 frames after the player's choice. The hold is what matters: an earlier attempt waited for the choice and so missed the part that counts | measured in `MenuConsole.md` §8, **not wired** |
| `VERSUS` | one press of Start player 2, 30 frames after the select screen appears | measured §8, **not wired** |
| the full roster | `0x0005F3BC`, `cmpwi r0,192` -> `cmpwi r0,0` | **done** (`Model3::unlock_roster`) |
| the region | `0x00120557`, 0 Japan 1 U.S.A. 2 Export 3 Asia, written when the settings have just been read from the NVRAM | **readable and writable** (`Model3::fv2_region`), not called |

A closed point, recorded so it is not reopened: the select screen's countdown at `0x00581408` (shown as the
halfword at `0x0058140A` >> 6) cannot be extended. Written once the game overwrites it; written every frame it
never expires and the match never starts. Reloading often enough is freezing under another name.


## 2. The pause menu

The four lines are the spine of the thing, and all four were measured.

| line | lever |
|---|---|
| `TITLE SCREEN` | `[0x00580DFC] = 14` (the attract list's last real entry) and `[0x00580DD2] = 0x7FFF` (its timer, long expired). Written **once**, not held: the game then walks off the end itself, which is the only way it takes the branch that installs the off-list run. Writing 15 does not work -- the sequencer tests the entry *after* the current one against the list's terminator and runs away |
| `CHANGE YOUR VIPER` | end the match off screen, in about two seconds, with three bytes: the round counter `0x00581455`, the round timer `0x00581406`, the screen-duration counter `0x0058FD56` |
| `REMATCH` | do **not** leave the fight. The game relaunches a round itself; the finding was that the loser's task stops at the end of a match. Player 2's task state is `0x00700100` |
| `CHANGE STAGE` | `0x0007111C` |

None of the four is wired here. What the flow document adds is that they can now be reasoned about: the match
loop is `SET_INT -> READY -> FIGHT -> JUDGE -> REPLAY -> VIC -> NEXT_PROG_SEL -> SET_INT`, and `ROUND_MASK_*`
-- the STAGE / VS screen -- runs only when the opponent changes.

A warning that cost the owner a session, and which the decompilation now explains precisely: **the two writes
Start leaves behind do not open the select screen.** `0x0005EFE4` stores the resume address `0x0005ED48` and
`0x0005EFEC` stores a state. Reproducing them breaks the game, from a fight and from the attract loop both.
They are what a routine leaves behind. What *commands* a task is the pair (bit 0 of its block, the word after
it), read by the scheduler at `0x00039174` sixty times a second -- which is a different thing, and is how
Honey's preview is rebuilt (`Model3::fv2_redo_fighter_model`).


## 3. OPTIONS

The twelve `GAME ASSIGNMENTS`, in the settings image in RAM, `0x00120540` to `0x0012055F`. Named by Sega's own
test-menu tables. `0x0012054E` difficulty, `0x00120550` survival, `0x00120557` region; the rest are in
`MenuConsole.md` §9 with a byte each.

This port already reads and writes that block for Virtua Fighter 3's Arcade settings page, and already knows
that changing one restarts the game when the menu closes. The work is the page, not the mechanism.


## 4. STATS -- the VS DIAGRAM

Drawn by the menu itself, from counters in the backup RAM. Their layout is `MenuConsole.md` §12, with the
offsets of each block, and `fv2mod\vs_blocs.py` reads them without launching the game.

The hardest part is not the data, it is the drawing: a table, per-character pages, a TOTAL line, alignment on
the ink rather than the box. §13 is entirely about that.


## 5. SOUNDS

The sound-board protocol is MIDI over `0xF0080000`, established by sending and listening. The twenty musics
and the effects are listed in §14. This port's sound board already plays, and the music/effects split is
already separate.

Not settled by anyone: the owner's music replacement never produced a sound.


## 6. DISPLAY

This port's own settings rather than the game's, and the ones that matter already exist here -- the picture
shape, the 3D renderer, the scale, the HD packs. A page over them, not new machinery.


## 7. What is genuinely not known

Four gaps, and it is worth being exact about which block anything.

- **The second player was never got in.** `0x0058145A == 3` -- 1 for player 1 having entered plus 2 for player
  two -- gates the cut-in, the consecutive-win counter and the two-player half of `NEXT_PROG_SEL`. All of it is
  read in the code and none of it has been watched running. This is a *harness* gap, not a knowledge one: it
  needs a second player's coin and Start at the right moment. Closing it would serve the Versus loop, Honey's
  real unlock condition, and whatever `NEXT_PROG_SEL` does with two players.
- **The ending, the name entry, the ranking and the Super K.O. states** were never reached. Named from Sega's
  table, never run.
- **What *decides* to rebuild a fighter** -- the test at `0x00060288` -- is unknown. What *commands* it is known
  and used. This blocks nothing.
- **Survival's branches** were never watched running, although the mode itself works here.

And one correction worth carrying: `0x0000ABC4` is not `fn_ecrit_progression`, as `fv2-arcade-notes/symbols.py`
has it. It is the main mode machine's tick -- `--watch 58FD98` shows `B_MAIN_MODE` written at `0x0000AC10`
with `LR 0x00039068`, and `0x00039064` is `bl 0x0000ABC4`.


## 8. The one thing that has no lever at all

The surface. Everything above is about making the game do something; none of it draws a menu that looks like a
console's. This port's menu is its own settings menu with pages added, which is not the same object: the owner's
covers the screen with its own image and opens on Start.

That is the only part with no prior art to lift here, and it depends on nothing in section 7. It is also what
makes the rest visible -- including the fact that the game gives no feedback at all for which costume is
selected, its own two included.
