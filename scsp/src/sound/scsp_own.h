// scsp_own.h -- the project's SCSP: an implementation of scsp::core that is not MAME's.
//
// Step 3 of docs/scsp_migration_plan.md, and its loop: "write the test -> write the block -> the harness
// says identical -> measure the cost -> keep it; otherwise: understand the gap, do not widen the
// tolerance." The seven block tests have existed since 2026-10-26 and the replayer since 2026-10-27; this
// file is what they were waiting for.
//
// -----------------------------------------------------------------------------------------------------
// PROVENANCE, and it has to be read before the code.
//
// `src/sound/scsp.cpp` -- MAME's core, which this port still uses -- is under BSD-3-Clause, and the
// repository under MIT: DERIVING from it would therefore be perfectly lawful, provided the notice and the
// warranty disclaimer are kept (docs/scsp_audit.md §2). But that would not be getting out of MAME: it
// would be having one more copy of it, carrying the same attribution, for the specification's same
// objective A -- "progressively eliminate MAME's third-party code". Translating eliminates nothing.
//
// So this file is written from `src/sound/scsp_contract.h` and from the seven block tests, not by
// transcribing scsp.cpp. The structure and the wording are the project's.
//
// WHAT IT DOES NOT CLAIM TO BE: a clean room. I read scsp.cpp at length in order to WRITE those tests and
// that contract, and one does not unread a file. What is true, and checkable, is more modest and more
// useful: the BEHAVIOUR aimed at is MAME's, measured and recorded in the tests -- a behaviour cannot be
// owned -- while the code below is written from those measurements.
//
// THE MEASURED TABLES ARE A CASE APART, and the project has already settled it. ARTimes, DRTimes, LFOFreq,
// PSCALE, ASCALE, SDLT and the attenuation curve are not formulas: they are constants read off hardware,
// which MAME transcribed. docs/scsp_architecture.md §3 says it of block 3: "taking them over is a
// fidelity choice to be made explicitly." They are taken over, the choice is owned, and each table says
// in a comment where it comes from. Block 3's test freezes their consequences in samples, so that a
// rewrite replacing them with a curve would fail there and nowhere else.
// -----------------------------------------------------------------------------------------------------
//
// WHERE THIS FILE STANDS: THE SEVEN BLOCKS ARE WRITTEN, and the core produces the same sound as MAME's
// BYTE FOR BYTE -- 3 219 865 samples per chip over the whole scene of fv2-fight.txt, PCM, interrupts,
// timer arms and register reads included (tools/sound/replay). Each block's state can also be read in
// tools/sound/test_*: what passes is done.
//
//     block 1  registers and access semantics .................. WRITTEN, 9 cases
//     block 2  slots, oscillator, PCM, looping ................ WRITTEN, 11 cases
//     block 3  ADSR envelopes ................................. WRITTEN, 10 cases
//     block 4  pitch, LFO, modulation ......................... WRITTEN, 11 cases
//     block 5  the effects DSP ................................ WRITTEN, 8 cases
//     block 6  timers, interrupts, synchronisation ............. WRITTEN, 11 cases
//     block 7  mixing and output .............................. WRITTEN, 9 cases
//
// BLOCK 5 WAS A CASE APART, AND HOW HAS TO BE SAID. The other six were written from scsp_contract.h,
// which says what the chip MUST do. For the DSP, the contract describes only the LAYOUT -- COEF, MADRS,
// MPRO, 128 steps of 8 bytes (§6) -- and not an instruction's semantics. And docs/scsp_references.md is
// clear about the state of the sources: the DSP's instruction set exists only IMPLEMENTED, in MAME and in
// Supermodel, both out of ElSemi's same reverse-engineering work. No datasheet, no independent
// specification.
//
// Writing this block "from the contract" was therefore impossible, and transcribing it from scspdsp.cpp
// would eliminate nothing -- one more copy, under the same BSD-3. SO THE SPECIFICATION WAS WRITTEN FIRST,
// on 2026-10-27: tools/sound/test_scsp_dsp.cpp, eight cases that each lay down a tiny program and RECORD
// what the DSP makes of it, observed through the register window (TEMP, MEMS, MIXS, EFREG) and through
// sound RAM. It also brought out the block's central fact, which no reading shouts: THE SHIFTER IS ONE
// STEP BEHIND THE ACCUMULATOR. Then the block against it: src/sound/dsp_own.{h,cpp}, whose header names
// the four fields this game never exercises -- written, but never tested.
//
// AND IT IS THE REPLAYER THAT JUDGES THE SEVEN BLOCKS TOGETHER, on a real scene, byte for byte. No unit
// test replaces it: all seven were passing while the replay was still finding two divergences.
#pragma once

