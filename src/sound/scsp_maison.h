// scsp_maison.h -- le SCSP du projet : une implementation de scsp::core qui n'est pas celle de MAME.
//
// Etape 3 de docs/scsp_migration_plan.md, et sa boucle : "ecrire le test -> ecrire le bloc -> le harnais dit
// identique -> mesurer le cout -> garder ; sinon : comprendre l'ecart, pas elargir la tolerance." Les sept
// tests de bloc existent depuis le 2026-10-26 et le rejoueur depuis le 2026-10-27 ; ce fichier est ce qu'ils
// attendaient.
//
// -----------------------------------------------------------------------------------------------------
// PROVENANCE, et il faut la lire avant le code.
//
// `src/sound/scsp.cpp` -- le coeur de MAME dont ce portage se sert encore -- est sous BSD-3-Clause, et le
// depot sous MIT : en DERIVER serait donc parfaitement licite, a condition de garder l'avis et la clause de
// non-garantie (docs/scsp_audit.md §2). Mais ce ne serait pas sortir de MAME : ce serait en avoir une copie
// de plus, portant la meme attribution, pour le meme objectif A du cahier des charges -- "eliminer
// progressivement le code tiers de MAME". Traduire n'elimine rien.
//
// Ce fichier est donc ecrit depuis `src/sound/scsp_contract.h` et depuis les sept tests de bloc, et non en
// transcrivant scsp.cpp. La structure et l'expression sont du projet.
//
// CE QUE CELA NE PRETEND PAS ETRE : une chambre blanche. J'ai lu scsp.cpp en long et en large pour ECRIRE
// ces tests et ce contrat, et on ne deslit pas un fichier. Ce qui est vrai, et verifiable, est plus modeste
// et plus utile : le COMPORTEMENT vise est celui de MAME, mesure et enregistre dans les tests -- un
// comportement ne s'approprie pas -- tandis que le code ci-dessous est ecrit a partir de ces mesures.
//
// LES TABLES MESUREES SONT UN CAS A PART, et le projet a deja tranche. ARTimes, DRTimes, LFOFreq, PSCALE,
// ASCALE, SDLT et la courbe d'attenuation ne sont pas des formules : ce sont des constantes relevees sur du
// materiel, que MAME a transcrites. docs/scsp_architecture.md §3 le dit du bloc 3 : "les reprendre est un
// choix de fidelite a assumer explicitement." Elles sont reprises, le choix est assume, et chaque table
// porte en commentaire d'ou elle vient. Le test du bloc 3 fige leurs consequences en echantillons, si bien
// qu'une reecriture qui les remplacerait par une courbe echouerait la et nulle part ailleurs.
// -----------------------------------------------------------------------------------------------------
//
// OU EN EST CE FICHIER. Un bloc a la fois, dans l'ordre que l'architecture donne -- les blocs verifiables
// sans le jeu d'abord. L'etat se lit dans `tools/son/test_*` : ce qui passe est fait.
//
//     bloc 1  registres et semantique d'acces .................. ECRIT
//     bloc 2  fentes, oscillateur, PCM, bouclage ............... a venir
//     bloc 3  enveloppes ADSR ................................. a venir
//     bloc 4  pitch, LFO, modulation .......................... a venir
//     bloc 5  le DSP d'effets ................................. a venir
//     bloc 6  timers, interruptions, synchronisation ........... a venir
//     bloc 7  melange et rendu ................................ a venir
//
// Tant qu'un bloc manque, le coeur rend ce qu'il peut et NON du bruit : sans voix il rend du silence exact,
// ce que le test du bloc 1 verifie. Un coeur qui rendrait n'importe quoi en attendant serait impossible a
// brancher pour mesurer.
#pragma once

#include "scsp_core.h"

#include <cstdint>

class scsp_maison final : public scsp::core {
public:
	using u8  = scsp::u8;
	using u16 = scsp::u16;
	using u32 = scsp::u32;
	using s32 = scsp::s32;
	using s16 = std::int16_t;

