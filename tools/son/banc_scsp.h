// banc_scsp.h : de quoi keyer une voix et lire ce qui sort, pour les tests de bloc du SCSP.
//
// Les blocs de docs/scsp_architecture.md §3 se testent tous de la meme facon -- un coeur neuf, une RAM, une
// fente posee par la table du contrat, et n echantillons lus -- et il y en a sept. Ce fichier existe pour
// que la deuxieme version de ce montage soit la derniere : test_scsp_fentes.cpp (bloc 2) l'avait en propre,
// test_scsp_adsr.cpp (bloc 3) en avait besoin a l'identique.
//
// IL NE CONTIENT AUCUNE ATTENTE. Ce qu'on attend d'un bloc appartient au test de ce bloc ; ici il n'y a que
// le moyen de mesurer. Un banc qui porterait des references deviendrait un deuxieme endroit ou la verite
// sur la puce est ecrite.
#pragma once

#include "../../src/sound/scsp_contract.h"
#include "../../src/sound/scsp_maison.h"
#include "../../src/sound/scsp_mame.h"
#include "../../src/sound/scsp_select.h"

#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace banc {

// Un puits qui n'ecoute rien : les interruptions, le MIDI et les timers sont d'autres blocs.
struct Muet final : scsp::sink {
	void irq(int, bool) override {}
	void midi_out(std::uint8_t) override {}
	void timer(int, std::uint32_t, std::uint32_t) override {}
};

inline constexpr std::uint32_t RAM_OCTETS = 0x100000;   // 1 Mo, comme une puce de la carte

// Un coeur neuf avec sa RAM.
//
// UN BANC NEUF PAR CAS, et c'est une obligation et non une precaution : une fente SDIR ne s'eteint pas. Le
// key-off ne fait que passer l'enveloppe en RELEASE (StopSlot), et SDIR saute l'enveloppe -- la fente
// continuerait de sonner dans les cas suivants. Construire un coeur par cas coute une page de memoire et
// garantit qu'aucun cas ne depend de l'etat laisse par le precedent.
// LE NOM DU COEUR QUE LES BANCS CONSTRUISENT. Un seul endroit, pose par le main() de chaque test depuis
// son premier argument : c'est ce qui permet au meme test de juger le coeur de MAME et celui du projet. Par
// defaut "mame", pour qu'un test lance sans argument mesure la reference.
inline const char *&nom_du_coeur()
{
	static const char *n = "mame";
	return n;
}

struct Banc {
	// Parentheses et non accolades : `{n, 0}` construirait un vecteur de DEUX elements.
	std::vector<std::uint16_t> ram = std::vector<std::uint16_t>(RAM_OCTETS / 2, 0);
	Muet muet;
	std::unique_ptr<scsp::core> tenu = scsp::make(nom_du_coeur(), 45158400 / 2, ram.data(), RAM_OCTETS, muet);
	scsp::core &coeur = *tenu;

	Banc()
	{
		if (!tenu) { fprintf(stderr, "banc_scsp: coeur inconnu\n"); std::exit(2); }
		coeur.reset();
		// 0x400 comme le pilote l'ecrit au demarrage (MEM4MB, DAC18B), MVOL a fond. MEM4B n'a aucun effet
		// sur ce coeur -- read_byte/read_word masquent a 0xFFFFF en dur, sans le consulter -- mais on l'ecrit
		// quand meme pour que la puce soit dans l'etat ou le jeu la met.
		coeur.write_reg(0x400 / 2, 0x0300 | 0x000F, 0xFFFF);
	}

	// Une rampe d'echantillons 16 bits a l'adresse octet `sa` : l'echantillon k vaut k * pas.
	void rampe(std::uint32_t sa, unsigned n, int pas)
	{
		for (unsigned k = 0; k < n; k++)
			ram[(sa / 2) + k] = std::uint16_t(std::int16_t(int(k) * pas));
	}

	// Un continu : n echantillons de meme valeur. C'est le vehicule des blocs qui mesurent un GAIN
	// (enveloppe, LFO d'amplitude, TL) -- la sortie vaut alors le gain, a un facteur pres.
	void continu(std::uint32_t sa, unsigned n, int valeur)
	{
		for (unsigned k = 0; k < n; k++)
			ram[(sa / 2) + k] = std::uint16_t(std::int16_t(valeur));
	}

