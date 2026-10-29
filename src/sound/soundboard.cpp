// license:BSD-3-Clause
// copyright-holders:R. Belmont, Ville Linde
// The Model 3 sound board. See soundboard.h.
#include "soundboard.h"
#include "sound_prof.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <cmath>

namespace {
const uint64_t CLOCK = 11289600;           // the 68000's (45.1584 MHz / 4): 256 a sample
const unsigned __int128 AS_PER_SEC = (unsigned __int128)1000000000000000000ull;
}

struct SoundBoard::Bus : m68000_bus
{
	SoundBoard &b;
	explicit Bus(SoundBoard &s) : b(s) {}
	static uint16_t word(const std::vector<uint8_t> &v, uint32_t a)
	{
		a &= ~1u;
		return a + 1 < v.size() ? uint16_t(v[a] | v[a + 1] << 8) : 0;
	}
	u16 peek(offs_t a) override
	{
		a &= 0xFFFFFE;
		switch (a >> 20) {
		case 0x0: return b.ram[0][(a & 0xFFFFF) >> 1];
		case 0x2: return b.ram[1][(a & 0xFFFFF) >> 1];
		case 0x6: return word(*b.rom, a & 0x7FFFF);
		default:
			if (a >= 0x800000) return word(*b.samples, ((b.ctrl & 0x10) ? 0x800000 : 0) + (a & 0x7FFFFF));
			return 0;
		}
	}
	u16 read(int space, offs_t a, u16 mem_mask) override
	{
		if (space == CPU_SPACE)
			return u16(0x18 + ((a >> 1) & 7));   // the autovectors
		a &= 0xFFFFFE;
		switch (a >> 20) {
		// The two chips up to date BEFORE reading them: the new interface does not bring time up to date
		// of its own accord (soundboard.h, sync_sound).
		case 0x1: b.sync_sound(0); return b.head[0]->read_reg((a & 0xFFF) >> 1);
		case 0x3: b.sync_sound(1); return b.head[1]->read_reg((a & 0xFFF) >> 1);
		default: return peek(a);
		}
	}
	void write(offs_t a, u16 data, u16 mem_mask) override
	{
		a &= 0xFFFFFE;
		// VF3_SCSP_DSPLOG=file: every write to either chip's DSP registers (0x700 and up), with the
		// frame. Written to settle one question the sound program's own code cannot answer: its
		// effects-preset loader (snd_charger_preset_dsp at 0x602B32, see docs/fv2-sound-program.md)
		// writes COEF at +0x700 and MADRS at +0x780 from a TABLE of commands, each carrying a byte
		// offset -- so whether anything ever lands in 0x7C0-0x7FF, the range this port and MAME
		// disagree about (scsp/docs/scsp_references.md), depends on data and not on the instructions.
		// This says what actually happens.
		// It also logs two ranges outside the DSP, for the other open question of that document (its
		// section 4): the driver accumulates per slot into +0x600, which this port routes to MAME's
		// FM ring buffer and the audio path rewrites every sample -- unless STWINH protects the
		// entry. STWINH is bit 9 of each slot's word 0x6, so slot words 0x6 are logged too, and the
		// two in one ordered log say whether the driver protects what it just wrote.
		if (static FILE *dl = getenv("VF3_SCSP_DSPLOG") ? fopen(getenv("VF3_SCSP_DSPLOG"), "w") : nullptr;
		    dl && ((a >> 20) == 1 || (a >> 20) == 3)) {
			const unsigned off = a & 0xFFF;
			const bool slot6 = off < 0x400 && (off & 0x1E) == 0x0C;   // word 0x6 of a slot
			// Words 0x0 and 0x2 of a slot carry SA, the sample's start address: bits 19-16 in the low
			// nibble of word 0x0, bits 15-0 in word 0x2. Logged to answer one question the 68000's code
			// cannot: 0x600524 loads a bank of sixteen one-cycle waveforms into 0x2000-0x2FFF of each
			// sound RAM, and NOTHING in the program names an address in that range, nor does any of the
			// 659 sample records point there (docs/fv2-sound-program.md §4 septies). If the chips play
			// the bank, a slot's SA says so -- and that is a measurement, not a reading.
			const bool slotsa = off < 0x400 && (off & 0x1E) <= 0x02;   // words 0x0 and 0x2 of a slot
			const char *zone = off >= 0x700
			                 ? (off < 0x780 ? "COEF" : off < 0x7c0 ? "MADRS"
			                    : off < 0x800 ? "7C0-7FF" : off < 0xc00 ? "MPRO" : "other")
			                 : (off >= 0x600 && off < 0x700) ? "RINGBUF"
			                 : slot6 ? "SLOT6" : slotsa ? "SLOTSA" : nullptr;
			if (zone) {
				if (slotsa)
					fprintf(dl, "%d chip%d %03x %-7s %04x mask %04x slot %u word %u\n", b.frames,
					        int((a >> 20) == 3), off, zone, data, mem_mask, off / 0x20, (off & 0x1F) / 2);
				else if (slot6)
					fprintf(dl, "%d chip%d %03x %-7s %04x mask %04x slot %u stwinh %d\n", b.frames,
					        int((a >> 20) == 3), off, zone, data, mem_mask, off / 0x20, (data >> 9) & 1);
				else
					fprintf(dl, "%d chip%d %03x %-7s %04x mask %04x\n", b.frames, int((a >> 20) == 3), off, zone, data, mem_mask);
			}
		}
		switch (a >> 20) {
		case 0x0: case 0x2: {
			const int chip = (a >> 20) == 2;
			const u32 word = (a & 0xFFFFF) >> 1;
			u16 &w = b.ram[chip][word];
			w = (w & ~mem_mask) | (data & mem_mask);
			// The trace has to carry them: the chip plays from RAM, and a replay without them would have
			// no sample to play. It is one of VF3_SCSP_WLOG's four gaps (scsp_trace.h).
			if (b.trace[chip]) b.trace[chip]->ram_written(word, data, mem_mask);
			break;
		}
		case 0x1:
			if (static FILE *wl = getenv("VF3_SCSP_WLOG") ? fopen(getenv("VF3_SCSP_WLOG"), "w") : nullptr; wl && (a & 0xFFFF) < 0x430) {
				if (mem_mask == 0xFFFF) fprintf(wl, "%d w %04x %04x\n", b.frames, a & 0xFFFF, data);
				else if (mem_mask == 0xFF00) fprintf(wl, "%d b %04x %02x\n", b.frames, a & 0xFFFF, data >> 8);
				else fprintf(wl, "%d b %04x %02x\n", b.frames, (a & 0xFFFF) + 1, data & 0xFF);
			}
			b.sync_sound(0);
			b.head[0]->write_reg((a & 0xFFF) >> 1, data, mem_mask);
			break;
		case 0x3: b.sync_sound(1); b.head[1]->write_reg((a & 0xFFF) >> 1, data, mem_mask); break;
		case 0x4:
			if (a == 0x400000 && (mem_mask & 0xFF)) b.ctrl = uint8_t(data);
			break;
		default: break;
		}
	}
};

