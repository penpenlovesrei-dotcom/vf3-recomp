#include "scsp_maison.h"

#include "scsp_contract.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {

// -----------------------------------------------------------------------------------------------------
// LIRE UN CHAMP DE FENTE, et c'est ici que la justesse et la vitesse ont failli s'exclure.
//
// La premiere version appelait scsp_contract::champ_fente(nom) DANS LA BOUCLE : une recherche par
// comparaison de chaines sur trente-quatre entrees, pour chacun des trente-huit champs lus, pour chacune
// des trente-deux fentes, quarante-quatre mille fois par seconde. C'etait juste -- le contrat restait la
// source unique des positions -- et le rejeu l'a confirme a l'octet. Mais la mesure a dit le prix : 47,80 s
// contre 21,05 s pour le coeur de MAME sur la meme trace, soit 2,27 fois plus lent, alors que le gain
// processeur est la priorite absolue du cahier des charges.
//
// La forme ci-dessous garde la source unique SANS la payer : les positions sont lues dans la table du
// contrat UNE FOIS, a la premiere utilisation, et rangees dans des entiers. Un champ faux dans le contrat
// casse donc toujours les tests, et la boucle chaude ne fait plus qu'un decalage et un masque.
// -----------------------------------------------------------------------------------------------------
struct Position {
	unsigned mot = 0, decalage = 0, masque = 0;
	bool bit_en_place = false;
};

struct Positions {
	Position alfos;
	Position alfows;
	Position ar;
	Position d1r;
	Position d2r;
	Position dipan;
	Position disdl;
	Position dl;
	Position efpan;
	Position efsdl;
	Position eghold;
	Position fns;
	Position imxl;
	Position isel;
	Position krs;
	Position kyonb;
	Position lea;
	Position lfof;
	Position lpctl;
	Position lpslnk;
	Position lsa;
	Position mdl;
	Position mdxsl;
	Position mdysl;
	Position oct;
	Position pcm8b;
	Position plfos;
	Position plfows;
	Position rr;
	Position sa_hi;
	Position sa_lo;
	Position sbctl;
	Position sdir;
	Position ssctl;
	Position stwinh;
	Position tl;

	Positions()
	{
		auto depuis = [](const char *nom) {
			Position p;
			const scsp_contract::Champ *c = scsp_contract::champ_fente(nom);
			if (!c) return p;
			p.mot = c->mot;
			p.decalage = c->decalage;
			p.masque = c->masque;
			p.bit_en_place = c->decalage == 0 && (c->masque & (c->masque + 1)) != 0;
			return p;
		};
		alfos = depuis("ALFOS");
		alfows = depuis("ALFOWS");
		ar = depuis("AR");
		d1r = depuis("D1R");
		d2r = depuis("D2R");
		dipan = depuis("DIPAN");
		disdl = depuis("DISDL");
		dl = depuis("DL");
		efpan = depuis("EFPAN");
		efsdl = depuis("EFSDL");
		eghold = depuis("EGHOLD");
		fns = depuis("FNS");
		imxl = depuis("IMXL");
		isel = depuis("ISEL");
		krs = depuis("KRS");
		kyonb = depuis("KYONB");
		lea = depuis("LEA");
		lfof = depuis("LFOF");
		lpctl = depuis("LPCTL");
		lpslnk = depuis("LPSLNK");
		lsa = depuis("LSA");
		mdl = depuis("MDL");
		mdxsl = depuis("MDXSL");
		mdysl = depuis("MDYSL");
		oct = depuis("OCT");
		pcm8b = depuis("PCM8B");
		plfos = depuis("PLFOS");
		plfows = depuis("PLFOWS");
		rr = depuis("RR");
		sa_hi = depuis("SA_HI");
		sa_lo = depuis("SA_LO");
		sbctl = depuis("SBCTL");
		sdir = depuis("SDIR");
		ssctl = depuis("SSCTL");
		stwinh = depuis("STWINH");
		tl = depuis("TL");
	}
};

const Positions &P()
{
	static const Positions p;
	return p;
}

inline unsigned champ(const scsp::u16 mot[16], const Position &p)
{
	return p.bit_en_place ? unsigned((mot[p.mot] & p.masque) != 0)
	                      : unsigned((mot[p.mot] >> p.decalage) & p.masque);
}

} // namespace


// -----------------------------------------------------------------------------------------------------
// BLOC 1 -- registres et semantique d'acces.
//
// Ce que le bloc doit rendre est enregistre dans tools/son/test_scsp_regs.cpp : pour chaque mot de 0x000 a
// 0x42F, ce qu'une ecriture LEVE et ce qui RESTE leve. La carte entiere se relit telle qu'elle s'ecrit sauf
// cinq mots, et ces cinq-la sont le bloc :
//
//     0x404  l'octet bas est la file du MIDI entrant : masque utile 0xFF00
//     0x408  MSLC/CA/SGC/EG, recalcule a chaque lecture : masque utile 0x0000
//     0x420  SCIPD : l'ecriture est un OU du seul bit 5, qui COLLE
//     0x42C  MCIPD : affectation, mais la lecture recopie l'etat interne -- le bit 5 revient
//     0x42E  MCIRE : recoit l'exception destinee a MCIPD, donc son bit 5 ne s'eteint jamais
// -----------------------------------------------------------------------------------------------------

scsp_maison::scsp_maison(u32 horloge, u16 *ram, u32 ram_octets, scsp::sink &puits)
	: m_horloge(horloge), m_ram(ram), m_ram_octets(ram_octets), m_puits(puits)
{
	construire_facteurs();
	construire_enveloppe();
	construire_lfo();
	reset();
}

void scsp_maison::reset()
{
	for (auto &f : m_fente) for (auto &w : f) w = 0;
	for (auto &w : m_commun) w = 0;
	for (auto &w : m_anneau) w = 0;
	for (auto &w : m_madrs_haut) w = 0;
	// COEF, MADRS et MPRO sont dans le DSP, que Init() remet a zero plus bas.
	m_mcieb = m_mcipd = 0;
	m_midi_e = m_midi_l = 0;
	m_niv_tima = m_niv_timbc = m_niv_midi = m_niv_cpu = m_niv_dma = 0;
	m_mslc = 0;
	m_ptr_anneau = 0;
	for (auto &c : m_compte) c = 0;
	for (auto &p : m_predivise) p = 1;
	m_graine = 0x9d14abd7;
	m_dsp.ram = m_ram;
	m_dsp.ram_bytes = m_ram_octets;
	m_dsp.Init();
	for (auto &v : m_vive) v = Vive();
	m_now = 0;
}

// -----------------------------------------------------------------------------------------------------
// LES ACCES
// -----------------------------------------------------------------------------------------------------

scsp_maison::u16 scsp_maison::etat_fente_surveillee() const
{
	// L'ETAT VIVANT DE LA FENTE SURVEILLEE, recalcule a chaque lecture : les quatre bits hauts de l'adresse
	// de lecture, l'etat de l'enveloppe, et le volume renverse.
	//
	// CE FUT UN BOUCHON, ET LE REJOUEUR L'A TROUVE. Pendant que seul le bloc 1 existait, cette fonction
	// rendait une constante -- une fente au repos -- et c'etait defendable : le test du bloc 1 ne verifie
	// ici que le MASQUE, qu'aucun bit ecrit ne se relise, ce qui est vrai quelle que soit la valeur. Mais
	// une fois les blocs 2 et 3 ecrits il y avait un vrai etat a rendre, et le bouchon est devenu faux sans
	// qu'aucun test unitaire ne bouge. C'est le rejeu d'une scene reelle qui l'a dit : "lecture 5292 du mot
	// 204 : 0040 contre 007f". Le pilote lit ce registre pour trouver une fente libre, donc un bouchon ici
	// change l'allocation des voix -- et le son, deux secondes plus tard, d'une facon qui ne ressemble plus
	// a sa cause.
	const Vive &v = m_vive[m_mslc & (FENTES - 1)];
	const u32 ca = (v.cur >> (PAS + 12)) & 0xF;
	const u32 sgc = u32(v.etat) & 3;
	const u32 eg = u32(0x1F - (v.volume >> (EG_PAS + 5))) & 0x1F;
	return u16((ca << 7) | (sgc << 5) | eg);
}

