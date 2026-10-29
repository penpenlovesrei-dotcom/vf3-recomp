# The standalone SCSP — interface contract and partition

> This document is one of the nine on the sound. The front door, which says where everything is:
> [docs/sound.md](sound.md).

> **Update of 2026-10-25.** This document describes the **C++ interface**. The contract's second piece —
> what the driver expects of the **chip**, field by field, with every number's provenance — now exists
> separately: `src/sound/scsp_contract.h`, checked by `tools/sound/test_scsp_contract.cpp` against the
> implementation and against the ROM. The two together are the prerequisite that
> `docs/scsp_migration_plan.md` called "the interface contract".

**This is a plan, not a description.** Nothing that follows is written as of today (2026-10-24): the
sound's core is still MAME's, modified (`docs/scsp_audit.md` §2). This document exists so that, the day it
is taken on, the first move is not to guess.

Why a standalone core: objective A of the specification — progressively getting out of third-party code.
It is **not** a speed objective (the gains measured on 2026-10-24 were obtained on MAME's core as it is,
without changing one of its outputs), and it is **not** a fidelity objective (MAME's code is today this
port's fidelity reference, measured game by game).


## 1. Today's interface, as it stands

Before drawing the next one, that one has to be read. What `SoundBoard` asks of the `scsp_device`, and the
other way round, is exactly:

**The board to the chip** (calls):

| | |
|---|---|
| construction | `scsp_device(u32 clock, u16 *ram, u32 ram_bytes, const host &)` — the RAM is **shared** with the 68000, not copied |
| registers | `u16 read(offs_t offset)` / `void write(offs_t offset, u16 data, u16 mem_mask)`, in words from the chip's base |
| MIDI | `midi_in(int state)` (one bit) and `midi_in_byte(u8)` (one byte, the route actually used) |
| timers | `timer_fired(int timer)` — 0-2 the timers A/B/C, 3 the receiver's clock, 4 the transmitter's |
| time | `update()` — "make the samples up to now" |
| output | `std::vector<float> m_out` (stereo pairs, −1..1, before the gain), `output_gain()`, `samples_made()` |

**The chip to the board** (`scsp_device::host`, today five `std::function`s):

| | |
|---|---|
| `irq(line, state)` | the 68000's IPL lines (only the first chip is wired to them) |
| `midi_out(state)` | TxD to the main board |
| `timer_adjust(timer, delay, period)` | arm/disarm, in attoseconds, `period != 0` = periodic |
| `sample_index()` | the current sample (`now * 44100`) |
| `time_is_zero()` | true just after power-on |

What this interface **drags along from MAME**, and that a standalone core has no reason to keep:

1. **The attoseconds.** `timer_adjust` speaks in `unsigned __int128` of attoseconds, which
   `soundboard.cpp` converts straight back into 68000 cycles, and rounds to the sample for timers A-C. Two
   conversions for nothing: this board's natural unit is **the sample** (and the 68000 cycle, which is
   1/256 of one).
2. **The pulled stream.** `update()` is MAME's `sound_stream::update`: the chip makes "up to now" when it
   is touched. That is what makes the samples be born **inside** the 68000's slices and what made the
   measurement hard to break up (`docs/scsp_audit.md` §1). The model is not wrong — it is even what gives
   the right grain when the 68000 writes a register in the middle of a sample — but it must be **chosen**,
   not inherited.
3. **The five `std::function`s.** Two of them (`sample_index`, `time_is_zero`) are called on every register
   access, hence thousands of times per frame, to return an integer. One indirect call per question.
4. **`device_serial_interface`.** MAME's serial receiver and transmitter (`emu/diserial.cpp`) are copied
   into `scsp.h` for a single MIDI link at 31250 baud, 8N1, of which this port uses only the "one whole
   byte" path (`midi_in_byte`). Some thirty members and seven methods for that.


## 2. The proposed contract

One partitioning rule only: **the core knows neither the PC, nor MAME, nor the 68000.** It knows samples,
registers and RAM.

```cpp
// No std::function, no attoseconds, no framework. The unit of time is the sample.
class scsp_core
{
public:
    struct sink {                                   // what the board plugs in underneath
        virtual void irq(int level) = 0;            // the level asked for (0 = nothing), not one line at a time
        virtual void midi_out(u8 byte) = 0;         // a byte, not a bit
        virtual void timer(int which, u32 samples, u32 period) = 0;   // 0 = disarm
    };

    scsp_core(u16 *ram, u32 ram_bytes, sink &);
    void reset();

    // The registers. The caller guarantees that time is up to date (see run) before touching them.
    u16  read_reg(u32 word_offset) const;
    void write_reg(u32 word_offset, u16 data, u16 mem_mask);

    // Time, pushed: makes exactly n samples into out (stereo pairs, s32 20 bits before the DAC, as the
    // chip has them). The caller slices wherever it likes -- to the sample if it wants MAME's grain,
    // wider if it does without.
    void run(u32 n, s32 *out);

    void timer_fired(int which);
    void midi_in(u8 byte);
};
```

