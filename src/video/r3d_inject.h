// VF3_SCENE=<directory>: render a scene read from disk instead of the game's.
//
// Why this exists: a stage converted from another version has to be looked at
// before anything is written into a ROM. Loading it into the board's own
// memory would not do -- the game rewrites culling and polygon RAM every
// frame, so an injected scene would live for none of them. Instead the scene
// builder and the rasteriser are pointed at buffers of our own for that frame,
// and the board's memory is never touched.
//
// The directory holds what tools/vers_real3d.py writes:
//
//     polygones.bin      the polygon lists, at polygon RAM word 0x60000
//     noeuds.bin         the root node and the chain, at culling RAM word 0x80000
//     culling_haut.bin   viewport and tables, at high culling RAM word 0x8000
//     planche.bin        a 2048x1024 sheet of 16-bit texels, page 0
//
// The scene is then entered at 0x808000 rather than 0x800000.
#ifndef R3D_INJECT_H
#define R3D_INJECT_H

#include <cstdint>
#include <vector>

namespace video {

// Un modele qui bouge, tel que `anime.txt` le decrit : une ligne
// `<texX> <texY> <dx> <dy> <dz> <periode> <pas> <copies>`.
//
// D'ou ca vient : la routine Dreamcast du decor d'entrainement (0x8C048878)
// translate un seul objet de (0, -hauteur, 0), ou la hauteur monte de 1 par
// image et repasse a 0 au-dela de 500 -- soit la demi-hauteur du cylindre, qui
// parcourt donc toute sa course en 500 images. L'objet porte l'index de texture
// 2 : c'est l'enveloppe, et la conversion le nomme pour nous.
struct Anime {
	int tex_x = -1, tex_y = -1;   // on reconnait le modele a sa texture, pas a son rang
	float d[3] = {};
	int periode = 1;
	// L'INSTANCIATION. La routine d'origine empile la matrice et dessine le MEME objet a +-500, +-1000,
	// +-1500, +-2000 et +-2500 (table de constantes en 0x8C048B84). Sans ces copies, l'objet glisse et
	// laisse un trou derriere lui -- ce qui se voyait a l'ecran comme une jonction qui ne colle pas.
	float pas = 0;
	int copies = 0;   // de chaque cote
};

// Un modele de MORCEAU DE PAROI, tel que `morceaux.txt` le decrit : une ligne
// `<id> <xmin> <xmax> <ymin> <ymax> <zmin> <zmax>`.
//
// D'ou ca vient : quand une paroi casse, le jeu aligne trente-deux morceaux et
// lit l'identifiant de chacun en +0xE de son entree, puis dessine l'objet
// `[0x8C1C03EC + 4 + id*4]`. Pour le decor d'entrainement, les quatre qui
// dessinent quelque chose -- 5, 6, 7 et 8 -- tombent sur les quatre premiers
// groupes de l'entree MDL ; les autres identifiants sont des terminateurs, et
// `9`, le plus frequent, ne dessine rien.
//
// On reconnait le modele a sa BOITE et non a son rang, pour la meme raison que
// l'animation le reconnait a sa texture : rien ne garantit que le rendu
// construise les modeles dans l'ordre de la conversion.
struct Morceau {
	int id = 0;
	float lo[3] = {}, hi[3] = {};
};

struct Injected {
	bool ok = false;
	std::vector<uint32_t> poly, cull_lo, cull_hi;
	std::vector<uint16_t> tex;
	std::vector<Anime> animes;
	std::vector<Morceau> morceaux;
	// LA LISTE D'OBJETS DU DECOR, relevee sur Dreamcast (`objets.txt`, ecrit par
	// tools/objets_dc.py) : trente-deux identifiants, un par entree de morceau.
	//
	// Pourquoi ne pas lire celle de la borne. Le champ +0x24 de la fiche de TEST
	// est du remplissage -- sa propre table de sons relue en demi-mots -- et
	// donne des identifiants de 160 et 161. La Dreamcast, elle, en a une vraie :
	// `5 9 9 9 8 9 ...`, ou 9 est un terminateur qui ne dessine rien. Huit
	// morceaux seulement sont dessines sur les trente-deux.
	std::vector<int> objets;
	uint32_t viewport_addr = 0x808000;
};

// The scene VF3_SCENE names, loaded once; null when the variable is not set or
// the files are missing (and then nothing changes).
const Injected *injected_scene();

// Says once how many models the injected viewport yielded.
void injected_note(int models);

} // namespace video

#endif
