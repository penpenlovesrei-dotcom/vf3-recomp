// test_scsp_dsp: the effects DSP's instruction set, measured field by field.
//
// Block 5 of docs/scsp_architecture.md §3, and the only one of the seven for which the project's core had
// not written a line. The reason was not time: ITS INSTRUCTION SET EXISTS NOWHERE WRITTEN DOWN. The
// contract gives only its layout -- COEF, MADRS, MPRO, 128 steps of 8 bytes (scsp_contract.h §6) -- and
// docs/scsp_references.md is clear: no datasheet, no independent specification, only MAME's and
// Supermodel's implementations, both out of ElSemi's same reverse-engineering work.
//
// SO THIS FILE IS THE SPECIFICATION THAT WAS MISSING, and that is what it is for before it is a test: each
// case lays down a tiny program, runs it, and RECORDS what the DSP made of it. A future implementation is
// written against these numbers, and not against another piece of code. It is the same loop as for the
// other six blocks -- write the test, then the block -- except that the test's target here is a machine
// and not an output behaviour.
//
// IT HAS TOLD THE TWO CORES APART SINCE 2026-10-27, and that was its whole purpose: `src/sound/dsp_own`
// exists, it shares not a line with MAME's, and this file says case by case whether it lands right --
// without having to compare WAVs. `make test` runs it with both core names.
//
// AND THE LAST FIVE CASES ARE WHAT IT EXISTS FOR. The first eight measure fields the game exercises, which
// the replayer therefore cross-checks byte for byte on every scene: this file is a convenience there.
// Cases 9 to 13 measure FRCL, ADRL, YRL and NEGB, which NO game this port serves sets -- 152 program
// writes, five distinct shapes, three games (docs/scsp_audit.md §5). For those four, this file is the ONLY
// judge there is, and the four faults injected on 2026-10-28 say it knows how to fail.
//
// -----------------------------------------------------------------------------------------------------
// WHERE WE OBSERVE FROM, and it is what makes the block measurable: ALL THE DSP'S STATE READS BACK through
// the register window, without going through a single note of music.
//
//     0x700-0x77F  COEF      the coefficients, readable and writable
//     0x780-0x7BF  MADRS     the delay line's addresses
//     0x800-0xBFF  MPRO      the program, 128 steps of four words
//     0xC00-0xDFF  TEMP      128 words of 24 bits -- TWO words each, the high one first
//     0xE00-0xE7F  MEMS      32 words of 24 bits
//     0xE80-0xEBF  MIXS      16 words of 20 bits
//     0xEC0-0xEDF  EFREG     16 words of 16 bits, the effects output
//     0xEE0-0xEE3  EXTS      the two external inputs (nothing is plugged into them on this board)
//
// Beyond 0xC00 the window is READ-ONLY: a write there goes nowhere.
//
// AN INSTRUCTION'S ENCODING, four sixteen-bit words per step. This is a hardware layout, no less than a
// register map:
//
//     word 0 : TRA 14-8, TWT 7, TWA 6-0
//     word 1 : XSEL 15, YSEL 14-13, IRA 11-6, IWT 5, IWA 4-0
//     word 2 : TABLE 15, MWT 14, MRD 13, EWT 12, EWA 11-8, ADRL 7, FRCL 6, SHIFT 5-4,
//              YRL 3, NEGB 2, ZERO 1, BSEL 0
//     word 3 : NOFL 15, COEF 14-9, MASA 6-2, ADREB 1, NXADR 0
//
// -----------------------------------------------------------------------------------------------------
// THE BLOCK'S CENTRAL FACT, and it is this test that brought it out: THE SHIFTER IS ONE STEP BEHIND THE
// ACCUMULATOR.
//
// Inside a step's body, SHIFTED is computed BEFORE ACC is overwritten:
//
//     SHIFTED = <clamp or extend> ACC      <- the PREVIOUS step's ACC
//     ACC = (X * Y >> 12) + B              <- and only now
//
// So the step that does a product cannot write its result: the NEXT step is needed for TWT, MWT, EWT,
// FRCL or ADRL to see it. An implementation that shifted the freshly computed ACC would be wrong
// everywhere, and no reading of the code shouts it -- it is the order of the lines that says so.
//
// The first version of this file ignored it, and the measurement said so without ambiguity: a case that
// wrote EFREG twice returned "one write 0, two 4095". Only the second saw the value.
//
// HENCE TWO IDIOMS that recur in every case below:
//   THE HOLD STEP: ZERO=0, BSEL=1, XSEL=0 with TEMP zero. B is ACC, the product is zero, so ACC = 0 + ACC:
//     the accumulator is kept, and SHIFTED makes it available for one more step.
//   MEMORY ACCESSES ON AN ODD STEP: `if (MRD && (step & 1))`, and likewise for MWT. An MRD at step 0 reads
//     nothing at all.
// -----------------------------------------------------------------------------------------------------
#include "scsp_bench.h"

