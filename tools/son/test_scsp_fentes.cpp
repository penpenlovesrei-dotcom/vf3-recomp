// test_scsp_fentes : l'oscillateur, l'adressage PCM et les quatre bouclages, a l'echantillon exact.
//
// Bloc 2 de docs/scsp_architecture.md §3 -- "Fentes, oscillateur, PCM, adressage, bouclage : 8 et 16 bits
// signes, SA/LSA/LEA, les quatre modes LPCTL, l'interpolation lineaire, les limites de la RAM. Se teste en
// comptant des echantillons." Comme pour le bloc 1, le test precede l'implementation : il dit ce que le
// coeur de MAME rend aujourd'hui, par l'interface neuve, pour qu'une reecriture ait une cible.
//
// COMMENT ON MESURE L'OSCILLATEUR SEUL. Un test qui keye une voix ordinaire mesure l'oscillateur ET
// l'enveloppe ET l'attenuateur, et ne peut rien affirmer d'exact. Mais la puce a une porte pour cela :
// SDIR ("sound direct"). Dans UpdateSlot, `if (!SDIR(slot))` saute l'enveloppe et l'ALFO ; dans le melange,
// `SDIR(slot) ? 0 : TL(slot)` met l'attenuation a zero. Avec SDIR=1, DISDL=7 et DIPAN=0, la sortie gauche
// vaut donc l'echantillon brut de l'oscillateur, a un facteur entier pres -- et ce facteur, le premier cas
// le MESURE au lieu de le supposer.
//
// CE QUE CELA PERMET DE DIRE, et qu'aucune lecture du code ne dit aussi surement : la suite exacte des
// INDICES d'echantillon que la fente parcourt. C'est tout le bloc 2 : les quatre bouclages ne sont que
// quatre suites d'indices, et une reecriture qui en change une le saura ici.
//
// Le montage -- un coeur neuf par cas, la RAM, le keyage -- est dans tools/son/banc_scsp.h.
#include "banc_scsp.h"

#include <cstdio>
#include <string>
#include <vector>

namespace {

using banc::Banc;
using banc::pose;
using banc::suite;

int echecs = 0;

void ok(const char *quoi, const std::string &d) { printf("  ok    %-38s %s\n", quoi, d.c_str()); }
void rate(const char *quoi, const std::string &d) { printf("  ECHEC %-38s %s\n", quoi, d.c_str()); ++echecs; }

// Les reglages communs a tous les cas : l'oscillateur a nu, pleine sortie directe au centre.
void a_nu(std::uint16_t mot[16], std::uint32_t sa)
{
	pose(mot, "SA_HI",  sa >> 16);
	pose(mot, "SA_LO",  sa & 0xFFFF);
	pose(mot, "SDIR",   1);     // saute l'enveloppe, l'ALFO et TL
	pose(mot, "DISDL",  7);     // pleine sortie directe
	pose(mot, "DIPAN",  0);     // centre
	pose(mot, "OCT",    0);     // avec FNS=0 : un echantillon de source par echantillon de sortie
	pose(mot, "FNS",    0);
	pose(mot, "KYONB",  1);
}

int FACTEUR = 0;   // mesure par le cas 1 : sortie gauche / echantillon brut

} // namespace

