// test_scsp_lfo : le pas de lecture, les deux LFO et la modulation par anneau.
//
// Bloc 4 de docs/scsp_architecture.md §3 -- "Pitch, LFO, modulation FM : OCT/FNS, les quatre formes de LFO,
// PLFOS/ALFOS, et la modulation par anneau (MDL/MDXSL/MDYSL)." Avec la consigne du document, qui est la
// moitie du sujet de ce fichier : "Les reglages de ce portage se gardent tels quels jusqu'a mesure
// contraire : profondeur FM a 24 et bit bas de MDL ignore (docs/scsp_audit.md §2)."
//
// DEUX SONDES, chacune exacte.
//
//   L'ACCUMULATEUR DE PHASE, pour tout ce qui touche au pas. Avec une rampe dont l'echantillon k vaut
//   1024*k, l'interpolation lineaire d'une droite est exacte, et la sortie gauche vaut
//       4 * (1024 * i + ((1024 * f) >> 12))  =  cur_addr - (f mod 4)
//   ou i et f sont la partie entiere et la partie fractionnaire de cur_addr. La sortie EST donc
//   l'accumulateur de phase, a trois pres -- et le pas se lit sur le deuxieme echantillon, sans compter
//   ni deviner. C'est la mesure que le bloc 2 ne pouvait pas faire : il lisait des indices, celui-ci lit
//   la phase elle-meme.
//
//   LE MULTIPLICATEUR D'AMPLITUDE, pour l'ALFO. Comme au bloc 3 : une onde continue a 8192, l'enveloppe
//   posee a plat (AR=0x1F, D1R=D2R=0, DL=0, donc 4096 des le deuxieme echantillon), et la sortie vaut huit
//   fois le multiplicateur de l'ALFO.
//
// ET LFOF SERT AUX DEUX LFO. Le champ est unique et les deux structures le recoivent, si bien qu'une
// question sur les cadences se repond par l'ALFO, qui n'a pas d'adresse et supporte donc les mesures
// longues.
//
// Le montage -- un coeur neuf par cas, la RAM, le keyage -- est dans tools/son/banc_scsp.h.
#include "banc_scsp.h"

#include <cstdio>
#include <set>
#include <string>
#include <vector>

