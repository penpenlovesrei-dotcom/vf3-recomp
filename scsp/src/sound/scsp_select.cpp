// The list of registered cores, and nothing else. NO concrete class is included or even named here:
// that is what cuts the dependency (scsp_select.h says why).
#include "scsp_select.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace scsp {

namespace {

// A FUNCTION static, not a file one: the `inscrit` objects in other translation units initialise in an
// order the standard does not fix, and a list created on first use is the only one always there.
std::vector<const inscription *> &liste()
{
	static std::vector<const inscription *> l;
	return l;
}

} // namespace

inscrit::inscrit(const inscription &i)
{
	liste().push_back(&i);
}

const char *noms_connus()
{
	// Built once, in registration order. Empty if nothing is linked, and the sentence says so rather
	// than handing back an empty string in the middle of an error message.
	static const std::string s = [] {
		std::string t;
		for (const inscription *i : liste()) { if (!t.empty()) t += ", "; t += i->nom; }
		return t.empty() ? std::string("(no core is linked into this binary)") : t;
	}();
	return s.c_str();
}

std::unique_ptr<core> make(const char *nom, u32 horloge, u16 *ram, u32 ram_octets, sink &puits)
{
	// With no name, the first registered one: link order decides, and the Makefiles put MAME first
	// wherever both are linked, so that nothing changes unless it is asked for.
	if (!nom || !*nom) {
		if (liste().empty()) { fprintf(stderr, "scsp_select: no core is linked into this binary\n"); return nullptr; }
		const inscription *i = liste().front();
		return i->fabrique(horloge, ram, ram_octets, puits);
	}
	for (const inscription *i : liste())
		if (!strcmp(nom, i->nom))
			return i->fabrique(horloge, ram, ram_octets, puits);

	fprintf(stderr, "scsp_select: unknown core \"%s\"; known: %s\n", nom, noms_connus());
	return nullptr;
}

bool regle_miroir_madrs(core &c, bool on)
{
	for (const inscription *i : liste())
		if (i->pose_miroir && i->pose_miroir(c, on)) return true;
	return false;
}

bool miroir_madrs(const core &c, bool *trouve)
{
	for (const inscription *i : liste()) {
		bool v = true;
		if (i->lit_miroir && i->lit_miroir(c, v)) { if (trouve) *trouve = true; return v; }
	}
	if (trouve) *trouve = false;
	return true;   // the chip's default, and both cores'
}

float gain_expose(const core &c)
{
	for (const inscription *i : liste()) {
		float g = -1.0f;
		if (i->lit_gain && i->lit_gain(c, g)) return g;
	}
	return -1.0f;
}

} // namespace scsp
