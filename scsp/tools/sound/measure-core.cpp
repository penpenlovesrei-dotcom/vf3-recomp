// measure-core : ce qu'un coeur de SCSP coute, par partie, sans rien d'autre dans la mesure.
//
// POURQUOI PAS LE REJOUEUR. tools/sound/replay compare deux coeurs a l'octet, et c'est son travail -- mais
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
// ET UN CRAN PLUS FIN, `--dsp` : le DSP par CARACTERISTIQUE, les deux coeurs cote a cote. Le poste "DSP"
// ci-dessus est un seul nombre pour un jeu d'instructions entier, et il dit que le coeur du projet y est
// +20 % sans dire de quoi. Des programmes de 112 pas ou tous les pas ne font qu'une chose repondent a ca.
// Son en-tete est plus bas, juste avant la table.
//
//   tools/sound/measure-core [coeur] [echantillons]      les quatre postes
//   tools/sound/measure-core --dsp [echantillons]        le DSP par caracteristique, les deux coeurs
//   tools/sound/measure-core --baseline [echantillons]      le socle seul, sept passages : de quoi entrelacer
//                                                       une reference et une variante du meme fichier
//   tools/sound/measure-core --baseline <coeur> [n]         UN seul coeur, et le processus ne fait que ca :
//                                                       de quoi qu'un compteur materiel l'attribue
//   tools/sound/measure-core --game <coeur> [n]           le meme, sur le programme de la FORME DU JEU --
//                                                       le socle est artificiel, celui-ci est ce qui compte
#include "../../src/sound/scsp_contract.h"
#include "../../src/sound/scsp_select.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace {

constexpr std::uint32_t RAM_BYTES = 0x100000;

struct Silent final : scsp::sink {
	void irq(int, bool) override {}
	void midi_out(std::uint8_t) override {}
	void timer(int, std::uint32_t, std::uint32_t) override {}
};

// Un pas de programme DSP, par ses champs (meme encodage que tools/sound/test_scsp_dsp.cpp).
struct DecodedStep {
	unsigned tra = 0, twt = 0, twa = 0, xsel = 0, ysel = 0, ira = 0, iwt = 0, iwa = 0;
	unsigned table = 0, mwt = 0, mrd = 0, ewt = 0, ewa = 0, shift = 0, zero = 0, bsel = 0;
	unsigned nofl = 0, coef = 0, masa = 0;

	// Les six champs que le programme du jeu ne pose jamais, et qu'il faut pourtant pouvoir poser pour les
	// MESURER : les quatre rares (ADRL, FRCL, YRL, NEGB -- docs/scsp_audit.md §5) et les deux modificateurs
	// d'adresse. Ils valent zero partout ailleurs, donc les ajouter ne change aucun cas existant.
	unsigned adrl = 0, frcl = 0, yrl = 0, negb = 0, adreb = 0, nxadr = 0;

	void mots(std::uint16_t m[4]) const
	{
		m[0] = std::uint16_t(((tra & 0x7F) << 8) | ((twt & 1) << 7) | (twa & 0x7F));
		m[1] = std::uint16_t(((xsel & 1) << 15) | ((ysel & 3) << 13) | ((ira & 0x3F) << 6)
		                     | ((iwt & 1) << 5) | (iwa & 0x1F));
		m[2] = std::uint16_t(((table & 1) << 15) | ((mwt & 1) << 14) | ((mrd & 1) << 13)
		                     | ((ewt & 1) << 12) | ((ewa & 0xF) << 8) | ((adrl & 1) << 7)
		                     | ((frcl & 1) << 6) | ((shift & 3) << 4) | ((yrl & 1) << 3)
		                     | ((negb & 1) << 2) | ((zero & 1) << 1) | (bsel & 1));
		m[3] = std::uint16_t(((nofl & 1) << 15) | ((coef & 0x3F) << 9) | ((masa & 0x1F) << 2)
		                     | ((adreb & 1) << 1) | (nxadr & 1));
	}
};

struct Bench {
	std::vector<std::uint16_t> ram = std::vector<std::uint16_t>(RAM_BYTES / 2, 0);
	Silent silent;
	std::unique_ptr<scsp::core> c;

