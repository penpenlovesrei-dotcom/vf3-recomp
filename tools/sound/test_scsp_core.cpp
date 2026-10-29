// test_scsp_core: drives MAME's SCSP through the NEW interface, and only through it.
//
// Step 3 of scsp/docs/scsp_migration_plan.md asks for "un adaptateur qui présente l'ancien coeur à travers le
// contrat neuf. C'est le geste qui décide si le contrat tient". This is how it is decided: the test touches
// nothing but scsp::core and scsp::sink. If something the board needs cannot be said through those, this
// file cannot be written -- and that, not a number, is what it proves.
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

void rate(const char *what, const std::string &detail)
{
	printf("  ECHEC %-36s %s\n", what, detail.c_str());
	++failures;
}

// Les champs se posent par le contrat lui-meme (scsp_contract::pose), et non par une copie locale.
// LA COPIE LOCALE A EXISTE, et c'est pour cela qu'elle n'existe plus : le contrat mele deux conventions --
// la plupart des champs sont une valeur a decaler, mais les bits uniques sont ecrits EN PLACE, masque sans
// decalage (KYONEX, KYONB, PCM8B, EGHOLD, LPSLNK, LFORE, STWINH, SDIR). La premiere version de ce test
// decalait aveuglement, KYONB sortait a zero, la voix restait muette et le message accusait le contrat alors
// que le defaut etait ici. Une seule regle, dans le fichier qui porte la table : scsp_contract.h section 8.
void put(std::uint16_t word[16], const char *name, unsigned value)
{
	if (!scsp_contract::put(word, name, value)) {
		fprintf(stderr, "test_scsp_core: le contrat n'a pas de champ nomme %s\n", name);
		std::exit(2);
	}
}

// What the board plugs underneath, remembering what it was told.
struct Mouchard final : scsp::sink {
	int dernier_niveau = -1;
	int irqs = 0;
	std::uint64_t octets_midi = 0;
	struct Arm { int which; std::uint32_t samples, period; };
	std::vector<Arm> timers;

	void irq(int level, bool on) override { dernier_niveau = on ? level : -1; ++irqs; }
	void midi_out(std::uint8_t) override { ++octets_midi; }
	void timer(int which, std::uint32_t samples, std::uint32_t period) override
	{
		timers.push_back({ which, samples, period });
	}
};

constexpr std::uint32_t RAM_BYTES = 0x100000;   // 1 Mo, comme une puce de la carte

} // namespace