scsp_maison::u16 scsp_maison::lire_octet_adresse(u32 octet)
{
	octet &= 0xFFFF;

	if (octet < 0x400)
		return m_fente[octet / 0x20][(octet & 0x1F) / 2];

	if (octet < 0x430) {
		const u32 bas = octet & 0x3F;
		switch (bas) {
		case 0x04: case 0x05: {
			// L'octet bas EST la file du MIDI entrant, et la lecture la DEPILE -- c'est elle qui fait
			// retomber l'interruption quand la file se vide (scsp_contract.h §12 point 9). Une
			// implementation qui rendrait le mot stocke laisserait l'interruption levee pour toujours.
			u16 v = u16(m_commun[0x04 / 2] & 0xFF00);
			v = u16(v | m_midi[m_midi_l]);
			if (m_midi_l != m_midi_e) m_midi_l = (m_midi_l + 1) & (MIDI_PROFONDEUR - 1);
			if (m_midi_l == m_midi_e) {              // la file est vide : l'interruption retombe
				m_puits.irq(m_niv_midi, false);
				m_commun[0x20 / 2] = u16(m_commun[0x20 / 2] & ~0x8);
			}
			m_commun[0x04 / 2] = v;
			return v;
		}
		case 0x08: case 0x09:
			m_commun[0x08 / 2] = etat_fente_surveillee();
			return m_commun[0x08 / 2];
		case 0x2A: case 0x2B:
			m_commun[0x2A / 2] = m_mcieb;
			return m_mcieb;
		case 0x2C: case 0x2D:
			m_commun[0x2C / 2] = m_mcipd;
			return m_mcipd;
		default:
			return m_commun[bas / 2];
		}
	}

	if (octet < 0x600) return 0;                     // 0x430-0x5FF : rien n'y repond
	if (octet < 0x700) return u16(m_anneau[(octet - 0x600) / 2]);
	if (octet < 0x780) return u16(m_dsp.COEF[(octet - 0x700) / 2]);
	if (octet < 0x7C0) return m_dsp.MADRS[(octet - 0x780) / 2];
	if (octet < 0x800) {
		const u32 i = (octet - 0x7C0) / 2;
		return m_miroir ? m_dsp.MADRS[i] : m_madrs_haut[i];
	}
	if (octet < 0xC00) return m_dsp.MPRO[(octet - 0x800) / 2];

	// L'ETAT DU DSP SE RELIT, et c'est par la qu'on peut le specifier. TEMP, MEMS, MIXS et EFREG sont tous
	// exposes dans la fenetre de registres -- en lecture seule, l'ecriture au-dela de 0xC00 n'allant nulle
	// part. Les trois premiers portent plus de seize bits, donc chacun occupe DEUX mots : le bit 1 de
	// l'adresse choisit la moitie, le haut d'abord.
	//
	// Ce bout de carte manquait au bloc 1, et son test ne pouvait pas le dire : son balayage s'arrete a
	// 0x42F. C'est en cherchant par ou observer le DSP qu'il a fallu le completer.
	if (octet < 0xE00) {   // TEMP, 128 mots de 24 bits
		const s32 v = m_dsp.TEMP[(octet >> 2) & 0x7F];
		return u16((octet & 2) ? (u32(v) & 0xFFFF) : (u32(v) >> 16));
	}
	if (octet < 0xE80) {   // MEMS, 32 mots de 24 bits
		const s32 v = m_dsp.MEMS[(octet >> 2) & 0x1F];
		return u16((octet & 2) ? (u32(v) & 0xFFFF) : (u32(v) >> 16));
	}
	if (octet < 0xEC0) {   // MIXS, 16 mots de 20 bits
		const s32 v = m_dsp.MIXS[(octet >> 2) & 0xF];
		return u16((octet & 2) ? (u32(v) & 0xFFFF) : (u32(v) >> 16));
	}
	if (octet < 0xEE0) return u16(m_dsp.EFREG[(octet - 0xEC0) / 2]);
	if (octet < 0xEE4) return u16(m_dsp.EXTS[(octet - 0xEE0) / 2]);
	return 0;
}

void scsp_maison::ecrire_octet_adresse(u32 octet, u16 data, u16 masque)
{
	octet &= 0xFFFF;

	if (octet < 0x400) {
		const u32 fente = octet / 0x20, mot = (octet & 0x1F) / 2;
		m_fente[fente][mot] = u16((m_fente[fente][mot] & ~masque) | (data & masque));
		apres_ecriture_fente(fente, mot);
		return;
	}

	if (octet < 0x430) {
		const u32 bas = octet & 0x3F;
		// LES DEUX MOTS QUI S'ECRIVENT PAR UN OU, et non par une affectation : SCIPD et -- malgre ce que le
		// commentaire de MAME annonce -- MCIRE et non MCIPD (scsp_contract.h §2). Seul le bit 5 passe, et
		// comme c'est un OU, ecrire zero ne l'eteint pas.
		if (bas == 0x20 || bas == 0x2E) m_commun[bas / 2] = u16(m_commun[bas / 2] | (data & 0x20));
		else m_commun[bas / 2] = u16((m_commun[bas / 2] & ~masque) | (data & masque));
		apres_ecriture_commun(bas);
		return;
	}

	if (octet < 0x600) return;
	if (octet < 0x700) {
		s32 &r = m_anneau[(octet - 0x600) / 2];
		r = s32(s16((u16(r) & ~masque) | (data & masque)));
		return;
	}
	auto pose = [&](u16 &w) { w = u16((w & ~masque) | (data & masque)); };
	if (octet < 0x780) {
		s16 &c = m_dsp.COEF[(octet - 0x700) / 2];
		c = s16((u16(c) & ~masque) | (data & masque));
		return;
	}
	if (octet < 0x7C0) { pose(m_dsp.MADRS[(octet - 0x780) / 2]); return; }
	if (octet < 0x800) {
		// LA PLAGE CONTESTEE. Elle a son propre stockage, et le miroir -- un reglage du jeu et non de la
		// puce -- la recopie dans MADRS. Mesure dans les deux reglages par le test du bloc 1.
		const u32 i = (octet - 0x7C0) / 2;
		pose(m_madrs_haut[i]);
		if (m_miroir) m_dsp.MADRS[i] = m_madrs_haut[i];
		return;
	}
	if (octet < 0xC00) {
		pose(m_dsp.MPRO[(octet - 0x800) / 2]);
		// Le programme est DECODE une fois a l'ecriture et non a chaque echantillon : c'est une avance de ce
		// depot sur MAME et sur Supermodel, qui le decodent dans la boucle (docs/scsp_references.md).
		m_dsp.Decode((octet - 0x800) / 8);
		if (octet == 0xBF0) m_dsp.Start();
		return;
	}
}

scsp_maison::u16 scsp_maison::read_reg(u32 mot)
{
	return lire_octet_adresse(mot * 2);
}

void scsp_maison::write_reg(u32 mot, u16 data, u16 masque)
{
	// LIRE, MODIFIER, ECRIRE, et ce n'est pas un detail d'implementation : c'est ce que fait la puce telle
	// que ce portage la connait, et la LECTURE a des effets de bord. Une ecriture d'octet sur 0x404 depile
	// donc un octet de la file MIDI avant d'ecrire. Une implementation qui ferait une ecriture partielle
	// directe ne depilerait rien -- meme resultat dans le registre, etat different dans la puce.
	const u32 octet = mot * 2;
	u16 fusion = lire_octet_adresse(octet);
	fusion = u16((fusion & ~masque) | (data & masque));
	ecrire_octet_adresse(octet, fusion, 0xFFFF);
}

// -----------------------------------------------------------------------------------------------------
// CE QUI SUIT UNE ECRITURE
// -----------------------------------------------------------------------------------------------------

