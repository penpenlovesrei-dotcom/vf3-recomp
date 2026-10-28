// scsp_select.h -- choisir une implementation du SCSP par son nom.
//
// Point 2 de l'etape 2 de docs/scsp_migration_plan.md : "les deux coeurs dans le meme binaire". Voici la
// porte par laquelle le second entrera.
//
// AUJOURD'HUI IL N'Y EN A QU'UN, et ce fichier le dit plutot que de faire semblant : "mame" est la seule
// entree. Ecrire un selecteur avant d'avoir deux choix peut sembler premature -- ca ne l'est pas, pour deux
// raisons. D'abord le rejeu en a besoin tout de suite : comparer deux coeurs demande de les nommer, et le
// comparer A LUI-MEME est le premier controle a passer, celui qui prouve que la machinerie ne mesure pas son
// propre bruit. Ensuite c'est ici, et nulle part ailleurs, que la question "de quoi un coeur a-t-il besoin
// pour etre construit" recoit une reponse unique -- l'horloge, la RAM, le puits -- ce qui est exactement ce
// qu'un coeur autonome devra fournir.
//
// CE QUE LE SELECTEUR NE PEUT PAS DONNER, et il vaut mieux l'ecrire ici que le decouvrir en branchant : les
// extras de ce portage ne sont pas dans scsp::core. Les gains par groupe, la sonde d'energie, le miroir
// MADRS et le volume maitre (mesure le 2026-10-26 : MVOL n'est jamais applique dans la puce, elle l'expose
// seulement) passent par scsp_mame::device(). Un coeur obtenu par ce selecteur est donc utilisable par un
// rejeu et par un test, mais pas encore par la carte -- c'est la derniere dette de l'etape 3.
#pragma once

#include "scsp_core.h"

#include <memory>

namespace scsp {

// Les noms acceptes, pour un message d'erreur et pour l'aide d'un outil.
const char *noms_connus();

// Construit le coeur nomme, ou nullptr si le nom est inconnu (le message va sur stderr).
//
// `ram` et `puits` doivent vivre plus longtemps que le coeur rendu.
std::unique_ptr<core> make(const char *nom, u32 horloge, u16 *ram, u32 ram_octets, sink &puits);

// LE MIROIR MADRS, ET POURQUOI IL EST ICI ET PLUS DANS LA CARTE.
//
// Des quatre choses que `scsp::core` ne porte pas deliberement (voir plus haut), le miroir est la seule qui
// CHANGE LE SON, et il change par jeu : vrai pour Virtua Fighter 3, FAUX pour Fighting Vipers 2
// (soundboard.h, docs/scsp_audit.md §2). Il se posait donc sur la classe concrete, par un `dynamic_cast`
// ecrit dans `SoundBoard::reset` -- et la consequence a coute une divergence difficile a lire : la trace de
// `scsp_trace.h` ne pouvait pas le voir, donc un rejeu le supposait `true`, et sur Fighting Vipers 2 il
// divergeait de la carte a l'echantillon 772 067 sans qu'on puisse dire si c'etait le coeur ou la trace.
// **Mesure le 2026-10-28** : la meme scene enregistree avec `VF3_SCSP_MADRS_MIRROR=1` se rejouait identique
// a l'octet, ce qui a nomme le coupable (docs/scsp_validation.md §3 c).
//
// D'ou ces deux fonctions : **une seule porte**, que la carte et l'enregistreur passent tous les deux. La
// carte le POSE depuis le jeu ; l'enregistreur le RELIT pour l'ecrire dans l'en-tete de la trace, au lieu
// qu'on pense a le lui dire -- ce qu'on finit par oublier. Elles rendent faux si le coeur ne le porte pas,
// et ne devinent rien en silence.
bool regle_miroir_madrs(core &c, bool on);
bool miroir_madrs(const core &c, bool *trouve = nullptr);

} // namespace scsp
