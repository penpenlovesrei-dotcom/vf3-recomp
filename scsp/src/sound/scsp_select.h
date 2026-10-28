// scsp_select.h -- choisir une implementation du SCSP par son nom, et atteindre ce que scsp::core ne porte
// pas. Point 2 de l'etape 2 de docs/scsp_migration_plan.md : "les deux coeurs dans le meme binaire".
//
// POURQUOI UN SELECTEUR, et la raison tient toujours apres que le second coeur existe : le rejeu en a
// besoin, parce que comparer deux coeurs demande de les nommer, et le comparer A LUI-MEME est le premier
// controle a passer -- celui qui prouve que la machinerie ne mesure pas son propre bruit. Et c'est ici, et
// nulle part ailleurs, que la question "de quoi un coeur a-t-il besoin pour etre construit" recoit une
// reponse unique : l'horloge, la RAM, le puits. Exactement ce qu'un coeur autonome devra fournir.
//
// CE QUE CE FICHIER NE CONNAIT PLUS, et c'est le changement du 2026-10-28 : aucune classe concrete. Il
// n'inclut ni scsp_mame.h ni scsp_maison.h, et ne nomme ni l'un ni l'autre. CHAQUE COEUR S'INSCRIT DANS SON
// PROPRE .cpp, par le `inscrit` ci-dessous, et le selecteur ne fait que tenir la liste de ceux qui l'ont
// fait. La consequence est celle qu'on cherchait : **ce qui est LIE decide ce qui existe.** Un binaire qui
// ne lie que scsp_maison.cpp et dsp_maison.cpp a un selecteur a une seule entree, qui le dit, et ne traine
// pas les 2 110 lignes du coeur de MAME -- ce qui est le dernier prealable d'un depot separe
// (docs/scsp_migration_plan.md, et docs/son.md §7 pour le "pourquoi pas encore").
//
// Avant cela, le selecteur citait les deux classes, et tout ce qui le liait liait donc MAME : les huit tests
// de bloc le faisaient par tools/son/banc_scsp.h, sans aucune raison de fond, leurs references etant des
// nombres geles. La cible `make test-sans-mame` est la MESURE que c'est fini : les memes tests, le meme
// code, lies sans une ligne de MAME.
#pragma once

#include "scsp_core.h"

#include <memory>

namespace scsp {

// Les noms acceptes, pour un message d'erreur et pour l'aide d'un outil. C'est ce qui est inscrit, donc ce
// qui est lie : la chaine n'est pas la meme dans tous les binaires du depot, et c'est voulu.
const char *noms_connus();

// Construit le coeur nomme, ou nullptr si le nom est inconnu (le message va sur stderr).
//
// `ram` et `puits` doivent vivre plus longtemps que le coeur rendu.
std::unique_ptr<core> make(const char *nom, u32 horloge, u16 *ram, u32 ram_octets, sink &puits);

// -----------------------------------------------------------------------------------------------------
// CE QUE scsp::core NE PORTE PAS, ET QU'IL FAUT POURTANT ATTEINDRE.
//
// scsp_core.h a une section "AND WHAT IS DELIBERATELY ABSENT" : les ajouts du portage n'y sont pas, parce
// qu'un coeur autonome ne doit pas avoir a les implementer pour etre utilisable. Deux d'entre eux doivent
// quand meme s'atteindre de l'exterieur, et les voici -- en UN endroit, et plus en trois.
//
// LE MIROIR MADRS, et il a coute cher. C'est le seul des ajouts QUI CHANGE LE SON, et il change par jeu :
// vrai pour Virtua Fighter 3, FAUX pour Fighting Vipers 2 (soundboard.h, docs/scsp_audit.md §2). Il se
// posait par un `dynamic_cast` ecrit dans SoundBoard::reset, et deux autres copies du meme aiguillage
// vivaient dans tools/son/banc_scsp.h et tools/son/test_scsp_regs.cpp. La copie de la carte etait posee
// APRES la creation de l'enregistreur de trace, donc la trace ne pouvait pas la voir : un rejeu laissait le
// miroir a son defaut `true` et divergeait de la carte a l'echantillon 772 067, sans qu'on puisse dire si
// c'etait le coeur ou la trace. Mesure et referme le 2026-10-28 (docs/scsp_validation.md §3 c).
//
// LE GAIN EXPOSE, c'est-a-dire MVOL : la puce ne l'applique jamais elle-meme, elle l'expose seulement, et
// c'est la carte qui multiplie (mesure le 2026-10-26, scsp_contract.h §13 point 7). Le bloc 7 le verifie.
//
// Ces fonctions ne devinent rien en silence : elles rendent faux, ou -1, quand le coeur ne porte pas la
// chose. Et LE JOUR OU ELLES DISPARAISSENT, la dette de l'etape 3 est payee.
// -----------------------------------------------------------------------------------------------------

bool regle_miroir_madrs(core &c, bool on);
bool miroir_madrs(const core &c, bool *trouve = nullptr);
float gain_expose(const core &c);   // -1 si le coeur ne l'expose pas

// -----------------------------------------------------------------------------------------------------
// L'INSCRIPTION D'UN COEUR, a ecrire dans le .cpp du coeur et nulle part ailleurs.
//
// Les trois dernieres fonctions prennent un `core&` et rendent FAUX quand ce n'est pas le leur : le
// `dynamic_cast` vit ainsi dans le seul fichier qui connait le type, et le selecteur n'a pas a le connaitre.
// C'est toute l'astuce, et elle ne coute qu'un parcours d'une liste de deux entrees sur un appel rare.
// -----------------------------------------------------------------------------------------------------
struct inscription {
	const char *nom;
	std::unique_ptr<core> (*fabrique)(u32 horloge, u16 *ram, u32 ram_octets, sink &puits);
	bool (*pose_miroir)(core &c, bool on);          // faux si `c` n'est pas ce coeur
	bool (*lit_miroir)(const core &c, bool &out);   // idem
	bool (*lit_gain)(const core &c, float &out);    // idem
};

// A construire en statique de fichier : `const scsp::inscrit s_inscrit{ ... };`. L'ordre d'initialisation
// entre unites de traduction n'importe pas -- la liste est un statique de fonction, cree au premier usage.
struct inscrit {
	explicit inscrit(const inscription &i);
};

} // namespace scsp