void scsp_maison::niveaux_depuis_scilv()
{
	// LE NIVEAU EST ASSEMBLE BIT PAR BIT, un bit par registre SCILV : bit 0 du niveau depuis SCILV0, bit 1
	// depuis SCILV1, bit 2 depuis SCILV2, chacun pris au rang de la SOURCE (scsp_contract.h §12 point 3).
	// Et il n'est recalcule qu'ici -- armer SCIEB avant d'avoir ecrit SCILV laisse tous les niveaux a zero.
	const u16 l0 = m_commun[0x24 / 2], l1 = m_commun[0x26 / 2], l2 = m_commun[0x28 / 2];
	auto niveau = [&](unsigned bit) {
		return int(((l0 >> bit) & 1) | (((l1 >> bit) & 1) << 1) | (((l2 >> bit) & 1) << 2));
	};
	m_niv_midi  = niveau(3);    // SCIMID
	m_niv_dma   = niveau(4);    // SCIDMA
	m_niv_cpu   = niveau(5);    // SCIIRQ
	m_niv_tima  = niveau(6);    // SCITMA
	m_niv_timbc = niveau(7);    // SCITMB
}

void scsp_maison::apres_ecriture_commun(u32 bas)
{
	switch (bas) {
	case 0x08: case 0x09:
		// MSLC, la fente surveillee : bits 15-11. Ce portage la prend AU MOMENT DE L'ECRITURE et non une
		// fois par echantillon comme MAME, parce que le pilote du Model 3 ecrit MSLC et relit aussitot pour
		// trouver une fente libre -- verrouillee par echantillon, il relit la fente d'avant et la musique
		// finit sur la moitie des fentes (le commentaire de scsp.cpp le dit, et c'est une divergence voulue).
		m_mslc = u8((m_commun[0x08 / 2] >> 11) & 0x1F);
		break;
	case 0x02: case 0x03:
		// La ligne de retard du DSP : sa taille et son origine dans la RAM son.
		m_dsp.RBL = (8 * 1024) << ((m_commun[0x02 / 2] >> 7) & 3);
		m_dsp.RBP = m_commun[0x02 / 2] & 0x3F;
		break;
	case 0x2A: case 0x2B:
		m_mcieb = m_commun[0x2A / 2];
		break;
	case 0x2C: case 0x2D:
		// MCIPD s'ecrit par affectation -- mais le bit 5 ecrit demande une interruption du cote maitre, et
		// comme la lecture recopie m_mcipd, le bit revient. C'est le troisieme chemin des trois que le bloc
		// 1 a mesures, et il n'est pas le meme que celui de SCIPD ni celui de MCIRE.
		if (m_commun[0x2C / 2] & 0x20) m_mcipd = u16(m_mcipd | 0x20);
		break;
	case 0x2E: case 0x2F:
		// MCIRE acquitte : les bits ecrits s'effacent de m_mcipd.
		m_mcipd = u16(m_mcipd & ~m_commun[0x2E / 2]);
		break;
	case 0x24: case 0x25: case 0x26: case 0x27: case 0x28: case 0x29:
		niveaux_depuis_scilv();
		break;
	case 0x16: case 0x17:
		if (m_commun[0x16 / 2] & 0x1000) executer_dma();   // DEXE
		break;
	case 0x18: case 0x19: arme_timer(TIMER_A); break;
	case 0x1A: case 0x1B: arme_timer(TIMER_B); break;
	case 0x1C: case 0x1D: arme_timer(TIMER_C); break;
	case 0x1E: case 0x1F:
		verifier_attente();
		break;
	case 0x20: case 0x21:
		// SCIPD : seule une demande du CPU deja armee relance l'examen. L'ecriture elle-meme est un OU du
		// bit 5, faite plus haut.
		if (m_commun[0x1E / 2] & m_commun[0x20 / 2] & 0x20) verifier_attente();
		break;
	case 0x22: case 0x23:
		m_commun[0x20 / 2] = u16(m_commun[0x20 / 2] & ~m_commun[0x22 / 2]);
		acquitter_interruptions();
		break;
	default:
		break;
	}
}

void scsp_maison::apres_ecriture_fente(u32 fente, u32 mot)
{
	// TROIS MOTS SE RECALCULENT A L'ECRITURE, et pas seulement au demarrage. En oublier un laisse une voix
	// deja en cours sur des valeurs perimees -- ce qui ne s'entend pas tout de suite, et se voit un million
	// d'echantillons plus tard.
	if (mot == 8) { calculer_pas(fente); return; }     // OCT et FNS : le pas de lecture
	if (mot == 9) { calculer_lfo(fente); return; }     // LFOF, les formes, les profondeurs
	if (mot == 5) {
		// LPSLNK/KRS/DL/RR. Et le relachement est recalcule AVEC UN RATE DE BASE NUL, ce qui n'est pas ce
		// que fait le calcul du demarrage : celui-ci part de `octave + 2*KRS + bit de FNS`. L'incoherence
		// est dans l'original et elle est OBSERVABLE -- c'est elle que le rejeu a trouvee.
		//
		// La trace de la decouverte, parce qu'elle dit comment on trouve ces choses : le rejeu d'une scene
		// de 1500 images sortait UN echantillon faux sur 1 149 952, a -967 contre -968. Les tables etaient
		// toutes identiques (65536 entrees de panoramique, 1024 de courbe, 124 de temps : zero ecart). En
		// rejouant les deux coeurs cote a cote, un echantillon a la fois, l'ecart s'est date : echantillon
		// 894 568, fente 0, le VOLUME en relachement, l'adresse etant juste. Mon pas valait 353 548 et le
		// sien 422 293, soit l'index 61 au lieu de 62 -- un cran de rate. Avec RR=0x1F et une octave de -1,
		// `clamp(-1 + 62)` contre `clamp(0 + 62)` : ce crochet-ci manquait.
		const u16 *r = m_fente[fente];
		Vive &v = m_vive[fente];
		v.rr = m_pas_dr[std::clamp(2 * int(champ(r, P().rr)), 0, 63)];
		v.dl = s32(0x1F - champ(r, P().dl));
		return;
	}
	if (mot != 0) return;

	// KYONEX, LA GACHETTE, ET ELLE EST GLOBALE : elle declenche un balayage des TRENTE-DEUX fentes et non
	// de celle qu'on vient d'ecrire. Chaque fente qui porte KYONB et se trouve en RELACHE demarre ; chaque
	// fente qui ne le porte pas s'arrete. Puis le bit se rebaisse tout seul.
	//
	// ET UNE FENTE QUI JOUE N'EST PAS REDEMARREE, parce que la condition exige l'etat RELACHE : un second
	// KYONEX sur une voix en cours ne fait rien. Une reecriture qui redemarrerait a chaque gachette ferait
	// claquer toutes les notes repetees du jeu (scsp_contract.h §10 point 10).
	const scsp_contract::Champ *kyonex = scsp_contract::champ_fente("KYONEX");
	if (!kyonex || !(m_fente[fente][0] & kyonex->masque)) return;

	for (unsigned i = 0; i < FENTES; i++) {
		const bool kyonb = champ(m_fente[i], P().kyonb) != 0;
		if (kyonb && m_vive[i].etat == RELACHE) demarrer(i);
		else if (!kyonb) arreter(i, true);
	}
	m_fente[fente][0] = u16(m_fente[fente][0] & ~kyonex->masque);
}

// -----------------------------------------------------------------------------------------------------
// BLOC 2 -- l'oscillateur, l'adressage PCM et les quatre bouclages.
//
// Ce que le bloc doit rendre est enregistre dans tools/son/test_scsp_fentes.cpp, qui lit la suite exacte
// des INDICES d'echantillon que la tete de lecture parcourt. Les trois faits qui ne vont pas de soi, et que
// le contrat tient de la mesure et non d'une lecture (scsp_contract.h §9) :
//
//   LSA et LEA comptent des ECHANTILLONS, quand la lecture 16 bits compte des OCTETS.
//   La RAM se replie a 1 Mo, et une adresse de lecture perd son bit 0.
//   LE BOUCLAGE SE DECLENCHE UN ECHANTILLON TOT, parce qu'il examine DEUX adresses dont l'une est un
//   fantome : `nxt` est reaffecte a chaque echantillon, donc ce que la passe `nxt` ECRIT est perdu -- mais
//   ses effets de bord, le demi-tour et l'arret, survivent. Aucun echantillon au-dela de LEA n'est jamais
//   lu, et une reecriture qui n'appliquerait le bouclage qu'a `cur` decalerait les quatre modes.
// -----------------------------------------------------------------------------------------------------

