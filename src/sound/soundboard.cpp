// license:BSD-3-Clause
// copyright-holders:R. Belmont, Ville Linde
// The Model 3 sound board. See soundboard.h.
#include "soundboard.h"
#include "sound_prof.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
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
		case 0x1: return b.scsp[0]->read((a & 0xFFF) >> 1);
		case 0x3: return b.scsp[1]->read((a & 0xFFF) >> 1);
		default: return peek(a);
		}
	}
	void write(offs_t a, u16 data, u16 mem_mask) override
	{
		a &= 0xFFFFFE;
		// VF3_SCSP_DSPLOG=file: every write to either chip's DSP registers (0x700 and up), with the
		// frame. Written to settle one question the sound program's own code cannot answer: its
		// effects-preset loader (snd_charger_preset_dsp at 0x602B32, see docs/fv2-son-programme.md)
		// writes COEF at +0x700 and MADRS at +0x780 from a TABLE of commands, each carrying a byte
		// offset -- so whether anything ever lands in 0x7C0-0x7FF, the range this port and MAME
		// disagree about (docs/scsp_references.md), depends on data and not on the instructions.
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
			// 659 sample records point there (docs/fv2-son-programme.md §4 septies). If the chips play
			// the bank, a slot's SA says so -- and that is a measurement, not a reading.
			const bool slotsa = off < 0x400 && (off & 0x1E) <= 0x02;   // words 0x0 and 0x2 of a slot
			const char *zone = off >= 0x700
			                 ? (off < 0x780 ? "COEF" : off < 0x7c0 ? "MADRS"
			                    : off < 0x800 ? "7C0-7FF" : off < 0xc00 ? "MPRO" : "autre")
			                 : (off >= 0x600 && off < 0x700) ? "RINGBUF"
			                 : slot6 ? "SLOT6" : slotsa ? "SLOTSA" : nullptr;
			if (zone) {
				if (slotsa)
					fprintf(dl, "%d chip%d %03x %-7s %04x mask %04x fente %u mot %u\n", b.frames,
					        int((a >> 20) == 3), off, zone, data, mem_mask, off / 0x20, (off & 0x1F) / 2);
				else if (slot6)
					fprintf(dl, "%d chip%d %03x %-7s %04x mask %04x fente %u stwinh %d\n", b.frames,
					        int((a >> 20) == 3), off, zone, data, mem_mask, off / 0x20, (data >> 9) & 1);
				else
					fprintf(dl, "%d chip%d %03x %-7s %04x mask %04x\n", b.frames, int((a >> 20) == 3), off, zone, data, mem_mask);
			}
		}
		switch (a >> 20) {
		case 0x0: case 0x2: {
			u16 &w = b.ram[(a >> 20) == 2][(a & 0xFFFFF) >> 1];
			w = (w & ~mem_mask) | (data & mem_mask);
			break;
		}
		case 0x1:
			if (static FILE *wl = getenv("VF3_SCSP_WLOG") ? fopen(getenv("VF3_SCSP_WLOG"), "w") : nullptr; wl && (a & 0xFFFF) < 0x430) {
				if (mem_mask == 0xFFFF) fprintf(wl, "%d w %04x %04x\n", b.frames, a & 0xFFFF, data);
				else if (mem_mask == 0xFF00) fprintf(wl, "%d b %04x %02x\n", b.frames, a & 0xFFFF, data >> 8);
				else fprintf(wl, "%d b %04x %02x\n", b.frames, (a & 0xFFFF) + 1, data & 0xFF);
			}
			b.scsp[0]->write((a & 0xFFF) >> 1, data, mem_mask);
			break;
		case 0x3: b.scsp[1]->write((a & 0xFFF) >> 1, data, mem_mask); break;
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
	for (int n = 0; n < 2; n++) {
		scsp_device::host h;
		if (n == 0)
			h.irq = [this](int line, int state) { cpu->set_input_line(line, state == CLEAR_LINE ? CLEAR_LINE : ASSERT_LINE); };
		else
			h.irq = [](int, int) {};
		h.midi_out = [](int) {};
		h.timer_adjust = [this, n](int t, scsp_device::attotime delay, scsp_device::attotime period) {
			Timer &tm = timers[n][t];
			if (delay == scsp_device::NEVER) { tm.on = false; return; }
			auto to_cycles = [](scsp_device::attotime as) {
				return uint64_t((as * CLOCK + AS_PER_SEC - 1) / AS_PER_SEC);
			};
			tm.on = true;
			tm.at = cycles_now() + std::max<uint64_t>(1, to_cycles(delay));
			// timers A-C count the samples: due on one (the 68000's cycles at 256 a sample), as Supermodel's
			if (t < scsp_device::TIMER_RCV) tm.at = std::max(tm.at / 256 * 256, cycles_now() + 1);
			tm.period = period ? std::max<uint64_t>(1, to_cycles(period)) : 0;
		};
		h.sample_index = [this] { return cycles_now() / 256; };
		h.time_is_zero = [this] { return cycles_now() == 0; };
		scsp[n] = std::make_unique<scsp_device>(45158400 / 2, ram[n].data(), uint32_t(ram[n].size() * 2), h);
		scsp[n]->group_of = [this, n](int slot) { return group_of(n, slot); };
		// the MADRS mirror, per game (soundboard.h), unless one of the two variables says otherwise
		{
			const char *sep = getenv("VF3_SCSP_MADRS_SEPARATE"), *mir = getenv("VF3_SCSP_MADRS_MIRROR");
			scsp[n]->set_madrs_mirror((sep && *sep == '1') ? false : (mir && *mir == '1') ? true : madrs_mirror);
		}
		for (int g = 0; g < 2; g++) scsp[n]->set_group_gain(g, volume[g]);
		if (getenv("VF3_KEYON_LOG")) {   // (tests: each voice started, with the 68000's state then)
			static FILE *kl = fopen(getenv("VF3_KEYON_LOG"), "w");
			scsp[n]->on_key_on = [this, n](const scsp_device::key_on &k) {
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
	for (auto &s : scsp)
		if (s) for (int g = 0; g < 2; g++) s->set_group_gain(g, volume[g]);
}

uint16_t SoundBoard::reg_word(int n, uint32_t i)
{
	return scsp[n]->read(i);
}

void SoundBoard::energy_on(bool on)
{
	for (auto &s : scsp)
		if (s) s->m_probe = on;
}

void SoundBoard::energy_take(double e[2][32])
{
	for (int n = 0; n < 2; n++)
		for (int sl = 0; sl < 32; sl++) {
			e[n][sl] = scsp[n] ? scsp[n]->m_probe_direct[sl] : 0.0;
			if (scsp[n]) scsp[n]->m_probe_direct[sl] = 0.0;
		}
}

void SoundBoard::midi_in(uint8_t byte)
{
	scsp[0]->midi_in_byte(byte);
}

void SoundBoard::fire_due()
{
	for (int n = 0; n < 2; n++)
		for (int t = 0; t < scsp_device::N_TIMERS; t++) {
			Timer &tm = timers[n][t];
			if (tm.on && tm.at <= now) {
				if (tm.period) tm.at += tm.period;
				else tm.on = false;
				scsp[n]->timer_fired(t);
			}
		}
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
		scsp[0]->update();
		scsp[1]->update();
	}
	std::vector<float> &a = scsp[0]->m_out, &b = scsp[1]->m_out;
	const size_t n = std::min(a.size(), b.size());
	float ga = scsp[0]->output_gain(), gb = scsp[1]->output_gain();
	static const int solo = getenv("VF3_SCSP_SOLO") ? atoi(getenv("VF3_SCSP_SOLO")) : -1;   // (tests: one SCSP alone)
	if (solo == 0) gb = 0;
	if (solo == 1) ga = 0;
	// above the board's own level (gain 1), a peak limiter keeps the loudest moments under -1 dBFS: down at
	// once, back up over about 0.1 s (as VF2's)
	constexpr double CEILING = 0.891, RELEASE = 1.0 / (0.1 * 44100);
	sndprof::Scope prof_mix(sndprof::MIX);
	for (size_t i = 0; i + 1 < n; i += 2) {
		double l = (double(a[i]) * ga + double(b[i]) * gb) * gain, r = (double(a[i + 1]) * ga + double(b[i + 1]) * gb) * gain;
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
}
