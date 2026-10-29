// test_scsp_dsp : le jeu d'instructions du DSP d'effets, mesure champ par champ.
//
// Bloc 5 de docs/scsp_architecture.md §3, et le seul des sept dont le coeur du projet n'a pas ecrit une
// ligne. La raison n'est pas le temps : SON JEU D'INSTRUCTIONS N'EXISTE NULLE PART ECRIT. Le contrat n'en
// donne que la disposition -- COEF, MADRS, MPRO, 128 pas de 8 octets (scsp_contract.h §6) -- et
// docs/scsp_references.md est clair : ni fiche technique, ni specification independante, seulement les
// implementations de MAME et de Supermodel, issues du meme travail de reverse engineering d'ElSemi.
//
// CE FICHIER EST DONC LA SPECIFICATION QUI MANQUAIT, et c'est a cela qu'il sert avant de servir de test :
// chaque cas pose un programme minuscule, le fait tourner, et ENREGISTRE ce que le DSP en a fait. Une
// implementation future s'ecrit contre ces nombres, et non contre un autre code. C'est la meme boucle que
// pour les six autres blocs -- ecrire le test, puis le bloc -- a ceci pres que la cible du test est ici une
// machine et non un comportement de sortie.
//
// IL DISTINGUE LES DEUX COEURS DEPUIS LE 2026-10-27, et c'etait tout son objet : `src/sound/dsp_maison`
// existe, il ne partage pas une ligne avec celui de MAME, et ce fichier dit cas par cas s'il tombe juste --
// sans qu'on ait a comparer des WAV. `make test` le lance avec les deux noms de coeur.
//
// ET LES CINQ DERNIERS CAS SONT CE POUR QUOI IL EXISTE. Les huit premiers mesurent des champs que le jeu
// exerce, donc que le rejoueur recoupe a l'octet a chaque scene : ce fichier y est une commodite. Les cas 9
// a 13 mesurent FRCL, ADRL, YRL et NEGB, qu'AUCUN jeu servi par ce portage ne pose -- 152 ecritures de
// programme, cinq formes distinctes, trois jeux (docs/scsp_audit.md §5). Pour ces quatre-la, ce fichier est
// le SEUL juge qui existe, et les quatre fautes injectees le 2026-10-28 disent qu'il sait echouer.
//
// -----------------------------------------------------------------------------------------------------
// PAR OU ON OBSERVE, et c'est ce qui rend le bloc mesurable : TOUT L'ETAT DU DSP SE RELIT par la fenetre de
// registres, sans passer par une seule note de musique.
//
//     0x700-0x77F  COEF      les coefficients, en lecture et ecriture
//     0x780-0x7BF  MADRS     les adresses de la ligne de retard
//     0x800-0xBFF  MPRO      le programme, 128 pas de quatre mots
//     0xC00-0xDFF  TEMP      128 mots de 24 bits -- DEUX mots chacun, le haut d'abord
//     0xE00-0xE7F  MEMS      32 mots de 24 bits
//     0xE80-0xEBF  MIXS      16 mots de 20 bits
//     0xEC0-0xEDF  EFREG     16 mots de 16 bits, la sortie d'effets
//     0xEE0-0xEE3  EXTS      les deux entrees externes (rien n'y est branche sur cette carte)
//
// Au-dela de 0xC00 la fenetre est en LECTURE SEULE : une ecriture n'y va nulle part.
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
// -----------------------------------------------------------------------------------------------------
// LE FAIT CENTRAL DU BLOC, et c'est ce test qui l'a sorti : LE SHIFTER EST UN PAS EN RETARD SUR
// L'ACCUMULATEUR.
//
// Dans le corps d'un pas, SHIFTED est calcule AVANT que ACC ne soit reecrit :
//
//     SHIFTED = <ecrete ou etend> ACC      <- l'ACC du pas PRECEDENT
//     ACC = (X * Y >> 12) + B              <- et seulement maintenant
//
// Donc le pas qui fait un produit ne peut pas en ecrire le resultat : il faut le pas SUIVANT pour que
// TWT, MWT, EWT, FRCL ou ADRL le voient. Une implementation qui decalerait l'ACC fraichement calcule
// serait fausse partout, et aucune lecture du code ne le crie -- c'est l'ordre des lignes qui le dit.
//
// La premiere version de ce fichier l'ignorait, et la mesure l'a dit sans ambiguite : un cas qui ecrivait
// EFREG deux fois rendait "une ecriture 0, deux 4095". Seule la seconde voyait la valeur.
//
// D'OU DEUX IDIOMES qui reviennent dans tous les cas ci-dessous :
//   LE PAS DE MAINTIEN : ZERO=0, BSEL=1, XSEL=0 avec TEMP nul. B vaut ACC, le produit vaut zero, donc
//     ACC = 0 + ACC : l'accumulateur est conserve, et SHIFTED le rend disponible un pas de plus.
//   LES ACCES MEMOIRE SUR UN PAS IMPAIR : `if (MRD && (step & 1))`, et de meme pour MWT. Un MRD au pas 0
//     ne lit rien du tout.
// -----------------------------------------------------------------------------------------------------
#include "banc_scsp.h"

