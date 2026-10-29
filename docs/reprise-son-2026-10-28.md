# Resumption — the sound, the day of 2026-10-28

**Read `scsp/docs/sound.md` first**: it says where everything is. This file only says what moved that day,
what is checked, and what remains — with, every time, the instrument that established it.

Twenty commits, from `7876901` to `d62f732`, pushed to `mine/fv2`. **The six points of the morning's
resumption list are closed.**

---

## 1. `VF3_SND_PROF` produced nothing — four reasons, all of them mute

Each one made the measurement **empty or wrong without saying so**. All closed, and the first three now
**speak**. The detail is in `scsp_audit.md` §4 sexies.

1. the call to `sndprof::report` lived inside `tools/vf3run.cpp`'s `if (prof)` block: `VF3_SND_PROF=1`
   without `VF3_PROF=1` printed nothing. It has its own test now, and writes to **stdout** like the `prof`
   line.
2. **`vf3run` only runs the sound board with `--wav`.** Without it the probe counted zero and displayed it
   as a measurement. The tool says so at start-up, and the report says "nothing measured".
3. the report only arrived at multiples of 500 frames: a final report covers the rest.
4. the `SCSP` and `DSP` scopes lived only in MAME's code; on `VF3_SCSP_CORE=own` the report gave **97% to
   the 68000, zero to the voices, zero to the DSP** — a wrong figure, not a hole.

## 2. What the project's core costs, in the game

Three passes per core, alternating, windows 1500-4200, 18 measurements per core, median. The six WAVs
byte-identical.

| | `mame` | `own` | gap |
|---|---|---|---|
| 68000 (the control: the same code) | 1 015 kc | 947 kc | **−68 kc** — the measurement's play |
| voices | 443 | 484 | +41 kc, **under the play: does not read** |
| **DSP** | **1 737** | **2 039** | **+302 kc (+17.4%)** |
| the whole sound board | 3 284 | 3 550 | **+266 kc (+8.1%)** |

In the frame under `--rec`: **+0.09 ms, that is 0.5% of the 17.4 ms budget**.

## 3. The replayer's trace was only complete over 300 frames — closed

The check "is the trace complete?" failed over the whole scene (samples 772 067 and 1 149 006). **The
cause: the MADRS mirror**, the only one of the four extras outside `scsp::core` that changes the sound, set
by `dynamic_cast` **after** the recorder was created — hence invisible to the trace, which a replay left at
its default `true` when Fighting Vipers 2 wants it false.

One door now (`scsp::set_madrs_mirror_on` in `scsp_select.h`), set **before** the recorder, which **reads
it back from the chip**, a trace header **version 3** that carries it, and a v1 or v2 trace **refused**.
The whole measurement is in `scsp_validation.md` §3 c.

## 4. Seven tests no longer drag MAME's core along

`scsp_bench.h` was including both concrete classes for the sake of two three-line switches. **Each core now
registers itself in its own `.cpp`** (`scsp::registered`): the selector no longer names any concrete class,
and **what is linked decides what exists**. `make test-without-mame` builds the seven block tests with
`scsp_select.o`, `scsp_own.o`, `dsp_own.o` and nothing else, and `make test` redoes it every time.

## 5. The DSP by characteristic: it is no instruction, it is the baseline

`measure-core --dsp`, programs of 112 steps where every step does only one thing, both cores side by side,
**net** cost with the baseline subtracted (`scsp_audit.md` §4 septies). The baseline — 112 steps that do
nothing — costs the project's core **+19%**, and the control of the game's shape has a net cost of **almost
nothing**. The project's core is **better** on half the table; it is only worse on memory access.

## 6. The assembly, read after the bench had narrowed the question

`Run<0>`: 108 instructions, **16 branches**. `loop<false>`: 145 and **31**. **Five hypotheses dead on
inspection**: both bodies exist, the decoded step is smaller (16 bytes against 28), no jump table, no
division, the 64-bit product is identical in MAME, and both compile `std::clamp` to `cmov`.

## 7. Two batches to remove those branches — and both lose

Undone (`scsp_audit.md` §4 octies). Five more bytes in `DecodedStep`: **+7%**. The same decisions in the
word's free bits: **+16%**. And a false gain of 6.9% was nearly kept — `mame` was drifting in the
denominator. Hence `measure-core --baseline`, fast enough to interleave reference and variant.

## 8. The DSP's four fields: looked for everywhere, then tested

**No game served sets them**: 152 program writes, five distinct shapes, three games including `srally2`
with an altogether different program (`scsp_audit.md` §5). **`test_scsp_dsp.cpp` cases 9 to 13** measure
them, each by the only read path that exists. The project's core returns them all **first time**, and four
injected faults are caught **each by its own case**.

## 9. The hardware counter was there, and I had said twice that it was not

`xperf`, from the Windows Performance Toolkit, is installed on this machine. I had declared it absent
without looking for it, and §4 septies as well as §4 octies had stopped on that sentence — both now keep
it, dated, with the pointer to the measurement that corrects them.

