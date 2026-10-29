// measure-core: what an SCSP core costs, item by item, with nothing else inside the measurement.
//
// WHY NOT THE REPLAYER. tools/sound/replay compares two cores byte for byte, and that is its job -- but
// its time is dominated by parsing 81 MB of text, identical in both and counted twice. So it gives a right
// GAP and a wrong share: one reads "5% more" there without knowing of what.
//
// This bench times `run()` and nothing else, and it SEPARATES the items, because a global figure does not
// say where to go:
//
//     idle           no voice, no DSP program -- the loop's floor
//     slots          32 voices playing, no DSP program
//     DSP            no voice, a program of 112 steps
//     both           what the game actually does
//
// The synthetic DSP program's shape follows the one the `dspprog` probe read off the game: 112 steps, and
// within them about 87 COEF reads, 108 inputs from MEMS, 39 TEMP writes, 23 memory reads, 10 writes, 5
// effects outputs (docs/fv2-sound-program.md §4). It is not the game's program -- it is its shape, which
// is enough to locate a cost.
//
// AND ONE NOTCH FINER, `--dsp`: the DSP by CHARACTERISTIC, both cores side by side. The "DSP" item above is
// a single number for a whole instruction set, and it says the project's core is +20% there without saying
// of what. Programs of 112 steps where every step does only one thing answer that. Its header is further
// down, just before the table.
//
//   tools/sound/measure-core [core] [samples]          the four items
//   tools/sound/measure-core --dsp [samples]           the DSP by characteristic, both cores
//   tools/sound/measure-core --baseline [samples]      the baseline alone, seven passes: enough to
//                                                      interleave a reference and a variant of one file
//   tools/sound/measure-core --baseline <core> [n]     ONE core only, and the process does nothing else:
//                                                      enough for a hardware counter to attribute it
//   tools/sound/measure-core --game <core> [n]         the same, on the program of the GAME'S SHAPE --
//                                                      the baseline is artificial, this is what counts
#include "../../src/sound/scsp_contract.h"
#include "../../src/sound/scsp_select.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace {

constexpr std::uint32_t RAM_BYTES = 0x100000;

struct Silent final : scsp::sink {
	void irq(int, bool) override {}
	void midi_out(std::uint8_t) override {}
	void timer(int, std::uint32_t, std::uint32_t) override {}
};

// One DSP program step, by its fields (the same encoding as tools/sound/test_scsp_dsp.cpp).
struct DecodedStep {
	unsigned tra = 0, twt = 0, twa = 0, xsel = 0, ysel = 0, ira = 0, iwt = 0, iwa = 0;
	unsigned table = 0, mwt = 0, mrd = 0, ewt = 0, ewa = 0, shift = 0, zero = 0, bsel = 0;
	unsigned nofl = 0, coef = 0, masa = 0;

	// The six fields the game's program never sets, and that nevertheless have to be settable in order to
	// MEASURE them: the four rare ones (ADRL, FRCL, YRL, NEGB -- docs/scsp_audit.md §5) and the two address
	// modifiers. They are zero everywhere else, so adding them changes no existing case.
	unsigned adrl = 0, frcl = 0, yrl = 0, negb = 0, adreb = 0, nxadr = 0;

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

struct Bench {
	std::vector<std::uint16_t> ram = std::vector<std::uint16_t>(RAM_BYTES / 2, 0);
	Silent silent;
	std::unique_ptr<scsp::core> c;

	explicit Bench(const char *name)
	{
		c = scsp::make(name, 45158400 / 2, ram.data(), RAM_BYTES, silent);
		if (!c) { fprintf(stderr, "unknown core: %s\n", name); std::exit(2); }
		c->reset();
		c->write_reg(0x400 / 2, 0x0300 | 0x000F, 0xFFFF);   // MEM4MB, DAC18B, MVOL at full
		// A wave at 0x1000: a square, which has no zero sample.
		for (int k = 0; k < 256; k++) ram[0x1000 / 2 + k] = std::uint16_t(k < 128 ? 12000 : -12000);
	}

