#include "scsp_trace.h"

#include "scsp_select.h"

#include <cinttypes>
#include <cstring>

namespace scsp {

namespace {

// Au-dela, la trace ne commence pas a une remise a zero et le balayage initial serait un vidage de RAM.
constexpr unsigned MOTS_NON_NULS_MAX = 4096;

} // namespace

enregistreur::enregistreur(core &dessous, const char *fichier, unsigned puce, u32 horloge,
                           const u16 *ram, u32 ram_octets)
	: m_dessous(dessous)
{
	m_f = fopen(fichier, "w");
	if (!m_f) {
		fprintf(stderr, "scsp_trace: impossible d'ecrire %s ; la trace est abandonnee\n", fichier);
		return;
	}
	// Le balayage initial, et son garde-fou : une trace doit commencer a la remise a zero, sinon la RAM
	// qu'un rejeu reconstruirait n'est pas celle que la puce avait.
	const u32 mots = ram_octets / 2;
	unsigned n = 0;
	for (u32 i = 0; i < mots; i++) if (ram[i]) n++;
	if (n > MOTS_NON_NULS_MAX) {
		fprintf(stderr, "scsp_trace: %u mots de RAM non nuls au depart (plus de %u) : cette trace ne commence"
		                " pas a une remise a zero, et un rejeu n'aurait pas la bonne RAM. Abandonnee.\n",
		        n, MOTS_NON_NULS_MAX);
		fclose(m_f);
		m_f = nullptr;
		return;
	}
	// LE MIROIR MADRS DANS L'EN-TETE, et c'est le seul etat de la puce qui n'arrive pas par un appel : il
	// se pose avant que la trace commence, il CHANGE LE SON, et un rejeu qui le supposait divergeait de la
	// carte sans qu'on puisse l'attribuer (scsp_select.h). Il est RELU de la puce et non recu en parametre :
	// ainsi il n'y a rien a penser a dire a l'enregistreur, et c'est le point de l'affaire.
	bool porte = false;
	const bool miroir = miroir_madrs(m_dessous, &porte);
	if (!porte)
		fprintf(stderr, "scsp_trace: ce coeur ne porte pas le miroir MADRS ; la trace note le defaut (vrai),"
		                " que le rejeu posera donc aussi\n");
	fprintf(m_f, "# scsp-trace 2 puce=%u horloge=%u ram=%u miroir=%u\n", puce, horloge, ram_octets,
	        unsigned(miroir));
	for (u32 i = 0; i < mots; i++)
		if (ram[i]) fprintf(m_f, "d %x %x ffff\n", i, ram[i]);

	// Le PCM a cote : de quoi prouver que la trace est complete (scsp_trace.h). Son absence n'arrete pas
	// l'enregistrement -- une trace sans reference reste rejouable, elle est juste moins demontrable.
	const std::string pcm = std::string(fichier) + ".pcm";
	m_pcm = fopen(pcm.c_str(), "wb");
	if (!m_pcm)
		fprintf(stderr, "scsp_trace: %s ne s'ouvre pas ; la trace n'aura pas sa reference PCM\n", pcm.c_str());
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
	// Pas de ligne : un reset en cours de trace changerait la RAM sous le rejeu sans qu'il le sache. La
	// carte ne reconstruit de toute facon ses coeurs qu'a SoundBoard::reset, ou la trace recommence.
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
	// APRES l'appel, parce que c'est le resultat qu'on garde et non la demande.
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
	if (!f) return dis(std::string("trace introuvable : ") + fichier);

	char ligne[256];
	if (!fgets(ligne, sizeof ligne, f)) { fclose(f); return dis("trace vide"); }
	unsigned puce = 0, horloge = 0, ram_octets = 0, version = 0, miroir = 1;
	const int champs = sscanf(ligne, "# scsp-trace %u puce=%u horloge=%u ram=%u miroir=%u",
	                          &version, &puce, &horloge, &ram_octets, &miroir);
	if (champs == 4 && version == 1) {
		fclose(f);
		// REFUSEE, ET NON DEVINEE : une trace de version 1 ne dit pas le miroir MADRS, qui change le son.
		// Le supposer est precisement ce qui a fait diverger un rejeu sans qu'on puisse dire si c'etait le
		// coeur ou la trace. Un echec clair vaut mieux qu'une mesure fausse.
		return dis("trace de version 1 : elle ne dit pas le miroir MADRS, qui change le son (scsp_select.h)."
		           " Reenregistre-la avec VF3_SCSP_TRACE ; la version 2 le porte.");
	}
	if (champs != 5 || version != 2) {
		fclose(f);
		return dis("en-tete inconnu : " + std::string(ligne));
	}
	// Le miroir AVANT la premiere ligne : les mots 0x7C0-0x7FF ne vont pas au meme endroit selon lui, et la
	// premiere de ces ecritures arrive tot dans la scene (echantillon 29 211, mesure le 2026-10-28).
	if (!regle_miroir_madrs(c, miroir != 0)) {
		fclose(f);
		return dis("ce coeur ne porte pas le miroir MADRS, que la trace demande (scsp_select.h)");
	}
	if (ram_octets / 2 != ram.size())
		return dis("la trace dit " + std::to_string(ram_octets) + " octets de RAM, le rejeu en a "
		           + std::to_string(ram.size() * 2)), fclose(f), false;

	// Le tampon de run(), agrandi a la demande : rien n'est alloue par echantillon.
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
			if (a >= ram.size()) { fclose(f); return dis("ligne " + std::to_string(numero)
			                                             + " : mot de RAM hors des bornes"); }
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
			return dis("ligne " + std::to_string(numero) + " illisible : " + std::string(ligne));
		}
	}
	fclose(f);
	return true;
}