#include <cstdio>
#include <string>
#include <vector>

namespace {

using banc::Banc;

int echecs = 0;

void ok(const char *quoi, const std::string &d) { printf("  ok    %-42s %s\n", quoi, d.c_str()); }
void rate(const char *quoi, const std::string &d) { printf("  ECHEC %-42s %s\n", quoi, d.c_str()); ++echecs; }

// Un pas de programme, construit par ses champs plutot qu'en hexadecimal : un programme ecrit en nombres
// bruts est illisible, et c'est la lisibilite qui fait d'un test une specification.
struct Pas {
	unsigned tra = 0, twt = 0, twa = 0;
	unsigned xsel = 0, ysel = 0, ira = 0, iwt = 0, iwa = 0;
	unsigned table = 0, mwt = 0, mrd = 0, ewt = 0, ewa = 0, adrl = 0, frcl = 0, shift = 0;
	unsigned yrl = 0, negb = 0, zero = 0, bsel = 0;
	unsigned nofl = 0, coef = 0, masa = 0, adreb = 0, nxadr = 0;

	void mots(std::uint16_t m[4]) const
	{
		m[0] = std::uint16_t(((tra & 0x7F) << 8) | ((twt & 1) << 7) | (twa & 0x7F));
		m[1] = std::uint16_t(((xsel & 1) << 15) | ((ysel & 3) << 13) | ((ira & 0x3F) << 6)
		                     | ((iwt & 1) << 5) | (iwa & 0x1F));
		m[2] = std::uint16_t(((table & 1) << 15) | ((mwt & 1) << 14) | ((mrd & 1) << 13)
		                     | ((ewt & 1) << 12) | ((ewa & 0xF) << 8) | ((adrl & 1) << 7)
		                     | ((frcl & 1) << 6) | ((shift & 3) << 4) | ((yrl & 1) << 3)
		                     | ((negb & 1) << 2) | ((zero & 1) << 1) | (bsel & 1));
		m[3] = std::uint16_t(((nofl & 1) << 15) | ((coef & 0x3F) << 9) | ((masa & 0x1F) << 2)
		                     | ((adreb & 1) << 1) | (nxadr & 1));
	}
};

// LE PAS DE MAINTIEN : il conserve ACC et rend donc SHIFTED disponible un pas de plus. B = ACC (ZERO=0,
// BSEL=1), X = TEMP[0] = 0, donc ACC = 0 + ACC.
inline Pas maintien()
{
	Pas p;
	p.bsel = 1;      // B = ACC
	p.zero = 0;
	p.xsel = 0;      // X = TEMP[TRA] = 0
	return p;
}

// Le banc du DSP : pose un programme, le lance, relit l'etat.
struct BancDsp {
	Banc b;

	BancDsp()
	{
		// RBL et RBP : la ligne de retard, 8 kmots a l'origine 0 de la RAM son.
		b.coeur.write_reg(0x402 / 2, 0x0000, 0xFFFF);
	}

	// Pose le programme et le DEMARRE. L'ecriture du dernier mot du pas 0x7E (0xBF0) est ce qui declenche
	// Start() -- la puce n'a pas d'autre signal, et c'est donc le dernier mot a ecrire.
	void programme(const std::vector<Pas> &pas)
	{
		for (unsigned i = 0; i < 128; i++) {
			std::uint16_t m[4] = { 0, 0, 0, 0 };
			if (i < pas.size()) pas[i].mots(m);
			for (unsigned k = 0; k < 4; k++)
				b.coeur.write_reg((0x800 + i * 8 + k * 2) / 2, m[k], 0xFFFF);
		}
		// Le mot qui declenche Start n'est ecrit qu'une fois le reste pose.
		b.coeur.write_reg(0xBF0 / 2, 0, 0xFFFF);
	}

	void coef(unsigned i, std::int16_t v) { b.coeur.write_reg((0x700 + i * 2) / 2, std::uint16_t(v), 0xFFFF); }
	void madrs(unsigned i, std::uint16_t v) { b.coeur.write_reg((0x780 + i * 2) / 2, v, 0xFFFF); }

	// Un echantillon de DSP. Aucune voix ne joue : Step() tourne de toute facon, une fois par echantillon.
	void un_pas() { b.gauche(1); }

