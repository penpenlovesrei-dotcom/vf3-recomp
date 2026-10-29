// test_scsp_mix: the mixing and the output -- DISDL, DIPAN, TL, DAC18B, MVOL, and the clipping.
//
// Block 7 of docs/scsp_architecture.md §3 -- "Mixing and output: DISDL/DIPAN, EFSDL/EFPAN, MVOL, DAC18B,
// the pan tables. This port's two per-group volumes and its effects leveller are ABOVE the chip, not in
// it: they stay in the board." The last block, and the one that touches everything: every voice goes
// through it twice, once for the direct output and once for the send to the DSP.
//
// THE PROBE IS BLOCKS 3 AND 4's STEADY WAVE, with SDIR=1 to set the envelope aside: the output is then the
// mix's GAIN, and nothing else. And since this block is the one about both channels, the bench can read
// both (bench::Bench::stereo) -- half of DIPAN's facts are on the channel the rest of the project throws
// away.
//
// THREE DECIBEL TABLES THE SEGA WAY, and that is the block's shape. None of them is a formula: each is a
// sum of terms chosen bit by bit, which gives gains that nothing makes "round".
//     TL     0.4, 0.8, 1.5, 3, 6, 12, 24, 48 dB, one bit each (eight bits, 95.7 dB in all)
//     DIPAN  3, 6, 12, 24 dB over four bits, and bit 4 picks WHICH channel is attenuated
//     SDLT   -inf, -36, -30, -24, -18, -12, -6, 0 dB for DISDL's eight values
// All three are read here in full, because a rewrite replacing them with a regular exponential would give
// itself away nowhere else.
//
// The rig -- a fresh core per case, the RAM, the keying -- is in tools/sound/scsp_bench.h.
#include "scsp_bench.h"

#include <cstdio>
#include <string>
#include <vector>