// -----------------------------------------------------------------------------------------------------
// BLOC 3 -- l'enveloppe.
// -----------------------------------------------------------------------------------------------------

void scsp_maison::construire_enveloppe()
{
	// LES DEUX TABLES DE TEMPS, en millisecondes, relevees sur materiel. Les deux premieres entrees sont
	// un temps infini (la note ne monte ni ne descend jamais), et la table d'attaque finit par deux zeros
	// qui veulent dire "immediat" -- un cas particulier, pas une limite de la courbe.
	static const double MS_AR[64] = {
		100000, 100000, 8100, 6900, 6000, 4800, 4000, 3400, 3000, 2400, 2000, 1700, 1500,
		1200, 1000, 860, 760, 600, 500, 430, 380, 300, 250, 220, 190, 150, 130, 110, 95,
		76, 63, 55, 47, 38, 31, 27, 24, 19, 15, 13, 12, 9.4, 7.9, 6.8, 6.0, 4.7, 3.8, 3.4, 3.0, 2.4,
		2.0, 1.8, 1.6, 1.3, 1.1, 0.93, 0.85, 0.65, 0.53, 0.44, 0.40, 0.35, 0.0, 0.0 };
	static const double MS_DR[64] = {
		100000, 100000, 118200, 101300, 88600, 70900, 59100, 50700, 44300, 35500, 29600, 25300, 22200, 17700,
		14800, 12700, 11100, 8900, 7400, 6300, 5500, 4400, 3700, 3200, 2800, 2200, 1800, 1600, 1400, 1100,
		920, 790, 690, 550, 460, 390, 340, 270, 230, 200, 170, 140, 110, 98, 85, 68, 57, 49, 43, 34,
		28, 25, 22, 18, 14, 12, 11, 8.5, 7.1, 6.1, 5.4, 4.3, 3.6, 3.1 };

	m_pas_ar[0] = m_pas_dr[0] = m_pas_ar[1] = m_pas_dr[1] = 0;   // temps infini
	for (unsigned i = 2; i < 64; i++) {
		const double echelle = double(1 << EG_PAS);
		m_pas_ar[i] = MS_AR[i] != 0.0 ? s32((1023 * 1000.0) / (44100.0 * MS_AR[i]) * echelle)
		                              : s32(1024 << EG_PAS);
		m_pas_dr[i] = s32((1023 * 1000.0) / (44100.0 * MS_DR[i]) * echelle);
	}

	// LA COURBE D'ATTENUATION : 3/32 dB par pas, 95,9 dB de bout en bout. C'est elle que tous les etats
	// sauf l'attaque traversent.
	for (unsigned i = 0; i < 0x400; i++) {
		const float db = float(3 * (int(i) - 0x3FF)) / 32.0f;
		m_courbe_eg[i] = s32(powf(10.0f, db / 20.0f) * float(1 << PAS));
	}
}

// -----------------------------------------------------------------------------------------------------
// BLOC 4 -- les deux LFO.
// -----------------------------------------------------------------------------------------------------

void scsp_maison::construire_lfo()
{
	// LES QUATRE FORMES, et ce ne sont pas quatre variantes d'une meme chose : la dent de scie est
	// monotone, le carre n'a que deux valeurs, le triangle monte et descend, le bruit est tire d'un
	// generateur congruentiel A L'INITIALISATION -- donc fixe pour la vie du peripherique, et non retire a
	// chaque tour.
	for (int i = 0; i < 256; i++) {
		m_lfo_scie_a[i] = 255 - i;
		m_lfo_scie_p[i] = i < 128 ? i : i - 256;

		m_lfo_carre_a[i] = i < 128 ? 255 : 0;
		m_lfo_carre_p[i] = i < 128 ? 127 : -128;

		m_lfo_tri_a[i] = i < 128 ? 255 - (i * 2) : (i * 2) - 256;
		m_lfo_tri_p[i] = i < 64 ? i * 2 : (i < 128 ? 255 - i * 2 : (i < 192 ? 256 - i * 2 : i * 2 - 511));

		const int bruit = int(tirage() & 0xFF);
		m_lfo_bruit_a[i] = bruit;
		m_lfo_bruit_p[i] = 128 - bruit;
	}

	// LES DEUX ECHELLES DE PROFONDEUR, relevees elles aussi : le vibrato en CENTS, le tremolo en DECIBELS.
	static const float CENTS[8] = { 0.0f, 7.0f, 13.5f, 27.0f, 55.0f, 112.0f, 230.0f, 494.0f };
	static const float DECIBELS[8] = { 0.0f, 0.4f, 0.8f, 1.5f, 3.0f, 6.0f, 12.0f, 24.0f };
	for (int s = 0; s < 8; s++) {
		for (int i = -128; i < 128; i++)
			m_echelle_p[s][i + 128] = int(256.0f * powf(2.0f, ((CENTS[s] * float(i)) / 128.0f) / 1200.0f));
		for (int i = 0; i < 256; i++)
			m_echelle_a[s][i] = int(256.0f * powf(10.0f, ((-DECIBELS[s] * float(i)) / 256.0f) / 20.0f));
	}
}

void scsp_maison::calculer_lfo(unsigned i)
{
	const u16 *r = m_fente[i];
	Vive &v = m_vive[i];

	// LES TRENTE-DEUX CADENCES, en hertz -- et HUIT D'ENTRE ELLES NE FONT RIEN. Le pas de phase est
	// `(u32)(256 * f * 256 / 44100)`, et la conversion TRONQUE : il faut f >= 44100/65536 = 0,673 Hz pour
	// que le pas vaille au moins un. Les huit premieres entrees sont en dessous, donc leur phase ne bouge
	// jamais. Une reecriture qui garderait une phase en flottant, ou qui arrondirait, ferait vibrer huit
	// cadences que ce portage laisse immobiles (scsp_contract.h §11 point 2).
	static const float HZ[32] = {
		0.17f, 0.19f, 0.23f, 0.27f, 0.34f, 0.39f, 0.45f, 0.55f, 0.68f, 0.78f, 0.92f, 1.10f,
		1.39f, 1.60f, 1.87f, 2.27f, 2.87f, 3.31f, 3.92f, 4.79f, 6.15f, 7.18f, 8.60f, 10.8f,
		14.4f, 17.2f, 21.5f, 28.7f, 43.1f, 57.4f, 86.1f, 172.3f };
	const unsigned lfof = champ(r, P().lfof);
	const u32 pas = u32(float(1 << 8) * (HZ[lfof & 31] * 256.0f / 44100.0f));

	const unsigned plfos = champ(r, P().plfos), alfos = champ(r, P().alfos);
	if (plfos != 0) {
		const int *tables[4] = { m_lfo_scie_p, m_lfo_carre_p, m_lfo_tri_p, m_lfo_bruit_p };
		v.plfo.pas = pas;
		v.plfo.table = tables[champ(r, P().plfows) & 3];
		v.plfo.echelle = m_echelle_p[plfos & 7];
	}
	if (alfos != 0) {
		const int *tables[4] = { m_lfo_scie_a, m_lfo_carre_a, m_lfo_tri_a, m_lfo_bruit_a };
		v.alfo.pas = pas;
		v.alfo.table = tables[champ(r, P().alfows) & 3];
		v.alfo.echelle = m_echelle_a[alfos & 7];
	}
}

