#include "dsp_maison.h"

#include "sound_prof.h"

#include <algorithm>

namespace {

// Le signe etendu depuis un bit donne. Le DSP travaille en 24 bits (et Y en 13) dans des entiers de 32.
//
// IL FAUT JETER LES BITS DU HAUT AVANT D'ETENDRE, et c'est tout l'interet de cette forme. La premiere
// version faisait `(v ^ m) - m`, qui est juste quand la valeur TIENT DEJA dans la largeur -- et faux sinon,
// parce qu'elle laisse passer les bits au-dessus. Or le cas le plus important lui en donne : `sext(ACC * 2,
// 24)` recoit jusqu'a 27 bits, ACC en portant 26.
//
// Le rejeu l'a trouve en 1 064 053 echantillons : EFREG[2] saturait a 0x8000 la ou il devait valoir zero.
// Un decalage a gauche puis un decalage arithmetique a droite fait les deux d'un coup.
inline dsp_maison::s32 sext(dsp_maison::s32 v, unsigned bits)
{
	const unsigned reste = 32 - bits;
	return dsp_maison::s32(dsp_maison::u32(v) << reste) >> reste;
}

// -----------------------------------------------------------------------------------------------------
// PACK et UNPACK : le format de la ligne de retard.
//
// UN FORMAT A EXPOSANT, et c'est une disposition de bits comme une carte de registres -- pas un
// algorithme a inventer. Un mot de 24 bits signes entre, seize sortent : signe en 15, exposant sur quatre
// bits en 14-11, mantisse sur onze bits. L'exposant est le nombre de bits de tete redondants, trouve en
// regardant ou le motif `val ^ (val << 1)` cesse d'etre constant -- c'est-a-dire ou le signe s'arrete de
// se repeter.
//
// LE CHEMIN DU RETOUR N'EST PAS SON INVERSE EXACT, et c'est voulu : au-dela de l'exposant 11 le bit de
// signe est recopie differemment. L'aller-retour perd donc de la precision -- 0x0100 revient a 0x00FF,
// 0x4000 a 0x4FFF, 0x7FFF a 0x67FE -- et ces trois nombres sont enregistres dans la specification.
// -----------------------------------------------------------------------------------------------------

dsp_maison::u16 empaquete(dsp_maison::s32 v)
{
	const int signe = (v >> 23) & 1;
	dsp_maison::u32 motif = dsp_maison::u32(v ^ (v << 1)) & 0xFFFFFF;
	int exposant = 0;
	for (int k = 0; k < 12; k++) {
		if (motif & 0x800000) break;
		motif <<= 1;
		exposant += 1;
	}
	if (exposant < 12) v = (v << exposant) & 0x3FFFFF;
	else               v <<= 11;
	v >>= 11;
	v &= 0x7FF;
	v |= signe << 15;
	v |= exposant << 11;
	return dsp_maison::u16(v);
}

dsp_maison::s32 depaquete(dsp_maison::u16 v)
{
	const int signe = (v >> 15) & 1;
	int exposant = (v >> 11) & 0xF;
	const int mantisse = v & 0x7FF;
	dsp_maison::s32 r = mantisse << 11;
	if (exposant > 11) {
		exposant = 11;
		r |= signe << 22;
	} else {
		r |= (signe ^ 1) << 22;
	}
	r |= signe << 23;
	r <<= 8;
	r >>= 8;
	r >>= exposant;
	return r;
}

} // namespace

void dsp_maison::Init()
{
	for (auto &v : COEF) v = 0;
	for (auto &v : MADRS) v = 0;
	for (auto &v : MPRO) v = 0;
	for (auto &v : TEMP) v = 0;
	for (auto &v : MEMS) v = 0;
	for (auto &v : MIXS) v = 0;
	for (auto &v : EXTS) v = 0;
	for (auto &v : EFREG) v = 0;
	for (auto &p : m_prog) p = Pas();
	m_dernier = 0;
	m_acc = m_memval = m_frc_reg = m_y_reg = m_adrs_reg = 0;
	m_dec = 0;
	RBP = 0;
	RBL = 8 * 1024;        // la valeur d'un RBL de zero
	m_arrete = true;       // tant que Start n'a pas eu lieu, rien ne tourne
}