	void voices(unsigned n)
	{
		for (unsigned f = 0; f < n; f++) {
			const std::uint32_t base = f * 0x10;
			std::uint16_t word[16] = {};
			auto put = [&](const char *name, unsigned v) { scsp_contract::put(word, name, v); };
			put("SA_LO", 0x1000);
			put("LSA", 0); put("LEA", 256); put("LPCTL", 1);
			put("AR", 0x1C); put("D1R", 2); put("D2R", 1); put("RR", 8); put("DL", 12);
			put("TL", 0x20); put("DISDL", 7); put("DIPAN", f & 0x1F);
			put("EFSDL", f < 16 ? 4 : 0); put("EFPAN", 0);
			put("ISEL", f & 0xF); put("IMXL", 4);
			put("OCT", f & 7); put("FNS", (f * 37) & 0x3FF);
			put("LFOF", 20); put("ALFOS", 3); put("ALFOWS", f & 3);
			put("KYONB", 1);
			for (unsigned w = 1; w < 16; w++) c->write_reg(base + w, word[w], 0xFFFF);
			c->write_reg(base + 0, word[0], 0xFFFF);
		}
		// KYONEX last: it sweeps all thirty-two slots in one go.
		std::uint16_t dec[16] = {};
		scsp_contract::put(dec, "KYONEX", 1);
		std::uint16_t word0 = 0;
		(void) word0;
		c->write_reg(0, std::uint16_t(c->read_reg(0) | dec[0]), 0xFFFF);
	}

	// Lays down any program: MPRO, the 64 COEF, the 32 MADRS, then the word that triggers Start. Factored
	// out of dsp_program() so that the per-characteristic bench uses it too.
	void program(const std::vector<DecodedStep> &step)
	{
		for (unsigned i = 0; i < 128; i++) {
			std::uint16_t m[4] = { 0, 0, 0, 0 };
			if (i < step.size()) step[i].words(m);
			for (unsigned k = 0; k < 4; k++) c->write_reg((0x800 + i * 8 + k * 2) / 2, m[k], 0xFFFF);
		}
		for (unsigned i = 0; i < 64; i++) c->write_reg((0x700 + i * 2) / 2, std::uint16_t(0x2000 + i * 13), 0xFFFF);
		for (unsigned i = 0; i < 32; i++) c->write_reg((0x780 + i * 2) / 2, std::uint16_t(i * 64), 0xFFFF);
		c->write_reg(0xBF0 / 2, 0, 0xFFFF);   // the word that triggers Start
	}

