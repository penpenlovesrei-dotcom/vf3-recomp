# The sound's bench — the baseline, and every batch measured against it

> This document is one of the nine on the sound. The front door, which says where everything is:
> [docs/sound.md](sound.md).

Measured on 2026-10-24. Frédéric's machine (Windows 11, g++ 16.1.0 MSYS2, `-O2 -g`), branch `fv2`, baseline
at commit `335c050`. **One bench at a time**: two runs started together fight over the cores and the
figures stop meaning anything.


## How these figures are redone

```bash
# build
./construire.cmd                       # or: make CXX=g++ build/bin/vf3run -j8

# the time of each part of a frame
VF3_PROF=1 build/bin/vf3run.exe "$ROMS/fvipers2.zip" \
    --script tools/scripts/fv2-fight.txt --frames 4200 --wav sound.wav --quiet

# and, within it, where the sound board's time goes
VF3_PROF=1 VF3_SND_PROF=1 build/bin/vf3run.exe ... --wav sound.wav --quiet

# the same, core by core: three alternating passes per core, windows 1500-4200 (audit S4 sexies)
VF3_PROF=1 VF3_SND_PROF=1 VF3_SCSP_CORE=mame|own build/bin/vf3run.exe ... --rec --wav sound.wav --quiet

# and: the sound has not moved (against a reference commit, byte by byte)
tools\sound\compare-sound.cmd fvipers2 tools\scripts\fv2-fight.txt 4200
```

`$ROMS` = `C:\Users\frede\Documents\SEGA Model 3\ROMs`. `VF3_PROF=1` prints one line every 500 frames;
`VF3_SND_PROF=1` adds the breakdown inside the sound board (`src/sound/sound_prof.h`) and the DSP program's
composition (`dspprog`, one line per program write). **`--wav` is required in all three commands**: without
it `vf3run` does not run the sound board, and the two probes have nothing to count — the tool now says so
instead of returning a line of zeros (`docs/scsp_audit.md` §4 sexies).

**The scenes.** Only one has served so far, and that has to be said:

| scene | file | what it goes through |
|---|---|---|
| **Fighting Vipers 2, one round** | `tools/scripts/fv2-fight.txt` | start-up (0-960), attract and title screen, coin-up, selection, loading, intro, one round against the computer with random play (seed 20261019) |
| Virtua Fighter 3, one round | `tools/scripts/vf3-match.txt` | the same shape, on the other game — used for non-regression, **not for time** |

What is missing and is asked for by the specification: **silence, music only, effects only, sound test**.
The port has no lever to get there without a human hand (the test menu is crossed with the pad), and
`VF3_SCSP_SOLO` separates only the two chips, not the music from the effects. So: all the figures below are
those of **one** scene, which contains music from beginning to end and effects from frame ~1900 on. Nothing
in it measures the "nothing is playing" case.

**The instrumentation's limiter.** `sound_prof.h`'s scopes cost two `rdtsc` each. Checked over the whole
scene: probes off against probes on, the sound board's time moves by less than 1%, and the WAV comes out
**byte-identical**. The report itself prints what its `rdtsc` weigh (~58 kcycles per frame). The timing
figures below are all taken **with the probes off**.


## The baseline, `335c050`

A frame's budget at 57.52 Hz: **17.4 ms**.

| frames | Model 3 board | sound board | share of the frame |
|---|---|---|---|
| 0-500 | 13.09 ms | 1.60 ms | 9.2% |
| 500-1000 | 12.89 | 1.75 | 10.1% |
| 1000-1500 | 13.49 | 1.83 | 10.5% |
| 1500-2000 | 14.02 | 1.83 | 10.5% |
| 2000-2500 | 13.45 | 1.85 | 10.6% |
| 2500-3000 | 14.57 | 1.89 | 10.9% |
| 3000-3500 | 14.57 | 2.03 | 11.7% |
| 3500-4000 | 14.67 | 2.02 | 11.6% |
| **mean** | **13.8** | **1.85** | **10.6%** |

The breakdown inside the sound board, and the cost of one DSP step (TSC cycles — not core cycles: the TSC
runs at the nominal frequency, the core goes higher, so these numbers serve to **compare with each other**,
not to convert into nanoseconds):

| | loop+timers | 68000 | voices | DSP | mix | cycles / DSP step |
|---|---|---|---|---|---|---|
| baseline | 1.1-1.4% | 22.6-24.2% | 5.7-13.3% | **62.0-69.0%** | 0.5-1.1% | **16.2-16.4** |


## Each batch, measured alone, on the same scene and the same machine

Every batch touches **only** `src/sound/scspdsp.{h,cpp}`. After each one, the 4200-frame WAV
(12 879 504 bytes) was compared with the baseline's: **byte-identical**, every time. That is not a bonus,
it is the condition for keeping the batch.

| frames | base | batch 1 | batch 2 | batch 3 |
|---|---|---|---|---|
| 0-500 | 1.60 | 1.41 | 1.38 | 1.40 |
| 500-1000 | 1.75 | 1.53 | 1.50 | 1.27 |
| 1000-1500 | 1.83 | 1.62 | 1.56 | 1.39 |
| 1500-2000 | 1.83 | 1.66 | 1.59 | 1.42 |
| 2000-2500 | 1.85 | 1.65 | 1.60 | 1.37 |
| 2500-3000 | 1.89 | 1.68 | 1.65 | 1.38 |
| 3000-3500 | 2.03 | 1.81 | 1.76 | 1.51 |
| 3500-4000 | 2.02 | 1.75 | 1.69 | 1.46 |
| **mean (ms)** | **1.85** | **1.64** | **1.59** | **1.40** |
| against the base | — | **−11.4%** | **−14.1%** | **−24.3%** |
| the batch's own gain | — | −11.4% | −3.1% | −12.0% |
| cycles / DSP step | 16.3 | 13.2 | 13.1 | **10.0** |
| the DSP's share | 62-69% | 57-66% | 57-66% | **52-60%** |

