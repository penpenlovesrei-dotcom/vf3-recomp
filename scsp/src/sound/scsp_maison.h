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
// OU EN EST CE FICHIER : LES SEPT BLOCS SONT ECRITS, et le coeur rend le meme son que celui de MAME A
// L'OCTET -- 3 219 865 echantillons par puce sur la scene entiere de fv2-fight.txt, PCM, interruptions,
// armements de timer et lectures de registre compris (tools/son/rejouer). L'etat de chaque bloc se lit
// aussi dans tools/son/test_* : ce qui passe est fait.
//
//     bloc 1  registres et semantique d'acces .................. ECRIT, 9 cas
//     bloc 2  fentes, oscillateur, PCM, bouclage ............... ECRIT, 11 cas
//     bloc 3  enveloppes ADSR ................................. ECRIT, 10 cas
//     bloc 4  pitch, LFO, modulation .......................... ECRIT, 11 cas
//     bloc 5  le DSP d'effets ................................. ECRIT, 8 cas
//     bloc 6  timers, interruptions, synchronisation ........... ECRIT, 11 cas
//     bloc 7  melange et rendu ................................ ECRIT, 9 cas
//
// LE BLOC 5 A ETE A PART, ET IL FAUT DIRE EN QUOI. Les six autres ont ete ecrits depuis scsp_contract.h,
// qui dit ce que la puce DOIT faire. Pour le DSP, le contrat ne decrit que la DISPOSITION -- COEF, MADRS,
// MPRO, 128 pas de 8 octets (§6) -- et pas la semantique d'une instruction. Et docs/scsp_references.md est
// clair sur l'etat des sources : le jeu d'instructions du DSP n'existe qu'IMPLEMENTE, chez MAME et chez
// Supermodel, tous deux issus du meme travail de reverse engineering d'ElSemi. Ni fiche technique, ni
// specification independante.
//
// Ecrire ce bloc "depuis le contrat" etait donc impossible, et le transcrire depuis scspdsp.cpp
// n'eliminerait rien -- une copie de plus, sous la meme BSD-3. LA SPECIFICATION A DONC ETE ECRITE D'ABORD,
// le 2026-10-27 : tools/son/test_scsp_dsp.cpp, huit cas qui posent chacun un programme minuscule et
// ENREGISTRENT ce que le DSP en fait, observes par la fenetre de registres (TEMP, MEMS, MIXS, EFREG) et par
// la RAM son. Elle a d'ailleurs sorti le fait central du bloc, qu'aucune lecture ne crie : LE SHIFTER EST UN
// PAS EN RETARD SUR L'ACCUMULATEUR. Puis le bloc contre elle : src/sound/dsp_maison.{h,cpp}, dont l'en-tete
// nomme les quatre champs que ce jeu n'exerce jamais -- ecrits, mais jamais eprouves.
//
// ET C'EST LE REJOUEUR QUI JUGE LES SEPT BLOCS ENSEMBLE, sur une vraie scene, a l'octet. Aucun test
// unitaire ne le remplace : les sept passaient tous quand le rejeu trouvait encore deux divergences.
#pragma once

