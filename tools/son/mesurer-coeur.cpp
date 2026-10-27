// mesurer-coeur : ce qu'un coeur de SCSP coute, par partie, sans rien d'autre dans la mesure.
//
// POURQUOI PAS LE REJOUEUR. tools/son/rejouer compare deux coeurs a l'octet, et c'est son travail -- mais
// son temps est domine par l'analyse de 81 Mo de texte, identique dans les deux et comptee deux fois. Il
// donne donc un ECART juste et une part fausse : on y lit "5 % de plus" sans savoir de quoi.
//
// Ce banc-ci ne chronometre que `run()`, et il SEPARE les postes, parce qu'un chiffre global ne dit pas ou
// aller :
//
//     a vide         aucune voix, aucun programme DSP -- le plancher de la boucle
//     fentes         32 voix qui jouent, pas de programme DSP
//     DSP            aucune voix, un programme de 112 pas
//     les deux       ce que le jeu fait vraiment
//
// La forme du programme DSP synthetique suit celle que la sonde `dspprog` a relevee sur le jeu : 112 pas,
// et dans ces pas environ 87 lectures de COEF, 108 entrees depuis MEMS, 39 ecritures de TEMP, 23 lectures
// memoire, 10 ecritures, 5 sorties d'effets (docs/fv2-son-programme.md §4). Ce n'est pas le programme du
// jeu -- c'en est la forme, ce qui suffit pour situer un cout.
//
//   tools/son/mesurer-coeur [coeur] [echantillons]
#include "../../src/sound/scsp_contract.h"
#include "../../src/sound/scsp_select.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace {

constexpr std::uint32_t RAM_OCTETS = 0x100000;

struct Muet final : scsp::sink {
	void irq(int, bool) override {}
	void midi_out(std::uint8_t) override {}
	void timer(int, std::uint32_t, std::uint32_t) override {}
};

// Un pas de programme DSP, par ses champs (meme encodage que tools/son/test_scsp_dsp.cpp).
struct Pas {
	unsigned tra = 0, twt = 0, twa = 0, xsel = 0, ysel = 0, ira = 0, iwt = 0, iwa = 0;
	unsigned table = 0, mwt = 0, mrd = 0, ewt = 0, ewa = 0, shift = 0, zero = 0, bsel = 0;
	unsigned nofl = 0, coef = 0, masa = 0;

	void mots(std::uint16_t m[4]) const
	{
		m[0] = std::uint16_t(((tra & 0x7F) << 8) | ((twt & 1) << 7) | (twa & 0x7F));
		m[1] = std::uint16_t(((xsel & 1) << 15) | ((ysel & 3) << 13) | ((ira & 0x3F) << 6)
		                     | ((iwt & 1) << 5) | (iwa & 0x1F));
		m[2] = std::uint16_t(((table & 1) << 15) | ((mwt & 1) << 14) | ((mrd & 1) << 13)
		                     | ((ewt & 1) << 12) | ((ewa & 0xF) << 8) | ((shift & 3) << 4)
		                     | ((zero & 1) << 1) | (bsel & 1));
		m[3] = std::uint16_t(((nofl & 1) << 15) | ((coef & 0x3F) << 9) | ((masa & 0x1F) << 2));
	}
};

struct Banc {
	std::vector<std::uint16_t> ram = std::vector<std::uint16_t>(RAM_OCTETS / 2, 0);
	Muet muet;
	std::unique_ptr<scsp::core> c;

	explicit Banc(const char *nom)
	{
		c = scsp::make(nom, 45158400 / 2, ram.data(), RAM_OCTETS, muet);
		if (!c) { fprintf(stderr, "coeur inconnu : %s\n", nom); std::exit(2); }
		c->reset();
		c->write_reg(0x400 / 2, 0x0300 | 0x000F, 0xFFFF);   // MEM4MB, DAC18B, MVOL a fond
		// Une onde a 0x1000 : un carre, qui n'a aucun echantillon nul.
		for (int k = 0; k < 256; k++) ram[0x1000 / 2 + k] = std::uint16_t(k < 128 ? 12000 : -12000);
	}

