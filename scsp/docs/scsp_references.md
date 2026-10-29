# The SCSP's sources — what was read, and what is known from it

> This document is one of the nine on the sound. The front door, which says where everything is:
> [docs/sound.md](sound.md).

Recorded on 2026-10-24. Every line says **where it was read**. What has not been checked is marked as such;
nothing here is deduced from a memory.


## The warning that comes first

**The implementations were not independent of one another, and their agreement proved nothing — and that
has stopped being true of Supermodel, which makes it the warning's own counter-example.**

MAME's SCSP carries `copyright-holders: ElSemi, R. Belmont` at its head: ElSemi wrote it, R. Belmont
converted it for MAME/M1, kingshriek corrected it. Supermodel's came from **the same ElSemi**, who gave it
to the project; the header of `Src/Sound/SCSP.cpp` still says so and separately credits a MAME conversion
by Paul Prosser. So for years the two emulators were not two witnesses but **one witness read twice**, and
"MAME and Supermodel do the same" settled nothing.

**Read at the source on 2026-10-29, and it is no longer the state of things.** Supermodel's commit
`b1f2c162` of **2026-10-21** rewrites that core twice over:

- its DSP pipeline now **follows Mednafen** — "memory reads/writes complete one step later, IWT takes the
  word read 2-3 steps earlier, NOFL is bit 8" — and the message states it "matches Mednafen exactly on 300
  random programs". Mednafen's SCSP is a Saturn lineage, not ElSemi's, so this is a **second** witness;
