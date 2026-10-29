// banc_scsp.h: what it takes to key a voice and read what comes out, for the SCSP's block tests.
//
// The blocks of docs/scsp_architecture.md §3 are all tested the same way -- a fresh core, a RAM, a slot
// laid down through the contract's table, and n samples read -- and there are seven of them. This file
// exists so that the second version of that rig is the last: test_scsp_fentes.cpp (block 2) had it of its
// own, test_scsp_adsr.cpp (block 3) needed it identically.
//
// IT CONTAINS NO EXPECTATION. What is expected of a block belongs to that block's test; here there is only
// the means of measuring. A bench carrying references would become a second place where the truth about
// the chip is written.
#pragma once

// NO CONCRETE CLASS HERE, and that is the point: the contract, the interface and the selector are enough.
// This file used to include scsp_maison.h AND scsp_mame.h for the two switches at the bottom, so that the
// eight block tests linked MAME's core through it -- for no substantive reason, their references being
// frozen numbers. Cut on 2026-10-28; `make test-sans-mame` is the measurement that it is over.
#include "../../src/sound/scsp_contract.h"
#include "../../src/sound/scsp_select.h"

#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace banc {

// A sink that listens to nothing: the interrupts, the MIDI and the timers are other blocks.
struct Muet final : scsp::sink {
	void irq(int, bool) override {}
	void midi_out(std::uint8_t) override {}
	void timer(int, std::uint32_t, std::uint32_t) override {}
};

inline constexpr std::uint32_t RAM_OCTETS = 0x100000;   // 1 MB, like one chip on the board

// A fresh core with its RAM.
//
// A FRESH BENCH PER CASE, and it is an obligation and not a precaution: an SDIR slot does not go out. Key-
// off only moves the envelope into RELEASE (StopSlot), and SDIR skips the envelope -- the slot would go on
// sounding into the following cases. Building one core per case costs a page of memory and guarantees that
// no case depends on the state the previous one left.
// THE NAME OF THE CORE THE BENCHES BUILD. One single place, set by each test's main() from its first
// argument: that is what lets the same test judge MAME's core and the project's. "mame" by default, so
// that a test run without an argument measures the reference.
inline const char *&nom_du_coeur()
{
	static const char *n = "mame";
	return n;
}

struct Banc {
	// Parentheses and not braces: `{n, 0}` would build a vector of TWO elements.
	std::vector<std::uint16_t> ram = std::vector<std::uint16_t>(RAM_OCTETS / 2, 0);
	Muet muet;
	std::unique_ptr<scsp::core> tenu = scsp::make(nom_du_coeur(), 45158400 / 2, ram.data(), RAM_OCTETS, muet);
	scsp::core &coeur = *tenu;

	Banc()
	{
		if (!tenu) { fprintf(stderr, "banc_scsp: unknown core\n"); std::exit(2); }
		coeur.reset();
		// 0x400 as the driver writes it at startup (MEM4MB, DAC18B), MVOL at full. MEM4B has no effect on
		// this core -- read_byte/read_word mask to 0xFFFFF outright, without consulting it -- but it is
		// written all the same so that the chip is in the state the game puts it in.
		coeur.write_reg(0x400 / 2, 0x0300 | 0x000F, 0xFFFF);
	}

	// A ramp of 16-bit samples at byte address `sa`: sample k is k * pas.
	void rampe(std::uint32_t sa, unsigned n, int pas)
	{
		for (unsigned k = 0; k < n; k++)
			ram[(sa / 2) + k] = std::uint16_t(std::int16_t(int(k) * pas));
	}

	// A constant: n samples of the same value. It is the vehicle for the blocks that measure a GAIN
	// (envelope, amplitude LFO, TL) -- the output is then the gain, up to a factor.
	void continu(std::uint32_t sa, unsigned n, int valeur)
	{
		for (unsigned k = 0; k < n; k++)
			ram[(sa / 2) + k] = std::uint16_t(std::int16_t(valeur));
	}

	// Lays down a slot's 16 words, then pulls the trigger. KYONEX goes LAST and alone, because it sets off
	// a sweep of all 32 slots (UpdateSlotReg case 0) and not just the one being written.
	void keye(unsigned fente, const std::uint16_t mot[16])
	{
		const std::uint32_t base = fente * 0x10;   // 0x20 bytes per slot, hence 0x10 words
		for (unsigned w = 1; w < 16; w++)
			coeur.write_reg(base + w, mot[w], 0xFFFF);
		coeur.write_reg(base + 0, mot[0], 0xFFFF);
		std::uint16_t dec[16] = {};
		scsp_contract::pose(dec, "KYONEX", 1);
		coeur.write_reg(base + 0, std::uint16_t(mot[0] | dec[0]), 0xFFFF);
	}

	// n samples, left channel only (the output is interleaved L,R).
	//
	// CAREFUL: this call ADVANCES the chip. Keep its result in a variable rather than calling it twice --
	// once to compare, once to print -- otherwise the message shows the next sample beside a test that
	// passes. It has happened.
	std::vector<std::int32_t> gauche(unsigned n)
	{
		std::vector<std::int32_t> out(std::size_t(n) * 2, 0);
		coeur.run(n, out.data());
		std::vector<std::int32_t> g(n);
		for (unsigned i = 0; i < n; i++) g[i] = out[std::size_t(i) * 2];
		return g;
	}

	// Both channels, for the block that measures the pan: half of DIPAN's and EFPAN's facts are on the
	// channel `gauche()` throws away.
	std::pair<std::int32_t, std::int32_t> stereo(unsigned n)
	{
		std::vector<std::int32_t> out(std::size_t(n) * 2, 0);
		coeur.run(n, out.data());
		return { out[std::size_t(n - 1) * 2], out[std::size_t(n - 1) * 2 + 1] };
	}
};

// Lays down a slot field, and dies if the name does not exist: a name absent from the contract's table is
// a typo, not a condition to handle.
inline void pose(std::uint16_t mot[16], const char *nom, unsigned valeur)
{
	if (!scsp_contract::pose(mot, nom, valeur)) {
		fprintf(stderr, "banc_scsp: the contract has no field named %s\n", nom);
		std::exit(2);
	}
}

// A sequence of values divided by a known step, the non-multiples shown as they are rather than rounded.
inline std::string suite(const std::vector<std::int32_t> &v, int diviseur)
{
	std::string s;
	for (std::size_t i = 0; i < v.size(); i++) {
		if (i) s += ",";
		if (diviseur != 0 && v[i] % diviseur == 0) s += std::to_string(v[i] / diviseur);
		else s += "?" + std::to_string(v[i]);
	}
	return s;
}

// THE TWO THINGS THAT ARE NOT IN scsp::core, and that the tests nevertheless have to reach: the MADRS
// mirror and the master volume. scsp_core.h says why they are not in it -- they belong to the port and not
// to the chip -- and these two switches are the MEASUREMENT of what the interface still lacks. The day
// they disappear, the debt is paid.
//
// THEY ARE NO LONGER WRITTEN HERE. They have moved to scsp_select.h, which makes them answer through the
// REGISTERED CORE and not through a dynamic_cast written on the spot: that is what lets this bench name no
// concrete class. Both names remain, because the tests use them and this file is their vocabulary.
inline bool pose_miroir(scsp::core &c, bool on) { return scsp::regle_miroir_madrs(c, on); }
inline float gain_expose(const scsp::core &c) { return scsp::gain_expose(c); }

} // namespace banc