	void dsp_program()
	{
		// 112 steps, of the shape the probe read off the game.
		std::vector<DecodedStep> step(112);
		for (unsigned i = 0; i < 112; i++) {
			DecodedStep &p = step[i];
			p.ira = i % 32;                 // the input comes from MEMS: 108 steps out of 112
			p.xsel = 1;
			p.ysel = 1;                     // COEF: 87 steps out of 112
			p.coef = i % 64;
			p.tra = i % 128;
			if (i % 3)       p.twt = 1, p.twa = (i * 7) % 128;   // ~39 TEMP writes
			if (i % 5 == 0)  p.mrd = 1, p.masa = i % 32;         // ~23 memory reads
			if (i % 11 == 0) p.mwt = 1, p.masa = i % 32;         // ~10 writes
			if (i % 22 == 0) p.ewt = 1, p.ewa = i % 16;          // ~5 effects outputs
			if (i % 2)       p.iwt = 1, p.iwa = i % 32;
			if (i % 3 == 0)  p.zero = 1;
			if (i % 3 == 1)  p.bsel = 1;
			p.shift = i % 4;
		}
		program(step);
	}
};

// run()'s time, and nothing else. The volatile sink stops the compiler throwing it all away.
double stopwatch(scsp::core &c, unsigned samples, volatile std::int64_t &snk)
{
	constexpr unsigned CHUNK = 256;
	std::vector<std::int32_t> buffer(CHUNK * 2);
	const auto t0 = std::chrono::steady_clock::now();
	for (unsigned done = 0; done < samples; done += CHUNK) {
		const unsigned n = std::min(CHUNK, samples - done);
		c.run(n, buffer.data());
		std::int64_t s = 0;
		for (unsigned i = 0; i < n * 2; i++) s += buffer[i];
		snk += s;
	}
	return std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
}

// -----------------------------------------------------------------------------------------------------
// THE PER-CHARACTERISTIC BENCH (`--dsp`), AND WHY IT EXISTS.
//
// Measured on 2026-10-28: the project's DSP costs +19.8% per step against MAME's -- 11.50 against 9.60
// cycles, in the game (docs/scsp_audit.md §4 sexies), and +20% on the four-item bench above. The two
// instruments agree on the GAP and neither says WHAT it is made of: the "DSP" item is still a single number
// for a whole instruction set.
//
// And that is exactly the mistake docs/scsp_audit.md §4 quinquies recounts -- having specialised the DSP
// aiming at the wrong item, for want of an instrument that separates. So one notch finer, and in the same
// way: programs of 112 steps where EVERY STEP DOES ONLY ONE THING, timed on both cores. The difference
// between the baseline and the baseline plus one characteristic IS that characteristic's cost, per step.
//
// WHAT THIS BENCH CANNOT SAY, and it is better written than discovered: a program whose 112 steps all do
// the same thing does not have the game's instruction mix, hence neither its cache misses nor its
// dependencies between steps. It LOCATES a cost, it does not predict it. The control is the last line: the
// program of the game's shape, measured by the same stopwatch -- if the sum of the characteristics were far
// from it, it is the bench that should be believed least.
//
// THE FOUR RARE ONES ARE IN THE TABLE, and there they answer another question: the game never sets them
// (§5), so this is the only place in the repository where their cost reads. On the project's core they also
// switch dsp_own over to its second body (`template <bool RARES>`), and so the line measures that too.
// -----------------------------------------------------------------------------------------------------

constexpr unsigned STEPS_PER_PROGRAM = 112;   // the length read off the game (docs/scsp_audit.md §5)

struct Characteristic {
	const char *name;
	const char *what;
	void (*put)(DecodedStep &p, unsigned i);
};

// The baseline: 112 steps that do as little as possible. ZERO sets B to zero, nothing comes in, nothing
// goes out.
void baseline(DecodedStep &p, unsigned) { p.zero = 1; }

const Characteristic CHARACTERISTICS[] = {
	{ "baseline", "112 steps that do nothing: the step loop's floor",
	  [](DecodedStep &, unsigned) {} },
	{ "IRA -> MEMS", "the input from the memory window, 108 steps out of 112 in the game",
	  [](DecodedStep &p, unsigned i) { p.ira = i % 32; p.xsel = 1; } },
	{ "COEF", "the coefficient read, 87 steps out of 112 in the game",
	  [](DecodedStep &p, unsigned i) { p.ysel = 1; p.coef = i % 64; } },
	{ "the product", "B + X * Y: the input, the coefficient and the accumulation together",
	  [](DecodedStep &p, unsigned i) { p.ira = i % 32; p.xsel = 1; p.ysel = 1; p.coef = i % 64;
	                           p.tra = i % 128; p.bsel = 1; } },
	{ "TWT", "the write into TEMP, 39 steps out of 112 in the game",
	  [](DecodedStep &p, unsigned i) { p.twt = 1; p.twa = (i * 7) % 128; } },
	{ "IWT", "the write into MEMS",
	  [](DecodedStep &p, unsigned i) { p.iwt = 1; p.iwa = i % 32; } },
	{ "MRD (NOFL)", "the memory read WITHOUT packing, 23 steps out of 112 in the game",
	  [](DecodedStep &p, unsigned i) { p.mrd = 1; p.nofl = 1; p.masa = i % 32; } },
	{ "MRD + UNPACK", "the same, PACKED: the difference is UNPACK's cost",
	  [](DecodedStep &p, unsigned i) { p.mrd = 1; p.nofl = 0; p.masa = i % 32; } },
	{ "MWT (NOFL)", "the memory write without packing, 10 steps out of 112 in the game",
	  [](DecodedStep &p, unsigned i) { p.mwt = 1; p.nofl = 1; p.masa = i % 32; } },
	{ "MWT + PACK", "the same, PACKED: the difference is PACK's cost",
	  [](DecodedStep &p, unsigned i) { p.mwt = 1; p.nofl = 0; p.masa = i % 32; } },
	{ "ADREB + NXADR", "the two address modifiers, on top of a read",
	  [](DecodedStep &p, unsigned i) { p.mrd = 1; p.nofl = 1; p.masa = i % 32; p.adreb = 1; p.nxadr = 1; } },
	{ "EWT", "the output to EFREG, 5 steps out of 112 in the game",
	  [](DecodedStep &p, unsigned i) { p.ewt = 1; p.ewa = i % 16; } },
	{ "SHIFT 1 (clamps)", "the shift that saturates",
	  [](DecodedStep &p, unsigned i) { p.ira = i % 32; p.xsel = 1; p.shift = 1; } },
	{ "SHIFT 3 (wraps)", "the shift that wraps",
	  [](DecodedStep &p, unsigned i) { p.ira = i % 32; p.xsel = 1; p.shift = 3; } },
	{ "FRCL", "one of the four the game NEVER sets (§5)",
	  [](DecodedStep &p, unsigned) { p.frcl = 1; } },
	{ "ADRL", "likewise",
	  [](DecodedStep &p, unsigned) { p.adrl = 1; } },
	{ "YRL", "likewise",
	  [](DecodedStep &p, unsigned) { p.yrl = 1; } },
	{ "NEGB", "likewise",
	  [](DecodedStep &p, unsigned i) { p.tra = i % 128; p.bsel = 1; p.negb = 1; } },
	{ "the four rare ones", "all four together: on the project's core, that is the second body",
	  [](DecodedStep &p, unsigned i) { p.tra = i % 128; p.bsel = 1;
	                           p.frcl = 1; p.adrl = 1; p.yrl = 1; p.negb = 1; } },
};

// The time per DSP step, median of three passes: a single measurement is carried along by its neighbours.
double ns_per_step(const char *the_core, const Characteristic &k, unsigned n, volatile std::int64_t &snk)
{
	double v[3];
	for (int trial = 0; trial < 3; trial++) {
		Bench b(the_core);
		std::vector<DecodedStep> step(STEPS_PER_PROGRAM);
		for (unsigned i = 0; i < STEPS_PER_PROGRAM; i++) { baseline(step[i], i); k.put(step[i], i); }
		b.program(step);
		v[trial] = stopwatch(*b.c, n, snk) * 1e9 / (double(n) * STEPS_PER_PROGRAM);
	}
	std::sort(v, v + 3);
	return v[1];
}

// The control: the program of the game's shape, by the same stopwatch and the same unit.
double ns_per_step_of_game(const char *the_core, unsigned n, volatile std::int64_t &snk)
{
	double v[3];
	for (int trial = 0; trial < 3; trial++) {
		Bench b(the_core);
		b.dsp_program();
		v[trial] = stopwatch(*b.c, n, snk) * 1e9 / (double(n) * STEPS_PER_PROGRAM);
	}
	std::sort(v, v + 3);
	return v[1];
}

// IS THE BASELINE PER STEP OR PER SAMPLE? The question decides where one repairs, and a single program
// length cannot answer it: a per-sample cost (clearing EFREG, the branch to one of the two bodies, what
// Start leaves behind it) divides by the number of steps and disguises itself as a per-step cost. Three
// lengths separate them -- the SLOPE is a step's cost, the INTERCEPT is what the sample costs before its
// first step.
void baseline_by_length(unsigned n, volatile std::int64_t &snk)
{
	const unsigned lengths[3] = { 16, 48, 112 };
	printf("\nTHE BASELINE, BY PROGRAM LENGTH: per step or per sample?\n\n");
	printf("  %-8s %12s %12s %12s\n", "core", "16 steps", "48 steps", "112 steps");
	printf("  %-8s %12s %12s %12s\n", "", "ns/smp", "ns/smp", "ns/smp");
	double ns[2][3] = {};
	const char *cores[2] = { "mame", "own" };
	for (int ic = 0; ic < 2; ic++) {
		for (int il = 0; il < 3; il++) {
			double v[3];
			for (int trial = 0; trial < 3; trial++) {
				Bench b(cores[ic]);
				std::vector<DecodedStep> step(lengths[il]);
				for (unsigned i = 0; i < lengths[il]; i++) baseline(step[i], i);
				b.program(step);
				v[trial] = stopwatch(*b.c, n, snk) * 1e9 / double(n);
			}
			std::sort(v, v + 3);
			ns[ic][il] = v[1];
		}
		printf("  %-8s %12.1f %12.1f %12.1f\n", cores[ic], ns[ic][0], ns[ic][1], ns[ic][2]);
	}
	// The slope between 16 and 112 steps, and what is left when it is taken away: two points are enough,
	// the third is there to say whether it really is a straight line.
	printf("\n  %-8s %14s %18s %14s\n", "core", "ns per step", "ns per sample", "(control)");
	for (int ic = 0; ic < 2; ic++) {
		const double slope = (ns[ic][2] - ns[ic][0]) / double(lengths[2] - lengths[0]);
		const double intercept = ns[ic][0] - slope * double(lengths[0]);
		const double expected48 = intercept + slope * double(lengths[1]);
		printf("  %-8s %14.3f %18.1f %14s\n", cores[ic], slope, intercept,
		       (std::abs(expected48 - ns[ic][1]) < 0.06 * ns[ic][1]) ? "straight" : "NOT STRAIGHT");
	}
	const double slope_a = (ns[0][2] - ns[0][0]) / 96.0, slope_b = (ns[1][2] - ns[1][0]) / 96.0;
	const double int_a = ns[0][0] - slope_a * 16.0, int_b = ns[1][0] - slope_b * 16.0;
	printf("\n  gap per step         %+.3f ns  (%.0f%%)\n", slope_b - slope_a,
	       slope_a > 0 ? 100.0 * (slope_b / slope_a - 1.0) : 0.0);
	printf("  gap per sample       %+.1f ns  (%.0f%%)\n", int_b - int_a,
	       int_a > 0 ? 100.0 * (int_b / int_a - 1.0) : 0.0);
	printf("  and over a 112-step program, the per-sample gap weighs %+.3f ns per step,\n"
	       "  that is %.0f%% of the baseline's gap -- the share one would repair OUTSIDE the step loop.\n",
	       (int_b - int_a) / 112.0,
	       (ns[1][2] - ns[0][2]) != 0 ? 100.0 * (int_b - int_a) / (ns[1][2] - ns[0][2]) : 0.0);
	printf("  (control: the third point must say `straight` on both sides, otherwise these two numbers\n"
	       "  separate nothing and more lengths are needed.)\n");
}

// `--baseline`: the baseline alone, and nothing else. It exists for a precise reason -- the baseline
// carries ALL of the DSP's gap (docs/scsp_audit.md §4 septies), and looking for it means comparing
// different BUILDS of the same file, hence interleaving reference and variant. The full table costs too
// much for that; this one takes two seconds, and it makes SEVEN passes instead of three because what it is
// asked to hold is a 20% gap on two nanoseconds.
// ONE CORE ONLY, AND THE PROCESS DOES NOTHING ELSE. That is what a hardware counter wants: `xperf` totals
// its counters PER PROCESS, so two cores in the same binary cannot be separated without symbols -- and our
// binaries are built by MinGW, in DWARF and with no PDB, which `xperf` does not read. Two captures of one
// process each sidestep the problem entirely: the instructions retired, the mispredicted branches and the
// process's cycles ARE that core's baseline's, up to the start-up, which is the same on both sides.
//
//   measure-core --baseline [samples]            both cores, to read the gap straight away
//   measure-core --baseline <core> [samples]     one only, so that an external counter attributes it
// `--game <core> [n]`: THE PROGRAM OF THE GAME'S SHAPE, one core, no voice. It is `--baseline` for the
// program that counts, and it took a measurement for it to exist: on 2026-10-28, the combination of the
// four variants won **29.3%** on the baseline and **nothing measurable** on this one
// (docs/scsp_audit.md §4 nonies). The baseline is a program whose 112 steps all do the same thing; it
// locates a cost, it does not say what a change is worth in the game.
//
// One core per process, for the same reason as `--baseline <core>`: a hardware counter totals per process,
// and our MinGW binaries give it no function name at all.
int bench_game(const char *the_core, unsigned n)
{
	volatile std::int64_t snk = 0;
	double v[7];
	for (int trial = 0; trial < 7; trial++) {
		Bench b(the_core);
		b.dsp_program();
		v[trial] = stopwatch(*b.c, n, snk) * 1e9 / (double(n) * STEPS_PER_PROGRAM);
	}
	std::sort(v, v + 7);
	printf("game %u, core \"%s\" ALONE: %.3f ns/step [%.3f-%.3f]; %llu DSP steps executed  (sink %lld)\n",
	       n, the_core, v[3], v[0], v[6],
	       (unsigned long long)(7ull * n * STEPS_PER_PROGRAM), (long long) snk);
	return 0;
}

int bench_baseline(unsigned n, const char *only)
{
	volatile std::int64_t snk = 0;
	double v[2][7];
	const char *cores[2] = { "mame", "own" };
	const int idx = only ? (strcmp(only, "own") == 0 ? 1 : 0) : 0;
	const int a_ = only ? idx : 0, b_ = only ? idx + 1 : 2;
	for (int ic = a_; ic < b_; ic++)
		for (int trial = 0; trial < 7; trial++) {
			Bench b(cores[ic]);
			std::vector<DecodedStep> step(STEPS_PER_PROGRAM);
			for (unsigned i = 0; i < STEPS_PER_PROGRAM; i++) baseline(step[i], i);
			b.program(step);
			v[ic][trial] = stopwatch(*b.c, n, snk) * 1e9 / (double(n) * STEPS_PER_PROGRAM);
		}
	for (int ic = a_; ic < b_; ic++) std::sort(v[ic], v[ic] + 7);
	if (only) {
		// The number of DSP STEPS this process has just executed: it is the denominator an external
		// counter will divide its totals by. 7 passes, n samples, 112 steps.
		const double ns = v[idx][3];
		printf("baseline %u, core \"%s\" ALONE: %.3f ns/step [%.3f-%.3f]; %llu DSP steps executed"
		       "  (sink %lld)\n",
		       n, cores[idx], ns, v[idx][0], v[idx][6],
		       (unsigned long long)(7ull * n * STEPS_PER_PROGRAM), (long long) snk);
		return 0;
	}
	const double a = v[0][3], b = v[1][3];
	printf("baseline %u: mame %.3f [%.3f-%.3f]  own %.3f [%.3f-%.3f]  gap %+.3f  ratio %.3f"
	       "  (sink %lld)\n",
	       n, a, v[0][0], v[0][6], b, v[1][0], v[1][6], b - a, a > 0 ? b / a : 0.0, (long long) snk);
	return 0;
}

int bench_dsp(unsigned n)
{
	printf("measure-core --dsp: the DSP by characteristic, %u samples per case,\n"
	       "median of three passes, 112 steps per program. Both cores, and the gap.\n\n", n);
	// The last three columns are the NET COST: the value minus the baseline. Those are the ones to read,
	// and not the raw time -- the baseline is in every line, and it already carries a gap.
	printf("  %-20s %9s %9s %9s %9s %9s %9s\n",
	       "characteristic", "mame", "own", "gap", "mame", "own", "gap");
	printf("  %-20s %9s %9s %9s %9s %9s %9s\n",
	       "", "ns/step", "ns/step", "ns/step", "net", "net", "net");

	volatile std::int64_t snk = 0;
	double baseline_a = 0, baseline_b = 0;
	for (const Characteristic &k : CHARACTERISTICS) {
		const double a = ns_per_step("mame", k, n, snk);
		const double b = ns_per_step("own", k, n, snk);
		if (!strcmp(k.name, "baseline")) {
			baseline_a = a; baseline_b = b;
			printf("  %-20s %9.3f %9.3f %+9.3f %9s %9s %9s\n", k.name, a, b, b - a, "--", "--", "--");
			continue;
		}
		printf("  %-20s %9.3f %9.3f %+9.3f %9.3f %9.3f %+9.3f\n", k.name, a, b, b - a,
		       a - baseline_a, b - baseline_b, (b - baseline_b) - (a - baseline_a));
	}
	const double ja = ns_per_step_of_game("mame", n, snk);
	const double jb = ns_per_step_of_game("own", n, snk);
	printf("\n  %-20s %9.3f %9.3f %+9.3f %9.3f %9.3f %+9.3f   <- THE CONTROL\n", "the game's shape", ja, jb,
	       jb - ja, ja - baseline_a, jb - baseline_b, (jb - baseline_b) - (ja - baseline_a));
	printf("\nWHAT TO READ, IN THIS ORDER:\n"
	       "  1. THE BASELINE FIRST: mame %.3f, own %.3f, that is %+.3f ns per step (%.0f%%) over 112 steps\n"
	       "     that do NOTHING. A gap that is already there is not an instruction, it is the step loop\n"
	       "     itself -- and it is not repaired in the same place as an instruction.\n"
	       "  2. then `net gap`, which is the baseline subtracted: what the characteristic costs EXTRA from\n"
	       "     one core to the other. A negative number says the project's core is better at it.\n"
	       "  3. the control last: if its gap is not of the same order as the ones just read, it is this\n"
	       "     bench that should be believed least -- 112 identical steps have neither the cache nor the\n"
	       "     dependencies of the real program (this file's header).\n",
	       baseline_a, baseline_b, baseline_b - baseline_a, 100.0 * (baseline_b / baseline_a - 1.0));
	baseline_by_length(n, snk);
	printf("\n(sink %lld, so that nothing is optimised away)\n", (long long) snk);
	return 0;
}

} // namespace

