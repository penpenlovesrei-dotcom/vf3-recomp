#include "scsp_maison.h"

#include "scsp_contract.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace {

// Un champ de fente, lu par la table du contrat : "SSCTL" plutot que "(mot 0 >> 7) & 3".
inline unsigned champ(const scsp::u16 mot[16], const char *nom)
{
	const scsp_contract::Champ *c = scsp_contract::champ_fente(nom);
	if (!c) return 0;
	const bool bit_en_place = c->decalage == 0 && (c->masque & (c->masque + 1)) != 0;
	return bit_en_place ? unsigned(mot[c->mot] & c->masque) != 0
	                    : unsigned((mot[c->mot] >> c->decalage) & c->masque);
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
	reset();
}

void scsp_maison::reset()
{
	for (auto &f : m_fente) for (auto &w : f) w = 0;
	for (auto &w : m_commun) w = 0;
	for (auto &w : m_anneau) w = 0;
	for (auto &w : m_coef) w = 0;
	for (auto &w : m_madrs) w = 0;
	for (auto &w : m_madrs_haut) w = 0;
	for (auto &w : m_mpro) w = 0;
	m_mcieb = m_mcipd = 0;
	m_midi_e = m_midi_l = 0;
	m_niv_tima = m_niv_timbc = m_niv_midi = m_niv_cpu = m_niv_dma = 0;
	m_mslc = 0;
	m_ptr_anneau = 0;
	m_graine = 0x9d14abd7;
	for (auto &v : m_vive) v = Vive();
	m_now = 0;
}

// -----------------------------------------------------------------------------------------------------
// LES ACCES
// -----------------------------------------------------------------------------------------------------

scsp_maison::u16 scsp_maison::etat_fente_surveillee() const
{
	// (bloc 1 : il n'y a pas encore de voix, donc l'etat est celui d'une fente au repos. Le test ne verifie
	// ici que le MASQUE -- qu'aucun bit ecrit ne se relise -- ce qui est vrai quelle que soit la valeur. Les
	// blocs 2 et 3 rempliront CA et EG ; la FORME du mot, elle, est deja celle du contrat.)
	const u32 ca = 0, sgc = 3 /* RELEASE */, eg = 0x1F;
	return u16((ca << 7) | (sgc << 5) | (eg & 0x1F));
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
	if (octet < 0x780) return m_coef[(octet - 0x700) / 2];
	if (octet < 0x7C0) return m_madrs[(octet - 0x780) / 2];
	if (octet < 0x800) {
		const u32 i = (octet - 0x7C0) / 2;
		return m_miroir ? m_madrs[i] : m_madrs_haut[i];
	}
	if (octet < 0xC00) return m_mpro[(octet - 0x800) / 2];
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
	if (octet < 0x780) { pose(m_coef[(octet - 0x700) / 2]); return; }
	if (octet < 0x7C0) { pose(m_madrs[(octet - 0x780) / 2]); return; }
	if (octet < 0x800) {
		// LA PLAGE CONTESTEE. Elle a son propre stockage, et le miroir -- un reglage du jeu et non de la
		// puce -- la recopie dans MADRS. Mesure dans les deux reglages par le test du bloc 1.
		const u32 i = (octet - 0x7C0) / 2;
		pose(m_madrs_haut[i]);
		if (m_miroir) m_madrs[i] = m_madrs_haut[i];
		return;
	}
	if (octet < 0xC00) { pose(m_mpro[(octet - 0x800) / 2]); return; }
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
	default:
		// Les timers (0x418-0x41C), SCIEB, SCIPD, SCIRE et le DMA (0x412-0x416) sont le bloc 6 : ils
		// s'ecrivent et se relisent deja, mais rien ne s'arme encore. Le test du bloc 1 ne demande que la
		// relecture ; celui du bloc 6 demandera le reste, et il echouera jusque-la.
		break;
	}
}