	// Pose les 16 mots d'une fente, puis tire la gachette. KYONEX passe en DERNIER et seul, parce qu'il
	// declenche un balayage des 32 fentes (UpdateSlotReg case 0) et non la seule qu'on ecrit.
	void keye(unsigned fente, const std::uint16_t mot[16])
	{
		const std::uint32_t base = fente * 0x10;   // 0x20 octets par fente, donc 0x10 mots
		for (unsigned w = 1; w < 16; w++)
			coeur.write_reg(base + w, mot[w], 0xFFFF);
		coeur.write_reg(base + 0, mot[0], 0xFFFF);
		std::uint16_t dec[16] = {};
		scsp_contract::pose(dec, "KYONEX", 1);
		coeur.write_reg(base + 0, std::uint16_t(mot[0] | dec[0]), 0xFFFF);
	}

	// n echantillons, canal gauche seul (la sortie est entrelacee L,R).
	//
	// ATTENTION : cet appel FAIT AVANCER la puce. Garder son resultat dans une variable plutot que
	// l'appeler deux fois -- une fois pour comparer, une fois pour afficher -- sinon le message montre
	// l'echantillon suivant a cote d'un test qui passe. C'est arrive.
	std::vector<std::int32_t> gauche(unsigned n)
	{
		std::vector<std::int32_t> out(std::size_t(n) * 2, 0);
		coeur.run(n, out.data());
		std::vector<std::int32_t> g(n);
		for (unsigned i = 0; i < n; i++) g[i] = out[std::size_t(i) * 2];
		return g;
	}

	// Les deux canaux, pour le bloc qui mesure le panoramique : la moitie des faits de DIPAN et d'EFPAN sont
	// sur le canal que `gauche()` jette.
	std::pair<std::int32_t, std::int32_t> stereo(unsigned n)
	{
		std::vector<std::int32_t> out(std::size_t(n) * 2, 0);
		coeur.run(n, out.data());
		return { out[std::size_t(n - 1) * 2], out[std::size_t(n - 1) * 2 + 1] };
	}
};

// Pose un champ de fente, et meurt si le nom n'existe pas : un nom absent de la table du contrat est une
// faute de frappe, pas une condition a gerer.
inline void pose(std::uint16_t mot[16], const char *nom, unsigned valeur)
{
	if (!scsp_contract::pose(mot, nom, valeur)) {
		fprintf(stderr, "banc_scsp: le contrat n'a pas de champ nomme %s\n", nom);
		std::exit(2);
	}
}

// Une suite de valeurs divisees par un pas connu, les non-multiples montres tels quels plutot qu'arrondis.
inline std::string suite(const std::vector<std::int32_t> &v, int diviseur)
{
	std::string s;
	for (std::size_t i = 0; i < v.size(); i++) {
		if (i) s += ",";
		if (diviseur != 0 && v[i] % diviseur == 0) s += std::to_string(v[i] / diviseur);
		else s += "?" + std::to_string(v[i]);
	}
	return s;
}

// LES DEUX CHOSES QUI NE SONT PAS DANS scsp::core, et que les tests doivent pourtant atteindre : le miroir
// MADRS et le volume maitre. scsp_core.h dit pourquoi elles n'y sont pas -- elles appartiennent au portage
// et non a la puce -- et ces deux aiguilleurs sont la MESURE de ce qui manque encore a l'interface. Le jour
// ou ils disparaissent, la dette est payee.
inline bool pose_miroir(scsp::core &c, bool on)
{
	if (auto *m = dynamic_cast<scsp_mame *>(&c)) { m->device().set_madrs_mirror(on); return true; }
	if (auto *n = dynamic_cast<scsp_maison *>(&c)) { n->set_madrs_mirror(on); return true; }
	return false;
}

inline float gain_expose(scsp::core &c)
{
	if (auto *m = dynamic_cast<scsp_mame *>(&c)) return m->device().output_gain();
	if (auto *n = dynamic_cast<scsp_maison *>(&c)) return n->output_gain();
	return -1.0f;
}

} // namespace banc