- and it cites the **YMF292 manual** throughout: Fig. 4.5 (MADRS), Fig. 4.7 (the internal buffers),
  Fig. 4.30/4.31 (the sound stack's delay), Fig. 4.65, Table 4.12 and 4.17, pages 35, 72 and 96,
  section 4.2 (DMA). That is a **primary source**, and it is one this repository does not have (see "what
  will remain impossible to check here").

So the warning holds for MAME against Supermodel-before-2026-10-21, and no longer after it. The practical
consequence has turned around: where Supermodel now cites the manual, it is this repository that has the
weaker evidence, and said so on the `0x7C0-0x7FF` question (point 3 below).

In this repository, the settings that depart from MAME were decided by measurement (a state's level, the
reverb's left/right correlation, a voice's spectral gap), not by a vote between emulators — and that stays
the right method. What the `0x7C0-0x7FF` question taught is the method's blind spot: a measurement against
a REFERENCE BUILD is only worth what that build is, and must be checked at its source.


## Licences, and what may be done with each source

| source | licence | what may be done with it here |
|---|---|---|
| MAME (`mamedev/mame`), `sound/scsp.cpp`, `sound/scspdsp.cpp`, `cpu/m68000/` | **BSD-3-Clause** | **reusable**, keeping the notice, the conditions and the warranty disclaimer. That is what the repository does. |
| Supermodel (`trzy/Supermodel`) | **GPL** | **observation only**. Neither copy nor mechanical translation: this repository is under MIT. One may look at what it *does*, not take over how it writes it. |
| Mednafen (SASPlay) | **GPL** | likewise: observation only. |
| Yabause, Kronos | **GPL** | likewise. |

`tools/ref/README.md` already lays down this line for the Supermodel coverage patch: "Supermodel is GPL,
and so is this patch. The port's own code must not copy from it." It holds for the sound as well.


## What was checked today, at the source

### MAME — the DSP decodes its program on every sample

Read in `src/devices/sound/scspdsp.cpp` on the `master` branch: the loop opens with

```
for (int step = 0; step < /*128*/LastStep; ++step)
    u16 *const IPtr = MPRO + (step * 4);
    u32 const TRA = (IPtr[0] >> 8) & 0x7f;
    ...
```

and **extracts the fields by shifts and masks at every step of every sample**. The only thing precomputed
is `LastStep`, which `Start()` obtains by looking for the last non-zero instruction.

Supermodel does the same: `Src/Sound/SCSPDSP.cpp` computes `IPtr = DSP->MPRO + step * 4` and extracts TRA,
TWT, XSEL, MWT… inside the loop, on every sample.

**So this repository is ahead of both** on that precise point: `SCSPDSP::Decode` writes the decoded program
into `prog[128]` once, at the moment the 68000 writes it, and since today the one-bit controls are grouped
into a word (`Inst::f`). See `docs/scsp_audit.md` §7. It is not a divergence in behaviour: the decoding is
a bijection, and the WAV is byte-identical.

Present in MAME as they are here: `PACK`/`UNPACK`, and the memory access restricted to odd steps
(`if (MRD && (step & 1)) //memory only allowed on odd?`, with the remark about the NOPs DoA inserts on the
even steps).

### MAME — the mirroring of MADRS, and what it did with it

`mamedev/mame` **PR #16527**, merged on **2026-10-19**:
> `sound/scsp.cpp: Don't mirror MADRS at 0x7c0-0x7ff, fixes corrupted and then silent sound in doa`

The state of `master` as read today, and precision is needed because the two paths do not do the same
thing:

| | MAME `master` | this repository, `madrs_mirror = true` | this repository, `madrs_mirror = false` |
|---|---|---|---|
| write into 0x7C0-0x7FF | **thrown away** (empty branch, comment "0x7c0-0x7ff does not mirror MADRS: doa clears this range") | goes into MADRS | kept in `m_madrs_hi` |
| read of 0x7C0-0x7FF | **returns MADRS** | returns MADRS | returns `m_madrs_hi` |

So:

1. **On the write side**, MAME `master` and this repository's "separate" mode do the same thing in what
   matters: the zeros the sound program writes over the top no longer erase the delay lines' addresses. It
   is the setting the measurement forced for **Fighting Vipers 2** (`docs/fv2-console-sound.md`: the victory
   screen's sound feeding itself, mean peak of the state 22831 → 6532 when they are kept apart). The
   repository and MAME arrived at the same answer, by two separate paths and for two different games (`doa`
   there, `fvipers2` here).
2. **On the read side**, the two differ: MAME returns MADRS, the "separate" mode returns the words written.
   If a sound program reads that range back and acts on it, the two will not give the same thing. **Not
   measured** here: it is not known whether either game reads it back.
3. ~~**And for Virtua Fighter 3, this repository keeps the mirror**~~ — **withdrawn on 2026-10-29, and
   the reason it rested on was circular.** The reason was that without the mirror VF3's reverb held for
   several seconds "where the reference cuts it in one". The reference is Supermodel at **`c65edb1`**, the
   commit `tools/ref/supermodel-cov.patch` is made against, and that build **mirrors**: its own comment
   reads `// MADRS is mirrored twice`. The agreement was with a build that had the same behaviour, so it
   established nothing. `madrs_mirror = false` for both games now (`src/sound/scsp.h` carries the whole
   reasoning).

   Two sources outside this repository point the same way, and neither is ours: MAME stopped mirroring on
   2026-10-19 (#16527, for `doa`), and Supermodel on 2026-10-21 (`b1f2c162`) **citing manual Fig. 4.5 —
   MADRS is 32 words at 780h-7BFh** — and saying in so many words that a mirror "wipes the taps and
   silences their reverb" for **VF3 and Sega Rally 2**. On VF3 specifically, that is a primary source
   against a reading of ours that had none.

   **Measured since, and it changes the reading** (`docs/fv2-sound-program.md` §5): **both** games' sound
   program loads MADRS and then writes **32 zeros into `0x7C0-0x7FF`**, in that order. With the mirror,
   those zeros overwrite MADRS: **878 addresses lost on Fighting Vipers 2, 176 on Virtua Fighter 3**, and
   only the 53 and 19 written after the erasure survive. So the mirror folds the DSP's delay lines onto
   address 0 for VF3 as well — the same mechanism that the same day judged wrong on Fighting Vipers 2, and
   that MAME corrected for `doa`.

   **But the magnitude is not there, and that has to be said.** The two sounds produced without a window on
   the same VF3 scene (`tools/sound/compare-madrs.cmd vf3`, 2400 frames) differ by little: mean peak
   **+2.2 dB** on the coin-up and **+0.0 dB** in combat, spectrum within **±0.1 dB**, the coin-up's and the
   start's echoes going from 2.05 to 2.25 s and from 0.95 to 1.15 s — and above all **the left/right
   correlation does not move** (0.861 against 0.865), when that is the signature of the collapse and it was
   0.99 against 0.73-0.85 on VF2. "Several seconds" is not reproduced. Either VF3 uses few delay lines, or
   the scene does not go where it shows — it crosses neither a K.O. nor a victory screen, and that is where
   FV2's defect lived.

   **What remains open, and is not settled here**: what the hardware does. If a real SCSP mirrors, the
   cabinet also ran with its lines collapsed and the short reverb is the period sound. And, closer to home:
   whether the gap is audible anywhere other than on that scene. Changing this setting changes the port's
   sound; it is a decision and not a correction.

### Mednafen — SASPlay

Read at `https://mednafen.github.io/documentation/sasplay.html`: it is a module for **playing the music**
of Sega Model 2A/2B/2C and 3 cabinets, not a chip reference. What is useful there:

- the Model 3 outputs **4 channels**, which SASPlay folds into stereo;
- a base of per-game volumes, with per-track adjustments (Virtua Fighter 3 is named there);
- and above all, said in black and white: the Model 3s' music can play **up to 2% fast**, because Mednafen
  does not emulate "dynamic SCSP wait states and interrupt signaling delays", and the interrupt handlers
  are sensitive to them.

That last point is worth keeping: the music's **rate** depends on the SCSP's wait states and on the
interrupt signalling delay. This repository has already had to treat the same family of problem another
way — its timers A-C count samples and land on a round sample, like Supermodel's (`src/sound/scsp.h`), and
`SoundBoard::run` gives the 68000 only one sample's worth of cycles at a time so that a timer rearmed
inside an interrupt is seen in the slice where it was set (`soundboard.cpp`'s comment says what happens
otherwise: "the music slowed to a crawl"). If the music's rate is ever called into doubt, that is where to
look, and Mednafen's sentence says where.

### What was NOT done

- **Yabause** (`libretro/yabause`) and **Kronos** (`FCare/Kronos`): not read. They are the two Saturn
  implementations the specification cites, and `src/sound/scsp.h`'s comment already asserts something about
  them ("Mednafen's SCSP (its MADRS at 0x780-0x7bf only) and Yabause's keep them apart") -- **that
  assertion was not rechecked at the source today**.
- **The YMF292-F / SCSP technical documentation**: not obtained. Nothing was downloaded, and no assertion
  in this repository rests on it for the time being.
- ~~**Supermodel's code beyond `SCSPDSP.cpp`**~~ — **read on 2026-10-29, and the assertion is CONFIRMED at
  the source.** `src/sound/scsp.h` had said "Supermodel kept ElSemi's older DSP for this very game until it
  made its DSP's memory access follow the manual", marked here as unconfirmed. `Src/Sound/SCSP.cpp`'s own
  to-do list says it almost word for word:

  > "Check that music in Fighting Vipers 2 Bahn's stage no longer hangs (it used to need ElSemi's older DSP
  > register handling, which has been removed now that DSP memory access follows the manual)."

  So the claim was right, and its game is Fighting Vipers 2. **And the option is gone**: a code search over
  `trzy/Supermodel` returns no `LegacySoundDSP`, which matters because that is still the only answer given
  on their open issue #321 about this very defect.

### And what will remain impossible to check here

No measurement on **real hardware** is possible from this machine: no Model 3 cabinet, no analogue
reference capture. Every question of fidelity settled here is settled against **another emulation** or
against **the game's internal coherence** (a level that should not rise, a reverb that should not fold into
mono). That is useful and it is not the same thing. No statement in this repository should say "faithful to
the hardware" on that basis.


## Sources

- MAME, [`src/devices/sound/scspdsp.cpp`](https://raw.githubusercontent.com/mamedev/mame/master/src/devices/sound/scspdsp.cpp) and [`src/devices/sound/scsp.cpp`](https://raw.githubusercontent.com/mamedev/mame/master/src/devices/sound/scsp.cpp) (branch `master`, read on 2026-10-24)
- MAME, [PR #16527](https://github.com/mamedev/mame/pull/16527) — "Don't mirror MADRS at 0x7c0-0x7ff"
- Supermodel, [`Src/Sound/SCSPDSP.cpp`](https://raw.githubusercontent.com/trzy/Supermodel/master/Src/Sound/SCSPDSP.cpp) and [`Src/Sound/SCSP.cpp`](https://repo.retrodeck.net/Xargon/Supermodel/src/branch/master/Src/Sound/SCSP.cpp)
- Mednafen, [SASPlay documentation](https://mednafen.github.io/documentation/sasplay.html)
- [`github.com/mirror/model3emu`](https://github.com/mirror/model3emu) (Supermodel mirror)