	explicit Bench(const char *name)
	{
		c = scsp::make(name, 45158400 / 2, ram.data(), RAM_BYTES, silent);
		if (!c) { fprintf(stderr, "coeur inconnu : %s\n", name); std::exit(2); }
		c->reset();
		c->write_reg(0x400 / 2, 0x0300 | 0x000F, 0xFFFF);   // MEM4MB, DAC18B, MVOL a fond
		// Une onde a 0x1000 : un carre, qui n'a aucun echantillon nul.
		for (int k = 0; k < 256; k++) ram[0x1000 / 2 + k] = std::uint16_t(k < 128 ? 12000 : -12000);
	}

	void voix(unsigned n)
	{
		for (unsigned f = 0; f < n; f++) {
			const std::uint32_t base = f * 0x10;
			std::uint16_t word[16] = {};
			auto put = [&](const char *name, unsigned v) { scsp_contract::put(word, name, v); };
			put("SA_LO", 0x1000);
			put("LSA", 0); put("LEA", 256); put("LPCTL", 1);
			put("AR", 0x1C); put("D1R", 2); put("D2R", 1); put("RR", 8); put("DL", 12);
			put("TL", 0x20); put("DISDL", 7); put("DIPAN", f & 0x1F);
			put("EFSDL", f < 16 ? 4 : 0); put("EFPAN", 0);
			put("ISEL", f & 0xF); put("IMXL", 4);
			put("OCT", f & 7); put("FNS", (f * 37) & 0x3FF);
			put("LFOF", 20); put("ALFOS", 3); put("ALFOWS", f & 3);
			put("KYONB", 1);
			for (unsigned w = 1; w < 16; w++) c->write_reg(base + w, word[w], 0xFFFF);
			c->write_reg(base + 0, word[0], 0xFFFF);
		}
		// KYONEX en dernier : il balaie les trente-deux fentes d'un coup.
		std::uint16_t dec[16] = {};
		scsp_contract::put(dec, "KYONEX", 1);
		std::uint16_t word0 = 0;
		(void) word0;
		c->write_reg(0, std::uint16_t(c->read_reg(0) | dec[0]), 0xFFFF);
	}

	// Pose un programme quelconque : MPRO, les 64 COEF, les 32 MADRS, puis le mot qui declenche Start.
	// Sort de programme_dsp() pour que le banc par caracteristique s'en serve aussi.
	void program(const std::vector<DecodedStep> &step)
	{
		for (unsigned i = 0; i < 128; i++) {
			std::uint16_t m[4] = { 0, 0, 0, 0 };
			if (i < step.size()) step[i].mots(m);
			for (unsigned k = 0; k < 4; k++) c->write_reg((0x800 + i * 8 + k * 2) / 2, m[k], 0xFFFF);
		}
		for (unsigned i = 0; i < 64; i++) c->write_reg((0x700 + i * 2) / 2, std::uint16_t(0x2000 + i * 13), 0xFFFF);
		for (unsigned i = 0; i < 32; i++) c->write_reg((0x780 + i * 2) / 2, std::uint16_t(i * 64), 0xFFFF);
		c->write_reg(0xBF0 / 2, 0, 0xFFFF);   // le mot qui declenche Start
	}