scsp_maison::s32 scsp_maison::pas_lfo(Lfo &l)
{
	l.phase = u16(l.phase + l.pas);          // u16 : le type replie la phase
	const int p = l.table[l.phase >> 8];
	return s32(l.echelle[p + 128]) << (PAS - 8);
}

scsp_maison::s32 scsp_maison::gain_lfo(Lfo &l)
{
	l.phase = u16(l.phase + l.pas);
	const int p = l.table[l.phase >> 8];
	return s32(l.echelle[p]) << (PAS - 8);
}

void scsp_maison::calculer_enveloppe(unsigned i)
{
	const u16 *r = m_fente[i];
	Vive &v = m_vive[i];

	// KRS=0xF VEUT DIRE ZERO, et non "ignorer KRS" : ni l'octave, ni le bit de FNS ne passent. Mesure a
	// OCT=2 : KRS=0 sature en 56 echantillons, KRS=0xF en 83 -- exactement le chiffre d'OCT=0, donc
	// l'octave est bien jetee et non reduite (scsp_contract.h §10 point 9).
	const unsigned krs = champ(r, P().krs);
	const int octave = (int(champ(r, P().oct)) ^ 8) - 8;
	const int rate = krs != 0xF ? octave + 2 * int(krs) + int((champ(r, P().fns) >> 9) & 1) : 0;
	auto pas_ar = [&](unsigned champ_valeur) {
		return m_pas_ar[std::clamp(rate + 2 * int(champ_valeur), 0, 63)];
	};
	auto pas_dr = [&](unsigned champ_valeur) {
		return m_pas_dr[std::clamp(rate + 2 * int(champ_valeur), 0, 63)];
	};

	// UNE VOIX NE PART PAS DU SILENCE : 0x17F sur 1023, soit 37 % du chemin. Avec AR=0 l'attaque est
	// infinie et le multiplicateur reste a 383 << 2 = 1532, ce qui est la facon dont le test relit cette
	// constante (scsp_contract.h §10 point 2).
	v.volume = 0x17F << EG_PAS;
	v.ar  = pas_ar(champ(r, P().ar));
	v.d1r = pas_dr(champ(r, P().d1r));
	v.d2r = pas_dr(champ(r, P().d2r));
	v.rr  = pas_dr(champ(r, P().rr));
	v.dl  = s32(0x1F - champ(r, P().dl));   // INVERSE
	v.eghold = champ(r, P().eghold) != 0;
}

scsp_maison::s32 scsp_maison::avancer_enveloppe(unsigned i)
{
	Vive &v = m_vive[i];
	const u16 *r = m_fente[i];

	switch (v.etat) {
	case ATTAQUE:
		v.volume += v.ar;
		if (v.volume >= (0x3FF << EG_PAS)) {
			// LPSLNK ENFERME CETTE TRANSITION : avec lui l'etat ne change pas a la saturation, il reste
			// ATTAQUE et n'en sort que par l'oscillateur, au passage de LSA (bloc 2).
			if (!champ(r, P().lpslnk)) {
				v.etat = CHUTE1;
				// Un D1R assez grand saute CHUTE1 entierement.
				if (v.d1r >= (1024 << EG_PAS)) v.etat = CHUTE2;
			}
			v.volume = 0x3FF << EG_PAS;
		}
		// EGHOLD TIENT LA SORTIE, PAS LE VOLUME : le volume monte toujours en interne, mais la sortie est a
		// fond des le premier echantillon. Une attaque lente ne sert alors qu'a retarder CHUTE1.
		if (v.eghold) return 0x3FF << (PAS - 10);
		break;
	case CHUTE1:
		// LE TEST DE SORTIE PASSE APRES LA DECREMENTATION, donc CHUTE1 decremente TOUJOURS une fois, meme
		// quand DL=0 le fait ceder au premier tic : le palier vaut alors 4052 et non 4096. Une reecriture
		// qui testerait avant de decrementer passerait tous les autres cas (scsp_contract.h §10 point 4).
		v.volume -= v.d1r;
		if (v.volume <= 0) v.volume = 0;
		if ((v.volume >> (EG_PAS + 5)) <= v.dl) v.etat = CHUTE2;
		break;
	case CHUTE2:
		// C'EST LE REGISTRE QUI EST CONSULTE, pas le pas calcule : un D2R non nul peut donner un pas nul a
		// bas rate, et la sortie ne se figerait alors PAS, elle descendrait de zero par echantillon. Meme
		// resultat a l'oreille, pas le meme etat (scsp_contract.h §10 point 6).
		if (champ(r, P().d2r) == 0) return (v.volume >> EG_PAS) << (PAS - 10);
		v.volume -= v.d2r;
		if (v.volume <= 0) v.volume = 0;
		break;
	case RELACHE:
		v.volume -= v.rr;
		if (v.volume <= 0) {
			v.volume = 0;
			arreter(i, false);   // sans keyoff : la fente devient inactive pour de bon
		}
		break;
	default:
		return 1 << PAS;
	}
	return (v.volume >> EG_PAS) << (PAS - 10);
}

void scsp_maison::demarrer(unsigned i)
{
	Vive &v = m_vive[i];
	v.active = true;
	v.cur = 0;
	v.nxt = 1u << PAS;
	v.arriere = false;
	calculer_pas(i);
	calculer_enveloppe(i);
	calculer_lfo(i);
	v.etat = ATTAQUE;
}

void scsp_maison::arreter(unsigned i, bool keyoff)
{
	// Le key-off ne coupe pas le son : il passe l'enveloppe en RELACHE. La fente ne devient inactive que
	// quand le volume atteint zero, et c'est alors un arret SANS keyoff qui l'eteint (bloc 3).
	if (keyoff) m_vive[i].etat = RELACHE;
	else m_vive[i].active = false;
	m_fente[i][0] = u16(m_fente[i][0] & ~0x800);   // KYONB retombe dans les deux cas
}

void scsp_maison::calculer_pas(unsigned i)
{
	// OCT EST UN ENTIER SIGNE SUR QUATRE BITS, en representation biaisee par OU EXCLUSIF : `(OCT ^ 8) - 8`,
	// donc le champ 0xF vaut -1 et le champ 0x8 vaut -8. Une implementation qui le lirait comme un non signe
	// jouerait huit octaves trop haut sur la moitie des notes (scsp_contract.h §11 point 1).
	const u16 *r = m_fente[i];
	const int octave = (int(champ(r, P().oct)) ^ 8) - 8 + PAS - 10;
	u32 fn = champ(r, P().fns) + (1u << 10);
	m_vive[i].pas = octave >= 0 ? (fn << octave) : (fn >> -octave);
}

scsp_maison::u8 scsp_maison::lire_octet_ram(u32 octet) const
{
	// Le repli a 1 Mo, et l'ordre gros-boutien : l'octet 0 est l'octet HAUT du mot 0, comme le 68000 qui
	// remplit cette RAM.
	octet &= 0xFFFFF;
	if (octet >= m_ram_octets) return 0;
	return u8(m_ram[octet >> 1] >> ((octet & 1) ? 0 : 8));
}

scsp_maison::u16 scsp_maison::lire_mot_ram(u32 octet) const
{
	// `& 0xFFFFE` : le repli a 1 Mo ET la perte du bit 0. Les deux sont observables (bloc 2, cas 7).
	octet &= 0xFFFFE;
	if (octet >= m_ram_octets) return 0;
	return m_ram[octet >> 1];
}

