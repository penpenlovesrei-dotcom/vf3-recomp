# The sound — the front door

**This file explains nothing. It says where everything is.** The sound subsystem sits in five places in the
repository and nothing named them together; it took measuring the footprint, on 2026-10-25, to notice:

| | lines | |
|---|---|---|
| `src/sound/` | 6 239 | of which **2 110 are MAME's** (`scsp.cpp`, `scsp.h`), that is a third |
| `tools/sound/` and `tools/ghidra/FV2Son*` | 5 963 | the measuring and reading toolkit — the ten tests are part of it |
| the nine documents below | 5 459 | **of the same order as all the code** |
| **in all** | **17 661** | 3.0% of the repository, but **18.7%** excluding `third_party/` |

(Recounted on **2026-10-28**. The table of 2026-10-25 gave 10 811 lines in all: it was written before the
project's core, its DSP, the seven block tests and the replayer, which are most of the difference. And
before that a first version gave 3 130 lines of tooling because it counted all of `tools/ghidra/`,
PowerPC scripts included. This table's figures age fast: the command that redoes them is
`grep -c '' src/sound/* tools/sound/* tools/ghidra/FV2Son*.java docs/*.md`.)

The documentation was larger than the code for a long time, and it remains of the same order: the bulk of
this work is **knowledge**, and knowledge that cannot be found again is lost. Hence this file.

---

## 1. Where to start, depending on what you came for

| I want to… | read first |
|---|---|
| understand what the sound costs and where | `scsp_audit.md` §4 and §4 bis; and what **the project's core** costs extra, §4 quinquies (on the bench) then §4 sexies (in the game) |
| know what the driver expects of the chip | **`src/sound/scsp_contract.h`** — the hardware contract, checked by a test |
| know how an implementation plugs in | `scsp_architecture.md` — the C++ interface |
| replace MAME's SCSP | `scsp_migration_plan.md`, which lists the prerequisites and says which exist |
| understand what the 68000 program does | `fv2-son-programme.md` — 3 062 lines, the 168 functions read |
| know whether a measurement is reliable | `scsp_validation.md`, and the provenance noted beside every figure |
| not repeat a mistake already made | `fv2-son-programme.md` keeps the false trails **beside** the measurements that corrected them |

## 2. The code

| | |
|---|---|
| `src/sound/soundboard.h`, `.cpp` | the sound board: one 68000, two SCSPs, the banked sample ROM. **This is where you come in** |
| `src/sound/scsp.h`, `.cpp` | **the chip, and it is MAME's code** (`license:BSD-3-Clause`, ElSemi and R. Belmont), modified. The repository is MIT: the licence is compatible, the boundary is here |
| `src/sound/scspdsp.h`, `.cpp` | the effects DSP, rewritten by this port — the two bodies `Run<0>` and `Run<USES_ALL>`, and the measured gain of −24% |
| `src/sound/scsp_core.h` | **the new interface** of an SCSP core: three `sink` methods, time pushed, the sample as the unit |
| `src/sound/scsp_mame.h`, `.cpp` | **the adapter** that presents MAME's core through it — step 3 of the migration plan, and the three places where the two do not line up are named in it |
| `src/sound/scsp_contract.h` | **the hardware contract**: 48 fields and the driver's constants, every number with its provenance, the owned departures marked `DEVIATION` |
| `src/sound/scsp_own.h`, `.cpp` | **the project's SCSP**, the seven blocks. Produces the same sound as MAME's **byte for byte** over 3 219 865 samples per chip. Its header says each block's state and what remains unvalidated by measurement |
| `src/sound/dsp_own.h`, `.cpp` | **the project's effects DSP**: block 5, written against the measured specification because its instruction set exists nowhere written down. Its header names the **four fields the game never exercises**, hence never tested |
| `src/sound/scsp_select.h`, `.cpp` | **the selector**: `scsp::make(name, ...)`, the door a second core comes in through. (On 2026-10-25, when this line was written, `mame` was its only entry and the file said so; since 2026-10-28 each core registers itself in its own `.cpp`, so what is linked decides what exists.) |
| `src/sound/scsp_trace.h`, `.cpp` | **the trace format, the recorder and the replayer**: every access to the sample, RAM writes included. The recording goes through a single door, `SoundBoard::head[n]`, and not through the eight call sites |
| `src/sound/sound_prof.h` | the per-part profiling (`VF3_SND_PROF=1`). The scopes are in `soundboard.cpp`, `scsp.cpp` **and** `scsp_own.cpp`: the four ways this wiring stayed silent are in `scsp_audit.md` §4 sexies |

