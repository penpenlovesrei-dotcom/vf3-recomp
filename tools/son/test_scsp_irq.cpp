// test_scsp_irq : les trois timers, les cinq lignes d'interruption, et le DMA.
//
// Bloc 6 de docs/scsp_architecture.md §3 -- "Timers, interruptions, synchronisation : A/B/C,
// SCIEB/SCIPD/SCILV, MCIEB/MCIPD, le DMA, et la cadence des interruptions, dont Mednafen dit qu'elle est ce
// qui fait deriver le tempo de 2 %." C'est ce dernier point qui fait l'importance du bloc : le tic de la
// musique de VF3 EST le timer A, et la sequence entiere du pilote pend a sa cadence.
//
// LA SONDE N'EST PAS LA SORTIE AUDIO, pour une fois : c'est le puits. scsp::sink porte les trois methodes
// par lesquelles la puce parle a la carte -- irq(niveau, etat), midi_out(octet), timer(lequel, echantillons,
// periode) -- et un puits qui NOTE tout ce qu'il recoit mesure ce bloc directement, sans passer par une
// seule note de musique. C'est aussi ce qui le rend exact : on compare des entiers, pas des gains.
//
// Le montage est dans tools/son/banc_scsp.h, mais ce fichier se construit son propre coeur : il lui faut un
// puits bavard, la ou le banc en pose un muet.
#include "banc_scsp.h"

#include "../../src/sound/scsp_select.h"

#include <cstdio>
#include <string>
#include <vector>

namespace {

int echecs = 0;

void ok(const char *quoi, const std::string &d) { printf("  ok    %-40s %s\n", quoi, d.c_str()); }
void rate(const char *quoi, const std::string &d) { printf("  ECHEC %-40s %s\n", quoi, d.c_str()); ++echecs; }

// Le puits qui note tout. C'est lui la mesure.
struct Mouchard final : scsp::sink {
	struct Ligne { int niveau; bool etat; };
	struct Arme { int lequel; std::uint32_t echantillons, periode; };
	std::vector<Ligne> lignes;
	std::vector<Arme> armes;
	std::vector<std::uint8_t> midi;

	void irq(int niveau, bool etat) override { lignes.push_back({ niveau, etat }); }
	void midi_out(std::uint8_t b) override { midi.push_back(b); }
	void timer(int lequel, std::uint32_t ech, std::uint32_t per) override { armes.push_back({ lequel, ech, per }); }

	void vide() { lignes.clear(); armes.clear(); midi.clear(); }
	// Le dernier niveau LEVE, ou -1 si la derniere chose vue est une retombee.
	int dernier_leve() const
	{
		for (std::size_t i = lignes.size(); i-- > 0; )
			if (lignes[i].etat) return lignes[i].niveau;
		return -1;
	}
	std::string trace() const
	{
		std::string s;
		for (const Ligne &l : lignes) {
			if (!s.empty()) s += " ";
			s += (l.etat ? "+" : "-") + std::to_string(l.niveau);
		}
		return s.empty() ? "(rien)" : s;
	}
};

// Un coeur avec un puits bavard. La RAM sert au DMA.
struct Banc6 {
	std::vector<std::uint16_t> ram = std::vector<std::uint16_t>(banc::RAM_OCTETS / 2, 0);
	Mouchard m;
	std::unique_ptr<scsp::core> tenu =
		scsp::make(banc::nom_du_coeur(), 45158400 / 2, ram.data(), banc::RAM_OCTETS, m);
	scsp::core &coeur = *tenu;

	Banc6()
	{
		if (!tenu) { fprintf(stderr, "test_scsp_irq: coeur inconnu\n"); std::exit(2); }
		coeur.reset();
		coeur.write_reg(0x400 / 2, 0x0300 | 0x000F, 0xFFFF);
	}