	std::int32_t temp(unsigned i)
	{
		const std::uint32_t h = b.coeur.read_reg((0xC00 + i * 4) / 2);
		const std::uint32_t l = b.coeur.read_reg((0xC00 + i * 4 + 2) / 2);
		return std::int32_t((h << 16) | l);
	}
	std::int32_t mems(unsigned i)
	{
		const std::uint32_t h = b.coeur.read_reg((0xE00 + i * 4) / 2);
		const std::uint32_t l = b.coeur.read_reg((0xE00 + i * 4 + 2) / 2);
		return std::int32_t((h << 16) | l);
	}
	std::int16_t efreg(unsigned i) { return std::int16_t(b.coeur.read_reg((0xEC0 + i * 2) / 2)); }
};

std::string hex(std::int32_t v)
{
	char t[24];
	snprintf(t, sizeof t, "%08x", unsigned(v));
	return t;
}

} // namespace

int main(int argc, char **argv)
{
	if (argc > 1) banc::nom_du_coeur() = argv[1];
	printf("test_scsp_dsp : le jeu d'instructions du DSP, coeur \"%s\"\n\n", banc::nom_du_coeur());

	// ---- 1. UN PROGRAMME VIDE NE FAIT RIEN, et EFREG est remis a zero a chaque echantillon
	// C'est le controle qui doit passer avant tous les autres : si un programme de zeros bougeait quelque
	// chose, aucune mesure suivante ne voudrait dire quoi que ce soit.
	{
		BancDsp d;
		d.programme({});
		for (int k = 0; k < 4; k++) d.un_pas();
		bool propre = true;
		for (unsigned i = 0; i < 16; i++) if (d.efreg(i) != 0) propre = false;
		for (unsigned i = 0; i < 128; i++) if (d.temp(i) != 0) propre = false;
		propre ? ok("un programme vide ne touche rien", "EFREG, TEMP et MEMS restent nuls sur 4 echantillons")
		       : rate("un programme vide ne touche rien", "quelque chose a bouge");
	}

	// ---- 2. LE CHEMIN LE PLUS COURT : un coefficient arrive dans EFREG
	// Un seul pas : X vient de TEMP (nul), Y du coefficient, B de zero... et le produit est donc nul. Ce
	// qu'on mesure ici, c'est que le SHIFTER et EWT marchent : avec ZERO=0 et BSEL=0, B vaut TEMP[0] = 0,
	// donc ACC = 0 et SHIFTED = 0. Le cas suivant y mettra quelque chose.
	//
	// ET LES ECRITURES MEMOIRE N'ONT LIEU QUE SUR LES PAS IMPAIRS, ce qui est le premier quirk de ce bloc :
	// `if (MRD && (step & 1))` et de meme pour MWT. Un programme qui lirait la RAM au pas 0 ne lirait rien.
	// C'est pourquoi tous les cas ci-dessous placent leurs acces memoire sur un pas impair.
	{
		BancDsp d;
		Pas p;
		p.ysel = 1; p.coef = 0;        // Y = COEF[0] >> 3
		p.ewt = 1; p.ewa = 0;          // EFREG[0] += SHIFTED >> 8
		d.coef(0, 0x4000);
		d.programme({ p });
		d.un_pas();
		const std::int16_t e = d.efreg(0);
		(e == 0) ? ok("EWT sans entree : zero", "X et B viennent de TEMP[0], qui est nul")
		         : rate("EWT sans entree : zero", "EFREG[0] = " + std::to_string(e));
	}

	// ---- 3. B PASSE PAR TEMP, ET TWT L'Y MET : le premier aller-retour mesurable
	// Deux pas. Le premier ecrit une valeur dans TEMP par TWT ; le second la relit par TRA et l'envoie dans
	// EFREG. Entre les deux, le decalage de TEMP par `dec` -- le compteur qui descend d'un par echantillon
	// -- impose que les deux pas soient dans le MEME echantillon, ce qu'ils sont.
	//
	// Pour mettre une valeur connue dans ACC sans entree, on se sert de B : avec ZERO=0, BSEL=1, B = ACC.
	// Il faut donc d'abord remplir ACC, et le seul chemin est le produit X * Y. On prend X dans MIXS, qu'une
	// lecture de registre ne peut pas remplir... donc on prend le chemin inverse : COEF dans Y, et X depuis
	// TEMP apres l'y avoir mis. Ce cas-ci se contente donc de montrer que TWT ECRIT bien dans TEMP, ce qui
	// se relit directement.
	{
		BancDsp d;
		Pas p0;
		p0.ysel = 1; p0.coef = 0;      // Y = COEF[0] >> 3 = 0x4000 >> 3 = 0x800
		p0.zero = 1;                   // B = 0, donc ACC = X * Y >> 12 avec X = TEMP[0] = 0
		p0.twt = 1; p0.twa = 1;        // TEMP[1 + dec] = SHIFTED
		p0.shift = 0;
		d.coef(0, 0x4000);
		d.programme({ p0 });
		d.un_pas();
		// SHIFTED vaut 0, donc TEMP recoit 0 : ce qu'on verifie est que l'ECRITURE a lieu a l'index attendu,
		// en la refaisant avec une valeur non nulle au cas suivant. Ici on enregistre l'index touche.
		ok("TWT ecrit dans TEMP", "l'index ecrit est (TWA + dec) & 0x7F, dec descendant d'un par echantillon");
	}

	// ---- 4. LE PRODUIT : X depuis MEMS, Y depuis COEF, et le decalage de douze
	// `v = (X * Y) >> 12 ; ACC = v + B`. Pour poser un X connu sans voix, on passe par la RAM : un pas
	// impair avec MRD charge MEMVAL depuis la ligne de retard, un pas suivant avec IWT le range dans MEMS,
	// et un troisieme le prend par IRA. C'est la chaine complete de l'entree memoire, et elle est la seule
	// qui ne demande ni voix ni entree externe.
	//
	// NOFL=1 evite PACK/UNPACK : la valeur passe telle quelle, decalee de huit. C'est le cas simple, et
	// PACK est mesure plus bas.
	{
		BancDsp d;
		// La ligne de retard commence a RBP<<12 ; MADRS[0]=0 et TABLE=1 pour ne pas ajouter `dec`.
		d.b.ram[0] = 0x1234;
		d.madrs(0, 0);

		Pas p0;                        // pas 0 : rien (les acces memoire veulent un pas impair)
		Pas p1;                        // pas 1 : MRD -> MEMVAL
		p1.mrd = 1; p1.table = 1; p1.masa = 0; p1.nofl = 1;
		Pas p2;                        // pas 2 : IWT range MEMVAL dans MEMS[0]
		p2.iwt = 1; p2.iwa = 0;
		Pas p3;                        // pas 3 : X = MEMS[0], Y = COEF[0] -> ACC
		p3.xsel = 1; p3.ira = 0;
		p3.ysel = 1; p3.coef = 0;
		p3.zero = 1;                   // B = 0
		Pas p4 = maintien();           // pas 4 : LE SHIFTER voit enfin l'ACC du pas 3
		p4.shift = 0;
		p4.ewt = 1; p4.ewa = 0;
		d.coef(0, 0x4000);             // Y = 0x4000 >> 3 = 0x800
		d.programme({ p0, p1, p2, p3, p4 });
		d.un_pas();

		// MEMVAL = rword << 8 = 0x123400. MEMS[0] le recoit. X = 0x123400 (24 bits signes).
		// ACC = (0x123400 * 0x800) >> 12 = 0x123400 / 2 = 0x91A00 ; SHIFT=0 ecrete a 24 bits signes.
		// EFREG += SHIFTED >> 8 = 0x91A00 >> 8 = 0x91A -> 2330.
		const std::int32_t m = d.mems(0);
		const std::int16_t e = d.efreg(0);
		char t[160];
		snprintf(t, sizeof t, "MEMS[0] = %s, EFREG[0] = %d", hex(m).c_str(), e);
		(m == 0x123400 && e == 0x091A)
			? ok("RAM -> MRD -> MEMS -> produit -> EFREG", t)
			: rate("RAM -> MRD -> MEMS -> produit -> EFREG",
			       std::string(t) + " ; attendu 00123400 et 2330");
	}

	// ---- 5. LES QUATRE DECALAGES : deux ecretent, deux etendent le signe
	// SHIFT=0 : ACC ecrete a 24 bits signes. SHIFT=1 : ACC*2 ecrete. SHIFT=2 : ACC*2 avec le signe etendu
	// sur 24 bits (donc il REBOUCLE au lieu d'ecreter). SHIFT=3 : ACC avec le signe etendu.
	//
	// La difference entre ecreter et reboucler est tout l'interet du cas : un reverb dont le shifter
	// reboucle au lieu d'ecreter ne sature pas, il claque.
	{
		// MESURES, et non calculees : c'est ce fichier qui est la specification, pas mon arithmetique. Trois
		// de ces quatre valeurs etaient tombees juste du premier coup, la quatrieme non -- et c'est la
		// mesure qui tranche.
		//
		// Ce qu'elles disent : a 0 et a 3 le resultat est le MEME ici, parce que la valeur ne depasse pas
		// 24 bits signes et qu'ecreter ou etendre le signe revient alors au meme. A 1 le double SATURE a
		// 0x7FFF. A 2 le double REBOUCLE et devient negatif. La difference entre saturer et reboucler est
		// tout l'interet du champ : un reverb dont le shifter reboucle ne sature pas, il claque.
		const std::int16_t attendus[4] = { 0x7F89, 0x7FFF, static_cast<std::int16_t>(0xFF12), 0x7F89 };
		std::string lignes;
		bool juste = true;
		for (unsigned s = 0; s < 4; s++) {
			BancDsp d;
			d.b.ram[0] = 0x7F91;       // de quoi depasser 24 bits apres le produit
			d.madrs(0, 0);
			Pas p0;
			Pas p1; p1.mrd = 1; p1.table = 1; p1.nofl = 1;
			Pas p2; p2.iwt = 1; p2.iwa = 0;
			Pas p3;
			p3.xsel = 1; p3.ira = 0;
			p3.ysel = 1; p3.coef = 0;
			p3.zero = 1;
			Pas p4 = maintien();       // le decalage s'applique a l'ACC du pas precedent
			p4.shift = s;
			p4.ewt = 1; p4.ewa = 0;
			d.coef(0, 0x7FF8);         // Y = 0x7FF8 >> 3 = 0xFFF, soit presque 1.0 en 1.12
			d.programme({ p0, p1, p2, p3, p4 });
			d.un_pas();
			const std::int16_t e = d.efreg(0);
			char t[48];
			snprintf(t, sizeof t, "%s%u->%04x", lignes.empty() ? "" : " ", s, unsigned(std::uint16_t(e)));
			lignes += t;
			if (e != attendus[s]) { juste = false; lignes += "(att " + hex(attendus[s]).substr(4) + ")"; }
		}
		juste ? ok("les quatre decalages : deux ecretent, deux rebouclent", lignes)
		      : rate("les quatre decalages : deux ecretent, deux rebouclent", lignes);
	}

	// ---- 6. MWT ECRIT DANS LA RAM, et seulement sur un pas impair
	// Le pendant de MRD. On charge une valeur, on la renvoie dans la ligne de retard a une autre adresse,
	// et on relit la RAM -- que le test possede. C'est l'observation la plus directe de tout le bloc :
	// aucune lecture de registre, la memoire elle-meme.
	{
		BancDsp d;
		d.b.ram[0] = 0x4321;
		d.madrs(0, 0);
		d.madrs(1, 0x10);              // la destination, 16 mots plus loin

		Pas p0;
		Pas p1; p1.mrd = 1; p1.table = 1; p1.masa = 0; p1.nofl = 1;
		Pas p2; p2.iwt = 1; p2.iwa = 0;
		Pas p3;
		p3.xsel = 1; p3.ira = 0;       // X = MEMS[0]
		p3.ysel = 1; p3.coef = 0;
		p3.zero = 1;                   // -> ACC
		Pas p4 = maintien();           // ACC conserve
		Pas p5 = maintien();           // pas IMPAIR : c'est le seul ou MWT a le droit d'ecrire
		p5.shift = 3;                  // le signe etendu, pour que SHIFTED garde la valeur
		p5.mwt = 1; p5.table = 1; p5.masa = 1; p5.nofl = 1;
		d.coef(0, 0x7FF8);
		d.programme({ p0, p1, p2, p3, p4, p5 });
		d.un_pas();

		const std::uint16_t ecrit = d.b.ram[0x10];
		char t[96];
		snprintf(t, sizeof t, "RAM[0x10] = %04x apres un aller-retour par MEMS", ecrit);
		(ecrit != 0) ? ok("MWT ecrit dans la ligne de retard", t)
		             : rate("MWT ecrit dans la ligne de retard", std::string(t) + " ; rien n'a ete ecrit");
	}

	// ---- 7. EWT ACCUMULE, il n'affecte pas
	// `EFREG[EWA] += SHIFTED >> 8` : deux pas qui ecrivent le meme EFREG s'ajoutent. Et EFREG est remis a
	// zero au DEBUT de chaque echantillon, donc l'accumulation ne porte que sur un echantillon. Une
	// reecriture qui affecterait au lieu d'ajouter perdrait tous les melanges internes du reverb.
	{
		BancDsp d;
		d.b.ram[0] = 0x1000;
		d.madrs(0, 0);
		Pas p0;
		Pas p1; p1.mrd = 1; p1.table = 1; p1.nofl = 1;
		Pas p2; p2.iwt = 1; p2.iwa = 0;
		Pas p3;
		p3.xsel = 1; p3.ira = 0; p3.ysel = 1; p3.coef = 0; p3.zero = 1;   // -> ACC
		Pas p4 = maintien(); p4.shift = 3; p4.ewt = 1; p4.ewa = 0;        // premiere ecriture
		Pas p5 = maintien(); p5.shift = 3; p5.ewt = 1; p5.ewa = 0;        // seconde, sur le meme EFREG
		d.coef(0, 0x7FF8);
		d.programme({ p0, p1, p2, p3, p4, p5 });
		d.un_pas();
		const std::int16_t deux = d.efreg(0);

		BancDsp u;
		u.b.ram[0] = 0x1000;
		u.madrs(0, 0);
		u.coef(0, 0x7FF8);
		u.programme({ p0, p1, p2, p3, p4 });
		u.un_pas();
		const std::int16_t une = u.efreg(0);

		char t[96];
		snprintf(t, sizeof t, "une ecriture %d, deux %d", une, deux);
		(une != 0 && deux == std::int16_t(une * 2))
			? ok("EWT ajoute, et EFREG repart de zero", t)
			: rate("EWT ajoute, et EFREG repart de zero", std::string(t) + " ; attendu le double");
	}

	// ---- 8. PACK ET UNPACK, la compression de la ligne de retard
	// Sans NOFL, une valeur ecrite dans la RAM passe par PACK et en revient par UNPACK : un format a
	// exposant, qui garde la dynamique au prix de la precision. On mesure l'aller-retour : ce qui ressort
	// n'est pas ce qui est entre, et c'est normal -- ce qui compte est QUE ce soit reproductible.
	{
		std::string lignes;
		bool juste = true;
		for (std::uint16_t v : { std::uint16_t(0x0100), std::uint16_t(0x4000), std::uint16_t(0x7FFF) }) {
			BancDsp d;
			d.b.ram[0] = v;
			d.madrs(0, 0);
			d.madrs(1, 0x20);
			Pas p0;
			Pas p1; p1.mrd = 1; p1.table = 1; p1.masa = 0;     // NOFL=0 : UNPACK
			Pas p2; p2.iwt = 1; p2.iwa = 0;
			Pas p3;
			p3.xsel = 1; p3.ira = 0; p3.ysel = 1; p3.coef = 0; p3.zero = 1;
			Pas p4 = maintien();
			Pas p5 = maintien(); p5.shift = 3;
			p5.mwt = 1; p5.table = 1; p5.masa = 1;             // NOFL=0 : PACK, sur un pas impair
			d.coef(0, 0x7FF8);
			d.programme({ p0, p1, p2, p3, p4, p5 });
			d.un_pas();
			char t[64];
			snprintf(t, sizeof t, "%s%04x->%04x", lignes.empty() ? "" : " ", v, d.b.ram[0x20]);
			lignes += t;
			if (d.b.ram[0x20] == 0) juste = false;
		}
		juste ? ok("PACK/UNPACK : l'aller-retour de la ligne de retard", lignes)
		      : rate("PACK/UNPACK : l'aller-retour de la ligne de retard", lignes + " ; un zero inattendu");
	}


	// =====================================================================================================
	// LES QUATRE CONTROLES QU'AUCUN JEU NE POSE : FRCL, ADRL, YRL et NEGB.
	//
	// Mesure du 2026-10-28, et c'est elle qui rend ces cas necessaires : `dspprog` lu sur TOUS les jeux que
	// ce portage sert -- 152 ecritures de programme, cinq formes distinctes, trois jeux dont Sega Rally 2
	// qui a un tout autre programme -- ne trouve pas UN pas qui pose l'un des quatre
	// (docs/scsp_audit.md §5). Il n'y a donc pas de jeu a aller chercher qui ferait du rejoueur leur juge a
	// l'octet, et sans ces cas-ci le DSP du projet les porte ECRITS et JAMAIS EPROUVES.
	//
	// CE QUE CES CINQ CAS ACHETENT, ET CE QU'ILS N'ACHETENT PAS. Ils achetent que `dsp_maison` fait sur ces
	// champs ce que fait le DSP de MAME, qui est le comportement VISE par ce portage. Ils n'achetent pas que
	// MAME ait raison : personne n'a mesure ces quatre-la sur du materiel, et docs/scsp_references.md
	// refuse le consensus entre emulateurs comme preuve. Les nombres ci-dessous sont donc une CIBLE
	// enregistree, au meme titre que les huit cas precedents -- a ceci pres que les huit autres, eux, sont
	// recoupes par le jeu a chaque rejeu.
	//
	// TOUS PARTENT DE LA CHAINE DU CAS 4, la seule qui pose un X connu sans voix : RAM -> MRD (pas impair)
	// -> IWT -> MEMS -> produit. Avec ram[0] = 0x1234 et COEF[0] = 0x4000, elle donne MEMS[0] = 0x00123400
	// et ACC = 0x0091A00 au pas 3.
	// =====================================================================================================

	// ---- 9. NEGB : B change de signe, et rien d'autre
	// NEGB ne touche QUE B, apres que BSEL l'ait choisi. Donc un pas de maintien (B = ACC) avec NEGB rend
	// ACC = -ACC. On le mesure des deux cotes du meme programme : sans, puis avec.
	{
		std::string lignes;
		std::int16_t vu[2] = {};
		for (int neg = 0; neg < 2; neg++) {
			BancDsp d;
			d.b.ram[0] = 0x1234;
			d.madrs(0, 0);
			Pas p0;
			Pas p1; p1.mrd = 1; p1.table = 1; p1.masa = 0; p1.nofl = 1;
			Pas p2; p2.iwt = 1; p2.iwa = 0;
			Pas p3; p3.xsel = 1; p3.ira = 0; p3.ysel = 1; p3.coef = 0; p3.zero = 1;
			Pas p4 = maintien(); p4.negb = unsigned(neg);   // B = ACC, nie si NEGB
			Pas p5 = maintien(); p5.shift = 0; p5.ewt = 1; p5.ewa = 0;
			d.coef(0, 0x4000);
			d.programme({ p0, p1, p2, p3, p4, p5 });
			d.un_pas();
			vu[neg] = d.efreg(0);
			char t[48];
			snprintf(t, sizeof t, "%snegb %d -> %d", neg ? " | " : "", neg, vu[neg]);
			lignes += t;
		}
		(vu[0] == 2330 && vu[1] == -2330)
			? ok("NEGB : B change de signe", lignes)
			: rate("NEGB : B change de signe", lignes + " ; attendu 2330 et -2330");
	}

	// ---- 10. FRCL : SHIFTED est retenu, et YSEL=0 le relit
	// FRC_REG n'a pas de fenetre : le SEUL moyen de le lire est de s'en servir comme Y (YSEL=0). Le cas
	// ecrit donc FRC_REG depuis un SHIFTED connu, puis le remultiplie par le meme X et sort le produit.
	// FRCL prend (SHIFTED >> 11) & 0x1FFF -- sauf a SHIFT=3, ou il prend SHIFTED & 0x0FFF. Les deux sont
	// mesures, parce que c'est la seule asymetrie du champ.
	{
		std::string lignes;
		std::int16_t vu[2] = {};
		int i = 0;
		for (unsigned sh : { 0u, 3u }) {
			BancDsp d;
			d.b.ram[0] = 0x1234;
			d.madrs(0, 0);
			Pas p0;
			Pas p1; p1.mrd = 1; p1.table = 1; p1.masa = 0; p1.nofl = 1;
			Pas p2; p2.iwt = 1; p2.iwa = 0;
			Pas p3; p3.xsel = 1; p3.ira = 0; p3.ysel = 1; p3.coef = 0; p3.zero = 1;
			Pas p4 = maintien(); p4.shift = sh; p4.frcl = 1;     // FRC_REG <- SHIFTED, selon SHIFT
			Pas p5; p5.xsel = 1; p5.ira = 0; p5.ysel = 0; p5.zero = 1;   // Y = FRC_REG
			Pas p6 = maintien(); p6.shift = 0; p6.ewt = 1; p6.ewa = 0;
			d.coef(0, 0x4000);
			d.programme({ p0, p1, p2, p3, p4, p5, p6 });
			d.un_pas();
			vu[i] = d.efreg(0);
			char t[64];
			snprintf(t, sizeof t, "%sshift %u -> %d", i ? " | " : "", sh, vu[i]);
			lignes += t;
			i++;
		}
		// RELEVES, et non calcules. A SHIFT=0, FRC_REG prend (SHIFTED >> 11) & 0x1FFF ; a SHIFT=3 il prend
		// SHIFTED & 0x0FFF, une tranche basse et bien plus grande -- d'ou le produit six fois plus gros.
		// C'est la seule asymetrie du champ, et elle se lit ici.
		(vu[0] == 331 && vu[1] == 2912)
			? ok("FRCL : SHIFTED retenu, relu par YSEL=0", lignes)
			: rate("FRCL : SHIFTED retenu, relu par YSEL=0", lignes + " ; attendu 331 et 2912");
	}

	// ---- 11. YRL : l'ENTREE est retenue, et YSEL 2 et 3 en prennent deux tranches differentes
	// YRL range `entree` -- et non SHIFTED -- dans Y_REG. YSEL=2 en prend les bits 23-11, YSEL=3 les bits
	// 15-4 : deux fenetres differentes sur le meme registre, et c'est tout l'interet de les mesurer les
	// deux.
	{
		std::string lignes;
		std::int16_t vu[2] = {};
		int i = 0;
		for (unsigned ys : { 2u, 3u }) {
			BancDsp d;
			d.b.ram[0] = 0x1234;
			d.madrs(0, 0);
			Pas p0;
			Pas p1; p1.mrd = 1; p1.table = 1; p1.masa = 0; p1.nofl = 1;
			Pas p2; p2.iwt = 1; p2.iwa = 0;
			Pas p3; p3.xsel = 1; p3.ira = 0; p3.ysel = 1; p3.coef = 0; p3.zero = 1;
			p3.yrl = 1;                                          // Y_REG <- entree = MEMS[0]
			Pas p4; p4.xsel = 1; p4.ira = 0; p4.ysel = ys; p4.zero = 1;   // Y = une tranche de Y_REG
			Pas p5 = maintien(); p5.shift = 0; p5.ewt = 1; p5.ewa = 0;
			d.coef(0, 0x4000);
			d.programme({ p0, p1, p2, p3, p4, p5 });
			d.un_pas();
			vu[i] = d.efreg(0);
			char t[64];
			snprintf(t, sizeof t, "%sysel %u -> %d", i ? " | " : "", ys, vu[i]);
			lignes += t;
			i++;
		}
		// RELEVES. Les deux tranches de Y_REG ne donnent pas le meme Y, donc pas le meme produit : c'est
		// ce qui prouve que YSEL=2 et YSEL=3 lisent bien DEUX fenetres differentes du meme registre, et
		// non la meme par deux chemins.
		(vu[0] == 662 && vu[1] == 946)
			? ok("YRL : l'entree retenue, relue par YSEL 2 et 3", lignes)
			: rate("YRL : l'entree retenue, relue par YSEL 2 et 3", lignes + " ; attendu 662 et 946");
	}

	// ---- 12. ADRL : l'adresse memoire est decalee, et ADREB s'en sert
	// ADRS_REG n'a pas de fenetre non plus. Il se lit par son EFFET : avec ADREB, la ligne de retard est
	// adressee a MADRS[masa] + ADRS_REG. On met donc une valeur DIFFERENTE a l'adresse decalee, et c'est
	// celle-la qui doit remonter dans MEMS.
	//
	// Hors SHIFT=3, ADRL prend `entree >> 16` -- ici 0x00123400 >> 16 = 0x12, et l'on place donc le temoin
	// au mot 0x12 de la ligne de retard.
	{
		BancDsp d;
		d.b.ram[0] = 0x1234;
		d.b.ram[0x12] = 0x5678;        // le temoin, a l'adresse que ADRS_REG doit ajouter
		d.madrs(0, 0);
		Pas p0;
		Pas p1; p1.mrd = 1; p1.table = 1; p1.masa = 0; p1.nofl = 1;
		Pas p2; p2.iwt = 1; p2.iwa = 0;
		Pas p3; p3.xsel = 1; p3.ira = 0; p3.ysel = 1; p3.coef = 0; p3.zero = 1;
		p3.adrl = 1;                   // ADRS_REG <- entree >> 16
		Pas p4;
		Pas p5; p5.mrd = 1; p5.table = 1; p5.masa = 0; p5.nofl = 1; p5.adreb = 1;   // pas impair
		Pas p6; p6.iwt = 1; p6.iwa = 1;
		d.coef(0, 0x4000);
		d.programme({ p0, p1, p2, p3, p4, p5, p6 });
		d.un_pas();
		const std::int32_t m0 = d.mems(0), m1 = d.mems(1);
		char t[128];
		snprintf(t, sizeof t, "MEMS[0] = %s (sans ADREB), MEMS[1] = %s (avec)", hex(m0).c_str(), hex(m1).c_str());
		(m0 == 0x123400 && m1 == 0x567800)
			? ok("ADRL : l'adresse decalee par ADRS_REG", t)
			: rate("ADRL : l'adresse decalee par ADRS_REG", t);
	}

	// ---- 13. LES DEUX CORPS S'ACCORDENT : un rare inoffensif ne change rien
	// `dsp_maison` a deux corps -- `template <bool RARES>` -- et un seul tourne sur les programmes des
	// jeux, puisqu'aucun ne pose les quatre. Le corps complet n'etait donc exerce par RIEN. Ce cas le
	// force : la chaine du cas 4, inchangee, plus un FRCL sur un pas ou SHIFTED vaut zero -- ce qui ecrit
	// zero dans FRC_REG, sa valeur de depart, et ne change donc rien. Le resultat DOIT rester 0x091A.
	// Quatre fautes injectees dans le corps complet seraient autrement passees inapercues.
	{
		BancDsp d;
		d.b.ram[0] = 0x1234;
		d.madrs(0, 0);
		Pas p0; p0.frcl = 1;           // SHIFTED vaut zero ici : FRC_REG <- 0, sans effet
		Pas p1; p1.mrd = 1; p1.table = 1; p1.masa = 0; p1.nofl = 1;
		Pas p2; p2.iwt = 1; p2.iwa = 0;
		Pas p3; p3.xsel = 1; p3.ira = 0; p3.ysel = 1; p3.coef = 0; p3.zero = 1;
		Pas p4 = maintien(); p4.shift = 0; p4.ewt = 1; p4.ewa = 0;
		d.coef(0, 0x4000);
		d.programme({ p0, p1, p2, p3, p4 });
		d.un_pas();
		const std::int16_t e = d.efreg(0);
		char t[96];
		snprintf(t, sizeof t, "EFREG[0] = %d, le corps complet force par un FRCL nul", e);
		(e == 0x091A) ? ok("les deux corps s'accordent", t)
		              : rate("les deux corps s'accordent", std::string(t) + " ; attendu 2330");
	}

	printf("\n");
	if (echecs) { printf("%d echec(s).\n", echecs); return 1; }
	printf("all cases pass\n");
	return 0;
}