scsp_maison::s32 scsp_maison::une_fente(unsigned i)
{
	Vive &v = m_vive[i];
	const u16 *r = m_fente[i];

	const unsigned ssctl = champ(r, P().ssctl);
	// SSCTL=3 SORT AVANT TOUT, et n'avance donc meme pas la tete de lecture -- contrairement a SSCTL=2, qui
	// rend aussi du silence mais parcourt la source. La difference ne se voit qu'apres (bloc 2, cas 8).
	if (ssctl == 3) return 0;

	u32 pas = v.pas;
	// LE PLFO MODULE LE PAS, et son champ PLFOS perd son bit bas -- divergence deliberee reprise de
	// Supermodel et reglee a l'oreille pour VF3 : les champs vont par paires, 0 avec 1, 2 avec 3, 4 avec 5,
	// 6 avec 7 (docs/scsp_audit.md §2 et scsp_contract.h §11 point 5). Le garde est `!= 0` sur le champ
	// DEJA masque, donc PLFOS=1 ne module pas du tout.
	if (champ(r, P().plfos) != 0) {
		pas = u32((std::uint64_t(pas) * u32(pas_lfo(v.plfo))) >> PAS);
	}

	const bool pcm8 = champ(r, P().pcm8b) != 0;
	u32 a1, a2;
	if (pcm8) { a1 = v.cur >> PAS; a2 = v.nxt >> PAS; }
	else      { a1 = (v.cur >> (PAS - 1)) & ~1u; a2 = (v.nxt >> (PAS - 1)) & ~1u; }

	// La modulation par anneau est le bloc 4 ; son GARDE porte sur trois champs et non sur MDL seul, donc il
	// est deja ecrit ici pour que le bloc 2 ne le contredise pas.
	const unsigned mdl = champ(r, P().mdl), mdxsl = champ(r, P().mdxsl), mdysl = champ(r, P().mdysl);
	if (mdl != 0 || mdxsl != 0 || mdysl != 0) {
		s32 smp = (m_anneau[(m_ptr_anneau + mdxsl) & 63] + m_anneau[(m_ptr_anneau + mdysl) & 63]) / 2;
		smp <<= 0xA;
		// Profondeur 24 et bit bas de MDL jete : divergence deliberee, reprise de Supermodel et reglee a
		// l'oreille pour VF3 (docs/scsp_audit.md §2, point 2).
		smp >>= 0x18 - int(mdl & 0xE);
		if (!pcm8) smp <<= 1;
		a1 += u32(smp); a2 += u32(smp);
	}

	s32 ech = 0;
	if (ssctl == 0) {
		const u32 sa = (champ(r, P().sa_hi) << 16) | champ(r, P().sa_lo);
		const s32 frac = s32(v.cur & ((1u << PAS) - 1)), plein = 1 << PAS;
		if (pcm8) {
			// Un octet par echantillon, place dans les huit bits HAUTS : il n'est pas etire sur seize.
			const s32 p1 = s32(std::int8_t(lire_octet_ram(sa + a1)));
			const s32 p2 = s32(std::int8_t(lire_octet_ram(sa + a2)));
			ech = ((p1 << 8) * (plein - frac) + (p2 << 8) * frac) >> PAS;
		} else {
			const s32 p1 = s32(s16(lire_mot_ram(sa + a1)));
			const s32 p2 = s32(s16(lire_mot_ram(sa + a2)));
			ech = (p1 * (plein - frac) + p2 * frac) >> PAS;
		}
	} else if (ssctl == 1) {
		ech = s32(s16(tirage() & 0xFFFF));
	}

	// SBCTL : DEUX INVERSIONS QUI NE SE RESSEMBLENT PAS. Le bit 0 est un OU exclusif sur l'entier, SANS
	// troncature ; le bit 1 tronque a seize bits. Les deux ensemble donnent l'oppose a un pres : 1000 sort
	// a -1001, ce que le bloc 2 mesure.
	const unsigned sbctl = champ(r, P().sbctl);
	if (sbctl & 1) ech ^= 0x7FFF;
	if (sbctl & 2) ech = s32(s16(u16(ech) ^ 0x8000));

	// --- le pas, APRES la lecture
	if (v.arriere) v.cur -= pas; else v.cur += pas;
	v.nxt = v.cur + (1u << PAS);

	a1 = v.cur >> PAS;
	a2 = v.nxt >> PAS;

	const u32 lsa = champ(r, P().lsa), lea = champ(r, P().lea);
	const unsigned lpctl = champ(r, P().lpctl);

	// LPSLNK : l'attaque ne finit pas a la saturation du volume mais au passage de LSA (bloc 3).
	if (a1 >= lsa && !v.arriere && champ(r, P().lpslnk) && v.etat == ATTAQUE) v.etat = CHUTE1;

	// LE BOUCLAGE, SUR LES DEUX ADRESSES. Voir l'en-tete du bloc : la passe `nxt` ecrit dans le vide, mais
	// ses effets de bord sont ceux qui decalent les quatre modes d'un echantillon.
	for (int k = 0; k < 2; k++) {
		u32 &adresse = k ? v.nxt : v.cur;
		const u32 a = k ? a2 : a1;
		s32 reste;
		switch (lpctl) {
		case 0:   // sans boucle
			if (a >= lsa && a >= lea) { v.active = false; m_fente[i][0] = u16(m_fente[i][0] & ~0x800); }
			break;
		case 1:   // boucle normale
			if (a >= lea) {
				reste = s32(adresse - (lea << PAS));
				adresse = (lsa << PAS) + u32(reste);
			}
			break;
		case 2:   // boucle inverse
			if (a >= lsa && !v.arriere) {
				reste = s32(adresse - (lsa << PAS));
				adresse = (lea << PAS) - u32(reste);
				v.arriere = true;
			} else if ((a < lsa || (adresse & 0x80000000u)) && v.arriere) {
				reste = s32((lsa << PAS) - adresse);
				adresse = (lea << PAS) - u32(reste);
			}
			break;
		case 3:   // va-et-vient
			if (a >= lea) {
				reste = s32(adresse - (lea << PAS));
				adresse = (lea << PAS) - u32(reste);
				v.arriere = true;
			} else if ((a < lsa || (adresse & 0x80000000u)) && v.arriere) {
				reste = s32((lsa << PAS) - adresse);
				adresse = (lsa << PAS) + u32(reste);
				v.arriere = false;
			}
			break;
		}
	}

	if (m_dit) fprintf(stderr, "  diag.osc fente %2u : brut %7d  etat %d  volume %9d  alfos %u plfos %u cur %u\n",
	                   i, ech, v.etat, v.volume, champ(r, P().alfos), champ(r, P().plfos), v.cur);
	// SDIR SAUTE L'ENVELOPPE ET L'ALFO. C'est la porte par laquelle le bloc 2 se mesure sans le bloc 3, et
	// c'est aussi ce qui fait que TL est mis a zero plus bas : une fente en SDIR passe a cote de toute
	// l'attenuation (docs/scsp_audit.md §2, point 6).
	if (!champ(r, P().sdir)) {
		// L'ALFO ATTENUE AVANT L'ENVELOPPE, et son garde empeche aussi la phase d'avancer : avec ALFOS=0 la
		// fonction n'est pas appelee du tout.
		if (champ(r, P().alfos) != 0) ech = (ech * gain_lfo(v.alfo)) >> PAS;
		//
		// DEUX ARITHMETIQUES, ET C'EST LE FAIT CENTRAL DU BLOC 3. L'etat ATTAQUE multiplie par le VOLUME
		// BRUT -- lineairement -- quand tout le reste passe par la courbe en decibels. Les deux se
		// rejoignent presque : a saturation l'une rend 4092 et l'autre 4096, si bien que le premier
		// echantillon de toute voix sort quatre millemes plus bas que le suivant. Mesure, pas du bruit
		// d'arrondi (scsp_contract.h §10 point 1).
		if (v.etat == ATTAQUE) ech = (ech * avancer_enveloppe(i)) >> PAS;
		else {
			const s32 brut = avancer_enveloppe(i);
			if (m_dit) fprintf(stderr, "  diag.eg  fente %2u : eg %6d -> index %4d -> courbe %6d\n",
			                   i, brut, brut >> (PAS - 10), m_courbe_eg[brut >> (PAS - 10)]);
			ech = (ech * m_courbe_eg[brut >> (PAS - 10)]) >> PAS;
		}
	}

	// L'anneau de modulation, que la fente alimente sauf si STWINH l'interdit. Le gain applique ici est
	// celui de TL a plein niveau de sortie, ou zero quand SDIR met TL de cote.
	if (!champ(r, P().stwinh)) {
		s32 g, d;
		gains(champ(r, P().sdir) ? 0 : champ(r, P().tl), 0, 7, g, d);
		m_anneau[m_ptr_anneau] = s32(s16((ech * g) >> (PAS + 1)));
	}

	return ech;
}

