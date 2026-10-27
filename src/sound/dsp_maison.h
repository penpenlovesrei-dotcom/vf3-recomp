// dsp_maison.h -- le DSP d'effets du projet : le bloc 5, ecrit contre sa specification.
//
// Bloc 5 de docs/scsp_architecture.md §3, et le dernier des sept. Les six autres ont ete ecrits depuis
// src/sound/scsp_contract.h, qui dit ce que la puce DOIT faire. Celui-ci ne pouvait pas l'etre : son jeu
// d'instructions n'existe nulle part ECRIT -- ni fiche technique, ni specification independante, seulement
// les implementations de MAME et de Supermodel, issues du meme travail de reverse engineering d'ElSemi
// (docs/scsp_references.md).
//
// LA SPECIFICATION A DONC ETE ECRITE D'ABORD, le 2026-10-27, et c'est contre elle que ce fichier existe :
// tools/son/test_scsp_dsp.cpp, huit cas qui posent chacun un programme minuscule et ENREGISTRENT ce que le
// DSP en fait, observes par la fenetre de registres (TEMP, MEMS, MIXS, EFREG) et par la RAM son.
//
// -----------------------------------------------------------------------------------------------------
// CE QUI EST VALIDE, ET PAR QUOI. Il faut le lire avant de se fier a ce fichier.
//
//   LES HUIT CAS DE LA SPECIFICATION couvrent la chaine d'entree memoire (MRD, IWT, IRA), le produit, les
//   quatre decalages, MWT, l'accumulation d'EWT et l'aller-retour PACK/UNPACK.
//
//   LE REJOUEUR couvre tout ce que le JEU exerce, et il le couvre a l'octet : 3 219 865 echantillons par
//   puce sur la scene entiere de fv2-fight.txt. C'est la vraie epreuve, et elle vaut mieux que n'importe
//   quel nombre de cas unitaires -- mais elle ne couvre que ce que les programmes de VF3 emploient.
//
//   QUATRE CHAMPS NE SONT VALIDES PAR AUCUNE MESURE, et c'est une mesure qui le dit : les programmes de ce
//   jeu laissent FRCL, ADRL, YRL et NEGB a zero sur toutes leurs arenes. Le compte vient de la sonde
//   `dspprog`, et il porte son temoin dans la meme ligne -- `frcl 0 adrl 0 yrl 0 negb 0` a cote de
//   `iwt 23 twt 39 mrd 23 mwt 10 ewt 5`, comptes par le meme code au meme moment
//   (docs/fv2-son-programme.md §4). Ces quatre-la sont donc ECRITS mais jamais EPROUVES : un autre jeu, ou
//   une arene qu'on n'a pas visitee, serait le premier a les essayer.
// -----------------------------------------------------------------------------------------------------
//
// L'ENCODAGE D'UNE INSTRUCTION, quatre mots de seize bits par pas. C'est une disposition materielle, au
// meme titre qu'une carte de registres :
//
//     mot 0 : TRA 14-8, TWT 7, TWA 6-0
//     mot 1 : XSEL 15, YSEL 14-13, IRA 11-6, IWT 5, IWA 4-0
//     mot 2 : TABLE 15, MWT 14, MRD 13, EWT 12, EWA 11-8, ADRL 7, FRCL 6, SHIFT 5-4,
//             YRL 3, NEGB 2, ZERO 1, BSEL 0
//     mot 3 : NOFL 15, COEF 14-9, MASA 6-2, ADREB 1, NXADR 0
//
// LES NOMS DES MEMBRES PUBLICS SONT CEUX DE L'AUTRE DSP, a dessein : le coeur du projet (scsp_maison) les
// atteint directement, et changer de DSP ne doit rien changer chez lui.
#pragma once

#include <cstdint>

class dsp_maison {
public:
	using u8  = std::uint8_t;
	using u16 = std::uint16_t;
	using u32 = std::uint32_t;
	using s16 = std::int16_t;
	using s32 = std::int32_t;
	using s64 = std::int64_t;

	// --- ce que la puce expose, et que la fenetre de registres lit ou ecrit
	u16 *ram = nullptr;              // la RAM son, en mots de 16 bits
	u32 ram_bytes = 0;
	u32 RBP = 0;                     // l'origine de la ligne de retard, en pas de 4 kmots
	u32 RBL = 8 * 1024;              // sa taille en mots

