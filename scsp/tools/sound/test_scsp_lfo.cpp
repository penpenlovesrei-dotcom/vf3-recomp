// test_scsp_lfo: the read step, the two LFOs and ring modulation.
//
// Block 4 of docs/scsp_architecture.md §3 -- "Pitch, LFO, FM modulation: OCT/FNS, the four LFO waveforms,
// PLFOS/ALFOS, and ring modulation (MDL/MDXSL/MDYSL)." With the document's instruction, which is half of
// this file's subject: "This port's settings are kept as they are until measured otherwise: FM depth at 24
// and MDL's low bit ignored (docs/scsp_audit.md §2)."
//
// TWO PROBES, each one exact.
//
//   THE PHASE ACCUMULATOR, for everything to do with the step. With a ramp whose sample k is 1024*k, the
//   linear interpolation of a straight line is exact, and the left output is
//       4 * (1024 * i + ((1024 * f) >> 12))  =  cur_addr - (f mod 4)
//   where i and f are cur_addr's integer and fractional parts. So the output IS the phase accumulator, to
//   within three -- and the step reads off the second sample, without counting or guessing. It is the
//   measurement block 2 could not make: it read indices, this one reads the phase itself.
//
//   THE AMPLITUDE MULTIPLIER, for the ALFO. As in block 3: a steady wave at 8192, the envelope laid flat
//   (AR=0x1F, D1R=D2R=0, DL=0, hence 4096 from the second sample on), and the output is eight times the
//   ALFO's multiplier.
//
// AND LFOF SERVES BOTH LFOS. The field is a single one and both structures receive it, so that a question
// about the rates is answered through the ALFO, which has no address and therefore tolerates long
// measurements.
//
// The rig -- a fresh core per case, the RAM, the keying -- is in tools/sound/scsp_bench.h.
#include "scsp_bench.h"

#include <cstdio>
#include <set>
#include <string>
#include <vector>

namespace {

using bench::Bench;
using bench::put;

int failures = 0;

void ok(const char *what, const std::string &d) { printf("  ok     %-40s %s\n", what, d.c_str()); }
void failed(const char *what, const std::string &d) { printf("  FAILED %-40s %s\n", what, d.c_str()); ++failures; }

constexpr std::uint32_t WAVE = 0x1000;
constexpr int RAMP_STEP = 1024;      // the ramp that makes the output the phase accumulator
constexpr int STEADY = 8192;         // the flat wave that makes the output a gain
constexpr int PER_MULT = 8;

// The oscillator bare: no envelope (SDIR), full direct output centred.
void bare_phase(std::uint16_t word[16])
{
	put(word, "SA_HI", WAVE >> 16);
	put(word, "SA_LO", WAVE & 0xFFFF);
	put(word, "SDIR",  1);
	put(word, "DISDL", 7);
	put(word, "DIPAN", 0);
	put(word, "LPCTL", 1);
	put(word, "LSA",   0);
	put(word, "LEA",   32);
	put(word, "KYONB", 1);
}

// The envelope laid flat: what is left varying is the ALFO.
void bare_gain(std::uint16_t word[16])
{
	put(word, "SA_HI", WAVE >> 16);
	put(word, "SA_LO", WAVE & 0xFFFF);
	put(word, "DISDL", 7);
	put(word, "DIPAN", 0);
	put(word, "TL",    0);
	put(word, "OCT",   0);
	put(word, "FNS",   0);
	put(word, "LPCTL", 1);
	put(word, "LSA",   0);
	put(word, "LEA",   256);
	put(word, "AR",    0x1F);
	put(word, "D1R",   0);
	put(word, "D2R",   0);
	put(word, "DL",    0);
	put(word, "KYONB", 1);
}

// The read step, read off the second sample.
std::int32_t measured_step(Bench &b, unsigned slot = 0)
{
	(void) slot;
	const std::vector<std::int32_t> g = b.left(2);
	return g[1] - g[0];   // g[0] is zero, cur_addr starting from zero
}

// The ALFO's multipliers, FROM THE SECOND SAMPLE ON, and the first is thrown away for a reason that can be
// measured: it goes through the ATTACK path, which multiplies by the raw volume (4092) and not by the table
// (4096), so that a gain of 256 comes out there as 255. But that removal SHIFTS EVERYTHING BY ONE with
// respect to the LFO table's indices, and the first version of the cases below looked for its landmarks
// one notch too far. So m[k] corresponds to index (k + 2) modulo 256 of the table.
std::vector<int> gains(Bench &b, unsigned n)
{
	const std::vector<std::int32_t> g = b.left(n + 1);
	std::vector<int> m(n);
	for (unsigned i = 0; i < n; i++) m[i] = int(g[i + 1] / PER_MULT);
	return m;
}

} // namespace

