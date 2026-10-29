// test_scsp_contract: checks src/sound/scsp_contract.h against the two things it claims to describe.
//
// A contract nobody checks drifts, and a drifted contract is worse than none: the next person writes an
// autonomous SCSP against numbers that stopped being true. So this test reads the contract's two sources
// back and compares:
//
//   PART 1 -- every field against the macros in src/sound/scsp.cpp. The implementation is the authority on
//   where a bit lives today; the contract only records it. The comparison is on the (word, shift, mask)
//   triple, in exactly the form the macro writes it.
//
//   PART 2 -- every driver constant against the sound program ROM: the ten words the driver copies into
//   0x418-0x42A, the timer value snd_irq2 reloads, and the 192-word pitch table.
//
// AND IT FAILS LOUDLY WHEN IT CANNOT CHECK. A field whose macro is not found is an ERROR, not a skip; the
// aliases and the hand-checked specials are listed explicitly and counted, so the total of
// matched + aliased + special must equal the number of fields. The lesson of this project's bad days is
// that a filter which silently drops lines looks exactly like a test that passes.
//
// PART 2 is skipped, with a visible line, when build/fv2-son-68000.bin is absent -- it is extracted from a
// ROM the repository does not carry (tools/sound/extract-sound.py). PART 1 always runs.
#include "scsp_contract.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {

int failures = 0;

void ok(const char *what, const char *detail)
{
	printf("  ok    %-34s %s\n", what, detail);
}

void rate(const char *what, const char *detail)
{
	printf("  ECHEC %-34s %s\n", what, detail);
	++failures;
}

std::string read(const char *chemin)
{
	FILE *f = fopen(chemin, "rb");
	if (!f) return {};
	std::string s;
	char buffer[65536];
	size_t n;
	while ((n = fread(buffer, 1, sizeof buffer, f)) > 0) s.append(buffer, n);
	fclose(f);
	return s;
}

// The implementation spells two fields differently from the datasheet name the contract uses.
struct Alias { const char *contrat; const char *source; };
constexpr Alias ALIAS[] = {
	{ "KYONEX", "KEYONEX" },
	{ "KYONB",  "KEYONB"  },
	{ "MEM4B",  "MEM4B"   },   // same, listed so the table is the whole story
};

// Fields the implementation does not express as one (shift, mask) macro, checked by hand below.
constexpr const char *SPECIAUX[] = { "SA_HI", "SA_LO", "LSA", "LEA" };

bool est_special(const char *name)
{
	for (const char *s : SPECIAUX)
		if (!strcmp(s, name)) return true;
	return false;
}

const char *nom_source(const char *contrat)
{
	for (const Alias &a : ALIAS)
		if (!strcmp(a.contrat, contrat)) return a.source;
	return contrat;
}

// Finds `#define NOM(...) ((...data[0xN] >> 0xS) & 0xMMMM)` and returns the triple.
bool macro(const std::string &src, const char *name, unsigned &word, unsigned &dec, unsigned &mask)
{
	const std::string clef = std::string("#define ") + name;
	for (size_t p = src.find(clef); p != std::string::npos; p = src.find(clef, p + 1)) {
		// the name must end here, so TL does not match TLnnn
		const char apres = src[p + clef.size()];
		if (apres != '(' && apres != ' ' && apres != '\t') continue;
		const size_t fin = src.find('\n', p);
		const std::string line = src.substr(p, fin - p);
		const size_t d = line.find("data[");
		if (d == std::string::npos) continue;
		unsigned m = 0, s = 0, k = 0;
		// data[0xN] or data[N], then >> 0xS, then & 0xMMMM
		if (sscanf(line.c_str() + d, "data[0x%x] >> 0x%x) & 0x%x", &m, &s, &k) == 3
		    || sscanf(line.c_str() + d, "data[%u] >> 0x%x) & 0x%x", &m, &s, &k) == 3) {
			word = m; dec = s; mask = k;
			return true;
		}
	}
	return false;
}