| per DSP step | `own` / `mame` ratio |
|---|---|
| instructions retired | **×1.144** |
| mispredicted branches | **×1.010 — identical** |

**It is not the branches**: they are perfectly predicted, they cost their decoding and not their misses —
which explains why point 7's two batches lost. **It is the instructions.**

Only the **ratios** are publishable: the interval `xperf -pmcsources` announces does not hold
arithmetically (§4 nonies details it).

## 10. The hunt for the eight instructions: first lead, and it gives nothing

MAME carries `u16 *const __restrict rp = ram;` in its loop header; the project did not have it, and it is a
true promise with a complete explanation. **It gains nothing**: −0.08% on the baseline, flat on the game.
Defeated. And the `own`/`mame` ratio, which protected against the two batches, **becomes a trap** as soon as
only one side changes: it is the absolute of the changed side that must be read.

## 11. Located as far as the function, and no lower

Attribution by function with no PDB: the dumper gives the `PrgrmCtr`, the trace the load base, `nm` the
ranges. **The step loop takes +19.6%**; the global +14.4% was diluted because the project's core **gains
15.7%** on the per-sample envelope.

**Three attempts to go lower, three reasons to stop**: the inlining blurs the line tables; the sampling
skid without PEBS makes the per-instruction histograms incomparable; and the `PrgrmCtr` is not an
instruction address — 21 of MAME's 84 hot addresses land on one.

## 12. The variants judged on the counter, and the ranking reverses

Six variants built by `build-variants.py`, each one **put through the ten tests** before being kept, and
judged by `measure-variants.cmd` over **both** programs:

| | baseline | **the game's shape** |
|---|---|---|
| `ysel_coef_premier` | −1.55% | **−3.09%** |
| `shift_deux_bits` | −3.03% | **−1.17%** |
| `sortie_groupee` | −6.02% | +0.85% |
| `tout` | — | +3.28% |
| **`entree_paresseuse`** | **−15.00%** | **+4.68%** |

**The baseline's big winner is the game's worst**: it adds one test per step to avoid a computation that 37
steps out of 112 need. The baseline **locates** a cost, it does not say what a change is worth. Hence
`measure-core --game`. The instrument is reproducible **to within 0.07 points** from one session to the
next.

**And cases 9 to 13 stopped `sortie_groupee` on the first round**: its grouped mask swept up the writes of
`FRCL` and of `ADRL`. It passed all the other tests, the replayer, and produced an identical WAV — because
no game sets those fields. That is what those cases really buy, and it is not what I had announced: **they
protect those four fields against our own optimisations.**

## 13. The optimisation kept: −3.47%

`1a14781`. COEF tested first in the choice of Y, and the `SHIFT` field resolved into two bits of the flags
word — **without growing the decoded step**, which is what sank batch B. Neither of the two changes what a
step **computes**, only the way it **chooses**. The full bar passed.

## 14. The core validated by ear, in the real game

`fv2-stable-1` + HEAD's eighteen `src/sound` files + four Makefile lines: **it compiles and it plays**, on a
base ten days old. No difference to the ear, as the replayer had promised.

And the WINNER screen's sound bug fell there **with one line** — `sound.madrs_mirror = board.set == "vf3"`,
which `stable-1` did not have. It is not the chip that fixes it: the mirror is a per-game setting, and the
core does not know the game. But it makes it fixable in one named and measured line.

## 15. The SCSP is a standalone repository, and it is git that carries it there