int main()
{
	printf("test_scsp_fentes : l'oscillateur et les bouclages, par scsp::core\n\n");

	// ---- 1. LE FACTEUR, mesure et non suppose
	// Tout le reste de ce fichier en depend, donc il ne doit pas etre une constante ecrite de memoire.
	{
		Banc b;
		for (unsigned k = 0; k < 16; k++) b.ram[0x1000 / 2 + k] = 1000;   // continu
		std::uint16_t mot[16] = {};
		a_nu(mot, 0x1000);
		pose(mot, "LPCTL", 1);
		pose(mot, "LSA", 0);
		pose(mot, "LEA", 16);
		b.keye(0, mot);
		const std::vector<std::int32_t> g = b.gauche(8);
		bool plat = true;
		for (std::int32_t v : g) if (v != g[0]) { plat = false; break; }
		FACTEUR = (plat && g[0] % 1000 == 0) ? int(g[0] / 1000) : 0;
		(plat && FACTEUR == 4)
			? ok("SDIR : la sortie est l'echantillon brut",
			     "1000 en RAM ressort a " + std::to_string(g[0]) + ", soit un facteur " + std::to_string(FACTEUR)
			     + " (131072 / 32768) sur les 8 echantillons")
			: rate("SDIR : la sortie est l'echantillon brut",
			       "attendu 8 fois 4000 ; obtenu " + suite(g, 1));
		if (FACTEUR == 0) { printf("\nle facteur n'a pas pu etre mesure : la suite n'aurait aucun sens.\n"); return 1; }
	}

	// ---- 2. LSA et LEA COMPTENT DES ECHANTILLONS, l'acces compte des octets
	// C'est l'asymetrie du bloc, et elle est invisible a la lecture : la lecture 16 bits prend
	// `(cur_addr >> (SHIFT-1)) & ~1`, soit DEUX octets par echantillon, alors que la comparaison de
	// bouclage reprend `cur_addr >> SHIFT`, soit UN indice. Si LEA etait en octets, la periode serait 4
	// et non 8 : c'est la mesure qui tranche, pas le raisonnement.
	{
		Banc b;
		b.rampe(0x1000, 32, 100);
		std::uint16_t mot[16] = {};
		a_nu(mot, 0x1000);
		pose(mot, "LPCTL", 1);
		pose(mot, "LSA", 0);
		pose(mot, "LEA", 8);
		b.keye(0, mot);
		const std::string s = suite(b.gauche(20), FACTEUR * 100);
		const char *attendu = "0,1,2,3,4,5,6,7,0,1,2,3,4,5,6,7,0,1,2,3";
		(s == attendu) ? ok("LSA/LEA en echantillons, pas en octets",
		                    "LEA=8 donne une periode de 8 indices : " + s)
		               : rate("LSA/LEA en echantillons, pas en octets",
		                      "attendu " + std::string(attendu) + " ; obtenu " + s);
	}

	// ---- 3. PCM8B : un octet par echantillon, l'octet HAUT du mot d'abord, et decale de 8
	// read_byte rend `m_ram[a >> 1] >> ((a & 1) ? 0 : 8)`, donc l'octet 0 est l'octet HAUT du mot 0 : la
	// RAM est vue en gros-boutien, comme le 68000 qui la remplit. Et la valeur part `p1 << 8` : un
	// echantillon 8 bits occupe les huit bits hauts, il n'est pas etire sur seize.
	{
		Banc b;
		b.ram[0x1000 / 2 + 0] = 0x1234;
		b.ram[0x1000 / 2 + 1] = 0x5678;
		std::uint16_t mot[16] = {};
		a_nu(mot, 0x1000);
		pose(mot, "PCM8B", 1);
		pose(mot, "LPCTL", 1);
		pose(mot, "LSA", 0);
		pose(mot, "LEA", 4);
		b.keye(0, mot);
		const std::vector<std::int32_t> g = b.gauche(4);
		// 0x12, 0x34, 0x56, 0x78 en signe, chacun << 8, puis le facteur
		const bool juste = g[0] == 0x12 * 256 * FACTEUR && g[1] == 0x34 * 256 * FACTEUR
		                && g[2] == 0x56 * 256 * FACTEUR && g[3] == 0x78 * 256 * FACTEUR;
		juste ? ok("PCM8B : octet haut d'abord, decale de 8",
		           "0x1234 0x5678 donnent 0x12 0x34 0x56 0x78, soit " + suite(g, FACTEUR * 256))
		      : rate("PCM8B : octet haut d'abord, decale de 8",
		             "attendu 18,52,86,120 (x256) ; obtenu " + suite(g, FACTEUR * 256));
	}

	// ---- 4. LES QUATRE BOUCLAGES, en suites d'indices
	// LSA=4, LEA=8, et on regarde ou va la tete de lecture. Les quatre suites sont ENREGISTREES : elles
	// ne sont pas deduites d'une specification, elles sont ce que ce coeur fait.
	//
	// ET C'EST ICI QUE LA MESURE A DEMENTI LA LECTURE, pour les quatre modes d'un coup. En lisant le code
	// j'avais predit que les modes 2 et 3 lisent l'indice 8, hors de [LSA, LEA] : le pas est applique APRES
	// la lecture, donc le demi-tour semble partir d'une adresse deja sortie de la boucle. C'est faux, et
	// voici pourquoi :
	//
	//   la boucle de bouclage tourne sur DEUX adresses, `cur_addr` et `nxt_addr` (le `for (addr_select...)`).
	//   Or juste avant elle, `slot->nxt_addr = slot->cur_addr + (1 << SHIFT)` : nxt_addr est REAFFECTE a
	//   chaque echantillon. Donc tout ce que la passe nxt_addr ECRIT dans l'adresse est perdu au tour
	//   suivant -- mais ses EFFETS DE BORD, eux, restent : `Backwards = 1` et `StopSlot`.
	//
	// Et comme nxt_addr vaut toujours un indice de plus que cur_addr, ces effets de bord se declenchent un
	// echantillon TOT. D'ou les trois consequences que la mesure montre et qu'aucune lecture ne donnait :
	//   LPCTL=0 s'arrete apres l'indice 6 et non 7 : c'est nxt_addr, a 8, qui a appele StopSlot ;
	//   LPCTL=2 passe de 3 a 6, pas a 8 ;
	//   LPCTL=3 repart de 6 apres 7, pas de 8 ;
	//   et AUCUN echantillon au-dela de LEA n'est jamais lu, dans aucun mode.
	//
	// Une reecriture qui n'appliquerait le bouclage qu'a cur_addr -- ce que la lecture naive suggere --
	// decalerait les quatre modes d'un echantillon et jouerait un echantillon de trop. Celle qui veut
	// "nettoyer" la double passe doit garder ce decalage-la.
	{
		struct Cas { unsigned lpctl; const char *nom; const char *attendu; };
		const Cas cas[] = {
			{ 0, "LPCTL=0 sans boucle",   "0,1,2,3,4,5,6,0,0,0,0,0,0,0,0,0,0,0,0,0" },
			{ 1, "LPCTL=1 boucle normale","0,1,2,3,4,5,6,7,4,5,6,7,4,5,6,7,4,5,6,7" },
			{ 2, "LPCTL=2 boucle inverse","0,1,2,3,6,5,4,7,6,5,4,7,6,5,4,7,6,5,4,7" },
			{ 3, "LPCTL=3 va-et-vient",   "0,1,2,3,4,5,6,7,6,5,4,5,6,7,6,5,4,5,6,7" },
		};
		for (const Cas &c : cas) {
			Banc b;
			b.rampe(0x1000, 32, 100);
			b.ram[0x1000 / 2] = 1;   // l'indice 0 ne doit pas valoir 0, sinon il se confond avec le silence
			std::uint16_t mot[16] = {};
			a_nu(mot, 0x1000);
			pose(mot, "LPCTL", c.lpctl);
			pose(mot, "LSA", 4);
			pose(mot, "LEA", 8);
			b.keye(0, mot);
			std::vector<std::int32_t> g = b.gauche(20);
			if (!g.empty() && g[0] == FACTEUR) g[0] = 0;   // on rend son 0 a l'indice 0
			const std::string s = suite(g, FACTEUR * 100);
			(s == c.attendu) ? ok(c.nom, s)
			                 : rate(c.nom, "attendu " + std::string(c.attendu) + " ; obtenu " + s);
		}
	}

	// ---- 5. L'INTERPOLATION LINEAIRE, au demi-echantillon exact
	// OCT vaut -1 (champ 0xF : `(OCT ^ 8) - 8`), donc le pas est 2048 au lieu de 4096 : un demi-echantillon
	// de source par echantillon de sortie. Entre 0 et 1000 on doit lire exactement 500, pas 499 ni 512.
	{
		Banc b;
		b.rampe(0x1000, 32, 1000);
		std::uint16_t mot[16] = {};
		a_nu(mot, 0x1000);
		pose(mot, "OCT", 0xF);      // -1
		pose(mot, "LPCTL", 1);
		pose(mot, "LSA", 0);
		pose(mot, "LEA", 24);
		b.keye(0, mot);
		const std::string s = suite(b.gauche(9), FACTEUR * 500);
		const char *attendu = "0,1,2,3,4,5,6,7,8";   // 0, 500, 1000, 1500, ... par pas de 500
		(s == attendu) ? ok("interpolation : le demi-echantillon exact",
		                    "pas 2048 : la sortie monte par 500 (demi-pas de 1000) : " + s)
		               : rate("interpolation : le demi-echantillon exact",
		                      "attendu des multiples exacts de 500 ; obtenu " + s);
	}

	// ---- 6. SBCTL : deux inversions qui ne se ressemblent pas
	// bit 0 fait `sample ^= 0x7FFF` -- un OU exclusif sur un s32, SANS troncature ; bit 1 fait
	// `sample = (s16)(sample ^ 0x8000)`, avec troncature. Les deux ensemble donnent presque l'oppose, a un
	// pres : 1000 ressort -1001. La difference de troncature n'est pas observable ici, l'echantillon venant
	// d'une lecture 16 bits -- c'est dit plutot que teste, pour qu'une reecriture ne croie pas l'avoir vue.
	{
		struct Cas { unsigned sbctl; std::int32_t attendu; const char *quoi; };
		const Cas cas[] = {
			{ 0,  1000, "SBCTL=0 : tel quel" },
			{ 1, 31767, "SBCTL=1 : ^0x7FFF, les quinze bits bas" },
			{ 2,-31768, "SBCTL=2 : ^0x8000 tronque, le signe" },
			{ 3, -1001, "SBCTL=3 : les deux, soit l'oppose a un pres" },
		};
		std::string lignes;
		bool juste = true;
		for (const Cas &c : cas) {
			Banc b;
			for (unsigned k = 0; k < 16; k++) b.ram[0x1000 / 2 + k] = 1000;
			std::uint16_t mot[16] = {};
			a_nu(mot, 0x1000);
			pose(mot, "SBCTL", c.sbctl);
			pose(mot, "LPCTL", 1);
			pose(mot, "LSA", 0);
			pose(mot, "LEA", 16);
			b.keye(0, mot);
			const std::int32_t v = b.gauche(4)[0] / FACTEUR;
			char t[80];
			snprintf(t, sizeof t, "%s%u->%d", lignes.empty() ? "" : " ", c.sbctl, v);
			lignes += t;
			if (v != c.attendu) { juste = false; lignes += "(attendu " + std::to_string(c.attendu) + ")"; }
		}
		juste ? ok("SBCTL : 1000 devient", lignes)
		      : rate("SBCTL : 1000 devient", lignes);
	}

	// ---- 7. LES LIMITES DE LA RAM : un repli a 1 Mo, et un SA impair qui perd son bit bas
	// read_word fait `a &= 0xffffe` : l'adresse se replie a 1 Mo ET perd son bit 0. Les deux sont
	// observables, et aucune des deux n'est evidente -- une reecriture qui adresserait en 32 bits ou qui
	// honorerait un SA impair jouerait autre chose.
	{
		// (a) le repli : l'echantillon d'apres le dernier mot est le PREMIER mot
		Banc b;
		b.ram[0xFFFFE / 2] = 7000;
		b.ram[0] = 1000;
		std::uint16_t mot[16] = {};
		a_nu(mot, 0xFFFFE);
		pose(mot, "LPCTL", 1);
		pose(mot, "LSA", 0);
		pose(mot, "LEA", 4);
		b.keye(0, mot);
		const std::vector<std::int32_t> g = b.gauche(2);
		const bool replie = g[0] == 7000 * FACTEUR && g[1] == 1000 * FACTEUR;

		// (b) un SA impair lit le meme mot qu'un SA pair
		Banc b2;
		b2.ram[0x1000 / 2] = 4242;
		std::uint16_t mot2[16] = {};
		a_nu(mot2, 0x1001);          // impair
		pose(mot2, "LPCTL", 1);
		pose(mot2, "LSA", 0);
		pose(mot2, "LEA", 4);
		b2.keye(0, mot2);
		// UN SEUL appel, garde dans une variable : `gauche()` fait AVANCER la fente. La premiere version de
		// ce cas l'appelait deux fois -- une fois pour comparer, une fois pour l'afficher -- et le message
		// imprimait l'echantillon SUIVANT, donc 0, a cote d'un test qui passait. Une mesure qui bouge en la
		// regardant ne se lit pas deux fois.
		const std::int32_t impair = b2.gauche(1)[0];
		const bool pair = impair == 4242 * FACTEUR;

		char t[160];
		snprintf(t, sizeof t, "0xFFFFE puis l'indice 1 rendent %d et %d ; SA=0x1001 rend %d",
		         g[0] / FACTEUR, g[1] / FACTEUR, impair / FACTEUR);
		(replie && pair) ? ok("la RAM se replie a 1 Mo, SA perd son bit 0", t)
		                 : rate("la RAM se replie a 1 Mo, SA perd son bit 0",
		                        std::string(t) + " ; attendu 7000, 1000 et 4242");
	}

	// ---- 8. SSCTL : le silence qui AVANCE, et celui qui n'avance pas
	// SSCTL=2 rend zero mais parcourt la source ; SSCTL=3 sort de UpdateSlot AVANT tout, donc la tete de
	// lecture ne bouge pas. En sortie les deux sont muets -- la difference ne se voit qu'APRES, en remettant
	// SSCTL a 0 et en regardant ou on en est. C'est le genre de detail qu'une reecriture unifie sans le
	// savoir, et qui decale ensuite tous les echantillons d'une voix.
	//
	// On reecrit le mot 0 sans KYONEX : UpdateSlotReg case 0 ne fait rien quand ce bit est absent, donc la
	// fente n'est pas redeclenchee.
	{
		std::string lignes;
		bool juste = true;
		for (auto [ssctl, attendu] : { std::pair<unsigned, const char *>{ 2, "4,5,6,7" },
		                               std::pair<unsigned, const char *>{ 3, "0,1,2,3" } }) {
			Banc b;
			b.rampe(0x1000, 32, 100);
			b.ram[0x1000 / 2] = 1;   // pour distinguer l'indice 0 du silence
			std::uint16_t mot[16] = {};
			a_nu(mot, 0x1000);
			pose(mot, "SSCTL", ssctl);
			pose(mot, "LPCTL", 1);
			pose(mot, "LSA", 0);
			pose(mot, "LEA", 32);
			b.keye(0, mot);
			b.gauche(4);                               // quatre echantillons muets
			std::uint16_t suite_mot[16] = {};
			a_nu(suite_mot, 0x1000);
			pose(suite_mot, "SSCTL", 0);               // la source redevient la RAM
			pose(suite_mot, "LPCTL", 1);
			b.coeur.write_reg(0, suite_mot[0], 0xFFFF);
			std::vector<std::int32_t> g = b.gauche(4);
			if (!g.empty() && g[0] == FACTEUR) g[0] = 0;
			const std::string s = suite(g, FACTEUR * 100);
			lignes += (lignes.empty() ? "" : " | ") + ("SSCTL=" + std::to_string(ssctl) + " -> " + s);
			if (s != attendu) { juste = false; lignes += " (attendu " + std::string(attendu) + ")"; }
		}
		juste ? ok("SSCTL=2 avance, SSCTL=3 non", lignes)
		      : rate("SSCTL=2 avance, SSCTL=3 non", lignes);
	}

	printf("\n");
	if (echecs) { printf("%d echec(s).\n", echecs); return 1; }
	printf("tous les cas passent\n");
	return 0;
}