	void voix(unsigned n)
	{
		for (unsigned f = 0; f < n; f++) {
			const std::uint32_t base = f * 0x10;
			std::uint16_t mot[16] = {};
			auto pose = [&](const char *nom, unsigned v) { scsp_contract::pose(mot, nom, v); };
			pose("SA_LO", 0x1000);
			pose("LSA", 0); pose("LEA", 256); pose("LPCTL", 1);
			pose("AR", 0x1C); pose("D1R", 2); pose("D2R", 1); pose("RR", 8); pose("DL", 12);
			pose("TL", 0x20); pose("DISDL", 7); pose("DIPAN", f & 0x1F);
			pose("EFSDL", f < 16 ? 4 : 0); pose("EFPAN", 0);
			pose("ISEL", f & 0xF); pose("IMXL", 4);
			pose("OCT", f & 7); pose("FNS", (f * 37) & 0x3FF);
			pose("LFOF", 20); pose("ALFOS", 3); pose("ALFOWS", f & 3);
			pose("KYONB", 1);
			for (unsigned w = 1; w < 16; w++) c->write_reg(base + w, mot[w], 0xFFFF);
			c->write_reg(base + 0, mot[0], 0xFFFF);
		}
		// KYONEX en dernier : il balaie les trente-deux fentes d'un coup.
		std::uint16_t dec[16] = {};
		scsp_contract::pose(dec, "KYONEX", 1);
		std::uint16_t mot0 = 0;
		(void) mot0;
		c->write_reg(0, std::uint16_t(c->read_reg(0) | dec[0]), 0xFFFF);
	}

	void programme_dsp()
	{
		// 112 pas, de la forme que la sonde a relevee sur le jeu.
		std::vector<Pas> pas(112);
		for (unsigned i = 0; i < 112; i++) {
			Pas &p = pas[i];
			p.ira = i % 32;                 // l'entree vient de MEMS : 108 pas sur 112
			p.xsel = 1;
			p.ysel = 1;                     // COEF : 87 pas sur 112
			p.coef = i % 64;
			p.tra = i % 128;
			if (i % 3)       p.twt = 1, p.twa = (i * 7) % 128;   // ~39 ecritures de TEMP
			if (i % 5 == 0)  p.mrd = 1, p.masa = i % 32;         // ~23 lectures memoire
			if (i % 11 == 0) p.mwt = 1, p.masa = i % 32;         // ~10 ecritures
			if (i % 22 == 0) p.ewt = 1, p.ewa = i % 16;          // ~5 sorties d'effets
			if (i % 2)       p.iwt = 1, p.iwa = i % 32;
			if (i % 3 == 0)  p.zero = 1;
			if (i % 3 == 1)  p.bsel = 1;
			p.shift = i % 4;
		}
		for (unsigned i = 0; i < 128; i++) {
			std::uint16_t m[4] = { 0, 0, 0, 0 };
			if (i < pas.size()) pas[i].mots(m);
			for (unsigned k = 0; k < 4; k++) c->write_reg((0x800 + i * 8 + k * 2) / 2, m[k], 0xFFFF);
		}
		for (unsigned i = 0; i < 64; i++) c->write_reg((0x700 + i * 2) / 2, std::uint16_t(0x2000 + i * 13), 0xFFFF);
		for (unsigned i = 0; i < 32; i++) c->write_reg((0x780 + i * 2) / 2, std::uint16_t(i * 64), 0xFFFF);
		c->write_reg(0xBF0 / 2, 0, 0xFFFF);   // le mot qui declenche Start
	}
};

// Le temps de run(), et rien d'autre. Le puits volatil empeche le compilateur de tout jeter.
double chronometre(scsp::core &c, unsigned echantillons, volatile std::int64_t &puits)
{
	constexpr unsigned TRANCHE = 256;
	std::vector<std::int32_t> tampon(TRANCHE * 2);
	const auto t0 = std::chrono::steady_clock::now();
	for (unsigned fait = 0; fait < echantillons; fait += TRANCHE) {
		const unsigned n = std::min(TRANCHE, echantillons - fait);
		c.run(n, tampon.data());
		std::int64_t s = 0;
		for (unsigned i = 0; i < n * 2; i++) s += tampon[i];
		puits += s;
	}
	return std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
}

} // namespace

int main(int argc, char **argv)
{
	const char *nom = argc > 1 ? argv[1] : "mame";
	const unsigned n = argc > 2 ? unsigned(strtoul(argv[2], nullptr, 10)) : 2000000;

	printf("mesurer-coeur : coeur \"%s\", %u echantillons par cas\n\n", nom, n);
	printf("  %-14s %10s %12s %14s\n", "cas", "secondes", "ns/ech", "%% d'un coeur");

	volatile std::int64_t puits = 0;
	struct Cas { const char *nom; unsigned voix; bool dsp; };
	const Cas cas[] = { { "a vide", 0, false }, { "fentes", 32, false },
	                    { "DSP", 0, true }, { "les deux", 32, true } };
	for (const Cas &k : cas) {
		Banc b(nom);
		if (k.dsp) b.programme_dsp();
		if (k.voix) b.voix(k.voix);
		const double s = chronometre(*b.c, n, puits);
		// La part d'un coeur : le temps mis pour produire n echantillons, contre leur duree reelle.
		const double duree = double(n) / 44100.0;
		printf("  %-14s %10.3f %12.1f %13.2f\n", k.nom, s, s * 1e9 / n, 100.0 * s / duree);
	}
	printf("\n(puits %lld, pour que rien ne soit optimise au loin)\n", (long long) puits);
	return 0;
}