## 3. The tools

| | |
|---|---|
| `tools/sound/extraire-son.py` | extracts the 68000's ROM from a `.zip` **and swaps the bytes** — without which nothing disassembles. It is the prerequisite for all the rest |
| `tools/sound/desas_son.py` | **searches** the 512 KB: `--cherche`, `--at`, `--fonctions`, `--carte`. It seeds the eleven computed jumps, without which it sees only half the program |
| `tools/sound/import-sound.cmd` | imports the ROM into Ghidra, with no interface |
| `tools/ghidra/FV2SonSetup.java` | seeds the entry points **before** the analysis (as `-preScript`) |
| `tools/ghidra/FV2SonTablesDeSaut.java` | opens the four jump tables — 41 functions to 90 |
| `tools/ghidra/FV2SonSautsIndirects.java` | resolves the last five `jmp (An)` |
| `tools/ghidra/FV2SonDonnees.java` | declares the data, and **recounts** what is left outside |
| `tools/ghidra/FV2SonListe.java` | lists the functions and their ranges, changing nothing — the control for the previous three |
| `tools/sound/dsp-variants.py`, `build-variants.py` | **the DSP variants to be judged**, and one exe per variant: each applied, built, put through the ten tests, kept only if all 24 files are green. An unverified variant that wins is worse than one that loses |
| `tools/sound/measure-variants.cmd` | **the judging on the hardware counter**: one capture of instructions retired per variant. Needs an **administrator** window, and checks for it before entering the loop |
| `tools/sound/compare-sound.cmd` | **the differential harness**: two WAVs, the same scene, compared byte for byte |
| `tools/sound/audibilite.py` | if two WAVs differ, **is it audible**: Bark bands, masking, absolute threshold, ambient floor |
| `tools/sound/compare-madrs.cmd`, `mesurer-madrs.py` | the special case of the MADRS mirror |

## 4. The probes, and where each one lives

None is on by default. **Every measurement in these documents names the probe and the scene that produced
it**; that is this subsystem's rule, and it has just been caught out several times.

| probe | what it gives | in |
|---|---|---|
| `VF3_SND_PROF=1` | the sound's cost by part: loop, 68000, voices, DSP, mix — and for **both cores** since 2026-10-28. Under `vf3run` it needs **`--wav`**, without which the sound board does not run and the probe counts nothing; it says so. `VF3_PROF` is no longer required | `sound_prof.h` |
| `VF3_PROF=1` | the cost per board, every 500 frames (`prof …: board … sound …`) | `tools/vf3run.cpp` |
| `VF3_SND_HOST=N` | **what the sound costs the host**: the thread's work, and above all **the game's wait** — this is not `vf3run`'s figure | `src/host/main.cpp` |
| `VF3_SCSP_DSPLOG=f` | **every write** to the DSP's registers, to the FM buffer, to word `0x6` and to a slot's `SA` words | `soundboard.cpp` |
| `VF3_FICHES=f` | the live voice records, with channel and category — what settled "which category is the music" | `tools/vf3run.cpp` |
| `VF3_STAGE=n` | **forces the arena** (1-12), holding both of the game's levers | `tools/vf3run.cpp` |
| `VF3_VOLUMES=m,e` | the music and effects volumes, separately | `tools/vf3run.cpp` |
| `VF3_SCSP_WLOG=f` | the register-write trail, to READ: frame by frame, chip 0 only. Not enough to replay — see `VF3_SCSP_TRACE` | `soundboard.cpp` |
| `VF3_SCSP_CORE=n` | **which SCSP core the board uses**: `mame` (the default) or `own`, the project's SCSP. The per-group gains, the energy probe and the key-on log apply to the first only, and the board says so at start-up | `soundboard.cpp` |
| `VF3_SCSP_TRACE=f` | **the replayable trace**: `f.0` and `f.1`, every access to the sample, RAM writes included, plus `f.N.pcm` as a reference | `scsp_trace.h` |
| `VF3_SCSP_RESTE=N` | what the two chips leave in the buffers, every N frames — a remainder that **grows** would be the sign of a desynchronisation | `soundboard.cpp` |
| `VF3_KEYON_LOG=1` | the note key-ons | `soundboard.cpp` |
| `VF3_SCSP_SOLO=n` | listen to one slot only | `soundboard.cpp` |
| `VF3_SCSP_MADRS_MIRROR`, `VF3_SCSP_MADRS_SEPARATE` | force one side or the other of the `0x7C0-0x7FF` question | `soundboard.cpp` |
| `VF3_SCSP_MSLC_LATCH` | `MSLC`'s latch | `scsp.h` |
| `VF3_DUMP_SOUND_RAM=f`, `VF3_MIDI_LOG`, `VF3_MIDI_SEND` | the sound RAM, the MIDI received, the MIDI injected | `tools/vf3run.cpp` |
| `VF3_ENERGY_LOG`, `VF3_TIMER_LOG`, `VF3_MENU_SOUND` | on the host's side | `src/host/main.cpp` |