// -----------------------------------------------------------------------------------------------------
// LE MELANGE, ce qu'il en faut pour qu'un echantillon sorte (le bloc 7 le completera)
// -----------------------------------------------------------------------------------------------------

void scsp_maison::construire_facteurs()
{
	// TROIS TABLES DE DECIBELS A LA SEGA, et aucune n'est une formule : chacune est une somme de termes
	// choisis bit par bit. Les trois premiers termes de TL cassent la regle du doublement -- 0,4 puis 0,8
	// puis 1,5 et non 1,6 -- ce qu'une reecriture "propre" arrondit sans le voir. Valeurs du contrat, et
	// consequences figees en entiers par tools/son/test_scsp_mix.cpp.
	static const float DB_TL[8] = { 0.4f, 0.8f, 1.5f, 3.0f, 6.0f, 12.0f, 24.0f, 48.0f };
	static const float DB_PAN[4] = { 3.0f, 6.0f, 12.0f, 24.0f };
	// L'entree 0 de SDLT est le SILENCE et non un niveau tres faible.
	static const float DB_SDL[8] = { 0.0f, -36.0f, -30.0f, -24.0f, -18.0f, -12.0f, -6.0f, 0.0f };

	float f_tl[256], f_pan[32], f_sdl[8];
	for (unsigned i = 0; i < 256; i++) {
		float db = 0.0f;
		for (unsigned b = 0; b < 8; b++) if (i & (1u << b)) db -= DB_TL[b];
		f_tl[i] = powf(10.0f, db / 20.0f);
	}
	for (unsigned i = 0; i < 32; i++) {
		float db = 0.0f;
		for (unsigned b = 0; b < 4; b++) if (i & (1u << b)) db -= DB_PAN[b];
		// 0x0F n'est PAS la somme des quatre bits : c'est une coupure en dur. Un cas particulier au milieu
		// d'une table par ailleurs additive, et une reecriture qui sommerait laisserait filtrer -45 dB.
		f_pan[i] = (i & 0xF) == 0xF ? 0.0f : powf(10.0f, db / 20.0f);
	}
	for (unsigned i = 0; i < 8; i++) f_sdl[i] = i ? powf(10.0f, DB_SDL[i] / 20.0f) : 0.0f;

	// LE FACTEUR QUATRE EST DANS LA TABLE, et c'est de la qu'il vient : le melange rend quatre fois
	// l'echantillon a plein niveau au centre. Ni l'adaptateur ni l'etage de sortie n'y sont pour rien.
	//
	// UNE SEULE TRONCATURE, du produit entier, et l'echelle EN DERNIER : c'est ce qui rend les memes
	// entiers que l'original (23196 a -3 dB, 2066 a -24 dB). Trois tables d'entiers multipliees
	// tronqueraient trois fois, et un ordre different ne tomberait pas toujours sur le meme entier.
	for (unsigned i = 0; i < 0x10000; i++) {
		const unsigned tl = i & 0xFF, pan = (i >> 8) & 0x1F, sdl = (i >> 13) & 7;
		const float p = (pan & 0x10) == 0 ? f_pan[pan] : 1.0f;
		m_gain[i] = s32(float(1 << PAS) * (4.0f * p * f_tl[tl] * f_sdl[sdl]));
	}
}

void scsp_maison::run(u32 n, s32 *out)
{
	// VF3_MAISON_DIAG=<indice> : un echantillon detaille terme par terme -- chaque fente, son echantillon
	// brut, son gain, son apport, puis les retours d'effets et le total. Ce n'est pas un reste de chantier :
	// c'est la sonde qui a trouve le decalage du relachement, et elle resservira. Les deux tests `m_dit` de
	// une_fente sont toujours faux en marche normale, donc predits, et la mesure ne les voit pas.
	static const char *diag_s = getenv("VF3_MAISON_DIAG");
	static const std::uint64_t diag = diag_s ? strtoull(diag_s, nullptr, 10) : ~0ull;

	for (u32 e = 0; e < n; e++) {
		s32 gauche = 0, droite = 0;
		const bool dit = (m_now + e) == diag;
		m_dit = dit;
		for (unsigned i = 0; i < FENTES; i++) {
			if (!m_vive[i].active) { m_ptr_anneau = (m_ptr_anneau + 1) & 63; continue; }
			const s32 ech = une_fente(i);
			const u16 *r = m_fente[i];
			// SDIR met TL a zero dans le melange aussi, et pas seulement l'enveloppe de cote : une fente
			// programmee avec SDIR et un grand TL serait autrement muette a tort (docs/scsp_audit.md §2,
			// point 6).
			s32 g, d;
			gains(champ(r, P().sdir) ? 0 : champ(r, P().tl), champ(r, P().dipan), champ(r, P().disdl), g, d);
			if (dit) fprintf(stderr, "diag fente %2u : ech %7d  g %6d  apport %7d  (tl %3u pan %2u sdl %u)\n",
			                 i, ech, g, (ech * g) >> PAS, champ(r, P().sdir) ? 0 : champ(r, P().tl),
			                 champ(r, P().dipan), champ(r, P().disdl));
			gauche += (ech * g) >> PAS;
			droite += (ech * d) >> PAS;
			// L'envoi au DSP : meme attenuation, mais par IMXL, et deux bits de plus.
			s32 ig, id;
			gains(champ(r, P().sdir) ? 0 : champ(r, P().tl), 0, champ(r, P().imxl), ig, id);
			m_dsp.SetSample((ech * ig) >> (PAS - 2), s32(champ(r, P().isel)), s32(champ(r, P().imxl)));
			m_ptr_anneau = (m_ptr_anneau + 1) & 63;
		}
		m_dsp.Step();

		// La sortie d'effets : les SEIZE PREMIERES fentes seulement, chacune portant le niveau et le
		// panoramique de son propre registre (EFSDL/EFPAN).
		for (unsigned i = 0; i < 16; i++) {
			const unsigned efsdl = champ(m_fente[i], P().efsdl);
			if (!efsdl) continue;
			s32 g, d;
			gains(0, champ(m_fente[i], P().efpan), efsdl, g, d);
			if (dit) fprintf(stderr, "diag efreg %2u : %6d  g %6d  apport %7d\n",
			                 i, m_dsp.EFREG[i], g, (s32(m_dsp.EFREG[i]) * g) >> PAS);
			gauche += (s32(m_dsp.EFREG[i]) * g) >> PAS;
			droite += (s32(m_dsp.EFREG[i]) * d) >> PAS;
		}

		// L'etage de sortie. DAC18B ne change pas le GAIN mais la RESOLUTION : sans lui les deux bits bas
		// de la somme sont jetes, le facteur quatre entre les deux denominateurs annulant le decalage.
		// L'ecretage est asymetrique, +131071 et non +131072, comme tout entier signe.
		const bool dac18b = (m_commun[0] & 0x0100) != 0;
		if (!dac18b) { gauche >>= 2; droite >>= 2; }
		const s32 borne = dac18b ? 131072 : 32768;
		gauche = std::clamp(gauche, -borne, borne - 1);
		droite = std::clamp(droite, -borne, borne - 1);
		if (!dac18b) { gauche *= 4; droite *= 4; }
		if (dit) fprintf(stderr, "diag total gauche %d\n", gauche);
		if (out) { out[e * 2] = gauche; out[e * 2 + 1] = droite; }
	}
	m_now += n;
}

// -----------------------------------------------------------------------------------------------------
// BLOC 6 -- les timers, les cinq lignes d'interruption et le DMA.
// -----------------------------------------------------------------------------------------------------

