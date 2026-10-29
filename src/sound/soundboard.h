// license:BSD-3-Clause
// copyright-holders:R. Belmont, Ville Linde
// The Model 3 sound board: a 68000 at 11.2896 MHz with two SCSPs (MAME's, src/sound/scsp.h), each with 1 MB of
// RAM, and the main board's MIDI bytes going into the first SCSP's MIDI input.
//
// The 68000's map: 000000 the first SCSP's RAM, 100000 its registers, 200000 the second's RAM, 300000 its
// registers, 400001 the bank register (bit 4: the samples' upper 8 MB), 600000 the program ROM, 800000-FFFFFF
// the samples (banked). The SCSPs play only from their RAM; the 68000 copies samples there. Only the first
// SCSP's interrupts reach the 68000. The board runs a video frame at a time, 256 68000 cycles a sample at
// 44.1 kHz.
#pragma once
#include "scsp.h"
// (no more scsp_own.h: the MADRS mirror used to go through a dynamic_cast written here, it now goes
// through scsp_select.h, so the board no longer names anything but the core it draws its three extras
// from -- MAME's)
#include "scsp_mame.h"
#include "scsp_select.h"
#include "scsp_trace.h"
#include "../../third_party/mame/m68000/m68000.h"
#include <cstdint>
#include <memory>
#include <vector>

class SoundBoard
{
public:
	SoundBoard();
	~SoundBoard();
	// rom and samples: 16-bit words in host order
	void attach(const std::vector<uint8_t> *rom, const std::vector<uint8_t> *samples);
	void reset();
	void midi_in(uint8_t byte);
	// Runs for `seconds`; the stereo samples made are added to out (16-bit, interleaved)
	void run(double seconds, std::vector<int16_t> &out);
	double gain = 1.0;   // the host's volume (over 1: limited)
	// The music's and the effects' own volumes (0 to 1; at 1 both, the board's mix untouched). VF3's sound driver
	// keeps a record a voice, 32 bytes at 0x400 + slot * 0x20 in each SCSP's RAM: the slot's register address,
	// then the channel the voice plays for. The music plays on channels 0x00 to 0x0E; the game's effects, voices
	// and announcer ask for channels 0x0F to 0x13, on the second SCSP.
	//
	// All three of those were deductions until the sound program was read (docs/fv2-sound-program.md): the
	// record's first long is written at reset by a loop over 32 slots, stride 0x20, from 0x00100000 for the
	// first chip and 0x00300000 for the second, which is exactly what group_of() tests. And byte +5 is not a
	// MIDI channel at all -- it is (A0 - 0xC40) / 0x20, the index of an entry in a table of 0x20-byte channel
	// objects based at 0xC40, which is why it can exceed 15. That table is laid out RIGHT-ALIGNED to end at
	// 0xEC0, with room for 20 entries, and this driver's ROM declares five: they land at indices 0x0F to 0x13,
	// leaving 0x00 to 0x0E to the music. So the 0x0F threshold is a consequence of the layout, not a magic
	// number -- and the range here said 0x0F to 0x12, one short of the five that are there. The test is a >=,
	// so it caught them all and nothing was broken; the comment was wrong, not the code.
	//
	// And the WRITE was found the 2026-10-25, so none of this rests on reading the readers any more. The
	// note-on tail at 0x6010EA does `move.w A0,D0 / subi.w #$C40,D0 / lsr.w #$5,D0 / move.b D0,$5(A4)` --
	// the same (A0 - 0xC40) >> 5 the control-change handlers use as their key, so key and value are computed
	// by identical code. The same read turned up a field this comment did not know: byte +6 is the voice's
	// MIDI CHANNEL, written at 0x600C5A as `move.b $1(A0),$6(A4)` from the channel object's +1. A record
	// therefore carries both identifiers, and they are different numberings -- 20 objects, 16 MIDI channels.
	// The volume handler (CC 7) keys on +6; modulation (CC 1) and pan (CC 10) key on +5, like group_of().
	void set_volumes(float music, float effects);
	// Whether this game's sound driver is the one group_of() reads. Virtua Fighter 3's is; for another game the
	// records at 0x400 are something else, and reading them could put voices in the wrong group by coincidence,
	// so every voice is the music's and the two volumes move together.
	bool driver_known = true;
	// Whether the DSP's MADRS table is mirrored at 0x7C0-0x7FF (scsp.h: m_madrs_mirror). Measured right
	// for Virtua Fighter 3, where it keeps the reverb as short as the reference's, and measured WRONG for
	// Fighting Vipers 2, where it collapses the DSP's delay lines onto one address and the victory
	// screen's sound feeds itself (docs/fv2-console-sound.md). Set from the set, like driver_known.
	bool madrs_mirror = true;

