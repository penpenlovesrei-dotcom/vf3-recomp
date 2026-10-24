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

That was true of the menu, and for a while it left the console way broken in a way only playing it showed: the
menu opened, a mode was chosen, and nothing happened. The Start was being handed over correctly; the game was
ignoring it, having no credit. A console has no coin slot, so asking the player for one was not an answer.

Feeding the game coins was tried and is not what a console does either. The answer was to put the board on
**FREE PLAY**, as the PC way already did for Virtua Fighter 3 -- which needed the one thing nobody had: where
this game keeps that setting.

It is one byte, `0x17` of the backup RAM, and it holds the COIN/CREDIT SETTING numbered as Virtua Fighter 3's
is, 1 to 27, with 27 being FREE PLAY. How it was found is in the next section. The console way is the PC way
plus a menu, so it now comes up on free play and a mode costs nothing.


## Free play: one byte, and how it was found

Neither this port nor the Supermodel-side work that preceded it had this. That work says so in as many words --
its console menu worked "without its byte ever having been found", by never deciding about credits at all.

What found it was the game's own test menu, driven from an input script. `tools/vf3run`'s scripts can press
`test` and `service`, which are the two cabinet buttons that menu is worked with, and `--shots` shows where the
cursor is, so the menu can be walked blind and then looked at:

```
TEST MENU             Test opens it, cursor on EXIT
                      Service moves, Test selects -- the screen says so
  9 x Service         down to COIN ASSIGNMENTS (the cursor wraps to the top and descends)
  Test                COIN/CREDIT SETTING #1 -- one coin, one credit
```

Then FREE PLAY was set there by hand, the game left, and the backup RAM compared with a copy taken before.
Seven bytes had moved:

```
0x0000B :  1 ->  4      the other lines of that page
0x0000F :  1 ->  3
0x00013 :  0 ->  6
0x00017 :  1 -> 27      COIN/CREDIT SETTING, #27 being FREE PLAY
0x01077 :  0 ->  2      bookkeeping
0x014CE : 77 -> 84
0x014CF : 228 -> 253
```

Seven bytes is not an answer. The one that matters was picked out by putting the other six back and leaving only
`0x17` at 27: the game still came up in FREE PLAY, and the picture was **byte-identical** to the run with all
seven changed. So there is no checksum to keep and no neighbour that has to agree -- `Model3::coin_offset`.

Two things fell out of this that are worth keeping:

- the numbering is the same as Virtua Fighter 3's, 1 to 27 with 27 being FREE PLAY, although the byte is at the
  other end of the backup RAM. The two games share their test menu's framework, not its addresses.
- `tools/vf3run` **never loads the backup RAM**. It starts every run on a board that has never been set, and
  the game writes its factory settings in the first frames. So nothing about an operator setting can be proved
  with it, and this was all done with the windowed host, which does load it. That cost a measurement before it
  was noticed.


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


## Start: one button, two jobs

Added the 2026-10-21. In the console way Start is the console's button and the game never sees it. What it does
is decided by one question -- is the game's OWN title screen on screen? -- and `Model3::fv2_on_title` answers
it from the game's state machine (`docs/fv2-etats.md`), which is not the question `fv2_in_attract` answers.

    on the title screen   ->  the list of modes opens over that screen
    elsewhere in the attract loop  ->  the game is asked for its title screen
    while a game is running        ->  NOTHING, and the press goes to the game

The decision is `console_play::take_start`, so the windowed host and `vf3run` make it the same way, and
`vf3run --modes --console-start N` measures it without a window. What that measured, with the game's own mode
log beside it, is the table in `docs/fv2-etats.md`: the title screen comes back one frame after the request
from the SEGA logo, from the demonstration, from the select screen and from inside a fight alike.

**But a request does not end a game.** Read back afterwards, the entry1 task is still in its playing state
(0x2A from a fight, 0x2D from the select screen). The title screen is back over a game that is still running,
so `fv2_in_attract` stays false. Leaving a fight properly is the game's own road, and it is not wired.

### Et ce que ça donnait à l'écran, mesuré le 2026-10-23