	// La table d'interruptions que le pilote de VF3 ecrit au demarrage, telle que le contrat la porte : dix
	// mots d'un coup de 0x418 a 0x42A (0x600288 les copie de 0x6002D4). On la pose en entier plutot que
	// d'inventer une configuration, pour que ce qui est mesure soit ce que le jeu vit.
	void table_du_pilote()
	{
		for (unsigned k = 0; k < 10; k++)
			coeur.write_reg((0x418 + 2 * k) / 2, scsp_contract::INIT_418_42A[k], 0xFFFF);
	}
	std::uint16_t lire(unsigned octet) { return coeur.read_reg(octet / 2); }
	void ecrire(unsigned octet, std::uint16_t v) { coeur.write_reg(octet / 2, v, 0xFFFF); }
};

} // namespace

int main(int argc, char **argv)
{
	if (argc > 1) banc::nom_du_coeur() = argv[1];
	printf("test_scsp_irq : timers, interruptions et DMA, par scsp::core\n\n");

	// ---- 1. LA DUREE D'UN TIMER, ET LE "+1" QUI EST UNE DIVERGENCE VOULUE
	// `timer_samples(reg) = ((255 - (reg & 0xff)) << ((reg >> 8) & 7)) + 1`. MAME ecrit la meme chose SANS
	// le +1, et le commentaire de scsp.cpp dit ce que ce +1 a corrige : avec 49 echantillons au lieu de 50,
	// le tic de VF3 (timer A a 0xCE) partait a 900 Hz au lieu de 882, et la musique sortait 0,8 % trop vite
	// contre Supermodel. C'est la troisieme divergence deliberee du portage, apres PLFOS et MDL.
	//
	// Elle se mesure sans jouer une note : le puits recoit timer(lequel, echantillons, periode) a chaque
	// ecriture du registre, et `echantillons` EST le chiffre.
	{
		struct Cas { std::uint16_t reg; std::uint32_t ech; const char *quoi; };
		const Cas cas[] = {
			{ 0x00CE,   50, "0xCE : le tic de VF3, 882 Hz" },
			{ 0x0000,  256, "0x00 : le maximum sans prediviseur" },
			{ 0x00FE,    2, "0xFE : le minimum qui arme encore" },
			{ 0x0100,  511, "prediviseur 1, compte 0" },
			{ 0x07CE, 6273, "prediviseur 7, compte 0xCE" },
		};
		std::string lignes;
		bool juste = true;
		for (const Cas &c : cas) {
			Banc6 b;
			b.table_du_pilote();
			b.m.vide();
			b.ecrire(scsp_contract::TIMA, c.reg);
			std::uint32_t vu = 0;
			for (const Mouchard::Arme &a : b.m.armes)
				if (a.lequel == 0) vu = a.echantillons;
			char t[48];
			snprintf(t, sizeof t, "%s%04x->%u", lignes.empty() ? "" : " ", c.reg, vu);
			lignes += t;
			if (vu != c.ech) { juste = false; lignes += "(att " + std::to_string(c.ech) + ")"; }
		}
		juste ? ok("la duree d'un timer, le +1 compris", lignes)
		      : rate("la duree d'un timer, le +1 compris", lignes);
	}

	// ---- 2. UN COMPTE DE 255 N'ARME PAS LE TIMER
	// `if ((m_udata.data[0x18/2] & 0xff) != 255)` : ecrire 0xFF dans l'octet bas laisse le timer muet. Ce
	// n'est pas une duree de zero, c'est une absence d'armement -- la difference se voit dans le puits, qui
	// ne recoit rien du tout, et pas un timer(A, 1, 0).
	{
		Banc6 b;
		b.table_du_pilote();
		b.m.vide();
		b.ecrire(scsp_contract::TIMA, 0x00FF);
		unsigned pour_a = 0;
		for (const Mouchard::Arme &a : b.m.armes) if (a.lequel == 0) pour_a++;
		(pour_a == 0) ? ok("compte 255 : le timer n'est pas arme", "le puits ne recoit rien pour A")
		              : rate("compte 255 : le timer n'est pas arme",
		                     std::to_string(pour_a) + " armement(s) recu(s), attendu aucun");
	}

	// ---- 3. LES TROIS NIVEAUX QUE LE PILOTE DEMANDE, mesures a la source
	// Le contrat (§3) dit MIDI au niveau 3, timer A au 2, timer B au 1, et le timer C non arme. Ces niveaux
	// ne sont pas dans un registre : ils sont ASSEMBLES bit par bit par DecodeSCI depuis SCILV0/1/2 -- bit 0
	// du niveau depuis SCILV0, bit 1 depuis SCILV1, bit 2 depuis SCILV2. Avec la table du pilote (0x0088,
	// 0x0048, 0x0000) cela donne bien 3, 2 et 1, et c'est ici qu'on le verifie plutot que de le croire.
	//
	// ATTENTION, et c'est un piege pour une reecriture : les niveaux ne sont RECALCULES que lorsqu'un des
	// mots 0x424-0x429 est ecrit. Armer SCIEB avant SCILV laisse les niveaux a zero.
	{
		std::string lignes;
		bool juste = true;
		struct Cas { int timer; int niveau; const char *nom; };
		const Cas cas[] = {
			{ 0, 2, "timer A" },
			{ 1, 1, "timer B" },
		};
		for (const Cas &c : cas) {
			Banc6 b;
			b.table_du_pilote();
			b.m.vide();
			b.coeur.timer_fired(c.timer);
			const int n = b.m.dernier_leve();
			char t[64];
			snprintf(t, sizeof t, "%s%s->%d", lignes.empty() ? "" : " ", c.nom, n);
			lignes += t;
			if (n != c.niveau) { juste = false; lignes += "(att " + std::to_string(c.niveau) + ")"; }
		}
		// MIDI : un octet entrant doit lever le niveau 3
		{
			Banc6 b;
			b.table_du_pilote();
			b.m.vide();
			b.coeur.midi_in(0x90);
			const int n = b.m.dernier_leve();
			lignes += " midi->" + std::to_string(n);
			if (n != 3) { juste = false; lignes += "(att 3)"; }
		}
		juste ? ok("les niveaux assembles par SCILV0/1/2", lignes)
		      : rate("les niveaux assembles par SCILV0/1/2", lignes);
	}

	// ---- 4. LE TIMER C EST CABLE MAIS PAS ARME, et il ne leve donc rien
	// Le contrat le dit du cote du pilote (SCIEB = 0x00C8 n'a pas le bit 8) ; ici on le voit du cote de la
	// puce : le timer C se declenche, SCIPD en garde la trace, et aucune ligne ne monte.
	{
		Banc6 b;
		b.table_du_pilote();
		b.m.vide();
		b.coeur.timer_fired(2);
		const int n = b.m.dernier_leve();
		const std::uint16_t scipd = b.lire(scsp_contract::SCIPD);
		char t[96];
		snprintf(t, sizeof t, "SCIPD vaut 0x%04x (bit 8 pose), et la trace est : %s", scipd, b.m.trace().c_str());
		(n == -1 && (scipd & 0x100)) ? ok("timer C : note mais pas arme", t)
		                             : rate("timer C : note mais pas arme",
		                                    std::string(t) + " ; attendu aucune montee et le bit 8");
	}

	// ---- 5. UNE SEULE LIGNE A LA FOIS, dans un ordre de priorite fixe
	// CheckPendingIRQ examine les sources dans un ordre et SORT a la premiere qui est a la fois en attente
	// et armee : CPU (0x20), puis timer A (0x40), puis B (0x80), puis C (0x100), puis MIDI (0x8). Donc si A
	// et B sont tous deux en attente, c'est le niveau de A qui monte et celui de B qui n'est jamais vu.
	//
	// Une reecriture qui leverait les deux -- ce qui semble plus juste -- donnerait au 68000 une interruption
	// que le pilote n'attend pas a ce moment-la.
	{
		Banc6 b;
		b.table_du_pilote();
		b.coeur.timer_fired(1);   // B en attente d'abord
		b.m.vide();
		b.coeur.timer_fired(0);   // puis A
		const int n = b.m.dernier_leve();
		const std::uint16_t scipd = b.lire(scsp_contract::SCIPD);
		char t[112];
		snprintf(t, sizeof t, "A et B en attente (SCIPD 0x%04x) : c'est le niveau %d qui monte", scipd, n);
		(n == 2 && (scipd & 0xC0) == 0xC0)
			? ok("A gagne sur B : une ligne, puis retour", t)
			: rate("A gagne sur B : une ligne, puis retour",
			       std::string(t) + " ; attendu le niveau 2 et les deux bits poses");
	}

	// ---- 6. LA LIGNE 0, ET LE RETOUR PREMATURE QUI DECIDE QUAND ELLE RETOMBE
	// `m_host.irq(0, CLEAR_LINE)` en fin de CheckPendingIRQ : une CINQUIEME ligne, qui n'est aucune des quatre
	// sources. C'est precisement ce qui a fait redessiner l'interface -- le §2 de l'architecture proposait
	// `irq(int niveau)` avec zero pour "plus rien", et la puce, elle, lache quatre lignes independantes PLUS
	// celle-la (scsp_core.h le dit en toutes lettres).
	//
	// MAIS ELLE NE RETOMBE PAS QUAND "RIEN N'EST EN ATTENTE", et la premiere version de ce cas l'affirmait a
	// tort. CheckPendingIRQ commence par `if (!pend) return;` et sort donc AVANT la derniere ligne : avec
	// SCIPD vide, aucune ligne n'est touchee du tout. Le -0 arrive dans l'autre situation, celle ou quelque
	// chose EST en attente et ou rien d'arme n'y correspond -- le timer C du cas 4 en est l'exemple, et sa
	// trace est bien un -0 seul.
	//
	// Les deux moities se mesurent, parce qu'une reecriture qui rabaisserait la ligne sans condition passerait
	// la premiere et echouerait la seconde.
	{
		// (a) en attente mais non arme : la ligne 0 retombe
		Banc6 b;
		b.table_du_pilote();
		b.m.vide();
		b.coeur.timer_fired(2);   // bit 8, absent de SCIEB = 0x00C8
		bool zero_retombe = false;
		for (const Mouchard::Ligne &l : b.m.lignes) if (l.niveau == 0 && !l.etat) zero_retombe = true;

		// (b) rien en attente : aucune ligne n'est touchee. On repasse par CheckPendingIRQ en reecrivant
		// SCIEB, seul registre dont le crochet l'appelle sans condition.
		Banc6 b2;
		b2.table_du_pilote();
		b2.m.vide();
		b2.ecrire(scsp_contract::SCIEB, scsp_contract::INIT_418_42A[3]);
		const bool muet = b2.m.lignes.empty();

		const std::string d = "en attente non arme : " + b.m.trace() + " ; rien en attente : " + b2.m.trace();
		(zero_retombe && muet) ? ok("la ligne 0 retombe, mais pas sur du vide", d)
		                       : rate("la ligne 0 retombe, mais pas sur du vide",
		                              d + " ; attendu un -0 puis (rien)");
	}

	// ---- 7. SCIRE SUR UN TIMER DEJA ECHU LE REMET AUSSITOT EN ATTENTE
	// Le commentaire de scsp.cpp l'attribue au materiel reel : "if you SCIRE a timer that's expired, it'll
	// immediately pop up again in SCIPD". Le compteur restant a 0xFFFF, l'acquittement est suivi d'une
	// repose du bit. Et le meme commentaire note qu'un autre jeu n'est pas d'accord -- donc c'est un choix,
	// pas une certitude, et il est enregistre comme tel.
	{
		Banc6 b;
		b.table_du_pilote();
		b.coeur.timer_fired(0);
		const std::uint16_t avant = b.lire(scsp_contract::SCIPD);
		b.ecrire(scsp_contract::SCIRE, 0x0040);
		const std::uint16_t apres = b.lire(scsp_contract::SCIPD);
		char t[112];
		snprintf(t, sizeof t, "SCIPD 0x%04x, acquitte, puis 0x%04x : le bit 6 revient", avant, apres);
		((avant & 0x40) && (apres & 0x40)) ? ok("SCIRE sur un timer echu : il repend", t)
		                                   : rate("SCIRE sur un timer echu : il repend",
		                                          std::string(t) + " ; attendu le bit 6 dans les deux");
	}

	// ---- 8. LE TIMER A PEND AUSSI DU COTE MAITRE, B ET C NON
	// timerA_cb appelle MainCheckPendingIRQ(0x40) en plus de CheckPendingIRQ ; timerB_cb et timerC_cb ne
	// touchent que le cote esclave. L'asymetrie se lit dans MCIPD (0x42C), que la lecture recopie depuis
	// m_mcipd. Rien dans VF3 ne la consulte (scsp_contract.h section 2), mais une reecriture qui
	// l'uniformiserait changerait un etat observable pour rien.
	{
		std::string lignes;
		bool juste = true;
		struct Cas { int timer; std::uint16_t attendu; const char *nom; };
		const Cas cas[] = {
			{ 0, 0x0040, "A" },
			{ 1, 0x0000, "B" },
			{ 2, 0x0000, "C" },
		};
		for (const Cas &c : cas) {
			Banc6 b;
			b.table_du_pilote();
			b.coeur.timer_fired(c.timer);
			const std::uint16_t mcipd = std::uint16_t(b.lire(scsp_contract::MCIPD) & 0x1C0);
			char t[48];
			snprintf(t, sizeof t, "%s%s->%04x", lignes.empty() ? "" : " ", c.nom, mcipd);
			lignes += t;
			if (mcipd != c.attendu) { juste = false; lignes += "(att " + std::to_string(c.attendu) + ")"; }
		}
		juste ? ok("MCIPD : A pend du cote maitre, B et C non", lignes)
		      : rate("MCIPD : A pend du cote maitre, B et C non", lignes);
	}

	// ---- 9. LE DMA, DANS LES DEUX SENS, ET SON ZERO-FILL
	// DMEA est l'adresse en RAM (vingt bits : les seize bas depuis 0x412, les quatre hauts depuis les bits
	// 15-12 de 0x414), DRGA l'adresse dans la fenetre de registres (douze bits, 0x414), DTLG la longueur en
	// octets (0x416), DDIR le sens et DGATE le remplissage a zero. DEXE lance, et se rebaisse tout seul a la
	// fin.
	//
	// On choisit l'anneau (0x600-0x6FF) comme destination en registres : c'est la seule zone large que la
	// fenetre laisse ecrire ET relire sans effet de bord, les fentes declenchant des voix et le commun
	// changeant la configuration.
	{
		// (a) RAM -> registres
		Banc6 b;
		for (unsigned k = 0; k < 4; k++) b.ram[0x2000 / 2 + k] = std::uint16_t(0x1100 + k);
		b.ecrire(0x412, 0x2000);         // DMEA bas
		b.ecrire(0x414, 0x0600);         // DMEA haut = 0, DRGA = 0x600 (l'anneau)
		b.ecrire(0x416, 0x1008);         // DEXE | DTLG = 8 octets
		std::uint16_t relu[4];
		for (unsigned k = 0; k < 4; k++) relu[k] = b.lire(0x600 + 2 * k);
		const bool monte = relu[0] == 0x1100 && relu[1] == 0x1101 && relu[2] == 0x1102 && relu[3] == 0x1103;
		const bool dexe_bas = (b.lire(0x416) & 0x1000) == 0;

		// (b) registres -> RAM, DDIR a 1
		Banc6 b2;
		for (unsigned k = 0; k < 4; k++) b2.ecrire(0x600 + 2 * k, std::uint16_t(0x2200 + k));
		b2.ecrire(0x412, 0x3000);
		b2.ecrire(0x414, 0x0600);
		b2.ecrire(0x416, 0x3008);        // DDIR | DEXE | 8 octets
		const bool descend = b2.ram[0x3000 / 2] == 0x2200 && b2.ram[0x3000 / 2 + 3] == 0x2203;

		// (c) DGATE : des zeros, et la source n'est pas lue
		Banc6 b3;
		for (unsigned k = 0; k < 4; k++) b3.ecrire(0x600 + 2 * k, 0xBEEF);
		b3.ecrire(0x412, 0x4000);
		b3.ecrire(0x414, 0x0600);
		b3.ecrire(0x416, 0x5008);        // DGATE | DEXE | 8 octets, DDIR a 0 : zero dans les REGISTRES
		bool zeros = true;
		for (unsigned k = 0; k < 4; k++) if (b3.lire(0x600 + 2 * k) != 0) zeros = false;

		char t[160];
		snprintf(t, sizeof t, "RAM->reg %s, reg->RAM %s, DGATE %s, DEXE rebaisse %s",
		         monte ? "oui" : "NON", descend ? "oui" : "NON", zeros ? "oui" : "NON",
		         dexe_bas ? "oui" : "NON");
		(monte && descend && zeros && dexe_bas) ? ok("le DMA dans les deux sens, et DGATE", t)
		                                        : rate("le DMA dans les deux sens, et DGATE", t);
	}

	// ---- 10. UN DMA NE PEUT PAS ECRASER SES PROPRES PARAMETRES, et seulement dans un sens
	// exec_dma sauve 0x412, 0x414 et 0x416 avant le transfert et les restaure apres -- mais UNIQUEMENT
	// quand DDIR vaut 0, c'est-a-dire quand la destination est la fenetre de registres et pourrait donc
	// contenir les registres de controle eux-memes. Le commentaire du code le dit : "DMA *can't* overwrite
	// its parameters". On le verifie en visant 0x412 avec un DMA venu de la RAM.
	{
		Banc6 b;
		for (unsigned k = 0; k < 4; k++) b.ram[0x5000 / 2 + k] = 0xFFFF;   // de quoi tout ecraser
		b.ecrire(0x412, 0x5000);
		b.ecrire(0x414, 0x0412);         // DRGA vise les registres de DMA eux-memes
		b.ecrire(0x416, 0x1008);         // DEXE | 8 octets
		const std::uint16_t a = b.lire(0x412), c = b.lire(0x414), d = b.lire(0x416);
		char t[128];
		snprintf(t, sizeof t, "apres un DMA visant 0x412 : 0x412=%04x 0x414=%04x 0x416=%04x", a, c, d);
		(a == 0x5000 && c == 0x0412 && (d & 0x0FFE) == 0x008)
			? ok("un DMA ne s'ecrase pas lui-meme", t)
			: rate("un DMA ne s'ecrase pas lui-meme", std::string(t) + " ; attendu 5000 0412 ...008");
	}

	// ---- 11. MIDI : un octet entre, la ligne monte ; on relit, elle retombe
	// Un octet entrant pose le bit 3 de SCIPD et leve la ligne du MIDI. La lecture de 0x404 depile un octet
	// et, quand la file se vide, redescend la ligne et efface le bit -- c'est fait dans UpdateRegR et non
	// dans la lecture elle-meme, ce qui veut dire qu'une reecriture qui rendrait le registre sans passer par
	// un crochet de lecture laisserait l'interruption levee pour toujours.
	{
		Banc6 b;
		b.table_du_pilote();
		b.m.vide();
		b.coeur.midi_in(0x90);
		const int apres_entree = b.m.dernier_leve();
		const std::uint16_t scipd_avant = b.lire(scsp_contract::SCIPD);
		const std::uint16_t octet = std::uint16_t(b.lire(0x404) & 0xFF);
		const std::uint16_t scipd_apres = b.lire(scsp_contract::SCIPD);
		char t[144];
		snprintf(t, sizeof t, "niveau %d a l'entree, octet relu 0x%02x, SCIPD 0x%04x puis 0x%04x",
		         apres_entree, octet, scipd_avant, scipd_apres);
		(apres_entree == 3 && octet == 0x90 && (scipd_avant & 8) && !(scipd_apres & 8))
			? ok("MIDI entrant : leve, relu, retombe", t)
			: rate("MIDI entrant : leve, relu, retombe",
			       std::string(t) + " ; attendu niveau 3, 0x90, le bit 3 pose puis efface");
	}

	printf("\n");
	if (echecs) { printf("%d echec(s).\n", echecs); return 1; }
	printf("tous les cas passent\n");
	return 0;
}
