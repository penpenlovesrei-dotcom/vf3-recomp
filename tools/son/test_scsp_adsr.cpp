// test_scsp_adsr : l'enveloppe, ses quatre etats et ses deux arithmetiques, a l'echantillon exact.
//
// Bloc 3 de docs/scsp_architecture.md §3 -- "Enveloppes ADSR : les quatre etats, AR/D1R/D2R/RR, DL, EGHOLD,
// KRS, LPSLNK, et les tables de temps." Avec l'avertissement du document, qui est le vrai sujet de ce
// fichier : "les tables ARTimes/DRTimes de MAME sont des constantes mesurees, pas une formule ; les
// reprendre est un choix de fidelite a assumer explicitement."
//
// COMMENT ON MESURE L'ENVELOPPE SEULE. C'est le miroir exact du bloc 2. Le test des fentes mettait
// l'enveloppe de cote avec SDIR pour lire l'oscillateur ; celui-ci met l'oscillateur de cote avec une onde
// CONTINUE pour lire l'enveloppe. Une source constante C donne en sortie `C * multiplicateur >> 12`, puis
// le melange direct (TL=0, DISDL=7, DIPAN=0) laisse passer tel quel, puis l'adaptateur multiplie par 4.
// Avec C = 8192, soit deux puissances de deux, la sortie vaut EXACTEMENT 8 fois le multiplicateur -- qui se
// relit donc sans arrondi, echantillon par echantillon.
//
// ET IL Y A DEUX ARITHMETIQUES, ce qui est le fait central du bloc. Dans UpdateSlot :
//     if (state == ATTACK) sample = (sample * EG_Update(slot)) >> SHIFT;
//     else                 sample = (sample * m_EG_TABLE[EG_Update(slot) >> (SHIFT-10)]) >> SHIFT;
// L'attaque multiplie par le VOLUME BRUT -- elle est lineaire. Tout le reste passe par m_EG_TABLE, une
// courbe en decibels (3/32 dB par pas, 95,9 dB en tout). Les deux se rejoignent presque, mais pas tout a
// fait : a saturation l'attaque rend 4092 et la table 4096. Le premier echantillon de toute voix sort donc
// 4 millemes plus bas que le suivant, et ce n'est pas du bruit de mesure.
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
constexpr int CONTINU = 8192;        // deux puissances de deux : la sortie vaut 8 x le multiplicateur
constexpr int PAR_MULT = 8;

// Les reglages communs : l'enveloppe a nu, pleine sortie directe au centre, un echantillon de source par
// echantillon de sortie, et une boucle assez longue pour que l'oscillateur ne s'arrete jamais.
void a_nu(std::uint16_t mot[16])
{
	pose(mot, "SA_HI",  ONDE >> 16);
	pose(mot, "SA_LO",  ONDE & 0xFFFF);
	pose(mot, "DISDL",  7);
	pose(mot, "DIPAN",  0);
	pose(mot, "TL",     0);
	pose(mot, "OCT",    0);
	pose(mot, "FNS",    0);
	pose(mot, "LPCTL",  1);
	pose(mot, "LSA",    0);
	pose(mot, "LEA",    256);
	pose(mot, "KYONB",  1);
}

// Le multiplicateur de l'enveloppe, releve echantillon par echantillon.
std::vector<int> enveloppe(Banc &b, unsigned n)
{
	const std::vector<std::int32_t> g = b.gauche(n);
	std::vector<int> m(n);
	for (unsigned i = 0; i < n; i++) m[i] = int(g[i] / PAR_MULT);
	return m;
}

std::string liste(const std::vector<int> &v, unsigned combien)
{
	std::string s;
	for (unsigned i = 0; i < combien && i < v.size(); i++) {
		if (i) s += ",";
		s += std::to_string(v[i]);
	}
	return s;
}

