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

} // namespace scsp
