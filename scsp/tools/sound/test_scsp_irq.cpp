// test_scsp_irq: the three timers, the five interrupt lines, and the DMA.
//
// Block 6 of docs/scsp_architecture.md §3 -- "Timers, interrupts, synchronisation: A/B/C,
// SCIEB/SCIPD/SCILV, MCIEB/MCIPD, the DMA, and the interrupt rate, which Mednafen says is what makes the
// tempo drift by 2%." That last point is what makes the block important: VF3's music tick IS timer A, and
// the driver's entire sequence hangs on its rate.
//
// THE PROBE IS NOT THE AUDIO OUTPUT, for once: it is the sink. scsp::sink carries the three methods
// through which the chip speaks to the board -- irq(level, state), midi_out(byte), timer(which, samples,
// period) -- and a sink that RECORDS everything it receives measures this block directly, without going
// through a single note of music. That is also what makes it exact: we compare integers, not gains.
//
// The rig is in tools/sound/scsp_bench.h, but this file builds its own core: it needs a talkative sink,
// where the bench puts a silent one.
#include "scsp_bench.h"

#include "../../src/sound/scsp_select.h"

#include <cstdio>
#include <string>
#include <vector>

namespace {

int failures = 0;

void ok(const char *what, const std::string &d) { printf("  ok     %-40s %s\n", what, d.c_str()); }
void failed(const char *what, const std::string &d) { printf("  FAILED %-40s %s\n", what, d.c_str()); ++failures; }

// The sink that records everything. It is the measurement.
struct Watcher final : scsp::sink {
	struct Line { int level; bool state; };
	struct Arm { int which; std::uint32_t samples, period; };
	std::vector<Line> lines;
	std::vector<Arm> arms;
	std::vector<std::uint8_t> midi;

	void irq(int level, bool state) override { lines.push_back({ level, state }); }
	void midi_out(std::uint8_t b) override { midi.push_back(b); }
	void timer(int which, std::uint32_t smp, std::uint32_t per) override { arms.push_back({ which, smp, per }); }

	void clear() { lines.clear(); arms.clear(); midi.clear(); }
	// The last level RAISED, or -1 if the last thing seen is a drop.
	int last_raised() const
	{
		for (std::size_t i = lines.size(); i-- > 0; )
			if (lines[i].state) return lines[i].level;
		return -1;
	}
	std::string trace() const
	{
		std::string s;
		for (const Line &l : lines) {
			if (!s.empty()) s += " ";
			s += (l.state ? "+" : "-") + std::to_string(l.level);
		}
		return s.empty() ? "(nothing)" : s;
	}
};

// A core with a talkative sink. The RAM is used by the DMA.
struct Bench6 {
	std::vector<std::uint16_t> ram = std::vector<std::uint16_t>(bench::RAM_BYTES / 2, 0);
	Watcher m;
	std::unique_ptr<scsp::core> held =
		scsp::make(bench::core_name(), 45158400 / 2, ram.data(), bench::RAM_BYTES, m);
	scsp::core &the_core = *held;

	Bench6()
	{
		if (!held) { fprintf(stderr, "test_scsp_irq: unknown core\n"); std::exit(2); }
		the_core.reset();
		the_core.write_reg(0x400 / 2, 0x0300 | 0x000F, 0xFFFF);
	}

	// The interrupt table VF3's driver writes at startup, as the contract carries it: ten words in one go
	// from 0x418 to 0x42A (0x600288 copies them from 0x6002D4). It is laid down whole rather than inventing
	// a configuration, so that what is measured is what the game lives with.
	void driver_table()
	{
		for (unsigned k = 0; k < 10; k++)
			the_core.write_reg((0x418 + 2 * k) / 2, scsp_contract::INIT_418_42A[k], 0xFFFF);
	}
	std::uint16_t read(unsigned byte) { return the_core.read_reg(byte / 2); }
	void write(unsigned byte, std::uint16_t v) { the_core.write_reg(byte / 2, v, 0xFFFF); }
};

} // namespace

