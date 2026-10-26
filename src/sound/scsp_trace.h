// scsp_trace.h -- une trace d'acces au SCSP : l'enregistrer, la rejouer, comparer deux rejeux.
//
// Point 2 de l'etape 2 de docs/scsp_migration_plan.md, le dernier prealable qui manquait : "les deux coeurs
// dans le meme binaire, la meme sequence d'acces rejouee dans les deux [...] et la comparaison des PCM, des
// etats observables, des interruptions et des timers -- pas seulement du WAV."
//
// POURQUOI LE HARNAIS EXISTANT NE SUFFIT PAS. tools/son/comparer-son.cmd compare le MELANGE FINAL, deux WAV.
// Il a prouve sa valeur -- il a trouve une paire d'echantillons manquante sur 3 219 865 -- mais il ne voit
// que ce qui ressort du haut-parleur. Un bloc reecrit peut etre faux d'une facon que le melange masque : une
// interruption un echantillon trop tot, un timer arme a la mauvaise duree, une lecture de registre qui rend
// autre chose. Le 68000 reagit alors differemment, et la divergence apparait trois arenes plus loin sous une
// forme qui n'a plus rien a voir avec sa cause. Ici on compare ce que la PUCE a fait, pas ce qu'on entend.
//
// ET VF3_SCSP_WLOG NE SUFFIT PAS NON PLUS, bien que le plan le donne pour un depart. Il note l'image, pas
// l'echantillon -- donc l'instant d'une ecriture dans la trame est perdu, et c'est precisement ce qui decide
// ou tombent les interruptions. Il ne note que la puce 0, ni les lectures, ni le MIDI entrant, ni les timers
// qui se declenchent, ni les ecritures de la RAM son -- sans lesquelles un rejeu n'a aucun echantillon a
// jouer. Le format ci-dessous est donc neuf, et l'ancien journal reste ce qu'il est : un journal a lire.
//
// LE FORMAT, du texte, une ligne par evenement, pour qu'il se lise et se diffe comme les autres journaux du
// projet. Les evenements sont dans l'ordre ou la carte les a faits, ce qui est tout l'interet :
//
//     # scsp-trace 1 puce=N horloge=H ram=B
//     d <mot> <data> <masque>   une ecriture dans la RAM son (mot = index de MOT dans la RAM)
//     w <mot> <data> <masque>   write_reg
//     r <mot>                   read_reg (une lecture a des effets de bord, et sa valeur se compare)
//     m <octet>                 midi_in
//     t <lequel>                timer_fired
//     s <n>                     run(n) : n echantillons
//
// ET L'ENREGISTREUR GARDE AUSSI LE PCM, dans un second fichier `<trace>.pcm` (des s32 entrelaces). Ce n'est
// pas une commodite : c'est la seule facon de PROUVER qu'une trace porte tout ce dont la puce depend. Rejouer
// la trace et retrouver le meme PCM a l'octet, c'est etablir qu'aucun etat n'entre par une porte que la trace
// ne note pas. Sans ce controle, un rejeu qui diverge laisse deux explications -- le coeur est different, ou
// la trace est incomplete -- et on ne peut pas choisir.
//
// UNE TRACE COMMENCE A LA REMISE A ZERO, et l'enregistreur le verifie plutot que de le supposer : au
// demarrage il balaie la RAM et ecrit un `d` par mot non nul. Apres un reset il y en a huit -- les vecteurs
// du 68000 que la carte y copie. Au milieu d'une partie il y en aurait des centaines de milliers, et
// l'enregistreur refuse alors avec un message clair au lieu d'ecrire un fichier de 500 Mo.
#pragma once

#include "scsp_core.h"

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace scsp {

// Ce qu'un coeur a RENDU pendant un rejeu : tout ce qui sort de la puce, horodate en echantillons.
struct sortie {
	struct Irq  { std::uint64_t ech; int niveau; bool etat; };
	struct Midi { std::uint64_t ech; u8 octet; };
	struct Arme { std::uint64_t ech; int lequel; u32 echantillons, periode; };
	struct Lue  { std::uint64_t ech; u32 mot; u16 valeur; };

	std::vector<Irq>  irqs;
	std::vector<Midi> midis;
	std::vector<Arme> armes;
	std::vector<Lue>  lues;     // les valeurs rendues par read_reg
	std::vector<s32>  pcm;      // entrelace gauche/droite, a l'echelle 18 bits
};

// Le puits qui note tout, pour un rejeu. Il demande l'instant au coeur, qu'il faut donc lui donner.
class puits_notant final : public sink {
public:
	explicit puits_notant(sortie &s) : m_s(s) {}
	void suit(const core *c) { m_c = c; }

	void irq(int niveau, bool etat) override { m_s.irqs.push_back({ ou(), niveau, etat }); }
	void midi_out(u8 octet) override { m_s.midis.push_back({ ou(), octet }); }
	void timer(int lequel, u32 ech, u32 per) override { m_s.armes.push_back({ ou(), lequel, ech, per }); }

private:
	std::uint64_t ou() const { return m_c ? m_c->samples_made() : 0; }
	sortie &m_s;
	const core *m_c = nullptr;
};

// L'ENREGISTREUR : un coeur qui passe tout a un autre et ecrit la trace au passage.
//
// C'est un decorateur et non une modification de la carte, pour une raison de fond : si l'enregistrement
// vivait dans soundboard.cpp il faudrait le poser a huit endroits, et le neuvieme appel ajoute un jour
// echapperait a la trace sans que rien ne le dise. Ici il n'y a qu'une porte.
class enregistreur final : public core {
public:
	// `dessous` doit vivre plus longtemps que l'enregistreur. `ram` sert au balayage initial. Le PCM va
	// dans `<fichier>.pcm`.
	enregistreur(core &dessous, const char *fichier, unsigned puce, u32 horloge,
	             const u16 *ram, u32 ram_octets);
	~enregistreur() override;

	// Vrai si le fichier est ouvert et le balayage initial accepte. Faux : la trace est abandonnee et un
	// message est deja sorti sur stderr -- l'enregistreur se contente alors de passer les appels.
	bool bon() const { return m_f != nullptr; }

	// Une ecriture dans la RAM son, que la carte fait sans passer par le coeur.
	void ram_ecrite(u32 mot, u16 data, u16 masque);

	void reset() override;
	u16  read_reg(u32 mot) override;
	void write_reg(u32 mot, u16 data, u16 masque) override;
	void run(u32 n, s32 *out) override;
	void timer_fired(int lequel) override;
	void midi_in(u8 octet) override;
	std::uint64_t samples_made() const override { return m_dessous.samples_made(); }

private:
	core &m_dessous;
	FILE *m_f = nullptr;
	FILE *m_pcm = nullptr;
};

// LE REJOUEUR : lit une trace, pilote `c`, remplit `s`.
//
// `ram` est la RAM que `c` a recue a sa construction : les lignes `d` y vont. `erreur` recoit la raison d'un
// echec (fichier absent, en-tete inconnu, ligne illisible) ; rien n'est devine en silence.
bool rejoue(const char *fichier, core &c, std::vector<u16> &ram, sortie &s, std::string *erreur);

// LA COMPARAISON : rend une chaine vide si les deux sorties sont identiques, sinon LA PREMIERE divergence,
// nommee et situee. Une seule, parce qu'apres la premiere les deux coeurs ne font plus la meme histoire et
// que la suite n'apprend rien.
std::string compare(const sortie &a, const sortie &b);

} // namespace scsp