int main(int argc, char **argv)
{
	if (argc > 1 && !strcmp(argv[1], "--dsp"))
		return bench_dsp(argc > 2 ? unsigned(strtoul(argv[2], nullptr, 10)) : 400000);
	if (argc > 2 && !strcmp(argv[1], "--game"))
		return bench_game(argv[2], argc > 3 ? unsigned(strtoul(argv[3], nullptr, 10)) : 400000);
	if (argc > 1 && !strcmp(argv[1], "--baseline")) {
		// `--baseline [n]` or `--baseline <core> [n]`: an argument that is not a number is a core name.
		const bool named = argc > 2 && !isdigit((unsigned char)argv[2][0]);
		const char *only = named ? argv[2] : nullptr;
		const int in = named ? 3 : 2;
		return bench_baseline(argc > in ? unsigned(strtoul(argv[in], nullptr, 10)) : 600000, only);
	}

	const char *name = argc > 1 ? argv[1] : "mame";
	const unsigned n = argc > 2 ? unsigned(strtoul(argv[2], nullptr, 10)) : 2000000;

	printf("measure-core: core \"%s\", %u samples per case\n\n", name, n);
	// "% of one core" is an ARGUMENT and not part of the format, so it carries one `%` and not two: the
	// French version doubled it and printed `%%` in the heading for as long as it existed.
	printf("  %-14s %10s %12s %14s\n", "case", "seconds", "ns/smp", "% of one core");

	volatile std::int64_t snk = 0;
	struct Case { const char *name; unsigned voices; bool dsp; };
	const Case cases[] = { { "idle", 0, false }, { "slots", 32, false },
	                    { "DSP", 0, true }, { "both", 32, true } };
	for (const Case &k : cases) {
		Bench b(name);
		if (k.dsp) b.dsp_program();
		if (k.voices) b.voices(k.voices);
		const double s = stopwatch(*b.c, n, snk);
		// The share of one core: the time taken to produce n samples, against their real duration.
		const double duration = double(n) / 44100.0;
		printf("  %-14s %10.3f %12.1f %13.2f\n", k.name, s, s * 1e9 / n, 100.0 * s / duration);
	}
	printf("\n(sink %lld, so that nothing is optimised away)\n", (long long) snk);
	return 0;
}
