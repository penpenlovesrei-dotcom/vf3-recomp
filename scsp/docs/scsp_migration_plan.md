# Getting out of MAME's SCSP — the plan, and what is already done

> This document is one of the nine on the sound. The front door, which says where everything is:
> [docs/sound.md](sound.md).

**This is a plan.** As of 2026-10-27, steps 0, 2 and 3 are done. **The project's SCSP exists** —
`src/sound/scsp_own.{h,cpp}` — and it produces the same sound as MAME's **byte for byte** over a real
scene. **The seven blocks are written**, and the last one — the effects DSP — was written against a
specification measured first (`tools/sound/test_scsp_dsp.cpp`), for want of the slightest description of
its instruction set. So the project's core no longer contains **a line of MAME**.

**On the replayer, over the whole scene of `fv2-fight.txt` (4200 frames): IDENTICAL.** 3 219 865 samples
per chip, 208 360 interrupts, 69 311 timer arms, 265 469 register reads — everything agrees, sample by
sample, on both chips.

**AND END TO END, THE WHOLE GAME: THE SAME FOOTPRINT.** `vf3run` over the 4200 frames, once with each
core, gives the same WAV — `9A2310F8AF000E09EB18BD114852B003`, which is **exactly the reference taken
before all this work** (step 3's commit). The replayer judged the chip over a frozen sequence; this judges
it in place, with the 68000 answering it and the board's mixer behind.

**The board can now run on either**: `VF3_SCSP_CORE=mame` (the default) or `own`. Two of the four extras
disappeared along the way — `MVOL` is **read from the register**, the chip not applying it itself, and
`reg_word` was only a `read_reg` in disguise. The **MADRS mirror** is carried by both cores, each on its
own concrete class, because it belongs to the game and not to the chip.

What is still owed, and it is named in `soundboard.h`: **the per-group gains** (the menu's music/effects
split), **the energy probe** and **the key-on log** live only in MAME's core. The last two are
diagnostics; the first matters to the player, but it is **inert as long as both volumes are 1**. That is
why the project's core is usable without them, and why the default stays `mame`: nothing changes unless it
is asked for.


## Why, and in what order the four objectives serve each other

The specification ranks them D > A > B > C: processor, then getting out of MAME, then fidelity, then the
C++. But they are not independent, and how has to be said:

- **D (processor) does not require A.** Demonstrated: the 24% won on 2026-10-24 was won **inside** MAME's
  code, without changing anything it produces. Two thirds of the cost was in the DSP, and the DSP is won
  through the code's shape, not through the code's ownership.
- **A (getting out of MAME) puts B (fidelity) at stake**, and that is the project's real cost. Today,
  MAME's code **is** this port's fidelity reference: every documented departure (`docs/scsp_audit.md` §2)
  was measured against it, game by game. A new core only keeps that fidelity if it is compared sample by
  sample with that one — so A cannot move forward without the old one staying available.
- **The licence is not pressing.** MAME is BSD-3-Clause, compatible with this repository's MIT, and the
  obligations are met (`docs/scsp_references.md`). Nothing compels getting out; the point is mastery, not
  compliance.

The consequence: **A is done block by block, behind a selector, and the last block removed is the riskiest
one**, not whichever comes first.


## Step 0 — done on 2026-10-24

| | |
|---|---|
| measure | `src/sound/sound_prof.h`: where the sound board's time goes, checked to have no effect on what it produces |
| map | `docs/scsp_audit.md`: parts, provenance, licences, interfaces, hot paths |
| source | `docs/scsp_references.md`: what was read at the source, and the warning about the emulators' lack of independence |
| the harness | `tools/sound/compare-sound.cmd`: the sound against a reference commit, byte by byte |
| win | three batches on `scspdsp`, **−24.3%** on the sound board, byte-identical WAV on both games |
| the batch undone | a fourth, on the voices, that gained nothing: undone, and the negative result recorded |
| test | `tools/sound/test_scspdsp.cpp` (`make test`): the DSP's two bodies against a reference that decodes in the loop, sixteen cases, and four faults injected to show it knows how to fail |

What step 0 had not done has been done since, and it should be read before what follows. It left one block
test out of seven; **all seven have one** now, the last six written on 2026-10-26: the registers (1), the
slots (2), the envelope (3), the LFOs (4), the DSP (5), the timers and the interrupts (6) and the mixing
(7). What is still missing: **one measurement scene only**, the start-up and the attract.

**The three measurements that were missing are done**:

| where | the sound's share | what it says |
|---|---|---|
| `vf3run --rec`, the board inside the frame loop | **30.5% of the frame** (§4 bis, 2026-10-25) | the Model 3 board is 4.3 times faster there and the sound itself does not change. That is where its margin counts |
| the host, the board on its thread, desktop machine | **8.5% of one core, 0.00% of the frame** (§4 ter, 2026-10-26) | the game never waits for it |
| the host, **two-core AMD laptop** | **17.4% of one core, 0.00% of the frame** (§4 quater, 2026-10-26) | twice as dear, and the wait stays zero. The rate there is held by the photo's uploads, not by the sound |

The DSP is half the sound in all three. What remains unmeasured: **concurrency** on two cores — the probe
counts the blocking wait, not the cost of sharing the cache and the scheduler; it would take the same
scene with the sound off. And still one scene only, the start-up and the attract.


## Step 1 — the rest of the processor gain, if any is needed

In the order the measurement gives, after step 0: DSP 52-60%, 68000 28-31%, voices 7-17%.

1. **The DSP, a form compiled per program.** The program is fixed between two writes of MPRO: 112 steps
   replayed 44 100 times a second. The decoded form (`Inst`) and the grouped controls (`Inst::f`) are
   already beyond MAME's and Supermodel's, which decode in the loop. What follows is to emit, on every
   `Start`, a sequence of operations with no decoding branch — not a JIT, a list of micro-operations chosen
   per step. To be measured before believing in it: the floor is `ACC`'s dependency chain, which does not
   vectorise.
2. **The 68000**, and that is the wall: its own cost comes from the virtual bus calls, one per memory
   access, and its handlers are MAME's by contract. Two honest routes, neither of them small: turn LTO on
   over the whole thing, or give the core a non-virtual bus interface (which modifies it, hence leaves the
   "unchanged" contract).
3. **The voices**: batch 4 showed it is not the address loop. What remain are the pan tables (two
   `int[0x10000]`, 256 KB each), the envelope and the LFO. **Not measured in detail.**

None of those three is necessary: at 1.40 ms per frame on a budget of 17.4, and on a separate thread in
the host, the sound board is no longer a problem. Step 1 is to be done **if** somebody needs it, not
because it is there.


## Step 2 — the rewrite's prerequisites

Nothing is written before those three, and they are the bulk of the work:

0. **The hardware contract**, which was not in this list and should have been: what the chip must do, as
   opposed to how it is wired up. Written on 2026-10-25 — `src/sound/scsp_contract.h`, 48 fields and the
   driver's constants, every number carrying its provenance and the owned departures marked as such — and
   **checked** by `tools/sound/test_scsp_contract.cpp`: 55 checks against `src/sound/scsp.cpp` and against
   the driver's ROM, with a count that must close so that no field is silently skipped.
1. **The per-block tests** of `docs/scsp_validation.md` §3b, written before the blocks they test. The
   first of the seven has existed since 2026-10-24 (`make test`: the DSP, its two bodies and the four
   controls the games never set) — ~~the other six not~~ **and the other six since 2026-10-26**:
   registers, slots, envelope, LFO, timers and interrupts, mixing. **The contract above is tested too**, by
   the same `make test`, which now chains nine tests.
2. ~~**The selector and the replayer**~~ — **done on 2026-10-26.**
   - `src/sound/scsp_select.h`: `scsp::make(name, ...)`, the door a second core will come in through.
     Today `mame` is the only entry, and the file says so rather than pretending.
   - `src/sound/scsp_trace.h`: the trace format, the recorder and the replayer. **The format is new, and
     `VF3_SCSP_WLOG` could not serve as a starting point**: it records the frame and not the sample — hence
     the instant of a write within the frame, which is what decides where the interrupts fall — and it
     leaves out chip 1, the reads, incoming MIDI, the timers that fire and **the writes to sound RAM**,
     without which a replay has no sample to play at all.
   - `tools/sound/replay.cpp` and `replay.cmd`: the replay and the comparison of the PCM, the interrupts,
     the timer arms, the register reads and the outgoing MIDI — the first divergence, located to the
     sample.
   - The recording goes through **a single door**, `SoundBoard::head[n]`: a decorator of `scsp::core`
     rather than a probe placed at the eight call sites, so that a ninth added one day cannot escape the
     trace in silence.

   **What was checked**, and in this order, because a check is worth nothing without the one before it:
   | | |
   |---|---|
   | the trace is **complete, over the whole scene** | the replay finds the PCM the board produced, **byte-identical over 3 219 865 samples per chip** (4 200 frames), for both chips. That is what establishes that no state comes in through a door the trace does not record. **Established first over 300 frames only, and that was too short**: over the whole scene this check failed (samples 772 067 and 1 149 006), because the **MADRS mirror** was being set outside the interface and so did not go into the trace. Found, named and closed on 2026-10-28 — trace header version 2, single door in `scsp_select.h`; `docs/scsp_validation.md` §3 c carries the whole measurement |
   | the comparator **knows how to fail** | one sample changed in the reference, and it names it: "sample 100000 right" |
   | `mame` against `mame` | identical on all five streams |
   | the board has **not changed** | `vf3run` 1200 frames against the build of `9753fde`: **the same MD5 footprint**. The detour through `head[n]` costs the sound nothing |
3. ~~**The interface** of `docs/scsp_architecture.md` §2, laid **on MAME's core first**~~ — **done on
   2026-10-26.** `src/sound/scsp_core.h` is the interface as §2 had drawn it (three `sink` methods, time
   **pushed** by `run(n, out)`, the sample as the unit, no allocation); `src/sound/scsp_mame.{h,cpp}` is
   the adapter that presents the old core through it; and `tools/sound/test_scsp_core.cpp` is what decides
   that the contract holds — it drives MAME **only** through `scsp::core`, and **writes its registers
   through the contract's field table**, so that a wrong field there cuts the sound instead of waiting to
   be found in a game.

   **The three places where the old core and the new shape do not line up** are named in `scsp_mame.h`
   rather than papered over: time is **pulled** in the old one (the adapter answers the question "what time
   is it" itself), the output is in floats (the conversion back is **exact**, the divisors being powers of
   two), and **the MIDI output is in bits where the contract asks for bytes** — the adapter does not bridge
   it, it **counts the bits** so that the gap is visible.

   **And the board is wired onto it**, on 2026-10-26 as well: `soundboard.cpp` no longer speaks to the
   chips except through the interface — registers, time, timers, MIDI — and reaches the old class, through
   `->device()`, only for what is not the chip's business (the per-group gains, the energy probe, the MADRS
   mirror). Those `device()` calls are what must go last.

   **On the harness: BYTE-IDENTICAL**, on `fv2-fight.txt`, 4 200 frames, `--rec`, and on **three arenas**
   (2, 7 and 9) — same lengths, same MD5 footprints. Arena 9's is `9a2310f8af00…`, that is to say
   **exactly the reference taken that same morning before all this work**: it is also the arena the game
   picks by itself, which closes the loop.

   **Three things the harness found and that reasoning alone would not have given:**

   - **§2 was wrong about the interrupts.** It proposed `irq(int level)`, 0 for nothing, "not one line at
     a time". But the chip puts out **four distinct lines** independently and can put one out while
     another is raised: a single number cannot say which one dropped. So `scsp_core.h` carries
     `irq(level, on)`, and says why. **That is exactly what step 3 exists to discover.**
   - **draining the old core's buffer entirely was losing samples.** `run()` copied `2n` values then
     called `clear()`: everything the old core had made beyond that went. The harness saw it — one pair
     lost at frame 1006, and all the sound after it identical **once shifted by one sample**. One `clear()`
     too many makes a faithful core look unfaithful.
   - **and a hypothesis of my own, wrong, ruled out by measurement**: I had believed that bringing *both*
     chips up to date on every access (instead of the one being touched) explained the gap. Corrected, the
     footprint did not move by a bit. The correction is right — the old path only advanced one chip — but
     **it explained nothing**, and it is the measurement that said so.

   - **and the missing pair was a counting convention.** The old core's `update()` does
     `n = idx - m_written + 1`: it makes the samples with index `m_written` **up to and including `idx`**,
     so at clock `idx` it has made `idx + 1` of them. The new interface says "`run(n)` makes n samples",
     which is cleaner — but the adapter had to absorb the difference. It does so by reporting an index
     **one behind** its counter, and the board aims at `target + 1`. Without that the game's very first
     sample was never made: **one pair in 3 219 865**, which neither the code nor the ear would have
     shown, and which the harness found in one comparison.

   Left open from this step: the MIDI receiver's and transmitter's clocks lose cycle precision going
   through an interface counted in samples (`soundboard.cpp`, `Sink::timer`). It is not audible on this
   scene — the footprint is identical — but it remains a loss of precision written into the code rather
   than corrected.

