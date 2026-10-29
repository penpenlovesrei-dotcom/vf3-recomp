// test_scsp_core: drives MAME's SCSP through the NEW interface, and only through it.
//
// Step 3 of scsp/docs/scsp_migration_plan.md asks for "an adapter that presents the old core through the
// new contract. It is the move that decides whether the contract holds". This is how it is decided: the
// test touches nothing but scsp::core and scsp::sink. If something the board needs cannot be said through
// those, this file cannot be written -- and that, not a number, is what it proves.
//
// AND IT WRITES ITS REGISTERS THROUGH THE CONTRACT'S OWN FIELD TABLE (src/sound/scsp_contract.h), not
// through hand-written shifts. So a wrong field position in the contract makes the sound stop coming out
// here, rather than waiting to be found in a game. The two files hold each other up.
//
// No ROM, no window, no framework: a RAM buffer, a waveform written into it by hand, one voice keyed on.
#include "scsp_contract.h"
#include "../../src/sound/scsp_mame.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {

int failures = 0;

void ok(const char *what, const std::string &detail)
{
	printf("  ok    %-36s %s\n", what, detail.c_str());
}

void fail(const char *what, const std::string &detail)
{
	printf("  FAIL  %-36s %s\n", what, detail.c_str());
	++failures;
}

// The fields are set by the contract itself (scsp_contract::put), and not by a local copy.
// THE LOCAL COPY EXISTED, and that is why it does not any more: the contract mixes two conventions --
// most fields are a value to be shifted, but the single bits are written IN PLACE, a mask with no shift
// (KYONEX, KYONB, PCM8B, EGHOLD, LPSLNK, LFORE, STWINH, SDIR). This test's first version shifted blindly,
// KYONB came out as zero, the voice stayed silent and the message accused the contract when the fault was
// here. One rule, in the file that holds the table: scsp_contract.h section 8.
void put(std::uint16_t word[16], const char *name, unsigned value)
{
	if (!scsp_contract::put(word, name, value)) {
		fprintf(stderr, "test_scsp_core: the contract has no field named %s\n", name);
		std::exit(2);
	}
}

// What the board plugs underneath, remembering what it was told.
struct Watcher final : scsp::sink {
	int last_level = -1;
	int irqs = 0;
	std::uint64_t midi_bytes = 0;
	struct Arm { int which; std::uint32_t samples, period; };
	std::vector<Arm> timers;

	void irq(int level, bool on) override { last_level = on ? level : -1; ++irqs; }
	void midi_out(std::uint8_t) override { ++midi_bytes; }
	void timer(int which, std::uint32_t samples, std::uint32_t period) override
	{
		timers.push_back({ which, samples, period });
	}
};

constexpr std::uint32_t RAM_BYTES = 0x100000;   // 1 MB, like one chip on the board

} // namespace

