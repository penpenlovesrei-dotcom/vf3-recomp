// test_scsp_core: drives MAME's SCSP through the NEW interface, and only through it.
//
// Step 3 of docs/scsp_migration_plan.md asks for "un adaptateur qui présente l'ancien coeur à travers le
// contrat neuf. C'est le geste qui décide si le contrat tient". This is how it is decided: the test touches
// nothing but scsp::core and scsp::sink. If something the board needs cannot be said through those, this
// file cannot be written -- and that, not a number, is what it proves.
//
// AND IT WRITES ITS REGISTERS THROUGH THE CONTRACT'S OWN FIELD TABLE (src/sound/scsp_contract.h), not
// through hand-written shifts. So a wrong field position in the contract makes the sound stop coming out
// here, rather than waiting to be found in a game. The two files hold each other up.
//
// No ROM, no window, no framework: a RAM buffer, a waveform written into it by hand, one voice keyed on.
#include "../../src/sound/scsp_contract.h"
#include "../../src/sound/scsp_mame.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {

int echecs = 0;

void ok(const char *quoi, const std::string &detail)
{
	printf("  ok    %-36s %s\n", quoi, detail.c_str());
}

void rate(const char *quoi, const std::string &detail)
{
	printf("  ECHEC %-36s %s\n", quoi, detail.c_str());
	++echecs;
}

// The contract's field table, by name, so the test says "AR" and not "bits 4-0 of word 4".
const scsp_contract::Champ &champ(const char *nom)
{
	for (const scsp_contract::Champ &c : scsp_contract::FENTE)
		if (!strcmp(c.nom, nom)) return c;
	fprintf(stderr, "test_scsp_core: le contrat n'a pas de champ nomme %s\n", nom);
	std::exit(2);
}

// Places a value in a slot register word, exactly where the contract says it lives.
//
// ET LE CONTRAT MELE DEUX CONVENTIONS, parce que l'implementation le fait : la plupart des champs sont
// "(mot >> decalage) & masque", une valeur a decaler ; mais les bits uniques sont ecrits "(mot >> 0) &
// 0x1000", c'est-a-dire LE BIT EN PLACE, masque sans decalage. KYONEX, KYONB, PCM8B, EGHOLD, LPSLNK,
// LFORE, STWINH et SDIR sont de ceux-la.
//
// Un `pose` qui decalerait aveuglement ecrit ZERO pour ces champs-la -- c'est ce qui est arrive la premiere
// fois que ce test a tourne : la voix restait muette et le message accusait le contrat, alors que le defaut
// etait ici. On distingue les deux par la forme du masque : un masque de bits bas contigus (1, 3, 7, 0x1F...)
// est une valeur, tout le reste est un bit en place.
void pose(std::uint16_t mot[16], const char *nom, unsigned valeur)
{
	const scsp_contract::Champ &c = champ(nom);
	const bool bit_en_place = (c.masque & (c.masque + 1)) != 0;
	if (bit_en_place) {
		if (valeur) mot[c.mot] = std::uint16_t(mot[c.mot] | c.masque);
		else        mot[c.mot] = std::uint16_t(mot[c.mot] & ~c.masque);
		return;
	}
	mot[c.mot] = std::uint16_t((mot[c.mot] & ~(c.masque << c.decalage))
	                           | ((valeur & c.masque) << c.decalage));
}

// What the board plugs underneath, remembering what it was told.
struct Mouchard final : scsp::sink {
	int dernier_niveau = -1;
	int irqs = 0;
	std::uint64_t octets_midi = 0;
	struct Arme { int which; std::uint32_t samples, period; };
	std::vector<Arme> timers;

	void irq(int level, bool on) override { dernier_niveau = on ? level : -1; ++irqs; }
	void midi_out(std::uint8_t) override { ++octets_midi; }
	void timer(int which, std::uint32_t samples, std::uint32_t period) override
	{
		timers.push_back({ which, samples, period });
	}
};

constexpr std::uint32_t RAM_OCTETS = 0x100000;   // 1 Mo, comme une puce de la carte

} // namespace

