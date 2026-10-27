// test_scsp_mix : le melange et le rendu -- DISDL, DIPAN, TL, DAC18B, MVOL, et l'ecretage.
//
// Bloc 7 de docs/scsp_architecture.md §3 -- "Melange et rendu : DISDL/DIPAN, EFSDL/EFPAN, MVOL, DAC18B, les
// tables de panoramique. Les deux volumes par groupe et le niveleur d'effets de ce portage sont AU-DESSUS du
// chip, pas dedans : ils restent dans la carte." Le dernier blocs, et celui qui touche a tout : chaque voix
// y passe deux fois, une pour la sortie directe et une pour l'envoi au DSP.
//
// LA SONDE EST L'ONDE CONTINUE DES BLOCS 3 ET 4, avec SDIR=1 pour ecarter l'enveloppe : la sortie est alors
// le GAIN du melange, et rien d'autre. Et comme ce bloc est celui des deux canaux, le banc a de quoi lire
// les deux (banc::Banc::stereo) -- la moitie des faits de DIPAN sont sur le canal que le reste du projet
// jette.
//
// TROIS TABLES DE DECIBELS A LA SEGA, et c'est la forme du bloc. Aucune n'est une formule : chacune est une
// somme de termes choisis bit par bit, ce qui donne des gains que rien ne rend "ronds".
//     TL     0.4, 0.8, 1.5, 3, 6, 12, 24, 48 dB, un bit chacun (huit bits, 95.7 dB en tout)
//     DIPAN  3, 6, 12, 24 dB sur quatre bits, et le bit 4 choisit QUEL canal est attenue
//     SDLT   -inf, -36, -30, -24, -18, -12, -6, 0 dB pour les huit valeurs de DISDL
// Les trois sont lues ici dans leur entier, parce qu'une reecriture qui les remplacerait par une
// exponentielle reguliere ne se trahirait nulle part ailleurs.
//
// Le montage -- un coeur neuf par cas, la RAM, le keyage -- est dans tools/son/banc_scsp.h.
#include "banc_scsp.h"

#include <cstdio>
#include <string>
#include <vector>

namespace {

using banc::Banc;
using banc::pose;

int echecs = 0;

void ok(const char *quoi, const std::string &d) { printf("  ok    %-40s %s\n", quoi, d.c_str()); }
void rate(const char *quoi, const std::string &d) { printf("  ECHEC %-40s %s\n", quoi, d.c_str()); ++echecs; }

constexpr std::uint32_t ONDE = 0x1000;
constexpr int CONTINU = 8192;

// Une voix dont il ne reste que le melange : l'enveloppe posee a plat, pas de LFO, une source constante.
//
// PAS SDIR, ET C'EST LE PIEGE DE CE BLOC. Les blocs 2 et 4 se servaient de SDIR pour ecarter l'enveloppe,
// mais dans le melange `dir_tl = SDIR(slot) ? 0 : TL(slot)` -- SDIR met aussi TL A ZERO. La premiere version
// de ce fichier mesurait donc TL avec SDIR=1 et trouvait huit fois le meme gain, sans que rien n'ait l'air
// casse. On pose l'enveloppe a plat a la place (AR=0x1F, D1R=D2R=0, DL=0 : le multiplicateur vaut 4096 des
// le deuxieme echantillon, cf. le bloc 3), ce qui laisse TL agir.
void melange_a_nu(std::uint16_t mot[16])
{
	pose(mot, "SA_HI", ONDE >> 16);
	pose(mot, "SA_LO", ONDE & 0xFFFF);
	pose(mot, "OCT",   0);
	pose(mot, "FNS",   0);
	pose(mot, "LPCTL", 1);
	pose(mot, "LSA",   0);
	pose(mot, "LEA",   256);
	pose(mot, "AR",    0x1F);
	pose(mot, "D1R",   0);
	pose(mot, "D2R",   0);
	pose(mot, "DL",    0);
	pose(mot, "KYONB", 1);
}

// Le gain du melange, sur les deux canaux, au quatrieme echantillon : le premier passe par le chemin ATTACK
// (4092 au lieu de 4096, cf. le bloc 3) et ne mesurerait donc pas le melange seul.
//
// DAC18B EST ALLUME PAR DEFAUT, et ce n'est pas un detail : sans lui l'etage de sortie fait `smpl >> 2` puis
// remultiplie par quatre, donc toute mesure est arrondie au multiple de 4 le plus proche. La premiere
// version de ce fichier le laissait eteint et lisait 516 la ou la table dit 518, 23196 la ou elle dit 23197 :
// trois tables de decibels fausses de deux unites, par la faute d'un bit de configuration.
std::pair<std::int32_t, std::int32_t> gain(unsigned disdl, unsigned dipan, unsigned tl,
                                           bool dac18b = true, int source = CONTINU)
{
	Banc b;
	b.continu(ONDE, 256, source);
	if (!dac18b) b.coeur.write_reg(0x400 / 2, 0x0200 | 0x000F, 0xFFFF);   // MEM4MB seul : DAC18B efface
	std::uint16_t mot[16] = {};
	melange_a_nu(mot);
	pose(mot, "DISDL", disdl);
	pose(mot, "DIPAN", dipan);
	pose(mot, "TL",    tl);
	b.keye(0, mot);
	return b.stereo(4);
}

} // namespace