#include "scsp_core.h"
#include "dsp_maison.h"

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

	// Les trois memes nombres que l'adaptateur de MAME expose, pour DATER une divergence au lieu de la
	// constater a la sortie (scsp.h, meme ajout du 2026-10-27).
	u32 slot_cur_addr(int fente) const { return m_vive[fente & 31].cur; }
	int slot_eg_state(int fente) const { return m_vive[fente & 31].etat; }
	s32 slot_eg_volume(int fente) const { return m_vive[fente & 31].volume; }

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
	struct Lfo {
		u16 phase = 0;
		u32 pas = 0;
		const int *table = nullptr;
		const int *echelle = nullptr;
	};
	struct Vive {
		u32 cur = 0, nxt = 1u << PAS;    // l'adresse de lecture et la suivante, en virgule fixe
		u32 pas = 0;                     // le pas, calcule depuis OCT et FNS
		bool active = false;
		bool arriere = false;            // les bouclages 2 et 3 lisent a reculons
		int  etat = RELACHE;
		s32  volume = 0;
		// L'enveloppe, calculee AU DEMARRAGE depuis les champs et le rate de la note : ce sont des pas par
		// echantillon, pas les champs du registre.
		s32  ar = 0, d1r = 0, d2r = 0, rr = 0;
		s32  dl = 0;                     // 0x1F - DL : le champ est INVERSE
		bool eghold = false;
		Lfo plfo, alfo;
	};
	Vive m_vive[FENTES];
	bool m_dit = false;   // (diagnostic temporaire)
	u8 m_ptr_anneau = 0;                 // le curseur du tampon de modulation, un cran par fente

	// --- le generateur de bruit, pour SSCTL=1 et la forme de LFO 3.
	// Les constantes et la graine sont celles du coeur de MAME, et c'est une obligation et non un emprunt :
	// un bruit different rendrait le son different, et ce portage doit rester identique a l'octet. La suite
	// elle-meme est un generateur congruentiel standard (Numerical Recipes), pas une invention.
	u32 m_graine = 0x9d14abd7;
	u32 tirage() { m_graine = 1664525u * m_graine + 1013904223u; return (m_graine >> 16) | (m_graine << 16); }

	// --- les facteurs du melange, en flottant simple comme l'original
	//
	// UNE TABLE D'ENTIERS, PRECALCULEE, ET UNE SEULE POUR LES DEUX CANAUX.
	//
	// L'index est le mot d'encodage de l'original : TL sur huit bits, PAN sur cinq, SDL sur trois.
	//
	// POURQUOI PRECALCULER, et non garder trois petites tables de flottants multipliees a la volee : parce
	// que le produit doit tomber sur le MEME entier que l'original, et qu'un produit flottant recalcule a
	// chaque echantillon ne le garantit pas. Le compilateur est libre d'y fusionner les multiplications
	// (FMA) ou de garder un intermediaire en precision superieure, et le resultat change d'une unite apres
	// troncature. Le rejeu d'une scene de 1500 images l'a montre : UN echantillon sur 1 149 952, a -967
	// contre -968. Calcule UNE FOIS a la construction, le probleme ne peut pas se poser.
	//
	// ET UNE SEULE TABLE SUFFIT, ce que l'original ne remarque pas : le canal droit a l'index `pan` vaut le
	// canal gauche a l'index `pan ^ 0x10`. Le bit 4 de PAN ne choisit que le canal attenue, et les quatre
	// bits bas -- d'ou vient l'attenuation -- ne changent pas. 256 Ko au lieu de 512.
	s32 m_gain[0x10000] = {};
	void construire_facteurs();

	// --- les tables de l'enveloppe (bloc 3)
	//
	// DES CONSTANTES RELEVEES SUR MATERIEL, PAS UNE FORMULE, et c'est le point que
	// docs/scsp_architecture.md §3 demande d'assumer explicitement : "les tables ARTimes/DRTimes de MAME
	// sont des constantes mesurees, pas une formule ; les reprendre est un choix de fidelite a assumer."
	// Elles sont reprises. Le test du bloc 3 fige leurs consequences en ECHANTILLONS -- 1297, 332, 83, 24, 1
	// pour AR de 0x10 a 0x1F -- si bien qu'une reecriture qui leur substituerait une exponentielle echoue la
	// et nulle part ailleurs.
	s32 m_pas_ar[64] = {}, m_pas_dr[64] = {};
	s32 m_courbe_eg[0x400] = {};     // 3/32 dB par pas, 95,9 dB en tout
	void construire_enveloppe();
	void calculer_enveloppe(unsigned i);
	s32  avancer_enveloppe(unsigned i);

	// --- les deux LFO (bloc 4)
	//
	// UN LFO EST UNE PHASE SUR SEIZE BITS ET UN PAS, et c'est le type qui la replie : `u16` suffit, et c'est
	// pour cela que l'original n'a pas besoin de masque explicite. La phase est incrementee AVANT la lecture
	// de table, si bien que le premier echantillon lit l'index 1 et non 0 (scsp_contract.h §11 point 3).
	int m_lfo_scie_p[256] = {}, m_lfo_carre_p[256] = {}, m_lfo_tri_p[256] = {}, m_lfo_bruit_p[256] = {};
	int m_lfo_scie_a[256] = {}, m_lfo_carre_a[256] = {}, m_lfo_tri_a[256] = {}, m_lfo_bruit_a[256] = {};
	// DES ENTIERS A L'ECHELLE 256, et non des flottants a l'echelle 4096 : l'original tronque le facteur a
	// 256 puis decale de quatre, ce qui n'est PAS la meme chose que tronquer a 4096. Pour un facteur de
	// 1,001 la premiere forme rend 4096 et la seconde 4100 -- et le son cesse d'etre identique a l'octet.
	// La profondeur du vibrato est en cents, celle du tremolo en decibels.
	int m_echelle_p[8][256] = {};
	int m_echelle_a[8][256] = {};
	void construire_lfo();
	void calculer_lfo(unsigned i);
	s32  pas_lfo(Lfo &l);             // le facteur de pitch, 1.0 = 1 << PAS
	s32  gain_lfo(Lfo &l);            // le facteur d'amplitude, 1.0 = 1 << PAS

	// --- les timers et les interruptions (bloc 6)
	//
	// Les cinq numeros de timer sont ceux du contrat et de la carte : A, B, C, puis les deux horloges du
	// MIDI serie. Seuls les trois premiers sont de ce bloc.
	enum { TIMER_A, TIMER_B, TIMER_C, N_TIMERS };
	u16 m_compte[3] = {};            // le compteur de chaque timer, 0xFFFF quand il vient d'echoir
	u32 m_predivise[3] = { 1, 1, 1 };
	static u32 duree_timer(u16 reg);
	void arme_timer(int lequel);
	void verifier_attente();         // CheckPendingIRQ : UNE ligne, dans un ordre fixe
	void acquitter_interruptions();  // ResetInterrupts, et la repose d'un timer deja echu
	void maitre_attente(u16 source) { m_mcipd = u16(m_mcipd | source); }
	void executer_dma();
	// Le gain du melange pour un mot d'encodage, gauche et droite. `pan` est le champ de cinq bits : son
	// bit 4 choisit QUEL canal est attenue, l'autre restant a plein niveau.
	void gains(unsigned tl, unsigned pan, unsigned sdl, s32 &g, s32 &d) const
	{
		const unsigned i = (tl & 0xFF) | ((pan & 0x1F) << 8) | ((sdl & 7) << 13);
		g = m_gain[i];
		d = m_gain[i ^ (0x10 << 8)];
	}

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
	// COEF, MADRS et MPRO vivent DANS le DSP : en garder une copie ici serait deux verites pour un registre.
	dsp_maison m_dsp;
	u16 m_madrs_haut[CONTESTE_MOTS] = {};

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