namespace {

using bench::Bench;
using bench::put;

int failures = 0;

void ok(const char *what, const std::string &d) { printf("  ok     %-40s %s\n", what, d.c_str()); }
void failed(const char *what, const std::string &d) { printf("  FAILED %-40s %s\n", what, d.c_str()); ++failures; }

constexpr std::uint32_t WAVE = 0x1000;
constexpr int STEADY = 8192;

// A voice with nothing left but the mix: the envelope laid flat, no LFO, a constant source.
//
// NOT SDIR, AND THAT IS THIS BLOCK'S TRAP. Blocks 2 and 4 used SDIR to set the envelope aside, but in the
// mix `dir_tl = SDIR(slot) ? 0 : TL(slot)` -- SDIR also sets TL TO ZERO. So the first version of this file
// measured TL with SDIR=1 and found the same gain eight times, with nothing looking broken. The envelope
// is laid flat instead (AR=0x1F, D1R=D2R=0, DL=0: the multiplier is 4096 from the second sample on, cf.
// block 3), which leaves TL free to act.
void bare_mix(std::uint16_t word[16])
{
	put(word, "SA_HI", WAVE >> 16);
	put(word, "SA_LO", WAVE & 0xFFFF);
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

// The mix's gain, on both channels, at the fourth sample: the first goes through the ATTACK path (4092
// instead of 4096, cf. block 3) and so would not measure the mix alone.
//
// DAC18B IS ON BY DEFAULT, and that is not a detail: without it the output stage does `smpl >> 2` then
// multiplies by four again, so every measurement is rounded to the nearest multiple of 4. The first
// version of this file left it off and read 516 where the table says 518, 23196 where it says 23197:
// three decibel tables wrong by two units, through one configuration bit.
std::pair<std::int32_t, std::int32_t> gain(unsigned disdl, unsigned dipan, unsigned tl,
                                           bool dac18b = true, int source = STEADY)
{
	Bench b;
	b.steady(WAVE, 256, source);
	if (!dac18b) b.the_core.write_reg(0x400 / 2, 0x0200 | 0x000F, 0xFFFF);   // MEM4MB alone: DAC18B cleared
	std::uint16_t word[16] = {};
	bare_mix(word);
	put(word, "DISDL", disdl);
	put(word, "DIPAN", dipan);
	put(word, "TL",    tl);
	b.key(0, word);
	return b.stereo(4);
}

} // namespace

int main(int argc, char **argv)
{
	if (argc > 1) bench::core_name() = argv[1];
	printf("test_scsp_mix: the mixing and the output, through scsp::core\n\n");

	// ---- 1. THE SCALE, AND THE FACTOR OF FOUR THAT COMES FROM THE TABLE
	// `m_LPANTABLE[i] = FIX(4.0f * LPAN * TL * fSDL)`: there is a FOUR in the table, and FIX multiplies by
	// 4096. At full level in the centre the table is therefore 16384 and not 4096, so that the mix returns
	// four times the sample. This is the origin of the factor of 4 that blocks 2 and 4's tests measured
	// without explaining -- it is neither in the adapter nor in the output stage, it is here.
	{
		const auto [g, d] = gain(7, 0, 0);
		(g == 4 * STEADY && d == 4 * STEADY)
			? ok("full level in the centre: four times the sample",
			     std::to_string(STEADY) + " in RAM comes out at " + std::to_string(g) + " on the left and "
			     + std::to_string(d) + " on the right")
			: failed("full level in the centre: four times the sample",
			         "got " + std::to_string(g) + " and " + std::to_string(d)
			         + ", expected " + std::to_string(4 * STEADY) + " on both sides");
	}

	// ---- 2. DISDL: SDLT'S EIGHT LEVELS, AND THE ZERO THAT IS A SILENCE
	// SDLT = {-inf, -36, -30, -24, -18, -12, -6, 0} dB. The step is 6 dB except between 1 and 2, where it
	// is 6 as well but starting from -36: the table does not begin at -42, it begins at negative infinity.
	// So DISDL=0 is not "very low", it is SILENT -- `fSDL = 0.0` written out in the code.
	{
		// Measured, not computed: FIX truncates a float, so the integers are not the ones a decibel
		// calculation gives by hand. The last unit comes from the table, not from the physics.
		const int expected[8] = { 0, 518, 1036, 2066, 4124, 8230, 16422, 32768 };
		std::string lines;
		bool correct = true;
		for (unsigned s = 0; s < 8; s++) {
			const auto [g, d] = gain(s, 0, 0);
			char t[48];
			snprintf(t, sizeof t, "%s%u->%d", lines.empty() ? "" : " ", s, g);
			lines += t;
			if (g != expected[s] || d != expected[s]) { correct = false; lines += "(exp " + std::to_string(expected[s]) + ")"; }
		}
		correct ? ok("DISDL: the eight levels, 0 being silent", lines)
		        : failed("DISDL: the eight levels, 0 being silent", lines);
	}

	// ---- 3. TL: EIGHT BITS, EIGHT DECIBEL FIGURES
	// Each bit of TL removes its own number of decibels, and they are not in geometric progression:
	// 0.4, 0.8, 1.5, 3, 6, 12, 24, 48. The first three break the doubling rule (0.4, 0.8, then 1.5 and not
	// 1.6), and that is exactly the kind of detail a "clean" rewrite rounds off without noticing.
	{
		struct Case { unsigned tl; int expected; const char *db; };
		const Case cases[] = {
			{ 0x01, 31292, "-0.4 dB" }, { 0x02, 29884, "-0.8 dB" }, { 0x04, 27570, "-1.5 dB" },
			{ 0x08, 23196, "-3 dB"   }, { 0x10, 16422, "-6 dB"   }, { 0x20,  8230, "-12 dB" },
			{ 0x40,  2066, "-24 dB"  }, { 0x80,   130, "-48 dB"  },
		};
		std::string lines;
		bool correct = true;
		for (const Case &c : cases) {
			const auto [g, d] = gain(7, 0, c.tl);
			(void) d;
			char t[48];
			snprintf(t, sizeof t, "%s%02x->%d", lines.empty() ? "" : " ", c.tl, g);
			lines += t;
			if (g != c.expected) { correct = false; lines += "(exp " + std::to_string(c.expected) + ")"; }
		}
		correct ? ok("TL: one decibel figure per bit", lines)
		        : failed("TL: one decibel figure per bit", lines);
	}

	// ---- 4. DIPAN: BIT 4 PICKS THE ATTENUATED CHANNEL, AND THE OPPOSITE ONE STAYS AT FULL
	// `if (iPAN < 0x10) { LPAN = PAN; RPAN = 1.0; } else { RPAN = PAN; LPAN = 1.0; }`: this chip's pan is
	// not a balance that slides, it is an ATTENUATION on one side only, the other staying at full level. A
	// rewrite applying a continuous pan law -- a cosine, a square root -- would change the absolute level
	// of every off-centre voice in the game.
	{
		struct Case { unsigned dipan; int g, d; const char *what; };
		const Case cases[] = {
			{ 0x00, 32768, 32768, "centre: both at full" },
			{ 0x01, 23196, 32768, "-3 dB on the left only" },
			{ 0x02, 16422, 32768, "-6 dB on the left" },
			{ 0x11, 32768, 23196, "bit 4: -3 dB on the right" },
			{ 0x12, 32768, 16422, "-6 dB on the right" },
		};
		std::string lines;
		bool correct = true;
		for (const Case &c : cases) {
			const auto [g, d] = gain(7, c.dipan, 0);
			char t[64];
			snprintf(t, sizeof t, "%s%02x->%d/%d", lines.empty() ? "" : " ", c.dipan, g, d);
			lines += t;
			if (g != c.g || d != c.d) {
				correct = false;
				lines += "(exp " + std::to_string(c.g) + "/" + std::to_string(c.d) + ")";
			}
		}
		correct ? ok("DIPAN attenuates ONE channel, the other stays full", lines)
		        : failed("DIPAN attenuates ONE channel, the other stays full", lines);
	}

	// ---- 5. DIPAN 0x0F CUTS THE CHANNEL DEAD, AND IT IS NOT THE SUM OF THE FOUR BITS
	// `if ((iPAN & 0xf) == 0xf) PAN = 0.0;`: the value 0xF is not -45 dB (3+6+12+24), it is that channel's
	// SILENCE. A hard special case in the middle of an otherwise additive table -- so a rewrite summing the
	// four bits without that exception would let -45 dB leak through instead of nothing, which is audible
	// on a loud voice.
	{
		const auto [g15, d15] = gain(7, 0x0F, 0);
		const auto [g31, d31] = gain(7, 0x1F, 0);
		char t[128];
		snprintf(t, sizeof t, "0x0F -> %d/%d (left cut), 0x1F -> %d/%d (right cut)", g15, d15, g31, d31);
		(g15 == 0 && d15 == 32768 && g31 == 32768 && d31 == 0)
			? ok("DIPAN 0x0F: a silence, not an attenuation", t)
			: failed("DIPAN 0x0F: a silence, not an attenuation",
			         std::string(t) + "; expected 0/32768 then 32768/0");
	}

	// ---- 6. DAC18B DOES NOT CHANGE THE GAIN, IT CHANGES THE RESOLUTION
	// The two output branches look little alike and yet give the same scale:
	//     DAC18B: clamp(smpl, -131072, 131071) / 131072
	//     else  : clamp(smpl >> 2, -32768, 32767) / 32768
	// The factor of 4 between the two denominators cancels the `>> 2`, so the level is identical. What the
	// bit really changes is that WITHOUT it the sum's TWO LOW BITS are thrown away. So a sum that is not a
	// multiple of four is needed to see it -- an odd TL is enough.
	{
		// 5000 AND NOT 8192: with a power-of-two source, (source * table) >> 12 is always a multiple of
		// four and the `>> 2` loses nothing -- the difference would be invisible.
		const auto [a18, b18] = gain(7, 0, 0x01, true, 5000);
		const auto [a16, b16] = gain(7, 0, 0x01, false, 5000);
		(void) b18; (void) b16;
		char t[144];
		snprintf(t, sizeof t, "TL=01: with DAC18B %d, without %d (that is %d with its two low bits thrown away)",
		         a18, a16, a18);
		(a18 == 19099 && a16 == 19096 && (a16 & 3) == 0)
			? ok("DAC18B: the resolution, not the gain", t)
			: failed("DAC18B: the resolution, not the gain",
			         std::string(t) + "; expected 19099 then 19096");
	}

	// ---- 7. THE CLIPPING, AND ITS ASYMMETRY
	// `clamp(smpl, -131072, 131072 - 1)`: the upper bound is one notch lower than the lower bound in
	// absolute value, like any signed integer. With two voices at full level the mix overflows, and it is
	// the clipping that decides -- not a soft saturation, a guillotine. It is measured by keying two slots
	// onto the same wave.
	{
		Bench b;
		b.steady(WAVE, 256, STEADY);
		for (unsigned f = 0; f < 5; f++) {       // five voices at 4 x 8192 = 163840 each: well beyond
			std::uint16_t word[16] = {};
			bare_mix(word);
			put(word, "DISDL", 7);
			put(word, "DIPAN", 0);
			b.key(f, word);
		}
		const auto [g, d] = b.stereo(4);
		char t[112];
		snprintf(t, sizeof t, "five voices at full level: %d and %d", g, d);
		(g == 131071 && d == 131071) ? ok("the clipping: +131071 and not +131072", t)
		                             : failed("the clipping: +131071 and not +131072",
		                                      std::string(t) + "; expected 131071 on both sides");
	}

	// ---- 8. MVOL IS NOT IN THE CHIP, and the interface cannot read it
	// `case 0x0: m_gain = MVOL() / 15.0;` -- and that is all: m_gain is used nowhere in DoMasterSamples.
	// It is only EXPOSED by `output_gain()`, which the board reads (src/sound/soundboard.cpp, the mix's
	// line) in order to multiply itself. So the master volume is ABOVE the chip, like the two per-group
	// volumes and the effects leveller that the architecture's block 7 already puts in the board.
	//
	// WHICH IS A HOLE IN THE INTERFACE, and this test is where that is written down: scsp::core has no way
	// to return MVOL, so the board has to go down through ->device() to get it. It is one of the ->device()
	// calls that step 3 flagged as "what must go last". So a standalone core will have to either apply MVOL
	// itself -- which would change the sound -- or carry one more accessor. Until that choice is made,
	// measuring it avoids discovering it while wiring.
	{
		const auto [full, fd] = gain(7, 0, 0);
		(void) fd;
		Bench b;
		b.steady(WAVE, 256, STEADY);
		b.the_core.write_reg(0x400 / 2, 0x0200 | 0x0000, 0xFFFF);   // MVOL = 0, the silence asked for
		std::uint16_t word[16] = {};
		bare_mix(word);
		put(word, "DISDL", 7);
		put(word, "DIPAN", 0);
		b.key(0, word);
		const auto [silent_g, silent_d] = b.stereo(4);
		(void) silent_d;
		const float exposed = bench::exposed_gain(b.the_core);
		char t[152];
		snprintf(t, sizeof t, "MVOL=0: the chip still returns %d (against %d at MVOL=15), and exposes a gain of %.3f",
		         silent_g, full, double(exposed));
		(silent_g == full && exposed == 0.0f)
			? ok("MVOL: exposed, never applied in the chip", t)
			: failed("MVOL: exposed, never applied in the chip",
			         std::string(t) + "; expected an unchanged output and an exposed gain of 0");
	}

	// ---- 9. THE THREE TABLES OVERLAP, which validates them against each other
	// TL, DIPAN and SDLT are three separate tables in the code, but they share terms: -3, -6, -12 and -24
	// dB appear in more than one. If all three are right, the SAME attenuation asked for through three
	// different registers must return the SAME integer -- down to the last unit, since all three go through
	// the same FIX and the same shift.
	//
	// This is the only check in this file that rests on no recorded value: it stands on its own, and it
	// would catch a rewrite that changed one table without touching the other two.
	{
		const auto [tl3,   x1] = gain(7, 0x00, 0x08);   // TL bit 3: -3 dB
		const auto [pan3,  x2] = gain(7, 0x01, 0x00);   // DIPAN bit 0: -3 dB
		const auto [tl24,  x3] = gain(7, 0x00, 0x40);   // TL bit 6: -24 dB
		const auto [sdl24, x4] = gain(3, 0x00, 0x00);   // DISDL 3: -24 dB
		(void) x1; (void) x2; (void) x3; (void) x4;
		char t[144];
		snprintf(t, sizeof t, "-3 dB: TL %d = DIPAN %d; -24 dB: TL %d = DISDL %d", tl3, pan3, tl24, sdl24);
		(tl3 == pan3 && tl24 == sdl24)
			? ok("the same attenuation through three registers", t)
			: failed("the same attenuation through three registers",
			         std::string(t) + "; the three tables no longer agree");
	}

	printf("\n");
	if (failures) { printf("%d failure(s).\n", failures); return 1; }
	printf("all cases pass\n");
	return 0;
}
