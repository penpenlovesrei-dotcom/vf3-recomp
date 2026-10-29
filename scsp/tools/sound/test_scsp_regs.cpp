// test_scsp_regs: what a write to an SCSP register reads back as.
//
// Block 1 of docs/scsp_architecture.md §3 -- "Registers and access semantics [...] Tested with no sound at
// all: write, read back" -- and the migration plan wants these tests WRITTEN BEFORE the blocks they test.
// So there is no standalone register bench yet: this file describes what MAME's core returns today,
// through the new interface, so that a future implementation has a target and not an intention.
//
// WHAT IT MEASURES: for every word from 0x000 to 0x42F, TWO masks -- what a write RAISES, and what STAYS
// raised when zero is written back. We write 0x0000 and read back, then 0xFFFF and read back, then 0x0000
// and read back. A bit that always reads back as 1 or always as 0 is not returned by the register, it is
// wired; a bit that goes up and does not come down is a bit the write SETS, and there are some (SCIPD,
// MCIPD).
//
// WHAT IT DOES NOT TOUCH, and why -- writing blind into a chip has side effects:
//   0x416  DEXE, which STARTS a DMA transfer and would write into sound RAM;
//   0x400  MVOL and company, which change the output of everything else;
//   the slots' word 0x0, whose bit 12 (KYONEX) triggers all 32 voices.
// Those three are named rather than silently skipped, and the final count checks it.
//
// AND IT IS A TEST WITH A RECORDED REFERENCE. The masks below were MEASURED on MAME's core on 2026-10-26,
// then frozen here. So the test does not prove they are right with respect to the hardware -- it proves
// they have not changed. That is exactly what one wants of a safety net before rewriting a block, and it
// is no more than that.
#include "../../src/sound/scsp_contract.h"
#include "../../src/sound/scsp_select.h"

#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace {

int failures = 0;

void ok(const char *what, const std::string &d) { printf("  ok     %-40s %s\n", what, d.c_str()); }
void fail(const char *what, const std::string &d) { printf("  FAILED %-40s %s\n", what, d.c_str()); ++failures; }

struct Silent final : scsp::sink {
	void irq(int, bool) override {}
	void midi_out(std::uint8_t) override {}
	void timer(int, std::uint32_t, std::uint32_t) override {}
};

constexpr std::uint32_t RAM_BYTES = 0x100000;

// The words we do not write, with the reason.
bool untouchable(std::uint32_t byte, const char *&pourquoi)
{
	if (byte < 0x400 && (byte & 0x1F) == 0x00) { pourquoi = "a slot's word 0: KYONEX triggers the voices"; return true; }
	if (byte == 0x400) { pourquoi = "MVOL, MEM4MB, DAC18B: change the output of everything else"; return true; }
	if (byte == 0x416) { pourquoi = "DEXE: starts a DMA and would write into sound RAM"; return true; }
	return false;
}

// The MADRS mirror setting is NOT in scsp::core, and scsp_core.h says why: it belongs to the port and not
// to the chip. So it goes through scsp_select.h, which makes it answer through the REGISTERED core -- this
// was the third copy of the same dynamic_cast in this repository, and it linked MAME into this test for
// nothing.
bool set_mirror(scsp::core &c, bool on) { return scsp::set_madrs_mirror_on(c, on); }

} // namespace