#include "scsp_core.h"
#include "dsp_own.h"

#include <cstdint>

class scsp_own final : public scsp::core {
public:
	using u8  = scsp::u8;
	using u16 = scsp::u16;
	using u32 = scsp::u32;
	using s32 = scsp::s32;
	using s16 = std::int16_t;

	// `ram` and `puits` must outlive the core. `ram_octets` is the size in BYTES.
	scsp_own(u32 clock, u16 *ram, u32 ram_bytes, scsp::sink &snk);

	void reset() override;
	u16  read_reg(u32 word) override;
	void write_reg(u32 word, u16 data, u16 mask) override;
	void run(u32 n, s32 *out) override;
	void timer_fired(int which) override;
	void midi_in(u8 byte) override;
	std::uint64_t samples_made() const override { return m_now; }

	// NOT THE CHIP'S BUSINESS, and scsp_core.h says why it is not in there: the mirroring of MADRS at
	// 0x7C0-0x7FF is a PORT setting, true for Virtua Fighter 3 and false for Fighting Vipers 2, where it
	// folds all the DSP's delay lines onto one address (docs/scsp_audit.md §2, point 1). So it comes in
	// through the concrete class, as it does on MAME's adapter.
	// MVOL, the master volume, IS NOT APPLIED IN THE CHIP: the chip exposes it, and the board does the
	// multiplying itself (scsp_contract.h §13 point 7). That is a hole in scsp::core, not a liberty -- so a
	// standalone core must return the same thing as MAME's adapter, through the same door.
	float output_gain() const { return float(m_common[0] & 0x000F) / 15.0f; }

	// The same three numbers MAME's adapter exposes, to DATE a divergence instead of noting it at the
	// output (scsp.h, same addition of 2026-10-27).
	u32 slot_cur_addr(int slot) const { return m_live[slot & 31].cur; }
	int slot_eg_state(int slot) const { return m_live[slot & 31].state; }
	s32 slot_eg_volume(int slot) const { return m_live[slot & 31].volume; }

	void set_madrs_mirror(bool on) { m_mirror = on; }
	bool madrs_mirror() const { return m_mirror; }

private:
	// --- the register map, as the contract divides it up (scsp_contract.h §§1-2, 6)
	static constexpr u32 SLOTS = 32, WORDS_PER_SLOT = 16;
	static constexpr u32 COMMON_WORDS = 0x30 / 2;     // 0x400-0x42F
	static constexpr u32 RING_WORDS = 64;           // 0x600-0x6FF
	static constexpr u32 COEF_WORDS = 64;             // 0x700-0x77F
	static constexpr u32 MADRS_WORDS = 32;            // 0x780-0x7BF
	static constexpr u32 DISPUTED_WORDS = 32;         // 0x7C0-0x7FF
	static constexpr u32 MPRO_WORDS = 512;            // 0x800-0xBFF

	u16 m_slot[SLOTS][WORDS_PER_SLOT] = {};