SoundBoard::SoundBoard() = default;
SoundBoard::~SoundBoard() = default;

void SoundBoard::attach(const std::vector<uint8_t> *r, const std::vector<uint8_t> *s)
{
	rom = r;
	samples = s;
}

uint64_t SoundBoard::cycles_now() const
{
	return now + (running ? uint64_t(cpu->cycles_used()) : 0);
}

void SoundBoard::reset()
{
	for (auto &r : ram) r.assign(0x80000, 0);
	// the 68000 starts from the program's vectors, copied to RAM as the board does
	for (int i = 0; i < 8; i++) ram[0][i] = Bus::word(*rom, 2 * i);
	ctrl = 0;
	now = 0;
	frac = 0;
	for (auto &c : timers) for (auto &t : c) t = Timer();
	bus = std::make_unique<Bus>(*this);
	cpu = std::make_unique<m68000_device>(*bus, uint32_t(CLOCK));
	midi_out_dropped = 0;
	for (int n = 0; n < 2; n++) {
		pcm[n].clear();
		sinks[n].b = this;
		sinks[n].n = n;
		// THE CORE IS CHOSEN HERE. MAME's by default, so that nothing changes unless it is asked for;
		// VF3_SCSP_CORE=own takes the project's (src/sound/scsp_own.h), byte-identical under the
		// replayer but without the three extras below.
		static const char *name = []() {
			const char *e = getenv("VF3_SCSP_CORE");
			return e && *e ? e : "mame";
		}();
		scsp[n] = scsp::make(name, 45158400 / 2, ram[n].data(), uint32_t(ram[n].size() * 2), sinks[n]);
		if (!scsp[n]) { fprintf(stderr, "soundboard: unknown core \"%s\"; known: %s\n",
		                        name, scsp::known_names()); std::exit(2); }
		extras[n] = dynamic_cast<scsp_mame *>(scsp[n].get());
		if (!extras[n] && n == 0)
			fprintf(stderr, "sound: core \"%s\" -- the per-group gains, the energy probe and the key-on"
			                " log do not apply (soundboard.h)\n", name);
		// THE MADRS MIRROR IS SET BEFORE THE TRACE, and the order is a divergence's lesson: it is the only
		// one of the four extras that changes the sound, and it does not go through scsp::core. Set AFTER
		// the recorder -- as it used to be -- it did not go into the trace's header, a replay assumed it
		// `true`, and on Fighting Vipers 2, which already had it false then, the replay diverged from the
		// board at sample 772 067 (scsp/docs/scsp_validation.md §3 c). Set here, the recorder READS it
		// back from the chip: there is nothing left to remember to tell it.
		//
		// It was per game until 2026-10-29 and is false for both now (soundboard.h); setting it here is
		// what keeps the trace honest either way.
		{
			const char *sep = getenv("VF3_SCSP_MADRS_SEPARATE"), *mir = getenv("VF3_SCSP_MADRS_MIRROR");
			const bool on = (sep && *sep == '1') ? false : (mir && *mir == '1') ? true : madrs_mirror;
			// Both cores carry it, each on its own concrete class, through the single door of
			// scsp_select.h -- the interface does not carry it, because it is not the chip's business
			// but the game's (scsp_core.h).
			if (!scsp::set_madrs_mirror_on(*scsp[n], on) && n == 0)
				fprintf(stderr, "sound: core \"%s\" -- the MADRS mirror does not set on it (scsp_select.h)\n", name);
		}
		// VF3_SCSP_TRACE=f: the access trace, one file per chip (f.0 and f.1), for tools/sound/replay.
		// It begins HERE, at the reset, because the RAM is known there -- eight words, the 68000's vectors
		// that the board has just copied into it. The recorder checks that rather than assuming it.
		head[n] = scsp[n].get();
		trace[n].reset();
		if (const char *base = getenv("VF3_SCSP_TRACE")) {
			const std::string f = std::string(base) + "." + std::to_string(n);
			auto e = std::make_unique<scsp::recorder>(*scsp[n], f.c_str(), unsigned(n), 45158400 / 2,
			                                             ram[n].data(), uint32_t(ram[n].size() * 2));
			if (e->good()) { head[n] = e.get(); trace[n] = std::move(e); }
		}
		if (extras[n]) extras[n]->device().group_of = [this, n](int slot) { return group_of(n, slot); };
		if (extras[n]) for (int g = 0; g < 2; g++) extras[n]->device().set_group_gain(g, volume[g]);
		if (getenv("VF3_KEYON_LOG")) {   // (tests: each voice started, with the 68000's state then)
			static FILE *kl = fopen(getenv("VF3_KEYON_LOG"), "w");
			if (extras[n]) extras[n]->device().on_key_on = [this, n](const scsp_device::key_on &k) {
				if (!kl) return;
				auto w = [&](uint32_t a) {
					a &= 0xFFFFFF;
					return a < 0x100000 ? unsigned(ram[0][(a >> 1) & 0x7FFFF]) : (a >> 20) == 2 ? unsigned(ram[1][(a & 0xFFFFF) >> 1]) : 0u;
				};
				// the voice's own shape first, because a sound that SWELLS is an envelope question: tl is its
				// attenuation, ar/d1r/d2r/rr its four rates, dl the level it decays to, and lpctl/lsa/lea how
				// it loops. A note whose rr is 0 never lets go, and then every new note piles on the last.
				fprintf(kl, "voix %d chip %d slot %2d tl %3d ar %2d d1r %2d d2r %2d rr %2d dl %2d eghold %d lpctl %d lsa %06x lea %06x sa %06x\n",
						frames, n, k.slot, k.tl, k.ar, k.d1r, k.d2r, k.rr, k.dl, k.eghold, k.lpctl, k.lsa, k.lea, k.sa);
				fprintf(kl, "%d %d %2d pc %06x a0 %06x a1 %06x a2 %06x a3 %06x a4 %06x a5 %06x a6 %06x d0 %08x d1 %08x d2 %08x d3 %08x [a0] %04x %04x [a1] %04x [a2] %04x [a3] %04x sa %06x\n",
						frames, n, k.slot, cpu->pc(), cpu->reg(8), cpu->reg(9), cpu->reg(10), cpu->reg(11), cpu->reg(12), cpu->reg(13), cpu->reg(14),
						cpu->reg(0), cpu->reg(1), cpu->reg(2), cpu->reg(3), w(cpu->reg(8)), w(cpu->reg(8) + 2), w(cpu->reg(9)), w(cpu->reg(10)), w(cpu->reg(11)), k.sa);
				// the return addresses on the stack (the program's, 0x600000 on)
				const uint32_t sp = cpu->reg(16);
				fprintf(kl, "   stack");
				for (int i = 0; i < 24; i++) {
					const uint32_t v = w(sp + 2 * i) << 16 | w(sp + 2 * i + 2);
					if ((v & 0xFF0000) == 0x600000) fprintf(kl, " +%d:%06x", 2 * i, v & 0xFFFFFF);
				}
				fprintf(kl, "\n");
				for (int r : {12, 14}) {   // the 32 bytes at a4 and at a6
					fprintf(kl, "   a%d", r - 8);
					for (int i = 0; i < 16; i++) fprintf(kl, " %04x", w(cpu->reg(r) + 2 * i));
					fprintf(kl, "\n");
				}
			};
		}
	}
	cpu->start();
	cpu->reset();
}