The order mattered: (3) before (2), because a replayer written against the old interface would have had to
be redone. It was respected — the interface on the morning of 2026-10-26, the replayer in the evening, and
the replayer knows nothing but `scsp::core`.

**So step 2 is closed, and nothing blocks step 3 any more.**


## Step 3 — the blocks, one by one

The partition and the order are in `docs/scsp_architecture.md` §3. For each block, the same loop:

```
write the test  ->  write the block  ->  the harness says "identical"  ->  measure the cost  ->  keep it
                                      \-> otherwise: understand the gap, do not widen the tolerance
```

And one rule about the order: **begin with the blocks whose fidelity is checkable without the game**
(registers, PCM, PACK/UNPACK, the DSP's shifter), finish with those whose only judge is the game itself
(the envelopes' time tables, the modulation depth, the MADRS mirror). Those last ones carry this port's
measured departures, and they are the ones that can break the sound without a test seeing it.


## Acceptance criteria

Taken from the specification, and made checkable here:

| | criterion | how it is seen |
|---|---|---|
| 1 | it compiles in the configurations served | `make build/bin/vf3run build/bin/vf3lock build/bin/vf3` with no error |
| 2 | the existing tests pass, or the difference is explained and isolated | `verify-vf3.cmd` says IDENTICAL everywhere |
| 3 | the reference sound scenes can be redone | `tools/sound/compare-sound.cmd` on both games |
| 4 | the audio differences are measured, classified, documented | for an optimisation: **no difference**, byte-identical WAV. For a new core: the list of differences, each with its measurement (max error, RMS, correlation, spectrum) and its cause |
| 5 | the processor cost is measured against the baseline | `VF3_PROF=1`, the same scene, the same machine, and the breakdown with `VF3_SND_PROF=1` |
| 6 | no gain announced without reproducible figures | the commands are in `docs/scsp_benchmark_baseline.md` |
| 7 | the dependencies and the licences are documented | `docs/scsp_audit.md` §2, `docs/scsp_references.md`, `third_party/README.md` |
| 8 | the fallback stays usable during the validation | step 2's selector, and the old core behind it. Since 2026-10-28 **each core registers itself in its own `.cpp`**: the selector no longer names any concrete class, so what is linked decides what exists, and `make test-without-mame` builds the seven block tests **without a line of MAME's core** — the last prerequisite for a separate repository |

**State as of 2026-10-28:** **all eight are satisfied.** 1 ✔. **2 ✔, run that day** after all the sound
work: `verify-vf3.cmd 3000` says **IDENTICAL** on all four checks — the PowerPC's state frame by frame,
the 8 MB of RAM, the frames rendered, the whole host — and the step-by-step interpreter against recompiled
code agrees over 2 231 frames. 3 ✔ on both games. 4 ✔: **no difference**, the footprint
`9A2310F8AF000E09EB18BD114852B003` held by both cores over the 4 200 frames. 5 ✔, and two notches finer
since (`scsp_audit.md` §4 sexies to §4 octies). 6 ✔. 7 ✔. 8 not applicable: nothing has been replaced, and
the selector now makes it possible **not to link** the old core at all (`make test-without-mame`).

(The state of 2026-10-24 said "2 not rechecked since the batches — to be run anyway". It is done, and it
needed to be: a check put off is not a check.)
