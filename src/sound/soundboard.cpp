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
		// Les deux puces a l'heure AVANT de les lire : l'interface neuve ne remet pas le temps a jour
		// d'elle-meme (soundboard.h, sync_sound).
		case 0x1: b.sync_sound(0); return b.tete[0]->read_reg((a & 0xFFF) >> 1);
		case 0x3: b.sync_sound(1); return b.tete[1]->read_reg((a & 0xFFF) >> 1);
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
			const int puce = (a >> 20) == 2;
			const u32 mot = (a & 0xFFFFF) >> 1;
			u16 &w = b.ram[puce][mot];
			w = (w & ~mem_mask) | (data & mem_mask);
			// La trace doit les porter : la puce joue depuis la RAM, et un rejeu sans elles n'aurait aucun
			// echantillon a jouer. C'est l'un des quatre manques de VF3_SCSP_WLOG (scsp_trace.h).
			if (b.trace[puce]) b.trace[puce]->ram_ecrite(mot, data, mem_mask);
			break;
		}
		case 0x1:
			if (static FILE *wl = getenv("VF3_SCSP_WLOG") ? fopen(getenv("VF3_SCSP_WLOG"), "w") : nullptr; wl && (a & 0xFFFF) < 0x430) {
				if (mem_mask == 0xFFFF) fprintf(wl, "%d w %04x %04x\n", b.frames, a & 0xFFFF, data);
				else if (mem_mask == 0xFF00) fprintf(wl, "%d b %04x %02x\n", b.frames, a & 0xFFFF, data >> 8);
				else fprintf(wl, "%d b %04x %02x\n", b.frames, (a & 0xFFFF) + 1, data & 0xFF);
			}
			b.sync_sound(0);
			b.tete[0]->write_reg((a & 0xFFF) >> 1, data, mem_mask);
			break;
		case 0x3: b.sync_sound(1); b.tete[1]->write_reg((a & 0xFFF) >> 1, data, mem_mask); break;
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
	midi_out_jetes = 0;
	for (int n = 0; n < 2; n++) {
		pcm[n].clear();
		sinks[n].b = this;
		sinks[n].n = n;
		// LE COEUR SE CHOISIT. Par defaut celui de MAME, pour que rien ne change sans qu'on le demande ;
		// VF3_SCSP_COEUR=maison prend celui du projet (src/sound/scsp_maison.h), identique a l'octet au
		// rejoueur mais sans les trois extras ci-dessous.
		static const char *nom = []() {
			const char *e = getenv("VF3_SCSP_COEUR");
			return e && *e ? e : "mame";
		}();
		scsp[n] = scsp::make(nom, 45158400 / 2, ram[n].data(), uint32_t(ram[n].size() * 2), sinks[n]);
		if (!scsp[n]) { fprintf(stderr, "soundboard: coeur \"%s\" inconnu ; connus : %s\n",
		                        nom, scsp::noms_connus()); std::exit(2); }
		extras[n] = dynamic_cast<scsp_mame *>(scsp[n].get());
		if (!extras[n] && n == 0)
			fprintf(stderr, "son: coeur \"%s\" -- les gains par groupe, la sonde d'energie et le journal"
			                " des key-on ne s'appliquent pas (soundboard.h)\n", nom);
		// LE MIROIR MADRS SE POSE AVANT LA TRACE, et l'ordre est la lecon d'une divergence : c'est le seul
		// des quatre extras qui change le son, il change par jeu (soundboard.h) et il ne passe pas par
		// scsp::core. Pose APRES l'enregistreur -- comme il l'etait -- il n'entrait pas dans l'en-tete de la
		// trace, un rejeu le supposait `true`, et sur Fighting Vipers 2 ou il vaut faux le rejeu divergeait
		// de la carte a l'echantillon 772 067 (docs/scsp_validation.md §3 c). Pose ici, l'enregistreur le
		// RELIT de la puce : il n'y a plus rien a penser a lui dire.
		{
			const char *sep = getenv("VF3_SCSP_MADRS_SEPARATE"), *mir = getenv("VF3_SCSP_MADRS_MIRROR");
			const bool on = (sep && *sep == '1') ? false : (mir && *mir == '1') ? true : madrs_mirror;
			// Les deux coeurs le portent, chacun sur sa classe concrete, par la porte unique de
			// scsp_select.h -- l'interface ne le porte pas, parce que ce n'est pas l'affaire de la puce
			// mais du jeu (scsp_core.h).
			if (!scsp::regle_miroir_madrs(*scsp[n], on) && n == 0)
				fprintf(stderr, "son: coeur \"%s\" -- le miroir MADRS ne s'y pose pas (scsp_select.h)\n", nom);
		}
		// VF3_SCSP_TRACE=f : la trace d'acces, un fichier par puce (f.0 et f.1), pour tools/son/rejouer.
		// Elle commence ICI, a la remise a zero, parce que la RAM y est connue -- huit mots, les vecteurs du
		// 68000 que la carte vient d'y copier. L'enregistreur le verifie au lieu de le supposer.
		tete[n] = scsp[n].get();
		trace[n].reset();
		if (const char *base = getenv("VF3_SCSP_TRACE")) {
			const std::string f = std::string(base) + "." + std::to_string(n);
			auto e = std::make_unique<scsp::enregistreur>(*scsp[n], f.c_str(), unsigned(n), 45158400 / 2,
			                                             ram[n].data(), uint32_t(ram[n].size() * 2));
			if (e->bon()) { tete[n] = e.get(); trace[n] = std::move(e); }
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
	return tete[n]->read_reg(i);   // (c'etait un ->device().read : l'interface suffit)
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
	tete[0]->midi_in(byte);
}

void SoundBoard::fire_due()
{
	for (int n = 0; n < 2; n++)
		for (int t = 0; t < scsp_device::N_TIMERS; t++) {
			Timer &tm = timers[n][t];
			if (tm.on && tm.at <= now) {
				if (tm.period) tm.at += tm.period;
				else tm.on = false;
				tete[n]->timer_fired(t);
			}
		}
}

// Ce que la carte fait des trois evenements de la puce. L'ancien cablage etait cinq std::function posees a
// la construction ; voici les memes gestes, nommes et non plus anonymes.
void SoundBoard::Sink::irq(int level, bool on)
{
	if (n != 0) return;   // seule la premiere puce est reliee aux lignes du 68000
	b->cpu->set_input_line(level, on ? ASSERT_LINE : CLEAR_LINE);
}

void SoundBoard::Sink::midi_out(std::uint8_t)
{
	// La carte n'en fait rien -- mais le pilote s'en sert en TROIS endroits (un 0xF0 tous les dix tics, une
	// recopie par piste, un MIDI thru : docs/fv2-son-programme.md), donc on COMPTE plutot que de taire.
	++b->midi_out_jetes;
}

void SoundBoard::Sink::timer(int which, std::uint32_t samples, std::uint32_t period)
{
	Timer &tm = b->timers[n][which];
	if (samples == 0) { tm.on = false; return; }
	// L'interface compte en ECHANTILLONS, la carte en cycles du 68000 : 11 289 600 / 44 100 = 256 tout rond,
	// donc la conversion est exacte pour les timers A-C, que l'ancien code arrondissait de toute facon au
	// multiple de 256 juste en dessous.
	//
	// ET C'EST LE SECOND ENDROIT OU LE DECOUPAGE DU §2 NE TIENT PAS TOUT A FAIT : les horloges du recepteur
	// et de l'emetteur MIDI (TIMER_RCV, TIMER_TRA) ne sont PAS quantifiees a 256 par l'ancien code, elles
	// gardent la precision du cycle. Les passer en echantillons la perd. Ce que ca change s'entend ou ne
	// s'entend pas, et c'est au harnais differentiel de le dire -- pas a moi de le supposer.
	tm.on = true;
	tm.at = b->cycles_now() + std::max<std::uint64_t>(1, std::uint64_t(samples) * 256);
	if (which < scsp_device::TIMER_RCV) tm.at = std::max(tm.at / 256 * 256, b->cycles_now() + 1);
	tm.period = period ? std::max<std::uint64_t>(1, std::uint64_t(period) * 256) : 0;
}

// Remet les deux puces a l'heure du 68000, en poussant ce qu'elles doivent. Voir soundboard.h : sans cet
// appel avant chaque acces registre, les ecritures ne tomberaient plus au meme endroit dans le son.
void SoundBoard::sync_sound(int n)
{
	if (!scsp[n]) return;
	// A l'horloge `cible` il doit exister cible + 1 echantillons : l'ancien coeur compte INCLUSIVEMENT,
	// l'echantillon d'indice 0 compris (scsp_mame.cpp, sample_index). Sans ce +1 le tout premier echantillon
	// de la partie n'etait jamais fabrique -- une paire sur 3 219 865, que seul le harnais a vue.
	const uint64_t veut = cycles_now() / 256 + 1;
	const uint64_t fait = tete[n]->samples_made();
	if (veut <= fait) return;
	const uint32_t k = uint32_t(veut - fait);
	const size_t base = pcm[n].size();
	pcm[n].resize(base + size_t(k) * 2);
	tete[n]->run(k, pcm[n].data() + base);
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
	// Les echantillons accumules au fil de la trame par sync_sound(), en entiers a l'echelle 18 bits.
	// L'ancien chemin lisait des flottants de la puce ; la reconversion est EXACTE (scsp_mame.h, note 2),
	// donc diviser ici par PLEINE_ECHELLE rend exactement ce que l'ancien chemin rendait.
	std::vector<std::int32_t> &a = pcm[0], &b = pcm[1];
	const size_t n = std::min(a.size(), b.size());
	constexpr double ECHELLE = 1.0 / scsp_mame::PLEINE_ECHELLE;
	// MVOL, LE VOLUME MAITRE, SE LIT DANS LE REGISTRE : la puce ne l'applique pas elle-meme, elle l'expose
	// seulement (scsp_contract.h §13 point 7), et un registre se lit par l'interface. Ce ->device()-la etait
	// donc une dette gratuite.
	auto mvol = [&](int n) { return float(tete[n]->read_reg(0x400 / 2) & 0x000F) / 15.0f; };
	float ga = mvol(0), gb = mvol(1);
	static const int solo = getenv("VF3_SCSP_SOLO") ? atoi(getenv("VF3_SCSP_SOLO")) : -1;   // (tests: one SCSP alone)
	if (solo == 0) gb = 0;
	if (solo == 1) ga = 0;
	// above the board's own level (gain 1), a peak limiter keeps the loudest moments under -1 dBFS: down at
	// once, back up over about 0.1 s (as VF2's)
	constexpr double CEILING = 0.891, RELEASE = 1.0 / (0.1 * 44100);
	sndprof::Scope prof_mix(sndprof::MIX);
	for (size_t i = 0; i + 1 < n; i += 2) {
		double l = (double(a[i]) * ECHELLE * ga + double(b[i]) * ECHELLE * gb) * gain,
		       r = (double(a[i + 1]) * ECHELLE * ga + double(b[i + 1]) * ECHELLE * gb) * gain;
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
	// VF3_SCSP_RESTE=N : ce que les deux puces laissent derriere elles, toutes les N images. La carte ne
	// melange que min(puce 0, puce 1) : si l'une est en avance d'un echantillon a la fin d'une trame, cet
	// echantillon attend la suivante. C'est normal ; ce qui ne l'est pas serait un reste qui GRANDIT.
	if (static const unsigned long tous = getenv("VF3_SCSP_RESTE")
	                                    ? strtoul(getenv("VF3_SCSP_RESTE"), nullptr, 10) : 0;
	    tous && frames % tous == 0)
		fprintf(stderr, "reste image %llu : puce 0 %zu paires, puce 1 %zu paires (melangees %zu)\n",
		        (unsigned long long)frames, a.size() / 2, b.size() / 2, n / 2);
	// (les deux restes, s'il y en a, repartent avec la trame suivante : c'est ce que l'ancien chemin faisait
	// de m_out, et le decalage d'un echantillon entre les deux puces ne doit pas se perdre)
}
