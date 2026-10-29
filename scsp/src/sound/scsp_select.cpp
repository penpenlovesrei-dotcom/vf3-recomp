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
std::vector<const registration *> &registry()
{
	static std::vector<const registration *> l;
	return l;
}

} // namespace

registered::registered(const registration &i)
{
	registry().push_back(&i);
}

const char *known_names()
{
	// Built once, in registration order. Empty if nothing is linked, and the sentence says so rather
	// than handing back an empty string in the middle of an error message.
	static const std::string s = [] {
		std::string t;
		for (const registration *i : registry()) { if (!t.empty()) t += ", "; t += i->name; }
		return t.empty() ? std::string("(no core is linked into this binary)") : t;
	}();
	return s.c_str();
}

std::unique_ptr<core> make(const char *name, u32 clock, u16 *ram, u32 ram_bytes, sink &snk)
{
	// With no name, the first registered one: link order decides, and the Makefiles put MAME first
	// wherever both are linked, so that nothing changes unless it is asked for.
	if (!name || !*name) {
		if (registry().empty()) { fprintf(stderr, "scsp_select: no core is linked into this binary\n"); return nullptr; }
		const registration *i = registry().front();
		return i->factory(clock, ram, ram_bytes, snk);
	}
	for (const registration *i : registry())
		if (!strcmp(name, i->name))
			return i->factory(clock, ram, ram_bytes, snk);

	fprintf(stderr, "scsp_select: unknown core \"%s\"; known: %s\n", name, known_names());
	return nullptr;
}

bool set_madrs_mirror_on(core &c, bool on)
{
	for (const registration *i : registry())
		if (i->set_mirror && i->set_mirror(c, on)) return true;
	return false;
}

bool madrs_mirror_of(const core &c, bool *found)
{
	for (const registration *i : registry()) {
		bool v = true;
		if (i->read_mirror && i->read_mirror(c, v)) { if (found) *found = true; return v; }
	}
	if (found) *found = false;
	return true;   // the chip's default, and both cores'
}

float exposed_gain(const core &c)
{
	for (const registration *i : registry()) {
		float g = -1.0f;
		if (i->read_gain && i->read_gain(c, g)) return g;
	}
	return -1.0f;
}

} // namespace scsp
