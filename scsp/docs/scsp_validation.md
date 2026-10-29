# Validating the sound — what is in place, and what is missing

> This document is one of the nine on the sound. The front door, which says where everything is:
> [docs/sound.md](sound.md).

State as of 2026-10-24. The "in place" part has been run and checked; the "missing" part is a list of work,
not a promise kept.


## 1. The rule

**For an optimisation, exact identity is the only acceptable answer.** An optimisation that changes the
sound is not an optimisation, it is a change to the port, and it is then judged on fidelity and not on
speed. There is no tolerance to widen: if the WAV moves by one sample, we look for why.

The looser measurements (max error, RMS, correlation, spectrum, tempo drift, transients) have their place
elsewhere — when two *implementations* are compared from which identity is not expected, or the port
against another emulation. They never replace the strict comparison where that one applies.


## 2. What is in place, and what it covers

### The sound, byte by byte, against a reference commit

`tools/sound/compare-sound.cmd <game> <script> <frames> [commit]`

Builds (once) a worktree of the reference commit in `C:\Temp\fv2-son-ref`, makes both `vf3run`s play the
same scene, and compares the two WAVs with `fc /b`.

On 2026-10-24, each of the four batches was compared byte for byte (`cmp`) against a reference WAV produced
by the build from before the batches, over 4200 frames of `tools/scripts/fv2-fight.txt` on `fvipers2.zip`
(12 879 504 bytes every time): **identical**, for all four batches. Then the script itself was run, against
a build of commit `335c050`, on **both** games the port serves:
`fvipers2`/`fv2-fight.txt`/4200 frames and `vf3`/`vf3-match.txt`/2400 frames — **byte-identical** on both.

What that covers, and it is a lot: the whole real path — 68000, the game's sound driver, 32 voices × 2
chips, envelopes, LFO, modulation, DSP, timers, interrupts, both SCSPs and the mixing — over a sequence the
game itself produces. One sample out of place and it shows.

What it does not cover: the edge cases *this game, this time* does not go through. A reverse loop, an
`EGHOLD`, a DMA, a reserved value: if Fighting Vipers 2's sound driver never asks for them, the harness
does not look at them.

### The DSP's two bodies, against each other and against a reference

`make test` → `build/bin/test_scspdsp` (`tools/sound/test_scspdsp.cpp`). No ROM, no game RAM, no window:
the programs are generated from fixed seeds, so two runs do the same thing.

The test carries a **naive reference** of the DSP: it decodes MPRO's bit fields **inside** the loop, as
MAME `master` and Supermodel still do, with no decoded table, no grouped controls, no template and no local
copies of `DEC`/`RBL`/`RBP`. So it tests exactly the machinery this port added. Both sides receive the same
program, the same tables, the same RAM, the same `MIXS` on every sample, and after **each** of the 64
samples `EFREG`, `TEMP`, `MEMS`, `DEC` and the **524 288 words** of the delay line are compared.

The sixteen cases, and what each one catches:

| case | what it exercises |
|---|---|
| two programs with none of the four | `Run<0>`, the body both games take |
| the same, `uses` forced to `USES_ALL` | **the two bodies must agree on the same program** |
| all four together, 48 then 112 steps | `Run<USES_ALL>` on what nobody was playing |
| `FRCL`, `ADRL`, `YRL`, `NEGB` alone | each one isolated, and `uses` is exactly its bit |
| `RBL` 16k/64k, `RBP` 1/3 | the delay line's addressing outside the default case |
| `IRA` out of range at step 20, then at step 0 | the whole step giving up: neither `DEC` nor `MIXS` moves |
| "the four bite" | a guard: the four controls **do change** the delay RAM (296 words), so the generator really did set them and the case is not secretly the one next door |
| `Decode` adds a bit | a step rewritten with no `Start`: `uses` gains the bit and the generic body takes over |
| `Start` recomputes `uses` | on a reused object, `uses` **goes back down** from 15 to 0 — the one assertion in `scspdsp.h` that nothing was holding |

**And the test has shown it knows how to fail**, which is the only thing that makes a green test
interesting. Four faults injected and then undone:

| fault injected | case that failed |
|---|---|
| `NEGB` no longer negates, in the generic body | 5 |
| the loop reads `TWT` instead of `XSEL` | 12 |
| `SHIFT` read with one shift too many | 13 |
| `Start` does not reset `uses` | 1 (the one written for it) |