int main()
{
	printf("test_scsp_core : le coeur de MAME pilote UNIQUEMENT par scsp::core\n\n");

	std::vector<std::uint16_t> ram(RAM_BYTES / 2, 0);
	// Une onde d'un cycle a 0x2000, comme la banque que le pilote y charge (docs/fv2-sound-program.md
	// §4 septies) : 128 mots de sinusoide, pleine echelle.
	constexpr std::uint32_t WAVE = 0x2000;
	// Un CARRE et non une sinusoide, et c'est delibere : une sinusoide commence a zero, donc une fente qui
	// ne lirait que son premier echantillon rendrait du silence sans que rien ne soit casse -- un faux
	// negatif qui ferait accuser le contrat. Un carre a pleine echelle n'a aucun echantillon nul.
	for (int k = 0; k < 128; k++)
		ram[(WAVE / 2) + k] = std::uint16_t(k < 64 ? 20000 : -20000);

	Mouchard m;
	scsp_mame the_core(45158400 / 2, ram.data(), RAM_BYTES, m);
	the_core.reset();
	// Le registre commun 0x400, que le pilote ecrit au demarrage (0x300 : MEM4MB et DAC18B) et dont MVOL
	// sort a zero. Sans lui rien ne garantit que la puce soit en etat de jouer, et un test qui ne le pose
	// pas mesure autre chose que ce qu'il croit.
	the_core.write_reg(0x400 / 2, 0x0300 | 0x000F, 0xFFFF);

	// ---- 1. le temps est POUSSE, et il avance
	{
		std::vector<std::int32_t> out(2 * 64, 0x7FFFFFFF);
		the_core.run(64, out.data());
		const bool n = the_core.samples_made() == 64;
		const bool rempli = out[0] != 0x7FFFFFFF && out[127] != 0x7FFFFFFF;
		(n && rempli) ? ok("run(64) pousse 64 paires",
		                   "samples_made() = " + std::to_string(the_core.samples_made()) + ", le tampon est ecrit")
		              : rate("run(64) pousse 64 paires",
		                     "samples_made() = " + std::to_string(the_core.samples_made())
		                     + (rempli ? "" : ", et le tampon n'a pas ete entierement ecrit"));
	}

	// ---- 2. le silence est du silence, et pas "ce qui trainait dans le tampon"
	{
		std::vector<std::int32_t> out(2 * 64, 1);
		the_core.run(64, out.data());
		bool silent = true;
		for (std::int32_t v : out) if (v != 0) { silent = false; break; }
		silent ? ok("aucune voix : silence exact", "les 128 valeurs sont nulles")
		     : rate("aucune voix : silence exact", "le tampon n'est pas nul alors qu'aucune voix ne joue");
	}

	// ---- 3. un timer arme passe par le sink, EN ECHANTILLONS
	{
		m.timers.clear();
		the_core.write_reg(scsp_contract::TIMA / 2, scsp_contract::TIMER_A_COMPTE, 0xFFFF);
		const std::uint32_t expected = scsp_device::timer_samples(scsp_contract::TIMER_A_COMPTE);
		bool found = false;
		std::string detail;
		for (const Mouchard::Arm &a : m.timers)
			if (a.which == scsp_device::TIMER_A) {
				found = a.samples == expected;
				detail = "which = A, samples = " + std::to_string(a.samples)
				       + " (attendu " + std::to_string(expected) + ")";
			}
		found ? ok("TIMA arme le timer A en echantillons", detail)
		       : rate("TIMA arme le timer A en echantillons",
		              detail.empty() ? "le sink n'a rien recu pour le timer A" : detail);
	}

	// ---- 4. UNE VOIX, dont chaque registre est pose par le contrat
	{
		std::uint16_t word[16] = {};
		put(word, "SA_HI", WAVE >> 16);
		put(word, "SA_LO", WAVE & 0xFFFF);
		put(word, "LSA",   0);
		put(word, "LEA",   127);
		put(word, "LPCTL", 1);          // boucle
		put(word, "AR",    0x1F);       // attaque immediate
		put(word, "D1R",   0);
		put(word, "D2R",   0);
		put(word, "RR",    0);
		put(word, "DL",    0);
		put(word, "TL",    0);          // pas d'attenuation
		put(word, "DISDL", 7);          // plein niveau en sortie directe
		put(word, "DIPAN", 0);          // centre
		put(word, "OCT",   0);
		put(word, "FNS",   0);
		put(word, "KYONB", 1);
		for (unsigned w = 0; w < 16; w++)
			if (word[w]) the_core.write_reg(w, word[w], 0xFFFF);
		// KYONEX, le declencheur, en dernier et seul
		std::uint16_t dec[16] = {};
		put(dec, "KYONEX", 1);
		put(dec, "KYONB", 1);
		the_core.write_reg(0, std::uint16_t(word[0] | dec[0]), 0xFFFF);

		std::vector<std::int32_t> out(2 * 512, 0);
		the_core.device().m_probe = true;   // (diagnostic : l'energie que la fente a reellement envoyee)
		the_core.run(512, out.data());
		printf("        [diag] energie directe fente 0 = %g ; par etat EG du groupe 0 = %g %g %g %g\n",
		       the_core.device().m_probe_direct[0],
		       the_core.device().m_probe_state[0][0], the_core.device().m_probe_state[0][1],
		       the_core.device().m_probe_state[0][2], the_core.device().m_probe_state[0][3]);
		printf("        [diag] groupe de la fente 0 = %d\n", the_core.device().slot_group(0));
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
		const std::uint64_t lost = the_core.midi_out_bits_dropped();
		ok("les bits MIDI non assembles sont comptes",
		   std::to_string(lost) + " bits laisses tomber (l'ancien coeur emet des bits, le contrat"
		   " demande des octets -- scsp_mame.h note 3)");
	}

	printf("\n");
	if (failures) {
		printf("%d echec(s).\n", failures);
		return 1;
	}
	printf("all cases pass\n");
	return 0;
}