int main(int argc, char **argv)
{
	if (argc > 1) bench::core_name() = argv[1];
	printf("test_scsp_lfo: the step, the LFOs and the modulation, through scsp::core\n\n");

	// ---- 1. THE STEP, by OCT and FNS, read and not counted
	// `Step()` does Fn = FNS + 1024 then shifts by `(OCT ^ 8) - 8 + SHIFT - 10`, that is the octave plus
	// two. So OCT is a SIGNED four-bit integer in a representation biased by exclusive OR -- field 0xF is
	// -1, field 0x8 is -8 -- and a rewrite reading it as unsigned would play eight octaves too high on half
	// the notes.
	{
		struct Case { unsigned oct, fns; std::int32_t step; const char *what; };
		const Case cases[] = {
			{ 0x0, 0x000, 4096, "OCT=0 FNS=0: one sample per sample" },
			{ 0x0, 0x200, 6144, "FNS halfway: one and a half times" },
			{ 0x0, 0x3FF, 8188, "FNS at maximum: almost double" },
			{ 0x1, 0x000, 8192, "OCT=+1: double" },
			{ 0xF, 0x000, 2048, "OCT=-1 (field 0xF): half" },
			{ 0xE, 0x000, 1024, "OCT=-2: a quarter" },
			{ 0x8, 0x000,   16, "OCT=-8 (field 0x8): the minimum" },
		};
		std::string lines;
		bool correct = true;
		for (const Case &c : cases) {
			Bench b;
			b.ramp(WAVE, 32, RAMP_STEP);
			std::uint16_t word[16] = {};
			bare_phase(word);
			put(word, "OCT", c.oct);
			put(word, "FNS", c.fns);
			b.key(0, word);
			const std::int32_t p = measured_step(b);
			char t[64];
			snprintf(t, sizeof t, "%s%x/%03x->%d", lines.empty() ? "" : " ", c.oct, c.fns, p);
			lines += t;
			if (p != c.step) { correct = false; lines += "(expected " + std::to_string(c.step) + ")"; }
		}
		correct ? ok("the step by OCT and FNS", lines)
		        : failed("the step by OCT and FNS", lines);
	}

	// ---- 2. EIGHT OF THE THIRTY-TWO LFO RATES ARE DEAD
	// `phase_step = (u32)((float)(1 << LFO_SHIFT) * LFOFreq[LFOF] * 256.0f / 44100.0f)`, and the conversion
	// to an unsigned integer TRUNCATES. So LFOFreq >= 44100 / 65536 = 0.673 Hz is needed for the step to be
	// at least one, and the table's first eight entries (0.17 to 0.55 Hz) are below that: their phase never
	// moves.
	//
	// This is not a rounding detail but a quarter of the register doing nothing, and a rewrite keeping the
	// phase in floating point -- or rounding instead of truncating -- would make eight rates vibrate that
	// this port leaves still.
	//
	// FOUR THOUSAND SAMPLES AND NOT SIX HUNDRED, and the figure is a measurement. The first version of this
	// case ran 600 and declared rates 0 to 11 dead: at phase_step = 1 the table index only advances by one
	// every 256 samples, and m_ASCALES truncates, so that several neighbouring indices give the SAME
	// integer. So an unmoving gain did not prove an unmoving phase. At 4096 samples the index gains sixteen
	// notches, which leaves the truncation zone.
	{
		std::string dead, alive;
		for (unsigned lfof = 0; lfof < 32; lfof++) {
			Bench b;
			b.steady(WAVE, 256, STEADY);
			std::uint16_t word[16] = {};
			bare_gain(word);
			put(word, "LFOF", lfof);
			put(word, "ALFOWS", 0);     // sawtooth: any phase that moves changes the gain
			put(word, "ALFOS", 7);
			b.key(0, word);
			const std::vector<int> m = gains(b, 4096);
			bool moves = false;
			for (int v : m) if (v != m[0]) { moves = true; break; }
			(moves ? alive : dead) += (((moves ? alive : dead).empty()) ? "" : ",")
			                        + std::to_string(lfof);
		}
		(dead == "0,1,2,3,4,5,6,7")
			? ok("LFOF 0 to 7: the phase does not move", "dead " + dead + "; alive from 8 on")
			: failed("LFOF 0 to 7: the phase does not move",
			         "dead " + dead + " (expected 0,1,2,3,4,5,6,7); alive " + alive);
	}

	// ---- 3. THE FOUR ALFO WAVEFORMS, one whole period
	// At LFOF=31 the phase step is exactly 256, so the table index advances by one per sample: 256 samples
	// are one complete period, and the table is read as it is.
	//
	// BUT SHIFTED BY ONE NOTCH, and that has to be said on pain of thinking the table wrong. PLFO_Step and
	// ALFO_Step increment the phase BEFORE reading: `phase += phase_step; p = table[phase >> 8]`. So the
	// first sample reads index 1 and not 0, and the period runs 1,2,...,255,0. The phase is a u16, which
	// wraps it through the type -- that is why those functions' explicit mask is compiled out when
	// LFO_SHIFT is 8, and not because it would be useless.
	//
	// The four waveforms are not four variants of one thing: the sawtooth is monotonic, the square has only
	// TWO values, the triangle rises then falls, and the noise is drawn from a congruential generator at the
	// device's initialisation. So the SHAPE of each is checked rather than its values, except at the
	// sawtooth's two ends, which are exact.
	{
		auto shape = [](unsigned alfows) {
			Bench b;
			b.steady(WAVE, 256, STEADY);
			std::uint16_t word[16] = {};
			bare_gain(word);
			put(word, "LFOF", 31);
			put(word, "ALFOWS", alfows);
			put(word, "ALFOS", 7);
			b.key(0, word);
			return gains(b, 256);
		};

		// sawtooth: a = 255 - i, so the attenuation DECREASES. So the gain rises over 255 samples (indices
		// 1 to 255) then DROPS BACK at the last, which reads index 0 -- the period's wrap, visible because
		// the phase is incremented before the read.
		{
			const std::vector<int> m = shape(0);
			bool rises = true;
			for (std::size_t i = 1; i <= 253; i++) if (m[i] < m[i - 1]) { rises = false; break; }
			(rises && m[0] == 256 && m[253] == 4096 && m[254] == 256)
				? ok("ALFOWS=0 sawtooth", "monotonic from " + std::to_string(m[0]) + " to "
				     + std::to_string(m[253]) + " over 254 samples, then the wrap to "
				     + std::to_string(m[254]))
				: failed("ALFOWS=0 sawtooth", "expected 256 ... 4096 then 256; got "
				         + std::to_string(m[0]) + " ... " + std::to_string(m[253]) + " then "
				         + std::to_string(m[254]) + (rises ? "" : ", and not monotonic"));
		}
		// square: two values, and nothing in between
		{
			const std::vector<int> m = shape(1);
			const std::set<int> v(m.begin(), m.end());
			(v.size() == 2 && *v.begin() == 256 && *v.rbegin() == 4096)
				? ok("ALFOWS=1 square", "exactly two values, 256 and 4096")
				: failed("ALFOWS=1 square", "expected two values 256 and 4096; got "
				         + std::to_string(v.size()) + " values");
		}
		// triangle: the attenuation falls from 255 to 0 over the table's first half then rises again, so
		// the GAIN rises to index 128 and falls back. One turning point only, and the period's wrap falls
		// outside the 256 samples measured: two directions and not three.
		//
		// They are counted IGNORING THE PLATEAUX, because m_ASCALES truncates: two neighbouring indices
		// often give the same integer, and a loose comparison counted those equalities as changes of
		// direction -- thirty-two instead of two, on a perfectly triangular shape.
		{
			const std::vector<int> m = shape(2);
			std::vector<int> dirs;
			for (std::size_t i = 1; i < m.size(); i++) {
				if (m[i] == m[i - 1]) continue;
				const int s = m[i] > m[i - 1] ? 1 : -1;
				if (dirs.empty() || dirs.back() != s) dirs.push_back(s);
			}
			(dirs.size() == 2 && dirs[0] == 1 && dirs[1] == -1)
				? ok("ALFOWS=2 triangle", "rises to the peak then falls back: two directions, plateaux ignored")
				: failed("ALFOWS=2 triangle", std::to_string(dirs.size())
				         + " changes of direction, expected 2 (up then down)");
		}
		// noise: many values, and the same draw from one core to the other (the generator is seeded)
		{
			const std::vector<int> m = shape(3), m2 = shape(3);
			const std::set<int> v(m.begin(), m.end());
			(v.size() > 100 && m == m2)
				? ok("ALFOWS=3 noise", std::to_string(v.size())
				     + " distinct values, and identical from one core to the other: the generator is seeded")
				: failed("ALFOWS=3 noise", std::to_string(v.size()) + " distinct values"
				         + (m == m2 ? "" : ", and TWO cores do not give the same draw"));
		}
	}

	// ---- 4. ALFOS: the eight depths, in plain sight
	// ASCALE is {0, 0.4, 0.8, 1.5, 3, 6, 12, 24} dB, and the gain at the square's trough encodes them.
	// ALFOS=0 attenuates nothing AT ALL -- and for one more reason than the zero depth: `if (ALFOS(slot) !=
	// 0)` prevents the call, so the ALFO's phase does not even advance.
	{
		const int expected[8] = { 4096, 3904, 3728, 3440, 2896, 2048, 1024, 256 };
		std::string lines;
		bool correct = true;
		for (unsigned alfos = 0; alfos < 8; alfos++) {
			Bench b;
			b.steady(WAVE, 256, STEADY);
			std::uint16_t word[16] = {};
			bare_gain(word);
			put(word, "LFOF", 31);
			put(word, "ALFOWS", 1);     // square: the trough is a clear-cut value
			put(word, "ALFOS", alfos);
			b.key(0, word);
			const std::vector<int> m = gains(b, 256);
			int trough = m[0];
			for (int v : m) if (v < trough) trough = v;
			char t[48];
			snprintf(t, sizeof t, "%s%u->%d", lines.empty() ? "" : " ", alfos, trough);
			lines += t;
			if (trough != expected[alfos]) { correct = false; lines += "(exp " + std::to_string(expected[alfos]) + ")"; }
		}
		correct ? ok("ALFOS: the trough per depth", lines)
		        : failed("ALFOS: the trough per depth", lines);
	}

	// ---- 5. PLFOS LOSES ITS LOW BIT, AND THAT IS DELIBERATE
	// `#define PLFOS(slot) ((slot->udata.data[0x9] >> 0x5) & 0x0006)`: the mask is 6 and not 7. This is a
	// DELIBERATE DIVERGENCE from MAME, taken over from Supermodel and tuned by ear for VF3
	// (docs/scsp_audit.md §2, and the macro's comment): the voices at PLFOS 1 were losing their vibrato,
	// and those at 3, 5, 7 come out as 2, 4, 6.
	//
	// This case exists so that nobody "corrects" it absent-mindedly. It does not say the divergence is
	// right -- it says it is there, that it was chosen, and what shape it has: the fields go in pairs.
	//
	// AND THE FIELD IS WRITTEN BY HAND, not through scsp_contract::put, because the contract's 0x0006 mask
	// is a READ mask and not a write one: the register's field is three bits wide (7-5), and it is the
	// implementation that throws its low bit away. So `put(word, "PLFOS", 1)` cannot set a 1, `1 & 6` being
	// zero -- and a test using it would not measure the divergence, it would reproduce it. It is the only
	// field in the table in that situation (scsp_contract.h section 8).
	{
		std::string lines;
		bool correct = true;
		std::int32_t step[8] = {};
		for (unsigned plfos = 0; plfos < 8; plfos++) {
			Bench b;
			b.ramp(WAVE, 32, RAMP_STEP);
			std::uint16_t word[16] = {};
			bare_phase(word);
			put(word, "OCT", 0);
			put(word, "FNS", 0);
			put(word, "LFOF", 31);
			put(word, "PLFOWS", 1);     // square: at the start the phase is at the peak, the step is constant
			word[0x9] = std::uint16_t((word[0x9] & ~(scsp_contract::PLFOS_FIELD_WIDTH << scsp_contract::PLFOS_FIELD_SHIFT))
			                         | ((plfos & scsp_contract::PLFOS_FIELD_WIDTH) << scsp_contract::PLFOS_FIELD_SHIFT));
			b.key(0, word);
			step[plfos] = measured_step(b);
			char t[48];
			snprintf(t, sizeof t, "%s%u->%d", lines.empty() ? "" : " ", plfos, step[plfos]);
			lines += t;
		}
		for (unsigned k = 0; k < 8; k += 2)
			if (step[k] != step[k + 1]) correct = false;
		const bool modulates = step[2] != step[0];
		(correct && modulates)
			? ok("PLFOS in pairs: the low bit is thrown away", lines + " (0=1, 2=3, 4=5, 6=7)")
			: failed("PLFOS in pairs: the low bit is thrown away",
			         lines + (modulates ? ": one pair differs" : ": no modulation, the probe is silent"));
	}

	// ---- 6. RING MODULATION: the depth is 24, and MDL's low bit is ignored
	// The other deliberate divergence, and the document's instruction is about it: `smp >>= 0x18 - (MDL &
	// 0xE)` where MAME writes 0x1A and keeps MDL's four bits. Tuned by ear on the theme of Pai's arena,
	// where an FM voice came out dull (docs/scsp_audit.md §2).
	//
	// IT IS MEASURED WITH NO MODULATING VOICE AT ALL. The ring is reachable from the register window
	// (0x600-0x6FF, `m_RINGBUF[(addr - 0x600) / 2] = val`), so the value is PUT there instead of waiting
	// for another slot to write it -- and STWINH=1 stops the measured slot overwriting it. The address
	// offset then reads directly: with a ramp of step 100 and cur_addr zero, the output is
	// 4 * 100 * the offset in samples.
	{
		constexpr int V = 16384;            // the value put into the ring
		constexpr unsigned XSL = 1;         // the entry read, BUFPTR being zero on the first sample
		// smp = ((V + V) / 2) << 10 >> (24 - (MDL & 0xE)), then << 1 for 16-bit: so the offset in samples
		// is (V << 10) >> (24 - (MDL & 0xE)).
		const int expected[10] = { 1, 1, 4, 4, 16, 16, 64, 64, 256, 256 };
		std::string lines;
		bool correct = true;
		for (unsigned mdl = 0; mdl < 10; mdl++) {
			Bench b;
			b.ramp(WAVE, 400, 100);
			b.the_core.write_reg(0x300 + XSL, std::uint16_t(V), 0xFFFF);   // 0x600 + 2*XSL in bytes
			std::uint16_t word[16] = {};
			bare_phase(word);
			put(word, "OCT", 0);
			put(word, "FNS", 0);
			put(word, "LEA", 400);
			put(word, "STWINH", 1);     // the slot does not write into the ring
			put(word, "MDL", mdl);
			put(word, "MDXSL", XSL);
			put(word, "MDYSL", XSL);
			b.key(0, word);
			const std::int32_t output = b.left(1)[0];
			const int shift_by = int(output / (4 * 100));
			char t[48];
			snprintf(t, sizeof t, "%s%x->%d", lines.empty() ? "" : " ", mdl, shift_by);
			lines += t;
			if (shift_by != expected[mdl] || output % (4 * 100) != 0) {
				correct = false;
				lines += "(exp " + std::to_string(expected[mdl]) + ")";
			}
		}
		correct ? ok("MDL: depth 24, low bit thrown away", lines + " (the pairs are equal)")
		        : failed("MDL: depth 24, low bit thrown away", lines);
	}

	// ---- 7. THE MODULATION'S GUARD COVERS THREE FIELDS, NOT MDL
	// `if (MDL(slot) != 0 || MDXSL(slot) != 0 || MDYSL(slot) != 0)`: a slot whose MDL is zero still
	// modulates if MDXSL or MDYSL is not -- which is what MDL=0 shows in the previous case, offsetting by
	// one sample. And all three at zero do not modulate, even with a full ring. A rewrite examining only
	// MDL would lose the first behaviour and would not notice on any game that leaves MDXSL at zero.
	{
		Bench b;
		b.ramp(WAVE, 400, 100);
		b.the_core.write_reg(0x300, std::uint16_t(16384), 0xFFFF);   // the ring full at entry 0
		std::uint16_t word[16] = {};
		bare_phase(word);
		put(word, "OCT", 0);
		put(word, "FNS", 0);
		put(word, "LEA", 400);
		put(word, "STWINH", 1);
		put(word, "MDL", 0);
		put(word, "MDXSL", 0);
		put(word, "MDYSL", 0);
		b.key(0, word);
		const std::int32_t output = b.left(1)[0];
		(output == 0) ? ok("MDL, MDXSL and MDYSL all zero: no modulation",
		                   "the ring is 16384 and the output stays at sample 0")
		              : failed("MDL, MDXSL and MDYSL all zero: no modulation",
		                       "output " + std::to_string(output) + ", expected 0");
	}

	// ---- 8. LFORE: what this core does with it, which is nothing
	// The field exists in the contract (bit 15 of word 9, "LFO reset") and the driver can write it, but
	// scsp.cpp reads it NOWHERE: the LFORE macro is defined and never called. So the LFO's phase is only
	// reset by StartSlot, through Compute_LFO. It is recorded as it is -- it is a difference from the
	// hardware that no measurement here can settle, and noting it beats letting it be discovered.
	{
		std::int32_t with_it = 0, without = 0;
		for (int i = 0; i < 2; i++) {
			Bench b;
			b.steady(WAVE, 256, STEADY);
			std::uint16_t word[16] = {};
			bare_gain(word);
			put(word, "LFOF", 31);
			put(word, "ALFOWS", 0);
			put(word, "ALFOS", 7);
			put(word, "LFORE", unsigned(i));
			b.key(0, word);
			const std::vector<int> m = gains(b, 64);
			(i ? with_it : without) = m[63];
		}
		(with_it == without) ? ok("LFORE is read nowhere in this core",
		                          "with and without, the same gain at the 64th sample: " + std::to_string(with_it))
		                     : failed("LFORE is read nowhere in this core",
		                              "with " + std::to_string(with_it) + ", without " + std::to_string(without)
		                              + ": so it is read somewhere, and this test is to be rewritten");
	}

	printf("\n");
	if (failures) { printf("%d failure(s).\n", failures); return 1; }
	printf("all cases pass\n");
	return 0;
}