namespace {

using banc::Banc;
using banc::pose;

int echecs = 0;

void ok(const char *quoi, const std::string &d) { printf("  ok    %-40s %s\n", quoi, d.c_str()); }
void rate(const char *quoi, const std::string &d) { printf("  ECHEC %-40s %s\n", quoi, d.c_str()); ++echecs; }

constexpr std::uint32_t ONDE = 0x1000;
constexpr int PAS_RAMPE = 1024;      // la rampe qui fait de la sortie l'accumulateur de phase
constexpr int CONTINU = 8192;        // l'onde plate qui fait de la sortie un gain
constexpr int PAR_MULT = 8;

// L'oscillateur a nu : pas d'enveloppe (SDIR), pleine sortie directe au centre.
void phase_a_nu(std::uint16_t mot[16])
{
	pose(mot, "SA_HI", ONDE >> 16);
	pose(mot, "SA_LO", ONDE & 0xFFFF);
	pose(mot, "SDIR",  1);
	pose(mot, "DISDL", 7);
	pose(mot, "DIPAN", 0);
	pose(mot, "LPCTL", 1);
	pose(mot, "LSA",   0);
	pose(mot, "LEA",   32);
	pose(mot, "KYONB", 1);
}

// L'enveloppe a plat : ce qui reste de variable est l'ALFO.
void gain_a_nu(std::uint16_t mot[16])
{
	pose(mot, "SA_HI", ONDE >> 16);
	pose(mot, "SA_LO", ONDE & 0xFFFF);
	pose(mot, "DISDL", 7);
	pose(mot, "DIPAN", 0);
	pose(mot, "TL",    0);
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

// Le pas de lecture, lu sur le deuxieme echantillon.
std::int32_t pas_mesure(Banc &b, unsigned fente = 0)
{
	(void) fente;
	const std::vector<std::int32_t> g = b.gauche(2);
	return g[1] - g[0];   // g[0] vaut zero, cur_addr partant de zero
}

// Les multiplicateurs de l'ALFO, A PARTIR DU DEUXIEME ECHANTILLON, et le premier est jete pour une raison
// qui se mesure : il passe par le chemin ATTACK, qui multiplie par le volume brut (4092) et non par la table
// (4096), si bien qu'un gain de 256 y sort a 255. Mais ce retrait DECALE TOUT D'UN CRAN par rapport aux
// index de la table du LFO, et la premiere version des cas ci-dessous cherchait ses reperes un cran trop
// loin. m[k] correspond donc a l'index (k + 2) modulo 256 de la table.
std::vector<int> gains(Banc &b, unsigned n)
{
	const std::vector<std::int32_t> g = b.gauche(n + 1);
	std::vector<int> m(n);
	for (unsigned i = 0; i < n; i++) m[i] = int(g[i + 1] / PAR_MULT);
	return m;
}

} // namespace

int main(int argc, char **argv)
{
	if (argc > 1) banc::nom_du_coeur() = argv[1];
	printf("test_scsp_lfo : le pas, les LFO et la modulation, par scsp::core\n\n");

	// ---- 1. LE PAS, par OCT et FNS, lu et non compte
	// `Step()` fait Fn = FNS + 1024 puis decale de `(OCT ^ 8) - 8 + SHIFT - 10`, soit l'octave plus deux.
	// OCT est donc un entier SIGNE sur quatre bits en representation biaisee par OU EXCLUSIF -- le champ
	// 0xF vaut -1, le champ 0x8 vaut -8 -- et une reecriture qui le lirait comme un non signe jouerait huit
	// octaves trop haut sur la moitie des notes.
	{
		struct Cas { unsigned oct, fns; std::int32_t pas; const char *quoi; };
		const Cas cas[] = {
			{ 0x0, 0x000, 4096, "OCT=0 FNS=0 : un echantillon par echantillon" },
			{ 0x0, 0x200, 6144, "FNS a mi-course : une fois et demie" },
			{ 0x0, 0x3FF, 8188, "FNS au maximum : presque le double" },
			{ 0x1, 0x000, 8192, "OCT=+1 : le double" },
			{ 0xF, 0x000, 2048, "OCT=-1 (champ 0xF) : la moitie" },
			{ 0xE, 0x000, 1024, "OCT=-2 : le quart" },
			{ 0x8, 0x000,   16, "OCT=-8 (champ 0x8) : le minimum" },
		};
		std::string lignes;
		bool juste = true;
		for (const Cas &c : cas) {
			Banc b;
			b.rampe(ONDE, 32, PAS_RAMPE);
			std::uint16_t mot[16] = {};
			phase_a_nu(mot);
			pose(mot, "OCT", c.oct);
			pose(mot, "FNS", c.fns);
			b.keye(0, mot);
			const std::int32_t p = pas_mesure(b);
			char t[64];
			snprintf(t, sizeof t, "%s%x/%03x->%d", lignes.empty() ? "" : " ", c.oct, c.fns, p);
			lignes += t;
			if (p != c.pas) { juste = false; lignes += "(attendu " + std::to_string(c.pas) + ")"; }
		}
		juste ? ok("le pas par OCT et FNS", lignes)
		      : rate("le pas par OCT et FNS", lignes);
	}

	// ---- 2. HUIT DES TRENTE-DEUX CADENCES DE LFO SONT MORTES
	// `phase_step = (u32)((float)(1 << LFO_SHIFT) * LFOFreq[LFOF] * 256.0f / 44100.0f)`, et la conversion
	// vers un entier non signe TRONQUE. Il faut donc LFOFreq >= 44100 / 65536 = 0,673 Hz pour que le pas
	// vaille au moins un, et les huit premieres entrees de la table (0,17 a 0,55 Hz) sont en dessous : leur
	// phase ne bouge jamais.
	//
	// Ce n'est pas un detail d'arrondi mais un quart du registre qui ne fait rien, et une reecriture qui
	// garderait une phase en virgule flottante -- ou qui arrondirait au lieu de tronquer -- ferait vibrer
	// huit cadences que ce portage laisse immobiles.
	//
	// QUATRE MILLE ECHANTILLONS ET NON SIX CENTS, et le chiffre est une mesure. La premiere version de ce
	// cas en lancait 600 et declarait mortes les cadences 0 a 11 : a phase_step = 1 l'index de table
	// n'avance que d'un tous les 256 echantillons, et m_ASCALES tronque, si bien que plusieurs index
	// voisins rendent le MEME entier. Un gain immobile ne prouvait donc pas une phase immobile. A 4096
	// echantillons l'index gagne seize crans, ce qui sort de la zone de troncature.
	{
		std::string mortes, vivantes;
		for (unsigned lfof = 0; lfof < 32; lfof++) {
			Banc b;
			b.continu(ONDE, 256, CONTINU);
			std::uint16_t mot[16] = {};
			gain_a_nu(mot);
			pose(mot, "LFOF", lfof);
			pose(mot, "ALFOWS", 0);     // dent de scie : toute phase qui bouge change le gain
			pose(mot, "ALFOS", 7);
			b.keye(0, mot);
			const std::vector<int> m = gains(b, 4096);
			bool bouge = false;
			for (int v : m) if (v != m[0]) { bouge = true; break; }
			(bouge ? vivantes : mortes) += (((bouge ? vivantes : mortes).empty()) ? "" : ",")
			                            + std::to_string(lfof);
		}
		(mortes == "0,1,2,3,4,5,6,7")
			? ok("LFOF 0 a 7 : la phase ne bouge pas", "mortes " + mortes + " ; vivantes a partir de 8")
			: rate("LFOF 0 a 7 : la phase ne bouge pas",
			       "mortes " + mortes + " (attendu 0,1,2,3,4,5,6,7) ; vivantes " + vivantes);
	}

	// ---- 3. LES QUATRE FORMES D'ALFO, une periode entiere
	// A LFOF=31 le pas de phase vaut exactement 256, donc l'index de table avance de un par echantillon :
	// 256 echantillons sont une periode complete, et on lit la table telle qu'elle est.
	//
	// MAIS DECALEE D'UN CRAN, et il faut le dire sous peine de croire la table fausse. PLFO_Step et
	// ALFO_Step incrementent la phase AVANT de lire : `phase += phase_step; p = table[phase >> 8]`. Le
	// premier echantillon lit donc l'index 1 et non 0, et la periode se parcourt 1,2,...,255,0. La phase
	// est un u16, ce qui la replie par le type -- c'est pourquoi le masque explicite de ces fonctions est
	// compile en dehors quand LFO_SHIFT vaut 8, et non parce qu'il serait inutile.
	//
	// Les quatre formes ne sont pas quatre variantes d'une meme chose : la dent de scie est monotone, le
	// carre n'a que DEUX valeurs, le triangle monte puis descend, et le bruit est tire d'un generateur
	// congruentiel a l'initialisation du peripherique. On verifie donc la FORME de chacune et non ses
	// valeurs, sauf aux deux bouts de la dent de scie, qui sont exacts.
	{
		auto formes = [](unsigned alfows) {
			Banc b;
			b.continu(ONDE, 256, CONTINU);
			std::uint16_t mot[16] = {};
			gain_a_nu(mot);
			pose(mot, "LFOF", 31);
			pose(mot, "ALFOWS", alfows);
			pose(mot, "ALFOS", 7);
			b.keye(0, mot);
			return gains(b, 256);
		};

		// dent de scie : a = 255 - i, donc l'attenuation DIMINUE. Le gain monte donc sur 255 echantillons
		// (index 1 a 255) puis RETOMBE au dernier, qui lit l'index 0 -- le repli de la periode, visible
		// parce que la phase est incrementee avant la lecture.
		{
			const std::vector<int> m = formes(0);
			bool monte = true;
			for (std::size_t i = 1; i <= 253; i++) if (m[i] < m[i - 1]) { monte = false; break; }
			(monte && m[0] == 256 && m[253] == 4096 && m[254] == 256)
				? ok("ALFOWS=0 dent de scie", "monotone de " + std::to_string(m[0]) + " a "
				     + std::to_string(m[253]) + " sur 254 echantillons, puis le repli a "
				     + std::to_string(m[254]))
				: rate("ALFOWS=0 dent de scie", "attendu 256 ... 4096 puis 256 ; obtenu "
				       + std::to_string(m[0]) + " ... " + std::to_string(m[253]) + " puis "
				       + std::to_string(m[254]) + (monte ? "" : ", et non monotone"));
		}
		// carre : deux valeurs, et rien entre les deux
		{
			const std::vector<int> m = formes(1);
			const std::set<int> v(m.begin(), m.end());
			(v.size() == 2 && *v.begin() == 256 && *v.rbegin() == 4096)
				? ok("ALFOWS=1 carre", "exactement deux valeurs, 256 et 4096")
				: rate("ALFOWS=1 carre", "attendu deux valeurs 256 et 4096 ; obtenu "
				       + std::to_string(v.size()) + " valeurs");
		}
		// triangle : l'attenuation descend de 255 a 0 sur la premiere moitie de la table puis remonte, donc
		// le GAIN monte jusqu'a l'index 128 et redescend. Un seul retournement, et le repli de periode tombe
		// hors des 256 echantillons mesures : deux sens et non trois.
		//
		// On compte EN IGNORANT LES PLATEAUX, parce que m_ASCALES tronque : deux index voisins rendent
		// souvent le meme entier, et une comparaison large comptait ces egalites comme des changements de
		// sens -- trente-deux au lieu de deux, sur une forme parfaitement triangulaire.
		{
			const std::vector<int> m = formes(2);
			std::vector<int> sens;
			for (std::size_t i = 1; i < m.size(); i++) {
				if (m[i] == m[i - 1]) continue;
				const int s = m[i] > m[i - 1] ? 1 : -1;
				if (sens.empty() || sens.back() != s) sens.push_back(s);
			}
			(sens.size() == 2 && sens[0] == 1 && sens[1] == -1)
				? ok("ALFOWS=2 triangle", "monte jusqu'au sommet puis redescend : deux sens, plateaux ignores")
				: rate("ALFOWS=2 triangle", std::to_string(sens.size())
				       + " changements de sens, attendu 2 (haut puis bas)");
		}
		// bruit : beaucoup de valeurs, et le meme tirage d'un coeur a l'autre (le generateur est seme)
		{
			const std::vector<int> m = formes(3), m2 = formes(3);
			const std::set<int> v(m.begin(), m.end());
			(v.size() > 100 && m == m2)
				? ok("ALFOWS=3 bruit", std::to_string(v.size())
				     + " valeurs distinctes, et identique d'un coeur a l'autre : le generateur est seme")
				: rate("ALFOWS=3 bruit", std::to_string(v.size()) + " valeurs distinctes"
				       + (m == m2 ? "" : ", et DEUX coeurs ne donnent pas le meme tirage"));
		}
	}

	// ---- 4. ALFOS : les huit profondeurs, en clair
	// ASCALE vaut {0, 0.4, 0.8, 1.5, 3, 6, 12, 24} dB, et le gain au creux du carre les encode. ALFOS=0
	// n'attenue rien DU TOUT -- et pour une raison de plus que la profondeur nulle : `if (ALFOS(slot) != 0)`
	// empeche l'appel, donc la phase de l'ALFO n'avance meme pas.
	{
		const int attendus[8] = { 4096, 3904, 3728, 3440, 2896, 2048, 1024, 256 };
		std::string lignes;
		bool juste = true;
		for (unsigned alfos = 0; alfos < 8; alfos++) {
			Banc b;
			b.continu(ONDE, 256, CONTINU);
			std::uint16_t mot[16] = {};
			gain_a_nu(mot);
			pose(mot, "LFOF", 31);
			pose(mot, "ALFOWS", 1);     // carre : le creux est une valeur franche
			pose(mot, "ALFOS", alfos);
			b.keye(0, mot);
			const std::vector<int> m = gains(b, 256);
			int creux = m[0];
			for (int v : m) if (v < creux) creux = v;
			char t[48];
			snprintf(t, sizeof t, "%s%u->%d", lignes.empty() ? "" : " ", alfos, creux);
			lignes += t;
			if (creux != attendus[alfos]) { juste = false; lignes += "(att " + std::to_string(attendus[alfos]) + ")"; }
		}
		juste ? ok("ALFOS : le creux par profondeur", lignes)
		      : rate("ALFOS : le creux par profondeur", lignes);
	}

	// ---- 5. PLFOS PERD SON BIT BAS, ET C'EST VOULU
	// `#define PLFOS(slot) ((slot->udata.data[0x9] >> 0x5) & 0x0006)` : le masque est 6 et non 7. C'est une
	// DIVERGENCE DELIBEREE d'avec MAME, reprise de Supermodel et reglee a l'oreille pour VF3
	// (docs/scsp_audit.md §2, et le commentaire de la macro) : les voix a PLFOS 1 perdaient leur vibrato,
	// et celles a 3, 5, 7 sortent comme 2, 4, 6.
	//
	// Ce cas existe pour que personne ne la "corrige" par distraction. Il ne dit pas qu'elle est juste --
	// il dit qu'elle est la, qu'elle a ete choisie, et quelle forme elle a : les champs vont par paires.
	//
	// ET LE CHAMP S'ECRIT A LA MAIN, non par scsp_contract::pose, parce que le masque 0x0006 du contrat est
	// un masque de LECTURE et non d'ecriture : le champ du registre fait trois bits (7-5), c'est
	// l'implementation qui en jette le bit bas. `pose(mot, "PLFOS", 1)` ne peut donc pas poser un 1, `1 & 6`
	// valant zero -- et un test qui s'en servirait ne mesurerait pas la divergence, il la reproduirait.
	// C'est le seul champ de la table dans ce cas (scsp_contract.h section 8).
	{
		std::string lignes;
		bool juste = true;
		std::int32_t pas[8] = {};
		for (unsigned plfos = 0; plfos < 8; plfos++) {
			Banc b;
			b.rampe(ONDE, 32, PAS_RAMPE);
			std::uint16_t mot[16] = {};
			phase_a_nu(mot);
			pose(mot, "OCT", 0);
			pose(mot, "FNS", 0);
			pose(mot, "LFOF", 31);
			pose(mot, "PLFOWS", 1);     // carre : au depart la phase est au sommet, le pas est constant
			mot[0x9] = std::uint16_t((mot[0x9] & ~(scsp_contract::PLFOS_CHAMP_LARGEUR << scsp_contract::PLFOS_CHAMP_DECALAGE))
			                         | ((plfos & scsp_contract::PLFOS_CHAMP_LARGEUR) << scsp_contract::PLFOS_CHAMP_DECALAGE));
			b.keye(0, mot);
			pas[plfos] = pas_mesure(b);
			char t[48];
			snprintf(t, sizeof t, "%s%u->%d", lignes.empty() ? "" : " ", plfos, pas[plfos]);
			lignes += t;
		}
		for (unsigned k = 0; k < 8; k += 2)
			if (pas[k] != pas[k + 1]) juste = false;
		const bool module = pas[2] != pas[0];
		(juste && module)
			? ok("PLFOS par paires : le bit bas est jete", lignes + " (0=1, 2=3, 4=5, 6=7)")
			: rate("PLFOS par paires : le bit bas est jete",
			       lignes + (module ? " : une paire differe" : " : aucune modulation, la sonde est muette"));
	}

	// ---- 6. LA MODULATION PAR ANNEAU : la profondeur est 24, et le bit bas de MDL est ignore
	// L'autre divergence deliberee, et la consigne du document porte sur elle : `smp >>= 0x18 - (MDL & 0xE)`
	// la ou MAME ecrit 0x1A et garde les quatre bits de MDL. Reglee a l'oreille sur le theme de l'arene de
	// Pai, ou une voix FM sortait terne (docs/scsp_audit.md §2).
	//
	// ON MESURE SANS AUCUNE VOIX MODULANTE. L'anneau est accessible depuis la fenetre de registres
	// (0x600-0x6FF, `m_RINGBUF[(addr - 0x600) / 2] = val`), donc on y POSE la valeur au lieu d'attendre
	// qu'une autre fente l'y ecrive -- et STWINH=1 empeche la fente mesuree de l'ecraser. Le decalage
	// d'adresse se lit alors directement : avec une rampe de pas 100 et cur_addr nul, la sortie vaut
	// 4 * 100 * le decalage en echantillons.
	{
		constexpr int V = 16384;            // la valeur posee dans l'anneau
		constexpr unsigned XSL = 1;         // l'entree lue, BUFPTR etant nul au premier echantillon
		// smp = ((V + V) / 2) << 10 >> (24 - (MDL & 0xE)), puis << 1 pour le 16 bits : le decalage en
		// echantillons vaut donc (V << 10) >> (24 - (MDL & 0xE)).
		const int attendus[10] = { 1, 1, 4, 4, 16, 16, 64, 64, 256, 256 };
		std::string lignes;
		bool juste = true;
		for (unsigned mdl = 0; mdl < 10; mdl++) {
			Banc b;
			b.rampe(ONDE, 400, 100);
			b.coeur.write_reg(0x300 + XSL, std::uint16_t(V), 0xFFFF);   // 0x600 + 2*XSL en octets
			std::uint16_t mot[16] = {};
			phase_a_nu(mot);
			pose(mot, "OCT", 0);
			pose(mot, "FNS", 0);
			pose(mot, "LEA", 400);
			pose(mot, "STWINH", 1);     // la fente n'ecrit pas dans l'anneau
			pose(mot, "MDL", mdl);
			pose(mot, "MDXSL", XSL);
			pose(mot, "MDYSL", XSL);
			b.keye(0, mot);
			const std::int32_t sortie = b.gauche(1)[0];
			const int decalage = int(sortie / (4 * 100));
			char t[48];
			snprintf(t, sizeof t, "%s%x->%d", lignes.empty() ? "" : " ", mdl, decalage);
			lignes += t;
			if (decalage != attendus[mdl] || sortie % (4 * 100) != 0) {
				juste = false;
				lignes += "(att " + std::to_string(attendus[mdl]) + ")";
			}
		}
		juste ? ok("MDL : profondeur 24, bit bas jete", lignes + " (les paires sont egales)")
		      : rate("MDL : profondeur 24, bit bas jete", lignes);
	}

	// ---- 7. LE GARDE DE LA MODULATION PORTE SUR TROIS CHAMPS, PAS SUR MDL
	// `if (MDL(slot) != 0 || MDXSL(slot) != 0 || MDYSL(slot) != 0)` : une fente dont MDL est nul module
	// quand meme si MDXSL ou MDYSL ne l'est pas -- c'est ce que montre MDL=0 au cas precedent, qui decale
	// d'un echantillon. Et les trois a zero ne modulent pas, meme avec l'anneau plein. Une reecriture qui
	// n'examinerait que MDL perdrait le premier comportement et ne s'en apercevrait sur aucun jeu qui
	// laisse MDXSL a zero.
	{
		Banc b;
		b.rampe(ONDE, 400, 100);
		b.coeur.write_reg(0x300, std::uint16_t(16384), 0xFFFF);   // l'anneau plein a l'entree 0
		std::uint16_t mot[16] = {};
		phase_a_nu(mot);
		pose(mot, "OCT", 0);
		pose(mot, "FNS", 0);
		pose(mot, "LEA", 400);
		pose(mot, "STWINH", 1);
		pose(mot, "MDL", 0);
		pose(mot, "MDXSL", 0);
		pose(mot, "MDYSL", 0);
		b.keye(0, mot);
		const std::int32_t sortie = b.gauche(1)[0];
		(sortie == 0) ? ok("MDL, MDXSL et MDYSL nuls : aucune modulation",
		                   "l'anneau vaut 16384 et la sortie reste a l'echantillon 0")
		              : rate("MDL, MDXSL et MDYSL nuls : aucune modulation",
		                     "sortie " + std::to_string(sortie) + ", attendu 0");
	}

	// ---- 8. LFORE : ce que ce coeur en fait, c'est-a-dire rien
	// Le champ existe dans le contrat (bit 15 du mot 9, "LFO reset") et le pilote peut l'ecrire, mais
	// scsp.cpp ne le lit NULLE PART : la macro LFORE est definie et jamais appelee. La phase du LFO n'est
	// donc remise a zero que par StartSlot, via Compute_LFO. On l'enregistre tel quel -- c'est une
	// difference d'avec le materiel qu'aucune mesure d'ici ne peut trancher, et la noter vaut mieux que la
	// laisser se decouvrir.
	{
		std::int32_t avec = 0, sans = 0;
		for (int i = 0; i < 2; i++) {
			Banc b;
			b.continu(ONDE, 256, CONTINU);
			std::uint16_t mot[16] = {};
			gain_a_nu(mot);
			pose(mot, "LFOF", 31);
			pose(mot, "ALFOWS", 0);
			pose(mot, "ALFOS", 7);
			pose(mot, "LFORE", unsigned(i));
			b.keye(0, mot);
			const std::vector<int> m = gains(b, 64);
			(i ? avec : sans) = m[63];
		}
		(avec == sans) ? ok("LFORE n'est lu nulle part dans ce coeur",
		                    "avec et sans, meme gain au 64e echantillon : " + std::to_string(avec))
		               : rate("LFORE n'est lu nulle part dans ce coeur",
		                      "avec " + std::to_string(avec) + ", sans " + std::to_string(sans)
		                      + " : il est donc lu quelque part, et ce test est a reecrire");
	}

	printf("\n");
	if (echecs) { printf("%d echec(s).\n", echecs); return 1; }
	printf("tous les cas passent\n");
	return 0;
}
