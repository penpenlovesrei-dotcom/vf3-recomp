#include "scsp_trace.h"

#include "scsp_select.h"

#include <cinttypes>
#include <cstring>

namespace scsp {

namespace {

// Beyond this, the trace does not start at a reset and the initial sweep would be a RAM dump.
constexpr unsigned MOTS_NON_NULS_MAX = 4096;

} // namespace

enregistreur::enregistreur(core &dessous, const char *fichier, unsigned puce, u32 horloge,
                           const u16 *ram, u32 ram_octets)
	: m_dessous(dessous)
{
	m_f = fopen(fichier, "w");
	if (!m_f) {
		fprintf(stderr, "scsp_trace: cannot write %s; the trace is abandoned\n", fichier);
		return;
	}
	// The initial sweep, and its guard: a trace must start at a reset, otherwise the RAM a replay would
	// rebuild is not the one the chip had.
	const u32 mots = ram_octets / 2;
	unsigned n = 0;
	for (u32 i = 0; i < mots; i++) if (ram[i]) n++;
	if (n > MOTS_NON_NULS_MAX) {
		fprintf(stderr, "scsp_trace: %u non-zero RAM words at the start (more than %u): this trace does not"
		                " begin at a reset, and a replay would not have the right RAM. Abandoned.\n",
		        n, MOTS_NON_NULS_MAX);
		fclose(m_f);
		m_f = nullptr;
		return;
	}
	// THE MADRS MIRROR IN THE HEADER, and it is the only chip state that does not arrive through a call:
	// it is set before the trace starts, it CHANGES THE SOUND, and a replay that assumed it diverged from
	// the board with no way to attribute it (scsp_select.h). It is READ BACK from the chip rather than
	// taken as a parameter: that way there is nothing to remember to tell the recorder, which is the point.
	bool porte = false;
	const bool miroir = miroir_madrs(m_dessous, &porte);
	if (!porte)
		fprintf(stderr, "scsp_trace: this core does not carry the MADRS mirror; the trace records the default"
		                " (true), which the replay will therefore set as well\n");
	fprintf(m_f, "# scsp-trace 2 puce=%u horloge=%u ram=%u miroir=%u\n", puce, horloge, ram_octets,
	        unsigned(miroir));
	for (u32 i = 0; i < mots; i++)
		if (ram[i]) fprintf(m_f, "d %x %x ffff\n", i, ram[i]);

	// The PCM alongside: enough to prove the trace is complete (scsp_trace.h). Its absence does not stop
	// the recording -- a trace without a reference is still replayable, it is just less demonstrable.
	const std::string pcm = std::string(fichier) + ".pcm";
	m_pcm = fopen(pcm.c_str(), "wb");
	if (!m_pcm)
		fprintf(stderr, "scsp_trace: %s does not open; the trace will have no PCM reference\n", pcm.c_str());
}

enregistreur::~enregistreur()
{
	if (m_f) fclose(m_f);
	if (m_pcm) fclose(m_pcm);
}

void enregistreur::ram_ecrite(u32 mot, u16 data, u16 masque)
{
	if (m_f) fprintf(m_f, "d %x %x %x\n", mot, data, masque);
}

void enregistreur::reset()
{
	// No line: a reset mid-trace would change the RAM under the replay without its knowing. In any case
	// the board only rebuilds its cores at SoundBoard::reset, where the trace starts over.
	m_dessous.reset();
}

u16 enregistreur::read_reg(u32 mot)
{
	if (m_f) fprintf(m_f, "r %x\n", mot);
	return m_dessous.read_reg(mot);
}

void enregistreur::write_reg(u32 mot, u16 data, u16 masque)
{
	if (m_f) fprintf(m_f, "w %x %x %x\n", mot, data, masque);
	m_dessous.write_reg(mot, data, masque);
}

void enregistreur::run(u32 n, s32 *out)
{
	if (m_f) fprintf(m_f, "s %u\n", n);
	m_dessous.run(n, out);
	// AFTER the call, because what is kept is the result and not the request.
	if (m_pcm && n) fwrite(out, sizeof(s32), std::size_t(n) * 2, m_pcm);
}

void enregistreur::timer_fired(int lequel)
{
	if (m_f) fprintf(m_f, "t %d\n", lequel);
	m_dessous.timer_fired(lequel);
}

void enregistreur::midi_in(u8 octet)
{
	if (m_f) fprintf(m_f, "m %x\n", octet);
	m_dessous.midi_in(octet);
}

bool rejoue(const char *fichier, core &c, std::vector<u16> &ram, sortie &s, std::string *erreur)
{
	auto dis = [erreur](const std::string &quoi) { if (erreur) *erreur = quoi; return false; };

	FILE *f = fopen(fichier, "r");
	if (!f) return dis(std::string("trace not found: ") + fichier);

	char ligne[256];
	if (!fgets(ligne, sizeof ligne, f)) { fclose(f); return dis("empty trace"); }
	unsigned puce = 0, horloge = 0, ram_octets = 0, version = 0, miroir = 1;
	const int champs = sscanf(ligne, "# scsp-trace %u puce=%u horloge=%u ram=%u miroir=%u",
	                          &version, &puce, &horloge, &ram_octets, &miroir);
	if (champs == 4 && version == 1) {
		fclose(f);
		// REFUSED, NOT GUESSED AT: a version 1 trace does not state the MADRS mirror, which changes the
		// sound. Assuming it is precisely what made a replay diverge with no way to say whether it was the
		// core or the trace. A clear failure beats a false measurement.
		return dis("version 1 trace: it does not state the MADRS mirror, which changes the sound"
		           " (scsp_select.h). Record it again with VF3_SCSP_TRACE; version 2 carries it.");
	}
	if (champs != 5 || version != 2) {
		fclose(f);
		return dis("unknown header: " + std::string(ligne));
	}
	// The mirror BEFORE the first line: words 0x7C0-0x7FF do not go to the same place depending on it, and
	// the first of those writes comes early in the scene (sample 29 211, measured on 2026-10-28).
	if (!regle_miroir_madrs(c, miroir != 0)) {
		fclose(f);
		return dis("this core does not carry the MADRS mirror, which the trace asks for (scsp_select.h)");
	}
	if (ram_octets / 2 != ram.size())
		return dis("the trace says " + std::to_string(ram_octets) + " bytes of RAM, the replay has "
		           + std::to_string(ram.size() * 2)), fclose(f), false;

	// run()'s buffer, grown on demand: nothing is allocated per sample.
	std::vector<s32> tampon;
	unsigned long numero = 1;
	while (fgets(ligne, sizeof ligne, f)) {
		numero++;
		const char k = ligne[0];
		if (k == '#' || k == '\n' || k == '\0') continue;
		unsigned a = 0, b = 0, d = 0;
		switch (k) {
		case 'd':
			if (sscanf(ligne + 1, "%x %x %x", &a, &b, &d) != 3) goto illisible;
			if (a >= ram.size()) { fclose(f); return dis("line " + std::to_string(numero)
			                                             + ": RAM word out of bounds"); }
			ram[a] = u16((ram[a] & ~u16(d)) | (u16(b) & u16(d)));
			break;
		case 'w':
			if (sscanf(ligne + 1, "%x %x %x", &a, &b, &d) != 3) goto illisible;
			c.write_reg(a, u16(b), u16(d));
			break;
		case 'r': {
			if (sscanf(ligne + 1, "%x", &a) != 1) goto illisible;
			const u16 v = c.read_reg(a);
			s.lues.push_back({ c.samples_made(), a, v });
			break;
		}
		case 'm':
			if (sscanf(ligne + 1, "%x", &a) != 1) goto illisible;
			c.midi_in(u8(a));
			break;
		case 't':
			if (sscanf(ligne + 1, "%u", &a) != 1) goto illisible;
			c.timer_fired(int(a));
			break;
		case 's': {
			if (sscanf(ligne + 1, "%u", &a) != 1) goto illisible;
			if (tampon.size() < std::size_t(a) * 2) tampon.resize(std::size_t(a) * 2);
			c.run(a, tampon.data());
			s.pcm.insert(s.pcm.end(), tampon.begin(), tampon.begin() + std::ptrdiff_t(a) * 2);
			break;
		}
		default:
		illisible:
			fclose(f);
			return dis("line " + std::to_string(numero) + " unreadable: " + std::string(ligne));
		}
	}
	fclose(f);
	return true;
}

std::string compare(const sortie &a, const sortie &b)
{
	char t[256];

	// THE PCM FIRST, because it is the only stream in which every sample counts and it is the one that
	// locates all the rest: a sample index is the shared coordinate of the four other lists.
	if (a.pcm.size() != b.pcm.size()) {
		snprintf(t, sizeof t, "PCM: %zu values against %zu", a.pcm.size(), b.pcm.size());
		return t;
	}
	for (std::size_t i = 0; i < a.pcm.size(); i++)
		if (a.pcm[i] != b.pcm[i]) {
			snprintf(t, sizeof t, "PCM: sample %zu %s, %d against %d",
			         i / 2, (i & 1) ? "right" : "left", a.pcm[i], b.pcm[i]);
			return t;
		}

	// THE INTERRUPTS, which are the other half of the subject: they are how the 68000 learns the time, and
	// a single one too many or too few changes the rest of the driver's program -- hence the music, two
	// seconds later, in a way that no longer resembles its cause.
	if (a.irqs.size() != b.irqs.size()) {
		snprintf(t, sizeof t, "interrupts: %zu against %zu", a.irqs.size(), b.irqs.size());
		return t;
	}
	for (std::size_t i = 0; i < a.irqs.size(); i++) {
		const sortie::Irq &x = a.irqs[i], &y = b.irqs[i];
		if (x.ech != y.ech || x.niveau != y.niveau || x.etat != y.etat) {
			snprintf(t, sizeof t, "interrupt %zu: %s%d at sample %" PRIu64
			                      " against %s%d at %" PRIu64,
			         i, x.etat ? "+" : "-", x.niveau, x.ech, y.etat ? "+" : "-", y.niveau, y.ech);
			return t;
		}
	}

	if (a.armes.size() != b.armes.size()) {
		snprintf(t, sizeof t, "timer arms: %zu against %zu", a.armes.size(), b.armes.size());
		return t;
	}
	for (std::size_t i = 0; i < a.armes.size(); i++) {
		const sortie::Arme &x = a.armes[i], &y = b.armes[i];
		if (x.ech != y.ech || x.lequel != y.lequel || x.echantillons != y.echantillons
		    || x.periode != y.periode) {
			snprintf(t, sizeof t, "arm %zu: timer %d for %u (period %u) at sample %" PRIu64
			                      " against timer %d for %u (period %u) at %" PRIu64,
			         i, x.lequel, x.echantillons, x.periode, x.ech,
			         y.lequel, y.echantillons, y.periode, y.ech);
			return t;
		}
	}

	if (a.lues.size() != b.lues.size()) {
		snprintf(t, sizeof t, "register reads: %zu against %zu", a.lues.size(), b.lues.size());
		return t;
	}
	for (std::size_t i = 0; i < a.lues.size(); i++) {
		const sortie::Lue &x = a.lues[i], &y = b.lues[i];
		if (x.mot != y.mot || x.valeur != y.valeur) {
			snprintf(t, sizeof t, "read %zu of word %03x: %04x against %04x (sample %" PRIu64 ")",
			         i, x.mot, x.valeur, y.valeur, x.ech);
			return t;
		}
	}

	if (a.midis.size() != b.midis.size()) {
		snprintf(t, sizeof t, "outgoing MIDI bytes: %zu against %zu", a.midis.size(), b.midis.size());
		return t;
	}
	for (std::size_t i = 0; i < a.midis.size(); i++)
		if (a.midis[i].octet != b.midis[i].octet || a.midis[i].ech != b.midis[i].ech) {
			snprintf(t, sizeof t, "MIDI byte %zu: %02x at sample %" PRIu64 " against %02x at %" PRIu64,
			         i, a.midis[i].octet, a.midis[i].ech, b.midis[i].octet, b.midis[i].ech);
			return t;
		}

	return {};
}

} // namespace scsp