void scsp_maison::apres_ecriture_fente(u32 fente, u32 mot)
{
	// Le mot 8 porte OCT et FNS : le pas se recalcule a l'ecriture, et non seulement au demarrage.
	if (mot == 8) { calculer_pas(fente); return; }
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
		const bool kyonb = champ(m_fente[i], "KYONB") != 0;
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

void scsp_maison::demarrer(unsigned i)
{
	Vive &v = m_vive[i];
	v.active = true;
	v.cur = 0;
	v.nxt = 1u << PAS;
	v.arriere = false;
	calculer_pas(i);
	// L'enveloppe est le bloc 3. Ce qu'on en sait deja et qui se mesure : une voix NE PART PAS DU SILENCE,
	// son volume commence a 0x17F sur 1023 (scsp_contract.h §10 point 2). Tant que le bloc 3 n'est pas
	// ecrit, l'etat passe en ATTAQUE pour que la fente joue, et le gain vaut un.
	v.etat = ATTAQUE;
	v.volume = 0x17F << EG_PAS;
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
	const int octave = (int(champ(r, "OCT")) ^ 8) - 8 + PAS - 10;
	u32 fn = champ(r, "FNS") + (1u << 10);
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

	const unsigned ssctl = champ(r, "SSCTL");
	// SSCTL=3 SORT AVANT TOUT, et n'avance donc meme pas la tete de lecture -- contrairement a SSCTL=2, qui
	// rend aussi du silence mais parcourt la source. La difference ne se voit qu'apres (bloc 2, cas 8).
	if (ssctl == 3) return 0;

	u32 pas = v.pas;

	const bool pcm8 = champ(r, "PCM8B") != 0;
	u32 a1, a2;
	if (pcm8) { a1 = v.cur >> PAS; a2 = v.nxt >> PAS; }
	else      { a1 = (v.cur >> (PAS - 1)) & ~1u; a2 = (v.nxt >> (PAS - 1)) & ~1u; }

	// La modulation par anneau est le bloc 4 ; son GARDE porte sur trois champs et non sur MDL seul, donc il
	// est deja ecrit ici pour que le bloc 2 ne le contredise pas.
	const unsigned mdl = champ(r, "MDL"), mdxsl = champ(r, "MDXSL"), mdysl = champ(r, "MDYSL");
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
		const u32 sa = (champ(r, "SA_HI") << 16) | champ(r, "SA_LO");
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
	const unsigned sbctl = champ(r, "SBCTL");
	if (sbctl & 1) ech ^= 0x7FFF;
	if (sbctl & 2) ech = s32(s16(u16(ech) ^ 0x8000));

	// --- le pas, APRES la lecture
	if (v.arriere) v.cur -= pas; else v.cur += pas;
	v.nxt = v.cur + (1u << PAS);

	a1 = v.cur >> PAS;
	a2 = v.nxt >> PAS;

	const u32 lsa = champ(r, "LSA"), lea = champ(r, "LEA");
	const unsigned lpctl = champ(r, "LPCTL");

	// LPSLNK : l'attaque ne finit pas a la saturation du volume mais au passage de LSA (bloc 3).
	if (a1 >= lsa && !v.arriere && champ(r, "LPSLNK") && v.etat == ATTAQUE) v.etat = CHUTE1;

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

	// SDIR saute l'enveloppe ET l'ALFO (les deux sont le bloc 3 et le bloc 4). Sans SDIR et sans enveloppe
	// ecrite, le gain vaut un : c'est ce qui permet de mesurer le bloc 2 avant le bloc 3.
	if (!champ(r, "SDIR")) {
		// (bloc 3 : l'enveloppe. bloc 4 : l'ALFO.)
	}

	// L'anneau de modulation, que la fente alimente sauf si STWINH l'interdit. Le gain applique ici est
	// celui de TL a plein niveau de sortie, ou zero quand SDIR met TL de cote.
	if (!champ(r, "STWINH")) {
		s32 g, d;
		gains(champ(r, "SDIR") ? 0 : champ(r, "TL"), 0, 7, g, d);
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
	for (unsigned i = 0; i < 256; i++) {
		float db = 0.0f;
		for (unsigned b = 0; b < 8; b++) if (i & (1u << b)) db -= DB_TL[b];
		m_f_tl[i] = powf(10.0f, db / 20.0f);
	}
	static const float DB_PAN[4] = { 3.0f, 6.0f, 12.0f, 24.0f };
	for (unsigned i = 0; i < 32; i++) {
		float db = 0.0f;
		for (unsigned b = 0; b < 4; b++) if (i & (1u << b)) db -= DB_PAN[b];
		// 0x0F n'est PAS la somme des quatre bits : c'est une coupure en dur. Un cas particulier au milieu
		// d'une table par ailleurs additive, et une reecriture qui sommerait laisserait filtrer -45 dB.
		m_f_pan[i] = (i & 0xF) == 0xF ? 0.0f : powf(10.0f, db / 20.0f);
	}
	// SDLT : l'entree 0 est le SILENCE et non un niveau tres faible.
	static const float DB_SDL[8] = { 0.0f, -36.0f, -30.0f, -24.0f, -18.0f, -12.0f, -6.0f, 0.0f };
	for (unsigned i = 0; i < 8; i++) m_f_sdl[i] = i ? powf(10.0f, DB_SDL[i] / 20.0f) : 0.0f;
}

void scsp_maison::gains(unsigned tl, unsigned pan, unsigned sdl, s32 &g, s32 &d) const
{
	// LE FACTEUR QUATRE EST DANS LA TABLE, et c'est de la qu'il vient : le melange rend quatre fois
	// l'echantillon a plein niveau au centre. Ni l'adaptateur ni l'etage de sortie n'y sont pour rien.
	//
	// UNE SEULE TRONCATURE, du produit entier : c'est ce qui rend les memes entiers que l'original
	// (23196 a -3 dB, 2066 a -24 dB), la ou trois tables d'entiers multipliees tronqueraient trois fois.
	const float commun = 4.0f * float(1 << PAS) * m_f_tl[tl & 0xFF] * m_f_sdl[sdl & 7];
	// Le bit 4 de PAN choisit QUEL canal est attenue, l'autre restant a plein niveau : ce n'est pas une
	// balance qui glisse.
	const float p = m_f_pan[pan & 0x1F];
	if ((pan & 0x10) == 0) { g = s32(commun * p); d = s32(commun); }
	else                   { g = s32(commun);     d = s32(commun * p); }
}

void scsp_maison::run(u32 n, s32 *out)
{
	for (u32 e = 0; e < n; e++) {
		s32 gauche = 0, droite = 0;
		for (unsigned i = 0; i < FENTES; i++) {
			if (!m_vive[i].active) { m_ptr_anneau = (m_ptr_anneau + 1) & 63; continue; }
			const s32 ech = une_fente(i);
			const u16 *r = m_fente[i];
			// SDIR met TL a zero dans le melange aussi, et pas seulement l'enveloppe de cote : une fente
			// programmee avec SDIR et un grand TL serait autrement muette a tort (docs/scsp_audit.md §2,
			// point 6).
			s32 g, d;
			gains(champ(r, "SDIR") ? 0 : champ(r, "TL"), champ(r, "DIPAN"), champ(r, "DISDL"), g, d);
			gauche += (ech * g) >> PAS;
			droite += (ech * d) >> PAS;
			m_ptr_anneau = (m_ptr_anneau + 1) & 63;
		}
		// (bloc 5 : le DSP d'effets et la sortie EFSDL/EFPAN des seize premieres fentes.)

		// L'etage de sortie. DAC18B ne change pas le GAIN mais la RESOLUTION : sans lui les deux bits bas
		// de la somme sont jetes, le facteur quatre entre les deux denominateurs annulant le decalage.
		// L'ecretage est asymetrique, +131071 et non +131072, comme tout entier signe.
		const bool dac18b = (m_commun[0] & 0x0100) != 0;
		if (!dac18b) { gauche >>= 2; droite >>= 2; }
		const s32 borne = dac18b ? 131072 : 32768;
		gauche = std::clamp(gauche, -borne, borne - 1);
		droite = std::clamp(droite, -borne, borne - 1);
		if (!dac18b) { gauche *= 4; droite *= 4; }
		if (out) { out[e * 2] = gauche; out[e * 2 + 1] = droite; }
	}
	m_now += n;
}

void scsp_maison::timer_fired(int)
{
	// Bloc 6.
}

void scsp_maison::midi_in(u8 octet)
{
	// La file se remplit des maintenant : c'est le pendant de la lecture de 0x404, et les deux ensemble
	// font un comportement complet. Ce qui manque est l'interruption, qui demande les niveaux du bloc 6.
	m_midi[m_midi_e] = octet;
	m_midi_e = (m_midi_e + 1) & (MIDI_PROFONDEUR - 1);
	m_commun[0x20 / 2] = u16(m_commun[0x20 / 2] | 0x8);
}