Ce paragraphe était écrit depuis le 21 sans que personne ait regardé le résultat. Frédéric l'a trouvé en
versus : **Start en plein combat, et l'écran titre revient déchiré.** Les couches de tuiles hachées les unes
dans les autres, les noms du roster imprimés à travers le logo, toute l'image en rayures. Reproduit sans
fenêtre sur la route `tools/scripts/fv2-console-vs-start.txt` — image 3001 d'un combat versus, le mode passe
de `GAME_DSP / FIGHT_DSP` à `ADV_DSP / FV2_TITLE_DSP` et la photo est illisible.

**Donc on ne demande plus rien depuis une partie.** Il n'y a pas de bon écran à demander de l'intérieur d'un
combat : la demande déplace l'écran sans arrêter la partie, et c'est cette superposition qui déchire l'image.
La règle devient `fv2_in_attract` — hors attraction, cette couche ne fait rien.

**Et elle ne retient pas l'appui non plus** : il part au jeu comme le Start de la borne, ce qui est ce qui
laisse un deuxième joueur s'inviter dans un match en cours. Vérifié : à la même image 3001, le combat
continue, l'image est intacte, et le match va jusqu'à son `NEXT_PROG_SEL` et son round suivant.

Ce qui reste non résolu est ce qui l'était déjà : **sortir d'un combat proprement**, par la route du jeu.

The press is withheld whichever way it went, and the next one is not read until this one has been let go --
otherwise the button that opened the list would still be down on the frame the list hands the game a Start of
its own, and would pick a fighter the instant the select screen appeared.


## The list of modes is a surface of its own

`src/host/fv2_mode_list.h`, and the thing to say first is what it is NOT: it is not a page of this port's menu.
That was built twice, and rejected twice, and the reason is not a matter of taste -- the menu under Escape is
this port's settings, an RmlUi document with its own backdrop, and a page added to it is a page of settings
whatever is written in it. This draws the owner's own picture of the seven lines straight over the picture the
game made, on the game's real title screen, with nothing else on it.

The picture is `assets/menu-console/title_list.png` (709 x 897, outside the repository), and every number in
the drawing is the measurement written down in `docs/fv2-etats.md`: the seven bands cut in the middles of the
white, each slid sideways to bring its own left-aligned ink to the middle, the block at 0.26157 of the
picture's height and 0.35557 of it tall, the width following the picture's proportions. On top of the
measurement are the two things the owner asked for, kept as factors applied to it rather than as the figures
they work out to: grown by 7/6 about its own middle, lifted by 50/1080.

One version of each line exists -- none of the 45 files is a "selected" variant -- so the chosen line is marked
by what it does rather than by what it is: it blinks, its alpha falling to 0.5 and back over 45 frames, which
is the owner's own answer to the same problem.

