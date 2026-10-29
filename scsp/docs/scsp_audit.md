# Audit of the sound subsystem — what there is, what it costs

> This document is one of the nine on the sound. The front door, which says where everything is:
> [docs/sound.md](sound.md).

Written on 2026-10-24, on branch `fv2` at commit `335c050`, Frédéric's machine (Windows 11, g++ 16.1.0
MSYS2, `-O2`). Every measurement in this document has been run; the commands are given as they are so that
they can be redone.

**The reading rule:** a measurement says what it says and nothing more. The figures below all come from
`vf3run` (the windowless tool, PowerPC interpreter) on `fvipers2.zip`, scene `tools/scripts/fv2-fight.txt`
— start-up, title screen, selection, one round. They say nothing about another machine or another scene.


## 1. The parts, and who makes them run

| part | file | what it is |
|---|---|---|
| the sound board | `src/sound/soundboard.{h,cpp}` | 68000 at 11.2896 MHz, two SCSPs with 1 MB of RAM each, the bank register, the program ROM, the samples |
| the SCSP | `src/sound/scsp.{h,cpp}` | Yamaha YMF292-F: 32 voices, envelopes, LFO, pitch, timers, interrupts, DMA, MIDI link |
| the effects DSP | `src/sound/scspdsp.{h,cpp}` | 128 steps of microcode, delay memory in sound RAM, 16 EFREG outputs |
| the 68000 | `third_party/mame/m68000/` | MAME's 68000 (Olivier Galibert), generated instruction handlers, bus-cycle accurate |
| the MAME frame | `src/core/mame_shim.*`, `src/core/shim_include/emu.h` | what MAME's cores expect of the framework, replaced by the bare minimum |

**Who calls whom.** The game (PowerPC) sends nothing but MIDI bytes; `model3.cpp` puts them into
`board.midi`. The host and `vf3run` pass them to `SoundBoard::midi_in`, then ask for
`SoundBoard::run(1/57.52 s, out)`. There:

```
run(seconds)
  for each slice of at most one sample (256 cycles of the 68000):
      cpu->run(cycles)                          <- the 68000
          Bus::read / Bus::write                    (virtual call)
              scsp_device::read / write
                  update() -> sound_stream_update -> DoMasterSamples   <- the samples
                                                       UpdateSlot x32
                                                       SCSPDSP::Step   <- 112 steps
      fire_due()                                <- the timers A/B/C, MIDI transmit/receive
  scsp[0]->update(); scsp[1]->update()           <- the rest of the frame
  the stereo mix, the limiter, out.push_back
```

The important point, and the one that made the measurement hard: **the samples are not made in a phase of
their own**. The 68000 triggers them from inside its own slices, every time it touches a chip register —
it is MAME's stream brought up to the present instant (`scsp_device::update`). The rest of the frame is
made by the two `update()`s that close it.

**Where it runs.** In the host (`src/host/main.cpp`), the sound board is on **a thread of its own**, one
frame behind: so its cost is not added to a frame's time, it occupies a core beside the rendering threads.
In `vf3run` everything is on one thread, and the cost reads directly there. **No gain announced here is a
gain in frame time**: it is processor time less, which counts for heat, battery and cores left to the
rendering, not for a frame that would arrive earlier.


## 2. Provenance and licences

| path | licence | authors credited | state |
|---|---|---|---|
| `src/sound/scsp.{h,cpp}` | BSD-3-Clause | ElSemi, R. Belmont; thanks to kingshriek | MAME `sound/scsp.cpp`, **modified** (see below) |
| `src/sound/scspdsp.{h,cpp}` | BSD-3-Clause | ElSemi, R. Belmont | MAME `sound/scspdsp.cpp`, **modified** (program decoded once, specialised body) |
| `src/sound/soundboard.{h,cpp}` | BSD-3-Clause | R. Belmont, Ville Linde | follows `sega/model3.cpp` (`model3_snd`), the rest is the project's |
| `src/sound/sound_prof.h` | — (project) | — | new, written for this audit |
| `third_party/mame/m68000/` | BSD-3-Clause | Olivier Galibert | MAME, handlers and decoding table **unchanged**; the device reduced to a bus |
| `src/core/mame_shim.*` | — (project) | — | the replacement for MAME's framework |

The repository is under MIT (`LICENSE`), and BSD-3-Clause is compatible with that redistribution provided
the notice, the conditions and the warranty disclaimer are kept — which every file does at its head.
`third_party/README.md` already holds that inventory; nothing was missing from it.

**What the project changed in MAME's SCSP**, and that must survive any rewrite (each one was measured
here, on this cabinet):

1. **The mirroring of MADRS at 0x7C0-0x7FF**, per game: true for Virtua Fighter 3, **false** for Fighting
   Vipers 2, where it makes all the DSP's delay lines fold onto one address and where the victory screen's
   sound feeds itself (+24 dB up to the limiter). MAME has since corrected that on its side
   (mamedev/mame#16527). See `src/sound/scsp.h` and `docs/fv2-console-son.md`.
2. **The FM modulation depth** as Supermodel's (MDL's low bit ignored, range 24 and not 26).
3. **The timers A-C counted in samples**, due on a round sample, like Supermodel's.
4. **The per-group volumes** (music / effects) and the effects leveller: that is not the chip, it is a host
   mixer, and it is disabled as long as both volumes are 1.
5. **The board's peak limiter**, above gain 1.
6. **SDIR** setting TL to zero in both downstream mixes.

The probes and the logs (`VF3_SCSP_WLOG`, `VF3_KEYON_LOG`, `VF3_SCSP_SOLO`, `VF3_DUMP_SOUND_RAM`,
`m_probe_*`) are the project's too, and serve the fidelity investigations.


## 3. How it is measured

### The sound board's time, frame by frame

Already present: `VF3_PROF=1` makes `vf3run` print, every 500 frames, the mean time of a frame spent in
the Model 3 board, in the sound board and in building the scene.

```bash
VF3_PROF=1 build/bin/vf3run.exe "<ROMs>/fvipers2.zip" \
    --script tools/scripts/fv2-fight.txt --frames 4200 --wav sound.wav --quiet
```

### Where that time goes, inside the sound board

New: `src/sound/sound_prof.h`, turned on by `VF3_SND_PROF=1`. Nested scopes count the TSC cycles of the
loop iteration, of the 68000, of the sample making, of the DSP and of the mix, and the report **takes the
nested time back out** to give each one its own share (the formula is in the header). Off, it costs nothing
but a boolean test.

```bash
VF3_PROF=1 VF3_SND_PROF=1 build/bin/vf3run.exe ... --wav sound.wav --quiet
```

**`--wav` is not decorative in that command: without it `vf3run` does not run the sound board at all**, and
the probe has nothing to count. It says so now, instead of producing a line of zeros; the three other ways
this wiring stayed silent are in §4 sexies. The two scopes `SCSP` and `DSP` have lived on both sides since
2026-10-28, so the report holds for `VF3_SCSP_CORE=own` as well.

**The instrumentation has been checked, as any self-respecting measurement requires**: over the same
4200-frame scene, probes off then on, the sound board's time is the same to within ~0.5%
(1.60/1.75/1.83/1.83/1.85/1.89/2.03/2.02 against 1.61/1.77/1.81/1.89/1.85/1.92/1.99/1.98 ms), and the WAV
produced is **byte-identical**. The report itself prints what its `rdtsc` cost (~58 kcycles per frame out
of ~4000, that is 1.5%).

### The sound has not moved

`tools/sound/compare-sound.cmd <game> <script> <frames> [commit]` makes the branch's `vf3run` and a
reference commit's play the same scene (a separate worktree, `C:\Temp\fv2-son-ref`), and compares the two
WAVs **byte by byte**. For an optimisation, exact identity is the only acceptable answer; there is no
tolerance to widen. The script has been run on both games; its verdict is in §7.


## 4. The baseline, and where the time goes

Scene `fv2-fight.txt`, 4200 frames, `fvipers2.zip`. A frame's budget at 57.52 Hz is **17.4 ms**.

| frames | Model 3 board | **sound board** | share of the frame |
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

What that establishes: **the sound board is not the bottleneck** — the Model 3 board (PowerPC interpreter
and video) costs seven times more. And what it does not establish: nothing about the recompiled version
(`--rec`), where the sound's share rises as much as the board's falls, nor about the host, where the sound
is on its own thread.

And within it, each part's own share (`VF3_SND_PROF=1`, the same 4200 frames):