int SoundBoard::group_of(int n, int slot) const
{
	if (!driver_known) return 0;   // (soundboard.h: not the driver these records belong to)
	const uint16_t *r = &ram[n][(0x400 + slot * 0x20) >> 1];
	if (r[0] != (n ? 0x30 : 0x10) || r[1] != slot * 0x20) return 0;   // not the driver's record: the music's
	return (r[2] & 0xFF) >= 0x0F;
}

void SoundBoard::set_volumes(float music, float effects)
{
	volume[0] = music;
	volume[1] = effects;
	for (int n = 0; n < 2; n++)
		if (extras[n]) for (int g = 0; g < 2; g++) extras[n]->device().set_group_gain(g, volume[g]);
}

uint16_t SoundBoard::reg_word(int n, uint32_t i)
{
	sync_sound(n);
	return head[n]->read_reg(i);   // (this used to be a ->device().read: the interface is enough)
}

void SoundBoard::energy_on(bool on)
{
	for (auto *e : extras)
		if (e) e->device().m_probe = on;
}

void SoundBoard::energy_take(double e[2][32])
{
	for (int n = 0; n < 2; n++)
		for (int sl = 0; sl < 32; sl++) {
			e[n][sl] = extras[n] ? extras[n]->device().m_probe_direct[sl] : 0.0;
			if (extras[n]) extras[n]->device().m_probe_direct[sl] = 0.0;
		}
}