A fifth, swapping two values of the `F_XSEL`/`F_TWT` enumeration, **made nothing fail** — and rightly so:
swapping two labels changes the write and the read together, which is a rename, not a fault. Noted because
a mutation that mutates nothing is not proof that the test is blind.

What this test does not say: it does not rederive the chip's behaviour from hardware documentation, and
`PACK`/`UNPACK` are **transcribed** in it rather than re-reasoned (this port does not touch them). An
IDENTICAL here means "the port still computes what the simple loop computes", which is the question asked,
and not "the hardware does that".

### The same hole, from the other side — and plugged on 2026-10-28

Everything above concerns MAME's DSP and its **two bodies**. The project's DSP, `src/sound/dsp_own`, had
exactly the same hole, in the same place, and for the same reason: it carries `FRCL`, `ADRL`, `YRL` and
`NEGB` **written**, its second body only runs when one of the four is set, and **no game sets them**.
Measured that day on every set the port knows: 152 program writes, five distinct shapes, three games
including Sega Rally 2 with an altogether different program, and not one step (`docs/scsp_audit.md` §5). So
there was nothing to go and find: the replayer could not be their judge.

**`tools/sound/test_scsp_dsp.cpp` cases 9 to 13** measure them, each by the only read path that exists —
those registers have no window:

| case | what makes it observable | reading |
|---|---|---|
| `NEGB` | B's sign on a hold step, the same program on both sides | `+2330` / `−2330` |
| `FRCL` | `FRC_REG` read back **as Y** (`YSEL=0`), multiplied again by the same X | `331` at `SHIFT=0`, `2912` at `SHIFT=3` |
| `YRL` | `Y_REG` read back through `YSEL=2` then `YSEL=3`: two windows of the same register | `662` / `946` |
| `ADRL` | the address `ADREB` offsets: a different control at word `0x12` of the delay line | `MEMS[1] = 0x567800` |
| the two bodies | case 4's chain unchanged, plus a null `FRCL` that forces the full body | `EFREG[0] = 2330` |

**The project's core returns all five first time**, identical to MAME's core's. It is a confirmation and
not a correction: that code was right, it simply had no witness.

**And those cases know how to fail**, by the same method as above — four faults injected into
`dsp_own.cpp`, one at a time, **each caught by its own case**: `NEGB` that no longer negates, `FRCL` that
takes the other slice, `YRL` that latches the input shifted by one bit, `ADRL` that shifts by eight instead
of sixteen. A fifth attempt, `YRL` latching `SHIFTED`, **does not compile** — and that is itself a fact
about the block: at that point in the step, `shifted` does not exist yet, because the shifter is one step
behind.

What that does not say, and it is the same limit as above: **that MAME is right.** Nobody has measured
those four fields on hardware, and `scsp_references.md` refuses consensus between emulators as proof. The
behaviour this port aims at is MAME's; these cases establish that its DSP reaches it, and nothing more. The
difference from the first eight cases is real and worth saying: those are cross-checked by the game on
every replay, these five by nothing but themselves.

### The rest of the board, already in place before this work

| | what it compares |
|---|---|
| `verify-vf3.cmd` | the PowerPC's state frame by frame and the 8 MB of RAM, this branch against `main`, on Virtua Fighter 3 — the port's non-regression contract |
| `tools/statecmp.py` | two state files (the instrumented Supermodel's and `vf3run --state`'s) |
| `build/bin/vf3lock` | the interpreter against the recompiled code |
| `VF3_SCSP_WLOG` | every SCSP register write, frame by frame — an access trail already usable |
| `VF3_KEYON_LOG` | every voice started, with its shape (TL, AR/D1R/D2R/RR, DL, EGHOLD, LPCTL, LSA/LEA/SA) and the 68000's state |
| `VF3_SCSP_SOLO=0\|1` | one of the two SCSPs at a time |
| `VF3_DUMP_SOUND_RAM` | the sound RAM and both chips' registers, at given frames |
| `m_probe_*` / `energy_on` | the energy each slot sent, direct and towards the DSP, and the energy of the DSP's outputs |
| `VF3_SCSP_MADRS_SEPARATE` / `_MIRROR`, `VF3_SCSP_MSLC_LATCH` | force a fidelity choice one way or the other, for comparison |

So this repository is **well equipped for investigation** and poorly equipped for automatic testing —
`make test` is the first, and it covers only one block out of eight. That is the opposite of what one would
want for a rewrite, and it is the point to correct before starting one.


