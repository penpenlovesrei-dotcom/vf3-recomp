// test_scsp_adsr: the envelope, its four states and its two arithmetics, to the exact sample.
//
// Block 3 of docs/scsp_architecture.md §3 -- "ADSR envelopes: the four states, AR/D1R/D2R/RR, DL, EGHOLD,
// KRS, LPSLNK, and the time tables." With the document's warning, which is this file's real subject:
// "MAME's ARTimes/DRTimes tables are measured constants, not a formula; taking them over is a fidelity
// choice to be made explicitly."
//
// HOW THE ENVELOPE IS MEASURED ON ITS OWN. It is block 2's exact mirror. The slot test set the envelope
// aside with SDIR in order to read the oscillator; this one sets the oscillator aside with a STEADY wave
// in order to read the envelope. A constant source C gives `C * multiplier >> 12` at the output, then the
// direct mix (TL=0, DISDL=7, DIPAN=0) lets it through unchanged, then the adapter multiplies by 4. With
// C = 8192, two powers of two, the output is EXACTLY 8 times the multiplier -- which therefore reads back
// with no rounding, sample by sample.
//
// AND THERE ARE TWO ARITHMETICS, which is the block's central fact. In UpdateSlot:
//     if (state == ATTACK) sample = (sample * EG_Update(slot)) >> SHIFT;
//     else                 sample = (sample * m_EG_TABLE[EG_Update(slot) >> (SHIFT-10)]) >> SHIFT;
// The attack multiplies by the RAW VOLUME -- it is linear. Everything else goes through m_EG_TABLE, a
// decibel curve (3/32 dB per step, 95.9 dB in all). The two almost meet, but not quite: at saturation the
// attack gives 4092 and the table 4096. So the first sample of any voice comes out 4 thousandths lower
// than the next, and that is not measurement noise.
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
constexpr int STEADY = 8192;        // two powers of two: the output is 8 x the multiplier
constexpr int PER_MULT = 8;

// The settings common to every case: the envelope bare, full direct output centred, one source sample per
// output sample, and a loop long enough that the oscillator never stops.
void blank(std::uint16_t word[16])
{
	put(word, "SA_HI",  WAVE >> 16);
	put(word, "SA_LO",  WAVE & 0xFFFF);
	put(word, "DISDL",  7);
	put(word, "DIPAN",  0);
	put(word, "TL",     0);
	put(word, "OCT",    0);
	put(word, "FNS",    0);
	put(word, "LPCTL",  1);
	put(word, "LSA",    0);
	put(word, "LEA",    256);
	put(word, "KYONB",  1);
}

// The envelope's multiplier, read off sample by sample.
std::vector<int> envelope(Bench &b, unsigned n)
{
	const std::vector<std::int32_t> g = b.left(n);
	std::vector<int> m(n);
	for (unsigned i = 0; i < n; i++) m[i] = int(g[i] / PER_MULT);
	return m;
}

std::string joined(const std::vector<int> &v, unsigned howmany)
{
	std::string s;
	for (unsigned i = 0; i < howmany && i < v.size(); i++) {
		if (i) s += ",";
		s += std::to_string(v[i]);
	}
	return s;
}

// How many samples until the attack saturates: that is what encodes one entry of m_ARTABLE without having
// to read it.
//
// THE CRITERION IS THE CEILING, 4092, and not "the multiplier stops rising". The first version of this
// function stopped at the first sample that did not rise, and returned 1 for the slowest attack in the
// test: when the volume gains less than a quarter of a step per sample, `(volume >> 16) << 2` REPEATS its
// value without the attack being over. A criterion that goes more wrong the longer the measurement is, is
// worse than a wrong constant, because it looks as if it works on the short cases.
unsigned attack_duration(Bench &b, unsigned ceiling)
{
	for (unsigned i = 0; i < ceiling; i++)
		if (int(b.left(1)[0] / PER_MULT) >= 4092) return i + 1;
	return ceiling;
}

} // namespace