	uint64_t cycles() const { return now; }
	uint32_t pc() const { return cpu->pc(); }
	uint16_t ram_word(int n, uint32_t i) const { return ram[n][i]; }
	uint16_t reg_word(int n, uint32_t i);   // an SCSP's register word (tests: the slots' 0x000-0x3FF)
	// WHICH VOICE IS MAKING THE NOISE. The chips carry the measurement already (scsp.h: m_probe_direct, the
	// energy each slot sent out since it was last read); nothing turned it on. energy_on arms both, and
	// energy_take empties them into e[chip][slot] -- so a caller can ask, frame by frame, where the sound is
	// coming from instead of guessing from the total. Written for Fighting Vipers 2's victory screen, where a
	// sound is born and swells 24 dB with no voice, envelope or effect to account for it (2026-10-24).
	void energy_on(bool on);
	void energy_take(double e[2][32]);

private:
	struct Bus;
	std::unique_ptr<Bus> bus;
	std::unique_ptr<m68000_device> cpu;
	// THE TWO CHIPS, THROUGH THEIR INTERFACE ALONE (src/sound/scsp_core.h), and the core is chosen:
	// VF3_SCSP_CORE=mame (the default) or own, the project's SCSP (src/sound/scsp_own.h).
	std::unique_ptr<scsp::core> scsp[2];

	// WHAT THE BOARD ASKS FOR BEYOND THE CHIP, and which is not in the interface because it is not a
	// SCSP's business: the per-group gains of the music/effects split, the energy probe, the key-on log.
	// `scsp_core.h` says why they are not there.
	//
	// THEY ARE IN MAME'S CORE ALONE FOR NOW, and this pointer -- non-owning, null when the other core is
	// running -- is the MEASURE of what is left to do. The per-group gains are the only one of the three
	// that matters to the player, and they are INERT as long as both volumes are 1
	// (scsp_device::set_group_gain): that is why the project's core is usable without them, and why the
	// board says so instead of letting them fall silently.
	scsp_mame *extras[2] = { nullptr, nullptr };
	// THE HEAD THROUGH WHICH THE BOARD SPEAKS TO THE CHIP, and it is `head[n]` and not `scsp[n]` that must
	// be called.
	//
	// Without VF3_SCSP_TRACE it is scsp[n].get() and costs nothing. With it, it is a scsp::recorder that
	// wraps the chip and notes every call (src/sound/scsp_trace.h), so that the trace goes through ONE door
	// instead of being laid at eight places -- and so that a ninth call added one day cannot escape it
	// silently. The `->device()` calls stay on scsp[n]: they are not the chip's business.
	std::unique_ptr<scsp::recorder> trace[2];
	scsp::core *head[2] = { nullptr, nullptr };
	// What the board plugs underneath each chip: three methods instead of five std::function.
	struct Sink final : scsp::sink {
		SoundBoard *b = nullptr;
		int n = 0;
		void irq(int level, bool on) override;
		void midi_out(std::uint8_t byte) override;
		void timer(int which, std::uint32_t samples, std::uint32_t period) override;
	};
	Sink sinks[2];
	// The frame's samples, accumulated by sync_sound() as the register accesses come, and mixed at the end.
	// In signed integers on the 18-bit scale (scsp_mame::FULL_SCALE), as the chip has them.
	std::vector<std::int32_t> pcm[2];
	// Brings the two chips up to the 68000's time. TO BE CALLED BEFORE ANY REGISTER ACCESS: the old class
	// did it of its own accord (its write() began with update()), the new interface does not -- that is the
	// price of pushed time, and §2 of the architecture says so: "the caller guarantees that time is up to
	// date before touching it". Without this call the whole frame would be made in one go at the end and the
	// writes would no longer fall at the same place in the sound.
	// ONE CHIP ONLY, the one being touched. The old path advanced that one alone (its write() called update()
	// on it alone); bringing BOTH up to date at every access makes a sample of the other one too early, and
	// the harness saw it: one pair lost out of 4200 frames and the whole sound shifted by one sample from
	// frame 1006 on, where the driver loads a DSP program in 2450 writes.
	void sync_sound(int n);
	void sync_sound() { sync_sound(0); sync_sound(1); }
	// How many MIDI bytes the board has thrown away (it does nothing with them today), counted rather than
	// kept quiet.
	std::uint64_t midi_out_dropped = 0;
	std::vector<uint16_t> ram[2];
	int frames = 0;   // run() calls (tests' logs)
	const std::vector<uint8_t> *rom = nullptr, *samples = nullptr;
	uint8_t ctrl = 0;
	uint64_t now = 0;           // 68000 cycles since reset (at the start of the current run)
	bool running = false;
	double frac = 0, limit = 1.0;
	float volume[2] = { 1, 1 };   // the music's, the effects'
	int group_of(int n, int slot) const;
	struct Timer { bool on = false; uint64_t at = 0, period = 0; };
	Timer timers[2][scsp_device::N_TIMERS];
	uint64_t cycles_now() const;
	void fire_due();
	friend struct Bus;
};