	// `ram` et `puits` doivent vivre plus longtemps que le coeur. `ram_octets` est la taille en OCTETS.
	scsp_maison(u32 horloge, u16 *ram, u32 ram_octets, scsp::sink &puits);

	void reset() override;
	u16  read_reg(u32 mot) override;
	void write_reg(u32 mot, u16 data, u16 masque) override;
	void run(u32 n, s32 *out) override;
	void timer_fired(int lequel) override;
	void midi_in(u8 octet) override;
	std::uint64_t samples_made() const override { return m_now; }

	// PAS L'AFFAIRE DE LA PUCE, et scsp_core.h dit pourquoi elle n'y est pas : le miroir de MADRS a
	// 0x7C0-0x7FF est un reglage du PORTAGE, vrai pour Virtua Fighter 3 et faux pour Fighting Vipers 2, ou
	// il replie toutes les lignes de retard du DSP sur une adresse (docs/scsp_audit.md §2, point 1). Il
	// vient donc par la classe concrete, comme chez l'adaptateur de MAME.
	// MVOL, le volume maitre, N'EST PAS APPLIQUE DANS LA PUCE : elle l'expose, et la carte multiplie
	// elle-meme (scsp_contract.h §13 point 7). C'est un trou dans scsp::core, pas une liberte -- un coeur
	// autonome doit donc rendre la meme chose que l'adaptateur de MAME, par la meme porte.
	float output_gain() const { return float(m_commun[0] & 0x000F) / 15.0f; }

	void set_madrs_mirror(bool on) { m_miroir = on; }
	bool madrs_mirror() const { return m_miroir; }

private:
	// --- la carte des registres, telle que le contrat la decoupe (scsp_contract.h §§1-2, 6)
	static constexpr u32 FENTES = 32, MOTS_PAR_FENTE = 16;
	static constexpr u32 COMMUN_MOTS = 0x30 / 2;     // 0x400-0x42F
	static constexpr u32 ANNEAU_MOTS = 64;           // 0x600-0x6FF
	static constexpr u32 COEF_MOTS = 64;             // 0x700-0x77F
	static constexpr u32 MADRS_MOTS = 32;            // 0x780-0x7BF
	static constexpr u32 CONTESTE_MOTS = 32;         // 0x7C0-0x7FF
	static constexpr u32 MPRO_MOTS = 512;            // 0x800-0xBFF

	u16 m_fente[FENTES][MOTS_PAR_FENTE] = {};

	// --- l'etat vivant d'une fente, qui n'est dans aucun registre
	static constexpr int PAS = 12;       // la virgule de l'adresse de lecture
	static constexpr int EG_PAS = 16;    // celle du volume d'enveloppe
	enum Etat { ATTAQUE, CHUTE1, CHUTE2, RELACHE };
	struct Vive {
		u32 cur = 0, nxt = 1u << PAS;    // l'adresse de lecture et la suivante, en virgule fixe
		u32 pas = 0;                     // le pas, calcule depuis OCT et FNS
		bool active = false;
		bool arriere = false;            // les bouclages 2 et 3 lisent a reculons
		int  etat = RELACHE;
		s32  volume = 0;
	};
	Vive m_vive[FENTES];
	u8 m_ptr_anneau = 0;                 // le curseur du tampon de modulation, un cran par fente

	// --- le generateur de bruit, pour SSCTL=1 et la forme de LFO 3.
	// Les constantes et la graine sont celles du coeur de MAME, et c'est une obligation et non un emprunt :
	// un bruit different rendrait le son different, et ce portage doit rester identique a l'octet. La suite
	// elle-meme est un generateur congruentiel standard (Numerical Recipes), pas une invention.
	u32 m_graine = 0x9d14abd7;
	u32 tirage() { m_graine = 1664525u * m_graine + 1013904223u; return (m_graine >> 16) | (m_graine << 16); }

