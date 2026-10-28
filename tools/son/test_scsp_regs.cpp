// test_scsp_regs: ce qu'une ecriture dans un registre du SCSP se relit.
//
// Bloc 1 de docs/scsp_architecture.md §3 -- "Registres et semantique d'acces [...] Se teste sans aucun son :
// ecrire, relire" -- et le plan de migration veut ces tests ECRITS AVANT les blocs qu'ils testent. Il n'y a
// donc pas encore de banc de registres autonome : ce fichier decrit ce que le coeur de MAME rend
// aujourd'hui, par l'interface neuve, pour qu'une implementation future ait une cible et non une intention.
//
// CE QU'IL MESURE : pour chaque mot de 0x000 a 0x42F, DEUX masques -- ce qu'une ecriture LEVE, et ce qui
// RESTE leve quand on reecrit zero. On ecrit 0x0000 et on relit, puis 0xFFFF et on relit, puis 0x0000 et on
// relit. Un bit qui se relit toujours a 1 ou toujours a 0 n'est pas rendu par le registre, il est cable ;
// un bit qui monte et ne retombe pas est un bit que l'ecriture POSE, et il y en a (SCIPD, MCIPD).
//
// CE QU'IL NE TOUCHE PAS, et pourquoi -- ecrire a l'aveugle dans un chip a des effets de bord :
//   0x416  DEXE, qui LANCE un transfert DMA et ecrirait dans la RAM son ;
//   0x400  MVOL et compagnie, qui changent le rendu de tout le reste ;
//   les mots 0x0 des fentes, dont le bit 12 (KYONEX) declenche les 32 voix.
// Ces trois-la sont nommes plutot que silencieusement sautes, et le compte final le verifie.
//
// ET C'EST UN TEST A REFERENCE ENREGISTREE. Les masques ci-dessous ont ete MESURES sur le coeur de MAME le
// 2026-10-26, puis figes ici. Le test ne prouve donc pas qu'ils sont justes au regard du materiel -- il
// prouve qu'ils n'ont pas change. C'est exactement ce qu'on veut d'un filet avant de reecrire un bloc, et
// ce n'est pas plus que ca.
#include "../../src/sound/scsp_contract.h"
#include "../../src/sound/scsp_select.h"

#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace {

int echecs = 0;

void ok(const char *quoi, const std::string &d) { printf("  ok    %-40s %s\n", quoi, d.c_str()); }
void rate(const char *quoi, const std::string &d) { printf("  ECHEC %-40s %s\n", quoi, d.c_str()); ++echecs; }

struct Muet final : scsp::sink {
	void irq(int, bool) override {}
	void midi_out(std::uint8_t) override {}
	void timer(int, std::uint32_t, std::uint32_t) override {}
};

constexpr std::uint32_t RAM_OCTETS = 0x100000;

// Les mots qu'on n'ecrit pas, avec la raison.
bool intouchable(std::uint32_t octet, const char *&pourquoi)
{
	if (octet < 0x400 && (octet & 0x1F) == 0x00) { pourquoi = "mot 0 d'une fente : KYONEX declenche les voix"; return true; }
	if (octet == 0x400) { pourquoi = "MVOL, MEM4MB, DAC18B : changent le rendu de tout le reste"; return true; }
	if (octet == 0x416) { pourquoi = "DEXE : lance un DMA et ecrirait dans la RAM son"; return true; }
	return false;
}

// Le reglage du miroir MADRS n'est PAS dans scsp::core, et scsp_core.h dit pourquoi : il appartient au
// portage et non a la puce. Il passe donc par scsp_select.h, qui le fait repondre par le coeur INSCRIT --
// c'etait la troisieme copie du meme dynamic_cast dans ce depot, et elle liait MAME a ce test pour rien.
bool pose_miroir(scsp::core &c, bool on) { return scsp::regle_miroir_madrs(c, on); }

} // namespace