int main()
{
	printf("test_scsp_core: MAME's core driven ONLY through scsp::core\n\n");

	std::vector<std::uint16_t> ram(RAM_BYTES / 2, 0);
	// A single-cycle wave at 0x2000, like the bank the driver loads there (docs/fv2-sound-program.md
	// §4 septies): 128 words, full scale.
	constexpr std::uint32_t WAVE = 0x2000;
	// A SQUARE and not a sine, and that is deliberate: a sine starts at zero, so a slot that read only
	// its first sample would render silence without anything being broken -- a false negative that would
	// have the contract accused. A full-scale square has no zero sample at all.
	for (int k = 0; k < 128; k++)
		ram[(WAVE / 2) + k] = std::uint16_t(k < 64 ? 20000 : -20000);

	Watcher w;
	scsp_mame the_core(45158400 / 2, ram.data(), RAM_BYTES, w);
	the_core.reset();
	// The common register 0x400, which the driver writes at start-up (0x300: MEM4MB and DAC18B) and whose
	// MVOL comes out of reset as zero. Without it nothing guarantees the chip is in a state to play, and a
	// test that does not set it measures something other than what it believes.
	the_core.write_reg(0x400 / 2, 0x0300 | 0x000F, 0xFFFF);

	// ---- 1. time is PUSHED, and it advances
	{
		std::vector<std::int32_t> out(2 * 64, 0x7FFFFFFF);
		the_core.run(64, out.data());
		const bool n = the_core.samples_made() == 64;
		const bool filled = out[0] != 0x7FFFFFFF && out[127] != 0x7FFFFFFF;
		(n && filled) ? ok("run(64) pushes 64 pairs",
		                   "samples_made() = " + std::to_string(the_core.samples_made()) + ", the buffer is written")
		              : fail("run(64) pushes 64 pairs",
		                     "samples_made() = " + std::to_string(the_core.samples_made())
		                     + (filled ? "" : ", and the buffer was not written in full"));
	}

	// ---- 2. silence is silence, and not "what was lying about in the buffer"
	{
		std::vector<std::int32_t> out(2 * 64, 1);
		the_core.run(64, out.data());
		bool silent = true;
		for (std::int32_t v : out) if (v != 0) { silent = false; break; }
		silent ? ok("no voice: exact silence", "all 128 values are zero")
		       : fail("no voice: exact silence", "the buffer is not zero although no voice is playing");
	}

	// ---- 3. an armed timer goes through the sink, IN SAMPLES
	{
		w.timers.clear();
		the_core.write_reg(scsp_contract::TIMA / 2, scsp_contract::TIMER_A_COUNT, 0xFFFF);
		const std::uint32_t expected = scsp_device::timer_samples(scsp_contract::TIMER_A_COUNT);
		bool found = false;
		std::string detail;
		for (const Watcher::Arm &a : w.timers)
			if (a.which == scsp_device::TIMER_A) {
				found = a.samples == expected;
				detail = "which = A, samples = " + std::to_string(a.samples)
				       + " (expected " + std::to_string(expected) + ")";
			}
		found ? ok("TIMA arms timer A in samples", detail)
		      : fail("TIMA arms timer A in samples",
		             detail.empty() ? "the sink received nothing for timer A" : detail);
	}

	// ---- 4. ONE VOICE, every register of which is set by the contract
	{
		std::uint16_t word[16] = {};
		put(word, "SA_HI", WAVE >> 16);
		put(word, "SA_LO", WAVE & 0xFFFF);
		put(word, "LSA",   0);
		put(word, "LEA",   127);
		put(word, "LPCTL", 1);          // loop
		put(word, "AR",    0x1F);       // an immediate attack
		put(word, "D1R",   0);
		put(word, "D2R",   0);
		put(word, "RR",    0);
		put(word, "DL",    0);
		put(word, "TL",    0);          // no attenuation
		put(word, "DISDL", 7);          // full level on the direct output
		put(word, "DIPAN", 0);          // centred
		put(word, "OCT",   0);
		put(word, "FNS",   0);
		put(word, "KYONB", 1);
		for (unsigned k = 0; k < 16; k++)
			if (word[k]) the_core.write_reg(k, word[k], 0xFFFF);
		// KYONEX, the trigger, last and on its own
		std::uint16_t trig[16] = {};
		put(trig, "KYONEX", 1);
		put(trig, "KYONB", 1);
		the_core.write_reg(0, std::uint16_t(word[0] | trig[0]), 0xFFFF);

		std::vector<std::int32_t> out(2 * 512, 0);
		the_core.device().m_probe = true;   // (diagnostic: the energy the slot actually sent)
		the_core.run(512, out.data());
		printf("        [diag] slot 0's direct energy = %g; by EG state of group 0 = %g %g %g %g\n",
		       the_core.device().m_probe_direct[0],
		       the_core.device().m_probe_state[0][0], the_core.device().m_probe_state[0][1],
		       the_core.device().m_probe_state[0][2], the_core.device().m_probe_state[0][3]);
		printf("        [diag] slot 0's group = %d\n", the_core.device().slot_group(0));
		std::int32_t peak = 0;
		for (std::int32_t v : out) peak = std::max(peak, std::abs(v));
		(peak > 1000)
			? ok("a keyed voice SOUNDS, through the contract alone",
			     "peak " + std::to_string(peak) + " out of " + std::to_string(scsp_mame::FULL_SCALE))
			: fail("a keyed voice SOUNDS, through the contract alone",
			       "peak " + std::to_string(peak) + ": nothing came out. A field in the contract is wrong,"
			       " or the adapter is not passing it on");
	}

	// ---- 5. what the adapter cannot do, said rather than kept quiet
	{
		const std::uint64_t lost = the_core.midi_out_bits_dropped();
		ok("the unassembled MIDI bits are counted",
		   std::to_string(lost) + " bits dropped (the old core emits bits, the contract asks for"
		   " bytes -- scsp_mame.h note 3)");
	}

	printf("\n");
	if (failures) {
		printf("%d failure(s).\n", failures);
		return 1;
	}
	printf("all cases pass\n");
	return 0;
}
