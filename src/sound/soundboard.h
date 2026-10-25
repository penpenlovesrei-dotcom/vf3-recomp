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
	// and announcer ask for channels 0x0F to 0x12 (requests 0x11 to 0x14), on the second SCSP.
	void set_volumes(float music, float effects);
	// Whether this game's sound driver is the one group_of() reads. Virtua Fighter 3's is; for another game the
	// records at 0x400 are something else, and reading them could put voices in the wrong group by coincidence,
	// so every voice is the music's and the two volumes move together.
	bool driver_known = true;
	// Whether the DSP's MADRS table is mirrored at 0x7C0-0x7FF (scsp.h). Measured right for Virtua
	// Fighter 3, where it keeps the reverb as short as the reference's, and WRONG for Fighting Vipers 2,
	// where it collapses the DSP's delay lines onto one address. Set from the set, like driver_known.
	bool madrs_mirror = true;

	uint64_t cycles() const { return now; }
	uint32_t pc() const { return cpu->pc(); }
	uint16_t ram_word(int n, uint32_t i) const { return ram[n][i]; }
	uint16_t reg_word(int n, uint32_t i);   // an SCSP's register word (tests: the slots' 0x000-0x3FF)

private:
	struct Bus;
	std::unique_ptr<Bus> bus;
	std::unique_ptr<m68000_device> cpu;
	std::unique_ptr<scsp_device> scsp[2];
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