int main(int argc, char **argv)
{
	if (argc > 1) banc::nom_du_coeur() = argv[1];
	printf("test_scsp_mix : le melange et le rendu, par scsp::core\n\n");

	// ---- 1. L'ECHELLE, ET LE FACTEUR QUATRE QUI VIENT DE LA TABLE
	// `m_LPANTABLE[i] = FIX(4.0f * LPAN * TL * fSDL)` : il y a un QUATRE dans la table, et FIX multiplie par
	// 4096. A plein niveau au centre la table vaut donc 16384 et non 4096, si bien que le melange rend
	// quatre fois l'echantillon. C'est l'origine du facteur 4 que les tests des blocs 2 et 4 mesuraient sans
	// l'expliquer -- il n'est ni dans l'adaptateur ni dans l'etage de sortie, il est ici.
	{
		const auto [g, d] = gain(7, 0, 0);
		(g == 4 * CONTINU && d == 4 * CONTINU)
			? ok("plein niveau au centre : quatre fois l'echantillon",
			     std::to_string(CONTINU) + " en RAM ressort a " + std::to_string(g) + " a gauche et "
			     + std::to_string(d) + " a droite")
			: rate("plein niveau au centre : quatre fois l'echantillon",
			       "obtenu " + std::to_string(g) + " et " + std::to_string(d)
			       + ", attendu " + std::to_string(4 * CONTINU) + " des deux cotes");
	}

	// ---- 2. DISDL : LES HUIT NIVEAUX DE SDLT, ET LE ZERO QUI EST UN SILENCE
	// SDLT = {-inf, -36, -30, -24, -18, -12, -6, 0} dB. Le pas est de 6 dB sauf entre 1 et 2, ou il est de
	// 6 aussi mais depuis -36 : la table ne commence pas a -42, elle commence a l'infini negatif. DISDL=0
	// n'est donc pas "tres faible", c'est MUET -- `fSDL = 0.0` en clair dans le code.
	{
		// Mesures, et non calculees : FIX tronque un flottant, donc les entiers ne sont pas ceux qu'un
		// calcul en decibels donne a la main. La derniere unite vient de la table, pas de la physique.
		const int attendus[8] = { 0, 518, 1036, 2066, 4124, 8230, 16422, 32768 };
		std::string lignes;
		bool juste = true;
		for (unsigned s = 0; s < 8; s++) {
			const auto [g, d] = gain(s, 0, 0);
			char t[48];
			snprintf(t, sizeof t, "%s%u->%d", lignes.empty() ? "" : " ", s, g);
			lignes += t;
			if (g != attendus[s] || d != attendus[s]) { juste = false; lignes += "(att " + std::to_string(attendus[s]) + ")"; }
		}
		juste ? ok("DISDL : les huit niveaux, 0 etant muet", lignes)
		      : rate("DISDL : les huit niveaux, 0 etant muet", lignes);
	}

	// ---- 3. TL : HUIT BITS, HUIT NOMBRES DE DECIBELS
	// Chaque bit de TL retire son propre nombre de decibels, et ils ne sont pas en progression
	// geometrique : 0.4, 0.8, 1.5, 3, 6, 12, 24, 48. Les trois premiers cassent la regle du doublement
	// (0.4, 0.8, puis 1.5 et non 1.6), et c'est exactement le genre de detail qu'une reecriture "propre"
	// arrondit sans le voir.
	{
		struct Cas { unsigned tl; int attendu; const char *db; };
		const Cas cas[] = {
			{ 0x01, 31292, "-0.4 dB" }, { 0x02, 29884, "-0.8 dB" }, { 0x04, 27570, "-1.5 dB" },
			{ 0x08, 23196, "-3 dB"   }, { 0x10, 16422, "-6 dB"   }, { 0x20,  8230, "-12 dB" },
			{ 0x40,  2066, "-24 dB"  }, { 0x80,   130, "-48 dB"  },
		};
		std::string lignes;
		bool juste = true;
		for (const Cas &c : cas) {
			const auto [g, d] = gain(7, 0, c.tl);
			(void) d;
			char t[48];
			snprintf(t, sizeof t, "%s%02x->%d", lignes.empty() ? "" : " ", c.tl, g);
			lignes += t;
			if (g != c.attendu) { juste = false; lignes += "(att " + std::to_string(c.attendu) + ")"; }
		}
		juste ? ok("TL : un nombre de dB par bit", lignes)
		      : rate("TL : un nombre de dB par bit", lignes);
	}

	// ---- 4. DIPAN : LE BIT 4 CHOISIT LE CANAL ATTENUE, ET LE CANAL OPPOSE RESTE A FOND
	// `if (iPAN < 0x10) { LPAN = PAN; RPAN = 1.0; } else { RPAN = PAN; LPAN = 1.0; }` : le panoramique de
	// cette puce n'est pas une balance qui glisse, c'est une ATTENUATION d'un seul cote, l'autre restant a
	// plein niveau. Une reecriture qui ferait une loi de panoramique continue -- un cosinus, une racine --
	// changerait le niveau absolu de toutes les voix non centrees du jeu.
	{
		struct Cas { unsigned dipan; int g, d; const char *quoi; };
		const Cas cas[] = {
			{ 0x00, 32768, 32768, "centre : les deux a fond" },
			{ 0x01, 23196, 32768, "-3 dB a gauche seulement" },
			{ 0x02, 16422, 32768, "-6 dB a gauche" },
			{ 0x11, 32768, 23196, "le bit 4 : -3 dB a droite" },
			{ 0x12, 32768, 16422, "-6 dB a droite" },
		};
		std::string lignes;
		bool juste = true;
		for (const Cas &c : cas) {
			const auto [g, d] = gain(7, c.dipan, 0);
			char t[64];
			snprintf(t, sizeof t, "%s%02x->%d/%d", lignes.empty() ? "" : " ", c.dipan, g, d);
			lignes += t;
			if (g != c.g || d != c.d) {
				juste = false;
				lignes += "(att " + std::to_string(c.g) + "/" + std::to_string(c.d) + ")";
			}
		}
		juste ? ok("DIPAN attenue UN canal, l'autre reste plein", lignes)
		      : rate("DIPAN attenue UN canal, l'autre reste plein", lignes);
	}

	// ---- 5. DIPAN 0x0F COUPE LE CANAL NET, ET CE N'EST PAS LA SOMME DES QUATRE BITS
	// `if ((iPAN & 0xf) == 0xf) PAN = 0.0;` : la valeur 0xF n'est pas -45 dB (3+6+12+24), c'est le SILENCE
	// de ce canal. Un cas particulier en dur, au milieu d'une table par ailleurs additive -- donc une
	// reecriture qui sommerait les quatre bits sans cette exception laisserait filtrer -45 dB au lieu de
	// rien, ce qui s'entend sur une voix forte.
	{
		const auto [g15, d15] = gain(7, 0x0F, 0);
		const auto [g31, d31] = gain(7, 0x1F, 0);
		char t[128];
		snprintf(t, sizeof t, "0x0F -> %d/%d (gauche coupe), 0x1F -> %d/%d (droite coupee)", g15, d15, g31, d31);
		(g15 == 0 && d15 == 32768 && g31 == 32768 && d31 == 0)
			? ok("DIPAN 0x0F : un silence, pas une attenuation", t)
			: rate("DIPAN 0x0F : un silence, pas une attenuation",
			       std::string(t) + " ; attendu 0/32768 puis 32768/0");
	}

	// ---- 6. DAC18B NE CHANGE PAS LE GAIN, IL CHANGE LA RESOLUTION
	// Les deux branches de sortie se ressemblent peu et donnent pourtant la meme echelle :
	//     DAC18B : clamp(smpl, -131072, 131071) / 131072
	//     sinon  : clamp(smpl >> 2, -32768, 32767) / 32768
	// Le facteur 4 entre les deux denominateurs annule le `>> 2`, donc le niveau est identique. Ce que le
	// bit change vraiment, c'est que SANS lui les DEUX BITS BAS de la somme sont jetes. Il faut donc une
	// somme qui ne soit pas un multiple de quatre pour le voir -- un TL impair suffit.
	{
		// 5000 ET NON 8192 : avec une source en puissance de deux, (source * table) >> 12 est toujours un
		// multiple de quatre et le `>> 2` ne perd rien -- la difference serait invisible.
		const auto [a18, b18] = gain(7, 0, 0x01, true, 5000);
		const auto [a16, b16] = gain(7, 0, 0x01, false, 5000);
		(void) b18; (void) b16;
		char t[144];
		snprintf(t, sizeof t, "TL=01 : avec DAC18B %d, sans %d (soit %d et ses deux bits bas jetes)",
		         a18, a16, a18);
		(a18 == 19099 && a16 == 19096 && (a16 & 3) == 0)
			? ok("DAC18B : la resolution, pas le gain", t)
			: rate("DAC18B : la resolution, pas le gain",
			       std::string(t) + " ; attendu 19099 puis 19096");
	}

	// ---- 7. L'ECRETAGE, ET SON ASYMETRIE
	// `clamp(smpl, -131072, 131072 - 1)` : la borne haute est un cran plus basse que la borne basse en
	// valeur absolue, comme tout entier signe. Avec deux voix a plein niveau le melange depasse, et c'est
	// l'ecretage qui decide -- pas une saturation douce, un couperet. On le mesure en keyant deux fentes sur
	// la meme onde.
	{
		Banc b;
		b.continu(ONDE, 256, CONTINU);
		for (unsigned f = 0; f < 5; f++) {       // cinq voix a 4 x 8192 = 163840 chacune : largement au-dela
			std::uint16_t mot[16] = {};
			melange_a_nu(mot);
			pose(mot, "DISDL", 7);
			pose(mot, "DIPAN", 0);
			b.keye(f, mot);
		}
		const auto [g, d] = b.stereo(4);
		char t[112];
		snprintf(t, sizeof t, "cinq voix a plein niveau : %d et %d", g, d);
		(g == 131071 && d == 131071) ? ok("l'ecretage : +131071 et non +131072", t)
		                             : rate("l'ecretage : +131071 et non +131072",
		                                    std::string(t) + " ; attendu 131071 des deux cotes");
	}

	// ---- 8. MVOL N'EST PAS DANS LA PUCE, et l'interface ne sait pas le lire
	// `case 0x0: m_gain = MVOL() / 15.0;` -- et c'est tout : m_gain n'est utilise nulle part dans
	// DoMasterSamples. Il est seulement EXPOSE par `output_gain()`, que la carte lit
	// (src/sound/soundboard.cpp, ligne du melange) pour multiplier elle-meme. Le volume maitre est donc
	// AU-DESSUS du chip, comme les deux volumes par groupe et le niveleur d'effets que le bloc 7 de
	// l'architecture range deja dans la carte.
	//
	// CE QUI EST UN TROU DANS L'INTERFACE, et ce test est l'endroit ou il est ecrit : scsp::core n'a aucun
	// moyen de rendre MVOL, si bien que la carte doit descendre par ->device() pour l'obtenir. C'est l'un
	// des ->device() que l'etape 3 a signales comme "ce qui doit partir en dernier". Un coeur autonome
	// devra donc, soit appliquer MVOL lui-meme -- ce qui changerait le son -- soit porter un accesseur de
	// plus. Tant que le choix n'est pas fait, le mesurer evite de le decouvrir en branchant.
	{
		const auto [plein, pd] = gain(7, 0, 0);
		(void) pd;
		Banc b;
		b.continu(ONDE, 256, CONTINU);
		b.coeur.write_reg(0x400 / 2, 0x0200 | 0x0000, 0xFFFF);   // MVOL = 0, le silence demande
		std::uint16_t mot[16] = {};
		melange_a_nu(mot);
		pose(mot, "DISDL", 7);
		pose(mot, "DIPAN", 0);
		b.keye(0, mot);
		const auto [muet_g, muet_d] = b.stereo(4);
		(void) muet_d;
		const float expose = banc::gain_expose(b.coeur);
		char t[144];
		snprintf(t, sizeof t, "MVOL=0 : la puce rend toujours %d (contre %d a MVOL=15), et expose un gain de %.3f",
		         muet_g, plein, double(expose));
		(muet_g == plein && expose == 0.0f)
			? ok("MVOL : expose, jamais applique dans la puce", t)
			: rate("MVOL : expose, jamais applique dans la puce",
			       std::string(t) + " ; attendu une sortie inchangee et un gain expose de 0");
	}

	// ---- 9. LES TROIS TABLES SE RECOUPENT, ce qui les valide l'une par l'autre
	// TL, DIPAN et SDLT sont trois tables separees dans le code, mais elles partagent des termes : -3, -6,
	// -12 et -24 dB apparaissent dans plusieurs. Si les trois sont justes, la MEME attenuation demandee par
	// trois registres differents doit rendre le MEME entier -- jusqu'a la derniere unite, puisque toutes
	// trois passent par le meme FIX et le meme decalage.
	//
	// C'est le seul controle de ce fichier qui ne repose sur aucune valeur enregistree : il tient tout seul,
	// et il attraperait une reecriture qui changerait une table sans toucher aux deux autres.
	{
		const auto [tl3,   x1] = gain(7, 0x00, 0x08);   // TL bit 3 : -3 dB
		const auto [pan3,  x2] = gain(7, 0x01, 0x00);   // DIPAN bit 0 : -3 dB
		const auto [tl24,  x3] = gain(7, 0x00, 0x40);   // TL bit 6 : -24 dB
		const auto [sdl24, x4] = gain(3, 0x00, 0x00);   // DISDL 3 : -24 dB
		(void) x1; (void) x2; (void) x3; (void) x4;
		char t[144];
		snprintf(t, sizeof t, "-3 dB : TL %d = DIPAN %d ; -24 dB : TL %d = DISDL %d", tl3, pan3, tl24, sdl24);
		(tl3 == pan3 && tl24 == sdl24)
			? ok("la meme attenuation par trois registres", t)
			: rate("la meme attenuation par trois registres",
			       std::string(t) + " ; les trois tables ne s'accordent plus");
	}

	printf("\n");
	if (echecs) { printf("%d echec(s).\n", echecs); return 1; }
	printf("tous les cas passent\n");
	return 0;
}