int main(int argc, char **argv)
{
	if (argc > 1) bench::core_name() = argv[1];
	printf("test_scsp_irq: timers, interrupts and DMA, through scsp::core\n\n");

	// ---- 1. A TIMER'S DURATION, AND THE "+1" THAT IS A DELIBERATE DIVERGENCE
	// `timer_samples(reg) = ((255 - (reg & 0xff)) << ((reg >> 8) & 7)) + 1`. MAME writes the same thing
	// WITHOUT the +1, and scsp.cpp's comment says what that +1 corrected: with 49 samples instead of 50,
	// VF3's tick (timer A at 0xCE) ran at 900 Hz instead of 882, and the music came out 0.8% fast against
	// Supermodel. It is the port's third deliberate divergence, after PLFOS and MDL.
	//
	// It is measured without playing a note: the sink receives timer(which, samples, period) on every write
	// of the register, and `samples` IS the figure.
	{
		struct Case { std::uint16_t reg; std::uint32_t smp; const char *what; };
		const Case cases[] = {
			{ 0x00CE,   50, "0xCE: VF3's tick, 882 Hz" },
			{ 0x0000,  256, "0x00: the maximum with no prescaler" },
			{ 0x00FE,    2, "0xFE: the minimum that still arms" },
			{ 0x0100,  511, "prescaler 1, count 0" },
			{ 0x07CE, 6273, "prescaler 7, count 0xCE" },
		};
		std::string lines;
		bool correct = true;
		for (const Case &c : cases) {
			Bench6 b;
			b.driver_table();
			b.m.clear();
			b.write(scsp_contract::TIMA, c.reg);
			std::uint32_t seen = 0;
			for (const Watcher::Arm &a : b.m.arms)
				if (a.which == 0) seen = a.samples;
			char t[48];
			snprintf(t, sizeof t, "%s%04x->%u", lines.empty() ? "" : " ", c.reg, seen);
			lines += t;
			if (seen != c.smp) { correct = false; lines += "(exp " + std::to_string(c.smp) + ")"; }
		}
		correct ? ok("a timer's duration, the +1 included", lines)
		        : failed("a timer's duration, the +1 included", lines);
	}

	// ---- 2. A COUNT OF 255 DOES NOT ARM THE TIMER
	// `if ((m_udata.data[0x18/2] & 0xff) != 255)`: writing 0xFF into the low byte leaves the timer silent.
	// This is not a duration of zero, it is an absence of arming -- the difference shows in the sink, which
	// receives nothing at all, and not a timer(A, 1, 0).
	{
		Bench6 b;
		b.driver_table();
		b.m.clear();
		b.write(scsp_contract::TIMA, 0x00FF);
		unsigned for_a = 0;
		for (const Watcher::Arm &a : b.m.arms) if (a.which == 0) for_a++;
		(for_a == 0) ? ok("count 255: the timer is not armed", "the sink receives nothing for A")
		             : failed("count 255: the timer is not armed",
		                      std::to_string(for_a) + " arm(s) received, expected none");
	}

	// ---- 3. THE THREE LEVELS THE DRIVER ASKS FOR, measured at the source
	// The contract (§3) says MIDI at level 3, timer A at 2, timer B at 1, and timer C not armed. Those
	// levels are not in a register: they are ASSEMBLED bit by bit by DecodeSCI out of SCILV0/1/2 -- bit 0
	// of the level from SCILV0, bit 1 from SCILV1, bit 2 from SCILV2. With the driver's table (0x0088,
	// 0x0048, 0x0000) that does give 3, 2 and 1, and this is where it is checked rather than believed.
	//
	// CAREFUL, and it is a trap for a rewrite: the levels are only RECOMPUTED when one of the words
	// 0x424-0x429 is written. Arming SCIEB before SCILV leaves the levels at zero.
	{
		std::string lines;
		bool correct = true;
		struct Case { int timer; int level; const char *name; };
		const Case cases[] = {
			{ 0, 2, "timer A" },
			{ 1, 1, "timer B" },
		};
		for (const Case &c : cases) {
			Bench6 b;
			b.driver_table();
			b.m.clear();
			b.the_core.timer_fired(c.timer);
			const int n = b.m.last_raised();
			char t[64];
			snprintf(t, sizeof t, "%s%s->%d", lines.empty() ? "" : " ", c.name, n);
			lines += t;
			if (n != c.level) { correct = false; lines += "(exp " + std::to_string(c.level) + ")"; }
		}
		// MIDI: an incoming byte must raise level 3
		{
			Bench6 b;
			b.driver_table();
			b.m.clear();
			b.the_core.midi_in(0x90);
			const int n = b.m.last_raised();
			lines += " midi->" + std::to_string(n);
			if (n != 3) { correct = false; lines += "(exp 3)"; }
		}
		correct ? ok("the levels assembled by SCILV0/1/2", lines)
		        : failed("the levels assembled by SCILV0/1/2", lines);
	}

	// ---- 4. TIMER C IS WIRED BUT NOT ARMED, so it raises nothing
	// The contract says it from the driver's side (SCIEB = 0x00C8 does not have bit 8); here we see it from
	// the chip's side: timer C fires, SCIPD keeps a record of it, and no line goes up.
	{
		Bench6 b;
		b.driver_table();
		b.m.clear();
		b.the_core.timer_fired(2);
		const int n = b.m.last_raised();
		const std::uint16_t scipd = b.read(scsp_contract::SCIPD);
		char t[96];
		snprintf(t, sizeof t, "SCIPD is 0x%04x (bit 8 set), and the trail is: %s", scipd, b.m.trace().c_str());
		(n == -1 && (scipd & 0x100)) ? ok("timer C: recorded but not armed", t)
		                             : failed("timer C: recorded but not armed",
		                                      std::string(t) + "; expected no rise and bit 8");
	}

	// ---- 5. ONE LINE AT A TIME, in a fixed priority order
	// CheckPendingIRQ examines the sources in order and RETURNS at the first one that is both pending and
	// armed: CPU (0x20), then timer A (0x40), then B (0x80), then C (0x100), then MIDI (0x8). So if A and B
	// are both pending, it is A's level that goes up and B's that is never seen.
	//
	// A rewrite raising both -- which seems fairer -- would give the 68000 an interrupt the driver is not
	// expecting at that moment.
	{
		Bench6 b;
		b.driver_table();
		b.the_core.timer_fired(1);   // B pending first
		b.m.clear();
		b.the_core.timer_fired(0);   // then A
		const int n = b.m.last_raised();
		const std::uint16_t scipd = b.read(scsp_contract::SCIPD);
		char t[112];
		snprintf(t, sizeof t, "A and B pending (SCIPD 0x%04x): it is level %d that goes up", scipd, n);
		(n == 2 && (scipd & 0xC0) == 0xC0)
			? ok("A wins over B: one line, then back", t)
			: failed("A wins over B: one line, then back",
			         std::string(t) + "; expected level 2 and both bits set");
	}

	// ---- 6. LINE 0, AND THE EARLY RETURN THAT DECIDES WHEN IT DROPS
	// `m_host.irq(0, CLEAR_LINE)` at the end of CheckPendingIRQ: a FIFTH line, which is none of the four
	// sources. It is precisely what made the interface get redrawn -- the architecture's §2 proposed
	// `irq(int level)` with zero for "nothing left", and the chip in fact drops four independent lines PLUS
	// that one (scsp_core.h says it in full).
	//
	// BUT IT DOES NOT DROP WHEN "NOTHING IS PENDING", and the first version of this case asserted that
	// wrongly. CheckPendingIRQ starts with `if (!pend) return;` and so leaves BEFORE the last line: with
	// SCIPD empty, no line is touched at all. The -0 happens in the other situation, the one where
	// something IS pending and nothing armed matches it -- case 4's timer C is the example, and its trail
	// is indeed a lone -0.
	//
	// Both halves are measured, because a rewrite lowering the line unconditionally would pass the first
	// and fail the second.
	{
		// (a) pending but not armed: line 0 drops
		Bench6 b;
		b.driver_table();
		b.m.clear();
		b.the_core.timer_fired(2);   // bit 8, absent from SCIEB = 0x00C8
		bool zero_drops = false;
		for (const Watcher::Line &l : b.m.lines) if (l.level == 0 && !l.state) zero_drops = true;

		// (b) nothing pending: no line is touched. We go through CheckPendingIRQ again by rewriting SCIEB,
		// the only register whose hook calls it unconditionally.
		Bench6 b2;
		b2.driver_table();
		b2.m.clear();
		b2.write(scsp_contract::SCIEB, scsp_contract::INIT_418_42A[3]);
		const bool silent = b2.m.lines.empty();

		const std::string d = "pending not armed: " + b.m.trace() + "; nothing pending: " + b2.m.trace();
		(zero_drops && silent) ? ok("line 0 drops, but not on an empty slate", d)
		                       : failed("line 0 drops, but not on an empty slate",
		                                d + "; expected a -0 then (nothing)");
	}

	// ---- 7. SCIRE ON AN ALREADY EXPIRED TIMER PUTS IT STRAIGHT BACK TO PENDING
	// scsp.cpp's comment attributes this to real hardware: "if you SCIRE a timer that's expired, it'll
	// immediately pop up again in SCIPD". The counter having stayed at 0xFFFF, the acknowledgement is
	// followed by the bit being set again. And the same comment notes that another game disagrees -- so it
	// is a choice, not a certainty, and it is recorded as such.
	{
		Bench6 b;
		b.driver_table();
		b.the_core.timer_fired(0);
		const std::uint16_t before = b.read(scsp_contract::SCIPD);
		b.write(scsp_contract::SCIRE, 0x0040);
		const std::uint16_t after = b.read(scsp_contract::SCIPD);
		char t[112];
		snprintf(t, sizeof t, "SCIPD 0x%04x, acknowledged, then 0x%04x: bit 6 comes back", before, after);
		((before & 0x40) && (after & 0x40)) ? ok("SCIRE on an expired timer: it pends again", t)
		                                    : failed("SCIRE on an expired timer: it pends again",
		                                             std::string(t) + "; expected bit 6 in both");
	}

	// ---- 8. TIMER A ALSO PENDS ON THE MASTER SIDE, B AND C DO NOT
	// timerA_cb calls MainCheckPendingIRQ(0x40) as well as CheckPendingIRQ; timerB_cb and timerC_cb touch
	// only the slave side. The asymmetry reads in MCIPD (0x42C), which the read copies out of m_mcipd.
	// Nothing in VF3 consults it (scsp_contract.h section 2), but a rewrite making them uniform would
	// change an observable state for nothing.
	{
		std::string lines;
		bool correct = true;
		struct Case { int timer; std::uint16_t expected; const char *name; };
		const Case cases[] = {
			{ 0, 0x0040, "A" },
			{ 1, 0x0000, "B" },
			{ 2, 0x0000, "C" },
		};
		for (const Case &c : cases) {
			Bench6 b;
			b.driver_table();
			b.the_core.timer_fired(c.timer);
			const std::uint16_t mcipd = std::uint16_t(b.read(scsp_contract::MCIPD) & 0x1C0);
			char t[48];
			snprintf(t, sizeof t, "%s%s->%04x", lines.empty() ? "" : " ", c.name, mcipd);
			lines += t;
			if (mcipd != c.expected) { correct = false; lines += "(exp " + std::to_string(c.expected) + ")"; }
		}
		correct ? ok("MCIPD: A pends on the master side, B and C do not", lines)
		        : failed("MCIPD: A pends on the master side, B and C do not", lines);
	}

	// ---- 9. THE DMA, IN BOTH DIRECTIONS, AND ITS ZERO FILL
	// DMEA is the address in RAM (twenty bits: the low sixteen from 0x412, the high four from bits 15-12 of
	// 0x414), DRGA the address in the register window (twelve bits, 0x414), DTLG the length in bytes
	// (0x416), DDIR the direction and DGATE the zero fill. DEXE starts it, and lowers itself at the end.
	//
	// The ring (0x600-0x6FF) is chosen as the register destination: it is the only wide area the window
	// lets you write AND read back without side effects, the slots triggering voices and the common area
	// changing the configuration.
	{
		// (a) RAM -> registers
		Bench6 b;
		for (unsigned k = 0; k < 4; k++) b.ram[0x2000 / 2 + k] = std::uint16_t(0x1100 + k);
		b.write(0x412, 0x2000);         // DMEA low
		b.write(0x414, 0x0600);         // DMEA high = 0, DRGA = 0x600 (the ring)
		b.write(0x416, 0x1008);         // DEXE | DTLG = 8 bytes
		std::uint16_t back[4];
		for (unsigned k = 0; k < 4; k++) back[k] = b.read(0x600 + 2 * k);
		const bool ram_to_reg = back[0] == 0x1100 && back[1] == 0x1101 && back[2] == 0x1102 && back[3] == 0x1103;
		const bool dexe_low = (b.read(0x416) & 0x1000) == 0;

		// (b) registers -> RAM, DDIR at 1
		Bench6 b2;
		for (unsigned k = 0; k < 4; k++) b2.write(0x600 + 2 * k, std::uint16_t(0x2200 + k));
		b2.write(0x412, 0x3000);
		b2.write(0x414, 0x0600);
		b2.write(0x416, 0x3008);        // DDIR | DEXE | 8 bytes
		const bool reg_to_ram = b2.ram[0x3000 / 2] == 0x2200 && b2.ram[0x3000 / 2 + 3] == 0x2203;

		// (c) DGATE: zeros, and the source is not read
		Bench6 b3;
		for (unsigned k = 0; k < 4; k++) b3.write(0x600 + 2 * k, 0xBEEF);
		b3.write(0x412, 0x4000);
		b3.write(0x414, 0x0600);
		b3.write(0x416, 0x5008);        // DGATE | DEXE | 8 bytes, DDIR at 0: zeros into the REGISTERS
		bool zeros = true;
		for (unsigned k = 0; k < 4; k++) if (b3.read(0x600 + 2 * k) != 0) zeros = false;

		char t[160];
		snprintf(t, sizeof t, "RAM->reg %s, reg->RAM %s, DGATE %s, DEXE lowered %s",
		         ram_to_reg ? "yes" : "NO", reg_to_ram ? "yes" : "NO", zeros ? "yes" : "NO",
		         dexe_low ? "yes" : "NO");
		(ram_to_reg && reg_to_ram && zeros && dexe_low) ? ok("the DMA in both directions, and DGATE", t)
		                                               : failed("the DMA in both directions, and DGATE", t);
	}

	// ---- 10. A DMA CANNOT OVERWRITE ITS OWN PARAMETERS, and only in one direction
	// exec_dma saves 0x412, 0x414 and 0x416 before the transfer and restores them after -- but ONLY when
	// DDIR is 0, that is, when the destination is the register window and so could contain the control
	// registers themselves. The code's comment says it: "DMA *can't* overwrite its parameters". It is
	// checked by aiming at 0x412 with a DMA coming from RAM.
	{
		Bench6 b;
		for (unsigned k = 0; k < 4; k++) b.ram[0x5000 / 2 + k] = 0xFFFF;   // enough to overwrite everything
		b.write(0x412, 0x5000);
		b.write(0x414, 0x0412);         // DRGA aims at the DMA registers themselves
		b.write(0x416, 0x1008);         // DEXE | 8 bytes
		const std::uint16_t a = b.read(0x412), c = b.read(0x414), d = b.read(0x416);
		char t[128];
		snprintf(t, sizeof t, "after a DMA aimed at 0x412: 0x412=%04x 0x414=%04x 0x416=%04x", a, c, d);
		(a == 0x5000 && c == 0x0412 && (d & 0x0FFE) == 0x008)
			? ok("a DMA does not overwrite itself", t)
			: failed("a DMA does not overwrite itself", std::string(t) + "; expected 5000 0412 ...008");
	}

	// ---- 11. MIDI: a byte comes in, the line goes up; we read it back, it drops
	// An incoming byte sets bit 3 of SCIPD and raises the MIDI line. Reading 0x404 pops a byte and, when
	// the queue empties, lowers the line and clears the bit -- done in UpdateRegR and not in the read
	// itself, which means a rewrite returning the register without going through a read hook would leave
	// the interrupt raised for ever.
	{
		Bench6 b;
		b.driver_table();
		b.m.clear();
		b.the_core.midi_in(0x90);
		const int on_entry = b.m.last_raised();
		const std::uint16_t scipd_before = b.read(scsp_contract::SCIPD);
		const std::uint16_t byte = std::uint16_t(b.read(0x404) & 0xFF);
		const std::uint16_t scipd_after = b.read(scsp_contract::SCIPD);
		char t[144];
		snprintf(t, sizeof t, "level %d on entry, byte read back 0x%02x, SCIPD 0x%04x then 0x%04x",
		         on_entry, byte, scipd_before, scipd_after);
		(on_entry == 3 && byte == 0x90 && (scipd_before & 8) && !(scipd_after & 8))
			? ok("incoming MIDI: raised, read back, dropped", t)
			: failed("incoming MIDI: raised, read back, dropped",
			         std::string(t) + "; expected level 3, 0x90, bit 3 set then cleared");
	}

	printf("\n");
	if (failures) { printf("%d failure(s).\n", failures); return 1; }
	printf("all cases pass\n");
	return 0;
}