**The picture behind goes to half** while the list is up, which is what the Dreamcast version does and what
makes the lines readable over the logo. The figure is measured, not chosen: two captures of that version, the
same screen with its menu and without, give 255 -> 128 on all three channels (median 0.502 over 56000 pixels
outside the menu's own block). This port draws black at alpha 128 over the whole picture, under the list, and
the same measurement on its own two captures gives 0.500.

The game is frozen behind it, as behind this port's menu, and its sound with it: the title screen lasts 192
frames of its own, and a list that let the game walk on would end up over a demonstration. Up and down move;
Start, the pad's A, Enter or the space bar choose; **the pad's B or Backspace back out** and leave the game
where it was; and Escape still opens this port's menu, which closes the list. On the keyboard, choosing and
backing out are the player's own punch and kick bindings, because that is where A and B sit on the pad here.

**The list has no sounds of its own, and that is on purpose for now.** A first attempt at them was taken out
the same day it was written (see the journal in REPRISE.md): it ran the sound board under a frozen game, sent
the MIDI reset behind the game's back and put the music back by hand, and it was put together in three patches
on three reports rather than from a plan. What a second attempt has to answer, before a line of it is written,
is in `docs/fv2-console-son.md`.

ARCADE, SURVIVAL, **RANDOM** and **V.S.** act, and that is every mode the arcade game has. **TRAINING** acts
too, as far as it has been built: it fights in Marble Arena. NETWORK and OPTIONS are drawn and leave the list
up rather than pretending something happened -- and they are not waiting for a lever to be found, because
there is none to find.

### TRAINING's arena, and the lever that is NOT the one from August

There is no training mode on this board, so there is nothing to switch on: it is a mode this port builds, and
what exists of it today is which arena it fights in. Frédéric's choice for now is **MARBLE ARENA**, scene 3.

The first lever tried was the stage picker at `0x0007111C`, the one the owner's `CHANGE STAGE` patches to
`li r3,<scene> ; blr`. **It does not command this.** Patched and held -- the words read back out of the
running game -- the game picked stage 9 all the same. That picker is reached only by a rematch going THROUGH
the select screen, which is how `CHANGE STAGE` uses it.

What commands a game's first arena is the game's own **route table**, found by watching who writes the scene
index and reading the dozen instructions above that write (`docs/fv2-etats.md`): eight bytes a match, four
(arena, opponent) pairs, the pair chosen by `0x0058145D`. One byte of it -- `0x000B4100`, route 0, first
match -- set to 3 puts the fight in Marble Arena, and it is put back when the game ends.

Measured both ways: the scene index reads 3 where it read 9, and the picture is the marble floor instead of
the dam.

### NETWORK, TRAINING and OPTIONS do not exist on the arcade board

This is not a gap in what has been read: it is what the game is. Sega's own twenty-four modes
(`docs/fv2-etats.md`) are WARNING, ADV, SEL, GAME, CONTINUE, INTRUDE, ENDING, NAME, RANKING, OVER, TEST and
RESULT. No training, no network, no options. And the owner's arcade notes say it in as many words:

> Le mode Versus n'existe pas comme mode distinct : dans Fighting Vipers 2, c'est le mode arcade avec un
> second joueur qui s'incruste. Il n'y a pas de mode Training.

Which also says that V.S., as this port does it -- a second player's Start into an arcade game -- is not an
approximation of a Versus mode. It **is** what Versus is on this board.

So the three lines are the Dreamcast's, and each would have to be BUILT rather than switched on:

| line | what the arcade actually has |
|---|---|
| `NETWORK` | nothing. The disc's `INTERNET` and `RANDOM NETWORK` have no counterpart in this program |
| `TRAINING` | a **stage**, not a mode. A training mode is this port's own, and its first half is built: it fights in **MARBLE ARENA**, by one byte of the game's route table (below). The rest -- a timer that does not run out, health that comes back, the dummy -- is still to be read |
| `OPTIONS` | the operator's own page, `GAME ASSIGNMENTS` at `0x00120540`-`0x0012055F`. This port already reads and writes that block for Virtua Fighter 3's Arcade settings page, so what is missing is the page, not the mechanism |

That settles what the list can be. It is **complete for the game underneath it**; what is left is work this
port would do itself, and it should be decided as such rather than listed as a hole.

### V.S., and the second player who had never been got in

The lever is the owner's, recorded in August: **one press of Start player 2, 30 frames after the select screen
appears**. Nothing else, and nothing written to the game's memory. What this port was missing was not the
lever but the two moments: the select screen appearing is the mode becoming SEL (4 or 5), and the thirty
frames clear the transition and the Start that opened the screen.

Measured: the select screen comes up with **two players on it**, 1P and 2P with their own cursors, and the
player-2 task `0x00700100` reads 5 -- the select screen's value -- where it reads 0 in every run before this
one. That closes a hole open since August: `docs/fv2-menu-console-inventaire.md` called the second player a
harness gap, not a knowledge one, and said it needed a coin and a Start at the right moment. It did.

### RANDOM: the game's own flag, after the hand manoeuvre refused to work

The lever recorded in August is Start **held** from 30 frames after the select screen until 90 past the
player's choice. Held here over exactly that window, it does **nothing at all**: the game's whole memory at
the frame of the choice is byte-identical to a run that held nothing, all 8 MB. And the control says it is
not this port's doing -- an input script holding Start over the same window gives the same zero. The hold
reaches the game, and the game ignores it.

So the question was asked the other way round, which is what the decompilation is for: **what does the game
set when it goes into that mode?** Section 5 of `docs/fv2-flux-de-match.md` lists the variables under Sega's
own names, and there it is, one line below Survival's flag:

```
0x0058145E   MODE_INFO   RANDOM_CHARA
```

Set, the select screen prints RANDOM MODE beside the fighter -- the same thing the hand manoeuvre produced
over there. The hold was a way of asking the game to set that byte; asking for the byte is shorter, and it is
measurable.

Two measurements decide how it is written, and both are the opposite of Survival's:

| | |
|---|---|
| does it have to be held? | **no** -- written once at frame 2000, RANDOM MODE is still up a hundred frames later |
| does writing it in the attract loop stop the game starting? | **no** -- written from frame 1800 the game still starts (`GAME_INT` at 2296), where Survival's flag there stops it dead |

It is armed on choosing and written once a game is running all the same: that shape costs nothing, it is the
one Survival already uses, and it is what gives a moment to put the flag back when the game ends.

### What was measured, and how

Driving something drawn over a frozen game needs hooks that do not go by the game's frames, because those
stop. Two were added to the host, and they are what made this verifiable at all:

```
VF3_QUIT_AFTER_S=N    end the run after N seconds of real time
VF3_KEYS_S=k@sec,...  press key k at that many seconds
```

With those, `capturer-fv2-liste.cmd` takes the three pictures in `build\`, hidden, without a hand on a pad:

| picture | what it shows |
|---|---|
| `liste-1-arcade.png` | the seven lines over the game's own title screen, FREE PLAY still readable under them |
| `liste-2-survival.png` | three presses of down, then Enter: the game starts, and the select screen says SURVIVAL MODE |
| `liste-3-retour-au-titre.png` | Start during the demonstration: the title screen is back |

The second is the one that matters most, because it is end to end: the list opened on the real title screen,
its own cursor moved, the line chosen handed the game a Start of its own, and the game began a match on free
play without a coin.


## Using it

```
vf3.exe fvipers2.zip --console
```

or Escape > Game > Play style, which now goes round three ways: PC, Arcade, Console. The third is only offered
for `fvipers2` -- everything above was measured on the Japan Revision A set, and offering it for another game
would be offering nothing. It is kept in `settings.ini` as `console_mode`; `--pc` puts it back.
`jouer-fv2-console.cmd` is the launcher.

In the console way, Start opens the console's own list of modes on the game's title screen, and brings that
title screen back from anywhere else (above). The same choices are also under Escape as *Fighting Vipers 2*,
as a page of this port's menu -- that page stays where it is, and it is not the list.

`tools/vf3run --console-mode arcade|survival --console-at N` drives the same mode machinery without a menu or a
window, which is how the measurements above were taken.


## What is not done

- **Random and Versus.** The owner's work has both, measured: Random is Start *held* from 30 frames after the
  select screen appears until 90 frames after the player's choice, and Versus is one press of Start player 2,
  30 frames after that screen appears. Both need the select screen's own timing, which this page has not
  measured here, so neither is wired up. The hooks are in `console_play` (`inject_start_p2` is there and
  unused); what is missing is the measurement, not the plumbing.
- **The pause menu** — **construit le 2026-10-24**, les modes étant finis. Voir plus bas.
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

## Le menu de pause

`src/host/fv2_pause.h`, construit le 2026-10-24. Start pendant un match l'ouvre par-dessus le combat, le jeu
gelé derrière ; Start le referme. C'est la troisième surface de ce portage, après la liste des modes et la
page OPTIONS, et elle est faite comme elles : elle gèle le jeu, prend le clavier et la manette, dit au seul
hôte ce qu'elle veut entendre (`want_sound`), et ne connaît rien de RmlUi.

### La forme est celle du disque

Frédéric a montré l'écran de la Dreamcast, et c'est lui qui est suivi, pas une page de lignes centrées :

- un **panneau** violet translucide sous une **barre pâle** qui porte `1P PAUSE`, avec un montant clair le
  long de son bord gauche ;
- **à gauche pour le joueur 1, à droite pour le joueur 2** — le panneau se range du côté de celui qui a mis
  en pause, et l'hôte sait lequel c'est par le Start qui est arrivé ;
- **l'écran assombri** derrière, et le combat qui reste lisible dessous ;
- les lignes **alignées à gauche** dans le panneau, la choisie en jaune ;
- les deux aides **séparées en bas**, à gauche et à droite, comme là-bas.

Toutes les mesures sont en fractions de l'écran 4:3 du jeu, comme partout ailleurs ici, donc le panneau garde
sa place quelle que soit la fenêtre.

### Les quatre lignes, et ce qui leur manque

Ce sont celles d'août (`docs/fv2-menu-console-inventaire.md` §2). Une seule a un levier qui marche d'ici ;
les trois autres sont **dessinées, en sourdine et enjambées**, la règle que la page OPTIONS suit déjà.

| ligne | état |
|---|---|
| `TITLE SCREEN` | **mesurée inopérante depuis un combat.** Le levier d'août est celui que ce portage appelle `fv2_attract_to_title` — pousser le séquenceur d'attraction au bout de sa liste, chrono expiré. Essayé depuis un combat : rien. Le séquenceur n'y tourne pas, il n'y a pas de liste à quitter ; le round s'est fini et le suivant a commencé |
| `CHANGE YOUR VIPER` | la note d'août donne trois **adresses** — `0x00581455`, `0x00581406`, `0x0058FD56` — et pas les trois valeurs |
| `REMATCH` | la note décrit ce qui a été observé, pas un levier |
| `CHANGE STAGE` | **câblée** : le sélecteur de décor que TRAINING utilise déjà, tenu sur la scène choisie. Gauche et droite changent le numéro sur la ligne |

Ce qui bloque les trois est la même chose que ce que `docs/fv2-etats.md` appelle depuis le début **sortir d'un
combat proprement**, par la route du jeu.

### Vérifié

En partie cachée, sur `tools/scripts/fv2-console-vs-start.txt` : Start à l'image 3000 d'un combat versus
(`GAME_DSP`, `FIGHT_DSP`) ouvre le panneau, les flèches enjambent bien les lignes sans levier, Start referme,
et le combat reprend intact. Les photographies sont prises à l'horloge et non à l'image, le jeu étant gelé —
une image qui n'avance pas ne déclenche jamais une capture calée dessus.

### Start a un quatrième sort : couper l'état, dans la présentation et la rediffusion

Frédéric, le 2026-10-24 : « la touche pause pendant l'état de présentation du combat et pendant le replay ne
doit pas ouvrir le menu pause mais juste couper l'état en cours comme dans le jeu d'origine ».

Les états concernés, par les noms que Sega a laissés (`docs/fv2-etats.md`, les vingt sous-états de GAME) :

```
 0 ROUND_MASK_INT   1 ROUND_MASK_IN   2 ROUND_MASK_DSP   3 ROUND_MASK_OUT    l'ecran STAGE n / VS nom
 4 ROUND_INT        5 ROUND_DSP                                              l'annonce du round
 6 SET_INT          7 SET_INT_POSE    8 SET_DSP                              le decor et les combattants places
 9 READY_INT       10 READY_DSP                                              « ready »
15 REPLAY_INT      16 REPLAY_DSP                                             la rediffusion
```

`FIGHT_DSP`, `JUDGE` et `VIC` n'y sont **pas** : le menu de pause leur appartient.

**Ce qui ne marche pas, et c'est la première chose mesurée.** Laisser la pression passer au jeu ne fait
**rien du tout**. Un vrai Start à l'image 2360 de `tools/scripts/fv2-vic-banc.txt`, pendant `ROUND_MASK_DSP`
qui court de 2319 à 2450 : la trace des états est identique, image par image, avec et sans. Le jeu ne lit pas
ce bouton là. L'hypothèse « il suffit d'arrêter d'intercepter » est donc fausse, et elle a été écartée par la
mesure avant d'écrire une ligne de correction.

**Ce qui marche, et c'est la route du jeu lui-même.** Les gestionnaires de sous-état se passent la main par
une **demande de sous-état** — `docs/fv2-flux-de-match.md` le dit de `VIC_INT` : « simply asks for the next
sub mode ». Une écriture d'une seule image de `SUBMODE_REQUEST` (`0x0058FD9B`) à 3 pendant `ROUND_MASK_DSP` :

```
  2319  GAME_DSP   2 ROUND_MASK_DSP
  2361  GAME_DSP   3 ROUND_MASK_OUT      <- au lieu de 2450
  2362  GAME_DSP   4 ROUND_INT
  2364  GAME_DSP   6 SET_INT
  2366  GAME_DSP   8 SET_DSP
```

89 images coupées, et la suite s'enchaîne normalement. C'est `Model3::fv2_cut_submode` : le sous-état courant
plus un, écrit **une seule fois**, le mode laissé tranquille. Et c'est une correction à la note qui
l'accompagne : la demande de mode « ne commande rien depuis l'intérieur d'un combat », mesuré dans
`FIGHT_DSP` — mais pendant la présentation, elle est lue.

Câblé dans `console_play::take_start`, qui a donc maintenant **quatre** sorts au lieu de trois :
`list` sur l'écran titre, `cut` dans ces douze sous-états, `pause` partout ailleurs dans une partie, `title`
dans l'attraction.

**Mesuré** : `vf3run --console-start 2360`, trace ci-dessus, et la ligne de journal
« Start in the presentation or the replay (mode 7, sub-state 2) -> the state is cut short ».
**Pas encore mesuré** : la rediffusion elle-même (sous-état 16). Le levier est le même et la table est la
même, mais aucune route hors-jeu n'atteint `REPLAY_DSP` pour l'instant — `fv2-vic-banc.txt` s'arrête dans
`JUDGE_DSP`. C'est donc à vérifier en jouant, et c'est écrit ici pour que ça ne passe pas pour acquis.

### Un choix de décor pour le VERSUS : la table des routes ne le commande pas

Frédéric, le 2026-10-24 : « sur le travail de la rom de cet été, il avait été introduit le stage select pour
le mode versus ». Avant d'écrire une page, mesuré — et la mesure dit non à l'évidence.

Les deux leviers connus du décor, et ce que chacun fait :

| levier | ce qu'il commande | mesuré |
|---|---|---|
| le sélecteur `0x0007111C`, patché en `li r3,<scene> ; blr` | rien pour un match qui démarre : il n'est atteint **que par un rematch passant par l'écran de choix**, et c'est ainsi que le `CHANGE STAGE` d'août l'utilise | 2026-10 : patché et tenu, le jeu a choisi le décor 9 quand même |
| la table des routes, `0x000B4100` | le décor du premier combat **d'une route en solo** | l'octet à 3 met le combat dans MARBLE ARENA, l'index de scène lit 3 où il lisait 9 |

**Et il ne commande pas un versus.** Sur `tools/scripts/fv2-vic-banc.txt`, un versus mené jusqu'au combat
(`GAME_DSP`, `FIGHT_DSP`) :

| | index de scène `0x0058FD9F` | octet de route `0x000B4100` |
|---|---|---|
| sans rien écrire | **8** | 09 |
| l'octet tenu à 3 des images 1990 à 2900 | **8** | 3 (l'écriture a bien tenu, relue dans le vidage) |

Deux faits s'additionnent : sans écriture, l'index de scène (8) **diverge déjà** du premier octet de la table
(09), alors qu'en solo les deux concordaient ; et tenu à 3 pendant toute la mise en place du match — qui
commence à `ROUND_MASK_INT`, image 2316 — le décor ne bouge pas. Le versus choisit son décor ailleurs.

**Où, probablement : dans les états que le jeu saute.** Les six sous-états du choix (`docs/fv2-etats.md`) sont
`0 SEL_START  1 SELECT  2 STAGE_SEL_INT  3 STAGE_SEL_DSP  4 DECIDE_INT  5 DECIDE_DSP`. **Le jeu a son propre
choix de décor**, et la trace du versus montre qu'il l'enjambe :

```
  1963  SEL_DSP   0 SEL_START
  2035  SEL_DSP   1 SELECT
  2305  SEL_DSP   4 DECIDE_INT      <- 2 STAGE_SEL_INT et 3 STAGE_SEL_DSP sautes
  2306  SEL_DSP   5 DECIDE_DSP
  2316  GAME_INT  0 ROUND_MASK_INT
```

C'est très probablement ce que le travail d'été « a introduit » : non pas une page à dessiner, mais faire
**entrer le jeu dans `STAGE_SEL`**, qui existe déjà et qui a ses propres images. Ce qu'il reste à trouver est
ce qui décide, à la sortie de `SELECT`, d'aller en 2 plutôt qu'en 4 — un drapeau, un réglage d'opérateur, ou
le nombre de joueurs. C'est une lecture de `fv2_sel_SELECT` dans Ghidra, pas une mesure.

**Un piège d'outillage rencontré ici, et qui a coûté deux courses** : dans `VF3_POKE`, un `b` en tête d'une
adresse signifie « la RAM de sauvegarde » (`tools/vf3run.cpp`, `const bool bk = *q == 'b'`). Donc
`VF3_POKE=b4103=3` n'écrit **pas** à l'index `0x0B4103` de la RAM : il écrit dans la sauvegarde à l'offset
`0x4103`, sans rien dire. Les adresses dont le premier chiffre hexadécimal est `b` s'écrivent avec un zéro
devant : `0b4103`.

#### Lu dans le code : `STAGE_SEL` est une SOUCHE VIDE sur la borne

Ghidra, le 2026-10-24, et ça ferme la question. La table des modes est à `0x00024354` ; son indice 5,
`SEL_DSP`, donne `0x0000BA74`, le repartiteur du choix. Il fait trois choses :

```
0000ba88  lbz r6, 0x0058FD9B      SUBMODE_REQUEST
0000ba94  stb r6, 0x0058FD99      MODE_SUB = la demande      <- il la recopie a chaque tour
0000bac8  addi r2, r2, 0x4404     la table des gestionnaires du choix, 0x00024404
0000bad4  bctr                    il saute dans le sous-gestionnaire
```

(La deuxieme ligne est la preuve directe de ce sur quoi `Model3::fv2_cut_submode` repose : le repartiteur
recopie la demande dans le sous-etat, donc ecrire la demande suffit.)

Et la table `0x00024404` :

| indice | sous-etat | gestionnaire |
|---|---|---|
| 0 | `SEL_START` | `0x0000BADC` |
| 1 | `SELECT` | `0x0000BB10` |
| 2 | `STAGE_SEL_INT` | **`0x0000BBB4`** |
| 3 | `STAGE_SEL_DSP` | **`0x0000BBB4`** — la meme adresse |
| 4 | `DECIDE_INT` | `0x0000BBB8` — quatre octets plus loin |
| 5 | `DECIDE_DSP` | `0x0000BC1C` |

Et ces quatre octets sont :

```
0000bbb4   b 0x0000bad8        la sortie commune du repartiteur
```

**Les deux etats du choix de decor sont une souche vide.** Une seule instruction, partagee, qui ne fait que
revenir. La borne n'a jamais eu de choix de decor : Sega a laisse les deux noms dans sa table de deboguage et
les deux emplacements dans la table des gestionnaires, et rien dedans.

Donc le travail de l'ete n'a pas « atteint » un ecran qui existait : il a fallu en **ecrire** un, ou detourner
autre chose — ce qui explique que son `CHANGE STAGE` finisse sur le selecteur `0x0007111C` par le chemin du
rematch, le seul endroit ou le jeu redemande un decor. Et ca double la premiere mesure : ni la table des
routes (mesuree ici) ni les etats `STAGE_SEL` (lus ici) ne donnent un choix de decor en versus.

Pour memoire, pendant qu'on y etait : `DECIDE_INT` finit par `li r0,5 ; stb r0,0x0058FD9B` — il demande
`DECIDE_DSP` —, et `DECIDE_DSP` lit `0x00581892` et, s'il est nul, ecrit `6` dans `MODE_REQUEST`
(`0x0058FD9A`), ce qui est le passage a GAME.