What this contract changes, and why:

| | today | proposed | why |
|---|---|---|---|
| time | pulled (`update()` asks the host) | **pushed** (`run(n)`) | the core no longer needs to know the time; the granularity goes back to being the board's decision, written once in `soundboard.cpp` |
| unit | attoseconds → cycles → samples | **samples** | the hardware's unit; no more rounding to patch up |
| callbacks | 5 `std::function`s | **one interface, 3 methods** | one virtual call per rare event instead of an indirect call per register access |
| output | a `std::vector<float>` that grows and gets drained | **a buffer the caller provides** | zero allocation on the real-time path, and the float arrives at the end, once |
| serial | `device_serial_interface` copied in | **a byte in, a byte out**; the bauds and the bits stay in a small separate `midi_uart` | the Model 3's link carries nothing but bytes; the bits serve only the interrupt rate, which is the one thing to keep |

**The 68000 and the Model 3 board stay outside.** `soundboard.cpp` keeps the board (memory map, banking,
timers, rate), `third_party/mame/m68000` keeps the 68000. The SCSP core depends on neither, which is the
condition for testing it alone.

**And the PC's audio API comes in nowhere.** It does not come in today either (`SoundBoard::run` returns
`int16_t`, the host does the rest): nothing to undo.


## 3. The blocks, and the order to write them in

One block per thing that can be tested on its own. The order is the order of dependency, not of importance.

1. **Registers and access semantics** — the map of 0x000-0xFFF: the 32 slots (0x20 bytes each), the common
   area (0x400-0x45F), the DSP registers (COEF 0x700, MADRS 0x780, MPRO 0x800, TEMP, MEMS, MIXS, EFREG,
   EXTS). Including the reserved bits, what reads back, and **the range 0x7C0-0x7FF**, which this port and
   MAME do not do the same thing with (`docs/scsp_references.md`). Tested with no sound at all: write,
   read back.
   **The test exists** since 2026-10-26: `tools/sound/test_scsp_regs.cpp`, nine cases, and it has already
   reported two things no reading had seen — MAME's "bit 5" exception lands on MCIRE (0x42E) and not on
   MCIPD (0x42C), which the comment names; and the range 0x7C0-0x7FF, measured in **both** settings of the
   MADRS mirror rather than in the default one.
2. **Slots, oscillator, PCM, addressing, looping** — signed 8 and 16 bits, `SA`/`LSA`/`LEA`, the four
   `LPCTL` modes, linear interpolation, RAM's limits. Tested by counting samples.
   **The test exists** since 2026-10-26: `tools/sound/test_scsp_slots.cpp`, eleven cases, and it
   contradicted the reading on all four loop modes at once. The looping loop runs over `cur_addr` **and**
   `nxt_addr`, but `nxt_addr` is reassigned on every sample: its address rewrite is lost, its side effects
   (`Backwards`, `StopSlot`) are not. So everything fires **one sample early**, and no sample beyond `LEA`
   is ever read — contrary to what reading alone predicted (`scsp_contract.h` §9).
3. **ADSR envelopes** — the four states, `AR`/`D1R`/`D2R`/`RR`, `DL`, `EGHOLD`, `KRS`, `LPSLNK`, and the
   time tables. **Careful**: MAME's `ARTimes`/`DRTimes` tables are measured constants, not a formula;
   taking them over is a fidelity choice to be owned explicitly.
   **The test exists** since 2026-10-26: `tools/sound/test_scsp_adsr.cpp`, ten cases. It pins the attack
   durations **in samples** (1297, 332, 83, 24, 1 for `AR` = 0x10 to 0x1F), so that a formula in the
   tables' place fails there and nowhere else — the warning above becomes a measurement. It also shows that
   the attack multiplies the **raw volume** when everything else goes through the decibel curve, and that
   `DECAY1` always decrements once even when `DL=0` makes it give way immediately (`scsp_contract.h` §10).
4. **Pitch, LFO, FM modulation** — `OCT`/`FNS`, the four LFO waveforms, `PLFOS`/`ALFOS`, and ring
   modulation (`MDL`/`MDXSL`/`MDYSL`). **This port's settings are kept as they are** until measured
   otherwise: FM depth at 24 and `MDL`'s low bit ignored (`docs/scsp_audit.md` §2).
   **The test exists** since 2026-10-26: `tools/sound/test_scsp_lfo.cpp`, eleven cases, and it pins the two
   settings named above rather than leaving them to a reviewer's goodwill. It also found that **eight of
   the thirty-two LFO rates do nothing** — `phase_step` truncates to zero below 0.673 Hz — and that
   `LFORE` is read nowhere in this core (`scsp_contract.h` §11).
