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

bool regle_miroir_madrs(core &c, bool on)
{
	if (auto *m = dynamic_cast<scsp_mame *>(&c)) { m->device().set_madrs_mirror(on); return true; }
	if (auto *j = dynamic_cast<scsp_maison *>(&c)) { j->set_madrs_mirror(on); return true; }
	return false;
}

bool miroir_madrs(const core &c, bool *trouve)
{
	if (trouve) *trouve = true;
	if (auto *m = dynamic_cast<const scsp_mame *>(&c)) return m->device_const().madrs_mirror();
	if (auto *j = dynamic_cast<const scsp_maison *>(&c)) return j->madrs_mirror();
	if (trouve) *trouve = false;
	return true;   // le defaut des deux coeurs, et de la puce
}

} // namespace scsp