void dsp_maison::Decode(int pas)
{
	if (pas < 0 || pas >= 128) return;
	const u16 *m = MPRO + pas * 4;
	Pas &p = m_prog[pas];
	p.tra   = u8((m[0] >> 8) & 0x7F);
	p.twa   = u8(m[0] & 0x7F);
	p.ysel  = u8((m[1] >> 13) & 3);
	p.ira   = u8((m[1] >> 6) & 0x3F);
	p.iwa   = u8(m[1] & 0x1F);
	p.ewa   = u8((m[2] >> 8) & 0xF);
	p.shift = u8((m[2] >> 4) & 3);
	p.coef  = u8((m[3] >> 9) & 0x3F);
	p.masa  = u8((m[3] >> 2) & 0x1F);

	u32 f = 0;
	if ((m[0] >> 7) & 1)  f |= I_TWT;
	if ((m[1] >> 15) & 1) f |= I_XSEL;
	if ((m[1] >> 5) & 1)  f |= I_IWT;
	if ((m[2] >> 15) & 1) f |= I_TABLE;
	if ((m[2] >> 14) & 1) f |= I_MWT;
	if ((m[2] >> 13) & 1) f |= I_MRD;
	if ((m[2] >> 12) & 1) f |= I_EWT;
	if ((m[2] >> 7) & 1)  f |= I_ADRL;
	if ((m[2] >> 6) & 1)  f |= I_FRCL;
	if ((m[2] >> 3) & 1)  f |= I_YRL;
	if ((m[2] >> 2) & 1)  f |= I_NEGB;
	if ((m[2] >> 1) & 1)  f |= I_ZERO;
	if (m[2] & 1)         f |= I_BSEL;
	if ((m[3] >> 15) & 1) f |= I_NOFL;
	if ((m[3] >> 1) & 1)  f |= I_ADREB;
	if (m[3] & 1)         f |= I_NXADR;
	p.f = f;
}

void dsp_maison::Start()
{
	m_arrete = false;
	// LE DERNIER PAS UTILE : le programme s'arrete au dernier pas non nul. Un programme entierement nul ne
	// fait donc rien du tout, ce que le premier cas de la specification verifie.
	int i = 127;
	for (; i >= 0; --i) {
		const u16 *m = MPRO + i * 4;
		if (m[0] || m[1] || m[2] || m[3]) break;
	}
	m_dernier = i + 1;
	for (int pas = 0; pas < 128; pas++) Decode(pas);
	// Refait a neuf sur tout le programme, et non accumule : c'est le seul moment ou l'on peut savoir si
	// les quatre controles rares sont employes quelque part.
	m_rares = false;
	for (int pas = 0; pas < m_dernier; pas++) if (m_prog[pas].f & I_RARES) m_rares = true;
}

void dsp_maison::SetSample(s32 echantillon, s32 sel, s32 mxl)
{
	(void) mxl;   // le niveau est deja applique par l'appelant, comme le melange du chip le fait
	MIXS[sel & 0xF] += echantillon;
}

void dsp_maison::Step()
{
	if (m_arrete) return;
	// Le corps sans les controles que ce programme n'emploie pas (dsp_maison.h : m_rares).
	if (m_rares) boucle<true>();
	else         boucle<false>();
}