void SoundBoard::midi_in(uint8_t byte)
{
	head[0]->midi_in(byte);
}

void SoundBoard::fire_due()
{
	for (int n = 0; n < 2; n++)
		for (int t = 0; t < scsp_device::N_TIMERS; t++) {
			Timer &tm = timers[n][t];
			if (tm.on && tm.at <= now) {
				if (tm.period) tm.at += tm.period;
				else tm.on = false;
				head[n]->timer_fired(t);
			}
		}
}

// What the board does with the chip's three events. The old wiring was five std::function laid down at
// construction; here are the same moves, named rather than anonymous.
void SoundBoard::Sink::irq(int level, bool on)
{
	if (n != 0) return;   // only the first chip is wired to the 68000's lines
	b->cpu->set_input_line(level, on ? ASSERT_LINE : CLEAR_LINE);
}

void SoundBoard::Sink::midi_out(std::uint8_t)
{
	// The board does nothing with it -- but the driver uses it in THREE places (a 0xF0 every ten ticks, a
	// per-track copy, a MIDI thru: docs/fv2-sound-program.md), so we COUNT rather than keep quiet.
	++b->midi_out_dropped;
}

void SoundBoard::Sink::timer(int which, std::uint32_t samples, std::uint32_t period)
{
	Timer &tm = b->timers[n][which];
	if (samples == 0) { tm.on = false; return; }
	// The interface counts in SAMPLES, the board in 68000 cycles: 11 289 600 / 44 100 = 256 exactly, so the
	// conversion is exact for timers A-C, which the old code rounded down to a multiple of 256 anyway.
	//
	// AND THIS IS THE SECOND PLACE WHERE §2'S SPLIT DOES NOT QUITE HOLD: the MIDI receiver's and
	// transmitter's clocks (TIMER_RCV, TIMER_TRA) are NOT quantised to 256 by the old code, they keep the
	// cycle's precision. Passing them through samples loses it. Whether that is audible or not is for the
	// differential harness to say -- not for me to assume.
	tm.on = true;
	tm.at = b->cycles_now() + std::max<std::uint64_t>(1, std::uint64_t(samples) * 256);
	if (which < scsp_device::TIMER_RCV) tm.at = std::max(tm.at / 256 * 256, b->cycles_now() + 1);
	tm.period = period ? std::max<std::uint64_t>(1, std::uint64_t(period) * 256) : 0;
}