void partie1()
{
	printf("-- PARTIE 1 : les champs contre src/sound/scsp.cpp --\n");
	const std::string src = read("src/sound/scsp.cpp");
	if (src.empty()) {
		rate("src/sound/scsp.cpp", "introuvable ou vide -- la partie 1 ne peut pas tourner");
		return;
	}
	unsigned vus = 0, specials = 0;
	struct Bloc { const char *what; const scsp_contract::Field *c; size_t n; };
	const Bloc blocs[] = {
		{ "fente",  scsp_contract::SLOT,  sizeof scsp_contract::SLOT  / sizeof(scsp_contract::Field) },
		{ "commun", scsp_contract::COMMON, sizeof scsp_contract::COMMON / sizeof(scsp_contract::Field) },
	};
	for (const Bloc &b : blocs)
		for (size_t i = 0; i < b.n; i++) {
			const scsp_contract::Field &ch = b.c[i];
			if (est_special(ch.name)) { ++specials; continue; }
			unsigned word = 0, dec = 0, mask = 0;
			char detail[160];
			if (!macro(src, nom_source(ch.name), word, dec, mask)) {
				snprintf(detail, sizeof detail, "aucune macro %s dans l'implementation", nom_source(ch.name));
				rate(ch.name, detail);
				continue;
			}
			if (word != ch.word || dec != ch.shift_by || mask != ch.mask) {
				snprintf(detail, sizeof detail,
				         "contrat mot %x dec %x masque %04x ; implementation mot %x dec %x masque %04x",
				         ch.word, ch.shift_by, ch.mask, word, dec, mask);
				rate(ch.name, detail);
				continue;
			}
			snprintf(detail, sizeof detail, "%s mot %x, decalage %x, masque %04x", b.what, word, dec, mask);
			ok(ch.name, detail);
			++vus;
		}
	// SA, LSA and LEA are whole words in the implementation, not (shift, mask) fields.
	{
		char detail[160];
		const bool a = src.find("#define SA(slot)") != std::string::npos
		            && src.find("(slot->udata.data[0x0] & 0xF) << 16") != std::string::npos
		            && src.find("| (slot->udata.data[0x1])") != std::string::npos;
		snprintf(detail, sizeof detail, "SA = (data[0] & 0xF) << 16 | data[1], comme le contrat le decoupe");
		a ? ok("SA_HI + SA_LO", detail) : rate("SA_HI + SA_LO", "la macro SA a change de forme");
		// Tolerant au blanc : on cherche la macro puis data[0xN] sur sa ligne, et on lit N. Une
		// comparaison de chaine exacte avait echoue ici sur deux espaces, ce que ce test a attrape --
		// c'est son travail, mais la rigidite etait la mienne et pas celle de l'implementation.
		const auto mot_entier = [&src](const char *name, unsigned expected) {
			const std::string clef = std::string("#define ") + name + "(slot)";
			const size_t p = src.find(clef);
			char detail[160];
			if (p == std::string::npos) { rate(name, "macro introuvable"); return; }
			const size_t fin = src.find('\n', p);
			const std::string line = src.substr(p, fin - p);
			unsigned n = 0;
			const size_t d = line.find("data[");
			if (d == std::string::npos || sscanf(line.c_str() + d, "data[0x%x])", &n) != 1) {
				snprintf(detail, sizeof detail, "la macro n'est plus un mot entier : %s", line.c_str());
				rate(name, detail);
				return;
			}
			if (n != expected) {
				snprintf(detail, sizeof detail, "contrat data[0x%x], implementation data[0x%x]", expected, n);
				rate(name, detail);
				return;
			}
			snprintf(detail, sizeof detail, "mot entier, data[0x%x]", n);
			ok(name, detail);
		};
		mot_entier("LSA", 0x2);
		mot_entier("LEA", 0x3);
	}
	const unsigned total = unsigned(blocs[0].n + blocs[1].n);
	char detail[160];
	snprintf(detail, sizeof detail, "%u compares + %u traites a part = %u champs du contrat",
	         vus, specials, total);
	(vus + specials == total) ? ok("le compte ferme", detail)
	                          : rate("le compte ferme", detail);
}