### Batch 1 — the DSP's body specialised on what the program uses

`Step` becomes `Run<U>`, a template on the set of controls the program really touches. Measured: Fighting
Vipers 2's program (112 steps, and 127 for the fight's) **never** sets `FRCL`, `ADRL`, `YRL` or `NEGB`.
Without them, four branches per step disappear and `FRC_REG`, `Y_REG`, `ADRS_REG` collapse to zero, which
takes others with them. The `TEMP` word that `B` and `X` both read is read once.

### Batch 2 — the local copies, against sound RAM's aliasing

A step that writes into the delay line writes through `ram`, and the compiler has no way of knowing that
sound RAM is not **inside the DSP object itself**: without copies, `DEC`, `RBL`, `RBP` and `LastStep` have
to be reread after every write, and so does every word of `TEMP`. The batch takes a local copy of all four
and goes through a `__restrict` pointer — a true promise: sound RAM is the 68000's, a separate allocation.

### Batch 3 — the one-bit controls grouped into a word

A step was fetching **eight separate bytes to test eight bits**. `Inst::f` puts them into a `u16`, with
`SHIFT` and `YSEL` thrown in; `Inst::mf` does the same for the four read only inside the memory block (a
third of the steps). The choice of `Y` tests the `COEF` case first, which is the one of 87 steps out of
112. The originally named fields stay in `Inst`: `Decode` and the report read them, and they say what they
are.


## The batch that gave nothing, and was undone

**Batch 4 — `UpdateSlot`'s two-address loop, unrolled.** MAME walks `(addr1, cur_addr)` then `(addr2,
nxt_addr)` through it via two arrays of pointers to locals, which keeps `addr1` and `addr2` in memory for
the whole function. Written out flat, with `LPCTL`, `LSA` and `LEA` read once, the code is clearer and the
WAV stays byte-identical.

**And it gains nothing:** a mean of **1.399 ms** against **1.400 ms** without it — that is, zero, in the
noise.

| frames | batch 3 | batch 3 + batch 4 |
|---|---|---|
| 0-500 | 1.40 | 1.32 |
| 500-1000 | 1.27 | 1.26 |
| 1000-1500 | 1.39 | 1.35 |
| 1500-2000 | 1.42 | 1.40 |
| 2000-2500 | 1.37 | 1.37 |
| 2500-3000 | 1.38 | 1.45 |
| 3000-3500 | 1.51 | 1.52 |
| 3500-4000 | 1.46 | 1.52 |
| **mean** | **1.400** | **1.399** |

**Undone.** A change in a fidelity file that buys nothing measurable is not kept: it moves the file away
from MAME for no reason, and somebody will read it one day thinking it served a purpose. The negative
result, on the other hand, is kept — it says the voices are not won there, and that the next person who
wants to speed them up must look elsewhere (the 256 KB pan tables, the envelope, the LFO).


## Virtua Fighter 3: checked, not quantified

The harness was run on `vf3.zip` (`tools\sound\compare-sound.cmd vf3 tools\scripts\vf3-match.txt 1200`,
then 2400 frames): **byte-identical sound** in both cases. Its DSP program also has 0 steps that set
`FRCL`, `ADRL`, `YRL` or `NEGB`, so it takes the lean body too.

The harness prints both builds' time along the way, and over VF3's first 1000 frames it gives 1.34 then
2.03 ms for the reference against 1.08 then 1.15 ms for this branch. **That is not an accepted
measurement**: two windows of 500 frames over the start-up, where the Model 3 board's time itself also
varies from 6.77 to 5.20 ms between the two runs — the machine was moving. The order of magnitude goes the
same way as the one measured on Fighting Vipers 2, and that is all that can be said of it. **VF3's time
remains to be measured properly.**


## What these figures do not say

1. ~~**Nothing about a frame's time in the game.**~~ **Measured on 2026-10-26** (`docs/scsp_audit.md` §4
   ter), and that line had it right: in the host the sound thread works 1.49 ms per frame and **the game
   never waits for it** — 0.000 ms, maximum 0.016. So the 0.45 ms saved really are processor time less —
   heat, battery, cores left to the rendering — and **not** a frame arriving earlier. In `vf3run`,
   everything is on one thread, and there the gain reads directly.
2. ~~**Nothing about the recompiled version.**~~ **Measured on 2026-10-25** (`docs/scsp_audit.md` §4 bis),
   and that line's prediction was right: with `--rec` the board goes from 13.77 to **3.23 ms** and the
   sound stays at **1.42**, so its share goes from 9.6% to **30.5%**. This document's percentages really do
   not transpose there. The sound's internal split, though, is identical in both modes, and the two WAVs
   are byte-identical.
3. **Nothing about another machine.** One workstation, one compiler, one optimisation level.
4. **Nothing about the scenes that are missing** (silence, music only, effects only, sound test) — see
   above.
5. **Nothing about fidelity to the hardware.** The byte-identical WAV says that *this port has not
   changed*, which is exactly what is asked of an optimisation. It says nothing about what a real YMF292-F
   does.