	void programme_dsp()
	{
		// 112 pas, de la forme que la sonde a relevee sur le jeu.
		std::vector<DecodedStep> step(112);
		for (unsigned i = 0; i < 112; i++) {
			DecodedStep &p = step[i];
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
		program(step);
	}
};

// Le temps de run(), et rien d'autre. Le puits volatil empeche le compilateur de tout jeter.
double stopwatch(scsp::core &c, unsigned samples, volatile std::int64_t &snk)
{
	constexpr unsigned TRANCHE = 256;
	std::vector<std::int32_t> buffer(TRANCHE * 2);
	const auto t0 = std::chrono::steady_clock::now();
	for (unsigned fait = 0; fait < samples; fait += TRANCHE) {
		const unsigned n = std::min(TRANCHE, samples - fait);
		c.run(n, buffer.data());
		std::int64_t s = 0;
		for (unsigned i = 0; i < n * 2; i++) s += buffer[i];
		snk += s;
	}
	return std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
}

// -----------------------------------------------------------------------------------------------------
// LE BANC PAR CARACTERISTIQUE (`--dsp`), ET POURQUOI IL EXISTE.
//
// Mesure du 2026-10-28 : le DSP du projet coute +19,8 % par pas a celui de MAME -- 11,50 contre 9,60 cycles,
// dans le jeu (docs/scsp_audit.md §4 sexies), et +20 % au banc des quatre postes ci-dessus. Les deux
// instruments s'accordent sur l'ECART et ni l'un ni l'autre ne dit DE QUOI il est fait : le poste "DSP" est
// encore un seul nombre pour un jeu d'instructions entier.
//
// Et c'est exactement l'erreur que docs/scsp_audit.md §4 quinquies raconte -- avoir specialise le DSP en
// visant le mauvais poste, faute d'un instrument qui separe. Donc un cran plus fin, et de la meme facon :
// des programmes de 112 pas ou TOUS LES PAS NE FONT QU'UNE CHOSE, chronometres sur les deux coeurs. La
// difference entre le socle et le socle + une caracteristique EST le cout de cette caracteristique, par pas.
//
// CE QUE CE BANC NE PEUT PAS DIRE, et il vaut mieux l'ecrire que le decouvrir : un programme ou les 112 pas
// font la meme chose n'a pas le melange d'instructions du jeu, donc ni ses defauts de cache ni ses
// dependances entre pas. Il SITUE un cout, il ne le predit pas. Le temoin est la derniere ligne : le
// programme de la forme du jeu, mesure par le meme chronometre -- si la somme des caracteristiques en etait
// loin, c'est le banc qu'il faudrait croire le moins.
//
// LES QUATRE RARES SONT DANS LA TABLE, et ils y repondent a une autre question : le jeu ne les pose jamais
// (§5), donc c'est le seul endroit du depot ou leur cout se lit. Chez le coeur du projet ils font en plus
// basculer dsp_own vers son second corps (`template <bool RARES>`), et la ligne le mesure donc aussi.
// -----------------------------------------------------------------------------------------------------

constexpr unsigned STEPS_PER_PROGRAM = 112;   // la longueur relevee sur le jeu (docs/scsp_audit.md §5)

struct Caracteristique {
	const char *name;
	const char *what;
	void (*put)(DecodedStep &p, unsigned i);
};

// Le socle : 112 pas qui font le moins possible. ZERO met B a zero, rien n'entre, rien ne sort.
void baseline(DecodedStep &p, unsigned) { p.zero = 1; }

const Caracteristique CHARACTERISTICS[] = {
	{ "socle", "112 pas qui ne font rien : le plancher de la boucle de pas",
	  [](DecodedStep &, unsigned) {} },
	{ "IRA -> MEMS", "l'entree depuis la fenetre memoire, 108 pas sur 112 dans le jeu",
	  [](DecodedStep &p, unsigned i) { p.ira = i % 32; p.xsel = 1; } },
	{ "COEF", "la lecture du coefficient, 87 pas sur 112 dans le jeu",
	  [](DecodedStep &p, unsigned i) { p.ysel = 1; p.coef = i % 64; } },
	{ "le produit", "B + X * Y : l'entree, le coefficient et l'accumulation ensemble",
	  [](DecodedStep &p, unsigned i) { p.ira = i % 32; p.xsel = 1; p.ysel = 1; p.coef = i % 64;
	                           p.tra = i % 128; p.bsel = 1; } },
	{ "TWT", "l'ecriture dans TEMP, 39 pas sur 112 dans le jeu",
	  [](DecodedStep &p, unsigned i) { p.twt = 1; p.twa = (i * 7) % 128; } },
	{ "IWT", "l'ecriture dans MEMS",
	  [](DecodedStep &p, unsigned i) { p.iwt = 1; p.iwa = i % 32; } },
	{ "MRD (NOFL)", "la lecture memoire SANS empaquetage, 23 pas sur 112 dans le jeu",
	  [](DecodedStep &p, unsigned i) { p.mrd = 1; p.nofl = 1; p.masa = i % 32; } },
	{ "MRD + UNPACK", "la meme, EMPAQUETEE : la difference est le cout de UNPACK",
	  [](DecodedStep &p, unsigned i) { p.mrd = 1; p.nofl = 0; p.masa = i % 32; } },
	{ "MWT (NOFL)", "l'ecriture memoire sans empaquetage, 10 pas sur 112 dans le jeu",
	  [](DecodedStep &p, unsigned i) { p.mwt = 1; p.nofl = 1; p.masa = i % 32; } },
	{ "MWT + PACK", "la meme, EMPAQUETEE : la difference est le cout de PACK",
	  [](DecodedStep &p, unsigned i) { p.mwt = 1; p.nofl = 0; p.masa = i % 32; } },
	{ "ADREB + NXADR", "les deux modificateurs d'adresse, par-dessus une lecture",
	  [](DecodedStep &p, unsigned i) { p.mrd = 1; p.nofl = 1; p.masa = i % 32; p.adreb = 1; p.nxadr = 1; } },
	{ "EWT", "la sortie vers EFREG, 5 pas sur 112 dans le jeu",
	  [](DecodedStep &p, unsigned i) { p.ewt = 1; p.ewa = i % 16; } },
	{ "SHIFT 1 (ecrete)", "le decalage qui sature",
	  [](DecodedStep &p, unsigned i) { p.ira = i % 32; p.xsel = 1; p.shift = 1; } },
	{ "SHIFT 3 (reboucle)", "le decalage qui reboucle",
	  [](DecodedStep &p, unsigned i) { p.ira = i % 32; p.xsel = 1; p.shift = 3; } },
	{ "FRCL", "un des quatre que le jeu ne pose JAMAIS (§5)",
	  [](DecodedStep &p, unsigned) { p.frcl = 1; } },
	{ "ADRL", "idem",
	  [](DecodedStep &p, unsigned) { p.adrl = 1; } },
	{ "YRL", "idem",
	  [](DecodedStep &p, unsigned) { p.yrl = 1; } },
	{ "NEGB", "idem",
	  [](DecodedStep &p, unsigned i) { p.tra = i % 128; p.bsel = 1; p.negb = 1; } },
	{ "les quatre rares", "les quatre ensemble : chez le coeur du projet, c'est le second corps",
	  [](DecodedStep &p, unsigned i) { p.tra = i % 128; p.bsel = 1;
	                           p.frcl = 1; p.adrl = 1; p.yrl = 1; p.negb = 1; } },
};

// Le temps par pas de DSP, mediane de trois passages : une seule mesure se fait porter par le voisinage.
double ns_par_pas(const char *the_core, const Caracteristique &k, unsigned n, volatile std::int64_t &snk)
{
	double v[3];
	for (int trial = 0; trial < 3; trial++) {
		Bench b(the_core);
		std::vector<DecodedStep> step(STEPS_PER_PROGRAM);
		for (unsigned i = 0; i < STEPS_PER_PROGRAM; i++) { baseline(step[i], i); k.put(step[i], i); }
		b.program(step);
		v[trial] = stopwatch(*b.c, n, snk) * 1e9 / (double(n) * STEPS_PER_PROGRAM);
	}
	std::sort(v, v + 3);
	return v[1];
}

// Le temoin : le programme de la forme du jeu, par le meme chronometre et la meme unite.
double ns_par_pas_du_jeu(const char *the_core, unsigned n, volatile std::int64_t &snk)
{
	double v[3];
	for (int trial = 0; trial < 3; trial++) {
		Bench b(the_core);
		b.programme_dsp();
		v[trial] = stopwatch(*b.c, n, snk) * 1e9 / (double(n) * STEPS_PER_PROGRAM);
	}
	std::sort(v, v + 3);
	return v[1];
}

// LE SOCLE EST-IL PAR PAS OU PAR ECHANTILLON ? La question decide ou l'on repare, et une seule longueur de
// programme ne peut pas y repondre : un cout par echantillon (la remise a zero d'EFREG, le branchement vers
// l'un des deux corps, ce que Start laisse derriere lui) se divise par le nombre de pas et se deguise en
// cout par pas. Trois longueurs le separent -- la PENTE est le cout d'un pas, l'ORDONNEE A L'ORIGINE est ce
// que l'echantillon coute avant son premier pas.
void baseline_by_length(unsigned n, volatile std::int64_t &snk)
{
	const unsigned longueurs[3] = { 16, 48, 112 };
	printf("\nLE SOCLE, PAR LONGUEUR DE PROGRAMME : par pas ou par echantillon ?\n\n");
	printf("  %-8s %12s %12s %12s\n", "coeur", "16 pas", "48 pas", "112 pas");
	printf("  %-8s %12s %12s %12s\n", "", "ns/ech", "ns/ech", "ns/ech");
	double ns[2][3] = {};
	const char *cores[2] = { "mame", "own" };
	for (int ic = 0; ic < 2; ic++) {
		for (int il = 0; il < 3; il++) {
			double v[3];
			for (int trial = 0; trial < 3; trial++) {
				Bench b(cores[ic]);
				std::vector<DecodedStep> step(longueurs[il]);
				for (unsigned i = 0; i < longueurs[il]; i++) baseline(step[i], i);
				b.program(step);
				v[trial] = stopwatch(*b.c, n, snk) * 1e9 / double(n);
			}
			std::sort(v, v + 3);
			ns[ic][il] = v[1];
		}
		printf("  %-8s %12.1f %12.1f %12.1f\n", cores[ic], ns[ic][0], ns[ic][1], ns[ic][2]);
	}
	// La pente entre 16 et 112 pas, et ce qui reste quand on la retire : deux points suffisent, le troisieme
	// est la pour dire si c'est bien une droite.
	printf("\n  %-8s %14s %18s %14s\n", "coeur", "ns par pas", "ns par echantillon", "(controle)");
	for (int ic = 0; ic < 2; ic++) {
		const double pente = (ns[ic][2] - ns[ic][0]) / double(longueurs[2] - longueurs[0]);
		const double origine = ns[ic][0] - pente * double(longueurs[0]);
		const double expected48 = origine + pente * double(longueurs[1]);
		printf("  %-8s %14.3f %18.1f %14s\n", cores[ic], pente, origine,
		       (std::abs(expected48 - ns[ic][1]) < 0.06 * ns[ic][1]) ? "droite" : "PAS DROITE");
	}
	const double pente_a = (ns[0][2] - ns[0][0]) / 96.0, pente_b = (ns[1][2] - ns[1][0]) / 96.0;
	const double orig_a = ns[0][0] - pente_a * 16.0, orig_b = ns[1][0] - pente_b * 16.0;
	printf("\n  ecart par pas        %+.3f ns  (%.0f %%)\n", pente_b - pente_a,
	       pente_a > 0 ? 100.0 * (pente_b / pente_a - 1.0) : 0.0);
	printf("  ecart par echantillon %+.1f ns  (%.0f %%)\n", orig_b - orig_a,
	       orig_a > 0 ? 100.0 * (orig_b / orig_a - 1.0) : 0.0);
	printf("  et sur un programme de 112 pas, l'ecart par echantillon pese %+.3f ns par pas,\n"
	       "  soit %.0f %% de l'ecart du socle -- c'est la part qu'on reparerait HORS de la boucle de pas.\n",
	       (orig_b - orig_a) / 112.0,
	       (ns[1][2] - ns[0][2]) != 0 ? 100.0 * (orig_b - orig_a) / (ns[1][2] - ns[0][2]) : 0.0);
	printf("  (controle : le troisieme point doit dire `droite` des deux cotes, sinon ces deux nombres ne\n"
	       "  separent rien et il faut plus de longueurs.)\n");
}

// `--baseline` : le socle seul, et rien d'autre. Il existe pour une raison precise -- le socle porte TOUT
// l'ecart du DSP (docs/scsp_audit.md §4 septies), et le chercher demande de comparer des CONSTRUCTIONS
// differentes du meme fichier, donc d'entrelacer reference et variante. La table complete coute trop cher
// pour ca ; celle-ci tient en deux secondes, et elle fait SEPT passages au lieu de trois parce que c'est un
// ecart de 20 % sur deux nanosecondes qu'on lui demande de tenir.
// UN SEUL COEUR, ET LE PROCESSUS NE FAIT QUE CA. C'est ce que demande un compteur materiel : `xperf`
// totalise ses compteurs PAR PROCESSUS, donc deux coeurs dans le meme binaire ne se separent pas sans
// symboles -- et nos binaires sont construits par MinGW, en DWARF et sans PDB, que `xperf` ne lit pas.
// Deux captures d'un processus chacune contournent le probleme entierement : les instructions retirees,
// les branchements mal predits et les cycles du processus SONT ceux du socle de ce coeur-la, a la mise en
// route pres, qui est la meme des deux cotes.
//
//   measure-core --baseline [echantillons]            les deux coeurs, pour lire l'ecart tout de suite
//   measure-core --baseline <coeur> [echantillons]    un seul, pour qu'un compteur externe l'attribue
// `--game <coeur> [n]` : LE PROGRAMME DE LA FORME DU JEU, un seul coeur, aucune voix. C'est `--baseline` pour
// le programme qui compte, et il a fallu une mesure pour qu'il existe : le 2026-10-28, la combinaison des
// quatre variantes gagnait **29,3 %** sur le socle et **rien de mesurable** sur celui-ci
// (docs/scsp_audit.md §4 nonies). Le socle est un programme ou les 112 pas font tous la meme chose ; il
// situe un cout, il ne dit pas ce qu'une modification rend dans le jeu.
//
// Un seul coeur par processus, pour la meme raison que `--baseline <coeur>` : un compteur materiel totalise
// par processus, et nos binaires MinGW ne lui donnent aucun nom de fonction.
int bench_game(const char *the_core, unsigned n)
{
	volatile std::int64_t snk = 0;
	double v[7];
	for (int trial = 0; trial < 7; trial++) {
		Bench b(the_core);
		b.programme_dsp();
		v[trial] = stopwatch(*b.c, n, snk) * 1e9 / (double(n) * STEPS_PER_PROGRAM);
	}
	std::sort(v, v + 7);
	printf("jeu %u, coeur \"%s\" SEUL : %.3f ns/pas [%.3f-%.3f] ; %llu pas de DSP executes  (puits %lld)\n",
	       n, the_core, v[3], v[0], v[6],
	       (unsigned long long)(7ull * n * STEPS_PER_PROGRAM), (long long) snk);
	return 0;
}

int bench_baseline(unsigned n, const char *seul)
{
	volatile std::int64_t snk = 0;
	double v[2][7];
	const char *cores[2] = { "mame", "own" };
	const int de = seul ? (strcmp(seul, "own") == 0 ? 1 : 0) : 0;
	const int a_ = seul ? de : 0, b_ = seul ? de + 1 : 2;
	for (int ic = a_; ic < b_; ic++)
		for (int trial = 0; trial < 7; trial++) {
			Bench b(cores[ic]);
			std::vector<DecodedStep> step(STEPS_PER_PROGRAM);
			for (unsigned i = 0; i < STEPS_PER_PROGRAM; i++) baseline(step[i], i);
			b.program(step);
			v[ic][trial] = stopwatch(*b.c, n, snk) * 1e9 / (double(n) * STEPS_PER_PROGRAM);
		}
	for (int ic = a_; ic < b_; ic++) std::sort(v[ic], v[ic] + 7);
	if (seul) {
		// Le nombre de PAS de DSP que ce processus vient d'executer : c'est le denominateur par lequel un
		// compteur externe divisera ses totaux. 7 passages, n echantillons, 112 pas.
		const double ns = v[de][3];
		printf("socle %u, coeur \"%s\" SEUL : %.3f ns/pas [%.3f-%.3f] ; %llu pas de DSP executes"
		       "  (puits %lld)\n",
		       n, cores[de], ns, v[de][0], v[de][6],
		       (unsigned long long)(7ull * n * STEPS_PER_PROGRAM), (long long) snk);
		return 0;
	}
	const double a = v[0][3], b = v[1][3];
	printf("socle %u : mame %.3f [%.3f-%.3f]  maison %.3f [%.3f-%.3f]  ecart %+.3f  rapport %.3f"
	       "  (puits %lld)\n",
	       n, a, v[0][0], v[0][6], b, v[1][0], v[1][6], b - a, a > 0 ? b / a : 0.0, (long long) snk);
	return 0;
}

int bench_dsp(unsigned n)
{
	printf("measure-core --dsp : le DSP par caracteristique, %u echantillons par cas,\n"
	       "mediane de trois passages, 112 pas par programme. Les deux coeurs, et l'ecart.\n\n", n);
	// Les trois dernieres colonnes sont le COUT PROPRE : la valeur moins le socle. C'est elles qu'il faut
	// lire, et pas le temps brut -- le socle est dans chaque ligne, et il porte deja un ecart.
	printf("  %-20s %9s %9s %9s %9s %9s %9s\n",
	       "caracteristique", "mame", "own", "ecart", "mame", "own", "ecart");
	printf("  %-20s %9s %9s %9s %9s %9s %9s\n",
	       "", "ns/pas", "ns/pas", "ns/pas", "propre", "propre", "propre");

	volatile std::int64_t snk = 0;
	double socle_a = 0, socle_b = 0;
	for (const Caracteristique &k : CHARACTERISTICS) {
		const double a = ns_par_pas("mame", k, n, snk);
		const double b = ns_par_pas("own", k, n, snk);
		if (!strcmp(k.name, "socle")) {
			socle_a = a; socle_b = b;
			printf("  %-20s %9.3f %9.3f %+9.3f %9s %9s %9s\n", k.name, a, b, b - a, "--", "--", "--");
			continue;
		}
		printf("  %-20s %9.3f %9.3f %+9.3f %9.3f %9.3f %+9.3f\n", k.name, a, b, b - a,
		       a - socle_a, b - socle_b, (b - socle_b) - (a - socle_a));
	}
	const double ja = ns_par_pas_du_jeu("mame", n, snk);
	const double jb = ns_par_pas_du_jeu("own", n, snk);
	printf("\n  %-20s %9.3f %9.3f %+9.3f %9.3f %9.3f %+9.3f   <- LE TEMOIN\n", "la forme du jeu", ja, jb,
	       jb - ja, ja - socle_a, jb - socle_b, (jb - socle_b) - (ja - socle_a));
	printf("\nCE QU'IL FAUT LIRE, dans cet ordre :\n"
	       "  1. LE SOCLE D'ABORD : mame %.3f, maison %.3f, soit %+.3f ns par pas (%.0f %%) sur 112 pas qui ne\n"
	       "     font RIEN. Un ecart qui est deja la n'est pas une instruction, c'est la boucle de pas\n"
	       "     elle-meme -- et il ne se repare pas au meme endroit qu'une instruction.\n"
	       "  2. puis `ecart propre`, qui est le socle deduit : ce que la caracteristique coute EN PLUS d'un\n"
	       "     coeur a l'autre. Un nombre negatif dit que le coeur du projet y est meilleur.\n"
	       "  3. le temoin en dernier : si son ecart n'est pas du meme ordre que ceux qu'on vient de lire,\n"
	       "     c'est ce banc qu'il faut croire le moins -- 112 pas identiques n'ont ni le cache ni les\n"
	       "     dependances du vrai programme (en-tete de ce fichier).\n",
	       socle_a, socle_b, socle_b - socle_a, 100.0 * (socle_b / socle_a - 1.0));
	baseline_by_length(n, snk);
	printf("\n(puits %lld, pour que rien ne soit optimise au loin)\n", (long long) snk);
	return 0;
}

} // namespace