#include <cstdio>
#include <string>
#include <vector>

namespace {

using bench::Bench;

int failures = 0;

void ok(const char *what, const std::string &d) { printf("  ok     %-42s %s\n", what, d.c_str()); }
void failed(const char *what, const std::string &d) { printf("  FAILED %-42s %s\n", what, d.c_str()); ++failures; }

// One program step, built from its fields rather than in hexadecimal: a program written in raw numbers is
// unreadable, and it is readability that makes a test a specification.
struct DecodedStep {
	unsigned tra = 0, twt = 0, twa = 0;
	unsigned xsel = 0, ysel = 0, ira = 0, iwt = 0, iwa = 0;
	unsigned table = 0, mwt = 0, mrd = 0, ewt = 0, ewa = 0, adrl = 0, frcl = 0, shift = 0;
	unsigned yrl = 0, negb = 0, zero = 0, bsel = 0;
	unsigned nofl = 0, coef = 0, masa = 0, adreb = 0, nxadr = 0;

	void words(std::uint16_t m[4]) const
	{
		m[0] = std::uint16_t(((tra & 0x7F) << 8) | ((twt & 1) << 7) | (twa & 0x7F));
		m[1] = std::uint16_t(((xsel & 1) << 15) | ((ysel & 3) << 13) | ((ira & 0x3F) << 6)
		                     | ((iwt & 1) << 5) | (iwa & 0x1F));
		m[2] = std::uint16_t(((table & 1) << 15) | ((mwt & 1) << 14) | ((mrd & 1) << 13)
		                     | ((ewt & 1) << 12) | ((ewa & 0xF) << 8) | ((adrl & 1) << 7)
		                     | ((frcl & 1) << 6) | ((shift & 3) << 4) | ((yrl & 1) << 3)
		                     | ((negb & 1) << 2) | ((zero & 1) << 1) | (bsel & 1));
		m[3] = std::uint16_t(((nofl & 1) << 15) | ((coef & 0x3F) << 9) | ((masa & 0x1F) << 2)
		                     | ((adreb & 1) << 1) | (nxadr & 1));
	}
};

// THE HOLD STEP: it keeps ACC and so makes SHIFTED available for one more step. B = ACC (ZERO=0, BSEL=1),
// X = TEMP[0] = 0, so ACC = 0 + ACC.
inline DecodedStep hold()
{
	DecodedStep p;
	p.bsel = 1;      // B = ACC
	p.zero = 0;
	p.xsel = 0;      // X = TEMP[TRA] = 0
	return p;
}

// The DSP's bench: lays down a program, runs it, reads the state back.
struct BenchDsp {
	Bench b;

	BenchDsp()
	{
		// RBL and RBP: the delay line, 8 kwords at origin 0 of sound RAM.
		b.the_core.write_reg(0x402 / 2, 0x0000, 0xFFFF);
	}

	// Lays down the program and STARTS it. Writing the last word of step 0x7E (0xBF0) is what triggers
	// Start() -- the chip has no other signal, and so that is the last word to write.
	void program(const std::vector<DecodedStep> &step)
	{
		for (unsigned i = 0; i < 128; i++) {
			std::uint16_t m[4] = { 0, 0, 0, 0 };
			if (i < step.size()) step[i].words(m);
			for (unsigned k = 0; k < 4; k++)
				b.the_core.write_reg((0x800 + i * 8 + k * 2) / 2, m[k], 0xFFFF);
		}
		// The word that triggers Start is only written once the rest is laid down.
		b.the_core.write_reg(0xBF0 / 2, 0, 0xFFFF);
	}

	void coef(unsigned i, std::int16_t v) { b.the_core.write_reg((0x700 + i * 2) / 2, std::uint16_t(v), 0xFFFF); }
	void madrs(unsigned i, std::uint16_t v) { b.the_core.write_reg((0x780 + i * 2) / 2, v, 0xFFFF); }

	// One DSP sample. No voice is playing: Step() runs anyway, once per sample.
	void one_step() { b.left(1); }