	s16 COEF[64] = {};
	u16 MADRS[32] = {};
	u16 MPRO[128 * 4] = {};
	s32 TEMP[128] = {};              // 24 bits signes
	s32 MEMS[32] = {};               // 24 bits signes
	s32 MIXS[16] = {};               // 20 bits signes
	s16 EXTS[2] = {};                // rien n'y est branche sur cette carte
	s16 EFREG[16] = {};              // la sortie d'effets

	void Init();
	void Start();                    // (re)decode tout le programme et trouve son dernier pas
	void Decode(int pas);            // un pas, decode a l'ecriture et non dans la boucle
	void SetSample(s32 echantillon, s32 sel, s32 mxl);
	void Step();                     // un echantillon

private:
	// Un pas decode. LE DECODAGE A L'ECRITURE est une avance de ce depot sur MAME et sur Supermodel, qui
	// extraient les champs a chaque echantillon : 112 pas fois 44 100 fois deux puces, pour un programme
	// qui ne change qu'entre deux arenes (docs/scsp_references.md).
	// UN MOT D'INDICATEURS, ET NON QUINZE BOOLEENS. Les champs d'un bit tiennent dans un entier lu une
	// fois, et la boucle teste des masques au lieu de charger quinze octets par pas. Mesure : le passage de
	// quinze booleens a ce mot a ramene le coeur de 22,93 s a ce qu'annonce le commentaire de Step.
	enum Indic : u32 {
		I_TWT = 1u << 0, I_XSEL = 1u << 1, I_IWT = 1u << 2, I_TABLE = 1u << 3,
		I_MWT = 1u << 4, I_MRD = 1u << 5, I_EWT = 1u << 6, I_ZERO = 1u << 7,
		I_BSEL = 1u << 8, I_NOFL = 1u << 9, I_ADREB = 1u << 10, I_NXADR = 1u << 11,
		// Les quatre que ce jeu n'exerce jamais, groupes pour qu'un seul test les ecarte toutes.
		I_FRCL = 1u << 12, I_ADRL = 1u << 13, I_YRL = 1u << 14, I_NEGB = 1u << 15,
		I_RARES = I_FRCL | I_ADRL | I_YRL | I_NEGB,
	};
	struct Pas {
		u32 f = 0;                   // les champs d'un bit
		u8  tra = 0, twa = 0, ira = 0, iwa = 0, ewa = 0, coef = 0, masa = 0;
		u8  shift = 0, ysel = 0;
	};
	Pas m_prog[128] = {};
	int m_dernier = 0;               // le nombre de pas a executer
	bool m_arrete = true;            // avant le premier Start, le DSP ne tourne pas

	// LE CORPS SPECIALISE SUR CE QUE LE PROGRAMME EMPLOIE REELLEMENT, et c'est une mesure qui l'exige :
	// sans lui le coeur du projet coutait 1 % d'un coeur de plus par puce que celui de MAME, soit un quart
	// de plus sur le son entier -- alors que le gain processeur est la priorite absolue du cahier des
	// charges. `m_rares` dit si UN SEUL pas du programme emploie FRCL, ADRL, YRL ou NEGB ; les programmes de
	// ce jeu n'en emploient aucun, donc la version maigre tourne, et les quatre tests disparaissent des
	// 112 pas que chaque echantillon execute.
	//
	// Il ne se defait jamais en cours de route : un programme reecrit pour cesser d'employer l'un des
	// quatre garde le corps complet jusqu'au Start suivant. Ca coute de la vitesse et rien d'autre.
	bool m_rares = false;
	template <bool RARES> void boucle();

	// L'etat vivant. ACC et SHIFTED sont deux registres distincts, et c'est tout le sujet : le shifter est
	// UN PAS EN RETARD sur l'accumulateur (voir Step).
	s32 m_acc = 0;
	s32 m_memval = 0;
	s32 m_frc_reg = 0, m_y_reg = 0, m_adrs_reg = 0;
	u32 m_dec = 0;                   // descend d'un par echantillon ; indexe TEMP et la ligne de retard

	u16 lire_mot(u32 octet) const { return octet < ram_bytes ? ram[octet >> 1] : 0; }
	void ecrire_mot(u32 octet, u16 v) { if (octet < ram_bytes) ram[octet >> 1] = v; }
};
