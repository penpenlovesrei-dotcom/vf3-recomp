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

struct Injected {
	bool ok = false;
	std::vector<uint32_t> poly, cull_lo, cull_hi;
	std::vector<uint16_t> tex;
	std::vector<Anime> animes;
	uint32_t viewport_addr = 0x808000;
};

// The scene VF3_SCENE names, loaded once; null when the variable is not set or
// the files are missing (and then nothing changes).
const Injected *injected_scene();

// Says once how many models the injected viewport yielded.
void injected_note(int models);

} // namespace video

#endif