## 5. The tests

`make test` builds and runs all ten, and for the most part they need neither a ROM nor a window:

| | |
|---|---|
| `tools/sound/replay.cpp`, `replay.cmd` | **the replayer**: a trace of accesses replayed through one or two cores, and the first divergence located to the sample — PCM, interrupts, timer arms, register reads, outgoing MIDI. The `.cmd` takes the trace and runs both checks in one command |
| `tools/sound/measure-core.cpp` | **what a core costs, item by item**: idle, the slots alone, the DSP alone, both. The replayer compares byte for byte but its time is dominated by parsing the trace — this one times `run()` only. It is what said the cost was in **the slots and not the DSP**. And `--dsp` goes one notch down: **the DSP by characteristic**, both cores side by side, baseline subtracted — it is what said the gap is **no instruction but the baseline** (`scsp_audit.md` §4 septies). `--baseline` measures the baseline only, seven passes: fast enough to interleave a reference and a variant of the same file |
| `tools/sound/scsp_bench.h` | the rig the block tests share: a fresh core, its RAM, the keying of a slot through the contract's table. **No expectation** lives in it — what is expected of a block belongs to that block's test. And **no concrete class** since 2026-10-28: it linked MAME's core into seven tests for the sake of two switches, now moved to `scsp_select.h` (`make test-without-mame`) |
| `tools/sound/test_scsp_dsp.cpp` | **the DSP's instruction set**, and it is a **specification** before it is a test: that instruction set exists nowhere written down. **Thirteen cases**, observed through the register window and through RAM. It brought out the shifter's step of delay. And since 2026-10-28 it is the **only judge** of `FRCL`, `ADRL`, `YRL` and `NEGB`, which no game served sets (cases 9 to 13, four faults injected so that it knows how to fail). Block 5 |
| `tools/sound/test_scspdsp.cpp` | the DSP's two bodies against a reference that decodes in the loop. 16 cases, including the four controls no game sets |
| `tools/sound/test_scsp_core.cpp` | **MAME driven only through the new interface**, registers written through the contract's table: a wrong field there cuts the sound |
| `tools/sound/test_scsp_contract.cpp` | **the contract against what it describes**: 48 fields against `scsp.cpp`'s macros, the constants against the ROM. 55 checks, and **a count that must close** so that no field is silently skipped |
| `tools/sound/test_scsp_regs.cpp` | **what a write to a register reads back as**, word by word from 0x000 to 0x42F: two masks (what goes up, what stays raised), the three words whose bit 5 sticks by three different paths, and the disputed range 0x7C0-0x7FF measured in **both** settings of the MADRS mirror. 9 cases. Block 1 of the architecture |
| `tools/sound/test_scsp_slots.cpp` | **the oscillator, PCM addressing and the four loop modes**, measured to the exact sample: `SDIR` sets the envelope aside, so the output is the raw sample and the sequence of **indices** reads. 11 cases. Block 2 of the architecture |
| `tools/sound/test_scsp_adsr.cpp` | **the ADSR envelope**: a steady wave makes the output *be* the envelope, and its multiplier reads back with no rounding. The **attack durations in samples** encode MAME's time tables there, so that a formula in their place does not go unnoticed. 10 cases. Block 3 |
| `tools/sound/test_scsp_lfo.cpp` | **the step, the two LFOs and ring modulation**. A ramp makes the output the **phase accumulator**, so the step is read instead of counted. Pins down the port's **two deliberate divergences** (`PLFOS` and `MDL` lose their low bit) so that nobody "corrects" them. 11 cases. Block 4 |
| `tools/sound/test_scsp_irq.cpp` | **the three timers, the five interrupt lines and the DMA**, measured through the **sink** and not through the sound: integers are compared, not gains. Pins down `timer_samples`'s `+1`, the third deliberate divergence. 11 cases. Block 6 |
| `tools/sound/test_scsp_mix.cpp` | **the mixing and the output**: the **three decibel tables the Sega way** (`TL`, `DIPAN`, `SDLT`) read in full and on **both** channels, plus a check that cross-validates them against each other with no recorded value. 9 cases. Block 7 |