| frames | loop+timers | 68000 | voices | **DSP** | mix | DSP steps / frame |
|---|---|---|---|---|---|---|
| 0-500 | 1.4% | 24.2% | 5.9% | **67.9%** | 0.6% | 153 416 |
| 500-1000 | 1.3 | 23.5 | 5.7 | **69.0** | 0.5 | 171 395 |
| 1000-1500 | 1.3 | 23.3 | 7.3 | **67.0** | 1.1 | 169 569 |
| 1500-2000 | 1.3 | 23.8 | 9.5 | **64.4** | 1.1 | |
| 2000-2500 | 1.2 | 22.9 | 7.5 | **67.5** | 0.9 | |
| 2500-3000 | 1.2 | 22.9 | 10.1 | **64.6** | 1.1 | |
| 3000-3500 | 1.1 | 22.6 | 13.3 | **62.0** | 1.0 | |
| 3500-4000 | 1.1 | 22.6 | 12.3 | **63.0** | 1.0 | |

**Two thirds of the sound board is the effects DSP.** The voices, which one would suspect first, are 6 to
13% — and their share rises with the fight, which is expected: more active voices. The 68000 is a steady
quarter. The loop and the mix are not worth looking at.

~170 000 DSP steps per frame: 766 samples × 2 chips × **112 steps**. So both SCSPs run a 112-step program
each, 44 100 times a second.

## 4 bis. The sound's share under `--rec`: measured, and it triples

The section above said "nothing about the recompiled version (`--rec`), where the sound's share rises as
much as the board's falls". **Measured on 2026-10-25**, the same `fv2-fight.txt` scene, the same 4 200
frames, `VF3_PROF=1` and `VF3_SND_PROF=1`, and the windows 1500-4000 on both sides so as to compare the
same frames — `--rec` only being warm after the recompiler's second pass, around frame 1000.

| | Model 3 board | sound board | total | **the sound's share** | of the 17.4 ms budget |
|---|---|---|---|---|---|
| interpreter | 13.77 ms | 1.46 ms | 15.23 ms | **9.6%** | 87.5% |
| **`--rec`** | **3.23 ms** | **1.42 ms** | **4.64 ms** | **30.5%** | **26.7%** |

**The board is 4.3 times faster; the sound does not move (×0.97, in the noise); its share triples —
×3.18.**

Three controls, and that is what makes these figures usable:

1. **The sound was not supposed to move, and it does not.** The recompiler only touches the PowerPC; the
   sound board remains an interpreted 68000. 1.46 against 1.42 ms: the measurement confirms it instead of
   assuming it.
2. **The internal split is identical in both modes** — DSP 52%, 68000 29%, voices 16% — as it must be for
   the same code.
3. **The two WAVs are byte for byte identical** (`md5` `9a2310f8…`, 12 879 504 bytes each). So `--rec`
   does not change one frame of the sound, and the comparison really is about the same work.

And the control that counts for the optimisation work: the sound is at **1.46 ms** where this document's
baseline measured **1.85 ms** — that is **−21%**, reproduced two weeks apart on the same scene. The −24.3%
gain announced elsewhere holds. Likewise, the DSP's own share has gone from 62-68% to ~52%, which is
exactly what one expects of an optimisation that bore on it.

**What that changes for what follows.** Under `--rec`, the sound is no longer a 10% detail: it is **the
second item and nearly a third of a frame's work**. It is still far from a bottleneck in absolute terms —
4.64 ms on a budget of 17.4 — but it is now where the easiest margin lies, and the DSP is half of it.

**What it does not establish**: nothing about another scene. As for the host, that was this section's
reservation and it is lifted: see §4 ter just below.


## 4 ter. And in the host, the sound costs the frame nothing

The 30.5% above is the **windowless launcher's**, where the sound board runs **inside** the frame loop. In
the host it is on **a thread of its own, one frame behind**: its cost splits in two, and only one half is
the frame's.

| | |
|---|---|
| **the CPU cost** | the thread's work, which has to be done somewhere |
| **the frame cost** | the time the game's thread spends **blocked** waiting for the previous frame's sound, in `finish_frame`. If that is zero, the sound costs the picture **nothing**, whatever CPU it burns |

Measured on 2026-10-26 with `VF3_SND_HOST=500`, with no window (`VF3_HIDDEN=1`), 120 seconds of real play,
7 500 frames, windows 2000-7499 once the pacing had settled:

| | |
|---|---|
| the sound thread works | **1.49 ms per frame** (1.35 to 1.57) |
| the game waits for it | **0.000 ms**, and the maximum ever seen is **0.016 ms** |
| the frame period | **17.38 ms**, that is exactly 1/57.524 |
| hence | **8.5% of one core**, and **0.00% of the frame** |

**The 17.38 ms period is what makes the measurement valid**: the host does pace to the Model 3's rhythm
even with no window, and that is not an assumption — it is the figure the probe prints. The first few
hundred frames give 16.0 to 19.4 ms, while the pacing settles; that is why the mean covers 2000-7499 and
not everything.

**The margin is a factor of 11.7**: the thread has 17.38 ms to do 1.49 ms of work. That is why the wait is
zero, and why it stays zero even in loaded scenes — the maximum of 0.016 ms represents 0.09% of a frame.

### What that changes for priority D

The two figures say opposite things and both are true:

| | the sound's share |
|---|---|
| `vf3run --rec`, the sound board inside the loop | **30.5% of the frame** |
| the host, the sound board on its thread | **0.00% of the frame**, 8.5% of one core |

So **optimising the sound further buys no frame** on a machine with cores to spare: it buys **CPU time**,
that is to say thermal margin, battery life, and room for a smaller machine. The −24% gain obtained on the
DSP remains a real gain; it simply does not show in the rate here.

**What it does not establish**, and that was the limit that mattered: **one machine only**, Frédéric's,
with free cores. On two cores the sound thread competes with the game's and the pictures', and the wait
could stop being zero. **That measurement has since been made** — §4 quater: the wait stays zero, and the
sound costs twice as much. Still nothing about a scene other than the start-up and the attract.


## 4 quater. On two cores: twice as dear, and the wait stays zero

This is the measurement §4 ter named as the next one, and it **contradicts its prediction**. Frédéric ran
it on 2026-10-26 on his laptop: **AMD A9-9425, two cores and two logical processors** (no SMT), integrated
Radeon R5, Windows 10 build 19045, 1366 x 768, 4 433 frames. With a window, so under the game's real
conditions.

Steady state, the six windows of frames 1500 to 3999:

| | |
|---|---|
| the sound thread works | **5.75 ms per frame** (5.04 to 6.47) |
| the game waits for it | **0.001 to 0.002 ms**, maximum 0.437 ms |
| the frame period | **33.5 ms**, that is 29.9 frames/s |
| hence | **17.4% of one core** (12.9 to 20.6), and **0.00% of the frame** |

**The sound costs twice what it cost**, 17.4% of one core against 8.5% on the desktop machine: it is the
same work on a slower processor, and 52.2 cycles per DSP step here. With two logical processors, that makes
**8.7% of the machine**.

**But the wait does not stop being zero.** That is the result, and it had to be demonstrated: §4 ter wrote
that "on two cores [...] the wait could stop being zero". It does not stop. The thread has 33.5 ms to do
5.75 ms of work, that is a margin of 5.8; it has shrunk from a factor of 11.7 to 5.8, and it holds.

### What limits the rate, and it is not the sound

This machine's 23 to 31 frames/s are set by **the picture**, and the log says so item by item:

