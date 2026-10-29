// test_scsp_slots: the oscillator, PCM addressing and the four loop modes, to the exact sample.
//
// Block 2 of docs/scsp_architecture.md §3 -- "Slots, oscillator, PCM, addressing, looping: signed 8 and 16
// bits, SA/LSA/LEA, the four LPCTL modes, linear interpolation, RAM's limits. Tested by counting samples."
// As for block 1, the test comes before the implementation: it says what MAME's core returns today,
// through the new interface, so that a rewrite has a target.
//
// HOW THE OSCILLATOR IS MEASURED ON ITS OWN. A test that keys an ordinary voice measures the oscillator AND
// the envelope AND the attenuator, and can assert nothing exactly. But the chip has a door for that: SDIR
// ("sound direct"). In UpdateSlot, `if (!SDIR(slot))` skips the envelope and the ALFO; in the mix,
// `SDIR(slot) ? 0 : TL(slot)` sets the attenuation to zero. With SDIR=1, DISDL=7 and DIPAN=0, the left
// output is therefore the oscillator's raw sample, up to an integer factor -- and that factor, the first
// case MEASURES instead of assuming.
//
// WHAT THAT MAKES IT POSSIBLE TO SAY, and that no reading of the code says as surely: the exact sequence of
// sample INDICES the slot walks through. That is the whole of block 2: the four loop modes are nothing but
// four sequences of indices, and a rewrite that changes one will know it here.
//
// The rig -- a fresh core per case, the RAM, the keying -- is in tools/sound/scsp_bench.h.
#include "scsp_bench.h"

#include <cstdio>
#include <string>
#include <vector>

namespace {

using bench::Bench;
using bench::put;
using bench::sequence;

int failures = 0;

void ok(const char *what, const std::string &d) { printf("  ok     %-38s %s\n", what, d.c_str()); }
void failed(const char *what, const std::string &d) { printf("  FAILED %-38s %s\n", what, d.c_str()); ++failures; }

// The settings common to every case: the oscillator bare, full direct output, centred.
void blank(std::uint16_t word[16], std::uint32_t sa)
{
	put(word, "SA_HI",  sa >> 16);
	put(word, "SA_LO",  sa & 0xFFFF);
	put(word, "SDIR",   1);     // skips the envelope, the ALFO and TL
	put(word, "DISDL",  7);     // full direct output
	put(word, "DIPAN",  0);     // centre
	put(word, "OCT",    0);     // with FNS=0: one source sample per output sample
	put(word, "FNS",    0);
	put(word, "KYONB",  1);
}

int FACTOR = 0;   // measured by case 1: left output / raw sample

} // namespace