5. **The effects DSP** — 128 steps, `TEMP`/`MEMS`/`MIXS`/`EFREG`, `PACK`/`UNPACK`, the delay line in sound
   RAM, memory access on odd steps. It is **the costliest block** (half the sound board) and the one with
   the most to gain from being written for what it is: a fixed 112-step program executed 44 100 times a
   second. This repository's decoded form (`Inst`, `Inst::f`) is already beyond MAME's and Supermodel's,
   which decode in the loop; the natural next step is a form compiled per program, not a tighter
   interpreter.
   **The only block `scsp_own` has not written**, and the reason is not time: its instruction set exists
   nowhere written down. The contract gives only its layout (§6), and `docs/scsp_references.md` is clear —
   there is neither a datasheet nor an independent specification, only MAME's and Supermodel's
   implementations, out of ElSemi's same work. So writing this block requires first a **test that
   measures** the behaviour instruction by instruction, as for the other six. As long as it does not exist,
   there is nothing to write against, and the core borrows `scspdsp.{h,cpp}`.
   **The specification was written first**, on 2026-10-27: `tools/sound/test_scsp_dsp.cpp`, eight cases
   measured through the register window (TEMP, MEMS, MIXS, EFREG) and through RAM, without a note of music.
   It brought out the block's central fact, which no reading shouts: **the shifter is one step behind the
   accumulator** — the step that does a product cannot write its result, the next one is needed.
   **Then the block**, `src/sound/dsp_own.{h,cpp}`, verified byte for byte by the replayer over the whole
   scene. Four of its twenty-five fields — `FRCL`, `ADRL`, `YRL`, `NEGB` — are validated by **no**
   measurement: this game's programs leave them at zero in all its arenas, and the `dspprog` probe says so
   with its own control in the same line. Written, never tested.
6. **Timers, interrupts, synchronisation** — A/B/C, `SCIEB`/`SCIPD`/`SCILV`, `MCIEB`/`MCIPD`, the DMA, and
   the interrupt rate, which Mednafen says is what makes the tempo drift by 2%
   (`docs/scsp_references.md`).
   **The test exists** since 2026-10-26: `tools/sound/test_scsp_irq.cpp`, eleven cases, measured through
   the sink rather than through the sound. It pins `timer_samples`'s `+1` — the port's third deliberate
   divergence, the one that put VF3's tick back to 882 Hz — and it found that **line 0 does not drop when
   nothing is pending**: `CheckPendingIRQ` returns before that, and the `-0` only happens on a pending
   source that nothing arms (`scsp_contract.h` §12).
7. **Mixing and output** — `DISDL`/`DIPAN`, `EFSDL`/`EFPAN`, `MVOL`, `DAC18B`, the pan tables. This port's
   two per-group volumes and its effects leveller are **above** the chip, not in it: they stay in the
   board.
   **The test exists** since 2026-10-26: `tools/sound/test_scsp_mix.cpp`, nine cases. It reads the three
   decibel tables in full, and it established that **`MVOL` is never applied in the chip** — the chip only
   exposes it, and the board multiplies — which is a **hole in the interface**: `scsp::core` cannot return
   `MVOL`, so the board goes down through `->device()` (`scsp_contract.h` §13).


## 4. What is particular to the Model 3, and that a Saturn document will not say

To keep in view, because the temptation will be to read Saturn documentation and apply it:

- **Two SCSPs**, not one. Each with its own 1 MB of RAM, each with its own 128-step DSP (both run:
  measured, 112 steps each during Fighting Vipers 2's attract). **Only the first one's interrupts reach the
  68000**; the second's go nowhere.
- **The 68000 is at 11.2896 MHz** (45.1584 / 4), that is exactly **256 cycles per sample** at 44.1 kHz.
  That is what makes the sample an honest unit of time on this board.
- **The SCSPs only play from their own RAM**; it is the 68000 that copies the samples there from the 16 MB
  of ROM, by bank (bit 4 of register 0x400001).
- **The MIDI arrives from the main board** into the first SCSP's input, and that is the whole command
  channel: the PowerPC sends nothing but bytes.
- **The Model 3 outputs four channels**, folded into stereo here as in Mednafen.
- This port's settings that depend on **these particular games** (the per-game MADRS mirror, a voice's
  group read from the records at 0x400 in sound RAM) are not the chip's: they are the board's and the sound
  driver's, and they must stay that way.


## 5. What must exist before the first line

1. The **differential harness**: it exists (`tools/sound/compare-sound.cmd`), but it compares two *builds*
   of the port, not two *cores*. It needs a selector (environment variable or build option) that runs the
   old one and the new one in the same binary, over the same sequence of accesses, and compares the PCM,
   the observable state, the interrupts and the timers.
2. The **fallback**: the old core stays there, behind that selector, and only goes afterwards.
3. The **per-block tests** of §3's list, written before the block they test.

As long as those three do not exist, writing the standalone core is a bet, not a job.
