// La liste des coeurs inscrits, et rien d'autre. AUCUNE classe concrete n'est incluse ni nommee ici :
// c'est ce qui coupe la dependance (scsp_select.h dit pourquoi).
#include "scsp_select.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace scsp {

namespace {

// Statique de FONCTION, et non de fichier : les `inscrit` d'autres unites de traduction s'initialisent dans
// un ordre que la norme ne fixe pas, et une liste creee au premier usage est la seule qui soit toujours la.
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
	// Construite une fois, dans l'ordre d'inscription. Vide si rien n'est lie, et la phrase le dit plutot
	// que de rendre une chaine vide au milieu d'un message d'erreur.
	static const std::string s = [] {
		std::string t;
		for (const inscription *i : liste()) { if (!t.empty()) t += ", "; t += i->nom; }
		return t.empty() ? std::string("(aucun coeur n'est lie a ce binaire)") : t;
	}();
	return s.c_str();
}

std::unique_ptr<core> make(const char *nom, u32 horloge, u16 *ram, u32 ram_octets, sink &puits)
{
	// Sans nom, le premier inscrit : l'ordre de liaison decide, et les Makefile mettent MAME d'abord la ou
	// les deux sont lies, pour que rien ne change sans qu'on le demande.
	if (!nom || !*nom) {
		if (liste().empty()) { fprintf(stderr, "scsp_select: aucun coeur n'est lie a ce binaire\n"); return nullptr; }
		const inscription *i = liste().front();
		return i->fabrique(horloge, ram, ram_octets, puits);
	}
	for (const inscription *i : liste())
		if (!strcmp(nom, i->nom))
			return i->fabrique(horloge, ram, ram_octets, puits);

	fprintf(stderr, "scsp_select: coeur inconnu \"%s\" ; connus : %s\n", nom, noms_connus());
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
	return true;   // le defaut de la puce, et des deux coeurs
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