**[github.com/penpenlovesrei-dotcom/scsp-model3](https://github.com/penpenlovesrei-dotcom/scsp-model3)**,
a real `git subtree` under the prefix `scsp/`. A fresh clone of the published repository passes its **seven
tests**, without a line of MAME. The paths are the same on both sides, so a `diff` reads without
translation, and the circulation takes one line in each direction:

    git subtree push --prefix=scsp scsp main
    git subtree pull --prefix=scsp scsp main

`tools/sound/export-scsp.py` has gone: it copied the files into a fresh folder, hence with no past.
`scsp/LICENSE` was born with the repository — the MIT without its list of third parties, since that list no
longer applies: nothing there comes from MAME, and the licence boundary runs exactly at the prefix's edge.

**The history does not cross over, and that has to be said.** `git subtree split` follows a path, and
before the move the path `scsp/` did not exist: the standalone repository begins on the day of the prefix.
The forty commits that made the core stay here, where `git log --follow scsp/src/sound/scsp_own.cpp` walks
back through all of them. I rebuilt them to see (a filter on the 30 old paths, 40 commits, the same blobs
at the top); grafting them cost **one merge per synchronisation**, and the choice was to keep `subtree
push` down to one line. The repository's README carries that sentence, for whoever arrives that way.

What that repository cannot do is in its README: redo its own numbers — **the proof lives in the game**,
and that was `sound.md` §7's argument.

## 16. On two cores: the last figure

AMD A9-9425, two cores, two logical processors. Five passes of 4 000 frames, medians over 70 to 76 readings
(`scsp_audit.md` §4 quater bis):

| | a frame | the sound thread | the game waits |
|---|---|---|---|
| `off` | **12.21 ms** | 0 | 0.001 |
| `mame-normal` | **12.34** | 6.10 | 0.003 |
| `own-normal` | **12.38** | 6.46 | 0.003 |
| `mame-sync` | **16.03** | 4.67 | 0.000 |
| `own-sync` | **15.95** | 4.64 | 0.000 |

- **The sound on its thread costs +0.12 ms per frame** — about 1%. The word "free" can at last be weighed.
- **The project's core's 8% does not show**, not even here: +0.04 ms at the frame, **unresolved** inside a
  spread that runs from 1.86 to 52.65 ms. On the thread, the extra cost is clearly there: +5.8%, which
  lines up with the +8.1%.
- **The overlap buys 3.7 ms per frame** — thirty per cent of a frame's duration. That is the sound thread's
  measured justification, and it did not exist before.

---

## What is checked

| | |
|---|---|
| the tests | **24 files green**: the ten on both cores, plus the seven **without a line of MAME** |
| the replayer, whole scene | **EVERYTHING AGREES**, the four checks, 3 219 865 samples per chip |
| the end-to-end footprint | **`9A2310F8AF000E09EB18BD114852B003`** with each core |
| VF3's non-regression | `verifier-vf3.cmd 3000` → **IDENTIQUE** on the four checks |
| by ear | the real game, windowed, no difference |

**The migration plan's eight acceptance criteria are satisfied.**

## What is not checked, and only one thing is left

**That MAME is right about the DSP's four fields.** Nobody has measured them on hardware, and
`scsp_references.md` refuses consensus between emulators as proof. Cases 9 to 13 establish that the
project's DSP reaches the **intended** behaviour, and nothing more.

## What remains

**Nothing on the sound.** The last open point was the real `git subtree`, done on 2026-10-29: the 30 files
moved under `scsp/` by `git mv`, the Makefile and the includes followed, the documents' 33 references
rewritten, and the three checks run again after the move. The standalone repository is online and a fresh
clone builds in it.

### The language, settled on 2026-10-29 and most of the way done

The rule for this repository is **English**. Everything written during the sound work went the other way,
and the measurement said how far: **9 563 lines of French prose across 186 files** — not the 36 842 a count
by file suggested, the difference being that `tools/scripts/*.txt` is controller data with 288 comment
lines out of 3 950.

What is done, and verified at every step by `make test` (24 files green), the replayer (byte-identical over
3 219 865 samples per chip) and the footprint `9A2310F8AF000E09EB18BD114852B003`:

| | |
|---|---|
| the published repository | **entirely English** — every source, every bench, the seven documents |
| the identifiers | **3 390 renames** in the code and **501 distinctive names**: `scsp_own`, `dsp_own`, `tools/sound`, the core named `own`, `VF3_SCSP_CORE` |
| the interface | `--baseline`, `--game`, `--against`, `make measure`, and the trace header at **version 3** with English keys |

**A guard watches every translated file**: its numbers, its backticked spans and its underscored
identifiers compared with HEAD, the decimal comma and the thousands separator normalised first, since both
change with the language and the number must not. Its limit is named: bare capitalised words are not
checked, because these files also capitalise for emphasis.

**Three homonyms cost something, and the third got through.** `rate` is the French verb *rater* and the
English word for the envelope rate; `armes` is a mask of armed timers and a list of arms; and `verts` is
the plural of *vert* and the video engine's **vertices** — `m.verts` became `m.green` in r3d_scene, r3d_gpu,
r3d_raster and vf3run, and it compiled. Those four files were reverted whole.

What is left, and it is prose only: **5 720 lines** — `docs/` (3 671, of which `fv2-son-programme.md` alone
is a 3 062-line file), the root (979, mostly `REPRISE.md`) and `tools/` (1 070, script comments).

The six points of the morning's list are closed, and what I was able to name along the way — the isolated
7.4 ms wait spike, `own`'s cycles, the assembly below the function — was **measured and ruled out**, not
left open.

## The tools new or changed that day

| | |
|---|---|
| `measure-core --dsp` | the DSP by characteristic, both cores, baseline subtracted |
| `measure-core --baseline [core]` | the baseline alone; with a core name, one per process |
| `measure-core --game <core>` | **the program of the game's shape** — the baseline is artificial |
| `make test-without-mame` | the seven block tests linked without a line of MAME |
| `build-variants.py`, `dsp-variants.py` | one variant per exe, each put through the ten tests |
| `measure-variants.cmd [baseline\|game]` | the judging on the hardware counter — **administrator** window |
| `git subtree push --prefix=scsp` | the standalone SCSP repository, copying nothing |
| `mesurer-concurrence.cmd` | five passes, **both cores** — for the laptop (it lives in the measurement folder, not in this repository) |
| `VF3_SND_PROF=1` | without `VF3_PROF`, requires `--wav`, and holds for both cores |
| trace `# scsp-trace 3` | the header carries `mirror=0\|1`; a v1 or v2 trace is refused |