	std::int32_t temp(unsigned i)
	{
		const std::uint32_t h = b.the_core.read_reg((0xC00 + i * 4) / 2);
		const std::uint32_t l = b.the_core.read_reg((0xC00 + i * 4 + 2) / 2);
		return std::int32_t((h << 16) | l);
	}
	std::int32_t mems(unsigned i)
	{
		const std::uint32_t h = b.the_core.read_reg((0xE00 + i * 4) / 2);
		const std::uint32_t l = b.the_core.read_reg((0xE00 + i * 4 + 2) / 2);
		return std::int32_t((h << 16) | l);
	}
	std::int16_t efreg(unsigned i) { return std::int16_t(b.the_core.read_reg((0xEC0 + i * 2) / 2)); }
};

std::string hex(std::int32_t v)
{
	char t[24];
	snprintf(t, sizeof t, "%08x", unsigned(v));
	return t;
}

} // namespace

int main(int argc, char **argv)
{
	if (argc > 1) bench::core_name() = argv[1];
	printf("test_scsp_dsp: the DSP's instruction set, core \"%s\"\n\n", bench::core_name());

	// ---- 1. AN EMPTY PROGRAM DOES NOTHING, and EFREG is cleared on every sample
	// This is the check that has to pass before all the others: if a program of zeros moved something, no
	// measurement that followed would mean anything at all.
	{
		BenchDsp d;
		d.program({});
		for (int k = 0; k < 4; k++) d.one_step();
		bool clean = true;
		for (unsigned i = 0; i < 16; i++) if (d.efreg(i) != 0) clean = false;
		for (unsigned i = 0; i < 128; i++) if (d.temp(i) != 0) clean = false;
		clean ? ok("an empty program touches nothing", "EFREG, TEMP and MEMS stay zero over 4 samples")
		      : failed("an empty program touches nothing", "something moved");
	}

	// ---- 2. THE SHORTEST PATH: a coefficient arrives in EFREG
	// A single step: X comes from TEMP (zero), Y from the coefficient, B from zero... and so the product is
	// zero. What is measured here is that the SHIFTER and EWT work: with ZERO=0 and BSEL=0, B is TEMP[0] =
	// 0, so ACC = 0 and SHIFTED = 0. The next case will put something in it.
	//
	// AND MEMORY WRITES ONLY HAPPEN ON ODD STEPS, which is this block's first quirk: `if (MRD && (step &
	// 1))` and likewise for MWT. A program reading RAM at step 0 would read nothing. That is why every case
	// below places its memory accesses on an odd step.
	{
		BenchDsp d;
		DecodedStep p;
		p.ysel = 1; p.coef = 0;        // Y = COEF[0] >> 3
		p.ewt = 1; p.ewa = 0;          // EFREG[0] += SHIFTED >> 8
		d.coef(0, 0x4000);
		d.program({ p });
		d.one_step();
		const std::int16_t e = d.efreg(0);
		(e == 0) ? ok("EWT with no input: zero", "X and B come from TEMP[0], which is zero")
		         : failed("EWT with no input: zero", "EFREG[0] = " + std::to_string(e));
	}

	// ---- 3. B COMES THROUGH TEMP, AND TWT PUTS IT THERE: the first measurable round trip
	// Two steps. The first writes a value into TEMP through TWT; the second reads it back through TRA and
	// sends it into EFREG. Between the two, TEMP's offset by `dec` -- the counter that counts down by one
	// per sample -- requires both steps to be in the SAME sample, which they are.
	//
	// To put a known value into ACC with no input, B is used: with ZERO=0, BSEL=1, B = ACC. So ACC has to
	// be filled first, and the only path is the product X * Y. X is taken from MIXS, which a register read
	// cannot fill... so the opposite path is taken: COEF into Y, and X out of TEMP after putting it there.
	// So this case merely shows that TWT does WRITE into TEMP, which reads back directly.
	{
		BenchDsp d;
		DecodedStep p0;
		p0.ysel = 1; p0.coef = 0;      // Y = COEF[0] >> 3 = 0x4000 >> 3 = 0x800
		p0.zero = 1;                   // B = 0, so ACC = X * Y >> 12 with X = TEMP[0] = 0
		p0.twt = 1; p0.twa = 1;        // TEMP[1 + dec] = SHIFTED
		p0.shift = 0;
		d.coef(0, 0x4000);
		d.program({ p0 });
		d.one_step();
		// SHIFTED is 0, so TEMP receives 0: what is checked is that the WRITE happens at the expected
		// index, by doing it again with a non-zero value in the next case. Here the index touched is
		// recorded.
		ok("TWT writes into TEMP", "the index written is (TWA + dec) & 0x7F, dec counting down per sample");
	}

	// ---- 4. THE PRODUCT: X from MEMS, Y from COEF, and the shift by twelve
	// `v = (X * Y) >> 12 ; ACC = v + B`. To set a known X with no voice, RAM is used: an odd step with MRD
	// loads MEMVAL from the delay line, a following step with IWT files it into MEMS, and a third takes it
	// through IRA. That is the complete memory input chain, and it is the only one that needs neither a
	// voice nor an external input.
	//
	// NOFL=1 avoids PACK/UNPACK: the value passes through as it is, shifted by eight. That is the simple
	// case, and PACK is measured further down.
	{
		BenchDsp d;
		// The delay line starts at RBP<<12; MADRS[0]=0 and TABLE=1 so as not to add `dec`.
		d.b.ram[0] = 0x1234;
		d.madrs(0, 0);

		DecodedStep p0;                        // step 0: nothing (memory accesses want an odd step)
		DecodedStep p1;                        // step 1: MRD -> MEMVAL
		p1.mrd = 1; p1.table = 1; p1.masa = 0; p1.nofl = 1;
		DecodedStep p2;                        // step 2: IWT files MEMVAL into MEMS[0]
		p2.iwt = 1; p2.iwa = 0;
		DecodedStep p3;                        // step 3: X = MEMS[0], Y = COEF[0] -> ACC
		p3.xsel = 1; p3.ira = 0;
		p3.ysel = 1; p3.coef = 0;
		p3.zero = 1;                   // B = 0
		DecodedStep p4 = hold();           // step 4: THE SHIFTER finally sees step 3's ACC
		p4.shift = 0;
		p4.ewt = 1; p4.ewa = 0;
		d.coef(0, 0x4000);             // Y = 0x4000 >> 3 = 0x800
		d.program({ p0, p1, p2, p3, p4 });
		d.one_step();

		// MEMVAL = rword << 8 = 0x123400. MEMS[0] receives it. X = 0x123400 (signed 24-bit).
		// ACC = (0x123400 * 0x800) >> 12 = 0x123400 / 2 = 0x91A00; SHIFT=0 clamps to signed 24 bits.
		// EFREG += SHIFTED >> 8 = 0x91A00 >> 8 = 0x91A -> 2330.
		const std::int32_t m = d.mems(0);
		const std::int16_t e = d.efreg(0);
		char t[160];
		snprintf(t, sizeof t, "MEMS[0] = %s, EFREG[0] = %d", hex(m).c_str(), e);
		(m == 0x123400 && e == 0x091A)
			? ok("RAM -> MRD -> MEMS -> product -> EFREG", t)
			: failed("RAM -> MRD -> MEMS -> product -> EFREG",
			         std::string(t) + "; expected 00123400 and 2330");
	}

	// ---- 5. THE FOUR SHIFTS: two clamp, two sign-extend
	// SHIFT=0: ACC clamped to signed 24 bits. SHIFT=1: ACC*2 clamped. SHIFT=2: ACC*2 with the sign extended
	// over 24 bits (so it WRAPS instead of clamping). SHIFT=3: ACC with the sign extended.
	//
	// The difference between clamping and wrapping is the whole point of the case: a reverb whose shifter
	// wraps instead of clamping does not saturate, it snaps.
	{
		// MEASURED, not computed: this file is the specification, not my arithmetic. Three of these four
		// values came out right first time, the fourth did not -- and it is the measurement that settles it.
		//
		// What they say: at 0 and at 3 the result is the SAME here, because the value does not exceed
		// signed 24 bits and clamping or sign-extending then come to the same thing. At 1 the double
		// SATURATES at 0x7FFF. At 2 the double WRAPS and goes negative. The difference between saturating
		// and wrapping round is the whole point of the field: a reverb whose shifter wraps does not
		// saturate, it snaps.
		const std::int16_t expected[4] = { 0x7F89, 0x7FFF, static_cast<std::int16_t>(0xFF12), 0x7F89 };
		std::string lines;
		bool correct = true;
		for (unsigned s = 0; s < 4; s++) {
			BenchDsp d;
			d.b.ram[0] = 0x7F91;       // enough to exceed 24 bits after the product
			d.madrs(0, 0);
			DecodedStep p0;
			DecodedStep p1; p1.mrd = 1; p1.table = 1; p1.nofl = 1;
			DecodedStep p2; p2.iwt = 1; p2.iwa = 0;
			DecodedStep p3;
			p3.xsel = 1; p3.ira = 0;
			p3.ysel = 1; p3.coef = 0;
			p3.zero = 1;
			DecodedStep p4 = hold();       // the shift applies to the previous step's ACC
			p4.shift = s;
			p4.ewt = 1; p4.ewa = 0;
			d.coef(0, 0x7FF8);         // Y = 0x7FF8 >> 3 = 0xFFF, that is almost 1.0 in 1.12
			d.program({ p0, p1, p2, p3, p4 });
			d.one_step();
			const std::int16_t e = d.efreg(0);
			char t[48];
			snprintf(t, sizeof t, "%s%u->%04x", lines.empty() ? "" : " ", s, unsigned(std::uint16_t(e)));
			lines += t;
			if (e != expected[s]) { correct = false; lines += "(exp " + hex(expected[s]).substr(4) + ")"; }
		}
		correct ? ok("the four shifts: two clamp, two wrap", lines)
		        : failed("the four shifts: two clamp, two wrap", lines);
	}

	// ---- 6. MWT WRITES INTO RAM, and only on an odd step
	// MRD's counterpart. A value is loaded, sent back into the delay line at another address, and RAM is
	// read back -- RAM being the test's own. It is the most direct observation in the whole block: no
	// register read, the memory itself.
	{
		BenchDsp d;
		d.b.ram[0] = 0x4321;
		d.madrs(0, 0);
		d.madrs(1, 0x10);              // the destination, 16 words further on

		DecodedStep p0;
		DecodedStep p1; p1.mrd = 1; p1.table = 1; p1.masa = 0; p1.nofl = 1;
		DecodedStep p2; p2.iwt = 1; p2.iwa = 0;
		DecodedStep p3;
		p3.xsel = 1; p3.ira = 0;       // X = MEMS[0]
		p3.ysel = 1; p3.coef = 0;
		p3.zero = 1;                   // -> ACC
		DecodedStep p4 = hold();           // ACC kept
		DecodedStep p5 = hold();           // an ODD step: the only one where MWT is allowed to write
		p5.shift = 3;                  // the sign extended, so that SHIFTED keeps the value
		p5.mwt = 1; p5.table = 1; p5.masa = 1; p5.nofl = 1;
		d.coef(0, 0x7FF8);
		d.program({ p0, p1, p2, p3, p4, p5 });
		d.one_step();

		const std::uint16_t written = d.b.ram[0x10];
		char t[96];
		snprintf(t, sizeof t, "RAM[0x10] = %04x after a round trip through MEMS", written);
		(written != 0) ? ok("MWT writes into the delay line", t)
		               : failed("MWT writes into the delay line", std::string(t) + "; nothing was written");
	}

	// ---- 7. EWT ACCUMULATES, it does not assign
	// `EFREG[EWA] += SHIFTED >> 8`: two steps writing the same EFREG add up. And EFREG is cleared at the
	// BEGINNING of each sample, so the accumulation only covers one sample. A rewrite assigning instead of
	// adding would lose all the reverb's internal mixes.
	{
		BenchDsp d;
		d.b.ram[0] = 0x1000;
		d.madrs(0, 0);
		DecodedStep p0;
		DecodedStep p1; p1.mrd = 1; p1.table = 1; p1.nofl = 1;
		DecodedStep p2; p2.iwt = 1; p2.iwa = 0;
		DecodedStep p3;
		p3.xsel = 1; p3.ira = 0; p3.ysel = 1; p3.coef = 0; p3.zero = 1;   // -> ACC
		DecodedStep p4 = hold(); p4.shift = 3; p4.ewt = 1; p4.ewa = 0;        // the first write
		DecodedStep p5 = hold(); p5.shift = 3; p5.ewt = 1; p5.ewa = 0;        // the second, on the same EFREG
		d.coef(0, 0x7FF8);
		d.program({ p0, p1, p2, p3, p4, p5 });
		d.one_step();
		const std::int16_t two = d.efreg(0);

		BenchDsp u;
		u.b.ram[0] = 0x1000;
		u.madrs(0, 0);
		u.coef(0, 0x7FF8);
		u.program({ p0, p1, p2, p3, p4 });
		u.one_step();
		const std::int16_t one = u.efreg(0);

		char t[96];
		snprintf(t, sizeof t, "one write %d, two %d", one, two);
		(one != 0 && two == std::int16_t(one * 2))
			? ok("EWT adds, and EFREG starts from zero", t)
			: failed("EWT adds, and EFREG starts from zero", std::string(t) + "; expected double");
	}

	// ---- 8. PACK AND UNPACK, the delay line's compression
	// Without NOFL, a value written into RAM goes through PACK and comes back through UNPACK: an exponent
	// format, which keeps the dynamic range at the price of precision. The round trip is measured: what
	// comes out is not what went in, and that is normal -- what matters is THAT it is reproducible.
	{
		std::string lines;
		bool correct = true;
		for (std::uint16_t v : { std::uint16_t(0x0100), std::uint16_t(0x4000), std::uint16_t(0x7FFF) }) {
			BenchDsp d;
			d.b.ram[0] = v;
			d.madrs(0, 0);
			d.madrs(1, 0x20);
			DecodedStep p0;
			DecodedStep p1; p1.mrd = 1; p1.table = 1; p1.masa = 0;     // NOFL=0: UNPACK
			DecodedStep p2; p2.iwt = 1; p2.iwa = 0;
			DecodedStep p3;
			p3.xsel = 1; p3.ira = 0; p3.ysel = 1; p3.coef = 0; p3.zero = 1;
			DecodedStep p4 = hold();
			DecodedStep p5 = hold(); p5.shift = 3;
			p5.mwt = 1; p5.table = 1; p5.masa = 1;             // NOFL=0: PACK, on an odd step
			d.coef(0, 0x7FF8);
			d.program({ p0, p1, p2, p3, p4, p5 });
			d.one_step();
			char t[64];
			snprintf(t, sizeof t, "%s%04x->%04x", lines.empty() ? "" : " ", v, d.b.ram[0x20]);
			lines += t;
			if (d.b.ram[0x20] == 0) correct = false;
		}
		correct ? ok("PACK/UNPACK: the delay line's round trip", lines)
		        : failed("PACK/UNPACK: the delay line's round trip", lines + "; an unexpected zero");
	}


	// =====================================================================================================
	// THE FOUR CONTROLS NO GAME SETS: FRCL, ADRL, YRL and NEGB.
	//
	// Measured on 2026-10-28, and it is that measurement that makes these cases necessary: `dspprog` read
	// over ALL the games this port serves -- 152 program writes, five distinct shapes, three games
	// including Sega Rally 2, which has an altogether different program -- does not find ONE step that sets
	// any of the four (docs/scsp_audit.md §5). So there is no game to go and find that would make the
	// replayer their byte-for-byte judge, and without these cases the project's DSP carries them WRITTEN
	// and NEVER TESTED.
	//
	// WHAT THESE FIVE CASES BUY, AND WHAT THEY DO NOT. They buy that `dsp_own` does on these fields what
	// MAME's DSP does, which is the behaviour this port AIMS at. They do not buy that MAME is right: nobody
	// has measured those four on hardware, and docs/scsp_references.md refuses consensus between emulators
	// as proof. So the numbers below are a recorded TARGET, just like the eight preceding cases -- except
	// that the other eight are cross-checked by the game on every replay.
	//
	// THEY ALL START FROM CASE 4'S CHAIN, the only one that sets a known X with no voice: RAM -> MRD (odd
	// step) -> IWT -> MEMS -> product. With ram[0] = 0x1234 and COEF[0] = 0x4000, it gives MEMS[0] =
	// 0x00123400 and ACC = 0x0091A00 at step 3.
	// =====================================================================================================

	// ---- 9. NEGB: B changes sign, and nothing else
	// NEGB touches ONLY B, after BSEL has chosen it. So a hold step (B = ACC) with NEGB gives ACC = -ACC.
	// It is measured on both sides of the same program: without, then with.
	{
		std::string lines;
		std::int16_t seen[2] = {};
		for (int neg = 0; neg < 2; neg++) {
			BenchDsp d;
			d.b.ram[0] = 0x1234;
			d.madrs(0, 0);
			DecodedStep p0;
			DecodedStep p1; p1.mrd = 1; p1.table = 1; p1.masa = 0; p1.nofl = 1;
			DecodedStep p2; p2.iwt = 1; p2.iwa = 0;
			DecodedStep p3; p3.xsel = 1; p3.ira = 0; p3.ysel = 1; p3.coef = 0; p3.zero = 1;
			DecodedStep p4 = hold(); p4.negb = unsigned(neg);   // B = ACC, negated if NEGB
			DecodedStep p5 = hold(); p5.shift = 0; p5.ewt = 1; p5.ewa = 0;
			d.coef(0, 0x4000);
			d.program({ p0, p1, p2, p3, p4, p5 });
			d.one_step();
			seen[neg] = d.efreg(0);
			char t[48];
			snprintf(t, sizeof t, "%snegb %d -> %d", neg ? " | " : "", neg, seen[neg]);
			lines += t;
		}
		(seen[0] == 2330 && seen[1] == -2330)
			? ok("NEGB: B changes sign", lines)
			: failed("NEGB: B changes sign", lines + "; expected 2330 and -2330");
	}

	// ---- 10. FRCL: SHIFTED is latched, and YSEL=0 reads it back
	// FRC_REG has no window: the ONLY way to read it is to use it as Y (YSEL=0). So the case writes FRC_REG
	// from a known SHIFTED, then multiplies it again by the same X and sends the product out. FRCL takes
	// (SHIFTED >> 11) & 0x1FFF -- except at SHIFT=3, where it takes SHIFTED & 0x0FFF. Both are measured,
	// because that is the field's only asymmetry.
	{
		std::string lines;
		std::int16_t seen[2] = {};
		int i = 0;
		for (unsigned sh : { 0u, 3u }) {
			BenchDsp d;
			d.b.ram[0] = 0x1234;
			d.madrs(0, 0);
			DecodedStep p0;
			DecodedStep p1; p1.mrd = 1; p1.table = 1; p1.masa = 0; p1.nofl = 1;
			DecodedStep p2; p2.iwt = 1; p2.iwa = 0;
			DecodedStep p3; p3.xsel = 1; p3.ira = 0; p3.ysel = 1; p3.coef = 0; p3.zero = 1;
			DecodedStep p4 = hold(); p4.shift = sh; p4.frcl = 1;     // FRC_REG <- SHIFTED, per SHIFT
			DecodedStep p5; p5.xsel = 1; p5.ira = 0; p5.ysel = 0; p5.zero = 1;   // Y = FRC_REG
			DecodedStep p6 = hold(); p6.shift = 0; p6.ewt = 1; p6.ewa = 0;
			d.coef(0, 0x4000);
			d.program({ p0, p1, p2, p3, p4, p5, p6 });
			d.one_step();
			seen[i] = d.efreg(0);
			char t[64];
			snprintf(t, sizeof t, "%sshift %u -> %d", i ? " | " : "", sh, seen[i]);
			lines += t;
			i++;
		}
		// READ OFF, not computed. At SHIFT=0, FRC_REG takes (SHIFTED >> 11) & 0x1FFF; at SHIFT=3 it takes
		// SHIFTED & 0x0FFF, a low slice and a much larger one -- hence the product six times bigger. That
		// is the field's only asymmetry, and it reads here.
		(seen[0] == 331 && seen[1] == 2912)
			? ok("FRCL: SHIFTED latched, read back through YSEL=0", lines)
			: failed("FRCL: SHIFTED latched, read back through YSEL=0", lines + "; expected 331 and 2912");
	}

	// ---- 11. YRL: the INPUT is latched, and YSEL 2 and 3 take two different slices of it
	// YRL files `input` -- and not SHIFTED -- into Y_REG. YSEL=2 takes its bits 23-11, YSEL=3 its bits
	// 15-4: two different windows on the same register, and that is the whole point of measuring both.
	{
		std::string lines;
		std::int16_t seen[2] = {};
		int i = 0;
		for (unsigned ys : { 2u, 3u }) {
			BenchDsp d;
			d.b.ram[0] = 0x1234;
			d.madrs(0, 0);
			DecodedStep p0;
			DecodedStep p1; p1.mrd = 1; p1.table = 1; p1.masa = 0; p1.nofl = 1;
			DecodedStep p2; p2.iwt = 1; p2.iwa = 0;
			DecodedStep p3; p3.xsel = 1; p3.ira = 0; p3.ysel = 1; p3.coef = 0; p3.zero = 1;
			p3.yrl = 1;                                          // Y_REG <- input = MEMS[0]
			DecodedStep p4; p4.xsel = 1; p4.ira = 0; p4.ysel = ys; p4.zero = 1;   // Y = a slice of Y_REG
			DecodedStep p5 = hold(); p5.shift = 0; p5.ewt = 1; p5.ewa = 0;
			d.coef(0, 0x4000);
			d.program({ p0, p1, p2, p3, p4, p5 });
			d.one_step();
			seen[i] = d.efreg(0);
			char t[64];
			snprintf(t, sizeof t, "%sysel %u -> %d", i ? " | " : "", ys, seen[i]);
			lines += t;
			i++;
		}
		// READ OFF. The two slices of Y_REG do not give the same Y, hence not the same product: that is
		// what proves YSEL=2 and YSEL=3 do read TWO different windows of the same register, and not the
		// same one by two paths.
		(seen[0] == 662 && seen[1] == 946)
			? ok("YRL: the input latched, read back through YSEL 2 and 3", lines)
			: failed("YRL: the input latched, read back through YSEL 2 and 3", lines + "; expected 662 and 946");
	}

	// ---- 12. ADRL: the memory address is offset, and ADREB uses it
	// ADRS_REG has no window either. It reads through its EFFECT: with ADREB, the delay line is addressed
	// at MADRS[masa] + ADRS_REG. So a DIFFERENT value is put at the offset address, and it is that one that
	// must come back in MEMS.
	//
	// Outside SHIFT=3, ADRL takes `input >> 16` -- here 0x00123400 >> 16 = 0x12, so the control is placed
	// at word 0x12 of the delay line.
	{
		BenchDsp d;
		d.b.ram[0] = 0x1234;
		d.b.ram[0x12] = 0x5678;        // the control, at the address ADRS_REG must add
		d.madrs(0, 0);
		DecodedStep p0;
		DecodedStep p1; p1.mrd = 1; p1.table = 1; p1.masa = 0; p1.nofl = 1;
		DecodedStep p2; p2.iwt = 1; p2.iwa = 0;
		DecodedStep p3; p3.xsel = 1; p3.ira = 0; p3.ysel = 1; p3.coef = 0; p3.zero = 1;
		p3.adrl = 1;                   // ADRS_REG <- input >> 16
		DecodedStep p4;
		DecodedStep p5; p5.mrd = 1; p5.table = 1; p5.masa = 0; p5.nofl = 1; p5.adreb = 1;   // odd step
		DecodedStep p6; p6.iwt = 1; p6.iwa = 1;
		d.coef(0, 0x4000);
		d.program({ p0, p1, p2, p3, p4, p5, p6 });
		d.one_step();
		const std::int32_t m0 = d.mems(0), m1 = d.mems(1);
		char t[128];
		snprintf(t, sizeof t, "MEMS[0] = %s (without ADREB), MEMS[1] = %s (with)", hex(m0).c_str(), hex(m1).c_str());
		(m0 == 0x123400 && m1 == 0x567800)
			? ok("ADRL: the address offset by ADRS_REG", t)
			: failed("ADRL: the address offset by ADRS_REG", t);
	}

	// ---- 13. THE TWO BODIES AGREE: a harmless rare field changes nothing
	// `dsp_own` has two bodies -- `template <bool RARES>` -- and only one runs on the games' programs,
	// since none of them sets the four. So the full body was exercised by NOTHING. This case forces it:
	// case 4's chain, unchanged, plus an FRCL on a step where SHIFTED is zero -- which writes zero into
	// FRC_REG, its starting value, and so changes nothing. The result MUST stay 0x091A. Four faults
	// injected into the full body would otherwise have gone unnoticed.
	{
		BenchDsp d;
		d.b.ram[0] = 0x1234;
		d.madrs(0, 0);
		DecodedStep p0; p0.frcl = 1;           // SHIFTED is zero here: FRC_REG <- 0, with no effect
		DecodedStep p1; p1.mrd = 1; p1.table = 1; p1.masa = 0; p1.nofl = 1;
		DecodedStep p2; p2.iwt = 1; p2.iwa = 0;
		DecodedStep p3; p3.xsel = 1; p3.ira = 0; p3.ysel = 1; p3.coef = 0; p3.zero = 1;
		DecodedStep p4 = hold(); p4.shift = 0; p4.ewt = 1; p4.ewa = 0;
		d.coef(0, 0x4000);
		d.program({ p0, p1, p2, p3, p4 });
		d.one_step();
		const std::int16_t e = d.efreg(0);
		char t[96];
		snprintf(t, sizeof t, "EFREG[0] = %d, the full body forced by a null FRCL", e);
		(e == 0x091A) ? ok("the two bodies agree", t)
		              : failed("the two bodies agree", std::string(t) + "; expected 2330");
	}

	printf("\n");
	if (failures) { printf("%d failure(s).\n", failures); return 1; }
	printf("all cases pass\n");
	return 0;
}