int main(int argc, char **argv)
{
	if (argc > 1) bench::core_name() = argv[1];
	printf("test_scsp_adsr: the envelope, through scsp::core\n\n");

	// ---- 1. THE START IS NOT ZERO, and the first step is not the same as the following ones
	// StartSlot sets `EG.volume = 0x17F << EG_SHIFT`, that is 383 out of 1023: a voice does not rise from
	// silence, it starts 37% of the way up. With AR=0x1F the attack jumps in one go, and the two
	// arithmetics are seen one after the other: 4092 (the raw volume, in the ATTACK state) then 4096 (the
	// table, from DECAY1 on).
	{
		Bench b;
		b.steady(WAVE, 256, STEADY);
		std::uint16_t word[16] = {};
		blank(word);
		put(word, "AR", 0x1F);
		put(word, "D1R", 0);
		put(word, "D2R", 0);
		put(word, "DL", 0);
		b.key(0, word);
		const std::vector<int> m = envelope(b, 6);
		const bool correct = m[0] == 4092 && m[1] == 4096 && m[2] == 4096 && m[3] == 4096;
		correct ? ok("ATTACK multiplies the volume, DECAY the table",
		             "4092 then 4096: " + joined(m, 6) + " (the gap is 4 in 4096, not noise)")
		        : failed("ATTACK multiplies the volume, DECAY the table",
		                 "expected 4092,4096,4096,4096; got " + joined(m, 6));
	}

	// ---- 2. THE STARTING VOLUME, read directly
	// With AR=0 the attack is infinite (m_ARTABLE[0] = 0, "Infinite time"), so the volume stays at its
	// initial value and the multiplier shows it: 383 << 2 = 1532. That is 0x17F, written in two places in
	// scsp.cpp, and a rewrite starting from zero would make every attack longer and softer.
	{
		Bench b;
		b.steady(WAVE, 256, STEADY);
		std::uint16_t word[16] = {};
		blank(word);
		put(word, "AR", 0);
		b.key(0, word);
		const std::vector<int> m = envelope(b, 4);
		bool flat = true;
		for (int v : m) if (v != m[0]) { flat = false; break; }
		(flat && m[0] == 0x17F * 4)
			? ok("AR=0: infinite attack, volume at 0x17F",
			     "multiplier " + std::to_string(m[0]) + " = 383 << 2, still over 4 samples")
			: failed("AR=0: infinite attack, volume at 0x17F",
			         "expected 1532 unmoving; got " + joined(m, 4));
	}

	// ---- 3. THE ATTACK'S DURATION, by AR: what the tables are worth, in samples
	// THIS IS THE CASE THAT MATTERS FOR THE DOCUMENT'S WARNING. ARTimes is a table of 64 constants measured
	// on hardware, not a curve: the gaps from one entry to the next follow no simple law. The durations
	// below are its observable consequence. A rewrite replacing the table with an exponential formula would
	// pass cases 1 and 2 and fail here -- which is exactly the point, because it is a CHOICE to be made with
	// open eyes and not an approximation to let slide.
	//
	// The count starts at `volume = 0x17F << 16` and stops when the multiplier stops rising, the step being
	// `m_ARTABLE[2 * AR]` (KRS=0, OCT=0, FNS=0 give a base rate of zero).
	{
		struct Case { unsigned ar; unsigned expected; };
		const Case cases[] = { { 0x10, 1297 }, { 0x14, 332 }, { 0x18, 83 }, { 0x1C, 24 }, { 0x1F, 1 } };
		std::string lines;
		bool correct = true;
		for (const Case &c : cases) {
			Bench b;
			b.steady(WAVE, 256, STEADY);
			std::uint16_t word[16] = {};
			blank(word);
			put(word, "AR", c.ar);
			put(word, "D1R", 0);
			put(word, "D2R", 0);
			put(word, "DL", 0);
			b.key(0, word);
			const unsigned n = attack_duration(b, 4000);
			char t[64];
			snprintf(t, sizeof t, "%sAR=%02x->%u", lines.empty() ? "" : " ", c.ar, n);
			lines += t;
			if (n != c.expected) { correct = false; lines += "(expected " + std::to_string(c.expected) + ")"; }
		}
		correct ? ok("the attack's duration, in samples", lines)
		        : failed("the attack's duration, in samples", lines);
	}

	// ---- 4. EGHOLD: the attack is at FULL from the very first sample
	// In the ATTACK state, `if (EG.EGHOLD) return 0x3ff << (SHIFT-10)` comes BEFORE the normal computation:
	// the volume still rises internally, but the output is at full throughout. So a slow attack becomes
	// inaudible as an attack -- it serves only to delay the move into DECAY1.
	{
		Bench b;
		b.steady(WAVE, 256, STEADY);
		std::uint16_t word[16] = {};
		blank(word);
		put(word, "AR", 0x18);      // slow: 83 samples without EGHOLD (case 3)
		put(word, "EGHOLD", 1);
		put(word, "D1R", 0);
		put(word, "D2R", 0);
		put(word, "DL", 0);
		b.key(0, word);
		const std::vector<int> m = envelope(b, 8);
		bool full = true;
		for (int v : m) if (v != 4092) { full = false; break; }
		full ? ok("EGHOLD: full output during the attack",
		          "4092 from the first sample, and over all 8: " + joined(m, 4) + ",...")
		     : failed("EGHOLD: full output during the attack",
		              "expected 4092 throughout; got " + joined(m, 8));
	}

	// ---- 5. DL IS INVERTED, and DL=0 skips DECAY1 entirely
	// `EG.DL = 0x1f - DL(slot)`, and DECAY1 stops when `volume >> 21 <= EG.DL`. So DL=0 gives EG.DL=31, a
	// condition true from the first tick: DECAY1 does not last a sample. At the other end, DL=0x1F gives
	// EG.DL=0 and DECAY1 falls all the way down. In between, the plateau where D2R takes over reads in the
	// output -- with D2R=0 it freezes there, DECAY2 handing back without decrementing anything.
	{
		struct Case { unsigned dl; int plateau; };
		// The plateau is m_EG_TABLE at the index where DECAY1 gave way, that is (0x1F - DL) * 32 + 31: 4052
		// for an index of 1022, 258 for 767, 16 for 511, 1 for 255.
		//
		// AND DL=0 DOES NOT GIVE 4096 BUT 4052, which is this case's exact detail: the exit condition is
		// true from DECAY1's first tick, but it is tested AFTER `volume -= EG.D1R`. So DECAY1 ALWAYS
		// decrements once, even when it does not last. One index lost, 0.09 dB -- and a rewrite testing
		// before decrementing would give 4096 here and pass all the rest.
		const Case cases[] = { { 0x00, 4052 }, { 0x08, 258 }, { 0x10, 16 }, { 0x18, 1 } };
		std::string lines;
		bool correct = true;
		for (const Case &c : cases) {
			Bench b;
			b.steady(WAVE, 256, STEADY);
			std::uint16_t word[16] = {};
			blank(word);
			put(word, "AR", 0x1F);      // at full straight away
			put(word, "D1R", 0x14);     // a clear fall but not an instant one
			put(word, "D2R", 0);        // DECAY2 freezes the plateau
			put(word, "DL", c.dl);
			b.key(0, word);
			// 20000 samples, and that figure is a measurement and not a margin: with D1R=0x14 the volume
			// loses 8943 per sample, that is 0.136 of a table index, so falling through the 1023 indices
			// takes 7500 samples. The first version ran 3000 and read DECAY1 STILL IN PROGRESS -- DL=0x10
			// and DL=0x18 then gave the same plateau of 49, which was nobody's plateau.
			const std::vector<int> m = envelope(b, 20000);
			const int last = m.back();
			char t[64];
			snprintf(t, sizeof t, "%sDL=%02x->%d", lines.empty() ? "" : " ", c.dl, last);
			lines += t;
			if (last != c.plateau) { correct = false; lines += "(expected " + std::to_string(c.plateau) + ")"; }
		}
		correct ? ok("DL inverted: the plateau where D2R takes over", lines)
		        : failed("DL inverted: the plateau where D2R takes over", lines);
	}

	// ---- 6. D2R=0 DOES NOT DECREMENT, and it is the REGISTER that is tested
	// `case SCSP_DECAY2: if (D2R(slot) == 0) return ...` consults D2R(slot), the register's field, and not
	// EG.D2R, the value the table gave. The two do not say the same thing: a non-zero D2R can give a zero
	// EG.D2R (m_DRTABLE at a low rate), and the output would then NOT freeze, it would fall by zero per
	// sample -- which sounds the same but is not written the same. So both edges are measured: D2R=0
	// freezes, D2R=1 moves.
	{
		std::string lines;
		bool correct = true;
		// D2R=0x10 and not 1: with D2R=1 the volume loses 12 per sample, that is one table index every
		// 5461 samples -- the fall exists but does not show in a measurement of reasonable size, and the
		// first version of this case concluded "should have moved" out of its own impatience.
		for (auto [d2r, moves] : { std::pair<unsigned, bool>{ 0, false }, std::pair<unsigned, bool>{ 0x10, true } }) {
			Bench b;
			b.steady(WAVE, 256, STEADY);
			std::uint16_t word[16] = {};
			blank(word);
			put(word, "AR", 0x1F);
			put(word, "D1R", 0);
			put(word, "D2R", d2r);
			put(word, "DL", 0);         // DECAY1 gives way straight away
			b.key(0, word);
			const std::vector<int> m = envelope(b, 4000);
			const bool moved = m.back() != m[1];
			char t[80];
			snprintf(t, sizeof t, "%sD2R=%02x: %d -> %d", lines.empty() ? "" : " | ", d2r, m[1], m.back());
			lines += t;
			if (moved != moves) { correct = false; lines += moved ? " (should have frozen)" : " (should have moved)"; }
		}
		correct ? ok("D2R=0 freezes the plateau, D2R=1 makes it fall", lines)
		        : failed("D2R=0 freezes the plateau, D2R=1 makes it fall", lines);
	}

	// ---- 7. LPSLNK: the attack ends when LSA is passed, not at saturation
	// The only place in block 3 that depends on block 2. In ATTACK, the move into DECAY1 is enclosed in
	// `if (!LPSLNK(slot))`: with LPSLNK the state DOES NOT CHANGE when the volume saturates, it stays
	// ATTACK. It only leaves through the oscillator, in the looping loop: `if (addr1 >= LSA && !Backwards)
	// { if (LPSLNK && state == ATTACK) state = DECAY1; }`. So it is seen through the arithmetic: as long as
	// we are in ATTACK the output is 4092 (the raw volume), and it goes to 4096 (the table) at the sample
	// where the read head reaches LSA. With LSA=20, that makes twenty samples at 4092.
	{
		Bench b;
		b.steady(WAVE, 256, STEADY);
		std::uint16_t word[16] = {};
		blank(word);
		put(word, "AR", 0x1F);
		put(word, "LPSLNK", 1);
		put(word, "LSA", 20);
		put(word, "LEA", 24);
		put(word, "D1R", 0);
		put(word, "D2R", 0);
		put(word, "DL", 0);
		b.key(0, word);
		const std::vector<int> m = envelope(b, 32);
		unsigned n = 0;
		while (n < m.size() && m[n] == 4092) n++;
		// NINETEEN AND NOT TWENTY, and the count says why: in UpdateSlot the LPSLNK test happens BEFORE the
		// envelope is applied. At the sample where the head reaches LSA=20, the state is already DECAY1
		// when the gain is computed, and that sample therefore comes out through the table. Nineteen
		// samples are left in ATTACK.
		(n == 19) ? ok("LPSLNK: ATTACK holds until LSA",
		               std::to_string(n) + " samples at 4092 (LSA=20, the twentieth is already DECAY1: "
		               + std::to_string(m[n]) + ")")
		          : failed("LPSLNK: ATTACK holds until LSA",
		                   "expected 19 samples at 4092; got " + std::to_string(n)
		                   + ", sequence " + joined(m, 24));
	}

	// ---- 8. KRS: the scaling by the note, and the 0xF that cancels it
	// `rate = octave + 2 * KRS + ((FNS >> 9) & 1)`, except KRS=0xF which gives rate = 0 outright -- not the
	// octave, not FNS's bit: ZERO. To see it a non-zero octave is needed, otherwise KRS=0 and KRS=0xF are
	// indistinguishable; so OCT=2 is used. The rate is added to 2*AR before reading the table, so the same
	// AR gives attacks of different durations depending on the note -- which is the whole purpose of KRS.
	{
		std::string lines;
		bool correct = true;
		struct Case { unsigned krs; unsigned fns; unsigned expected; };
		// KRS=0xF gives 83, exactly case 3's duration at OCT=0: the proof that the 0xF cancels the octave
		// and does not merely half-ignore it.
		const Case cases[] = { { 0x0, 0x000, 56 }, { 0x1, 0x000, 45 }, { 0x0, 0x200, 50 }, { 0xF, 0x000, 83 } };
		for (const Case &c : cases) {
			Bench b;
			b.steady(WAVE, 1024, STEADY);
			std::uint16_t word[16] = {};
			blank(word);
			put(word, "OCT", 2);        // (OCT ^ 8) - 8 = 2
			put(word, "FNS", c.fns);
			put(word, "LEA", 1000);     // octave 2 advances four times faster
			put(word, "AR", 0x18);
			put(word, "KRS", c.krs);
			put(word, "D1R", 0);
			put(word, "D2R", 0);
			put(word, "DL", 0);
			b.key(0, word);
			const unsigned n = attack_duration(b, 4000);
			char t[80];
			snprintf(t, sizeof t, "%sKRS=%x FNS=%03x -> %u", lines.empty() ? "" : " ", c.krs, c.fns, n);
			lines += t;
			if (n != c.expected) { correct = false; lines += "(expected " + std::to_string(c.expected) + ")"; }
		}
		correct ? ok("KRS scales, KRS=0xF cancels everything", lines)
		        : failed("KRS scales, KRS=0xF cancels everything", lines);
	}

	// ---- 9. RELEASE: the fall, then the slot goes out FOR GOOD
	// Key-off does not cut the sound, it moves the envelope into RELEASE (StopSlot with keyoff=1). The slot
	// only becomes inactive when the volume reaches zero, and it is then StopSlot(slot, 0) that sets
	// `active = 0`. Two things to check, then: the fall happens, and it ends in a real silence and not on a
	// floor.
	{
		Bench b;
		b.steady(WAVE, 256, STEADY);
		std::uint16_t word[16] = {};
		blank(word);
		put(word, "AR", 0x1F);
		put(word, "D1R", 0);
		put(word, "D2R", 0);
		put(word, "DL", 0);
		put(word, "RR", 0x18);
		b.key(0, word);
		const int before = envelope(b, 4)[3];
		// key-off: KYONB at zero and KYONEX, which sweeps the 32 slots
		std::uint16_t off[16] = {};
		put(off, "KYONEX", 1);
		b.the_core.write_reg(0, off[0], 0xFFFF);
		unsigned n = 0;
		int last_v = -1;
		for (; n < 4000; n++) {
			last_v = int(b.left(1)[0] / PER_MULT);
			if (last_v == 0) break;
		}
		const std::vector<int> after = envelope(b, 8);
		bool silent = true;
		for (int v : after) if (v != 0) { silent = false; break; }
		char t[128];
		snprintf(t, sizeof t, "4096 before, zero after %u samples, and silent from then on", n);
		(before == 4096 && n == 251 && silent)
			? ok("RELEASE falls, then the slot goes out", t)
			: failed("RELEASE falls, then the slot goes out",
			         "before " + std::to_string(before) + ", zero after " + std::to_string(n)
			         + " (expected 251), silent afterwards: " + (silent ? "yes" : "no"));
	}

	// ---- 10. A SECOND KYONEX DOES NOT RETRIGGER a voice that is playing
	// `if (KEYONB(s2) && s2->EG.state == SCSP_RELEASE) StartSlot(s2)`: only a slot IN RELEASE is
	// (re)started. So a slot that is playing ignores the trigger, and its volume does not go back to 0x17F.
	// A rewrite restarting on every KYONEX would make all the game's repeated notes snap.
	{
		Bench b;
		b.steady(WAVE, 256, STEADY);
		std::uint16_t word[16] = {};
		blank(word);
		put(word, "AR", 0x1F);
		put(word, "D1R", 0);
		put(word, "D2R", 0);
		put(word, "DL", 0);
		b.key(0, word);
		const int settled = envelope(b, 4)[3];
		b.key(0, word);                      // the same trigger, a second time
		const std::vector<int> after = envelope(b, 4);
		const bool unchanged = settled == 4096 && after[0] == 4096 && after[1] == 4096;
		unchanged ? ok("a second KYONEX retriggers nothing",
		               "4096 before and after: " + joined(after, 4) + " (no return to 1532)")
		          : failed("a second KYONEX retriggers nothing",
		                   "before " + std::to_string(settled) + ", after " + joined(after, 4)
		                   + "; a return to 1532 would be a restart");
	}

	printf("\n");
	if (failures) { printf("%d failure(s).\n", failures); return 1; }
	printf("all cases pass\n");
	return 0;
}
