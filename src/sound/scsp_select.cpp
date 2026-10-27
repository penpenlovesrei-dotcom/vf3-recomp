#include "scsp_select.h"

#include "scsp_maison.h"
#include "scsp_mame.h"

#include <cstdio>
#include <cstring>

namespace scsp {

const char *noms_connus()
{
	return "mame, maison";
}

std::unique_ptr<core> make(const char *nom, u32 horloge, u16 *ram, u32 ram_octets, sink &puits)
{
	if (!nom || !strcmp(nom, "mame"))
		return std::make_unique<scsp_mame>(horloge, ram, ram_octets, puits);
	if (!strcmp(nom, "maison"))
		return std::make_unique<scsp_maison>(horloge, ram, ram_octets, puits);

	fprintf(stderr, "scsp_select: coeur inconnu \"%s\" ; connus : %s\n", nom, noms_connus());
	return nullptr;
}

} // namespace scsp
