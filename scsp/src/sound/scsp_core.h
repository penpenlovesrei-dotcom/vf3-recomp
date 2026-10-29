// scsp_core.h -- the interface an SCSP implementation presents to this port's sound board.
//
// This is step 3 of docs/scsp_migration_plan.md, and the plan is explicit that it comes FIRST, before the
// replayer and before any autonomous code: "the interface laid on MAME's core first -- an adapter that
// presents the old core through the new contract. It is the move that decides whether the contract holds,
// and it costs nothing to undo."
//
// The shape below is the one docs/scsp_architecture.md section 2 designed, implemented as designed rather
// than redesigned here. Its one rule: THE CORE KNOWS NEITHER THE PC, NOR MAME, NOR THE 68000. It knows
// samples, registers and RAM.
//
// WHAT BELONGS HERE AND WHAT DOES NOT. This file is the chip's interface. What the chip must DO with those
// registers -- the field positions, the interrupt levels, the timer the music hangs off, the pitch table's
// unit -- is src/sound/scsp_contract.h, checked by tools/sound/test_scsp_contract.cpp. The two are meant to be
// read together: this one is the shape, that one is the behaviour.
//
// AND WHAT IS DELIBERATELY ABSENT. The port's own additions are not in this interface: the per-group gains
// that make the music/effects split, the energy probe, the MADRS mirror switch. They are diagnostics and
// mixing of this port's, not things a Model 3 sound board asks of an SCSP, and an autonomous core should not
// have to implement them to be usable. During the changeover the board keeps reaching the old class directly
// for those; that is what scsp_mame::device() is for, and it is the part meant to go away last.
#pragma once

#include <cstdint>

namespace scsp {

using u8  = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using s32 = std::int32_t;

// What the board plugs underneath. Three methods, called on rare events, instead of five std::function
// consulted on every register access.
struct sink {
	virtual ~sink() = default;

	// One interrupt level, raised or dropped. The board decides what to do with it; on this board only the
	// first chip is wired to the 68000.
	//
	// AND THIS IS WHERE THE DESIGN DID NOT HOLD, which is what step 3 exists to find out. The architecture
	// (docs/scsp_architecture.md section 2) proposed `irq(int level)` with 0 for none -- "the level asked
	// for, not one line at a time" -- and that is one method too few. Read in src/sound/scsp.cpp on
	// 2026-10-26:
	// the chip drops FOUR different lines independently (m_IrqTimA, m_IrqTimBC, m_IrqCPU, m_IrqMidi, plus
	// line 0), and it can drop one while another is still raised. A single "the level I am asking for" cannot
	// say which one went away, and a board that guessed would clear the wrong one.
	//
	// So the shape here is level + on/off. The proposal was right that FIVE std::function were four too
	// many; it was wrong that the levels collapse into one number. Kept as found rather than argued away.
	virtual void irq(int level, bool on) = 0;

	// One byte out, not one bit. The Model 3's link carries bytes; the bit timing only ever mattered for the
	// interrupt cadence, which stays inside the core.
	//
	// NOTE, measured 2026-10-25/26 and worth knowing before implementing this: the driver uses MIDI out in
	// three places -- a 0xF0 every ten timer-A ticks, a per-track echo of every sequencer byte, and a MIDI
	// thru that returns every received byte (docs/fv2-sound-program.md). THIS PORT DISCARDS ALL THREE
	// (soundboard.cpp: h.midi_out = [](int) {}). Whether the main board reads them back has never been
	// checked, so an implementation should carry the byte rather than assume it is unwanted.
	virtual void midi_out(u8 byte) = 0;

	// Arm or disarm a timer, in SAMPLES -- the hardware's unit, so nothing has to be converted back and
	// forth through attoseconds. `period` 0 means one shot; `samples` 0 means disarm.
	virtual void timer(int which, u32 samples, u32 period) = 0;
};

class core {
public:
	virtual ~core() = default;

	virtual void reset() = 0;

	// The registers, by WORD offset from the chip's base. The caller guarantees time is up to date -- that
	// is, that it has run() everything owed -- before touching them.
	virtual u16  read_reg(u32 word_offset) = 0;
	virtual void write_reg(u32 word_offset, u16 data, u16 mem_mask) = 0;

	// Time, PUSHED: make exactly n stereo pairs into out, interleaved left then right, as the chip has them
	// before the DAC -- a signed value in +-0x20000 (18 bits) or +-0x8000 when the chip is set to 16.
	// `out` holds 2 * n values and belongs to the caller: nothing is allocated on this path.
	//
	// The caller chooses its slice. A sample at a time gives MAME's grain; wider costs less and the board
	// decides, once, in soundboard.cpp -- instead of the core having to ask what time it is.
	virtual void run(u32 n, s32 *out) = 0;

	virtual void timer_fired(int which) = 0;
	virtual void midi_in(u8 byte) = 0;

	// How many samples run() has made since reset. The board paces on it.
	virtual std::uint64_t samples_made() const = 0;
};

} // namespace scsp