// Brings the two chips up to the 68000's time, pushing what they owe. See soundboard.h: without this call
// before every register access, the writes would no longer fall at the same place in the sound.
void SoundBoard::sync_sound(int n)
{
	if (!scsp[n]) return;
	// At clock `target` there must exist target + 1 samples: the old core counts INCLUSIVELY, sample index 0
	// included (scsp_mame.cpp, sample_index). Without that +1 the game's very first sample was never made --
	// one pair out of 3 219 865, which only the harness saw.
	const uint64_t want = cycles_now() / 256 + 1;
	const uint64_t made = head[n]->samples_made();
	if (want <= made) return;
	const uint32_t k = uint32_t(want - made);
	const size_t base = pcm[n].size();
	pcm[n].resize(base + size_t(k) * 2);
	head[n]->run(k, pcm[n].data() + base);
}

void SoundBoard::run(double seconds, std::vector<int16_t> &out)
{
	frames++;
	sndprof::Scope prof_run(sndprof::RUN);
	frac += seconds * 44100.0;
	const uint64_t samples_now = uint64_t(frac);
	frac -= double(samples_now);
	const uint64_t end = (now / 256 + samples_now) * 256;
	while (now < end) {
		// at most a sample at a time: a timer the 68000 sets again in its interrupt (the sound driver's tick, timer A
		// about 880 times a second) is due within the slice it was set in; run to the next timer known before, the
		// slice ran to the end of the frame and the music's tick came once a frame (the music slowed to a crawl)
		uint64_t next = std::min(end, (now / 256 + 1) * 256);
		for (auto &c : timers)
			for (auto &t : c)
				if (t.on && t.at > now && t.at < next) next = t.at;
		running = true;
		int used;
		{
			sndprof::Scope prof_cpu(sndprof::M68K);
			used = cpu->run(int(next - now));
		}
		running = false;
		now += uint64_t(std::max(used, 1));
		fire_due();
	}
	{
		sndprof::Scope prof_flush(sndprof::FLUSH);
		sync_sound();
	}
	// The samples accumulated through the frame by sync_sound(), in integers on the 18-bit scale. The old
	// path read floats from the chip; the conversion back is EXACT (scsp_mame.h, note 2), so dividing by
	// FULL_SCALE here gives exactly what the old path gave.
	std::vector<std::int32_t> &a = pcm[0], &b = pcm[1];
	const size_t n = std::min(a.size(), b.size());
	constexpr double SCALE = 1.0 / scsp_mame::FULL_SCALE;
	// MVOL, THE MASTER VOLUME, IS READ FROM THE REGISTER: the chip does not apply it itself, it only exposes
	// it (scsp_contract.h §13 point 7), and a register is read through the interface. That ->device() was a
	// debt for nothing.
	auto mvol = [&](int n) { return float(head[n]->read_reg(0x400 / 2) & 0x000F) / 15.0f; };
	float ga = mvol(0), gb = mvol(1);
	static const int solo = getenv("VF3_SCSP_SOLO") ? atoi(getenv("VF3_SCSP_SOLO")) : -1;   // (tests: one SCSP alone)
	if (solo == 0) gb = 0;
	if (solo == 1) ga = 0;
	// above the board's own level (gain 1), a peak limiter keeps the loudest moments under -1 dBFS: down at
	// once, back up over about 0.1 s (as VF2's)
	constexpr double CEILING = 0.891, RELEASE = 1.0 / (0.1 * 44100);
	sndprof::Scope prof_mix(sndprof::MIX);
	for (size_t i = 0; i + 1 < n; i += 2) {
		double l = (double(a[i]) * SCALE * ga + double(b[i]) * SCALE * gb) * gain,
		       r = (double(a[i + 1]) * SCALE * ga + double(b[i + 1]) * SCALE * gb) * gain;
		if (gain > 1.0) {
			const double peak = std::max(std::fabs(l), std::fabs(r));
			const double want = peak > CEILING ? CEILING / peak : 1.0;
			limit = want < limit ? want : limit + (want - limit) * RELEASE;
			l *= limit;
			r *= limit;
		}
		out.push_back(int16_t(std::clamp(std::lround(l * 32768.0), -32768l, 32767l)));
		out.push_back(int16_t(std::clamp(std::lround(r * 32768.0), -32768l, 32767l)));
	}
	a.erase(a.begin(), a.begin() + n);
	b.erase(b.begin(), b.begin() + n);
	// VF3_SCSP_LEFTOVER=N: what the two chips leave behind them, every N frames. The board mixes only
	// min(chip 0, chip 1): if one is a sample ahead at the end of a frame, that sample waits for the next
	// one. That is normal; what would not be is a leftover that GROWS.
	if (static const unsigned long every = getenv("VF3_SCSP_LEFTOVER")
	                                     ? strtoul(getenv("VF3_SCSP_LEFTOVER"), nullptr, 10) : 0;
	    every && frames % every == 0)
		fprintf(stderr, "leftover frame %llu: chip 0 %zu pairs, chip 1 %zu pairs (mixed %zu)\n",
		        (unsigned long long)frames, a.size() / 2, b.size() / 2, n / 2);
	// (the two leftovers, if any, go on with the next frame: that is what the old path did with m_out, and
	// the one-sample offset between the two chips must not be lost)
}