The second part of the contract's test stands down, **with a visible line**, when `build/fv2-son-68000.bin`
is not there — it comes from a ROM the repository does not carry.

## 6. The nine documents

| | lines | |
|---|---|---|
| `scsp_audit.md` | 628 | what there is, what it costs, and where the time goes |
| `scsp_benchmark_baseline.md` | 175 | the baseline, and every batch measured against it |
| `scsp_architecture.md` | 215 | the C++ interface of a standalone core |
| `scsp_migration_plan.md` | 231 | the plan for getting out of MAME's SCSP, and what is done |
| `scsp_validation.md` | 199 | what validates what, and what is missing — including **how far the trace is complete** |
| `scsp_references.md` | 162 | the sources read, and the rule: **never consensus between emulators as proof** |
| `fv2-son-programme.md` | 3 062 | the 68000 program, read — the 168 functions, the RAM map, the mistakes kept |
| `fv2-console-son.md` | 643 | the sound of the mode list |
| `sound.md` | 144 | this file |

## 7. Why this was not a separate repository

The question was asked on 2026-10-25 and settled **no, not yet**, for one reason and one only: **the proof
lives in the game.** The figures in these documents — the sound's share under `--rec`, the three arenas
that load their own DSP program, the 993 FM-buffer writes all guarded by `STWINH`, the 1 807 voice
allocations that settled the music/effects split — all come out of `vf3run` driving the real game with the
real ROMs. A separate repository would carry the code and the documents **without being able to redo or
recheck a single one of those numbers**. The contract's test reads `scsp.cpp` on one side and the driver's
ROM on the other; the differential harness compares *this* port's output on *these* games.

And there was still nothing standalone to extract: the chip was still MAME's.

**The moment to separate is named**: when the standalone core exists and passes the differential harness.
Then it will no longer depend on the game, the licence boundary will be clean, and separating will cost
nothing.

**Where that moment stood on 2026-10-28.** The core exists and passes the harness — byte for byte over the
whole scene, and end to end in the game. And the last prerequisite fell the same day: the seven block tests
were linking MAME's core's 2 110 lines **through `tools/sound/scsp_bench.h`**, for the sake of two
three-line switches. Each core now registers itself in its own `.cpp` (`src/sound/scsp_select.h`), so
**what is linked decides what exists**, and `make test-without-mame` builds those seven tests with the
project's core alone — `scsp_select.o`, `scsp_own.o`, `dsp_own.o`, and nothing else. It is no longer an
intention: it is a target, which `make test` redoes every time.

**AND THE SEPARATION HAPPENED ON 2026-10-29**, which is why this section is written in the past tense from
here on. The subsystem lives under the prefix `scsp/` as a `git subtree`, and that subtree is published as
a repository of its own; a fresh clone of it builds and passes its seven block tests without a line of
MAME or anything from the port. What this section argued has not been refuted, and the standalone
repository's README says it in the same words: **the proof still lives in the game.** That repository
carries the code and the documents and cannot redo its own numbers. Three tests did not go with it --
`test_scsp_core`, `test_scspdsp` and `test_scsp_contract` have MAME's core for their subject -- and the
contract is still checked against the driver's ROM, here.