	// --- a slot's live state, which is in no register
	static constexpr int FRAC = 12;       // the binary point of the read address
	static constexpr int EG_FRAC = 16;    // that of the envelope volume
	enum State { ATTACK, DECAY1, DECAY2, RELEASE };
	struct Lfo {
		u16 phase = 0;
		u32 step = 0;
		const int *table = nullptr;
		const int *scale = nullptr;
	};
	struct Live {
		u32 cur = 0, nxt = 1u << FRAC;    // the read address and the next one, in fixed point
		u32 step = 0;                     // the step, computed from OCT and FNS
		bool active = false;
		bool arriere = false;            // loop modes 2 and 3 read backwards
		int  state = RELEASE;
		s32  volume = 0;
		// The envelope, computed AT KEY-ON from the fields and the note's rate: these are steps per
		// sample, not the register's fields.
		s32  ar = 0, d1r = 0, d2r = 0, rr = 0;
		s32  dl = 0;                     // 0x1F - DL: the field is INVERTED
		bool eghold = false;
		Lfo plfo, alfo;
	};
	Live m_live[SLOTS];
	bool m_verbose = false;   // (temporary diagnostic)
	u8 m_ring_ptr = 0;                 // the modulation buffer's cursor, one notch per slot

	// --- the noise generator, for SSCTL=1 and LFO waveform 3.
	// The constants and the seed are MAME's core's, and that is an obligation and not a borrowing: a
	// different noise would make the sound different, and this port has to stay byte-identical. The
	// sequence itself is a standard congruential generator (Numerical Recipes), not an invention.
	u32 m_seed = 0x9d14abd7;
	u32 draw() { m_seed = 1664525u * m_seed + 1013904223u; return (m_seed >> 16) | (m_seed << 16); }

	// --- the mixing factors, in single-precision float like the original
	//
	// ONE TABLE OF INTEGERS, PRECOMPUTED, AND ONE ONLY FOR BOTH CHANNELS.
	//
	// The index is the original's encoding word: TL in eight bits, PAN in five, SDL in three.
	//
	// WHY PRECOMPUTE, instead of keeping three small float tables multiplied on the fly: because the
	// product has to land on the SAME integer as the original's, and a float product recomputed on every
	// sample does not guarantee that. The compiler is free to fuse the multiplications (FMA) or to keep an
	// intermediate at higher precision, and the result changes by one unit after truncation. The replay of
	// a 1500-frame scene showed it: ONE sample out of 1 149 952, at -967 against -968. Computed ONCE at
	// construction, the problem cannot arise.
	//
	// AND ONE TABLE IS ENOUGH, which the original does not notice: the right channel at index `pan` equals
	// the left channel at index `pan ^ 0x10`. Bit 4 of PAN only picks which channel is attenuated, and the
	// four low bits -- where the attenuation comes from -- do not change. 256 KB instead of 512.
	s32 m_gain[0x10000] = {};
	void build_factors();

	// --- the envelope tables (block 3)
	//
	// CONSTANTS READ OFF HARDWARE, NOT A FORMULA, and this is the point that
	// docs/scsp_architecture.md §3 asks to be owned explicitly: "MAME's ARTimes/DRTimes tables are
	// measured constants, not a formula; taking them over is a fidelity choice to be owned."
	// They are taken over. Block 3's test freezes their consequences in SAMPLES -- 1297, 332, 83, 24, 1
	// for AR from 0x10 to 0x1F -- so that a rewrite substituting an exponential for them fails there
	// and nowhere else.
	s32 m_ar_step[64] = {}, m_dr_step[64] = {};
	s32 m_eg_curve[0x400] = {};     // 3/32 dB per step, 95.9 dB in all
	void build_envelope();
	void compute_envelope(unsigned i);
	s32  advance_envelope(unsigned i);

	// --- the two LFOs (block 4)
	//
	// AN LFO IS A SIXTEEN-BIT PHASE AND A STEP, and it is the type that wraps it: `u16` is enough, and that
	// is why the original needs no explicit mask. The phase is incremented BEFORE the table lookup, so that
	// the first sample reads index 1 and not 0 (scsp_contract.h §11 point 3).
	int m_lfo_saw_p[256] = {}, m_lfo_square_p[256] = {}, m_lfo_tri_p[256] = {}, m_lfo_noise_p[256] = {};
	int m_lfo_saw_a[256] = {}, m_lfo_square_a[256] = {}, m_lfo_tri_a[256] = {}, m_lfo_noise_a[256] = {};
	// INTEGERS ON THE 256 SCALE, and not floats on the 4096 scale: the original truncates the factor at
	// 256 then shifts by four, which is NOT the same thing as truncating at 4096. For a factor of 1.001 the
	// first form gives 4096 and the second 4100 -- and the sound stops being byte-identical.
	// The vibrato's depth is in cents, the tremolo's in decibels.
	int m_scale_p[8][256] = {};
	int m_scale_a[8][256] = {};
	void build_lfo();
	void compute_lfo(unsigned i);
	s32  lfo_step(Lfo &l);             // the pitch factor, 1.0 = 1 << PAS
	s32  lfo_gain(Lfo &l);            // the amplitude factor, 1.0 = 1 << PAS