	// --- les facteurs du melange, en flottant simple comme l'original
	//
	// TROIS PETITES TABLES ET UNE SEULE TRONCATURE, la ou le coeur de MAME garde deux tables de 0x10000
	// entiers (512 Ko). Le resultat doit etre le MEME entier, et c'est ce qui dicte la forme : l'original
	// fait `FIX(4.0f * LPAN * TL * fSDL)`, c'est-a-dire UNE troncature du produit. Trois tables d'entiers
	// multipliees entre elles tronqueraient trois fois et ne rendraient pas les memes valeurs que le test
	// du bloc 7 a enregistrees. On garde donc les facteurs en flottant et on ne tronque qu'a la fin.
	float m_f_tl[256] = {};              // TL : 0.4, 0.8, 1.5, 3, 6, 12, 24, 48 dB, un bit chacun
	float m_f_pan[32] = {};              // DIPAN/EFPAN : 3, 6, 12, 24 dB sur quatre bits
	float m_f_sdl[8] = {};               // DISDL/EFSDL : SDLT
	void construire_facteurs();
	// Le gain du melange pour un mot d'encodage, gauche et droite. `pan` est le champ de cinq bits : son
	// bit 4 choisit QUEL canal est attenue, l'autre restant a plein niveau.
	void gains(unsigned tl, unsigned pan, unsigned sdl, s32 &g, s32 &d) const;

	// --- la RAM son, vue comme la puce la voit
	u8  lire_octet_ram(u32 octet) const;
	u16 lire_mot_ram(u32 octet) const;

	// --- l'oscillateur
	s32 une_fente(unsigned i);
	void calculer_pas(unsigned i);
	void demarrer(unsigned i);
	void arreter(unsigned i, bool keyoff);

	u16 m_commun[COMMUN_MOTS] = {};
	s32 m_anneau[ANNEAU_MOTS] = {};                  // s32 : le tampon de modulation porte des echantillons
	u16 m_coef[COEF_MOTS] = {};
	u16 m_madrs[MADRS_MOTS] = {};
	u16 m_madrs_haut[CONTESTE_MOTS] = {};
	u16 m_mpro[MPRO_MOTS] = {};

	// Les deux registres d'interruption du cote maitre, que la LECTURE recopie depuis ces champs et non
	// l'inverse (scsp_contract.h §2 et §12).
	u16 m_mcieb = 0, m_mcipd = 0;

	// La file du MIDI entrant. Trente-deux octets, comme le contrat l'observe.
	static constexpr u32 MIDI_PROFONDEUR = 32;
	u8  m_midi[MIDI_PROFONDEUR] = {};
	u32 m_midi_e = 0, m_midi_l = 0;                  // ecriture, lecture

	// Les niveaux d'interruption, ASSEMBLES bit par bit depuis SCILV0/1/2 et recalcules seulement quand un
	// de ces trois mots est ecrit (scsp_contract.h §12 point 3).
	int m_niv_tima = 0, m_niv_timbc = 0, m_niv_midi = 0, m_niv_cpu = 0, m_niv_dma = 0;

	u8 m_mslc = 0;                                   // la fente surveillee, prise a l'ecriture de 0x408
	std::uint64_t m_now = 0;                         // echantillons rendus depuis reset()
	u32 m_horloge;
	u16 *m_ram;
	u32 m_ram_octets;
	scsp::sink &m_puits;
	bool m_miroir = true;

	// --- les acces, en octets, comme la fenetre de registres les voit
	u16  lire_octet_adresse(u32 octet);
	void ecrire_octet_adresse(u32 octet, u16 data, u16 masque);
	void apres_ecriture_commun(u32 bas);             // `bas` = octet & 0x3F
	void apres_ecriture_fente(u32 fente, u32 mot);
	void niveaux_depuis_scilv();
	u16  etat_fente_surveillee() const;              // ce que 0x408 rend, recalcule a chaque lecture
};