## 3. What is missing

### a. The scenes

There is **only one scene** for the sound: `fv2-fight.txt`. What is missing, and the specification asks for
them:

| scene | what it would take | why it is missing |
|---|---|---|
| silence | the game at a moment when nothing is playing | no moment of the current scene is silent; it would have to be found or imposed |
| music only | `VF3_VOLUMES=100,0` | **doable right now**, not done yet |
| effects only | `VF3_VOLUMES=0,100` | **doable right now**, not done yet; careful, those two turn the effects leveller on (`m_group_gains`), so they do not measure the original mix |
| sound test | the cabinet's test menu | crossed with the pad; `console_play` has no lever to get there without a human hand |
| Virtua Fighter 3, the time | `vf3-match.txt` with `VF3_PROF=1` | the scene exists and already serves for non-regression; its **time** has not been recorded |

### b. The per-block tests

**One out of eight**, the DSP (`make test`, described above). The other seven do not exist, and nothing
exercises the SCSP itself outside the game. The list of what it would take, in order of dependency (the
partition is `docs/scsp_architecture.md` §3's):

1. **registers** — write every word from 0x000 to 0xFFF and read it back; the reserved bits; the range
   0x7C0-0x7FF, where this port and MAME do not do the same thing (`docs/scsp_references.md`);
2. **PCM** — 8 and 16 bits, the four `LPCTL`s, `SA`/`LSA`/`LEA`, the interpolation, RAM's edges
   (`read_byte`/`read_word` mask to 0xFFFFF and return 0 beyond: to be exercised);
3. **envelopes** — the four states and their transitions, `EGHOLD`, `KRS`, `LPSLNK`, key-off in mid
   attack, rates 0 and 1 ("infinity" in the tables);
4. **pitch, LFO, modulation** — `OCT`/`FNS` at the extremes, the four waveforms, and ring modulation with
   this port's depth;
5. **DSP** — ✔ **done** (`make test`): the two bodies, the four controls these games never set, the delay
   line's addressing, the step giving up, `uses` added by `Decode` and recomputed by `Start`. Left outside:
   `PACK`/`UNPACK` over the whole 24-bit range for themselves (the test transcribes them, so a shared fault
   would pass), and the shifter's saturation at the exact edges rather than at the seeds' mercy;
6. **timers and interrupts** — A/B/C and their periods, `SCIEB`/`SCIPD`/`SCILV`, the priority decoding,
   the DMA, and the relative order of the 68000's accesses;
7. **the two chips and the mixing** — `DISDL`/`EFSDL`, the pan, `MVOL`, `DAC18B`;
8. **reset and repeatability** — two identical runs give the same PCM (which is already true in fact: two
   runs' WAVs compare byte for byte, and the only random source, `machine_rand`, has a fixed seed).

### c. The two-core harness

`compare-sound.cmd` compares two **builds**. For a rewrite, two **cores in the same binary** have to be
compared: the same sequence of accesses replayed through both, and the comparison of the PCM, the
observable state, the interrupts and the timers, not just the final WAV.

**Done on 2026-10-26**: `tools/sound/replay` and `tools/sound/replay.cmd`, on the format of
`src/sound/scsp_trace.h` and the selector of `src/sound/scsp_select.h`. `VF3_SCSP_WLOG` did not serve as a
starting point: it records the frame and not the sample, and leaves out chip 1, the reads, incoming MIDI,
the timers and the RAM writes. The replay finds the board's PCM **byte for byte** over 229 991 samples,
which establishes that the trace is complete — the check to pass before comparing two cores means
anything.

**And how far has to be said, because it was not all the way.** Those 229 991 samples are **300 frames**,
`replay.cmd`'s default. Run on 2026-10-28 over the whole scene — 4 200 frames, 3 219 865 samples per chip
— **this check failed**:

| chip | first divergence | what the replay returned | what the board had produced |
|---|---|---|---|
| 0 | sample **772 067** left (frame ~1 007) | 0 | −4 |
| 1 | sample **1 149 006** right (frame ~1 499) | −992 | −976 |

**Found and closed the same day.** What follows is the measurement that named it, kept because the way this
divergence hid itself is the most instructive part of the affair.

### What the measurement said before knowing

1. **It was not a divergence between the two cores.** The second check, `mame` against `own` over the same
   4 200-frame trace, said **IDENTICAL** on all five streams. So the cores' identity held over the whole
   scene; it was **the trace's completeness** that only held over 300 frames.
2. **The extent, and it is what gave the direction.** Not one isolated sample: **84.7% of the values after
   the first divergence**, all the way to the end, of tiny amplitude — median difference 12 out of 131 072,
   maximum 156, and **all multiples of 4** (the 18-bit output whose two low bits fall away). So a state,
   not a calculation accident. And chip 1 was **byte-identical** over the first 1 050 frames while chip 0
   diverged: the cause depended on what the driver was writing, chip by chip.
3. **Three leads ruled out by measurement, not by reasoning**: a sound-RAM write path that the trace might
   be missing (there is only one, `Bus` cases 0x0/0x2, and it calls `ram_written`); the per-group gains
   (inert, both volumes being 1, and `m_group_gains` stays false); the sample ROM (the SCSPs only play from
   their own RAM, the 68000 copies into it — `soundboard.h`).

### The cause, and it was written in advance

`scsp_core.h` had named it from the start, in its **"AND WHAT IS DELIBERATELY ABSENT"** section: four
things of this port's do not go through the interface, and the board was setting them **directly on the
concrete class**. The MADRS mirror is the only one of the four **that changes the sound**, and it changes
**per game**: true for Virtua Fighter 3, **false for Fighting Vipers 2**. It was being set in
`SoundBoard::reset`, through a `dynamic_cast`, **after** the recorder was created — so the trace could not
see it, and a replay left it at its default, `true`.

**The decisive check needed no code**: the same scene recorded again with `VF3_SCSP_MADRS_MIRROR=1`, which
puts the board in the setting the replay was assuming, replayed **byte-identical on both chips**. That is
what named the culprit instead of suspecting it.

And it explains why 300 frames passed: the first write into `0x7C0-0x7FF` arrives at sample **29 211**, but
it is **zero** — and the mirror only tells the two settings apart when a non-zero value goes there and the
DSP program uses it. The check was not lying; it was too short.

### What was closed

| | |
|---|---|
| **a single door** | `scsp::set_madrs_mirror_on` and `scsp::madrs_mirror_of` in `scsp_select.h` — whose header already named this gap. The `dynamic_cast` no longer lives in `soundboard.cpp` |
| **the order** | the mirror is set **before** the recorder exists, and the recorder **reads it back from the chip** instead of being told it |
| **the trace carries it** | header **version 3**: `# scsp-trace 3 chip=N clock=H ram=B mirror=0\|1`, and the replay sets it before the first line |
| **a v1 or v2 trace is refused** | with a message that says what to do, and not guessed at — assuming is precisely what this divergence cost |

**Checked on 2026-10-28, whole scene**, and the three checks count together:

| | |
|---|---|
| `tools/sound/replay.cmd fvipers2 tools/scripts/fv2-fight.txt 4200 mame own` | **EVERYTHING AGREES** — the four checks, 3 219 865 samples per chip, both chips |
| `make test` | the ten tests, **all cases pass**, both cores wherever the test takes a core name |
| `vf3run --frames 4200 --rec --wav`, each core | the same footprint **`9A2310F8AF000E09EB18BD114852B003`** — the fix does not change one value of the sound |

So the trace's completeness is established over the whole scene, and no longer over 300 frames only.

**The lesson, and it is a general one:** *state that does not arrive through a call does not go into a
trace of calls.* The port's three other extras do not change the sound at the default setting, and that is
the only reason they are not in the header; if one of them started to count, it would have to go in as
well.


## 4. The hole this work dug, and that is plugged

Batch 1 of 2026-10-24 cut the DSP's body into two paths (`docs/scsp_audit.md` §7). For this port's two
games, it is the lean path that runs, and it is verified byte for byte; the other was verified by nothing,
and it was the first test to write if this port ever served a third game.

**It is written** (`tools/sound/test_scspdsp.cpp`, §2): `Run<USES_ALL>` is exercised by eleven of the
sixteen cases, on programs that set the four controls together and then each one alone, and on the games'
own program with `uses` forced — that last case being the one that says **the two bodies agree**. Four
faults injected into the generic body did bring the test down.

What is left of the hole, and it is small: the test compares the port with a transcription of the same
semantics, not with hardware documentation. It would catch a miswired `if constexpr`, a misread bit of
`Inst::f`, a stale `uses`, a shifted local copy — that is to say everything this batch could break. It
would not catch an error MAME already has, nor a fault copied identically into the reference.