int main(int argc, char **argv)
{
	if (argc > 1 && !strcmp(argv[1], "--dsp"))
		return bench_dsp(argc > 2 ? unsigned(strtoul(argv[2], nullptr, 10)) : 400000);
	if (argc > 2 && !strcmp(argv[1], "--game"))
		return bench_game(argv[2], argc > 3 ? unsigned(strtoul(argv[3], nullptr, 10)) : 400000);
	if (argc > 1 && !strcmp(argv[1], "--baseline")) {
		// `--baseline [n]` ou `--baseline <coeur> [n]` : un argument qui n'est pas un nombre est un nom de coeur.
		const bool nomme = argc > 2 && !isdigit((unsigned char)argv[2][0]);
		const char *seul = nomme ? argv[2] : nullptr;
		const int in = nomme ? 3 : 2;
		return bench_baseline(argc > in ? unsigned(strtoul(argv[in], nullptr, 10)) : 600000, seul);
	}

	const char *name = argc > 1 ? argv[1] : "mame";
	const unsigned n = argc > 2 ? unsigned(strtoul(argv[2], nullptr, 10)) : 2000000;

	printf("measure-core : coeur \"%s\", %u echantillons par cas\n\n", name, n);
	printf("  %-14s %10s %12s %14s\n", "cas", "secondes", "ns/ech", "%% d'un coeur");

	volatile std::int64_t snk = 0;
	struct Case { const char *name; unsigned voix; bool dsp; };
	const Case cases[] = { { "a vide", 0, false }, { "fentes", 32, false },
	                    { "DSP", 0, true }, { "les deux", 32, true } };
	for (const Case &k : cases) {
		Bench b(name);
		if (k.dsp) b.programme_dsp();
		if (k.voix) b.voix(k.voix);
		const double s = stopwatch(*b.c, n, snk);
		// La part d'un coeur : le temps mis pour produire n echantillons, contre leur duree reelle.
		const double duration = double(n) / 44100.0;
		printf("  %-14s %10.3f %12.1f %13.2f\n", k.name, s, s * 1e9 / n, 100.0 * s / duration);
	}
	printf("\n(puits %lld, pour que rien ne soit optimise au loin)\n", (long long) snk);
	return 0;
}