int main(int argc, char **argv)
{
	const char *name = argc > 1 ? argv[1] : "mame";
	printf("test_scsp_regs: what a write reads back as, through scsp::core -- core \"%s\"\n\n", name);

	std::vector<std::uint16_t> ram(RAM_BYTES / 2, 0);
	Silent silent;
	std::unique_ptr<scsp::core> p = scsp::make(name, 45158400 / 2, ram.data(), RAM_BYTES, silent);
	if (!p) return 2;
	scsp::core &the_core = *p;
	the_core.reset();

	// ---- 1. each word's useful mask, measured
	//
	// TWO MASKS AND NOT ONE, because a register does not necessarily follow the write in both directions.
	// The first version of this loop did `hauts & ~bas` after writing 0xFFFF then 0x0000, and it returned
	// 0x0000 for SCIPD and MCIPD -- which looks like "no bit reads back" when in fact those two are
	// STICKY: the implementation ORs them ("SCIPD and MCIPD are r/o except for bit 5"), so writing 0 does
	// not clear them, and the subtraction was erasing precisely the bit being looked for. So we measure
	// what GOES UP on a write, and separately what STAYS raised when zero is written back.
	const std::size_t N_MOTS = 0x430 / 2;
	std::vector<std::uint16_t> raises(N_MOTS, 0), sticks(N_MOTS, 0);
	unsigned skipped = 0;
	for (std::uint32_t byte = 0; byte < 0x430; byte += 2) {
		const char *pourquoi = nullptr;
		if (untouchable(byte, pourquoi)) { ++skipped; continue; }
		const std::uint32_t word = byte / 2;
		the_core.write_reg(word, 0x0000, 0xFFFF);
		const std::uint16_t a0 = the_core.read_reg(word);
		the_core.write_reg(word, 0xFFFF, 0xFFFF);
		const std::uint16_t a1 = the_core.read_reg(word);
		the_core.write_reg(word, 0x0000, 0xFFFF);
		const std::uint16_t a2 = the_core.read_reg(word);
		raises[word] = std::uint16_t(a1 & ~a0);        // what a write raises
		sticks[word] = std::uint16_t(a1 & a2 & ~a0);   // ... and that does not come down
	}
	ok("sweep from 0x000 to 0x42F", std::to_string(0x430 / 2 - skipped) + " words written and read back, "
	   + std::to_string(skipped) + " left aside");

	// ---- 2. the three set aside are indeed the ones named, and no others
	{
		unsigned slots = 0, others = 0;
		for (std::uint32_t byte = 0; byte < 0x430; byte += 2) {
			const char *p = nullptr;
			if (!untouchable(byte, p)) continue;
			if (byte < 0x400) ++slots; else ++others;
		}
		const bool correct = slots == 32 && others == 2 && skipped == slots + others;
		correct ? ok("the ones set aside are named", std::to_string(slots) + " slot word 0s + 0x400 + 0x416")
		      : fail("the ones set aside are named",
		             std::to_string(slots) + " slots and " + std::to_string(others) + " others, for "
		             + std::to_string(skipped) + " skipped");
	}

	// ---- 3. the interrupt-pending registers: read-only EXCEPT bit 5
	//
	// It is written out in full in the implementation -- "SCIPD and MCIPD are r/o except for bit 5 CPU
	// irqs" -- and it is the kind of thing a rewrite forgets.
	//
	// BUT THE COMMENT AND THE CODE DO NOT SAY THE SAME THING, and it is this measurement that showed it.
	// The exception is applied to 0x420 and 0x42E; but 0x42E is not MCIPD, it is MCIRE -- MCIPD is at
	// 0x42C (UpdateRegR case 0x2c copies m_mcipd there, UpdateReg case 0x2e does
	// `m_mcipd &= ~data[0x2e/2]`). So all three words do carry a sticky bit 5, by three different paths:
	//   0x420 SCIPD: the write is an OR, the bit stays. The intended exception.
	//   0x42C MCIPD: plain assignment, then UpdateReg sees bit 5 and calls MainCheckPendingIRQ(0x20),
	//                which raises m_mcipd; the read back returns m_mcipd, so the bit comes back.
	//   0x42E MCIRE: receives the exception MEANT FOR MCIPD, so its bit 5 can never be cleared again,
	//                while its twin SCIRE (0x422) is an ordinary register (mask 0xFFFF).
	// What this costs VF3: nothing, and that is measured -- the driver touches neither. The detail and
	// the search that establishes it are in scsp_contract.h, section 2.
	// We record all three as they are: the first version of this test called 0x42E "MCIPD" and passed for
	// the wrong reason. A rewrite will decide to reproduce the asymmetry or to correct it, but it will
	// decide while looking at it.
	for (auto [byte, name] : { std::pair<std::uint32_t, const char *>{ 0x420, "SCIPD (0x420)" },
	                           std::pair<std::uint32_t, const char *>{ 0x42C, "MCIPD (0x42C)" },
	                           std::pair<std::uint32_t, const char *>{ 0x42E, "MCIRE (0x42E)" } }) {
		const std::uint16_t mo = raises[byte / 2], co = sticks[byte / 2];
		char d[64];
		snprintf(d, sizeof d, "raises 0x%04x, sticks 0x%04x", mo, co);
		// And bit 5 STICKS: it is a write that SETS it, not one that copies it. A rewrite doing an
		// assignment would pass the first half of this test and fail the second.
		(mo == 0x0020 && co == 0x0020)
			? ok(name, std::string(d) + ": bit 5 alone, and it stays raised")
			: fail(name, std::string(d) + ", expected raises and sticks 0x0020");
	}

	// ---- 3 bis. SCIRE, the twin that does NOT have the exception
	// The previous case's asymmetry is checked from its other end: on the slave side it is SCIPD that is
	// ORed, on the master side it is MCIRE. So SCIRE must be a whole register.
	{
		const std::uint16_t mo = raises[0x422 / 2];
		char d[64];
		snprintf(d, sizeof d, "raises 0x%04x", mo);
		(mo == 0xFFFF) ? ok("SCIRE (0x422)", std::string(d) + ": a whole register, no exception on this side")
		               : fail("SCIRE (0x422)", std::string(d) + ", expected 0xFFFF");
	}

	// ---- 3 ter. the two words the READ manufactures
	// Those do not return what is written to them because a read recomputes them (UpdateRegR). This is a
	// known divergence from the hardware and not an implementation detail: for 0x408, the Model 3's driver
	// writes MSLC and reads straight back to find a free slot, and this core returns it the state of the
	// chosen slot -- deliberate behaviour, see UpdateRegR case 8's comment in scsp.cpp.
	{
		const std::uint16_t m404 = raises[0x404 / 2], m408 = raises[0x408 / 2];
		char d[160];
		snprintf(d, sizeof d, "0x404 raises 0x%04x (the low byte is the incoming MIDI queue), "
		                      "0x408 raises 0x%04x (MSLC/CA/SGC/EG, recomputed on every read)", m404, m408);
		(m404 == 0xFF00 && m408 == 0x0000)
			? ok("0x404 and 0x408: the read manufactures them", d)
			: fail("0x404 and 0x408: the read manufactures them", std::string(d) + ", expected 0xFF00 and 0x0000");
	}

	// ---- 4. the fields the driver depends on all read back
	// The contract says where each field lives (scsp_contract.h); here we check that the register KEEPS
	// it. A field that does not read back is a field a rewrite can lose with nothing to shout about it.
	{
		std::string lost;
		for (const scsp_contract::Field &c : scsp_contract::SLOT) {
			if (c.word == 0) continue;   // word 0 is not written (KYONEX)
			const bool in_place = (c.mask & (c.mask + 1)) != 0;
			const std::uint16_t expected = std::uint16_t(in_place ? c.mask : (c.mask << c.shift_by));
			// slot 0: word c.word is at byte 2 * c.word
			const std::uint16_t m = raises[c.word];
			if ((m & expected) != expected) lost += std::string(lost.empty() ? "" : ", ") + c.name;
		}
		lost.empty() ? ok("a slot's 34 fields all read back", "no bit lost by the register")
		               : fail("a slot's 34 fields all read back", "lost: " + lost);
	}

	// ---- 5. the disputed range 0x7C0-0x7FF, described and not judged
	// This port and MAME do not do the same thing with it (docs/scsp_references.md), and no reading of the
	// program can settle what the CHIP does (docs/fv2-sound-program.md §5). So we record what today's core
	// returns, without making it a truth.
	//
	// AND WE MEASURE BOTH SETTINGS, not only the default one. m_madrs_mirror comes up `true`, so that a
	// test writing 0x7C0 and reading 0x780 finds the value and seems to describe the chip when all it
	// describes is a construction default. The setting belongs to the game and not to the chip (scsp.h),
	// so it is set through the object and not through the interface -- the only place in this file that
	// goes below scsp::core, and that is owned: this test characterises THAT core.
	{
		std::string lines;
		bool correct = true;
		for (bool mirror : { false, true }) {
			set_mirror(the_core, mirror);
			the_core.write_reg(0x7C0 / 2, 0x1234, 0xFFFF);
			the_core.write_reg(0x780 / 2, 0x5678, 0xFFFF);   // MADRS[0] by the low route, afterwards
			const std::uint16_t high = the_core.read_reg(0x7C0 / 2);
			const std::uint16_t low = the_core.read_reg(0x780 / 2);
			char t[128];
			snprintf(t, sizeof t, "%smirror %s: 0x7C0 reads 0x%04x, 0x780 reads 0x%04x",
			         lines.empty() ? "" : " | ", mirror ? "on " : "off", high, low);
			lines += t;
			// Mirror off: 0x7C0 has its own storage, so it keeps 0x1234 and 0x780 keeps 0x5678.
			// Mirror on: both are the same word, so the second write wins both times.
			correct = correct && (mirror ? (high == 0x5678 && low == 0x5678) : (high == 0x1234 && low == 0x5678));
		}
		set_mirror(the_core, true);   // we hand the core back in the state we took it in
		correct ? ok("0x7C0-0x7FF: what this core does with it", lines)
		      : fail("0x7C0-0x7FF: what this core does with it", lines + " -- not what was recorded");
	}

	// ---- 6. the map of the masks, so that a rewrite has a target and not an intention
	//
	// BOUNDED, and that did not go without saying: the first version of this loop went up to 0x43E while
	// the array stops at 0x42E, and printed the memory after it -- 374b, c473, 44d0, which were the
	// registers of nothing. An overrun returns figures, and figures look like a measurement.
	printf("\n-- what a write raises, by word (.... = untouchable, * = sticky) --\n");
	for (std::uint32_t byte = 0; byte < 0x430; byte += 0x20) {
		printf("  %03x ", byte);
		for (std::uint32_t k = 0; k < 0x20 && byte + k < 0x430; k += 2) {
			const std::uint32_t word = (byte + k) / 2;
			const char *p = nullptr;
			if (untouchable(byte + k, p)) printf("  .... ");
			else printf("  %04x%c", raises[word], sticks[word] ? '*' : ' ');
		}
		printf("\n");
	}

	printf("\n");
	if (failures) { printf("%d failure(s).\n", failures); return 1; }
	printf("all cases pass\n");
	return 0;
}