template <bool RARES>
void dsp_maison::boucle()
{

	// EFREG REPART DE ZERO A CHAQUE ECHANTILLON. EWT ajoute, donc sans cette remise a zero la sortie
	// d'effets s'accumulerait sans fin.
	for (auto &v : EFREG) v = 0;

	const u32 dec = m_dec, rbl = RBL, rbp = RBP;
	const int dernier = m_dernier;

	for (int pas = 0; pas < dernier; ++pas) {
		const Pas &I = m_prog[pas];
		const u32 f = I.f;

		// --- l'entree
		s32 entree;
		if (I.ira <= 0x1F)      entree = MEMS[I.ira];
		else if (I.ira <= 0x2F) entree = MIXS[I.ira - 0x20] << 4;    // MIXS fait 20 bits
		else if (I.ira <= 0x31) entree = EXTS[I.ira - 0x30] << 8;    // EXTS en fait 16
		else return;                                                 // au-dela, le pas abandonne tout
		entree = sext(entree, 24);

		if ((f & I_IWT)) {
			// MEMVAL vient d'un MRD d'un pas PRECEDENT : la lecture memoire n'arrive pas dans le meme pas.
			MEMS[I.iwa] = m_memval;
			if (I.ira == I.iwa) entree = m_memval;
		}

		// --- les operandes. Le meme mot de TEMP alimente B et X : lu et etendu une fois.
		const s32 tempv = sext(TEMP[(I.tra + dec) & 0x7F], 24);

		s32 b = 0;
		if (!(f & I_ZERO)) {
			b = (f & I_BSEL) ? m_acc : tempv;
			if constexpr (RARES) if ((f & I_NEGB)) b = 0 - b;
		}
		const s32 x = (f & I_XSEL) ? entree : tempv;

		s32 y = 0;
		switch (I.ysel) {
		case 0: y = m_frc_reg; break;
		case 1: y = COEF[I.coef] >> 3; break;       // COEF fait 16 bits, Y en veut 13
		case 2: y = (m_y_reg >> 11) & 0x1FFF; break;
		case 3: y = (m_y_reg >> 4) & 0x0FFF; break;
		}
		if constexpr (RARES) if ((f & I_YRL)) m_y_reg = entree;

		// --- LE SHIFTER, ET IL EST UN PAS EN RETARD SUR L'ACCUMULATEUR.
		//
		// C'est le fait central de ce bloc, et la specification l'a sorti : `shifted` se calcule depuis
		// l'ACC du pas PRECEDENT, avant que celui-ci ne soit reecrit plus bas. Le pas qui fait un produit ne
		// peut donc pas en ecrire le resultat -- il faut le pas suivant pour que TWT, MWT, EWT, FRCL ou ADRL
		// le voient. Une implementation qui decalerait l'ACC fraichement calcule serait fausse partout, et
		// aucune lecture ne le crie : c'est l'ORDRE des lignes qui le dit.
		//
		// Les deux premiers decalages ECRETENT a 24 bits signes, les deux autres ETENDENT LE SIGNE -- donc
		// rebouclent. La difference entre saturer et claquer est tout l'interet du champ.
		s32 shifted = 0;
		switch (I.shift) {
		case 0: shifted = std::clamp<s32>(m_acc, -0x00800000, 0x007FFFFF); break;
		case 1: shifted = std::clamp<s32>(m_acc * 2, -0x00800000, 0x007FFFFF); break;
		case 2: shifted = sext(m_acc * 2, 24); break;
		case 3: shifted = sext(m_acc, 24); break;
		}

		// --- le produit, en 13 bits signes pour Y
		//
		// LA SOMME SE FAIT EN 64 BITS PUIS SE TRONQUE, et non l'inverse : tronquer le produit avant
		// d'ajouter B n'est pas la meme chose des que l'un des deux sort de la plage d'un entier de 32.
		y = sext(y, 13);
		const s64 produit = (s64(x) * s64(y)) >> 12;
		m_acc = s32(produit + b);

		if ((f & I_TWT)) TEMP[(I.twa + dec) & 0x7F] = shifted;

		if constexpr (RARES) if ((f & I_FRCL)) m_frc_reg = (I.shift == 3) ? (shifted & 0x0FFF) : ((shifted >> 11) & 0x1FFF);

		// --- la ligne de retard
		if ((f & I_MRD) || (f & I_MWT)) {
			u32 adresse = MADRS[I.masa];
			if (!(f & I_TABLE)) adresse += dec;
			if ((f & I_ADREB)) adresse += u32(m_adrs_reg) & 0x0FFF;
			if ((f & I_NXADR)) adresse++;
			// Sans TABLE, l'adresse tourne dans la ligne de retard ; avec, elle balaie 64 kmots.
			if (!(f & I_TABLE)) adresse &= rbl - 1;
			else          adresse &= 0xFFFF;
			adresse += rbp << 12;
			adresse <<= 1;
			// ET LES ACCES N'ONT LIEU QUE SUR UN PAS IMPAIR. Un MRD au pas 0 ne lit rien : c'est pourquoi
			// les programmes intercalent des pas vides.
			if (pas & 1) {
				if ((f & I_MRD)) m_memval = (f & I_NOFL) ? (s32(lire_mot(adresse)) << 8) : depaquete(lire_mot(adresse));
				if ((f & I_MWT)) ecrire_mot(adresse, (f & I_NOFL) ? u16(shifted >> 8) : empaquete(shifted));
			}
		}

		if constexpr (RARES) if ((f & I_ADRL)) m_adrs_reg = (I.shift == 3) ? ((shifted >> 12) & 0x0FFF) : (entree >> 16);

		// EWT AJOUTE, il n'affecte pas : plusieurs pas peuvent nourrir la meme sortie d'effets.
		if ((f & I_EWT)) EFREG[I.ewa] = s16(EFREG[I.ewa] + s16(shifted >> 8));
	}

	m_dec = dec - 1;
	if (sndprof::on) sndprof::dsp_steps += unsigned(dernier);
	// MIXS est vide a la FIN : les fentes le remplissent pour l'echantillon suivant.
	for (auto &v : MIXS) v = 0;
}