	// --- the timers and the interrupts (block 6)
	//
	// The five timer numbers are the contract's and the board's: A, B, C, then the serial MIDI's two
	// clocks. Only the first three belong to this block.
	enum { TIMER_A, TIMER_B, TIMER_C, N_TIMERS };
	u16 m_count[3] = {};            // each timer's counter, 0xFFFF when it has just expired
	u32 m_prescale[3] = { 1, 1, 1 };
	static u32 timer_duration(u16 reg);
	void arm_timer(int which);
	void check_pending();         // CheckPendingIRQ: ONE line, in a fixed order
	void ack_interrupts();  // ResetInterrupts, and the re-arming of an already expired timer
	void master_pending(u16 source) { m_mcipd = u16(m_mcipd | source); }
	void run_dma();
	// The mixing gain for one encoding word, left and right. `pan` is the five-bit field: its bit 4 picks
	// WHICH channel is attenuated, the other staying at full level.
	void gains(unsigned tl, unsigned pan, unsigned sdl, s32 &g, s32 &d) const
	{
		const unsigned i = (tl & 0xFF) | ((pan & 0x1F) << 8) | ((sdl & 7) << 13);
		g = m_gain[i];
		d = m_gain[i ^ (0x10 << 8)];
	}

	// --- sound RAM, seen the way the chip sees it
	u8  read_ram_byte(u32 byte) const;
	u16 read_ram_word(u32 byte) const;

	// --- the oscillator
	s32 one_slot(unsigned i);
	void compute_step(unsigned i);
	void key_on(unsigned i);
	void stop(unsigned i, bool keyoff);

	u16 m_common[COMMON_WORDS] = {};
	s32 m_ring[RING_WORDS] = {};                  // s32: the modulation buffer carries samples
	// COEF, MADRS and MPRO live IN the DSP: keeping a copy here would be two truths for one register.
	dsp_own m_dsp;
	u16 m_madrs_high[DISPUTED_WORDS] = {};

	// The master side's two interrupt registers, which the READ copies out of these fields and not the
	// other way round (scsp_contract.h §2 and §12).
	u16 m_mcieb = 0, m_mcipd = 0;

	// The incoming MIDI queue. Thirty-two bytes, as the contract observes.
	static constexpr u32 MIDI_DEPTH = 32;
	u8  m_midi[MIDI_DEPTH] = {};
	u32 m_midi_w = 0, m_midi_r = 0;                  // write, read

	// The interrupt levels, ASSEMBLED bit by bit from SCILV0/1/2 and recomputed only when one of those
	// three words is written (scsp_contract.h §12 point 3).
	int m_lvl_tima = 0, m_lvl_timbc = 0, m_lvl_midi = 0, m_lvl_cpu = 0, m_lvl_dma = 0;

	u8 m_mslc = 0;                                   // the monitored slot, taken when 0x408 is written
	std::uint64_t m_now = 0;                         // samples produced since reset()
	u32 m_clock;
	u16 *m_ram;
	u32 m_ram_bytes;
	scsp::sink &m_sink;
	bool m_mirror = true;

	// --- the accesses, in bytes, as the register window sees them
	u16  read_at(u32 byte);
	void write_at(u32 byte, u16 data, u16 mask);
	void after_common_write(u32 low);             // `bas` = octet & 0x3F
	void after_slot_write(u32 slot, u32 word);
	void levels_from_scilv();
	u16  monitored_slot_state() const;              // what 0x408 returns, recomputed on every read
};