int main(int argc, char **argv)
{
	if (argc > 1) bench::core_name() = argv[1];
	printf("test_scsp_slots: the oscillator and the loop modes, through scsp::core\n\n");

	// ---- 1. THE FACTOR, measured and not assumed
	// All the rest of this file depends on it, so it must not be a constant written from memory.
	{
		Bench b;
		for (unsigned k = 0; k < 16; k++) b.ram[0x1000 / 2 + k] = 1000;   // steady
		std::uint16_t word[16] = {};
		blank(word, 0x1000);
		put(word, "LPCTL", 1);
		put(word, "LSA", 0);
		put(word, "LEA", 16);
		b.key(0, word);
		const std::vector<std::int32_t> g = b.left(8);
		bool flat = true;
		for (std::int32_t v : g) if (v != g[0]) { flat = false; break; }
		FACTOR = (flat && g[0] % 1000 == 0) ? int(g[0] / 1000) : 0;
		(flat && FACTOR == 4)
			? ok("SDIR: the output is the raw sample",
			     "1000 in RAM comes out at " + std::to_string(g[0]) + ", that is a factor of "
			     + std::to_string(FACTOR) + " (131072 / 32768) over the 8 samples")
			: failed("SDIR: the output is the raw sample",
			         "expected 8 times 4000; got " + sequence(g, 1));
		if (FACTOR == 0) { printf("\nthe factor could not be measured: the rest would mean nothing.\n"); return 1; }
	}

	// ---- 2. LSA AND LEA COUNT SAMPLES, the access counts bytes
	// This is the block's asymmetry, and it is invisible on a reading: the 16-bit read takes
	// `(cur_addr >> (SHIFT-1)) & ~1`, that is TWO bytes per sample, while the loop comparison takes
	// `cur_addr >> SHIFT`, that is ONE index. If LEA were in bytes, the period would be 4 and not 8: it is
	// the measurement that settles it, not the reasoning.
	{
		Bench b;
		b.ramp(0x1000, 32, 100);
		std::uint16_t word[16] = {};
		blank(word, 0x1000);
		put(word, "LPCTL", 1);
		put(word, "LSA", 0);
		put(word, "LEA", 8);
		b.key(0, word);
		const std::string s = sequence(b.left(20), FACTOR * 100);
		const char *expected = "0,1,2,3,4,5,6,7,0,1,2,3,4,5,6,7,0,1,2,3";
		(s == expected) ? ok("LSA/LEA in samples, not in bytes",
		                    "LEA=8 gives a period of 8 indices: " + s)
		                : failed("LSA/LEA in samples, not in bytes",
		                         "expected " + std::string(expected) + "; got " + s);
	}

	// ---- 3. PCM8B: one byte per sample, the word's HIGH byte first, and shifted by 8
	// read_byte returns `m_ram[a >> 1] >> ((a & 1) ? 0 : 8)`, so byte 0 is the HIGH byte of word 0: RAM is
	// seen big-endian, like the 68000 that fills it. And the value goes out as `p1 << 8`: an 8-bit sample
	// occupies the high eight bits, it is not stretched over sixteen.
	{
		Bench b;
		b.ram[0x1000 / 2 + 0] = 0x1234;
		b.ram[0x1000 / 2 + 1] = 0x5678;
		std::uint16_t word[16] = {};
		blank(word, 0x1000);
		put(word, "PCM8B", 1);
		put(word, "LPCTL", 1);
		put(word, "LSA", 0);
		put(word, "LEA", 4);
		b.key(0, word);
		const std::vector<std::int32_t> g = b.left(4);
		// 0x12, 0x34, 0x56, 0x78 signed, each << 8, then the factor
		const bool correct = g[0] == 0x12 * 256 * FACTOR && g[1] == 0x34 * 256 * FACTOR
		                  && g[2] == 0x56 * 256 * FACTOR && g[3] == 0x78 * 256 * FACTOR;
		correct ? ok("PCM8B: high byte first, shifted by 8",
		             "0x1234 0x5678 give 0x12 0x34 0x56 0x78, that is " + sequence(g, FACTOR * 256))
		        : failed("PCM8B: high byte first, shifted by 8",
		                 "expected 18,52,86,120 (x256); got " + sequence(g, FACTOR * 256));
	}

	// ---- 4. THE FOUR LOOP MODES, as sequences of indices
	// LSA=4, LEA=8, and we look at where the read head goes. The four sequences are RECORDED: they are not
	// deduced from a specification, they are what this core does.
	//
	// AND IT IS HERE THAT THE MEASUREMENT CONTRADICTED THE READING, for all four modes at once. Reading the
	// code I had predicted that modes 2 and 3 read index 8, outside [LSA, LEA]: the step is applied AFTER
	// the read, so the turnaround seems to start from an address already out of the loop. That is wrong, and
	// here is why:
	//
	//   the looping loop runs over TWO addresses, `cur_addr` and `nxt_addr` (the `for (addr_select...)`).
	//   But just before it, `slot->nxt_addr = slot->cur_addr + (1 << SHIFT)`: nxt_addr is REASSIGNED on
	//   every sample. So everything the nxt_addr pass WRITES into the address is lost on the next round --
	//   but its SIDE EFFECTS do remain: `Backwards = 1` and `StopSlot`.
	//
	// And since nxt_addr is always one index ahead of cur_addr, those side effects fire one sample EARLY.
	// Hence the three consequences the measurement shows and that no reading gave:
	//   LPCTL=0 stops after index 6 and not 7: it was nxt_addr, at 8, that called StopSlot;
	//   LPCTL=2 goes from 3 to 6, not to 8;
	//   LPCTL=3 starts again from 6 after 7, not from 8;
	//   and NO sample beyond LEA is ever read, in any mode.
	//
	// A rewrite applying the loop only to cur_addr -- which the naive reading suggests -- would shift all
	// four modes by one sample and play one sample too many. Anyone who wants to "clean up" the double pass
	// has to keep that very shift.
	{
		struct Case { unsigned lpctl; const char *name; const char *expected; };
		const Case cases[] = {
			{ 0, "LPCTL=0 no loop",       "0,1,2,3,4,5,6,0,0,0,0,0,0,0,0,0,0,0,0,0" },
			{ 1, "LPCTL=1 normal loop",   "0,1,2,3,4,5,6,7,4,5,6,7,4,5,6,7,4,5,6,7" },
			{ 2, "LPCTL=2 reverse loop",  "0,1,2,3,6,5,4,7,6,5,4,7,6,5,4,7,6,5,4,7" },
			{ 3, "LPCTL=3 ping-pong",     "0,1,2,3,4,5,6,7,6,5,4,5,6,7,6,5,4,5,6,7" },
		};
		for (const Case &c : cases) {
			Bench b;
			b.ramp(0x1000, 32, 100);
			b.ram[0x1000 / 2] = 1;   // index 0 must not be 0, or it is indistinguishable from silence
			std::uint16_t word[16] = {};
			blank(word, 0x1000);
			put(word, "LPCTL", c.lpctl);
			put(word, "LSA", 4);
			put(word, "LEA", 8);
			b.key(0, word);
			std::vector<std::int32_t> g = b.left(20);
			if (!g.empty() && g[0] == FACTOR) g[0] = 0;   // index 0 gets its 0 back
			const std::string s = sequence(g, FACTOR * 100);
			(s == c.expected) ? ok(c.name, s)
			                  : failed(c.name, "expected " + std::string(c.expected) + "; got " + s);
		}
	}

	// ---- 5. LINEAR INTERPOLATION, at the exact half sample
	// OCT is -1 (field 0xF: `(OCT ^ 8) - 8`), so the step is 2048 instead of 4096: half a source sample per
	// output sample. Between 0 and 1000 we must read exactly 500, not 499 and not 512.
	{
		Bench b;
		b.ramp(0x1000, 32, 1000);
		std::uint16_t word[16] = {};
		blank(word, 0x1000);
		put(word, "OCT", 0xF);      // -1
		put(word, "LPCTL", 1);
		put(word, "LSA", 0);
		put(word, "LEA", 24);
		b.key(0, word);
		const std::string s = sequence(b.left(9), FACTOR * 500);
		const char *expected = "0,1,2,3,4,5,6,7,8";   // 0, 500, 1000, 1500, ... in steps of 500
		(s == expected) ? ok("interpolation: the exact half sample",
		                    "step 2048: the output rises by 500 (half a step of 1000): " + s)
		                : failed("interpolation: the exact half sample",
		                         "expected exact multiples of 500; got " + s);
	}

	// ---- 6. SBCTL: two inversions that do not resemble each other
	// bit 0 does `sample ^= 0x7FFF` -- an exclusive OR on an s32, WITHOUT truncation; bit 1 does
	// `sample = (s16)(sample ^ 0x8000)`, with truncation. The two together give almost the opposite, to
	// within one: 1000 comes out as -1001. The truncation difference is not observable here, the sample
	// coming from a 16-bit read -- said rather than tested, so that a rewrite does not think it has seen it.
	{
		struct Case { unsigned sbctl; std::int32_t expected; const char *what; };
		const Case cases[] = {
			{ 0,  1000, "SBCTL=0: as it is" },
			{ 1, 31767, "SBCTL=1: ^0x7FFF, the low fifteen bits" },
			{ 2,-31768, "SBCTL=2: ^0x8000 truncated, the sign" },
			{ 3, -1001, "SBCTL=3: both, that is the opposite to within one" },
		};
		std::string lines;
		bool correct = true;
		for (const Case &c : cases) {
			Bench b;
			for (unsigned k = 0; k < 16; k++) b.ram[0x1000 / 2 + k] = 1000;
			std::uint16_t word[16] = {};
			blank(word, 0x1000);
			put(word, "SBCTL", c.sbctl);
			put(word, "LPCTL", 1);
			put(word, "LSA", 0);
			put(word, "LEA", 16);
			b.key(0, word);
			const std::int32_t v = b.left(4)[0] / FACTOR;
			char t[80];
			snprintf(t, sizeof t, "%s%u->%d", lines.empty() ? "" : " ", c.sbctl, v);
			lines += t;
			if (v != c.expected) { correct = false; lines += "(expected " + std::to_string(c.expected) + ")"; }
		}
		correct ? ok("SBCTL: 1000 becomes", lines)
		        : failed("SBCTL: 1000 becomes", lines);
	}

	// ---- 7. RAM'S LIMITS: a fold at 1 MB, and an odd SA that loses its low bit
	// read_word does `a &= 0xffffe`: the address folds at 1 MB AND loses its bit 0. Both are observable,
	// and neither is obvious -- a rewrite addressing in 32 bits, or honouring an odd SA, would play
	// something else.
	{
		// (a) the fold: the sample after the last word is the FIRST word
		Bench b;
		b.ram[0xFFFFE / 2] = 7000;
		b.ram[0] = 1000;
		std::uint16_t word[16] = {};
		blank(word, 0xFFFFE);
		put(word, "LPCTL", 1);
		put(word, "LSA", 0);
		put(word, "LEA", 4);
		b.key(0, word);
		const std::vector<std::int32_t> g = b.left(2);
		const bool wrapped = g[0] == 7000 * FACTOR && g[1] == 1000 * FACTOR;

		// (b) an odd SA reads the same word as an even one
		Bench b2;
		b2.ram[0x1000 / 2] = 4242;
		std::uint16_t word2[16] = {};
		blank(word2, 0x1001);          // odd
		put(word2, "LPCTL", 1);
		put(word2, "LSA", 0);
		put(word2, "LEA", 4);
		b2.key(0, word2);
		// ONE call, kept in a variable: `left()` ADVANCES the slot. The first version of this case called
		// it twice -- once to compare, once to print -- and the message printed the NEXT sample, hence 0,
		// beside a test that passed. A measurement that moves while you look at it is not read twice.
		const std::int32_t odd = b2.left(1)[0];
		const bool even = odd == 4242 * FACTOR;

		char t[160];
		snprintf(t, sizeof t, "0xFFFFE then index 1 give %d and %d; SA=0x1001 gives %d",
		         g[0] / FACTOR, g[1] / FACTOR, odd / FACTOR);
		(wrapped && even) ? ok("RAM folds at 1 MB, SA loses its bit 0", t)
		                  : failed("RAM folds at 1 MB, SA loses its bit 0",
		                           std::string(t) + "; expected 7000, 1000 and 4242");
	}

	// ---- 8. SSCTL: the silence that ADVANCES, and the one that does not
	// SSCTL=2 returns zero but walks the source; SSCTL=3 leaves UpdateSlot BEFORE anything, so the read
	// head does not move. At the output both are silent -- the difference only shows AFTERWARDS, by putting
	// SSCTL back to 0 and looking at where we are. It is the kind of detail a rewrite unifies without
	// knowing, and that then shifts every sample of a voice.
	//
	// Word 0 is rewritten without KYONEX: UpdateSlotReg case 0 does nothing when that bit is absent, so the
	// slot is not retriggered.
	{
		std::string lines;
		bool correct = true;
		for (auto [ssctl, expected] : { std::pair<unsigned, const char *>{ 2, "4,5,6,7" },
		                                std::pair<unsigned, const char *>{ 3, "0,1,2,3" } }) {
			Bench b;
			b.ramp(0x1000, 32, 100);
			b.ram[0x1000 / 2] = 1;   // to tell index 0 from silence
			std::uint16_t word[16] = {};
			blank(word, 0x1000);
			put(word, "SSCTL", ssctl);
			put(word, "LPCTL", 1);
			put(word, "LSA", 0);
			put(word, "LEA", 32);
			b.key(0, word);
			b.left(4);                               // four silent samples
			std::uint16_t next_word[16] = {};
			blank(next_word, 0x1000);
			put(next_word, "SSCTL", 0);               // the source becomes RAM again
			put(next_word, "LPCTL", 1);
			b.the_core.write_reg(0, next_word[0], 0xFFFF);
			std::vector<std::int32_t> g = b.left(4);
			if (!g.empty() && g[0] == FACTOR) g[0] = 0;
			const std::string s = sequence(g, FACTOR * 100);
			lines += (lines.empty() ? "" : " | ") + ("SSCTL=" + std::to_string(ssctl) + " -> " + s);
			if (s != expected) { correct = false; lines += " (expected " + std::string(expected) + ")"; }
		}
		correct ? ok("SSCTL=2 advances, SSCTL=3 does not", lines)
		        : failed("SSCTL=2 advances, SSCTL=3 does not", lines);
	}

	printf("\n");
	if (failures) { printf("%d failure(s).\n", failures); return 1; }
	printf("all cases pass\n");
	return 0;
}