| | |
|---|---|
| a frame (the game's CPU) | 4.8 to 10.3 ms |
| a picture | **18.3 to 32.8 ms**, of which **8.7 to 17.1 ms of uploads** |
| the 3D's in-between | up to 11.3 ms |
| the GPU rendering itself | 1.5 to 3.3 ms only |
| the sound, in parallel | 5.75 ms, waited for 0.001 |

The GPU hardly works; it is the uploads and the triangle preparation that hold the rate. The sound cannot
be the limit at 5.75 ms of parallel work for 0.001 ms of waiting.

### What this measurement does not establish

**The probe measures the BLOCKING wait, not the concurrency.** On two cores, the sound thread and the main
thread fight over the machine: cache, scheduler, frequency. That cost does not show in `attente_ms`, which
only counts time spent waiting in `finish_frame`. To isolate it, the same scene **with the sound off**
would be needed, and the rates compared; that measurement has not been made. So one cannot say "the sound
is free on two cores" — only that **the game does not wait for it**.

> **Made on 2026-10-28: §4 quater bis.** Sound off against sound on its thread against sound in the frame,
> and both cores. The word "free" can now be weighed, and it is — **+0.12 ms per frame**.

Two other reservations: the start-up is slow on this machine (960 frames in 81 s) and the first window, at
frame 499, shows a maximum wait of 69.975 ms — that is the recompilation, not the steady state. And the
audio queue never builds up: 0 bytes everywhere, except 1 304 once.

### Where the sound's time goes, on this machine

| | the sound's share |
|---|---|
| DSP | **49.8%** (47.0 to 54.2) |
| 68000 | 35.1% |
| voices | 12.9% |
| loop and timers | 1.7% |
| mix | 0.6% |

The DSP is still half, as under `--rec` and as in the host. If the sound is ever to cost less on a machine
of this size, it is there and nowhere else — and the voices' share has doubled (6.4 → 15.0%) as the music
has got busier, which makes it the second item and not the third.


## 4 quater bis. Sound off, and both cores: what the concurrency really costs

§4 quater ended on a named reservation: the probe measures the blocking wait and not the concurrency, and
to isolate it **the same scene with the sound off** would be needed. Measured on 2026-10-28 on the same
laptop — **AMD A9-9425, two cores, two logical processors**, no SMT — by
`tools/sound/mesurer-concurrence.cmd`, which gained a second dimension that day: **the core**.

Five passes of 4 000 frames, the same scene, with a window. Medians over 70 to 76 readings per case:

| | a frame | the sound thread | the game waits |
|---|---|---|---|
| `off` — no sound at all | **12.21 ms** | 0 | 0.001 |
| `mame-normal` — on its thread | **12.34** | 6.10 | 0.003 |
| `own-normal` — on its thread | **12.38** | 6.46 | 0.003 |
| `mame-sync` — inside the frame | **16.03** | 4.67 | 0.000 |
| `own-sync` — inside the frame | **15.95** | 4.64 | 0.000 |

### Three things, and the third was not the question

1. **THE SOUND ON ITS THREAD COSTS +0.12 ms PER FRAME**, and that is at last a figure and not an absence
   of proof. Against `off`: +0.12 ms for MAME's core, +0.16 for the project's, that is **about 1% of a
   12.2 ms frame**. So the word "free" that §4 quater refused to write is almost right — almost, and now
   we know by how much.

2. **THE PROJECT'S CORE'S 8% DOES NOT SHOW, NOT EVEN ON TWO CORES.** At the frame, +0.04 ms against MAME's
   core (12.38 against 12.34): 0.3% of a frame, and **well below the spread** (the range goes from 1.86 to
   52.65 ms on this machine). That is not a measured null result, it is an **unresolved** one, and it has
   to be said that way. On the sound thread, on the other hand, the extra cost is clearly there: **+5.8%**
   (6.46 against 6.10 ms), which lines up cleanly with the +8.1% measured without a window in §4 sexies.
   **It stays on the thread, and the thread has the time.**

3. **AND THE OVERLAP BUYS 3.7 ms PER FRAME.** That is the most useful figure in the measurement, and it was
   not what we came looking for: doing the same work **inside** the frame costs 16.0 ms against 12.3 ms on
   a separate thread — +3.69 ms for MAME, +3.57 for the project. So the sound thread is not an
   architectural convenience: on a two-core machine it is worth **thirty per cent of a frame's duration**,
   and that is the first time that number exists.

### What it still does not say

The isolated spike: a maximum wait of **7.4 ms** appears once on the `own-normal` side, against 3.7 on the
`mame` side. Out of 72 readings whose median is 0.003 ms, that is an isolated accident and not a trend —
but it is not explained, and a single pass does not allow one to say whether it comes back.

And comparing the **rates** with `off` remains forbidden, for the reason the folder's README gives: cutting
the sound also cuts the pacing by audio. It is the **work durations** that compare, and they are the ones
in the table.


## 4 quinquies. What the project's core costs, and where

Measured on 2026-10-27. **Two instruments, and the second was written because the first could not answer
the question**:

| | |
|---|---|
| `tools/sound/replay` | compares the two cores **byte for byte** over the same trace. Its time is dominated by parsing 81 MB of text, identical in both and counted twice: it gives a right gap and a **wrong share** |
| `tools/sound/measure-core` | times `run()` only, and **separates the items**: idle, the slots alone, the DSP alone, both. It is what says where to go |

### By item, 32 voices and a 112-step program

As a percentage of one core, 400 000 samples per case:

| item | `mame` | `own`, first version | `own`, after measuring |
|---|---|---|---|
| idle | 0.15 | 0.21 | 0.13 |
| **the slots** | **2.10** | **4.44** | **2.48** |
| the DSP | 2.10 | 2.45 | 2.53 |
| both | 4.13 | 6.75 | **4.67** (4.55 to 4.74) |

**It was the slot loop, and not the DSP**, which the global figure could not say: the project's DSP was
never more than 17% above, when the slots were at **111%**.

### The cause, and it is mine

The field positions came from the contract's table — a good thing, it is the single source — but through a
**function static**. In C++, every read of such a static pays an **atomic initialisation check**: twenty
per slot, thirty-two slots, forty-four thousand times a second. Making `scsp_contract::slot_field`
**`constexpr`** resolves each position at compile time: the loop now does no more than a load, a shift and
a mask, and the single source is intact.

The complete path, through the replayer (3 219 865 samples, two trace parses included):

| stage | |
|---|---|
| string search inside the loop | 47.80 s |
| positions resolved once, through a static | 22.93 s |
| flags word for the DSP's one-bit fields | 22.48 s |
| DSP body specialised on what the program uses | 21.88 s |
| **positions resolved at compile time** | — and the per-item bench takes over |
| | **20.80 s** for MAME's core |

### Where it stands, without rounding in my favour

The project's core is still **11 to 14% dearer** than MAME's on the same work, that is **+0.5 points of
one core** in the 32-voice-plus-DSP case. On the 8.5% the sound costs on the desktop (§4 ter), the sound
would go to about 9.5%. That is 63% better than the first version, and it is not zero.

Three things to take from it rather than one:

1. **Correctness was never paid for with speed.** The first version, 63% slower, was **already
   byte-identical**; the replayer checked it after each of the five stages.
2. **No optimisation sacrificed the single source.** The contract remains the only place where a field's
   position lives; it is read there at compile time instead of being searched for a million times.
3. **This figure is not the sound's share of a frame.** It is a bench. The real share required the core to
   be wired onto the board, and `VF3_SND_PROF` to measure it part by part: **that is done, §4 sexies** —
   +8.1% over the whole sound board, +0.09 ms per frame, and this bench's "11 to 14%" found again in the
   game.

And a size, which is not a performance figure but is worth saying: 1 759 lines for the project's core
against 2 540 for MAME's, at identical behaviour.


## 4 sexies. The real share, with each core — and the probe that produced nothing

§4 quinquies ended on a debt: "This figure is not the sound's share of a frame. It is a bench. The real
share requires the core to be wired onto the board, and `VF3_SND_PROF` will then measure it part by part."
**Measured on 2026-10-28**, and the debt is paid — after repairing the instrument, which produced nothing.

### Three reasons to produce nothing, and each one kept quiet

The probe was right; it was its wiring that was not. The three traps are noted because each one makes a
measurement **empty or wrong without saying so**, which is worse than a noisy error:

| | what was happening | closed by |
|---|---|---|
| 1 | the call to `sndprof::report` lived **inside the `if (prof)` block**: `VF3_SND_PROF=1` without `VF3_PROF=1` printed nothing at all | the probe has its own test in `tools/vf3run.cpp`, and it writes to **the same stream** as the `prof` line (stdout) — before, one was on stdout and the other on stderr, and a redirection kept one while losing the other |
| 2 | **`vf3run` only runs the sound board with `--wav`** (all the call sites are behind that test): without it, the report produced a line of **zeros**, which reads like a measurement | `vf3run` says so at start-up, and the report says "nothing measured" instead of showing zeros |
| 3 | the report only arrived at multiples of 500 frames: a shorter run saw **none** of them, and a 4 200-frame run lost its tail | a final report over the remaining frames (`frames 4000-4200` below) |

And a fourth, which was not a hole but **a wrong figure**: the two scopes `SCSP` and `DSP` lived only in
`src/sound/scsp.cpp` and `scspdsp.cpp`, hence in MAME's core alone. On `VF3_SCSP_CORE=own`, the report gave
**97% to the 68000, zero to the voices and zero to the DSP** — not empty cells, but a wrong attribution,
since the nested share the formula subtracts from the 68000 was then zero. Both scopes are now in
`scsp_own::run` **at the places that answer** to `DoMasterSamples` and to `m_DSP.Step()`. The control that
the two measurements are comparable: both cores now count **2 886 scopes per frame, and ~58 kcycles of
`rdtsc`** each — the probe's overhead cancels out in the comparison instead of biasing it.

### The protocol, because a single pass settled nothing

One pass per core gave between +3% and +5% depending on the window, with one window **below** 1 — that is
to say nothing. So: **three passes per core, alternating** (`mame`, `own`, `mame`, … so that the thermal
drift does not carry the conclusion), windows **1500-4200** only — `--rec` only being warm after the
recompiler's second pass, around frame 1000 — that is **18 measurements of 500 frames per core**, and the
median.

```bash
VF3_PROF=1 VF3_SND_PROF=1 VF3_SCSP_CORE=mame|own build/bin/vf3run.exe "<ROMs>/fvipers2.zip" \
    --script tools/scripts/fv2-fight.txt --frames 4200 --rec --wav sound.wav --quiet
```

**The control first: the six WAVs are byte-identical**, md5 `9A2310F8AF000E09EB18BD114852B003` — the
footprint from before all this work. So the two cores measured produce exactly the same sound, and the
comparison really is about the same work.

### Where the extra cost goes: into the DSP, and nowhere else legibly

Each part's own cost, in **kcycles per frame**, median of the 18 measurements (the range in brackets):

| part | `mame` | `own` | gap |
|---|---|---|---|
| loop+timers | 46 [44-48] | 45 [43-46] | −2 kc (−3.8%) |
| 68000 | 1 015 [959-1047] | 947 [896-1045] | **−68 kc (−6.7%)** |
| voices (the slot loop) | 443 [285-565] | 484 [312-646] | +41 kc (+9.3%) |
| **the DSP** | **1 737 [1654-1818]** | **2 039 [1937-2172]** | **+302 kc (+17.4%)** |
| mix | 45 [34-48] | 46 [34-49] | +1 kc (+2.1%) |
| **the whole sound board** | **3 284** | **3 550** | **+266 kc (+8.1%)** |

**The 68000 is the control, and it has to be read before the rest**: it is the *same* code on both sides —
an interpreted 68000, which the choice of core does not touch — and it moves by −68 kc. That is **the
measurement's play**, and it has a nameable cause: `sound_prof.h`'s formula takes the nested time back out
(`68000 = M68K − (SCSP − FLUSH)`), hence a difference of large numbers. **No per-part gap smaller than
68 kc reads here.** The DSP at +302 kc exceeds it by a factor of four; the voices at +41 kc are below it.

And the DSP **with no denominator at all**, because both cores run the same program over the same frames —
180 463 steps per frame on each side:

| | cycles per DSP step |
|---|---|
| `mame` | **9.60** [9.1-10.7] |
| `own` | **11.50** [11.0-12.4] |
| gap | **+19.8%** |

**+19.8% against the +20% that `tools/sound/measure-core` gives outside the game.** Two instruments that
share nothing — a 400 000-sample bench with no board and no 68000 on one side, the whole game on the other
— land on the same number. That is the best control one could have of either.

### Three readings of the same extra cost, and all three are needed

The same +266 kc per frame, depending on what it is referred to:

| referred to | `mame` | `own` | |
|---|---|---|---|
| **the chip alone** (the sound board minus the 68000) | 2 269 kc | 2 535 kc | **+11.7%** — and that is exactly §4 quinquies' bench's "11 to 14%", found again in the game |
| **the whole sound board** (the 68000 included) | 3 284 kc | 3 550 kc | **+8.1%** — the interpreted 68000 weighs 31% of the board and **dilutes** the extra cost |
| **the frame under `--rec`** | 1.46 ms of sound out of 4.75 ms | 1.55 ms out of 4.90 ms | **+0.09 ms**, that is **0.5% of the 17.4 ms budget**; the sound's share goes from **30.7% to 31.6%** |

**It is the third that decides**, because it is the one inside a frame's budget: wiring the board onto the
project's core costs **half a point of the frame budget**, in the least favourable case we know how to
measure (`--rec`, where the Model 3 board is 4.3 times faster and the sound's share is therefore at its
maximum). On the interpreter, the same 0.09 ms is diluted in 15 ms and does not show.

The measurement's noise in milliseconds reads in the same place: the Model 3 board, which does not depend
on the sound core, gives 3.29 against 3.35 ms — **1.8%**. The sound's 0.09 ms, that is 6% of 1.46, is
therefore above it, but not by much: that is why the conclusion is carried by the kcycles and not by the
milliseconds.

The first line carries an assumption that has to be stated: **the 68000 is held constant at 1 015 kc**
there, because it is the same code on both sides and its measured −68 kc is play, not a gain — so the whole
+266 kc is charged to the chip. Taken raw, without that assumption, the same calculation would give 2 269
against 2 603 kc, that is **+14.7%**: the top of the bench's range rather than the bottom. Both readings
stay within "11 to 14%", and that is what matters.

### What this measurement does not establish

1. **Nothing about the slot loop.** §4 quinquies' bench gives it at **+18%**; here it is +9.3%, **under the
   68 kc of play**. +18% on 443 kc would be +80 kc: so the measurement is compatible with anything between
   zero and the bench's figure, and it does not settle it. It would take profiling to the instruction,
   which `VF3_SND_PROF` does not do — its scopes are too coarse for a loop over 32 slots.
2. **Nothing about the host**, where the sound is on its own thread (§4 ter): `VF3_SND_HOST` and
   `tools/sound/measure-host.cmd` are that question's instruments, and they have not been rerun on the
   project's core.
3. **Nothing about another machine.** One desktop processor, one scene. The measurement on the two-core
   laptop (§4 quater) has not been redone with both cores.
4. **Nothing about the other arenas.** `fv2-fight.txt` runs **one** 112-step DSP program; three arenas
   load their own (§5). A longer program would shift the DSP's share, hence the extra cost.


## 4 septies. The DSP by characteristic: it is no instruction, it is the baseline

§4 sexies left the DSP at **+19.8% per step** and said that one notch finer would take "profiling to the
instruction that this workstation does not have". **Written on 2026-10-28**: `tools/sound/measure-core
--dsp`, and no profiler was needed.

### The instrument

§4 quinquies' "DSP" item is **a single number for a whole instruction set**. So one notch finer, and in the
same way: programs of **112 steps where every step does only one thing**, timed on both cores. The
difference between the baseline and the baseline plus one characteristic **is** that characteristic's cost,
per step. Three passes, median.

And the column that counts is the **net cost** — the value minus the baseline. The raw time says nothing:
the baseline is in every line, and it already carries a gap.

```bash
build/bin/measure-core --dsp 600000
```

### What it gives

600 000 samples per case, 112 steps per program, ns per DSP step:

| characteristic | `mame` | `own` | gap | **net `mame`** | **net `own`** | **net gap** |
|---|---|---|---|---|---|---|
| **baseline** (112 steps that do nothing) | **2.722** | **3.242** | **+0.520** | — | — | — |
| IRA → MEMS | 2.642 | 3.238 | +0.596 | −0.079 | −0.004 | +0.075 |
| COEF | 3.021 | 3.332 | +0.311 | 0.299 | 0.090 | **−0.209** |
| the product | 3.021 | 3.453 | +0.432 | 0.300 | 0.211 | −0.088 |
| TWT | 2.636 | 3.318 | +0.682 | −0.086 | 0.076 | +0.162 |
| IWT | 3.196 | 3.522 | +0.326 | 0.474 | 0.280 | **−0.194** |
| **MRD** | 4.141 | 5.384 | +1.242 | 1.420 | 2.142 | **+0.722** |
| MRD + UNPACK | 4.351 | 5.704 | +1.353 | 1.630 | 2.462 | **+0.832** |
| **MWT** | 4.223 | 5.476 | +1.253 | 1.501 | 2.234 | **+0.733** |
| MWT + PACK | 6.387 | 7.796 | +1.409 | 3.665 | 4.554 | **+0.888** |
| ADREB + NXADR | 4.136 | 5.019 | +0.883 | 1.414 | 1.777 | +0.363 |
| EWT | 2.856 | 3.515 | +0.659 | 0.135 | 0.273 | +0.139 |
| SHIFT 1 (clamps) | 3.124 | 3.150 | +0.026 | 0.402 | −0.092 | **−0.494** |
| SHIFT 3 (wraps) | 2.827 | 3.881 | +1.055 | 0.105 | 0.640 | **+0.534** |
| FRCL | 3.678 | 3.599 | −0.078 | 0.956 | 0.357 | **−0.599** |
| ADRL | 3.687 | 3.734 | +0.046 | 0.966 | 0.492 | −0.474 |
| YRL | 3.608 | 3.597 | −0.011 | 0.886 | 0.355 | −0.532 |
| NEGB | 3.479 | 3.666 | +0.187 | 0.757 | 0.424 | −0.334 |
| the four rare ones together | 3.800 | 3.667 | −0.133 | 1.078 | 0.425 | **−0.653** |
| | | | | | | |
| **the game's shape** — the control | **4.312** | **4.941** | **+0.629** | 1.590 | 1.699 | **+0.108** |

### The three things it establishes, and the first overturns what we believed

1. **IT IS NO INSTRUCTION. IT IS THE BASELINE.** 112 steps that do nothing cost the project's core
   **+0.520 ns per step**, that is **+19%** — and the control, the program of the game's shape, has a **net
   cost between the two cores of almost nothing** (+0.108 ns, and −0.245 on a second pass: zero at this
   bench's precision). In other words: **once the baseline is taken away, the two DSPs execute the game's
   instruction mix at the same price.** All of the +19.8% measured in the game is a step's unavoidable path
   — the input selection, the sign extension, the TEMP read, the two `switch`es, the shifter — and not any
   particular instruction.

2. **THE PROJECT'S CORE IS BETTER ON HALF THE CHARACTERISTICS**, and that has to be said too: COEF
   (−0.209), IWT (−0.194), the product, `SHIFT 1` (−0.494), and **the four rare fields** (−0.33 to −0.65).
   The last of those are explainable: setting them switches both cores over to their second body, and
   `dsp_own`'s is cheaper than `Run<USES_ALL>`. **It is the only place in the repository where those four's
   cost reads**, since the game never sets them (§5).

3. **THE ONLY ITEM WHERE IT IS GENUINELY WORSE IS MEMORY ACCESS**: `MRD` **+0.72**, `MWT` **+0.73**, and
   the packing on top (`UNPACK` +0.83, `PACK` +0.89), plus `SHIFT 3` (+0.53). In the game that weighs 33
   steps out of 112 (23 reads, 10 writes, §5), that is about **+0.21 ns per step** — roughly **40% of the
   baseline's gap**, and therefore the second place to go.

### Per step or per sample? Settled, and with its control

A per-sample cost — clearing EFREG, the branch to one of the two bodies, what `Start` leaves behind it —
divides by the number of steps and **disguises itself as a per-step cost**. So the bench measures the
baseline at three program lengths: the **slope** is a step's cost, the **intercept** what the sample costs
before its first step.

| | 16 steps | 48 steps | 112 steps | → ns per step | → ns per sample |
|---|---|---|---|---|---|
| `mame` | 72.6 | 159.2 | 303.2 | **2.402** | 34.2 |
| `own` | 86.3 | 188.2 | 365.0 | **2.903** | 39.8 |

**The gap is per step: +0.50 ns (+21%)**, and it is so on both passes (+0.50 and +0.65). Over a 112-step
program, the per-sample share weighs only **±0.05 ns per step, less than 10% of the baseline's gap**:
repairing outside the step loop would return almost nothing.

**And the intercept is NOT resolved — the bench says so itself.** The two passes give it opposite signs
(−6.1 then +5.7 ns) and the linearity control refused `mame`'s straight line on the second. It is the same
disease as §4 sexies' "68000" control: a difference of large numbers. The control is in the tool so that
nobody reads those two numbers as a measurement; **only the slope is publishable.**

### What is left to do, and in this order

1. **Read the generated assembly** of the two step loops — `g++ -S -O2` on `dsp_own::loop<false>` against
   `SCSPDSP::Run<0>` — **on the unavoidable path only**, now that the bench says which one it is. It was
   useless before: one would have compared two pages looking for what.
2. **Two structural hypotheses are already dead**, and by inspection, not by reasoning: the project's core
   does have its **two bodies** (`template <bool RARES> void loop()`), and its decoded step is **smaller**
   than MAME's — 16 bytes against 28 (`dsp_own::DecodedStep` against `SCSPDSP::Inst`). So it is neither the
   missing specialisation, nor the program's cache footprint.
3. **Then the memory access**, the second item, with the same bench to judge.

The bar is set in advance, as for §7's three batches: **byte-identical WAV**, footprint
`9A2310F8AF000E09EB18BD114852B003` unchanged.

### Then the assembly, and the question had to be narrowed first

Read on 2026-10-28, `g++ -S -O2` on the two step loops — `dsp_own::loop<false>` against `SCSPDSP::Run<0>`
— **and only after the bench had said it was the baseline**. Read before, one would have compared two pages
of a hundred and forty instructions looking for what.

**Five hypotheses are dead, and none by reasoning:**

| hypothesis | what killed it |
|---|---|
| the two bodies' specialisation is missing | `dsp_own` has them: `template <bool RARES> void loop()` |
| the decoded step is bigger, hence more cache | it is **smaller**: 16 bytes against 28 (`DecodedStep` against `Inst`) |
| a `switch` compiles to a jump table, hence an indirect branch | **zero** indirect jumps on either side |
| a division, or a slow multiplication | neither has one |
| the 64-bit product is a divergence of this port's | **MAME writes the same line**: `s64 const v = (s64(X) * s64(Y)) >> 12` |
| `std::clamp` compiles to branches on one side and to `cmov` on the other | both use `cmov`: 25 against 22 |

**And what remains can be counted.** The step loop, epilogue excluded:

| | instructions | **branches** | moves |
|---|---|---|---|
| `SCSPDSP::Run<0>` | 108 | **16** | 37 |
| `dsp_own::loop<false>` | **145** | **31** | 36 |

**+34% of instructions and almost TWICE the branches, for the same algorithm.** There is the cost, and
there it is: a step's unavoidable path is more branchy in the project's core. The `cmpb` say so (9 against
0) — they are the byte comparisons of the `IRA` chain and of the two `switch`es.

### The measurement by removal, and what it is worth

To confirm, a **disposable** experiment: remove a piece of the unavoidable path, measure the baseline, put
it back. Every variant makes the DSP wrong, so none of it is kept — only the figure is. And every variant
is **bracketed by a measurement of the reference**, because a first attempt saw the reference gap move from
+0.41 to +0.98 depending on the moment: that is why `--baseline` exists, fast enough to interleave.

600 000 samples, seven passes per measurement. "closed" is the share of the reference gap that disappears:

| what is removed | gap | closed |
|---|---|---|
| the `IRA` input chain reduced to MEMS | +0.831 | −14% |
| TEMP's sign extension | +1.090 | −101% |
| **the YSEL `switch` reduced to its case 0** | **−0.014** | **103%** |
| **the SHIFT `switch` reduced to its case 0** | **−0.038** | **106%** |
| the 64-bit product brought back to 32 | +0.482 | 17% |

The unavoidable path's two `switch`es **close the gap on their own**, each one entirely — which is
consistent with the 31 branches.

**BUT WHAT THIS EXPERIMENT CANNOT DO HAS TO BE SAID, and it matters:** a removal does not only take work
away, it **exposes invariants**. In the baseline, `YSEL` is zero at every step, so `y` is `m_frc_reg`, so a
loop constant: by removing the `switch`, the compiler is allowed to hoist it out of the loop, and perhaps
to simplify the product with it. The removal **locates the region, it does not quantify it** — "103%" is
not the `switch`'s cost, it is the `switch`'s cost plus what its absence permits. The two columns are read
together or not at all.

### What is left to do, and it is precise now

**The direction seemed named: resolve AT DECODE what gets re-branched at every step of every sample.**
`Decode` already builds the `DecodedStep` once per program write; the `IRA` chain (three byte comparisons
and a `return`) and the two four-case `switch`es would be resolved there once and for all. 44 100 times a
second per chip, that is where the branches go.

> **TRIED THE SAME DAY, AND IT LOSES. See §4 octies.** Both ways of doing it are SLOWER than what they
> replace, by 7% and by 16%. The sentence above is kept because it was reasonable and it is wrong: the
> number of branches was not the cost.

And the bar does not change: **byte-identical WAV**, footprint `9A2310F8AF000E09EB18BD114852B003`.

**What remains out of reach**, and it is better written down: why 145 instructions instead of 108 for the
same algorithm written the same way, a hardware counter would say in one pass — instructions retired,
mispredicted branches, cycles. The question is now narrow enough for a profiler to answer it in one go,
which was not true this morning.

> **"This workstation does not have one" — AND THAT WAS FALSE, see §4 nonies.** The sentence stayed there a
> whole day and it served twice to conclude. The Windows Performance Toolkit is installed on this machine,
> with the counters that were needed. I had not looked for it. The counter answered on 2026-10-28: the
> mispredicted branches are **identical** on both sides, and it is the instructions that are more numerous.

### What this bench cannot say

A program whose 112 steps all do the same thing has **neither the cache misses nor the dependencies between
steps** of the real program. It **locates** a cost, it does not predict it — and the control shows it: its
total gap (+0.629 ns per step, that is +15%) is below the **+19.8%** measured in the game (§4 sexies). The
bench underestimates. That is why the control is in the output: if its gap were not of the same order as
those in the table, it is the bench that should be believed least.


## 4 octies. Two batches that lost, and what they teach

§4 septies had named a direction: the step loop's 31 branches against MAME's 16, hence resolving the `IRA`
chain and the two `switch`es at decode. **Tried on 2026-10-28, and both ways of doing it are slower.**
Undone. It is this document's second batch to end that way, after §7's fourth, and it is kept for the same
reason: an attempt that loses is a measurement.

### The two batches, and what they cost

Both do exactly what §4 septies asked — the `IRA` chain becomes an array index, the two `switch`es become
flags already set, and the chain's `return` is found once at `Start` rather than tested at every step. They
differed only in **where to put the decisions**:

| | what changes | `own` baseline | ratio to `mame`'s baseline |
|---|---|---|---|
| **the original** | — | 3.48 — 3.57 | **1.22** |
| **batch A** | five more bytes in `DecodedStep`: `src`, `idx`, `ira_dec`, `y_src`, `y_dec` | 3.50 — 3.56 | **1.30** |
| **batch B** | the same decisions put into the flags word's free bits | 4.24 — 4.33 | **1.42** |

Measured by `measure-core --baseline 600000`, three times each, **in the same session** — that is why this
comparison is worth something, `mame`'s baseline having itself drifted from 2.70 to 3.21 between the
beginning and the end of the attempts.

### What it teaches, and it is more useful than the batch

1. **THE NUMBER OF BRANCHES WAS NOT THE COST.** That is the hypothesis §4 septies drew from the assembly —
   145 instructions and 31 branches against 108 and 16 — and it was reasonable. It is wrong: removing a
   good part of them makes the code **slower**. A static instruction counter does not measure a time, and
   this document has just paid for that a second time.

2. **BATCH A SAID WHY: THE DECODED STEP'S SIZE.** Five more bytes take `DecodedStep` from 16 to 24 bytes,
   hence the program from 2 KB to 3 KB, on a path walked 112 times per sample and 44 100 times a second.
   **+7% for one kilobyte.** And it is the exact converse of what §4 septies had ruled out: "the decoded
   step is bigger, hence more cache" was among the dead hypotheses there, because the project's is SMALLER
   than MAME's. It is — and that is precisely why it must not be made bigger.

3. **BATCH B SAYS THE REMEDY IS WORSE.** Putting the same decisions into the word's free bits keeps the 16
   bytes, but every step then pays three extractions (`>>` then `&`) to read them back. **+16%**, double
   batch A's. Reading a byte is cheaper than unpacking a field, and the loop does that 112 times per
   sample.

4. **AND TAKEN TOGETHER IT SAYS WHAT TO GO AND LOOK FOR.** Three instruments have spoken — the
   per-characteristic bench, the assembly, and these two batches — and they agree on the place (the
   baseline, per step) and disagree on the cause. What is needed is **a hardware counter**: cycles,
   instructions retired, mispredicted branches, cache misses, over two loops of the same size.

   > **AND IT IS ON THIS MACHINE.** I wrote here "the instrument is not there" without having looked for
   > it. `xperf`, from the Windows Performance Toolkit, is installed. Measured the same day, **§4 nonies**:
   > the mispredicted branches are identical, so conclusion 1 above is right for a reason it did not give
   > — the extra branches are **perfectly predicted**, they cost their decoding and not their misses.

### What was kept from these two batches

**Nothing in `src/`.** `dsp_own.{h,cpp}` came back identical, and the three measurements of the original
above were redone after the restoration to check it. What remains is `measure-core --baseline`, written for
these attempts and serving every later one: it interleaves a reference and a variant of the same file,
which is what was needed to see that batch A lost 7% while the machine itself drifted by 19%.


## 4 nonies. The hardware counter: it is not the branches, it is the instructions

**And first a correction, because it cost a day of conclusions.** §4 septies and §4 octies both say there
is no hardware counter on this workstation. **That is false.** The Windows Performance Toolkit is installed
there — `xperf` — and it exposes `InstructionRetired`, `BranchMispredictsRetired`, `UnhaltedCoreCycles`,
`CacheMisses`, `LLCMisses` and `BranchInstructionRetired`. I had declared it absent without looking for it,
twice, and both sections stopped there.

### What it took for the measurement to come out, and it was worth noting

Four attempts, and none of the first three says it is failing:

| | why nothing came out |
|---|---|
| without elevation | `Failed to configure counters` — the only frank message of the four |
| `-Pmc <counters> CSWITCH` | counts on an event: `-a perfctrs` returns `No Counter Info`, `-a pmc` a header with no lines, and the `CSwitch` events carry no counter column |
| `-PmcProfile` with `-on PROC_THREAD+LOADER` | the right switch, but **the kernel group that records the samples was not turned on**: 54 885 `UnknownEvent/Classic` and not one `PmcInterrupt` |
| `-on PROC_THREAD+LOADER+PMC_PROFILE` | **that one works.** `PMC_PROFILE : PMC sampling events`, and `xperf -providers K` names it itself |

And one more trap, which is not `xperf`'s: our binaries come from MinGW, hence in **DWARF with no PDB**,
which `xperf` does not read. It would have named no function. Hence `measure-core --baseline <core>`,
written for the occasion: **one core per process**, and the process's counters are then that core's
baseline's. 99.9% of the samples fall in our own module; the rest is kernel noise.

### What the counter says

Two captures, `--baseline mame 2000000` and `--baseline own 2000000`, 1 568 000 000 DSP steps each:

| | `mame` | `own` | ratio |
|---|---|---|---|
| **instructions retired** | 1 404 064 smp. | 1 605 796 smp. | **×1.144 — +14.4%** |
| **mispredicted branches** | 608 smp. | 614 smp. | **×1.010 — identical** |

**ONLY THE RATIOS ARE PUBLISHABLE, AND WHY HAS TO BE SAID.** `xperf -pmcsources` announces an interval of
65 536 events per sample, and that number does not hold: applied to the cycle capture, it gives 2.14
billion cycles over 4.09 seconds of work, that is 523 MHz — impossible. Applied to the instructions, it
would give 22.5 billion instructions per second on a single thread, which does not exist either. So the
REAL interval is not the one displayed, and **no absolute value is usable**. The ratios, for their part, do
not depend on the interval: the same counter, the same load, the same number of steps on both sides. That
is all the question needed.

### The two conclusions

1. **IT IS NOT THE BRANCHES.** `BranchMispredictsRetired` is the same to within 1% — about one miss every
   forty steps, on both sides. So `dsp_own::loop`'s 31 branches against MAME's 16 are **perfectly
   predicted**: they cost their decoding, not their misses. §4 septies' hypothesis falls for good, and it
   explains at last why §4 octies' two batches lost — **they were removing something that cost nothing**,
   while adding cache (batch A) or bit unpacking (batch B).

2. **IT IS THE INSTRUCTIONS: +14.4% per step.** And it is the first figure in this whole investigation one
   can aim at. §4 septies' static count gave +34% (145 against 108); the dynamic one gives **+14.4%**, so a
   good part of those 145 does not execute in the baseline — which was expected, and which nobody had
   measured. So the next job is arithmetic: find, in a step's unavoidable path, the instructions MAME does
   not execute.

### The hunt for the eight instructions, first lead: the restricted pointer

+14.4% of instructions per step is **about eight instructions** to find in the unavoidable path. And the
first lead does not come from the loop bodies — they are almost identical, line for line — but from their
**headers**. MAME's carries this, which the project's did not have:

```c++
u16 *const __restrict rp = ram;
```

Its comment says exactly why, and it is a true promise: sound RAM is the 68000's, a separate allocation,
and nothing else in the function reaches it. Without `restrict`, the compiler has no way of knowing that it
is not **inside** this object — so a step that writes the delay line could, for all it knows, change `dec`,
`rbl`, `rbp`, or any word of `TEMP` and `MEMS`. It has to reread everything after every possible write,
that is to say at every step, the write being behind a test. It was a complete explanation, elegant, and
attributable to one missing line.

**It gains nothing. Defeated.**

| | `own`, ns per sample | |
|---|---|---|
| baseline, A/B bracketed | ratio 1.2180 → 1.2170 | **−0.08%**, and the three ratios overlap |
| the game's shape (33 steps out of 112 do an access) | 593.4 → 595.4 | flat |

**AND HOW I ALMOST KEPT IT HAS TO BE SAID.** On the game's program, the `own`/`mame` RATIO fell from 1.161
to 1.081 — a gain of 6.9%, clean over three rounds. It was false: `mame` went there from ~515 to ~551 ns
**while only `dsp_own.cpp` had changed.** My driver always ran "without" then "with", so the machine warmed
up and inflated the second's denominator. The gain was the drift, entirely. The ratio, which protected
against §4 octies' two batches, becomes a trap as soon as **only one of the two sides** changes: it is then
the ABSOLUTE of the changed side that must be read, and that one did not move.

### What it would take to find those eight instructions

The counter can count them, it cannot yet say **where** they are: `xperf` attributes by module, and our
MinGW binaries give it no name. Two routes, and neither is long:

1. **Do the mapping by hand.** The `PmcInterrupt` samples carry an address; `nm` or `objdump` gives the
   ranges of each of the exe's functions. Nothing prevents converting one into the other offline, and the
   attribution then becomes per function, even per line with the DWARF tables.
2. **Build this bench with a compiler that emits a PDB** (clang-cl, or `-gcodeview`), for this bench only.
   `xperf` would name everything, and the rest of the repository would not change.

The first needs nothing but work; the second needs checking that the code measured really is the same. And
the question itself is now properly put: **where are the eight instructions MAME does not execute?**

### The first route, taken: as far as the function yes, as far as the instruction no

Done on 2026-10-28, with no PDB and rebuilding nothing. `xperf`'s dumper prints each sample's raw
`PrgrmCtr`; the trace gives the exe's load base, `objdump -p` its `ImageBase`, and `nm` the ranges of its
557 functions. One subtraction and a binary search later, every sample has a name.

**And the result moves the figure, because the functions have to be PAIRED** — they do not carry the same
name on both sides, and a table comparing them by name gives nothing:

| | `mame` | `own` | gap |
|---|---|---|---|
| **the step loop** | `Run<0>` **1 249 810** | `loop<false>` **1 494 867** | **+19.6%** |
| the per-sample envelope | `DoMasterSamples` + `Step` + `run` **127 858** | `scsp_own::run` + `Step` **107 746** | **−15.7%** |
| `lround`, `ceil`, `log2l` | 20 842 | ~190 | — |
| **the whole process** | 1 404 064 | 1 605 796 | +14.4% |

Three things come out of it:

1. **THE GAP IS BIGGER AND MORE LOCALISED THAN +14.4%.** The step loop takes **+19.6%** of it; the global
   figure was diluted because the project's core **gains 15.7%** on the per-sample envelope. §4 septies had
   already seen it by separating the per-step from the per-sample; another instrument confirms it.
2. **THERE IS INDEED AN IPC LOSS, and it is deduced without the cycle capture**: +14.4% of instructions for
   +21.8% of time gives an IPC lower by about **6%**. So the missing capture would no longer say anything
   essential, and the list of what remains is lightened by it.
3. **A detail that is not the subject**: `mame` spends ~1.5% of its instructions in `lround`, `ceil` and
   `log2l`, which the project's core does not call at all. That is floating point in `DoMasterSamples`,
   outside the DSP — to look at another day, on the envelope's side.

### And why it stops at the function

Two finer notches were tried, and **neither is usable**:

| | why |
|---|---|
| **by source line** (DWARF tables, `addr2line`) | at `-O2` the inlining blurs everything: several "hot lines" are COMMENTS, and `std::clamp` appears as `stl_algo.h:3617` in the project's core while it was folded into the calling line in MAME's. The two columns do not measure the same thing |
| **by instruction** (disassembly, each sample attached to the preceding instruction) | **the sampling skid.** Without PEBS, a PMC sample lands several instructions after the one that retired, and it piles up on some: `test` and `ja` carry half the project's core's samples between them, and MAME comes out at 70% loads. Two code layouts skid differently, so the two histograms are not comparable **with each other** |
| **by instruction, second attempt**: take the path to be the SET of addresses touched, on the grounds that a sample always lands on an instruction that was REACHED | **`xperf`'s `PrgrmCtr` is not an instruction address.** Disassembly anchored on each loop's symbol, hence on a safe start: **21 of MAME's 84 hot addresses and 35 of the project's 104** land on one, that is **2% and 22% of the samples**. Two thirds of the addresses designate no instruction, so the set is not the path |

The function level, for its part, is robust, and for a reason that can be quantified: the two loop bodies
are **1 072 and 1 248 bytes**, that is 283 and 307 instructions; a skid of a few tens of bytes therefore
stays inside the same function. **The notch below the function does not exist with this toolkit** — three
attempts, three different reasons, none of them circumventable without PEBS or without a PDB. One
indicative figure survives the notch below, and it is worth noting: **84 distinct addresses touched in
MAME's loop against 104 in the project's**, that is twenty more instructions on the path actually executed.

### The next instrument, and it is better than the one just worn out

Rather than looking for WHERE the eight instructions are, **count them for each variant**. The
instructions-retired counter is far less noisy than time: it does not drift with temperature, it does not
depend on the order of the measurements, and that is precisely what made the `restrict` reading fail above
and almost validated a false gain of 6.9%. One capture per variant, and the verdict is clear-cut.

What it requires: an **administrator** window per capture, hence Frédéric. What it would return: §4 octies'
two batches rejudged on a hard figure, and every later attempt with them.

### What is still missing, and it is one single thing

**The CYCLES ratio.** `mame`'s capture is made; `own`'s is not. With both one would know whether there is,
on top of the 14.4% of instructions, an IPC loss — the quotient lets it be guessed: 14.4% more instructions
should give 3.107 ns per step, and the bench measures about 3.35. So **about 8% would remain to be
explained otherwise** than by the number of instructions. The command is in the resumption note.

### The five variants are ready, and awaiting an administrator window

Prepared on 2026-10-28. Each one is a transformation of `src/sound/dsp_own.cpp` **equivalent down to
behaviour** that attacks the NUMBER OF INSTRUCTIONS of the unavoidable path, where the gap goes:

| variant | what it attacks |
|---|---|
| `control` | the code as it is, so that the other four compare against something measured the same day on the same machine |
| `grouped_output` | a baseline step does none of the four outputs and yet tests them one by one: a single mask rules them all out |
| `lazy_input` | `entree` is computed at every step and **thrown away** in the baseline — XSEL zero, IWT zero, rare readers outside the lean body. Three comparisons, a load and a sign extension for nothing. **MAME does not do that either** |
| `shift_two_bits` | the four-case `switch` asks two independent questions; two bits of the flags word ask them once, **without growing the decoded step** — which is what sank batch B |
| `ysel_coef_first` | MAME tests `YSEL == 1` first, and for its reason: 87 of the game's 112 steps take it (§5). No effect on the baseline; it is the game's program this would aim at |

**The exes are built and already verified** — `python tools/sound/build-variants.py` makes them one by one,
runs **the ten tests** every time, and keeps only those whose 24 files are green. So the administrator
window has nothing to compile: `tools/sound/measure-variants.cmd` chains capture, exe, stop, once per
variant, and checks first that the counters are programmable so as not to discover on the third round that
the window was not elevated.

**AND ONE OF THE FIVE WAS REFUSED ON THE FIRST ROUND, BY CASES 9 TO 13.** `grouped_output` grouped the four
output tests into a mask that swept up along the way the writes of `FRCL` and of `ADRL` — they live BETWEEN
`TWT`'s and `EWT`'s. So a step that sets only `FRCL` was skipping them. It passed **all** the other tests,
would have passed the replayer and produced a byte-identical WAV, because no game sets those two fields.
See §5: that is what those cases really buy, and it is not what I had announced.

**AND THE LESSON OF METHOD, which is worth more than the measurement:** I wrote twice "this workstation has
no hardware counter" without running a single command to check, and both sections stopped on that sentence.
An instrument declared absent closes a line of enquiry as surely as a wrong measurement.


## 5. What the DSP program really contains

Measured (`dspprog`, printed by `SCSPDSP::Start` under `VF3_SND_PROF=1`), over the 112 steps of Fighting
Vipers 2's program:

| control | steps that set it |
|---|---|
| `IWT` (MEMS write) | 23 |
| `TWT` (TEMP write) | 39 |
| `FRCL` | **0** |
| `ADRL` | **0** |
| `YRL` | **0** |
| `NEGB` | **0** |
| `MRD` / `MWT` | 23 / 10 |
| memory access (MRD or MWT) | 33 — **all on an odd step** |
| `EWT` (EFREG output) | 5 |
| `ZERO` / `BSEL` / `XSEL` | 36 / 39 / 37 |
| `YSEL == COEF` | 87 |
| `IRA` in MEMS | 108 |

### Looked for in every game this port serves, and always zero

The table above carried **one program from one game only**, and `dsp_own.h`'s header drew the sentence "this
game's programs leave them at zero" from it. **Widened on 2026-10-28**: `dspprog` read over every set
`src/machine/romset.cpp` knows, in attract and through a whole fight.

| | program writes | distinct shapes | `FRCL` `ADRL` `YRL` `NEGB` |
|---|---|---|---|
| `fvipers2` | 45 | 4 — of 0, 108, 112 and **127** steps | **0 everywhere** |
| `vf3` | 46 | 3 — 0, 108, 112 steps, **the same as `fvipers2`'s** | **0 everywhere** |
| `srally2` | 61 | 2 — 0 and 112 steps, but **a different program** | **0 everywhere** |
| `vf3tb`, `fvipers2p` | — | — | incomplete sets, unusable |

**152 program writes, five distinct shapes, three games, and not one of the four controls.**

Three things to take from it:

1. **`srally2` is an independent control**, and it is what gives weight to the rest: its 112-step program is
   not the fighting games' — `iwt 18 twt 28 mrd 18 mwt 6 ewt 6` against
   `iwt 23 twt 39 mrd 23 mwt 10 ewt 5`. Another game, another driver, another program, the same four zeros.
   `vf3` and `fvipers2`, for their part, load **exactly the same programs**: they do not count twice.
2. **Two more programs than in the original §5**: one of 108 steps, and one of **127** that only appears
   during a `fvipers2` fight. The table above saw only the 112-step one, and it still holds: reread that
   day, it gives the same twelve numbers.
3. **So the question is not closed by one game.** There is no program to go and find that would exercise
   those four fields and make the replayer their byte-for-byte judge. What remains is to extend the
   **specification** — `tools/sound/test_scsp_dsp.cpp`, which already judges both cores, and whose
   equivalent cases for MAME's DSP `tools/sound/test_scspdsp.cpp` already carries. With what that buys and
   what it does not: consistency with the declared target, and not proof that MAME is right
   (`docs/scsp_references.md`: never consensus between emulators as proof).

**AND WHAT THEY REALLY BUY IS NOT WHAT I HAD ANNOUNCED.** I had written that those five cases buy
consistency with the declared target, and not proof that MAME is right — which is true and stays true. But
**four hours after writing them, they stopped an optimisation**: §4 nonies' `grouped_output` variant
grouped a step's output tests into a mask that swept up the writes of `FRCL` and of `ADRL`. It passed all
the other tests, would have passed the replayer and produced a byte-identical WAV — **because no game sets
those two fields**, and nothing else looks at them. **Those cases protect the four fields against our own
optimisations**, and that is what they are worth most for: the day this repository serves a game that sets
them, it will be thanks to them.

And a figure §4 septies produced along the way: those four, **when they are set**, cost **+1.078 ns per
step** to MAME's core and **+0.425** to the project's — the only place in the repository where their cost
reads, since no game sets them.

Two things read in all that:

1. **Four controls are never set** — `FRCL`, `ADRL`, `YRL`, `NEGB`. Testing them at every step of every
   sample is four branches per step for nothing, and it keeps `FRC_REG`, `Y_REG` and `ADRS_REG` alive when
   they can then only be zero.
2. **Every memory access already falls on an odd step.** MAME's `if (MRD && (step & 1))`, which one might
   think worth optimising by moving the test onto the whole block, would gain nothing here: there is no
   even step to avoid. *Measured before touching it — that is exactly why one measures.*


## 6. Hot paths, and what the measurement says of each

| path | share | what the measurement says |
|---|---|---|
| `SCSPDSP::Run` (formerly `Step`), 112 steps × 44.1 kHz × 2 chips | **62-69%** | the target; ~16.3 TSC cycles per step at the start |
| MAME's 68000, its virtual bus calls (`m68000_bus::read/write/peek`) | 23% | one indirect call per memory access; `third_party`, not to be touched without LTO or a non-virtual interface |
| `UpdateSlot` × 32 voices | 6-13% | rises with the number of active voices; arrays of pointers to locals (`addr[2]`, `slot_addr[2]`) and a 2-round loop on `LPCTL` |
| the loop iteration and the sweep of the 10 timers | ~1.2% | not worth a line |
| the stereo mix and the limiter | ~1% | not worth a line |
| the host's `std::function`s (`sample_index`, `time_is_zero`) on every register access | within the 23% | an indirect call, measurable only by removing it |
| `getenv` in `midi_in_byte` | negligible (a few bytes per frame) | the same pattern as the per-CROM-read `getenv` already corrected elsewhere; to be cleaned up for tidiness, not for speed |

Nothing above allocates on the real-time path, except `m_out.push_back` (two floats per sample, amortised
capacity) and the mix's `out.push_back` — both vectors keep their capacity from one frame to the next.


## 7. What was changed, and what it gave

### Batch 1 — the DSP's body specialised on what the program uses

`SCSPDSP::Step` becomes `Run<U>`, a template on the set of controls the program really touches
(`SCSPDSP::uses`, computed by `Start` over the whole program and only **increased** by `Decode` when a step
gains one — a bit set for nothing only costs the slow body). For a program that sets neither `FRCL`, nor
`ADRL`, nor `YRL`, nor `NEGB` — Fighting Vipers 2's and Virtua Fighter 3's — the compiled body no longer has
those four tests, and `FRC_REG`, `Y_REG`, `ADRS_REG` collapse to zero. Along the way, the `TEMP` word that
`B` and `X` both read is read and extended **once**.

### Batch 2 — the local copies, against sound RAM's aliasing

A step that writes into the delay line writes through `ram`, and nothing tells the compiler that sound RAM
is not **inside the DSP object itself**: without copies, `DEC`, `RBL`, `RBP` and `LastStep` have to be
reread after every write, and so does every word of `TEMP`. The batch takes local copies of them and goes
through a `__restrict` pointer — a true promise: sound RAM is the 68000's, a separate allocation.

### Batch 3 — the one-bit controls grouped into a word

A step was fetching **eight separate bytes to test eight bits**. `Inst::f` puts them into a `u16`, with
`SHIFT` and `YSEL` thrown in; `Inst::mf` does the same for the four read only inside the memory block. The
choice of `Y` tests the `COEF` case first, the one of 87 steps out of 112. The originally named fields stay
in `Inst`: `Decode` and the `dspprog` report read them, and they say what they are.

### What the three batches give

The same scene, the same machine, probes off. Each batch's detail is in
`docs/scsp_benchmark_baseline.md`.

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
| against the base | — | −11.4% | −14.1% | **−24.3%** |
| cycles / DSP step | 16.3 | 13.2 | 13.1 | **10.0** |
| the DSP's share of the sound board | 62-69% | 57-66% | 57-66% | **52-60%** |

The share of the frame goes from **10.6% to 8.0%**, and the DSP from two thirds of the sound board to a
little over half.

**The sound has not moved by one sample**, and that is the condition for keeping the batches, not a bonus.
Checked against a build of the reference commit `335c050`, on **both** games the port serves:

| game | scene | frames | WAV | verdict |
|---|---|---|---|---|
| `fvipers2` | `fv2-fight.txt` | 4200 | 12 879 504 bytes | **byte-identical** |
| `vf3` | `vf3-match.txt` | 2400 | 6 841 492 bytes | **byte-identical** |

### A fourth batch, which gained nothing, and was undone

`UpdateSlot`'s two-address loop, unrolled (MAME walks `(addr1, cur_addr)` then `(addr2, nxt_addr)` through
it via two arrays of pointers to locals). Bit for bit identical, clearer code — and **a mean of 1.399 ms
against 1.400 ms**, that is zero. Undone: a change in a fidelity file that buys nothing is not kept. The
negative result, on the other hand, is recorded (`docs/scsp_benchmark_baseline.md`): the voices are not won
there.


## 8. What was not done, and why

- **The 68000** — 23% of the sound board at the start, **28 to 31% after the batches**, since it is the DSP
  that got thinner — is in `third_party` and its handlers are MAME's, unchanged by contract. Its own cost
  comes from the virtual bus calls, one per memory access: they cannot be removed without changing the
  core's interface or turning LTO on, which goes beyond a small and reversible change. It is now the
  largest item after the DSP.
- **SIMD**: the DSP's loop is a dependency chain (`ACC` feeds on itself through `BSEL`), it does not
  vectorise.
- **Replacing MAME's SCSP** with a standalone implementation: the code has not been started, but **both
  prerequisites now exist**. The differential harness: `tools/sound/compare-sound.cmd`. And the contract,
  in two pieces that do not say the same thing:
  - `docs/scsp_architecture.md` — **the C++ interface** between `SoundBoard` and the chip, and what it
    drags along from MAME;
  - `src/sound/scsp_contract.h` — **the hardware contract**: what the driver expects of the chip, field by
    field, with every number's provenance and the owned departures marked as such. Written on 2026-10-25,
    and **checked** by `tools/sound/test_scsp_contract.cpp` against `scsp.cpp` and against the ROM.
- ~~**The measurements under `--rec`**~~: **made on 2026-10-25**, see §4 bis. The sound's share goes from
  9.6% to **30.5%**, the board being 4.3 times faster and the sound unchanged. **And in the host, measured
  on 2026-10-26** (§4 ter): the sound thread works **1.49 ms per frame** and **the game never waits for
  it** — 0.000 ms, maximum 0.016 never exceeded. That is **8.5% of one core and 0.00% of the frame**.