int main(int argc, char **argv)
{
	const char *nom = argc > 1 ? argv[1] : "mame";
	printf("test_scsp_regs : ce qu'une ecriture se relit, par scsp::core -- coeur \"%s\"\n\n", nom);

	std::vector<std::uint16_t> ram(RAM_OCTETS / 2, 0);
	Muet muet;
	std::unique_ptr<scsp::core> p = scsp::make(nom, 45158400 / 2, ram.data(), RAM_OCTETS, muet);
	if (!p) return 2;
	scsp::core &coeur = *p;
	coeur.reset();

	// ---- 1. le masque utile de chaque mot, mesure
	//
	// DEUX MASQUES ET NON UN, parce qu'un registre ne suit pas forcement l'ecriture dans les deux sens. La
	// premiere version de cette boucle faisait `hauts & ~bas` apres avoir ecrit 0xFFFF puis 0x0000, et elle
	// rendait 0x0000 pour SCIPD et MCIPD -- ce qui ressemble a "aucun bit ne se relit" alors que ces deux-la
	// sont COLLANTS : l'implementation les OR ("SCIPD and MCIPD are r/o except for bit 5"), donc ecrire 0 ne
	// les eteint pas, et la soustraction effaçait justement le bit qu'on cherchait. On mesure donc ce qui
	// MONTE a l'ecriture, et separement ce qui RESTE leve quand on reecrit zero.
	const std::size_t N_MOTS = 0x430 / 2;
	std::vector<std::uint16_t> monte(N_MOTS, 0), colle(N_MOTS, 0);
	unsigned sautes = 0;
	for (std::uint32_t octet = 0; octet < 0x430; octet += 2) {
		const char *pourquoi = nullptr;
		if (intouchable(octet, pourquoi)) { ++sautes; continue; }
		const std::uint32_t mot = octet / 2;
		coeur.write_reg(mot, 0x0000, 0xFFFF);
		const std::uint16_t a0 = coeur.read_reg(mot);
		coeur.write_reg(mot, 0xFFFF, 0xFFFF);
		const std::uint16_t a1 = coeur.read_reg(mot);
		coeur.write_reg(mot, 0x0000, 0xFFFF);
		const std::uint16_t a2 = coeur.read_reg(mot);
		monte[mot] = std::uint16_t(a1 & ~a0);        // ce qu'une ecriture leve
		colle[mot] = std::uint16_t(a1 & a2 & ~a0);   // ... et qui ne retombe pas
	}
	ok("balayage de 0x000 a 0x42F", std::to_string(0x430 / 2 - sautes) + " mots ecrits et relus, "
	   + std::to_string(sautes) + " laisses de cote");

	// ---- 2. les trois ecartes sont bien ceux qu'on a dits, et pas d'autres
	{
		unsigned fentes = 0, autres = 0;
		for (std::uint32_t octet = 0; octet < 0x430; octet += 2) {
			const char *p = nullptr;
			if (!intouchable(octet, p)) continue;
			if (octet < 0x400) ++fentes; else ++autres;
		}
		const bool juste = fentes == 32 && autres == 2 && sautes == fentes + autres;
		juste ? ok("les ecartes sont nommes", std::to_string(fentes) + " mots 0 de fente + 0x400 + 0x416")
		      : rate("les ecartes sont nommes",
		             std::to_string(fentes) + " fentes et " + std::to_string(autres) + " autres, pour "
		             + std::to_string(sautes) + " sautes");
	}

	// ---- 3. les registres d'interruption en attente : en lecture seule SAUF le bit 5
	//
	// C'est ecrit en toutes lettres dans l'implementation -- "SCIPD and MCIPD are r/o except for bit 5 CPU
	// irqs" -- et c'est le genre de chose qu'une reecriture oublie.
	//
	// MAIS LE COMMENTAIRE ET LE CODE NE DISENT PAS LA MEME CHOSE, et c'est cette mesure qui l'a montre.
	// L'exception est appliquee a 0x420 et 0x42E ; or 0x42E n'est pas MCIPD, c'est MCIRE -- MCIPD est a
	// 0x42C (UpdateRegR case 0x2c y recopie m_mcipd, UpdateReg case 0x2e fait `m_mcipd &= ~data[0x2e/2]`).
	// Les trois mots portent donc bien un bit 5 collant, mais par trois chemins differents :
	//   0x420 SCIPD : l'ecriture est un OR, le bit reste. L'exception voulue.
	//   0x42C MCIPD : affectation simple, puis UpdateReg voit le bit 5 et appelle MainCheckPendingIRQ(0x20),
	//                 qui leve m_mcipd ; la relecture rend m_mcipd, donc le bit revient.
	//   0x42E MCIRE : recoit l'exception DESTINEE A MCIPD, donc son bit 5 ne peut plus jamais etre eteint,
	//                 alors que son jumeau SCIRE (0x422) est un registre ordinaire (masque 0xFFFF).
	// Ce que ca coute a VF3 : rien, et c'est mesure -- le pilote ne touche ni l'un ni l'autre. Le detail et
	// la recherche qui l'etablit sont dans scsp_contract.h, section 2.
	// On enregistre les trois tels quels : la premiere version de ce test appelait 0x42E "MCIPD" et passait
	// pour la mauvaise raison. Une reecriture decidera de reproduire l'asymetrie ou de la corriger, mais
	// elle le decidera en la voyant.
	for (auto [octet, nom] : { std::pair<std::uint32_t, const char *>{ 0x420, "SCIPD (0x420)" },
	                           std::pair<std::uint32_t, const char *>{ 0x42C, "MCIPD (0x42C)" },
	                           std::pair<std::uint32_t, const char *>{ 0x42E, "MCIRE (0x42E)" } }) {
		const std::uint16_t mo = monte[octet / 2], co = colle[octet / 2];
		char d[64];
		snprintf(d, sizeof d, "monte 0x%04x, colle 0x%04x", mo, co);
		// Et le bit 5 COLLE : c'est une ecriture qui le POSE, pas une qui le recopie. Une reecriture qui
		// ferait une affectation passerait la premiere moitie de ce test et echouerait la seconde.
		(mo == 0x0020 && co == 0x0020)
			? ok(nom, std::string(d) + " : le bit 5 seul, et il reste leve")
			: rate(nom, std::string(d) + ", attendu monte et colle 0x0020");
	}

	// ---- 3 bis. SCIRE, le jumeau qui N'A PAS l'exception
	// L'asymetrie du cas precedent se verifie par son autre bout : du cote esclave c'est SCIPD qui est OR,
	// du cote maitre c'est MCIRE. Donc SCIRE doit etre un registre entier.
	{
		const std::uint16_t mo = monte[0x422 / 2];
		char d[64];
		snprintf(d, sizeof d, "monte 0x%04x", mo);
		(mo == 0xFFFF) ? ok("SCIRE (0x422)", std::string(d) + " : registre entier, pas d'exception de ce cote")
		               : rate("SCIRE (0x422)", std::string(d) + ", attendu 0xFFFF");
	}

	// ---- 3 ter. les deux mots que la LECTURE fabrique
	// Ceux-la ne rendent pas ce qu'on y ecrit parce qu'une lecture les recalcule (UpdateRegR). C'est une
	// divergence connue d'avec le materiel et non un detail d'implementation : pour 0x408, le pilote du
	// Model 3 ecrit MSLC et relit aussitot pour trouver une fente libre, et ce coeur lui rend l'etat de la
	// fente choisie -- comportement delibere, voir le commentaire de UpdateRegR case 8 dans scsp.cpp.
	{
		const std::uint16_t m404 = monte[0x404 / 2], m408 = monte[0x408 / 2];
		char d[160];
		snprintf(d, sizeof d, "0x404 monte 0x%04x (l'octet bas est la file MIDI en entree), "
		                      "0x408 monte 0x%04x (MSLC/CA/SGC/EG, recalcules a chaque lecture)", m404, m408);
		(m404 == 0xFF00 && m408 == 0x0000)
			? ok("0x404 et 0x408 : la lecture les fabrique", d)
			: rate("0x404 et 0x408 : la lecture les fabrique", std::string(d) + ", attendus 0xFF00 et 0x0000");
	}

	// ---- 4. les champs dont le pilote depend se relisent, tous
	// Le contrat dit ou vit chaque champ (scsp_contract.h) ; ici on verifie que le registre le GARDE. Un
	// champ qui ne se relit pas est un champ qu'une reecriture peut perdre sans que rien ne crie.
	{
		std::string perdus;
		for (const scsp_contract::Champ &c : scsp_contract::FENTE) {
			if (c.mot == 0) continue;   // le mot 0 n'est pas ecrit (KYONEX)
			const bool en_place = (c.masque & (c.masque + 1)) != 0;
			const std::uint16_t attendu = std::uint16_t(en_place ? c.masque : (c.masque << c.decalage));
			// fente 0 : le mot c.mot est a l'octet 2 * c.mot
			const std::uint16_t m = monte[c.mot];
			if ((m & attendu) != attendu) perdus += std::string(perdus.empty() ? "" : ", ") + c.nom;
		}
		perdus.empty() ? ok("les 34 champs d'une fente se relisent", "aucun bit perdu par le registre")
		               : rate("les 34 champs d'une fente se relisent", "perdus : " + perdus);
	}

	// ---- 5. la plage contestee 0x7C0-0x7FF, decrite et non jugee
	// Ce portage et MAME n'en font pas la meme chose (docs/scsp_references.md), et aucune lecture du
	// programme ne peut trancher ce que fait la PUCE (docs/fv2-son-programme.md §5). On enregistre donc ce
	// que le coeur d'aujourd'hui rend, sans en faire une verite.
	//
	// ET ON MESURE LES DEUX REGLAGES, pas seulement celui qui est par defaut. m_madrs_mirror sort a `true`,
	// si bien qu'un test qui ecrit 0x7C0 et relit 0x780 trouve la valeur et semble decrire la puce alors
	// qu'il ne decrit qu'un defaut de construction. Le reglage appartient au jeu et non au chip (scsp.h),
	// donc il se pose par l'objet et non par l'interface -- seul endroit de ce fichier qui descende sous
	// scsp::core, et c'est assume : ce test caracterise CE coeur-la.
	{
		std::string lignes;
		bool juste = true;
		for (bool miroir : { false, true }) {
			pose_miroir(coeur, miroir);
			coeur.write_reg(0x7C0 / 2, 0x1234, 0xFFFF);
			coeur.write_reg(0x780 / 2, 0x5678, 0xFFFF);   // MADRS[0] par la voie basse, apres
			const std::uint16_t haut = coeur.read_reg(0x7C0 / 2);
			const std::uint16_t bas = coeur.read_reg(0x780 / 2);
			char t[128];
			snprintf(t, sizeof t, "%smiroir %s : 0x7C0 relu 0x%04x, 0x780 relu 0x%04x",
			         lignes.empty() ? "" : " | ", miroir ? "on " : "off", haut, bas);
			lignes += t;
			// Miroir eteint : 0x7C0 a son propre stockage, donc il garde 0x1234 et 0x780 garde 0x5678.
			// Miroir allume : les deux sont le meme mot, donc la seconde ecriture gagne les deux fois.
			juste = juste && (miroir ? (haut == 0x5678 && bas == 0x5678) : (haut == 0x1234 && bas == 0x5678));
		}
		pose_miroir(coeur, true);   // on rend le coeur dans l'etat ou on l'a pris
		juste ? ok("0x7C0-0x7FF : ce que ce coeur en fait", lignes)
		      : rate("0x7C0-0x7FF : ce que ce coeur en fait", lignes + " -- pas ce qui etait enregistre");
	}

	// ---- 6. la carte des masques, pour qu'une reecriture ait une cible et pas une intention
	//
	// BORNEE, et ca n'allait pas de soi : la premiere version de cette boucle montait jusqu'a 0x43E alors que
	// le tableau s'arrete a 0x42E, et imprimait la memoire d'apres -- 374b, c473, 44d0, qui n'etaient les
	// registres de rien. Un debordement rend des chiffres, et des chiffres ressemblent a une mesure.
	printf("\n-- ce qu'une ecriture leve, par mot (.... = intouchable, * = collant) --\n");
	for (std::uint32_t octet = 0; octet < 0x430; octet += 0x20) {
		printf("  %03x ", octet);
		for (std::uint32_t k = 0; k < 0x20 && octet + k < 0x430; k += 2) {
			const std::uint32_t mot = (octet + k) / 2;
			const char *p = nullptr;
			if (intouchable(octet + k, p)) printf("  .... ");
			else printf("  %04x%c", monte[mot], colle[mot] ? '*' : ' ');
		}
		printf("\n");
	}

	printf("\n");
	if (echecs) { printf("%d echec(s).\n", echecs); return 1; }
	printf("tous les cas passent\n");
	return 0;
}