scsp_maison::u32 scsp_maison::duree_timer(u16 reg)
{
	// LE "+1" EST UNE DIVERGENCE DELIBEREE, et la troisieme du portage apres PLFOS et MDL : MAME ecrit la
	// meme chose SANS lui. C'est ce +1 qui a remis le tic de VF3 (timer A a 0xCE) a 882 Hz au lieu de 900,
	// corrigeant une musique 0,8 % trop vite contre Supermodel (scsp_contract.h §12 point 1).
	return (u32(255 - (reg & 0xFF)) << ((reg >> 8) & 7)) + 1;
}

void scsp_maison::arme_timer(int lequel)
{
	static const u32 OCTET[3] = { 0x18, 0x1A, 0x1C };
	const u16 reg = m_commun[OCTET[lequel] / 2];
	m_predivise[lequel] = 1u << ((reg >> 8) & 7);
	m_compte[lequel] = u16((reg & 0xFF) << 8);
	// UN COMPTE DE 255 N'ARME PAS. Ce n'est pas une duree nulle, c'est une absence d'armement : le puits ne
	// recoit rien du tout, et pas un timer(..., 1, 0).
	if ((reg & 0xFF) != 255) m_puits.timer(lequel, duree_timer(reg), 0);
}

void scsp_maison::verifier_attente()
{
	u16 attente = m_commun[0x20 / 2];
	const u16 armes = m_commun[0x1E / 2];
	if (m_midi_l != m_midi_e) {               // un octet attend dans la file : le bit 3 se pose
		m_commun[0x20 / 2] = u16(m_commun[0x20 / 2] | 8);
		attente = u16(attente | 8);
	}
	// SORTIE PREMATUREE : avec rien en attente, AUCUNE ligne n'est touchee. C'est ce qui distingue "rien ne
	// pend" de "quelque chose pend que rien n'arme" -- seul le second fait retomber la ligne 0, et la
	// premiere version du test du bloc 6 affirmait le contraire (scsp_contract.h §12 point 5).
	if (!attente) return;

	// UNE SEULE LIGNE A LA FOIS, dans un ordre fixe, et on SORT a la premiere qui pend et qui est armee.
	// Si A et B pendent tous deux, c'est le niveau de A qui monte et celui de B qu'on ne voit jamais.
	if (attente & armes & 0x20) { m_puits.irq(m_niv_cpu, true); return; }
	if ((attente & 0x40) && (armes & 0x40)) { m_puits.irq(m_niv_tima, true); return; }
	if ((attente & 0x80) && (armes & 0x80)) { m_puits.irq(m_niv_timbc, true); return; }
	if ((attente & 0x100) && (armes & 0x100)) { m_puits.irq(m_niv_timbc, true); return; }
	if ((attente & 0x8) && (armes & 0x8)) { m_puits.irq(m_niv_midi, true); return; }

	// LA CINQUIEME LIGNE. Ce n'est aucune des quatre sources, et c'est elle qui a fait redessiner
	// l'interface : scsp_core.h porte un niveau ET un etat parce que la puce lache quatre lignes
	// independamment, plus celle-ci.
	m_puits.irq(0, false);
}

void scsp_maison::acquitter_interruptions()
{
	const u16 remise = m_commun[0x22 / 2];
	if (remise & 0x40) m_puits.irq(m_niv_tima, false);
	if (remise & 0x180) m_puits.irq(m_niv_timbc, false);
	if (remise & 0x20) {
		m_commun[0x20 / 2] = u16(m_commun[0x20 / 2] & ~0x20);
		m_puits.irq(m_niv_cpu, false);
	}
	if (remise & 0x8) m_puits.irq(m_niv_midi, false);
	verifier_attente();

	// UN TIMER DEJA ECHU SE REMET AUSSITOT EN ATTENTE, son compteur etant reste a 0xFFFF. Le commentaire de
	// l'original l'attribue au materiel reel et note dans la meme phrase qu'un autre jeu n'est pas d'accord
	// -- c'est donc un CHOIX, enregistre comme tel (scsp_contract.h §12 point 6).
	if (m_compte[0] == 0xFFFF) m_commun[0x20 / 2] = u16(m_commun[0x20 / 2] | 0x40);
	if (m_compte[1] == 0xFFFF) m_commun[0x20 / 2] = u16(m_commun[0x20 / 2] | 0x80);
	if (m_compte[2] == 0xFFFF) m_commun[0x20 / 2] = u16(m_commun[0x20 / 2] | 0x100);
}

void scsp_maison::executer_dma()
{
	// DMEA : vingt bits, les seize bas depuis 0x412 et les quatre hauts depuis les bits 15-12 de 0x414.
	// DRGA : douze bits dans la fenetre de registres. DTLG : la longueur en OCTETS.
	u32 dmea = u32(m_commun[0x12 / 2] & 0xFFFE) | (u32(m_commun[0x14 / 2] & 0xF000) << 4);
	u32 drga = u32(m_commun[0x14 / 2] & 0x0FFE);
	const u32 dtlg = u32(m_commun[0x16 / 2] & 0x0FFE);
	const bool vers_la_ram = (m_commun[0x16 / 2] & 0x2000) != 0;   // DDIR
	const bool zeros = (m_commun[0x16 / 2] & 0x4000) != 0;         // DGATE

	// UN DMA NE PEUT PAS ECRASER SES PROPRES PARAMETRES, et seulement dans un sens : on les sauve quand la
	// destination est la fenetre de registres, puisqu'elle pourrait contenir ces registres-la.
	const bool proteger = !vers_la_ram;
	const u16 sauve[3] = { m_commun[0x12 / 2], m_commun[0x14 / 2], m_commun[0x16 / 2] };

	for (u32 k = 0; k < dtlg; k += 2) {
		if (vers_la_ram) {
			const u16 v = zeros ? u16(0) : lire_octet_adresse(drga);
			const u32 a = dmea & 0xFFFFE;
			if (a < m_ram_octets) m_ram[a >> 1] = v;
			dmea += 2;
			if (!zeros) drga += 2;
		} else {
			const u16 v = zeros ? u16(0) : lire_mot_ram(dmea);
			ecrire_octet_adresse(drga, v, 0xFFFF);
			drga += 2;
			if (!zeros) dmea += 2;
		}
	}

	if (proteger) {
		m_commun[0x12 / 2] = sauve[0];
		m_commun[0x14 / 2] = sauve[1];
		m_commun[0x16 / 2] = sauve[2];
	}
	m_commun[0x16 / 2] = u16(m_commun[0x16 / 2] & ~0x1000);   // DEXE se rebaisse tout seul
	if (m_commun[0x1E / 2] & 0x10) m_puits.irq(m_niv_dma, true);
}

void scsp_maison::timer_fired(int lequel)
{
	static const u32 OCTET[3] = { 0x18, 0x1A, 0x1C };
	static const u16 BIT[3] = { 0x40, 0x80, 0x100 };
	if (lequel < 0 || lequel > 2) return;

	m_compte[lequel] = 0xFFFF;
	m_commun[0x20 / 2] = u16(m_commun[0x20 / 2] | BIT[lequel]);
	// Le compteur recharge se relit : l'octet bas du registre du timer passe a 0xFF.
	m_commun[OCTET[lequel] / 2] = u16((m_commun[OCTET[lequel] / 2] & 0xFF00) | (m_compte[lequel] >> 8));

	verifier_attente();
	// LE TIMER A PEND AUSSI DU COTE MAITRE, B ET C NON. Rien dans VF3 ne lit MCIPD, mais uniformiser
	// changerait un etat observable pour rien (scsp_contract.h §12 point 7).
	if (lequel == TIMER_A) maitre_attente(0x40);
}

void scsp_maison::midi_in(u8 octet)
{
	// La file se remplit des maintenant : c'est le pendant de la lecture de 0x404, et les deux ensemble
	// font un comportement complet. Ce qui manque est l'interruption, qui demande les niveaux du bloc 6.
	m_midi[m_midi_e] = octet;
	m_midi_e = (m_midi_e + 1) & (MIDI_PROFONDEUR - 1);
	m_commun[0x20 / 2] = u16(m_commun[0x20 / 2] | 0x8);
	verifier_attente();
}