std::string compare(const sortie &a, const sortie &b)
{
	char t[256];

	// LE PCM D'ABORD, parce que c'est le seul flux dont chaque echantillon compte et que c'est celui qui
	// situe tout le reste : un indice d'echantillon est la coordonnee commune des quatre autres listes.
	if (a.pcm.size() != b.pcm.size()) {
		snprintf(t, sizeof t, "PCM : %zu valeurs contre %zu", a.pcm.size(), b.pcm.size());
		return t;
	}
	for (std::size_t i = 0; i < a.pcm.size(); i++)
		if (a.pcm[i] != b.pcm[i]) {
			snprintf(t, sizeof t, "PCM : echantillon %zu %s, %d contre %d",
			         i / 2, (i & 1) ? "droite" : "gauche", a.pcm[i], b.pcm[i]);
			return t;
		}

	// LES INTERRUPTIONS, qui sont l'autre moitie du sujet : c'est par elles que le 68000 apprend l'heure, et
	// une seule de trop ou de moins change la suite du programme du pilote -- donc la musique, deux secondes
	// plus tard, d'une facon qui ne ressemble plus a sa cause.
	if (a.irqs.size() != b.irqs.size()) {
		snprintf(t, sizeof t, "interruptions : %zu contre %zu", a.irqs.size(), b.irqs.size());
		return t;
	}
	for (std::size_t i = 0; i < a.irqs.size(); i++) {
		const sortie::Irq &x = a.irqs[i], &y = b.irqs[i];
		if (x.ech != y.ech || x.niveau != y.niveau || x.etat != y.etat) {
			snprintf(t, sizeof t, "interruption %zu : %s%d a l'echantillon %" PRIu64
			                      " contre %s%d a %" PRIu64,
			         i, x.etat ? "+" : "-", x.niveau, x.ech, y.etat ? "+" : "-", y.niveau, y.ech);
			return t;
		}
	}

	if (a.armes.size() != b.armes.size()) {
		snprintf(t, sizeof t, "armements de timer : %zu contre %zu", a.armes.size(), b.armes.size());
		return t;
	}
	for (std::size_t i = 0; i < a.armes.size(); i++) {
		const sortie::Arme &x = a.armes[i], &y = b.armes[i];
		if (x.ech != y.ech || x.lequel != y.lequel || x.echantillons != y.echantillons
		    || x.periode != y.periode) {
			snprintf(t, sizeof t, "armement %zu : timer %d pour %u (periode %u) a l'echantillon %" PRIu64
			                      " contre timer %d pour %u (periode %u) a %" PRIu64,
			         i, x.lequel, x.echantillons, x.periode, x.ech,
			         y.lequel, y.echantillons, y.periode, y.ech);
			return t;
		}
	}

	if (a.lues.size() != b.lues.size()) {
		snprintf(t, sizeof t, "lectures de registre : %zu contre %zu", a.lues.size(), b.lues.size());
		return t;
	}
	for (std::size_t i = 0; i < a.lues.size(); i++) {
		const sortie::Lue &x = a.lues[i], &y = b.lues[i];
		if (x.mot != y.mot || x.valeur != y.valeur) {
			snprintf(t, sizeof t, "lecture %zu du mot %03x : %04x contre %04x (echantillon %" PRIu64 ")",
			         i, x.mot, x.valeur, y.valeur, x.ech);
			return t;
		}
	}

	if (a.midis.size() != b.midis.size()) {
		snprintf(t, sizeof t, "octets MIDI sortants : %zu contre %zu", a.midis.size(), b.midis.size());
		return t;
	}
	for (std::size_t i = 0; i < a.midis.size(); i++)
		if (a.midis[i].octet != b.midis[i].octet || a.midis[i].ech != b.midis[i].ech) {
			snprintf(t, sizeof t, "octet MIDI %zu : %02x a l'echantillon %" PRIu64 " contre %02x a %" PRIu64,
			         i, a.midis[i].octet, a.midis[i].ech, b.midis[i].octet, b.midis[i].ech);
			return t;
		}

	return {};
}

} // namespace scsp