// Combien d'echantillons jusqu'a la saturation de l'attaque : c'est ce qui encode une entree de
// m_ARTABLE sans avoir a la lire.
//
// LE CRITERE EST LE PLAFOND, 4092, et non "le multiplicateur cesse de monter". La premiere version de cette
// fonction s'arretait au premier echantillon qui ne montait pas, et rendait 1 pour l'attaque la plus lente
// du test : quand le volume gagne moins d'un quart de pas par echantillon, `(volume >> 16) << 2` REPETE sa
// valeur sans que l'attaque soit finie. Un critere qui se trompe d'autant plus que la mesure est longue est
// pire qu'une constante fausse, parce qu'il a l'air de marcher sur les cas courts.
unsigned duree_attaque(Banc &b, unsigned plafond)
{
	for (unsigned i = 0; i < plafond; i++)
		if (int(b.gauche(1)[0] / PAR_MULT) >= 4092) return i + 1;
	return plafond;
}

} // namespace

int main()
{
	printf("test_scsp_adsr : l'enveloppe, par scsp::core\n\n");

	// ---- 1. LE DEPART N'EST PAS ZERO, et la premiere marche n'est pas la meme que les suivantes
	// StartSlot pose `EG.volume = 0x17F << EG_SHIFT`, soit 383 sur 1023 : une voix ne monte pas du silence,
	// elle part a 37 % du chemin. Avec AR=0x1F l'attaque saute d'un coup, et on voit les deux arithmetiques
	// l'une apres l'autre : 4092 (le volume brut, dans l'etat ATTACK) puis 4096 (la table, des DECAY1).
	{
		Banc b;
		b.continu(ONDE, 256, CONTINU);
		std::uint16_t mot[16] = {};
		a_nu(mot);
		pose(mot, "AR", 0x1F);
		pose(mot, "D1R", 0);
		pose(mot, "D2R", 0);
		pose(mot, "DL", 0);
		b.keye(0, mot);
		const std::vector<int> m = enveloppe(b, 6);
		const bool juste = m[0] == 4092 && m[1] == 4096 && m[2] == 4096 && m[3] == 4096;
		juste ? ok("ATTACK multiplie le volume, DECAY la table",
		           "4092 puis 4096 : " + liste(m, 6) + " (l'ecart est de 4 sur 4096, pas du bruit)")
		      : rate("ATTACK multiplie le volume, DECAY la table",
		             "attendu 4092,4096,4096,4096 ; obtenu " + liste(m, 6));
	}

	// ---- 2. LE VOLUME DE DEPART, lu directement
	// Avec AR=0 l'attaque est infinie (m_ARTABLE[0] = 0, "Infinite time"), donc le volume reste a sa valeur
	// initiale et le multiplicateur la montre : 383 << 2 = 1532. C'est 0x17F, ecrit en deux endroits de
	// scsp.cpp, et une reecriture qui partirait de zero rendrait toute attaque plus longue et plus douce.
	{
		Banc b;
		b.continu(ONDE, 256, CONTINU);
		std::uint16_t mot[16] = {};
		a_nu(mot);
		pose(mot, "AR", 0);
		b.keye(0, mot);
		const std::vector<int> m = enveloppe(b, 4);
		bool plat = true;
		for (int v : m) if (v != m[0]) { plat = false; break; }
		(plat && m[0] == 0x17F * 4)
			? ok("AR=0 : attaque infinie, volume a 0x17F",
			     "multiplicateur " + std::to_string(m[0]) + " = 383 << 2, immobile sur 4 echantillons")
			: rate("AR=0 : attaque infinie, volume a 0x17F",
			       "attendu 1532 immobile ; obtenu " + liste(m, 4));
	}

	// ---- 3. LA DUREE DE L'ATTAQUE, par AR : ce que les tables valent, en echantillons
	// C'EST LE CAS QUI COMPTE POUR L'AVERTISSEMENT DU DOCUMENT. ARTimes est une table de 64 constantes
	// mesurees sur du materiel, pas une courbe : les ecarts d'une entree a l'autre ne suivent aucune loi
	// simple. Les durees ci-dessous en sont la consequence observable. Une reecriture qui remplacerait la
	// table par une formule exponentielle passerait les cas 1 et 2 et echouerait ici -- ce qui est
	// exactement le but, parce que c'est un CHOIX a faire les yeux ouverts et non une approximation a
	// laisser filer.
	//
	// Le compte part de `volume = 0x17F << 16` et s'arrete quand le multiplicateur cesse de monter, le pas
	// etant `m_ARTABLE[2 * AR]` (KRS=0, OCT=0, FNS=0 donnent un rate de base nul).
	{
		struct Cas { unsigned ar; unsigned attendu; };
		const Cas cas[] = { { 0x10, 1297 }, { 0x14, 332 }, { 0x18, 83 }, { 0x1C, 24 }, { 0x1F, 1 } };
		std::string lignes;
		bool juste = true;
		for (const Cas &c : cas) {
			Banc b;
			b.continu(ONDE, 256, CONTINU);
			std::uint16_t mot[16] = {};
			a_nu(mot);
			pose(mot, "AR", c.ar);
			pose(mot, "D1R", 0);
			pose(mot, "D2R", 0);
			pose(mot, "DL", 0);
			b.keye(0, mot);
			const unsigned n = duree_attaque(b, 4000);
			char t[64];
			snprintf(t, sizeof t, "%sAR=%02x->%u", lignes.empty() ? "" : " ", c.ar, n);
			lignes += t;
			if (n != c.attendu) { juste = false; lignes += "(attendu " + std::to_string(c.attendu) + ")"; }
		}
		juste ? ok("la duree de l'attaque, en echantillons", lignes)
		      : rate("la duree de l'attaque, en echantillons", lignes);
	}

	// ---- 4. EGHOLD : l'attaque est PLEINE des le premier echantillon
	// Dans l'etat ATTACK, `if (EG.EGHOLD) return 0x3ff << (SHIFT-10)` passe AVANT le calcul normal : le
	// volume monte toujours en interne, mais la sortie est a fond tout du long. Une attaque lente devient
	// donc inaudible comme attaque -- elle ne sert plus qu'a retarder le passage en DECAY1.
	{
		Banc b;
		b.continu(ONDE, 256, CONTINU);
		std::uint16_t mot[16] = {};
		a_nu(mot);
		pose(mot, "AR", 0x18);      // lente : 83 echantillons sans EGHOLD (cas 3)
		pose(mot, "EGHOLD", 1);
		pose(mot, "D1R", 0);
		pose(mot, "D2R", 0);
		pose(mot, "DL", 0);
		b.keye(0, mot);
		const std::vector<int> m = enveloppe(b, 8);
		bool pleine = true;
		for (int v : m) if (v != 4092) { pleine = false; break; }
		pleine ? ok("EGHOLD : pleine sortie pendant l'attaque",
		            "4092 des le premier echantillon, et sur les 8 : " + liste(m, 4) + ",...")
		       : rate("EGHOLD : pleine sortie pendant l'attaque",
		              "attendu 4092 partout ; obtenu " + liste(m, 8));
	}

	// ---- 5. DL EST INVERSE, et DL=0 saute DECAY1 entierement
	// `EG.DL = 0x1f - DL(slot)`, et DECAY1 s'arrete quand `volume >> 21 <= EG.DL`. Donc DL=0 donne
	// EG.DL=31, une condition vraie des le premier tic : DECAY1 ne dure pas un echantillon. A l'autre bout,
	// DL=0x1F donne EG.DL=0 et DECAY1 descend jusqu'en bas. Entre les deux, le palier ou D2R prend la main
	// se lit dans la sortie -- avec D2R=0 il s'y fige, DECAY2 rendant la main sans rien decrementer.
	{
		struct Cas { unsigned dl; int palier; };
		// Le palier est m_EG_TABLE a l'index ou DECAY1 a cede, soit (0x1F - DL) * 32 + 31 : 4052 pour un
		// index de 1022, 258 pour 767, 16 pour 511, 1 pour 255.
		//
		// ET DL=0 NE REND PAS 4096 MAIS 4052, ce qui est le detail exact de ce cas : la condition de sortie
		// est vraie des le premier tic de DECAY1, mais elle est testee APRES `volume -= EG.D1R`. DECAY1
		// decremente donc TOUJOURS une fois, meme quand il ne dure pas. Un index perdu, 0,09 dB -- et une
		// reecriture qui testerait avant de decrementer rendrait 4096 ici et passerait tout le reste.
		const Cas cas[] = { { 0x00, 4052 }, { 0x08, 258 }, { 0x10, 16 }, { 0x18, 1 } };
		std::string lignes;
		bool juste = true;
		for (const Cas &c : cas) {
			Banc b;
			b.continu(ONDE, 256, CONTINU);
			std::uint16_t mot[16] = {};
			a_nu(mot);
			pose(mot, "AR", 0x1F);      // a fond tout de suite
			pose(mot, "D1R", 0x14);     // une descente franche mais pas instantanee
			pose(mot, "D2R", 0);        // DECAY2 fige le palier
			pose(mot, "DL", c.dl);
			b.keye(0, mot);
			// 20000 echantillons, et ce chiffre est une mesure et non une marge : avec D1R=0x14 le volume
			// perd 8943 par echantillon, soit 0,136 index de table, donc descendre les 1023 index demande
			// 7500 echantillons. La premiere version en lancait 3000 et lisait DECAY1 EN COURS -- DL=0x10 et
			// DL=0x18 rendaient alors le meme palier de 49, qui n'etait le palier de personne.
			const std::vector<int> m = enveloppe(b, 20000);
			const int fin = m.back();
			char t[64];
			snprintf(t, sizeof t, "%sDL=%02x->%d", lignes.empty() ? "" : " ", c.dl, fin);
			lignes += t;
			if (fin != c.palier) { juste = false; lignes += "(attendu " + std::to_string(c.palier) + ")"; }
		}
		juste ? ok("DL inverse : le palier ou D2R prend la main", lignes)
		      : rate("DL inverse : le palier ou D2R prend la main", lignes);
	}

	// ---- 6. D2R=0 NE DECREMENTE PAS, et c'est le REGISTRE qui est teste
	// `case SCSP_DECAY2: if (D2R(slot) == 0) return ...` consulte D2R(slot), le champ du registre, et non
	// EG.D2R, la valeur passee par la table. Les deux ne disent pas la meme chose : un D2R non nul peut
	// donner un EG.D2R nul (m_DRTABLE d'un rate bas), et la sortie ne se figerait alors PAS, elle
	// descendrait de zero par echantillon -- ce qui s'entend pareil mais ne s'ecrit pas pareil. On mesure
	// donc les deux bords : D2R=0 fige, D2R=1 bouge.
	{
		std::string lignes;
		bool juste = true;
		// D2R=0x10 et non 1 : avec D2R=1 le volume perd 12 par echantillon, soit un index de table tous les
		// 5461 echantillons -- la descente existe mais ne se voit pas dans une mesure de taille raisonnable,
		// et la premiere version de ce cas concluait "devait bouger" sur sa propre impatience.
		for (auto [d2r, bouge] : { std::pair<unsigned, bool>{ 0, false }, std::pair<unsigned, bool>{ 0x10, true } }) {
			Banc b;
			b.continu(ONDE, 256, CONTINU);
			std::uint16_t mot[16] = {};
			a_nu(mot);
			pose(mot, "AR", 0x1F);
			pose(mot, "D1R", 0);
			pose(mot, "D2R", d2r);
			pose(mot, "DL", 0);         // DECAY1 cede tout de suite
			b.keye(0, mot);
			const std::vector<int> m = enveloppe(b, 4000);
			const bool a_bouge = m.back() != m[1];
			char t[80];
			snprintf(t, sizeof t, "%sD2R=%02x : %d -> %d", lignes.empty() ? "" : " | ", d2r, m[1], m.back());
			lignes += t;
			if (a_bouge != bouge) { juste = false; lignes += a_bouge ? " (devait figer)" : " (devait bouger)"; }
		}
		juste ? ok("D2R=0 fige le palier, D2R=1 le fait descendre", lignes)
		      : rate("D2R=0 fige le palier, D2R=1 le fait descendre", lignes);
	}

	// ---- 7. LPSLNK : l'attaque finit au passage de LSA, pas a la saturation
	// Le seul endroit du bloc 3 qui depende du bloc 2. Dans ATTACK, le passage en DECAY1 est enferme dans
	// `if (!LPSLNK(slot))` : avec LPSLNK l'etat NE CHANGE PAS quand le volume sature, il reste ATTACK. Il
	// n'en sort que par l'oscillateur, dans la boucle de bouclage : `if (addr1 >= LSA && !Backwards) { if
	// (LPSLNK && state == ATTACK) state = DECAY1; }`. On le voit donc par l'arithmetique : tant qu'on est
	// en ATTACK la sortie vaut 4092 (le volume brut), et elle passe a 4096 (la table) a l'echantillon ou la
	// tete de lecture atteint LSA. Avec LSA=20, cela fait vingt echantillons a 4092.
	{
		Banc b;
		b.continu(ONDE, 256, CONTINU);
		std::uint16_t mot[16] = {};
		a_nu(mot);
		pose(mot, "AR", 0x1F);
		pose(mot, "LPSLNK", 1);
		pose(mot, "LSA", 20);
		pose(mot, "LEA", 24);
		pose(mot, "D1R", 0);
		pose(mot, "D2R", 0);
		pose(mot, "DL", 0);
		b.keye(0, mot);
		const std::vector<int> m = enveloppe(b, 32);
		unsigned n = 0;
		while (n < m.size() && m[n] == 4092) n++;
		// DIX-NEUF ET NON VINGT, et le compte dit pourquoi : dans UpdateSlot le test LPSLNK a lieu AVANT
		// que l'enveloppe soit appliquee. A l'echantillon ou la tete atteint LSA=20, l'etat est deja DECAY1
		// quand le gain est calcule, et cet echantillon sort donc par la table. Il reste dix-neuf
		// echantillons en ATTACK.
		(n == 19) ? ok("LPSLNK : ATTACK tient jusqu'a LSA",
		               std::to_string(n) + " echantillons a 4092 (LSA=20, le vingtieme est deja DECAY1 : "
		               + std::to_string(m[n]) + ")")
		          : rate("LPSLNK : ATTACK tient jusqu'a LSA",
		                 "attendu 19 echantillons a 4092 ; obtenu " + std::to_string(n)
		                 + ", suite " + liste(m, 24));
	}

	// ---- 8. KRS : l'echelonnage par la note, et le 0xF qui l'annule
	// `rate = octave + 2 * KRS + ((FNS >> 9) & 1)`, sauf KRS=0xF qui donne rate = 0 tout court -- pas
	// l'octave, pas le bit de FNS : ZERO. Pour le voir il faut une octave non nulle, sinon KRS=0 et KRS=0xF
	// se confondent ; on prend donc OCT=2. Le rate s'ajoute a 2*AR avant de lire la table, donc une meme
	// AR donne des attaques de durees differentes selon la note -- ce qui est tout l'objet de KRS.
	{
		std::string lignes;
		bool juste = true;
		struct Cas { unsigned krs; unsigned fns; unsigned attendu; };
		// KRS=0xF rend 83, exactement la duree du cas 3 a OCT=0 : la preuve que le 0xF annule l'octave et
		// ne se contente pas de l'ignorer a moitie.
		const Cas cas[] = { { 0x0, 0x000, 56 }, { 0x1, 0x000, 45 }, { 0x0, 0x200, 50 }, { 0xF, 0x000, 83 } };
		for (const Cas &c : cas) {
			Banc b;
			b.continu(ONDE, 1024, CONTINU);
			std::uint16_t mot[16] = {};
			a_nu(mot);
			pose(mot, "OCT", 2);        // (OCT ^ 8) - 8 = 2
			pose(mot, "FNS", c.fns);
			pose(mot, "LEA", 1000);     // l'octave 2 avance quatre fois plus vite
			pose(mot, "AR", 0x18);
			pose(mot, "KRS", c.krs);
			pose(mot, "D1R", 0);
			pose(mot, "D2R", 0);
			pose(mot, "DL", 0);
			b.keye(0, mot);
			const unsigned n = duree_attaque(b, 4000);
			char t[80];
			snprintf(t, sizeof t, "%sKRS=%x FNS=%03x -> %u", lignes.empty() ? "" : " ", c.krs, c.fns, n);
			lignes += t;
			if (n != c.attendu) { juste = false; lignes += "(attendu " + std::to_string(c.attendu) + ")"; }
		}
		juste ? ok("KRS echelonne, KRS=0xF annule tout", lignes)
		      : rate("KRS echelonne, KRS=0xF annule tout", lignes);
	}

	// ---- 9. RELEASE : la descente, puis la fente s'eteint POUR DE BON
	// Le key-off ne coupe pas le son, il passe l'enveloppe en RELEASE (StopSlot avec keyoff=1). La fente ne
	// devient inactive que quand le volume atteint zero, et c'est alors StopSlot(slot, 0) qui met
	// `active = 0`. Deux choses a verifier donc : la descente a lieu, et elle finit par un vrai silence et
	// non par un plancher.
	{
		Banc b;
		b.continu(ONDE, 256, CONTINU);
		std::uint16_t mot[16] = {};
		a_nu(mot);
		pose(mot, "AR", 0x1F);
		pose(mot, "D1R", 0);
		pose(mot, "D2R", 0);
		pose(mot, "DL", 0);
		pose(mot, "RR", 0x18);
		b.keye(0, mot);
		const int avant = enveloppe(b, 4)[3];
		// key-off : KYONB a zero et KYONEX, qui balaie les 32 fentes
		std::uint16_t off[16] = {};
		pose(off, "KYONEX", 1);
		b.coeur.write_reg(0, off[0], 0xFFFF);
		unsigned n = 0;
		int dernier = -1;
		for (; n < 4000; n++) {
			dernier = int(b.gauche(1)[0] / PAR_MULT);
			if (dernier == 0) break;
		}
		const std::vector<int> apres = enveloppe(b, 8);
		bool muette = true;
		for (int v : apres) if (v != 0) { muette = false; break; }
		char t[128];
		snprintf(t, sizeof t, "4096 avant, zero apres %u echantillons, et muette ensuite", n);
		(avant == 4096 && n == 251 && muette)
			? ok("RELEASE descend, puis la fente s'eteint", t)
			: rate("RELEASE descend, puis la fente s'eteint",
			       "avant " + std::to_string(avant) + ", zero apres " + std::to_string(n)
			       + " (attendu 251), muette ensuite : " + (muette ? "oui" : "non"));
	}

	// ---- 10. UN SECOND KYONEX NE REDECLENCHE PAS une voix qui joue
	// `if (KEYONB(s2) && s2->EG.state == SCSP_RELEASE) StartSlot(s2)` : seule une fente EN RELEASE est
	// (re)demarree. Une fente qui joue ignore donc la gachette, et son volume ne revient pas a 0x17F. Une
	// reecriture qui redemarrerait sur chaque KYONEX ferait claquer toutes les notes repetees du jeu.
	{
		Banc b;
		b.continu(ONDE, 256, CONTINU);
		std::uint16_t mot[16] = {};
		a_nu(mot);
		pose(mot, "AR", 0x1F);
		pose(mot, "D1R", 0);
		pose(mot, "D2R", 0);
		pose(mot, "DL", 0);
		b.keye(0, mot);
		const int etabli = enveloppe(b, 4)[3];
		b.keye(0, mot);                      // la meme gachette, une seconde fois
		const std::vector<int> apres = enveloppe(b, 4);
		const bool inchange = etabli == 4096 && apres[0] == 4096 && apres[1] == 4096;
		inchange ? ok("un second KYONEX ne redeclenche rien",
		              "4096 avant et apres : " + liste(apres, 4) + " (pas de retour a 1532)")
		         : rate("un second KYONEX ne redeclenche rien",
		                "avant " + std::to_string(etabli) + ", apres " + liste(apres, 4)
		                + " ; un retour a 1532 serait un redemarrage");
	}

	printf("\n");
	if (echecs) { printf("%d echec(s).\n", echecs); return 1; }
	printf("tous les cas passent\n");
	return 0;
}