int main()
{
	printf("test_scsp_core : le coeur de MAME pilote UNIQUEMENT par scsp::core\n\n");

	std::vector<std::uint16_t> ram(RAM_OCTETS / 2, 0);
	// Une onde d'un cycle a 0x2000, comme la banque que le pilote y charge (docs/fv2-son-programme.md
	// §4 septies) : 128 mots de sinusoide, pleine echelle.
	constexpr std::uint32_t ONDE = 0x2000;
	// Un CARRE et non une sinusoide, et c'est delibere : une sinusoide commence a zero, donc une fente qui
	// ne lirait que son premier echantillon rendrait du silence sans que rien ne soit casse -- un faux
	// negatif qui ferait accuser le contrat. Un carre a pleine echelle n'a aucun echantillon nul.
	for (int k = 0; k < 128; k++)
		ram[(ONDE / 2) + k] = std::uint16_t(k < 64 ? 20000 : -20000);

	Mouchard m;
	scsp_mame coeur(45158400 / 2, ram.data(), RAM_OCTETS, m);
	coeur.reset();
	// Le registre commun 0x400, que le pilote ecrit au demarrage (0x300 : MEM4MB et DAC18B) et dont MVOL
	// sort a zero. Sans lui rien ne garantit que la puce soit en etat de jouer, et un test qui ne le pose
	// pas mesure autre chose que ce qu'il croit.
	coeur.write_reg(0x400 / 2, 0x0300 | 0x000F, 0xFFFF);

	// ---- 1. le temps est POUSSE, et il avance
	{
		std::vector<std::int32_t> out(2 * 64, 0x7FFFFFFF);
		coeur.run(64, out.data());
		const bool n = coeur.samples_made() == 64;
		const bool rempli = out[0] != 0x7FFFFFFF && out[127] != 0x7FFFFFFF;
		(n && rempli) ? ok("run(64) pousse 64 paires",
		                   "samples_made() = " + std::to_string(coeur.samples_made()) + ", le tampon est ecrit")
		              : rate("run(64) pousse 64 paires",
		                     "samples_made() = " + std::to_string(coeur.samples_made())
		                     + (rempli ? "" : ", et le tampon n'a pas ete entierement ecrit"));
	}

	// ---- 2. le silence est du silence, et pas "ce qui trainait dans le tampon"
	{
		std::vector<std::int32_t> out(2 * 64, 1);
		coeur.run(64, out.data());
		bool muet = true;
		for (std::int32_t v : out) if (v != 0) { muet = false; break; }
		muet ? ok("aucune voix : silence exact", "les 128 valeurs sont nulles")
		     : rate("aucune voix : silence exact", "le tampon n'est pas nul alors qu'aucune voix ne joue");
	}

	// ---- 3. un timer arme passe par le sink, EN ECHANTILLONS
	{
		m.timers.clear();
		coeur.write_reg(scsp_contract::TIMA / 2, scsp_contract::TIMER_A_COMPTE, 0xFFFF);
		const std::uint32_t attendu = scsp_device::timer_samples(scsp_contract::TIMER_A_COMPTE);
		bool trouve = false;
		std::string detail;
		for (const Mouchard::Arme &a : m.timers)
			if (a.which == scsp_device::TIMER_A) {
				trouve = a.samples == attendu;
				detail = "which = A, samples = " + std::to_string(a.samples)
				       + " (attendu " + std::to_string(attendu) + ")";
			}
		trouve ? ok("TIMA arme le timer A en echantillons", detail)
		       : rate("TIMA arme le timer A en echantillons",
		              detail.empty() ? "le sink n'a rien recu pour le timer A" : detail);
	}

	// ---- 4. UNE VOIX, dont chaque registre est pose par le contrat
	{
		std::uint16_t mot[16] = {};
		pose(mot, "SA_HI", ONDE >> 16);
		pose(mot, "SA_LO", ONDE & 0xFFFF);
		pose(mot, "LSA",   0);
		pose(mot, "LEA",   127);
		pose(mot, "LPCTL", 1);          // boucle
		pose(mot, "AR",    0x1F);       // attaque immediate
		pose(mot, "D1R",   0);
		pose(mot, "D2R",   0);
		pose(mot, "RR",    0);
		pose(mot, "DL",    0);
		pose(mot, "TL",    0);          // pas d'attenuation
		pose(mot, "DISDL", 7);          // plein niveau en sortie directe
		pose(mot, "DIPAN", 0);          // centre
		pose(mot, "OCT",   0);
		pose(mot, "FNS",   0);
		pose(mot, "KYONB", 1);
		for (unsigned w = 0; w < 16; w++)
			if (mot[w]) coeur.write_reg(w, mot[w], 0xFFFF);
		// KYONEX, le declencheur, en dernier et seul
		std::uint16_t dec[16] = {};
		pose(dec, "KYONEX", 1);
		pose(dec, "KYONB", 1);
		coeur.write_reg(0, std::uint16_t(mot[0] | dec[0]), 0xFFFF);

		std::vector<std::int32_t> out(2 * 512, 0);
		coeur.device().m_probe = true;   // (diagnostic : l'energie que la fente a reellement envoyee)
		coeur.run(512, out.data());
		printf("        [diag] energie directe fente 0 = %g ; par etat EG du groupe 0 = %g %g %g %g\n",
		       coeur.device().m_probe_direct[0],
		       coeur.device().m_probe_state[0][0], coeur.device().m_probe_state[0][1],
		       coeur.device().m_probe_state[0][2], coeur.device().m_probe_state[0][3]);
		printf("        [diag] groupe de la fente 0 = %d\n", coeur.device().slot_group(0));
		std::int32_t crete = 0;
		for (std::int32_t v : out) crete = std::max(crete, std::abs(v));
		(crete > 1000)
			? ok("une voix keyee SONNE, par le contrat seul",
			     "crete " + std::to_string(crete) + " sur " + std::to_string(scsp_mame::PLEINE_ECHELLE))
			: rate("une voix keyee SONNE, par le contrat seul",
			       "crete " + std::to_string(crete) + " : rien n'est sorti. Un champ du contrat est faux,"
			       " ou l'adaptateur ne transmet pas");
	}

	// ---- 5. ce que l'adaptateur ne sait pas faire, dit plutot que taise
	{
		const std::uint64_t perdus = coeur.midi_out_bits_dropped();
		ok("les bits MIDI non assembles sont comptes",
		   std::to_string(perdus) + " bits laisses tomber (l'ancien coeur emet des bits, le contrat"
		   " demande des octets -- scsp_mame.h note 3)");
	}

	printf("\n");
	if (echecs) {
		printf("%d echec(s).\n", echecs);
		return 1;
	}
	printf("tous les cas passent\n");
	return 0;
}