void partie2()
{
	printf("\n-- PARTIE 2 : les constantes du pilote contre la ROM --\n");
	const std::string rom = read("build/fv2-son-68000.bin");
	if (rom.size() < 0x80000) {
		printf("  passee build/fv2-son-68000.bin absente : tools/sound/extract-sound.py la fabrique\n");
		printf("        depuis une ROM que le depot ne porte pas. La partie 1 a tourne.\n");
		return;
	}
	const auto u16 = [&rom](unsigned a) -> unsigned {
		const unsigned o = a - 0x600000;
		return unsigned(uint8_t(rom[o])) << 8 | unsigned(uint8_t(rom[o + 1]));
	};
	char detail[200];

	// Les dix mots que 0x600288 recopie dans 0x418-0x42A.
	bool tous = true;
	for (unsigned i = 0; i < 10; i++)
		if (u16(0x6002D4 + 2 * i) != scsp_contract::INIT_418_42A[i]) {
			snprintf(detail, sizeof detail, "mot %u : ROM %04x, contrat %04x",
			         i, u16(0x6002D4 + 2 * i), scsp_contract::INIT_418_42A[i]);
			rate("les dix mots de 0x6002D4", detail);
			tous = false;
			break;
		}
	if (tous) ok("les dix mots de 0x6002D4", "0x418 a 0x42A, identiques au contrat");

	// La valeur que snd_irq2 recharge a chaque tic.
	if (u16(0x60806A) == scsp_contract::TIMER_A_COMPTE)
		ok("[0x60806A], le tic rechargé", "0x00CE, soit 50 echantillons et 882 Hz");
	else {
		snprintf(detail, sizeof detail, "ROM %04x, contrat %02x", u16(0x60806A), scsp_contract::TIMER_A_COMPTE);
		rate("[0x60806A], le tic rechargé", detail);
	}

	// Les trois niveaux, recalcules depuis les SCILV du contrat.
	for (const scsp_contract::Source &s : scsp_contract::SOURCES) {
		if (!s.armed) {
			const bool clair = (scsp_contract::INIT_418_42A[3] >> s.bit & 1) == 0;
			clair ? ok(s.name, "non armee dans SCIEB, comme le contrat le dit")
			      : rate(s.name, "le contrat la dit non armee, SCIEB la trouve armee");
			continue;
		}
		const unsigned n = scsp_contract::level_of(s.bit);
		const bool armed = (scsp_contract::INIT_418_42A[3] >> s.bit & 1) != 0;
		if (n == s.level && armed) {
			snprintf(detail, sizeof detail, "bit %u, armee, niveau %u", s.bit, n);
			ok(s.name, detail);
		} else {
			snprintf(detail, sizeof detail, "bit %u : armee %d, niveau calcule %u, contrat %u",
			         s.bit, int(armed), n, s.level);
			rate(s.name, detail);
		}
	}

	// La table de hauteur : 192 mots croissants, puis le bouclage.
	{
		const unsigned base = scsp_contract::FNS_ROM_TABLE, n = scsp_contract::MOTS_PAR_OCTAVE;
		bool croissants = true;
		unsigned maxi = 0;
		for (unsigned k = 0; k + 1 < n; k++) {
			if (u16(base + 2 * k) > u16(base + 2 * (k + 1))) { croissants = false; break; }
			if (u16(base + 2 * k) > maxi) maxi = u16(base + 2 * k);
		}
		if (u16(base + 2 * (n - 1)) > maxi) maxi = u16(base + 2 * (n - 1));
		const bool loop = u16(base + 2 * n) < u16(base + 2 * (n - 1));
		const bool tient  = maxi <= 0x3FF;
		if (croissants && loop && tient) {
			snprintf(detail, sizeof detail,
			         "%u mots croissants jusqu'a %04x, et le %u-ieme reboucle : une octave",
			         n, maxi, n);
			ok("la table de 0x603D62", detail);
		} else {
			snprintf(detail, sizeof detail, "croissants %d, bouclage %d, tient dans FNS %d (max %04x)",
			         int(croissants), int(loop), int(tient), maxi);
			rate("la table de 0x603D62", detail);
		}
	}
}

} // namespace

int main()
{
	printf("test_scsp_contract : src/sound/scsp_contract.h contre l'implementation et la ROM\n\n");
	partie1();
	partie2();
	printf("\n");
	if (failures) {
		printf("%d echec(s). Le contrat et ce qu'il decrit ont divergé : corriger l'un ou l'autre,\n", failures);
		printf("et pas le test.\n");
		return 1;
	}
	printf("all cases pass\n");
	return 0;
}
