// The Real3D's scene. See r3d_scene.h.
//
// Addresses are in words. Culling RAM: below 0x100000 the low culling RAM, 0x800000-0x83FFFF the high. Models:
// below 0x100000 polygon RAM, else VROM.
//
// Viewport node (Step 1.0), words:
//   0      bit 5 disabled, bits 3-4 priority (0-3)
//   1      the next viewport (0x01000000: none)
//   2      the first culling node
//   3      cota (float; the texture LOD's scale)
//   4-7    sun Z, X, Y (floats, in the Real3D's axes) and intensity
//   8-0xB  cv = 1/(right-left), cw = 1/(top-bottom), io, jo (floats: the frustum)
//   0xC-13 the four culling planes' factors (left u, v; top u, w; right u, v; bottom u, w)
//   0x14   height (high 16) and width (low 16), 14.2
//   0x16   the matrix table (a culling RAM address); 0x17 the LOD table
//   0x1A   Y (high 16) and X (low 16) of the top left, 12.4
//   0x1C   the line-of-sight test's position
//   0x1D   spotlight Y size (high 16) and Y centre (low 16, 13.3); 0x1E the same for X
//   0x1F   spotlight extent; 0x21 1/its start (floats)
//   0x20   bits 11-13 spotlight colour, 8-10 its colour on fog (RGB, a bit each), 0-7 scroll fog
//   0x22   fog colour (R 16-23, G 8-15, B 0-7); 0x23 fog density (float)
//   0x24   fog attenuation 16-23, ambient 8-15, scroll fog attenuation 0-7
//   0x25   fog ambient 16-23, fog start 0-15 (signed, /255)
//
// Culling node (Step 1.0: 8 words; the later steps' 10, less words 1 and 2):
//   0      bits 0-1 type (0 viewport), bit 2 has a colour table address, bit 3 the child is a 4-entry LOD
//          table, bit 4 a translation (words 2-4) in place of a matrix, bit 7 reset the matrix's rotation, bits
//          8-9 both set: not drawn (one: half alpha, on the polygons); (bits 0-2 == 6: no sibling)
//   1      bits 0-11 the matrix (index in the matrix table), 12-18 the LOD table entry, 19-31 colour table bits,
//          bit 29 the LOD table's entries are culling nodes (else models)
//   2-4    translation (floats)
//   5      the child (bits 0-26; bits 28-31 colour table bits)
//   6      the sibling (bits 0-24, bit 24 none; bits 25-31 colour table bits)
//   7      blend radius (high 16) and culling radius (low 16), 16-bit floats
// A child pointer's bits 24-26 say what it is: 0 a culling node, 1 a model, 4 a list of culling nodes (entries
// with bit 24: empty, bit 25: the last).
//
// Matrices: 12 floats, translation first then the 3x3 by rows. The table's first is the base (view) matrix; the
// Real3D's axes are taken to GL's by a fixed swap (x' = y, y' = -z, z' = -x).
//
// Polygon (7 words of header, then 4 words for each vertex it doesn't share with the previous polygon):
//   0      bits 0-3 vertices shared (vertex i of the previous polygon), bit 6 a quad, bit 7 specular, bits 8-9
//          discard (both: not drawn; bit 9 alone halves alpha), bits 26-31 specular amount
//   1-3    the face normal (high 24 bits each, 2.22); word 1 also: bit 1 colour in word 4 (else a colour table
//          index), bit 2 the last polygon, bit 3 smooth shading (vertex normals), bit 4 double-sided, bit 5 fixed
//          shading (the shade in the vertex's low X byte), bit 6 texture coordinates 1x (else 1/8)
//   2      bits 0-1 mirror V, U; bits 2-3 microtexture min LOD, 4 microtexture, 5-7 its number
//   3      bits 0-2 texture height, 3-5 width (32 << n), 6-7 smooth V, U
//   4      RGB (24-31, 16-23, 8-15) or colour index (8-19); bit 6 the texture sheet, bit 7 translator map, bits
//          0-4 with word 5's bit 7 the texture's X (in 32s)
//   5      bits 0-4 the texture's Y (in 32s), 8-31 texture LOD base (12.12)
//   6      bits 0-2 texture alpha, 3 layered, 4 high priority, 5-6 shininess, 7-9 texture format, 10 textured,
//          11-15 fog intensity (/16), 16 no lighting, 17 the second translucent layer, 18-22 translucency
//          (x/32), 23 opaque, 24-30 translator map offset (2: inverted texture), 31 contour (alpha test)
// Vertex: X, Y, Z (high 24 bits each, 17.7; the low bytes the normal, or the fixed shade), then U (high 16) and
// V (low 16).
#include "r3d_scene.h"
#include "r3d_inject.h"
#include <atomic>
#include "../machine/model3.h"
#include <algorithm>
#include <tuple>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <cstring>
#include <string>
#include <limits>
#include <map>

namespace video {

Mat4 Mat4::identity()
{
	Mat4 r = {};
	r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1;
	return r;
}

Mat4 Mat4::operator*(const Mat4 &b) const
{
	Mat4 r;
	for (int i = 0; i < 4; i++)
		for (int j = 0; j < 4; j++)
			r.m[j * 4 + i] = m[i] * b.m[j * 4] + m[4 + i] * b.m[j * 4 + 1] + m[8 + i] * b.m[j * 4 + 2] + m[12 + i] * b.m[j * 4 + 3];
	return r;
}

namespace {

inline float as_float(uint32_t v)
{
	float f;
	memcpy(&f, &v, 4);
	return f;
}

// The Real3D's 16-bit float (the radii)
float float16(uint16_t v)
{
	uint32_t a = uint32_t(v) << 15;
	uint32_t e = (a & 0x7E000000) >> 25;
	e += e <= 31 ? 127 : 127 - 64;
	return as_float((a & 0x80000000) | (e << 23) | ((a & 0x1FFFFFF) >> 2));
}

inline float byte_to_float(int8_t b) { return (2.0f * b + 1.0f) * (1.0f / 255.0f); }

const float spot_rgb[8][3] = { { 0, 0, 0 }, { 0, 0, 1 }, { 0, 1, 0 }, { 0, 1, 1 }, { 1, 0, 0 }, { 1, 0, 1 }, { 1, 1, 0 }, { 1, 1, 1 } };

const int shared_count[16] = { 0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4 };

} // namespace

const uint32_t *SceneBuilder::culling(uint32_t addr) const
{
	addr &= 0xFFFFFF;
	if (addr >= 0x800000 && addr < 0x840000) return cull_hi + (addr & 0x3FFFF);
	if (addr < 0x100000) return cull_lo + addr;
	return nullptr;
}

const uint32_t *SceneBuilder::model_at(uint32_t addr) const
{
	addr &= 0xFFFFFF;
	return addr < 0x100000 ? poly + addr : vrom + addr;
}

void SceneBuilder::build(const m3::Model3 &board, Scene &scene)
{
	cull_lo = board.cull_lo.data();
	cull_hi = board.cull_hi.data();
	poly = board.poly_ram.data();
	vrom = reinterpret_cast<const uint32_t *>(board.vrom.data());
	// Step 1.5 and later put two more words at the start of every culling node and hold vertices and texture NP
	// with more fraction bits (r3d_scene.h)
	node_shift = board.stepping > 0x10 ? 2 : 0;
	vertex_scale = board.stepping > 0x10 ? 1.0f / 2048.0f : 1.0f / 128.0f;
	np_scale = board.stepping > 0x10 ? 1.0f / 16384.0f : 1.0f / 4096.0f;
	scene.viewports.clear();
	scene.blank = false;
	static std::atomic<uint64_t> next_id{ 1 };
	scene.id = next_id++;
	visits = 0;
	if (rom_cache.size() > 20000)
		rom_cache.clear();
	if (dyn_cache.size() > 20000)
		dyn_cache.clear();
	// VF3_SCENE=<directory>: a scene read from disk, rendered in place of the game's. The board's memory is left
	// alone -- the builder is pointed at our own buffers for this frame, because the game rewrites culling and
	// polygon RAM every frame and an injected scene would live for none of them. See r3d_inject.h.
	injected_note(-1);   // (instrument : build() a ete atteint)
	const Injected *in = injected_scene();
	// VF3_SCENE_ADD=<n> : AJOUTER la scene lue sur disque a celle du jeu, au lieu de la remplacer, et seulement
	// quand le decor courant est le numero <n>.
	//
	// Pourquoi ajouter. Le decor d'entrainement existe dans la borne -- nom, camera, brouillard, arene, et les
	// combattants tiennent debout dessus -- mais sa geometrie propre n'est nulle part : le combat force sur TEST
	// donne les combattants, leurs ombres et rien d'autre. Il ne manque donc que le ring. On garde tout le reste
	// du jeu et on ne lui ajoute que ce qui lui manque.
	//
	// NE PAS REECRIRE « le creneau 3 de ses huit identifiants de ressource ». Ces huit mots sont la bande-son du
	// decor, pas sa geometrie : le repartiteur Dreamcast 0x8C047C9E lit le BAS OCTET de l'identifiant et ne
	// connait que 0xA9 (effets), 0xF0 (musique) et 0xFF. Le creneau 3 de TEST y vaut 0x001800F0 -- musique,
	// index 24. Ou un decor declare sa geometrie reste une question ouverte.
	//
	// Comment. Nos modeles sont convertis avec `--cam identite`, donc leur matrice est modele -> MONDE. Le
	// viewport du jeu porte dans `root` la matrice de sa camera, monde -> vue. La composition est donc
	// `root * la notre`, dans cet ordre : operator* applique son operande de droite d'abord.
	static const int add_stage = [] {
		const char *s = getenv("VF3_SCENE_ADD");
		return (s && *s) ? int(strtol(s, nullptr, 0)) : -1;
	}();
	const bool additif = in && add_stage >= 0;
	if (in && !additif) {
		poly = in->poly.data();
		cull_lo = in->cull_lo.data();
		cull_hi = in->cull_hi.data();
		viewport(in->viewport_addr, scene, 0);
		injected_note(scene.viewports.empty() ? 0 : int(scene.viewports[0].models.size()));
	} else {
		viewport(0x800000, scene, 0);
		const bool ce_decor = additif && board.ram.size() > (0x58FD9F ^ 3)
		                      && board.ram[0x58FD9F ^ 3] == uint8_t(add_stage);
		if (ce_decor && !scene.viewports.empty()) {
			const uint32_t *sp = poly, *sl = cull_lo, *sh = cull_hi;
			poly = in->poly.data();
			cull_lo = in->cull_lo.data();
			cull_hi = in->cull_hi.data();
			Scene mien;
			// LE BUDGET DE NOEUDS EST CELUI DE TOUTE LA SCENE. `visits` est remis a zero une fois par
			// build() et `node_one` abandonne des qu'il passe NODE_BUDGET -- or le parcours du JEU a
			// deja eu lieu quand on arrive ici. Si sa scene est lourde dans un sous-etat, la fin de
			// NOTRE chaine tombe, et c'est le seul mecanisme de la chaine qui puisse faire disparaitre
			// un modele sur quarante-six sans rien changer d'autre.
			const uint32_t visites_du_jeu = visits;
			viewport(in->viewport_addr, mien, 0);
			const uint32_t visites_a_nous = visits - visites_du_jeu;
			poly = sp; cull_lo = sl; cull_hi = sh;
			// LE PLAN DE SOL BOUCHE-TROU DU JEU, RETIRE.
			//
			// La borne dessine sous les combattants un carre plat de douze unites de cote au niveau
			// zero -- un seul maillage, six sommets, modele 0x860542 en VROM. Sur un decor normal la
			// geometrie du decor le cache ; sur TEST, dont le creneau de geometrie vaut zero, il n'y a
			// rien pour ca, et c'est LUI qu'on voyait comme un sol blanc.
			//
			// Pourquoi il n'apparait que dans trois sous-etats. Le tampon de profondeur est vide par
			// NIVEAU de priorite et un niveau superieur repeint ceux d'en dessous. Dans READY_DSP et
			// FIGHT_DSP notre viewport est en priorite 2 et le plan en priorite 1 : on le recouvre.
			// Dans SET_DSP (et dans ROUND_MASK et REPLAY) tout le jeu retombe en priorite 1, le plan
			// partage alors notre passe, et il gagne la profondeur -- a juste titre : il est a y = 0,
			// notre sol Dreamcast est a y = -0,04 a -0,14, donc DESSOUS.
			//
			// On le reconnait par sa FORME, pas par son adresse : un modele d'un seul maillage, six
			// sommets, tous au meme y, tenant dans huit unites. Ni un combattant ni une ombre ni une
			// piece du ring n'a cette forme. Les ombres, elles, sont d'autres modeles et restent.
			if (!mien.viewports.empty()) {
				size_t otes = 0;
				for (Viewport &v : scene.viewports) {
					auto bouche_trou = [](const Model &m) {
						if (!m.meshes || m.meshes->size() != 1) return false;
						const std::vector<Vertex> &s = (*m.meshes)[0].verts;
						if (s.size() != 6) return false;
						for (const Vertex &p : s) {
							if (p.pos[1] != s[0].pos[1]) return false;
							if (std::fabs(p.pos[0]) > 8.0f || std::fabs(p.pos[2]) > 8.0f) return false;
						}
						return true;
					};
					const size_t avant = v.models.size();
					v.models.erase(std::remove_if(v.models.begin(), v.models.end(), bouche_trou),
					               v.models.end());
					otes += avant - v.models.size();
				}
				static bool dit = false;
				if (otes && !dit) {
					dit = true;
					fprintf(stderr, "plan de sol bouche-trou du jeu retire : %zu modele(s)\n", otes);
				}
			}
			if (!mien.viewports.empty()) {
				// UN VIEWPORT A NOUS, et non les modeles verses dans celui du jeu.
				//
				// Pourquoi. Nos polygones a ombrage fixe portent le gris peint dans la donnee Dreamcast, et le
				// rendu calcule `li = ombre * sun_intensity + ambiante`. Le viewport de la borne vaut 0,600 et
				// 0,298 (mesures dans son propre viewport en 0xFFDF9800), donc un sol a 0,36 ressortait a 0,51
				// et le noir ne descendait plus sous 0,298 : tout etait releve et ecrase, le sol delave vers le
				// blanc. Avec soleil 1 et ambiante 0, `li` vaut l'octet, et le gris est celui de la Dreamcast.
				//
				// Cohabitation : le tampon de profondeur est vide par NIVEAU DE PRIORITE, pas par viewport
				// (r3d_raster.cpp, draw_band), donc deux viewports de meme priorite partagent la passe et se
				// composent correctement avec les combattants.
				// LA PRIORITE SE PREND SUR LE CONTENU, pas sur le premier viewport venu. Le rasteriseur
				// vide le tampon de profondeur A CHAQUE NIVEAU DE PRIORITE (draw_band), donc un niveau
				// superieur repeint ceux d'en dessous. Mesure pendant l'intro d'un combat : le jeu pose
				// une dizaine de viewports, les combattants sont dans un de priorite 1, et le premier de
				// la liste est en priorite 2 -- notre decor le copiait et les recouvrait. En plein combat
				// le defaut ne se voyait pas, le premier viewport ayant la meme priorite qu'eux.
				//
				// On prend donc la priorite de celui qui porte le plus de modeles : c'est la scene de
				// combat. A defaut, celle du premier.
				const Viewport *porteur = &scene.viewports[0];
				for (const Viewport &v : scene.viewports)
					if (v.models.size() > porteur->models.size()) porteur = &v;
				const Viewport &jeu = *porteur;
				// LES MURS CASSES, PRIS DANS L'ETAT DU JEU.
				//
				// La borne tient toute la simulation : elle detecte la projection, pose le masque,
				// aligne les morceaux et joue le son. Il ne lui manque que des modeles a dessiner,
				// et nous en injectons. On lit donc SON etat et on retire nos panneaux.
				//
				// Le masque est [[0x00075124] + 0xC0]. 0x00046924 y fait le OU des bits de paroi
				// du combattant (+0xE8C), mais seulement quand celui-ci porte le bit 0x04000000 de
				// son mot d'etat ET depasse un seuil de force : ce sont donc bien les murs CASSES,
				// pas un simple contact.
				//
				// LES BITS SONT 1, 2, 4, 8, et il faut lire `rlwinm` avec soin pour le voir.
				// 0x00027B70 teste `rlwinm. r0,r20,SH,0,0` pour SH = 31, 30, 29, 28. En
				// numerotation PowerPC le bit 0 est le POIDS FORT, et cette forme met dans le bit 0
				// du resultat le bit SH de la source -- donc SH = 31 designe le bit 31, c'est-a-dire
				// la VALEUR 1, et non 0x40000000.
				//
				// Je m'y suis trompe une fois : j'ai cru aux bits hauts, et le mur a cesse de
				// disparaitre puisque ces bits-la ne sont jamais poses. Ne pas y revenir.
				//
				//     bit 1  angle 0x0000  z = -D        bit 4  angle 0x8000  z = +D
				//     bit 2  angle 0xC000  x = +D        bit 8  angle 0x4000  x = -D
				//
				// Les cotes viennent de 0x00046A60, qui ecrit ses quatre points de contact dans cet
				// ordre : +0xE38 a z = -D, +0xE44 a x = +D, +0xE50 a z = +D, +0xE5C a x = -D.
				//
				// Un panneau de notre ring se range par sa boite : il tient d'un seul cote, donc le
				// signe de son centre sur l'axe ou il est MINCE le designe.
				uint32_t murs_casses = 0;
				if (board.ram.size() > 0x00075128) {
					const uint32_t combat = (uint32_t(board.ram[0x00075124 ^ 3]) << 24)
					                      | (uint32_t(board.ram[0x00075125 ^ 3]) << 16)
					                      | (uint32_t(board.ram[0x00075126 ^ 3]) << 8)
					                      | uint32_t(board.ram[0x00075127 ^ 3]);
					if (combat + 0xC4 < board.ram.size())
						for (int k = 0; k < 4; k++)
							murs_casses = (murs_casses << 8) | board.ram[(combat + 0xC0 + uint32_t(k)) ^ 3];
				}
				// VF3_MURS=<masque> : forcer le masque, pour VOIR le retrait sans avoir a reussir une
				// vraie projection. Le jeu ne le pose que sur un coup assez fort, et un match joue au
				// hasard ne le pose jamais -- 26 000 images sur TOWER l'ont laisse a zero.
				if (const char *s = getenv("VF3_MURS")) murs_casses = uint32_t(strtoul(s, nullptr, 0));
				Viewport a_nous = jeu;
				a_nous.models.clear();
				a_nous.sun_intensity = 1.0f;
				a_nous.ambient = 0.0f;
				// Pas de brouillard en double : fog_layers choisit UN viewport par priorite.
				a_nous.fog_density = a_nous.fog_start = a_nous.fog_attenuation = 0;
				a_nous.scroll_fog = a_nous.scroll_att = 0;
				// NI DE PROJECTEUR. Le rendu l'AJOUTE apres l'eclairage : `lr = li + spot_colour * lobe`.
				// Nos polygones a ombrage fixe portent deja le gris peint dans la donnee ; un projecteur
				// allume par le jeu -- pour la presentation d'un combat, par exemple -- s'ajoutait par
				// dessus et blanchissait le sol, alors qu'en plein combat, projecteur eteint, il etait
				// correct. C'est ce qu'on voyait comme un sol blanc au debut du round.
				for (int k = 0; k < 3; k++) a_nous.spot_colour[k] = a_nous.spot_fog_colour[k] = 0;
				a_nous.spot_range[0] = a_nous.spot_range[1] = 0;
				for (int k = 0; k < 4; k++) a_nous.spot_ellipse[k] = 0;
				// L'ANIMATION. Le compteur monte d'une unite par image, chacun sur sa propre periode,
				// comme la hauteur de 0x8C110FD4 sur la Dreamcast. La translation s'applique dans
				// l'espace du MONDE -- la routine d'origine empile la matrice, translate, puis dessine,
				// donc le decalage vient a gauche de la transformation du modele et a droite de la
				// camera : `camera * translation * modele`.
				// On reconnait le modele a sa TEXTURE, pas a son rang. Rien ne garantit que le rendu
				// construise les modeles dans l'ordre de la conversion -- et c'est pour avoir suppose le
				// contraire que le cylindre ne bougeait pas : on animait un autre objet.
				static uint64_t tic = 0;
				static bool echelle_dite = false;   // VF3_ECHELLE : la mesure ne se fait qu'une fois
				std::vector<Model> modeles_morceaux;   // mis de cote, poses seulement a la casse
				tic++;

				// AUX MANCHES PAIRES, LE DECOR TOURNE SUR UN AXE INCLINE DE 25 DEGRES.
				//
				// C'est ce que Frederic decrivait depuis deux jours : « oblique, plongeant vers la
				// droite, il tourne sur lui-meme ». Ce n'est pas le cylindre, c'est TOUT le decor, et
				// ce n'est pas la routine d'animation : c'est la matrice de base.
				//
				// MESURE, dans ses propres sauvegardes Dreamcast. Les 64 octets en 0x8C1C03AC, juste
				// avant la table d'objets, sont une matrice 4x4 -- FUN_8C0B9BC0 y ecrit les seize
				// registres fr0..fr15, donc XMTRX, et FUN_8C0291AA l'appelle avec cette adresse.
				//
				//   manche 1 (deux sauvegardes)   matrice FIXE, identique
				//   manche 2 (trois sauvegardes)  la meme, composee d'une rotation de
				//                                 32,22  54,16  61,26 degres
				//
				// autour de l'axe (0 ; -0,9063 ; -0,4226), soit EXACTEMENT 25,00 degres de la
				// verticale -- et (0 ; 0,9063 ; 0,4226) est la colonne 0 de la matrice de la manche 1.
				// Le decor tourne donc autour de son propre premier axe.
				//
				// CONFIRME PAR UNE SECONDE VOIE, independante de la matrice : l'angle est range en
				// 0x8C1C03A0, douze octets avant elle, en demi-mot signe d'unites binaires
				// (65536 = 360 degres). Il vaut 0 aux deux sauvegardes de la manche 1, et -5866,
				// -9860, -11152 a celles de la manche 2 -- soit -32,22, -54,16 et -61,26 degres, les
				// memes valeurs au centieme. Deux mesures qui ne partagent rien et qui concordent.
				//
				// LA VITESSE, MESUREE : -3,8000 unites binaires par image.
				//
				// Deux sauvegardes de la MEME session (les combattants y sont identiques au bit
				// pres), prises a douze secondes d'intervalle :
				//
				//   compteur 0x8C288528   178848 -> 194488   soit 15640, et 15640/46 = 340,0 images
				//   hauteur  0x8C110FD4      381 ->    220   soit 381+340 = 721, moins 501 = 220
				//   angle    0x8C1C03A0    -9860 -> -11152   soit -1292
				//
				// La hauteur confirme le decompte d'images toute seule, par son cycle de 501 : elle
				// tombe sur 220 exactement. Deux grandeurs independantes, un seul nombre d'images.
				//
				//   -1292 / 340 = -3,8000 unites par image = -0,02087 degre, un tour en 287 s.
				//
				// Ce n'est donc pas une estimation. Mon precedent defaut de 12 en etait une, tiree
				// de sauvegardes venant de parties differentes, et elle etait fausse d'un facteur
				// trois.
				//
				// (Le texte ci-dessous reste : il dit pourquoi aucun increment n'est trouvable dans
				// le code, ce qui explique qu'il ait fallu mesurer.)
				//
				// POURQUOI LE CODE NE DONNE PAS CE NOMBRE.
				//
				// Le defaut etait 12 unites par image, tire d'un croisement avec le compteur
				// 0x8C288528. C'etait une estimation, et elle ne tient pas : les cinq sauvegardes
				// viennent de parties differentes, donc l'ecart entre deux d'entre elles ne dit rien.
				// Les deux paires donnaient d'ailleurs 12 et 4 unites par image -- un rapport de
				// trois, ce qui suffit a condamner la methode.
				//
				// Cherche et pas trouve : la routine de TEST (0x8C048878) ne touche jamais cet
				// angle ; FUN_8C0291AA ne fait que le LIRE, avec ses deux voisins, pour en faire
				// trois rotations et ranger XMTRX en 0x8C1C03AC ; 0x8C0293A0 n'est qu'un affichage
				// de debug des trois angles ; et le seul autre site qui ecrit 0x8C1C03A0
				// (0x8C04ABD4, increment 0x4000) est une boucle de quatre quarts de tour d'un AUTRE
				// decor, pas un increment par image.
				//
				// CE QUI TRANCHERAIT, et c'est une mesure, pas une estimation : deux sauvegardes
				// Flycast prises dans la MEME manche 2, a un nombre d'images connu d'ecart. La
				// difference des deux valeurs de 0x8C1C03A0 divisee par ce nombre donne la vitesse
				// au centieme, comme l'angle lui-meme a ete obtenu.
				//
				// Donc : pas de rotation tant que VF3_SPIN n'est pas donne. Le mecanisme est en
				// place et mesure ; seul son rythme attend sa mesure.
				Mat4 rotation = Mat4::identity();
				{
					const unsigned manche = board.ram.size() > (0x00581456 ^ 3)
					                      ? board.ram[0x00581456 ^ 3] : 0u;
					static double pas = -1.0;
					if (pas < 0) {
						const char *s = getenv("VF3_SPIN");
						pas = s ? atof(s) : 3.8;   // MESURE : voir juste au-dessus
					}
					// PAS DE CONDITION DE MANCHE -- ma premiere lecture etait fausse.
					//
					// Trois sauvegardes a la manche 2 portaient un angle non nul et deux a la manche
					// 1 un angle nul : la correlation etait trop belle. Deux sauvegardes FRAICHES,
					// prises toutes deux a la manche 2 et a 540 images d'ecart, donnent un angle
					// rigoureusement nul dans les deux. La parite n'y est donc pour rien, et le
					// cylindre tourne sur lui-meme comme Frederic le decrit depuis le debut.
					(void)manche;
					if (pas != 0.0) {
						const double a = double(tic) * pas * 2.0 * 3.14159265358979 / 65536.0;
						const double c = cos(a), s = sin(a), u = 1.0 - c;
						// L'AXE EST Y : CELUI DU CYLINDRE, DANS LE REPERE DE NOTRE GEOMETRIE.
						//
						// J'ai d'abord pose X, en raisonnant sur la convention de XMTRX. A l'ecran
						// Frederic a vu « la modification appliquee » -- une deformation -- et
						// « il ne tourne toujours pas sur lui-meme ». C'est exactement ce que fait
						// une rotation autour de X sur un cylindre dont l'axe est Y : elle le
						// COUCHE au lieu de le faire tourner.
						//
						// Or la boite de export_decor/TEST/TEST.obj dit x et z de -1000 a +1000 et
						// y de -500 a +500 : dans NOTRE geometrie, l'axe du cylindre est Y. C'est
						// ce repere-la qui compte pour composer la matrice, pas celui de la
						// Dreamcast. L'erreur etait de transporter un axe d'un repere a l'autre
						// sans verifier lequel portait la donnee.
						//
						// Et une rotation autour de Y se VOIT malgre la symetrie de revolution,
						// parce que le cylindre porte des barres verticales : c'est elles qui
						// defilent.
						//
						// (Le calcul d'origine, conserve : les deux conventions donnaient
						//  colonnes/monde X, colonnes/modele (0 ; 0,906 ; 0,423), rangees/monde
						//  (0 ; -0,906 ; -0,423). Aucune ne vaut sans savoir dans quel repere la
						//  geometrie convertie se trouve, et c'est la boite qui le dit.)
						//
						// Je l'avais d'abord lu a 25 degres de la verticale, et c'etait faux :
						// j'avais traite les seize flottants comme des RANGEES. Or FUN_8C0B9BC0
						// ecrit fr0..fr15, c'est-a-dire XMTRX, qui est en COLONNES. Le calcul refait
						// dans les deux conventions tranche sans ambiguite :
						//
						//   colonnes, monde  R = M2.M1t   axe ( 1,000 -0,000  0,000)
						//   colonnes, modele S = M1t.M2   axe (-0,000  0,906  0,423)
						//   rangees,  monde              axe ( 0,000 -0,906 -0,423)   <- mon erreur
						//
						// Les 25 degres existent bien, mais c'est l'axe dans l'espace du MODELE. En
						// coordonnees de monde -- celles que la conversion produit, cf. --cam
						// identite -- la rotation est un basculement autour de X. C'est-a-dire
						// « oblique, plongeant vers la droite », la description de Frederic.
						const double ax = 0.0, ay = 1.0, az = 0.0;
						// Rodrigues, en colonnes (Mat4 est en colonnes : m[col*4 + ligne]).
						const double R[3][3] = {
							{ c + ax*ax*u,      ax*ay*u + az*s,  ax*az*u - ay*s },
							{ ay*ax*u - az*s,   c + ay*ay*u,     ay*az*u + ax*s },
							{ az*ax*u + ay*s,   az*ay*u - ax*s,  c + az*az*u    },
						};
						for (int col = 0; col < 3; col++)
							for (int lig = 0; lig < 3; lig++)
								rotation.m[col * 4 + lig] = float(R[col][lig]);
					}
				}
				for (Model &m : mien.viewports[0].models) {
					// UN MODELE DE MORCEAU NE SE DESSINE PAS DANS LA SCENE AU REPOS.
					//
					// Les quatre morceaux de paroi font partie de l'entree MDL du decor, donc la
					// conversion les sort comme les autres -- mais le jeu ne les pose qu'a la
					// casse, chacun sur la matrice de son entree. Les laisser dans la scene fixe
					// mettrait quatre rubans de mille unites en permanence au milieu du ring.
					// On les reconnait a leur boite, celle que `morceaux.txt` donne.
					if (!in->morceaux.empty() && m.meshes) {
						float lo[3] = { 1e30f, 1e30f, 1e30f }, hi[3] = { -1e30f, -1e30f, -1e30f };
						for (const Mesh &me : *m.meshes)
							for (const Vertex &s : me.verts)
								for (int k = 0; k < 3; k++) {
									lo[k] = std::min(lo[k], s.pos[k]);
									hi[k] = std::max(hi[k], s.pos[k]);
								}
						bool morceau = false;
						for (const Morceau &q : in->morceaux) {
							bool pareil = true;
							for (int k = 0; k < 3 && pareil; k++)
								pareil = std::fabs(lo[k] - q.lo[k]) < 0.01f
								      && std::fabs(hi[k] - q.hi[k]) < 0.01f;
							if (pareil) { morceau = true; break; }
						}
						// On les met de cote plutot que de les jeter : la boucle des morceaux,
						// plus bas, y puise par identifiant.
						if (morceau) { modeles_morceaux.push_back(std::move(m)); continue; }
					}
					// UN PANNEAU DONT LE MUR EST TOMBE NE SE DESSINE PLUS.
					//
					// On range le modele par sa boite : un panneau de paroi est MINCE sur un axe et
					// large sur l'autre, et le signe de son centre sur l'axe mince dit de quel cote
					// il est. Un modele qui n'est mince sur aucun des deux -- le sol, l'enveloppe,
					// la jupe -- n'appartient a aucune paroi et reste toujours la.
					if (murs_casses && m.meshes) {
						float lo[3] = { 1e30f, 1e30f, 1e30f }, hi[3] = { -1e30f, -1e30f, -1e30f };
						for (const Mesh &me : *m.meshes)
							for (const Vertex &s : me.verts)
								for (int k = 0; k < 3; k++) {
									lo[k] = std::min(lo[k], s.pos[k]);
									hi[k] = std::max(hi[k], s.pos[k]);
								}
						const float ex = hi[0] - lo[0], ez = hi[2] - lo[2];
						const float cx = (lo[0] + hi[0]) * 0.5f, cz = (lo[2] + hi[2]) * 0.5f;
						uint32_t bit = 0;
						if (ex < ez * 0.5f) bit = cx > 0 ? 2u : 8u;        // mince en x : paroi x = +-D
						else if (ez < ex * 0.5f) bit = cz > 0 ? 4u : 1u;   // mince en z : paroi z = +-D
						if (bit && (murs_casses & bit))
							continue;
					}
					const Anime *sien = nullptr;
					for (const Anime &a : in->animes) {
						if (!m.meshes) break;
						for (const Mesh &me : *m.meshes)
							if (me.textured && me.x == a.tex_x && me.y == a.tex_y) { sien = &a; break; }
						if (sien) break;
					}
					if (sien) {
						// LE SENS DU CYLINDRE ALTERNE AVEC LE ROUND.
						//
						// La routine Dreamcast lit un octet en 0x8C1E6354, en garde le BIT 0 dans
						// 0x8C110FD0, et ce drapeau ne sert qu'a une chose (0x8C0488DC) :
						//
						//     drapeau 1 -> neg r1,r1   la translation vaut -hauteur
						//     drapeau 0 ->             elle vaut +hauteur
						//
						// Et 0x8C1E6354 est le NUMERO DE ROUND : releve a 1 sur deux sauvegardes
						// prises au round 1, a 2 sur trois prises au round 2, le drapeau suivant
						// son bit 0 a chaque fois. Le cylindre descend aux rounds impairs et monte
						// aux rounds pairs.
						//
						// La borne a le meme compteur en 0x00581456 : 0x0000BF90 le pose a 1 au
						// debut du match et 0x0000DD38 l'incremente a chaque round.
						//
						// anime.txt donne le sens du round 1 ; on l'inverse quand le round est pair.
						const unsigned manche = board.ram.size() > (0x00581456 ^ 3)
						                      ? board.ram[0x00581456 ^ 3] : 0u;
						float signe = (manche & 1) ? 1.0f : -1.0f;
						const float h = float(tic % uint64_t(sien->periode + 1)) * signe;
						// VF3_CYL_LOG=1 : une ligne a chaque changement de manche, pour savoir si le
						// signe est bien applique ici -- et non si l'octet bouge, ce que la sonde de
						// camera a deja etabli (manche 1 a l'image 1744, 2 a l'image 3432).
						if (getenv("VF3_CYL_LOG")) {
							static unsigned vue = 0xFFFFFFFF;
							if (manche != vue) {
								vue = manche;
								fprintf(stderr, "CYL : manche %u  signe %+.0f  periode %d  pas %g"
								        "  copies %d  d (%g, %g, %g)\n",
								        manche, double(signe), sien->periode, double(sien->pas),
								        sien->copies, double(sien->d[0]), double(sien->d[1]),
								        double(sien->d[2]));
							}
						}
						// Les copies d'abord : le meme modele, decale d'un multiple du pas. Celle du
						// decalage nul est la derniere, et c'est elle qui suit le chemin commun.
						for (int k = -sien->copies; k <= sien->copies; k++) {
							if (!k) continue;
							Model c = m;
							Mat4 t = Mat4::identity();
							t.m[12] = sien->d[0] * h;
							t.m[13] = sien->d[1] * h + sien->pas * float(k);
							t.m[14] = sien->d[2] * h;
							c.matrix = jeu.root * (rotation * (t * c.matrix));
							if (c.meshes)
								for (Mesh &me : *c.meshes) me.injected = true;
							a_nous.models.push_back(std::move(c));
						}
						Mat4 t = Mat4::identity();
						t.m[12] = sien->d[0] * h;
						t.m[13] = sien->d[1] * h;
						t.m[14] = sien->d[2] * h;
						// LA ROTATION NE CONCERNE QUE LE CYLINDRE, PAS LE RING.
						//
						// Premier essai : elle etait appliquee a TOUS les modeles injectes, et le ring
						// penchait avec -- les combattants se retrouvaient debout sur un sol incline.
						// La liste de dessin Dreamcast le disait pourtant : elle dessine quinze fois
						// LE MEME objet, [0x8C1C03EC + 0x14], et c'est le cylindre. La matrice de
						// 0x8C1C03AC est la sienne, pas celle du decor entier.
						m.matrix = rotation * (t * m.matrix);
					}
					m.matrix = jeu.root * m.matrix;
					// VF3_ECHELLE=1 : la taille de chaque modele injecte, TELLE QUE LE MOTEUR LA
					// DECODE, une seule fois.
					//
					// La regle est donnee par la borne elle-meme : le plan bouche-trou qu'on retire
					// plus haut fait x -6..6 et z -6..6 en espace modele, pour une arene qui vaut
					// +-5 (fiche_decor+0x30). Notre sol doit donc sortir a un rayon de 4,78 dans la
					// MEME echelle. S'il en sort deux fois plus grand, le combattant s'arrete au bon
					// endroit en coordonnees mais parait bute trop tot sur un ring trop large -- et
					// c'est la conversion qu'il faut corriger, pas la borne de collision.
					if (!echelle_dite && getenv("VF3_ECHELLE") && m.meshes) {
						float lo[3] = { 1e30f, 1e30f, 1e30f }, hi[3] = { -1e30f, -1e30f, -1e30f };
						size_t nv = 0;
						for (const Mesh &me : *m.meshes)
							for (const Vertex &s : me.verts) {
								nv++;
								for (int k = 0; k < 3; k++) {
									lo[k] = std::min(lo[k], s.pos[k]);
									hi[k] = std::max(hi[k], s.pos[k]);
								}
							}
						float r = 0;
						for (const Mesh &me : *m.meshes)
							for (const Vertex &s : me.verts)
								r = std::max(r, std::sqrt(s.pos[0] * s.pos[0] + s.pos[2] * s.pos[2]));
						fprintf(stderr, "echelle : modele %2zu  %zu sommets  rayon %8.3f"
						        "  x %8.3f..%8.3f  y %8.3f..%8.3f  z %8.3f..%8.3f\n",
						        a_nous.models.size(), nv, r, lo[0], hi[0], lo[1], hi[1], lo[2], hi[2]);
					}
					// Marquer les maillages : eux seuls prennent la planche injectee (r3d_raster.cpp).
					if (m.meshes)
						for (Mesh &me : *m.meshes) me.injected = true;
					a_nous.models.push_back(std::move(m));
				}
				if (getenv("VF3_ECHELLE") && !echelle_dite) {
					echelle_dite = true;
					fprintf(stderr, "echelle : fin (%zu modeles)\n", a_nous.models.size());
				}
				// LES MORCEAUX DE LA PAROI TOMBEE, POSES SUR LES POSITIONS DU JEU.
				//
				// La borne aligne trente-deux morceaux le long de la paroi (0x00027C04) et les anime
				// par image (0x000280AC). Leurs entrees font 0x10 octets -- x, y, z, l'etat en +0xC,
				// l'identifiant en +0xE -- et leur base est trouvee par la forme des entrees, faute
				// de pouvoir lire r12 (model3.cpp, murs_note). On ne simule rien : on lit SES
				// positions et on y pose notre geometrie.
				//
				// Quelle geometrie. On n'a pas de modele d'eclat : on reprend un panneau de
				// deflecteur, reduit au quart par sa matrice. C'est un premier jet assume, pas la
				// forme d'origine -- la Dreamcast a ses propres morceaux et ils restent a extraire.
				// LES ECLATS. Rallumes apres verification dans le code Dreamcast.
				//
				// Je les avais eteints en croyant poser trop de geometrie : un groupe 0 a 3 compte
				// neuf parties, dont deux rubans de mille unites. La lecture de 0x8C027F0C et de
				// 0x8C0BDCC0 montre que c'est faux -- un objet dessine le maillage de son +0x04,
				// 0x8C0C3900 fait `add #24, r4` pour sauter l'en-tete de groupe, et ces 24 octets
				// sont exactement ceux que notre decodeur appelle l'en-tete de groupe. Le groupe
				// EST l'objet que le jeu dessine, rubans compris.
				//
				// Les artefacts venaient d'ailleurs : du JEU, qui dessinait ses propres objets
				// inexistants depuis la liste de remplissage de TEST. C'est traite dans
				// model3.cpp, morceaux_neutraliser.
				//
				// VF3_ECLATS=0 les eteint.
				const char *eclats = getenv("VF3_ECLATS");
				if ((!eclats || *eclats != '0') && board.morceaux_base && !modeles_morceaux.empty()) {
					// Quel modele porte quel identifiant : `morceaux.txt` donne la BOITE de
					// chacun, et on apparie sur elle. Les quatre boites sont tres differentes
					// -- des rubans de mille unites dans quatre directions -- donc
					// l'appariement est sans ambiguite.
					const Model *par_id[16] = {};
					for (const Model &m : modeles_morceaux) {
						if (!m.meshes) continue;
						float lo[3] = { 1e30f, 1e30f, 1e30f }, hi[3] = { -1e30f, -1e30f, -1e30f };
						for (const Mesh &me : *m.meshes)
							for (const Vertex &s : me.verts)
								for (int k = 0; k < 3; k++) {
									lo[k] = std::min(lo[k], s.pos[k]);
									hi[k] = std::max(hi[k], s.pos[k]);
								}
						for (const Morceau &q : in->morceaux) {
							if (q.id < 0 || q.id >= 16 || par_id[q.id]) continue;
							bool pareil = true;
							for (int k = 0; k < 3 && pareil; k++)
								pareil = std::fabs(lo[k] - q.lo[k]) < 0.01f
								      && std::fabs(hi[k] - q.hi[k]) < 0.01f;
							if (pareil) { par_id[q.id] = &m; break; }
						}
					}
					auto octet = [&](uint32_t a) {
						return a < board.ram.size() ? board.ram[a ^ 3] : uint8_t(0);
					};
					auto flot = [&](uint32_t a) {
						uint32_t w = 0;
						for (int k = 0; k < 4; k++) w = (w << 8) | octet(a + uint32_t(k));
						float f; memcpy(&f, &w, 4); return f;
					};
					// CHAQUE MORCEAU PORTE SA PROPRE MATRICE 3x4, et on la prend telle quelle.
					//
					// La boucle de dessin du jeu (0x000284F4) appelle FUN_000021C0 sur
					// `0x00581A1C + i*0x1B4 + 0xD4` : douze flottants, une 3x3 en +0x00..+0x20 puis
					// la translation en +0x24..+0x2C, multiplies dans la matrice courante. La
					// Dreamcast fait exactement pareil en 0x8C0BA680, qui lit seize flottants depuis
					// `r12 + 0xD4` et fait quatre `ftrv xmtrx`.
					//
					// On lit donc SA matrice au lieu de poser nos modeles a la main : rotation et
					// position viennent avec, et il n'y a plus rien a inventer sur le placement.
					for (int i = 0; i < 32; i++) {
						const uint32_t e = board.morceaux_base + uint32_t(i) * 0x10;
						// L'octet d'etat ne sert plus de filtre : on l'a mis a 2 partout pour
						// que le JEU cesse de dessiner ses objets inexistants
						// (model3.cpp, morceaux_neutraliser). C'est la liste d'objets relevee
						// sur Dreamcast qui dit maintenant lesquels se dessinent.
						// L'identifiant de CE morceau, PRIS SUR LA DREAMCAST et non dans
						// l'entree de la borne : le champ +0x24 de la fiche de TEST est du
						// remplissage, et le jeu y lit des identifiants de 160 et 161. La
						// liste Dreamcast vaut `5 9 9 9 8 9 ...`, ou 9 est un terminateur :
						// huit morceaux seulement sont dessines sur les trente-deux.
						const unsigned id = size_t(i) < in->objets.size()
						                  ? unsigned(in->objets[size_t(i)])
						                  : ((unsigned(octet(e + 0xE)) << 8) | octet(e + 0xF));
						const Model *piece = id < 16 ? par_id[id] : nullptr;
						if (piece) {
							const uint32_t M = 0x00581A1C + uint32_t(i) * 0x1B4 + 0xD4;
							Mat4 t = Mat4::identity();
							for (int c3 = 0; c3 < 3; c3++)
								for (int r = 0; r < 3; r++)
									t.m[c3 * 4 + r] = flot(M + uint32_t(c3 * 3 + r) * 4);
							t.m[12] = flot(M + 0x24);
							t.m[13] = flot(M + 0x28);
							t.m[14] = flot(M + 0x2C);
							Model c = *piece;
							c.matrix = jeu.root * t;
							if (c.meshes)
								for (Mesh &me : *c.meshes) me.injected = true;
							a_nous.models.push_back(std::move(c));
						}
					}
				}
				injected_note(int(a_nous.models.size()));
				// NOTRE DECOR AMPUTE : la plainte, des que le compte baisse. Le decor converti est
				// statique, donc son nombre de modeles ne PEUT pas varier d'une image a l'autre --
				// s'il baisse, c'est que le parcours a ete interrompu, et le budget de noeuds est
				// le seul endroit ou ca arrive. La ligne dit combien le jeu en avait deja depense.
				{
					static size_t plein = 0;
					const size_t n = a_nous.models.size();
					if (n > plein) plein = n;
					static size_t dit = 0;
					if (n < plein && n != dit) {
						dit = n;
						fprintf(stderr, "decor ampute : %zu modeles sur %zu  (noeuds : le jeu %u,"
						        " nous %u, budget %u)\n",
						        n, plein, visites_du_jeu, visites_a_nous, NODE_BUDGET);
					}
				}
				// VF3_SCENE_LOG : la forme de la scene du jeu, ecrite quand elle change. Les combattants
				// manquent pendant les etats de presentation et les replays, et c'est la qu'on saura si notre
				// viewport ajoute y est pour quelque chose -- combien le jeu en produit, leurs priorites, et
				// combien de modeles chacun porte.
				if (getenv("VF3_SCENE_LOG")) {
					std::string forme;
					for (const Viewport &v : scene.viewports) {
						// La lumiere et le brouillard viennent avec : le sol n'est blanc que dans
						// trois sous-etats, et rien dans le chemin d'eclairage d'un maillage a
						// ombrage fixe, sans projecteur ni brouillard, n'en depend. Ce qui change
						// d'un etat a l'autre est donc la SCENE, et c'est cette ligne qui le dit.
						char buf[192];
						snprintf(buf, sizeof buf,
						         "[pri %d, %d modele(s), %gx%g en %g,%g | sol %.2f amb %.2f"
						         " proj %.2f/%.2f/%.2f brou %.2f/%.3f/%.3f] ",
						         v.priority, int(v.models.size()), v.vp_w, v.vp_h, v.vp_x, v.vp_y,
						         v.sun_intensity, v.ambient,
						         v.spot_colour[0], v.spot_colour[1], v.spot_colour[2],
						         v.scroll_fog, v.fog_density, v.fog_start);
						forme += buf;
						// LES VIEWPORTS MAIGRES, NOMMES. Celui qui ne porte qu'un ou deux modeles
						// est suspect : c'est la que vit le plan blanc que le jeu dessine sous les
						// combattants quand TEST n'a pas de geometrie a lui. On donne l'adresse du
						// modele et sa boite, de quoi le reconnaitre.
						if (v.models.size() <= 2)
							for (const Model &m : v.models) {
								if (!m.meshes) continue;
								float lo[3] = { 1e30f, 1e30f, 1e30f }, hi[3] = { -1e30f, -1e30f, -1e30f };
								size_t nv = 0;
								for (const Mesh &me : *m.meshes)
									for (const Vertex &s : me.verts) {
										nv++;
										for (int k = 0; k < 3; k++) {
											lo[k] = std::min(lo[k], s.pos[k]);
											hi[k] = std::max(hi[k], s.pos[k]);
										}
									}
								char b2[176];
								snprintf(b2, sizeof b2, "{maigre %06x : %zu maillage(s), %zu sommets,"
								         " x %.1f..%.1f y %.1f..%.1f z %.1f..%.1f} ",
								         m.addr, m.meshes->size(), nv,
								         lo[0], hi[0], lo[1], hi[1], lo[2], hi[2]);
								forme += b2;
							}
					}
					// L'ETAT DU JEU, EN TETE DE LIGNE, SOUS LE NOM DE SEGA. Le mode est en
					// 0x0058FD98 et le sous-etat en 0x0058FD99 (model3.cpp, fv2_mode), et Sega
					// a laisse les quatre-vingts noms en clair dans la ROM, une table de
					// pointeurs en 0x00024494. Les trois sous-etats ou le sol est blanc y
					// portent un nom : ROUND_MASK_*, SET_*/READY_*, REPLAY_*. La ligne porte
					// aussi l'index de decor (0x0058FD9F), dont depend notre `ce_decor`.
					static const char *SOUS_GAME[20] = {
						"ROUND_MASK_INT", "ROUND_MASK_IN", "ROUND_MASK_DSP", "ROUND_MASK_OUT",
						"ROUND_INT", "ROUND_DSP", "SET_INT", "SET_INT_POSE", "SET_DSP",
						"READY_INT", "READY_DSP", "FIGHT_INT", "FIGHT_DSP", "JUDGE", "JUDGE_DSP",
						"REPLAY_INT", "REPLAY_DSP", "VIC_INT", "VIC_DSP", "NEXT_PROG_SEL"
					};
					auto octet = [&](uint32_t a) {
						return board.ram.size() > (a ^ 3) ? board.ram[a ^ 3] : 0xFFu;
					};
					const unsigned mode = octet(0x0058FD98), sous = octet(0x0058FD99);
					const char *nom = ((mode == 6 || mode == 7) && sous < 20) ? SOUS_GAME[sous] : "?";
					char tete[96];
					snprintf(tete, sizeof tete, "mode %u, %s (%u), decor %u : ",
					         mode, nom, sous, octet(0x0058FD9F));
					forme = std::string(tete) + forme;
					static std::string avant;
					if (forme != avant) {
						avant = forme;
						fprintf(stderr, "scene : %s+ le notre (%d modeles), copie du porteur pri %d\n",
						        forme.c_str(), int(a_nous.models.size()), jeu.priority);
					}
				}
				scene.viewports.push_back(std::move(a_nous));
			}
		}
	}
	// VF3_TRANS_AUDIT=1: every translucent mesh in the scene, with where it came from and how big it is in the
	// camera's space, for finding the one that covers the picture (debugging)
	if (getenv("VF3_TRANS_AUDIT")) {
		for (size_t iv = 0; iv < scene.viewports.size(); iv++) {
			const Viewport &p = scene.viewports[iv];
			for (const Model &m : p.models)
				for (size_t im = 0; im < m.meshes->size(); im++) {
					const Mesh &me = (*m.meshes)[im];
					if (!me.poly_alpha && !me.texture_alpha && m.alpha >= 1.0f)
						continue;
					float lo[3] = { 1e30f, 1e30f, 1e30f }, hi[3] = { -1e30f, -1e30f, -1e30f };
					for (const Vertex &v : me.verts) {
						const float *M = m.matrix.m;
						const float t[3] = {
							M[0] * v.pos[0] + M[4] * v.pos[1] + M[8] * v.pos[2] + M[12],
							M[1] * v.pos[0] + M[5] * v.pos[1] + M[9] * v.pos[2] + M[13],
							M[2] * v.pos[0] + M[6] * v.pos[1] + M[10] * v.pos[2] + M[14],
						};
						for (int k = 0; k < 3; k++) { lo[k] = std::min(lo[k], t[k]); hi[k] = std::max(hi[k], t[k]); }
					}
					const uint8_t *c = me.verts.empty() ? nullptr : me.verts[0].colour;
					fprintf(stderr, "trans vp%zu model %06X mesh %zu  verts %zu  tex %d fmt %d  rgba %02X%02X%02X%02X"
					        "  node_alpha %.2f  view x %.0f..%.0f y %.0f..%.0f z %.0f..%.0f\n",
					        iv, m.addr, im, me.verts.size(), int(me.textured), me.format,
					        c ? c[0] : 0, c ? c[1] : 0, c ? c[2] : 0, c ? c[3] : 0, m.alpha,
					        lo[0], hi[0], lo[1], hi[1], lo[2], hi[2]);
				}
		}
	}
}

void SceneBuilder::mult_matrix(uint32_t index)
{
	if (!matrix_base)
		return;
	const float *s = matrix_base + index * 12;
	Mat4 m = {};
	m.m[0] = s[3]; m.m[4] = s[4]; m.m[8] = s[5]; m.m[12] = s[0];
	m.m[1] = s[6]; m.m[5] = s[7]; m.m[9] = s[8]; m.m[13] = s[1];
	m.m[2] = s[9]; m.m[6] = s[10]; m.m[10] = s[11]; m.m[14] = s[2];
	m.m[15] = 1;
	cur = cur * m;
}

void SceneBuilder::viewport(uint32_t addr, Scene &scene, int guard)
{
	for (; guard < 64; guard++) {
		if (!(addr & 0xFFFFFF))
			return;
		const uint32_t *v = culling(addr);
		if (!v)
			return;
		scene.viewports.emplace_back();
		Viewport &p = scene.viewports.back();
		vp = &p;
		p.priority = (v[0] >> 3) & 3;
		p.vp_x = (v[0x1A] & 0xFFFF) / 16.0f;
		p.vp_y = (v[0x1A] >> 16) / 16.0f;
		p.vp_w = (v[0x14] & 0xFFFF) / 4.0f;
		p.vp_h = (v[0x14] >> 16) / 4.0f;
		p.full_screen = p.vp_x == 0 && p.vp_w >= 495 && p.vp_y == 0 && p.vp_h >= 383;
		p.intensity_clamp = !node_shift;   // Step 1.0 only (r3d_scene.h)
		matrix_base = reinterpret_cast<const float *>(culling(v[0x16] & 0xFFFFFF));
		lod_table = reinterpret_cast<const float *>(culling(v[0x17] & 0xFFFFFF));
		float cv = as_float(v[8]), cw = as_float(v[9]), io = as_float(v[10]), jo = as_float(v[11]);
		planes = { as_float(v[0xC]), as_float(v[0xD]), as_float(v[0xE]), as_float(v[0xF]),
			as_float(v[0x10]), as_float(v[0x11]), as_float(v[0x12]), as_float(v[0x13]) };
		if (p.full_screen && widen > 1) {
			// x / widen in the side planes' tests: their edges as far out as the picture goes (the planes' normals
			// shortened with it: a sphere's distance read short, never culled sooner)
			planes.lv /= widen;
			planes.rv /= widen;
		}
		p.left = (0.0f - jo) / cv;
		p.right = (1.0f - jo) / cv;
		p.bottom = -(1.0f - io) / cw;
		p.top = -(0.0f - io) / cw;
		p.cota = as_float(v[3]);
		p.sun[0] = as_float(v[5]);
		p.sun[1] = -as_float(v[6]);
		p.sun[2] = -as_float(v[4]);
		p.sun_intensity = std::clamp(as_float(v[7]), 0.0f, 1.0f);
		if (std::isnan(p.sun_intensity)) p.sun_intensity = 0;
		p.ambient = ((v[0x24] >> 8) & 0xFF) / 255.0f;
		// VF3_AMBIENT=<n>: force the ambient light, for telling a scene that is lit too brightly from one whose
		// ambient is read wrongly (debugging; the game's own value is used when it isn't set)
		if (const char *a = getenv("VF3_AMBIENT")) p.ambient = float(atof(a));
		int spot = (v[0x20] >> 11) & 7, spot_fog = (v[0x20] >> 8) & 7;
		p.spot_ellipse[0] = int16_t(v[0x1E] & 0xFFFF) / 8.0f;
		p.spot_ellipse[1] = int16_t(v[0x1D] & 0xFFFF) / 8.0f;
		p.spot_ellipse[2] = std::round(2047.0f / std::max(1.0f, float((v[0x1E] >> 16) & 0xFFFF)));
		p.spot_ellipse[3] = std::round(2047.0f / std::max(1.0f, float((v[0x1D] >> 16) & 0xFFFF)));
		p.spot_range[0] = 1.0f / as_float(v[0x21]);
		p.spot_range[1] = as_float(v[0x1F]);
		for (int i = 0; i < 3; i++) {
			p.spot_colour[i] = spot_rgb[spot][i];
			p.spot_fog_colour[i] = spot_rgb[spot_fog][i];
		}
		p.fog_colour[0] = ((v[0x22] >> 16) & 0xFF) / 255.0f;
		p.fog_colour[1] = ((v[0x22] >> 8) & 0xFF) / 255.0f;
		p.fog_colour[2] = (v[0x22] & 0xFF) / 255.0f;
		p.fog_density = std::fabs(as_float(v[0x23]));
		if (!std::isfinite(p.fog_density)) p.fog_density = 0;
		p.fog_start = int16_t(v[0x25] & 0xFFFF) / 255.0f;
		p.fog_attenuation = ((v[0x24] >> 16) & 0xFF) / 255.0f;
		p.fog_ambient = ((v[0x25] >> 16) & 0xFF) / 255.0f;
		p.scroll_fog = (v[0x20] & 0xFF) / 255.0f;
		p.scroll_att = (v[0x24] & 0xFF) / 255.0f;

		attr = Attribs();
		depth = 0;
		stack.clear();
		// The Real3D's axes to GL's, then the base matrix
		cur = {};
		cur.m[4] = 1;      // x' = y
		cur.m[9] = -1;     // y' = -z
		cur.m[2] = -1;     // z' = -x
		cur.m[15] = 1;
		mult_matrix(0);
		path = 0;
		if (!(v[0] & 0x20) && ((v[2] >> 24) & 5) == 0)
			node_ptr(v[2]);
		if (v[1] == 0x01000000)
			return;
		addr = v[1];
	}
}

void SceneBuilder::node_ptr(uint32_t addr)
{
	if (!(addr & 0xFFFFFF))
		return;
	switch ((addr >> 24) & 5) {
	case 0: node(addr & 0xFFFFFF); break;
	case 1: draw_model(addr & 0xFFFFFF); break;
	case 4: pointer_list(addr & 0xFFFFFF); break;
	default: break;
	}
}

void SceneBuilder::pointer_list(uint32_t addr)
{
	const uint32_t *list = culling(addr);
	if (!list)
		return;
	for (int i = 0; i < 0x10000; i++) {
		if (list[i] & 0x01000000)
			break;
		node(list[i] & 0xFFFFFF);
		if (list[i] & 0x02000000)
			break;
	}
}

// A node and the siblings after it: the chain walked here, each drawn last first (as one recursion a sibling did:
// a node naming itself as its sibling, or a long chain, ran the stack out), at most MAX_CHAIN of them
void SceneBuilder::node(uint32_t addr)
{
	constexpr size_t MAX_CHAIN = 4096;
	const size_t base = chain.size();
	for (uint32_t a = addr; chain.size() - base < MAX_CHAIN;) {
		const uint32_t *n = culling(a);
		if (!n)
			break;
		chain.push_back(a);
		const uint32_t w0 = n[0], sibling = n[6 + node_shift] & 0x1FFFFFF;
		if ((w0 & 3) == 0 || (w0 & 0x300) == 0x300 || (w0 & 7) == 6 || (sibling & 0x1000000) || !sibling)
			break;
		a = sibling;
	}
	for (size_t i = chain.size(); i-- > base;) {
		const uint32_t a = chain[i];   // (a copy: node_one adds to the chain)
		node_one(a);
	}
	chain.resize(base);
}

void SceneBuilder::node_one(uint32_t addr)
{
	// (and at most NODE_BUDGET a scene: nodes that lead back to themselves, a cycle through children or a LOD's two
	// choices, doubled at each level up to the depth limit)
	if (depth >= 1024 || ++visits > NODE_BUDGET)
		return;
	const uint32_t *n = culling(addr);
	if (!n)
		return;
	const int s = node_shift;
	const uint32_t w0 = n[0], w1 = n[1 + s], w5 = n[5 + s], w6 = n[6 + s], w7 = n[7 + s];
	if ((w0 & 3) == 0)
		return;                 // a viewport: not drawn from here
	if ((w0 & 0x300) == 0x300)
		return;
	const uint32_t child = w5 & 0x7FFFFFF;
	if (w0 & 4)
		colour_table = ((w1 >> 19) | ((w5 >> 28) << 13) | ((w6 >> 25) << 17)) & 0xFFFFF;

	Attribs saved = attr;
	Mat4 saved_m = cur;
	uint32_t saved_path = path;
	path = path * 31 + addr;
	depth++;
	// The two words Step 1.5 and later have that Step 1.0 hasn't: a scale for the model below this node, a flag
	// that turns its culling off, and a texture offset with the sheet to take it from.
	if (s) {
		if (n[1] & 1)
			attr.scale = as_float(n[1] & ~3u);
		if (n[1] & 2)
			attr.no_cull = true;
		if (n[2] & 0x8000) {
			attr.tex_x = 32 * int((n[2] >> 7) & 0x3F);
			attr.tex_y = 32 * int(n[2] & 0x1F);
			attr.page = int((n[2] & 0x4000) >> 14);
		}
	}
	if (w0 & 0x10) {
		Mat4 t = Mat4::identity();
		t.m[12] = as_float(n[2 + s]);
		t.m[13] = as_float(n[3 + s]);
		t.m[14] = as_float(n[4 + s]);
		cur = cur * t;
	} else if (w1 & 0xFFF) {
		mult_matrix(w1 & 0xFFF);
		if (vp && vp->root_matrix < 0) { vp->root = cur; vp->root_matrix = int(w1 & 0xFFF); }
	}
	if (w0 & 0x80) {
		// the rotation taken out (the transposed 3x3, its columns normalised, multiplied in)
		Mat4 m = cur;
		std::swap(m.m[1], m.m[4]);
		std::swap(m.m[2], m.m[8]);
		std::swap(m.m[6], m.m[9]);
		m.m[12] = m.m[13] = m.m[14] = 0;
		m.m[15] = 1;
		for (int c = 0; c < 3; c++) {
			float s = std::sqrt(m.m[c * 4] * m.m[c * 4] + m.m[c * 4 + 1] * m.m[c * 4 + 1] + m.m[c * 4 + 2] * m.m[c * 4 + 2]);
			if (s != 0)
				for (int r = 0; r < 3; r++) m.m[c * 4 + r] /= s;
		}
		cur = cur * m;
	}
	const float x = cur.m[12], y = cur.m[13], z = cur.m[14];
	const float cull_r = float16(w7 & 0xFFFF) * attr.scale;
	const float blend_r = float16(w7 >> 16) * attr.scale;
	const bool outside = (z * planes.lu - x * planes.lv) > cull_r || (z * planes.tu + y * planes.tw) > cull_r ||
		(z * planes.ru - x * planes.rv) > cull_r || (z * planes.bu + y * planes.bw) > cull_r;
	float lod_scale = attr.no_cull ? std::numeric_limits<float>::max() : blend_r / std::sqrt(x * x + y * y + z * z);
	if (!(lod_scale >= 0)) lod_scale = 0;   // NaN too
	const int lod_entry = (w1 >> 12) & 0x7F;
	// the LOD table: 128 entries of 4 (delete size, blend factor)
	auto lod_del = [&](int i) { return lod_table ? lod_table[lod_entry * 8 + i * 2] : 0.0f; };
	auto lod_blend = [&](int i) { return lod_table ? lod_table[lod_entry * 8 + i * 2 + 1] : 0.0f; };
	if (attr.no_cull || (!outside && lod_scale >= lod_del(3))) {
		if (w0 & 8) {
			if (const uint32_t *lod = culling(child)) {
				int k;
				for (k = 0; k < 3; k++)
					if (lod_scale >= lod_del(k) && (lod[k] & 0x1000000))
						break;
				float before = attr.alpha;
				float a = std::clamp(lod_blend(k) * (lod_scale - lod_del(k)), 0.0f, 1.0f);
				if (!(a == a)) a = 0;
				if (a > 31.0f / 32.0f) a = 1;
				else if (a < 1.0f / 32.0f) a = 0;
				attr.alpha *= a;
				bool nodes = (w1 & 0x20000000) != 0;
				if (nodes) node(lod[k] & 0xFFFFFF);
				else draw_model(lod[k] & 0xFFFFFF);
				if (a < 1.0f && k != 3) {
					attr.alpha = (1.0f - a) * before;
					if (nodes) node(lod[k + 1] & 0xFFFFFF);
					else draw_model(lod[k + 1] & 0xFFFFFF);
				}
			}
		} else {
			float a = std::clamp(lod_blend(3) * (lod_scale - lod_del(3)), 0.0f, 1.0f);
			if (!(a == a)) a = 0;
			attr.alpha *= a;
			node_ptr(child);
		}
	}
	depth--;
	cur = saved_m;
	attr = saved;
	path = saved_path;
}

void SceneBuilder::draw_model(uint32_t addr)
{
	addr &= 0xFFFFFF;
	const uint32_t *data = model_at(addr);
	Model m;
	m.addr = addr;
	m.matrix = cur;
	m.alpha = attr.alpha;
	m.scale = attr.scale;
	m.page = attr.page;
	m.tex_x = attr.tex_x;
	m.tex_y = attr.tex_y;
	m.node_path = path;
	// ROM models without colour table indexes are the same every frame
	bool dynamic = addr < 0x100000;
	if (!dynamic) {
		const uint32_t *h = data;
		const uint32_t *end_rom = vrom + 0x1000000;
		for (int i = 0; i < 100000 && h + 7 <= end_rom; i++) {
			if (!(h[1] & 2)) { dynamic = true; break; }
			if (h[1] & 4) break;
			h += 7 + (((h[0] & 0x40) ? 4 : 3) - shared_count[h[0] & 0xF]) * 4;
		}
	}
	if (!dynamic) {
		auto it = rom_cache.find(addr);
		if (it != rom_cache.end()) {
			m.meshes = it->second;
		} else {
			m.meshes = parse(data, false);
			rom_cache[addr] = m.meshes;
		}
	} else if (!(data[0] & 0xF)) {
		// (a model whose first polygon shares no vertices with the model before: parsed again only when its words
		// (built in polygon RAM: three in four of them the same as the frame before) or an entry of the colour table it
		// reads have changed; about 80 a frame were parsed each frame)
		uint64_t hs = 1469598103934665603ull;
		if (addr < 0x100000) {
			const uint32_t *h = data, *end = poly + 0x100000;
			for (int i = 0; i < 100000; i++) {
				if (h + 7 > end || h[6] == 0) break;
				const size_t n = 7 + size_t(((h[0] & 0x40) ? 4 : 3) - shared_count[h[0] & 0xF]) * 4;
				for (size_t k = 0; k < n && h + k < end; k++) hs = (hs ^ h[k]) * 1099511628211ull;
				if (h[1] & 4) break;
				h += n;
			}
		}
		DynEntry &e = dyn_cache[addr];
		bool same = e.meshes && e.hash == hs && e.table == colour_table;
		for (size_t i = 0; same && i < e.refs.size(); i++) same = poly[e.refs[i].first] == e.refs[i].second;
		if (same) {
			m.meshes = e.meshes;
			std::copy(&e.prev[0], &e.prev[0] + 4, &prev[0]);
			memcpy(prev_uv, e.prev_uv, sizeof prev_uv);
		} else {
			e.refs.clear();
			colour_refs = &e.refs;
			m.meshes = parse(data, true);
			colour_refs = nullptr;
			e.meshes = m.meshes;
			e.hash = hs;
			e.table = colour_table;
			std::copy(&prev[0], &prev[0] + 4, &e.prev[0]);
			memcpy(e.prev_uv, prev_uv, sizeof prev_uv);
		}
	} else {
		m.meshes = parse(data, true);
	}
	if (!m.meshes->empty())
		vp->models.push_back(std::move(m));
}

namespace {

uint64_t header_key(const uint32_t *h)
{
	uint64_t k = h[3] & 0xFF;
	k |= uint64_t(((h[4] & 0x1F) << 1) | ((h[5] >> 7) & 1)) << 8;
	k |= uint64_t(h[5] & 0x1F) << 14;
	k |= uint64_t((h[4] & 0xC0) >> 6) << 19;
	k |= uint64_t((h[1] >> 4) & 1) << 21;
	k |= uint64_t(h[6] >> 31) << 22;
	k |= uint64_t(!(h[6] & 0x800000)) << 23;
	k |= uint64_t(h[2] & 0xFF) << 24;
	k |= uint64_t((h[0] >> 7) & 1) << 32;
	k |= uint64_t((h[1] >> 3) & 1) << 33;
	k |= uint64_t((h[1] >> 5) & 1) << 34;
	k |= uint64_t(h[0] >> 26) << 35;
	k |= uint64_t(h[6] & 0x3FFFF) << 41;
	k |= uint64_t(h[1] & 1) << 59;
	return k;
}

void mesh_state(Mesh &m, const uint32_t *h)
{
	m.textured = (h[6] & 0x400) != 0;
	m.alpha_test = (h[6] & 0x80000000) != 0;
	m.texture_alpha = (h[6] & 7) != 0;
	m.poly_alpha = !(h[6] & 0x800000);
	m.lighting = !(h[6] & 0x10000);
	m.smooth_shading = (h[1] & 8) != 0;
	m.fixed_shading = (h[1] & 0x20) && !m.smooth_shading;
	m.high_priority = (h[6] & 0x10) != 0;
	m.trans_select = (h[6] & 0x20000) != 0;
	m.layered = (h[6] & 8) != 0;
	m.specular = (h[0] & 0x80) != 0;
	m.shininess = float((h[6] >> 5) & 3);
	m.specular_value = (h[0] >> 26) / 63.0f;
	m.fog_intensity = ((h[6] >> 11) & 0x1F) / 16.0f;
	m.translator_map = (h[4] & 0x80) != 0;
	if (m.textured) {
		int f = (h[6] >> 7) & 7;
		if (m.alpha_test && f >= 1 && f <= 4) f += 7;   // contour: the 4-bit alpha formats lose their alpha
		m.format = f;
		if (f == 7) m.alpha_test = false;
		m.x = (32 * (((h[4] & 0x1F) << 1) | ((h[5] >> 7) & 1))) & 2047;
		m.y = (32 * (h[5] & 0x1F)) & 2047;
		unsigned w = (h[3] >> 3) & 7, hh = h[3] & 7;
		m.width = 32 << (w >= 6 ? 0 : w);
		m.height = 32 << (hh >= 6 ? 0 : hh);
		m.micro = (h[2] & 0x10) != 0;
		m.micro_id = (h[2] >> 5) & 7;
		m.micro_min_lod = float((h[2] >> 2) & 3);
		m.inverted = ((h[6] >> 24) & 0x7F) == 2;
		m.page = (h[4] >> 6) & 1;
		bool su = (h[3] & 0x80) && !m.alpha_test, sv = (h[3] & 0x40) && !m.alpha_test;
		m.wrap_u = (h[2] & 2) ? (su ? 2 : 3) : (su ? 0 : 1);
		m.wrap_v = (h[2] & 1) ? (sv ? 2 : 3) : (sv ? 0 : 1);
	}
}

} // namespace

Meshes SceneBuilder::parse(const uint32_t *h, bool dynamic)
{
	(void)dynamic;
	auto out = std::make_shared<std::vector<Mesh>>();
	std::map<std::pair<uint64_t, uint32_t>, size_t> by_key;
	uint64_t last_key = ~0ull;
	uint32_t last_rgba = 0;
	Mesh *mesh = nullptr;
	const uint32_t *end_lo = poly + 0x100000, *end_rom = vrom + 0x1000000;
	const uint32_t *limit = (h >= poly && h < end_lo) ? end_lo : end_rom;
	for (int guard = 0; guard < 200000; guard++) {
		if (h + 7 > limit || h[6] == 0)
			break;
		uint8_t colour[4];
		if (!(h[1] & 2)) {
			const uint32_t ci = (colour_table + ((h[4] >> 8) & 0xFFF)) & 0xFFFFF;
			uint32_t c = poly[ci];
			if (colour_refs) colour_refs->push_back({ ci, c });
			colour[0] = uint8_t(c >> 16); colour[1] = uint8_t(c >> 8); colour[2] = uint8_t(c);
		} else {
			colour[0] = uint8_t(h[4] >> 24); colour[1] = uint8_t(h[4] >> 16); colour[2] = uint8_t(h[4] >> 8);
		}
		colour[3] = (h[6] & 0x800000) ? 255 : uint8_t((((h[6] >> 18) & 0x3F) * 255) / 32);
		if ((h[0] & 0x200) && !(h[0] & 0x100))
			colour[3] /= 2;
		// a mesh per state and colour (the renderers take a mesh's colour from its first polygon)
		uint64_t key = header_key(h);
		uint32_t rgba;
		memcpy(&rgba, colour, 4);
		if (key != last_key || rgba != last_rgba) {
			auto it = by_key.find({ key, rgba });
			if (it == by_key.end()) {
				by_key[{ key, rgba }] = out->size();
				out->emplace_back();
				mesh = &out->back();
				mesh->key = key;
				mesh_state(*mesh, h);
			} else {
				mesh = &(*out)[it->second];
			}
		}
		const int count = (h[0] & 0x40) ? 4 : 3;
		const float uv_scale = (h[1] & 0x40) ? 1.0f : 1.0f / 8.0f;
		float face[3] = { float(int32_t(h[1]) >> 8) / 4194304.0f, float(int32_t(h[2]) >> 8) / 4194304.0f,
			float(int32_t(h[3]) >> 8) / 4194304.0f };
		Vertex v[4] = {};
		uint16_t uv[4][2] = {};
		int j = 0;
		for (int i = 0; i < 4; i++)
			if (h[0] & (1u << i)) {
				v[j] = prev[i];
				uv[j][0] = prev_uv[i][0];
				uv[j][1] = prev_uv[i][1];
				if (key != last_key && mesh->textured) {
					v[j].uv[0] = uv[j][0] * uv_scale / mesh->width;
					v[j].uv[1] = uv[j][1] * uv_scale / mesh->height;
				}
				j++;
			}
		last_key = key;
		last_rgba = rgba;
		const bool smooth = (h[1] & 8) != 0, fixed = (h[1] & 0x20) != 0, spec = (h[0] & 0x80) != 0;
		if (!smooth)
			for (int i = 0; i < count; i++) memcpy(v[i].normal, face, sizeof(face));
		const float np = float(h[5] >> 8) * np_scale;
		const uint32_t *d = h + 7;
		for (; j < count; j++, d += 4) {
			if (d + 4 > limit) break;
			v[j].pos[0] = float(int32_t(d[0]) >> 8) * vertex_scale;
			v[j].pos[1] = float(int32_t(d[1]) >> 8) * vertex_scale;
			v[j].pos[2] = float(int32_t(d[2]) >> 8) * vertex_scale;
			if (smooth) {
				v[j].normal[0] = byte_to_float(int8_t(d[0]));
				v[j].normal[1] = byte_to_float(int8_t(d[1]));
				v[j].normal[2] = byte_to_float(int8_t(d[2]));
			}
			if (fixed && !smooth)
				v[j].fixed_shade = spec ? (d[0] & 0xFF) / 255.0f : byte_to_float(int8_t(d[0]));
			uv[j][0] = uint16_t(d[3] >> 16);
			uv[j][1] = uint16_t(d[3]);
			v[j].uv[0] = mesh->textured ? uv[j][0] * uv_scale / mesh->width : 0;
			v[j].uv[1] = mesh->textured ? uv[j][1] * uv_scale / mesh->height : 0;
		}
		for (int i = 0; i < count; i++) {
			memcpy(v[i].face_normal, face, sizeof(face));
			v[i].texture_np = np;
			memcpy(v[i].colour, colour, 4);
		}
		auto emit = [&](const Vertex *p) {
			if (p[0].colour[3] == 0) return;
			mesh->verts.push_back(p[0]);
			mesh->verts.push_back(p[1]);
			mesh->verts.push_back(p[2]);
			if (count == 4) {
				mesh->verts.push_back(p[0]);
				mesh->verts.push_back(p[2]);
				mesh->verts.push_back(p[3]);
			}
		};
		const bool discard = (h[0] & 0x300) == 0x300;
		if ((h[1] & 0x10) && !discard) {
			Vertex b[4];
			for (int i = 0; i < 4; i++) {
				b[i] = v[i];
				for (int k = 0; k < 3; k++) { b[i].face_normal[k] = -b[i].face_normal[k]; b[i].normal[k] = -b[i].normal[k]; }
			}
			emit(b);
		}
		if (!discard)
			emit(v);
		for (int i = 0; i < 4; i++) {
			prev[i] = v[i];
			prev_uv[i][0] = uv[i][0];
			prev_uv[i][1] = uv[i][1];
		}
		if (h[1] & 4)
			break;
		h += 7 + (count - shared_count[h[0] & 0xF]) * 4;
	}
	// meshes with no triangles are dropped
	out->erase(std::remove_if(out->begin(), out->end(), [](const Mesh &m) { return m.verts.empty(); }), out->end());
	return out;
}

namespace {

float moved2(const Model &p, const Model &m)
{
	float d = 0;
	for (int r = 0; r < 3; r++) {
		const float e = p.matrix.m[12 + r] - m.matrix.m[12 + r];
		d += e * e;
	}
	return d;
}

// An affine matrix's inverse (false if it has none)
bool affine_inverse(const Mat4 &a, Mat4 &r)
{
	const float *m = a.m;
	const float c00 = m[5] * m[10] - m[9] * m[6], c01 = m[9] * m[2] - m[1] * m[10], c02 = m[1] * m[6] - m[5] * m[2];
	const float det = m[0] * c00 + m[4] * c01 + m[8] * c02;
	if (!(std::fabs(det) > 1e-20f)) return false;
	const float k = 1.0f / det;
	r = Mat4::identity();
	r.m[0] = c00 * k; r.m[1] = c01 * k; r.m[2] = c02 * k;
	r.m[4] = (m[8] * m[6] - m[4] * m[10]) * k; r.m[5] = (m[0] * m[10] - m[8] * m[2]) * k; r.m[6] = (m[4] * m[2] - m[0] * m[6]) * k;
	r.m[8] = (m[4] * m[9] - m[8] * m[5]) * k; r.m[9] = (m[8] * m[1] - m[0] * m[9]) * k; r.m[10] = (m[0] * m[5] - m[4] * m[1]) * k;
	for (int i = 0; i < 3; i++) r.m[12 + i] = -(r.m[i] * m[12] + r.m[4 + i] * m[13] + r.m[8 + i] * m[14]);
	return true;
}

// Where a's model would be in b if it stood still while the camera moved by c (b = c a)
float predicted2(const Mat4 &c, const Model &p, const Model &m)
{
	float d = 0;
	for (int r = 0; r < 3; r++) {
		const float x = c.m[r] * p.matrix.m[12] + c.m[4 + r] * p.matrix.m[13] + c.m[8 + r] * p.matrix.m[14] + c.m[12 + r];
		const float e = x - m.matrix.m[12 + r];
		d += e * e;
	}
	return d;
}

// How differently two models point (their axes, a's turned by the camera's move c if known): 0 the same, up to 12
float turned2(const Mat4 *c, const Model &p, const Model &m)
{
	float d = 0;
	for (int j = 0; j < 3; j++)
		for (int r = 0; r < 3; r++) {
			float x = p.matrix.m[j * 4 + r];
			if (c) x = c->m[r] * p.matrix.m[j * 4] + c->m[4 + r] * p.matrix.m[j * 4 + 1] + c->m[8 + r] * p.matrix.m[j * 4 + 2];
			const float e = x - m.matrix.m[j * 4 + r];
			d += e * e;
		}
	return d;
}

float dist2(const Model &m)
{
	return m.matrix.m[12] * m.matrix.m[12] + m.matrix.m[13] * m.matrix.m[13] + m.matrix.m[14] * m.matrix.m[14];
}

// a model's shape: its meshes and triangles
uint64_t shape_of(const Model &m)
{
	uint64_t n = m.meshes ? m.meshes->size() : 0, verts = 0;
	if (m.meshes)
		for (auto &mesh : *m.meshes) verts += mesh.verts.size();
	return (n << 32) ^ verts;
}

} // namespace

// The camera's move between two frames' viewport (b = camera a, for what stands still), from the models alone at
// their address in both: the one most of the others agree with (the fighters move on their own)
bool camera_move(const Viewport &va, const Viewport &vb, Mat4 &camera)
{
	const std::vector<Model> &was = va.models, &now = vb.models;
	std::unordered_map<uint32_t, int> n_was, n_now;
	for (auto &m : was) n_was[m.addr]++;
	for (auto &m : now) n_now[m.addr]++;
	std::vector<std::pair<int, int>> sure;
	{
		std::unordered_map<uint32_t, int> at;
		for (size_t i = 0; i < was.size(); i++)
			if (n_was[was[i].addr] == 1) at[was[i].addr] = int(i);
		for (size_t j = 0; j < now.size(); j++)
			if (n_now[now[j].addr] == 1)
				if (auto it = at.find(now[j].addr); it != at.end()) sure.push_back({ it->second, int(j) });
	}
	// Only the farther half count, by their distance: most of the models alone at their address are the fighters'
	// parts, near the camera and moving together, and the stage's far parts show the camera's turn
	{
		std::vector<float> d;
		for (auto &q : sure) d.push_back(dist2(now[size_t(q.second)]));
		if (d.size() < 3) return false;
		std::nth_element(d.begin(), d.begin() + d.size() / 2, d.end());
		const float median = d[d.size() / 2];
		sure.erase(std::remove_if(sure.begin(), sure.end(), [&](const std::pair<int, int> &q) { return dist2(now[size_t(q.second)]) < median; }), sure.end());
	}
	camera = Mat4::identity();
	double best = 0, all = 0;
	size_t best_n = 0;
	for (auto &q : sure) all += std::sqrt(dist2(now[size_t(q.second)]));
	const size_t step = std::max<size_t>(1, sure.size() / 48);
	for (size_t k = 0; k < sure.size(); k += step) {
		Mat4 inv, c;
		if (!affine_inverse(was[size_t(sure[k].first)].matrix, inv)) continue;
		c = now[size_t(sure[k].second)].matrix * inv;
		double agree = 0;
		size_t n = 0;
		for (auto &q : sure)
			if (predicted2(c, was[size_t(q.first)], now[size_t(q.second)]) < 4e-4f * dist2(now[size_t(q.second)]) + 1e-2f) {
				agree += std::sqrt(dist2(now[size_t(q.second)]));
				n++;
			}
		if (agree > best) { best = agree; best_n = n; camera = c; }
	}
	return best_n >= 3 && best * 3 >= all;
}

namespace {

// b's model m's meshes with their vertices blended from p's (the same meshes made again)
// A model the game builds each frame (a fighter's joint, between two limbs): its vertices where they are between the
// two frames in the view (each from where a's matrix put it to where b's does), given in the blended matrix's terms.
// Blending the vertices and the matrix each on their own put them elsewhere when both change (a knee bending as the
// leg turns: the cross term), the joint off the limbs' ends, which blend exactly (a rigid model's vertex goes from a's
// place to b's): Lau's trousers in pieces for a picture as he changed stance.
void blend_vertices(const Model &p, Model &m, float t)
{
	const float *A = p.matrix.m, *B = m.matrix.m;
	float M[16];
	for (int i = 0; i < 16; i++) M[i] = A[i] + (B[i] - A[i]) * t;
	// the blended matrix's 3x3 inverse (column-major: column c, row r at c * 4 + r)
	auto at = [&](int r, int c) { return M[c * 4 + r]; };
	const float det = at(0, 0) * (at(1, 1) * at(2, 2) - at(1, 2) * at(2, 1)) - at(0, 1) * (at(1, 0) * at(2, 2) - at(1, 2) * at(2, 0)) +
	                  at(0, 2) * (at(1, 0) * at(2, 1) - at(1, 1) * at(2, 0));
	const bool view = std::fabs(det) > 1e-12f;
	float inv[3][3] = {};
	if (view) {
		const float k = 1.0f / det;
		inv[0][0] = (at(1, 1) * at(2, 2) - at(1, 2) * at(2, 1)) * k;
		inv[0][1] = (at(0, 2) * at(2, 1) - at(0, 1) * at(2, 2)) * k;
		inv[0][2] = (at(0, 1) * at(1, 2) - at(0, 2) * at(1, 1)) * k;
		inv[1][0] = (at(1, 2) * at(2, 0) - at(1, 0) * at(2, 2)) * k;
		inv[1][1] = (at(0, 0) * at(2, 2) - at(0, 2) * at(2, 0)) * k;
		inv[1][2] = (at(0, 2) * at(1, 0) - at(0, 0) * at(1, 2)) * k;
		inv[2][0] = (at(1, 0) * at(2, 1) - at(1, 1) * at(2, 0)) * k;
		inv[2][1] = (at(0, 1) * at(2, 0) - at(0, 0) * at(2, 1)) * k;
		inv[2][2] = (at(0, 0) * at(1, 1) - at(0, 1) * at(1, 0)) * k;
	}
	// x in the view: a's (or b's) matrix on a model-space vector (w 1 for a point, 0 for a direction)
	auto apply = [](const float *X, const float *v, float w, float *o) {
		for (int r = 0; r < 3; r++) o[r] = X[r] * v[0] + X[4 + r] * v[1] + X[8 + r] * v[2] + X[12 + r] * w;
	};
	auto blend = [&](const float *from, float *to, float w) {
		if (!view) {   // (a matrix that flattens it: as before)
			for (int c = 0; c < 3; c++) to[c] = from[c] + (to[c] - from[c]) * t;
			return;
		}
		float x[3], y[3], z[3];
		apply(A, from, w, x);
		apply(B, to, w, y);
		for (int c = 0; c < 3; c++) z[c] = x[c] + (y[c] - x[c]) * t - M[12 + c] * w;
		for (int r = 0; r < 3; r++) to[r] = inv[r][0] * z[0] + inv[r][1] * z[1] + inv[r][2] * z[2];
	};
	auto blended = std::make_shared<std::vector<Mesh>>(*m.meshes);
	for (size_t k = 0; k < blended->size(); k++) {
		const std::vector<Vertex> &from = (*p.meshes)[k].verts;
		std::vector<Vertex> &to = (*blended)[k].verts;
		for (size_t i = 0; i < to.size(); i++) {
			blend(from[i].pos, to[i].pos, 1);
			blend(from[i].normal, to[i].normal, 0);
			blend(from[i].face_normal, to[i].face_normal, 0);
		}
	}
	m.meshes = blended;
}

// The pairing of two frames' models, kept for the next picture between the same two (at 120 a second there are two;
// the camera's move and the pairing don't depend on where between them a picture is): for each of b's models, a's it
// is blended from (-1: shown as it is in b) and whether its vertices are too
// A flipbook (Shun's harbour water: a 64x64 picture and wave shapes that step every 3 frames, ~19 a second): a model
// whose texture moved to another place since the frame before, its shapes the same in number. Its last step is
// remembered (from: the model as it was before it) with the frames since (age); once two steps have come at most 8
// frames apart (period), each picture shows the step faded in from the one before over a period, a period late: it
// moves at the pictures' rate, not 19 steps a second. A step that doesn't repeat (a face's texture changed once) isn't.
struct Flip {
	Meshes from;
	int age = -1;      // frames since the step (-1: none remembered)
	int period = 0;    // frames between its last two steps (0: not known yet)
};
bool flip_step(const Model &p, const Model &m)
{
	if (!p.meshes || !m.meshes || p.meshes == m.meshes || p.meshes->size() != m.meshes->size()) return false;
	bool moved = false;
	for (size_t k = 0; k < m.meshes->size(); k++) {
		const Mesh &a = (*p.meshes)[k], &c = (*m.meshes)[k];
		if (a.verts.size() != c.verts.size() || a.textured != c.textured) return false;
		if (c.textured && a.width == c.width && a.height == c.height && a.page == c.page && a.format == c.format && (a.x != c.x || a.y != c.y)) moved = true;
	}
	return moved;
}

struct Plan {
	uint64_t a = 0, b = 0;
	bool ok = false;
	InterpStats st;
	std::vector<std::vector<int>> from;
	std::vector<std::vector<int>> verts;     // a's model each of b's vertices are blended from, + 1 (0: not blended)
	std::vector<std::vector<int>> paired;    // every one of b's models' pair in a (blended or not)
	std::vector<std::vector<Flip>> flips;    // per b's model
};
Plan plan;

// the flipbooks' steps faded in (after the rest of the blend): their shapes and the two pictures, by where between its
// frames the picture is
void apply_flips(const Scene &b, float t, Scene &out)
{
	static const bool off = getenv("VF3_NO_FLIPS") != nullptr;   // (tests: as before)
	if (off) return;
	for (size_t v = 0; v < out.viewports.size() && v < plan.flips.size(); v++)
		for (size_t j = 0; j < out.viewports[v].models.size() && j < plan.flips[v].size(); j++) {
			const Flip &f = plan.flips[v][j];
			if (f.age < 0 || f.period <= 0) continue;
			const float w = (float(f.age) + t) / float(f.period);   // (the newer step's share)
			if (w >= 1) continue;
			const Meshes &to = b.viewports[v].models[j].meshes;
			if (!to || !f.from || to->size() != f.from->size()) continue;
			auto blended = std::make_shared<std::vector<Mesh>>(*to);
			for (size_t k = 0; k < blended->size(); k++) {
				Mesh &n = (*blended)[k];
				const Mesh &o = (*f.from)[k];
				if (n.verts.size() != o.verts.size()) continue;
				for (size_t i = 0; i < n.verts.size(); i++)
					for (int c = 0; c < 3; c++) {
						n.verts[i].pos[c] = o.verts[i].pos[c] + (n.verts[i].pos[c] - o.verts[i].pos[c]) * w;
						n.verts[i].normal[c] = o.verts[i].normal[c] + (n.verts[i].normal[c] - o.verts[i].normal[c]) * w;
						n.verts[i].face_normal[c] = o.verts[i].face_normal[c] + (n.verts[i].face_normal[c] - o.verts[i].face_normal[c]) * w;
					}
				if (n.textured && (o.x != n.x || o.y != n.y) && o.width == n.width && o.height == n.height && o.page == n.page) {
					n.flip = 1 - w;
					n.flip_x = o.x;
					n.flip_y = o.y;
				}
			}
			out.viewports[v].models[j].meshes = blended;
		}
}

} // namespace

bool interpolate(const Scene &a, const Scene &b, float t, Scene &out, InterpStats *st)
{
	out = b;
	if (a.viewports.size() != b.viewports.size() || a.blank || b.blank)
		return false;
	static const bool no_plan = getenv("VF3_NO_INTERP_PLAN") != nullptr;   // (tests: every picture paired afresh)
	if (!no_plan && a.id && b.id && plan.a == a.id && plan.b == b.id && plan.from.size() == out.viewports.size()) {
		if (st) *st = plan.st;
		if (!plan.ok) { out = b; return false; }
		for (size_t v = 0; v < out.viewports.size(); v++) {
			const std::vector<Model> &was = a.viewports[v].models;
			std::vector<Model> &now = out.viewports[v].models;
			for (size_t j = 0; j < now.size() && j < plan.from[v].size(); j++) {
				const int i = plan.from[v][j];
				if (i < 0) continue;
				const Model &p = was[size_t(i)];
				Model &m = now[j];
				if (plan.verts[v][j]) blend_vertices(was[size_t(plan.verts[v][j] - 1)], m, t);
				for (int k = 0; k < 16; k++) m.matrix.m[k] = p.matrix.m[k] + (m.matrix.m[k] - p.matrix.m[k]) * t;
				m.alpha = p.alpha + (m.alpha - p.alpha) * t;
			}
		}
		apply_flips(b, t, out);
		return true;
	}
	Plan np;
	np.a = a.id;
	np.b = b.id;
	InterpStats local;
	if (!st) st = &local;
	*st = InterpStats();
	size_t total = 0, matched = 0;
	for (size_t v = 0; v < out.viewports.size(); v++) {
		np.from.emplace_back(out.viewports[v].models.size(), -1);
		np.verts.emplace_back(out.viewports[v].models.size(), 0);
		np.paired.emplace_back(out.viewports[v].models.size(), -1);
		const std::vector<Model> &was = a.viewports[v].models;
		std::vector<Model> &now = out.viewports[v].models;
		std::vector<char> taken(was.size(), 0);
		std::vector<int> pair(now.size(), -1);
		std::unordered_map<uint32_t, int> n_was, n_now;
		for (auto &m : was) n_was[m.addr]++;
		for (auto &m : now) n_now[m.addr]++;
		Mat4 camera;
		const bool have_camera = camera_move(a.viewports[v], b.viewports[v], camera);
		// how far a's model is from b's: from where the camera's move puts it, if known
		auto apart2 = [&](const Model &p, const Model &m) { return have_camera ? predicted2(camera, p, m) : moved2(p, m); };
		// The nearest of a's models with the same key not yet paired (several share a model's address: a stage's
		// palms, the fighters' limbs; the culling nodes above them are rebuilt elsewhere most frames, so they don't
		// tell them apart: in order, a model would be paired with its neighbour and drawn halfway to it)
		// (the nearest pairs of all of a key's first: taken in the drawing order, one could take another's nearest, the
		// two small flags on a tower each drawn halfway to the other)
		std::vector<char> crowded(now.size(), 0);   // (paired among several of its key)
		// An address pair more than 0.5% of its distance from where it would be (the camera's move, or none known: where
		// it was), when another model of its shape is 4 times nearer: left for the shape pass. Animations step through the same
		// models in ROM: a tower's two small flags, one this frame in the model the other had in the last, and a flock
		// of birds on Pai's rooftop sharing theirs; paired by address, each was drawn halfway to another. (Only with
		// another much nearer: a fighter's limb moves far on its own.)
		// (each model's shape once, and a's models by shape: looked through for each of b's models far from where it
		// would be, which on a big camera move is most of them, every one of a's models with its meshes counted made
		// a picture 3-4 ms)
		std::vector<uint64_t> was_shape(was.size()), now_shape(now.size());
		for (size_t i = 0; i < was.size(); i++) was_shape[i] = shape_of(was[i]);
		for (size_t j = 0; j < now.size(); j++) now_shape[j] = shape_of(now[j]);
		std::unordered_map<uint64_t, std::vector<int>> was_by_shape;
		for (size_t i = 0; i < was.size(); i++) was_by_shape[was_shape[i]].push_back(int(i));
		auto elsewhere = [&](int i, int j) {
			const Model &m = now[size_t(j)];
			const float own = apart2(was[size_t(i)], m);
			if (own <= 2.5e-5f * dist2(m) + 1e-2f) return false;
			auto it = was_by_shape.find(now_shape[size_t(j)]);
			if (it == was_by_shape.end() || it->second.size() > 32) return false;   // (a swarm of hundreds: each looked through all, a ms a picture)
			for (int k : it->second)
				if (!taken[size_t(k)] && k != i && apart2(was[size_t(k)], m) * 16 < own) return true;
			return false;
		};
		auto pair_by = [&](auto key, bool by_addr) {
			std::unordered_map<uint64_t, std::vector<int>> by, by_now;
			for (size_t i = 0; i < was.size(); i++)
				if (!taken[i]) by[key(was[i])].push_back(int(i));
			for (size_t j = 0; j < now.size(); j++)
				if (pair[j] < 0) by_now[key(now[j])].push_back(int(j));
			for (auto &[k, js] : by_now) {
				auto it = by.find(k);
				if (it == by.end()) continue;
				const std::vector<int> &is = it->second;
				if (is.size() == 1 && js.size() == 1) {
					if (elsewhere(is[0], js[0])) continue;
					pair[size_t(js[0])] = is[0];
					taken[size_t(is[0])] = 1;
					continue;
				}
				// (by place and by the way each points: a fighter's trouser segments share an address and nearly their
				// place, at the hip; by place alone one was paired with its neighbour, pointing another way, and drawn
				// halfway to it, the legs in pieces for a picture. Pointing counts as much as a tenth of the distance.)
				// A big group (a swarm of dust or sand, hundreds of one shape): each of b's, in turn, with the nearest of a's
				// not yet taken. Every pair sorted was the square of them, 5-9 ms a picture on a Steam Deck in Wolf's
				// desert; a dust grain paired with its neighbour's last place isn't seen.
				if (is.size() * js.size() > 1024) {
					for (int j : js) {
						int best = -1;
						float best_d = 0;
						for (int i : is) {
							if (taken[size_t(i)]) continue;
							const float d = apart2(was[size_t(i)], now[size_t(j)]);
							if (best < 0 || d < best_d) { best = i; best_d = d; }
						}
						if (best >= 0 && !(by_addr && elsewhere(best, j))) { pair[size_t(j)] = best; taken[size_t(best)] = 1; crowded[size_t(j)] = 1; }
					}
					continue;
				}
				std::vector<std::tuple<float, int, int>> all;
				for (int j : js)
					for (int i : is) {
						const Model &p = was[size_t(i)], &m = now[size_t(j)];
						all.push_back({ apart2(p, m) + turned2(have_camera ? &camera : nullptr, p, m) * 0.01f * dist2(m), i, j });
					}
				std::sort(all.begin(), all.end());
				for (auto &[d, i, j] : all)
					if (!taken[size_t(i)] && pair[size_t(j)] < 0 && !(by_addr && elsewhere(i, j))) { pair[size_t(j)] = i; taken[size_t(i)] = 1; crowded[size_t(j)] = 1; }
				static const bool pair_log = getenv("VF3_PAIR_LOG") != nullptr;
				if (pair_log && is.size() <= 6) {   // (tests)
					fprintf(stderr, "pair group cam %d:", have_camera);
					for (int j : js) {
						const Model &mm = now[size_t(j)];
						fprintf(stderr, " [now %08x (%.0f %.0f %.0f) <- %d", mm.addr, mm.matrix.m[12], mm.matrix.m[13], mm.matrix.m[14], pair[size_t(j)]);
						if (pair[size_t(j)] >= 0) { const Model &pp = was[size_t(pair[size_t(j)])]; fprintf(stderr, " %08x (%.0f %.0f %.0f) d %.1f", pp.addr, pp.matrix.m[12], pp.matrix.m[13], pp.matrix.m[14], std::sqrt(apart2(pp, mm))); }
						fprintf(stderr, "]");
					}
					fprintf(stderr, "\n");
				}
			}
		};
		pair_by([](const Model &m) { return uint64_t(m.addr); }, true);
		// then by shape: an animation's next model (the birds' wings, a splash: the game steps through models in
		// ROM), a model the game builds each frame (in polygon RAM, wherever there was room)
		pair_by([&](const Model &m) { return (&m >= now.data() && &m < now.data() + now.size()) ? now_shape[size_t(&m - now.data())] : was_shape[size_t(&m - was.data())]; }, false);
		// then by place: another model where one was (a detail level for another, the same nodes over it)
		for (size_t j = 0; j < now.size(); j++) {
			if (pair[j] >= 0) continue;
			float d0 = 0;
			for (int r = 0; r < 3; r++) d0 += now[j].matrix.m[12 + r] * now[j].matrix.m[12 + r];
			int best = -1;
			float best_d = 1e-6f * d0 + 1e-4f;
			for (size_t i = 0; i < was.size(); i++)
				if (!taken[i] && moved2(was[i], now[j]) < best_d) { best = int(i); best_d = moved2(was[i], now[j]); }
			if (best >= 0) { pair[j] = best; taken[best] = 1; }
		}
		if (const char *pn = getenv("VF3_PAIR_NEAR")) {   // (tests: "x,y,r": the models whose place is near (x, y) on screen, and their pairs)
			float tx = 0, ty = 0, tr = 0;
			sscanf(pn, "%f,%f,%f", &tx, &ty, &tr);
			const Viewport &vp = b.viewports[v];
			auto proj = [&](const Model &mm, float &sx, float &sy) {
				const float z = -mm.matrix.m[14];
				if (z <= 0) return false;
				sx = vp.vp_x + vp.vp_w * (mm.matrix.m[12] / z - vp.left) / (vp.right - vp.left);
				sy = vp.vp_y + vp.vp_h * (vp.top - mm.matrix.m[13] / z) / (vp.top - vp.bottom);
				return true;
			};
			for (size_t j = 0; j < now.size(); j++) {
				float sx, sy;
				if (!proj(now[j], sx, sy) || std::fabs(sx - tx) > tr || std::fabs(sy - ty) > tr) continue;
				fprintf(stderr, "near vp %zu now %zu %08x at %.0f,%.0f", v, j, now[j].addr, sx, sy);
				if (pair[j] >= 0) {
					float px, py;
					proj(was[size_t(pair[j])], px, py);
					fprintf(stderr, " <- %d %08x at %.0f,%.0f crowded %d", pair[j], was[size_t(pair[j])].addr, px, py, int(crowded[j]));
				}
				fprintf(stderr, "\n");
				if (pair[j] >= 0 && getenv("VF3_PAIR_MAT")) {
					const Model &pp = was[size_t(pair[j])];
					for (int k = 0; k < 16; k++) fprintf(stderr, " %.2f/%.2f", pp.matrix.m[k], now[j].matrix.m[k]);
					fprintf(stderr, " meshes %zu/%zu same %d\n", pp.meshes ? pp.meshes->size() : 0, now[j].meshes ? now[j].meshes->size() : 0, int(pp.meshes == now[j].meshes));
				}
			}
			for (size_t i = 0; i < was.size(); i++) {
				float sx, sy;
				if (!proj(was[i], sx, sy) || std::fabs(sx - tx) > tr || std::fabs(sy - ty) > tr) continue;
				fprintf(stderr, "near vp %zu was %zu %08x at %.0f,%.0f taken %d\n", v, i, was[i].addr, sx, sy, int(taken[i]));
			}
		}
		// too far in a frame: shown where it is now (0 blended, 1 turned, 2 moved, 3 another of several)
		std::vector<uint8_t> snap(now.size(), 0);
		for (size_t j = 0; j < now.size(); j++) {
			if (pair[j] < 0) continue;
			const Model &m = now[j], &p = was[size_t(pair[j])];
			float rot = 0, dist = 0;
			for (int c = 0; c < 3; c++)
				for (int r = 0; r < 3; r++) rot = std::max(rot, std::fabs(p.matrix.m[c * 4 + r] - m.matrix.m[c * 4 + r]));
			for (int r = 0; r < 3; r++) dist += m.matrix.m[12 + r] * m.matrix.m[12 + r];
			const float move = moved2(p, m);
			// One of several sharing an address (a stage's flags) far from where the camera's move puts the one it
			// was paired with (more than 15% of its distance): another of them, not blended (it was drawn halfway
			// between the two, a flag in the sky for a picture)
			const bool wrong = (n_was[p.addr] > 1 || n_now[m.addr] > 1 || crowded[j]) && apart2(p, m) > 0.0225f * dist + 1.0f;
			static const bool no_crowded = getenv("VF3_NO_CROWDED") != nullptr;   // (tests: crowded pairs not blended)
			snap[j] = rot > 0.75f ? 1 : move > 0.25f * dist + 1.0f ? 2 : wrong || (no_crowded && crowded[j]) ? 3 : 0;
		}
		for (size_t j = 0; j < now.size(); j++) {
			Model &m = now[j];
			total++;
			np.paired[v][j] = pair[j];
			if (pair[j] < 0) { if (st) st->no_key++; continue; }
			const Model &p = was[size_t(pair[j])];
			if (snap[j]) {
				if (st) (snap[j] == 1 ? st->turned : st->moved)++;
				continue;
			}
			matched++;
			np.from[v][j] = pair[j];
			int matrix_from = pair[j];   // (the partner its vertices are blended from, when another: its matrix too)
			// A model the game builds each frame (the fighters' joints, between their limbs): its vertices blended
			// too, when it is the same model (its meshes made the same way, with the same textures: their texture
			// coordinates may change, a shiny surface's are made each frame), or it stays where it is in b while the
			// limbs move between. Another model built in the same place (one face for another) isn't blended into it.
			if (m.meshes && p.meshes && m.meshes != p.meshes && m.meshes->size() == p.meshes->size()) {
				// The same model made the same way (its meshes with the same textures), and how far its vertices are
				// from b's in the view (the farthest one, squared; size: b's across)
				auto alike = [&](const Model &q) {
					if (!q.meshes || q.meshes->size() != m.meshes->size()) return false;
					for (size_t k = 0; k < m.meshes->size(); k++) {
						const Mesh &x = (*m.meshes)[k], &y = (*q.meshes)[k];
						if (!(x.verts.size() == y.verts.size() && x.key == y.key && x.x == y.x && x.y == y.y && x.page == y.page &&
						      x.width == y.width && x.height == y.height && x.format == y.format)) return false;
					}
					return true;
				};
				float size = 0;
				auto apart = [&](const Model &q, bool &uv_same) {
					float most = 0, lo[3] = { 1e30f, 1e30f, 1e30f }, hi[3] = { -1e30f, -1e30f, -1e30f };
					uv_same = true;
					for (size_t k = 0; k < m.meshes->size(); k++) {
						const std::vector<Vertex> &x = (*m.meshes)[k].verts, &y = (*q.meshes)[k].verts;
						for (size_t i = 0; i < x.size(); i++) {
							uv_same &= x[i].uv[0] == y[i].uv[0] && x[i].uv[1] == y[i].uv[1];
							float d = 0;
							for (int r = 0; r < 3; r++) {
								const float *u = y[i].pos, *w = x[i].pos;
								const float e = q.matrix.m[r] * u[0] + q.matrix.m[4 + r] * u[1] + q.matrix.m[8 + r] * u[2] + q.matrix.m[12 + r];
								const float f = m.matrix.m[r] * w[0] + m.matrix.m[4 + r] * w[1] + m.matrix.m[8 + r] * w[2] + m.matrix.m[12 + r];
								d += (e - f) * (e - f);
								lo[r] = std::min(lo[r], f);
								hi[r] = std::max(hi[r], f);
							}
							most = std::max(most, d);
						}
					}
					size = 0;
					for (int r = 0; r < 3; r++) size += (hi[r] - lo[r]) * (hi[r] - lo[r]);
					return most;
				};
				int from = -1;
				bool uv_same = false;
				float best = 0;
				if (alike(p)) { from = pair[j]; best = apart(p, uv_same); }
				const bool by_place = from >= 0;
				// Another of a's models of its shape, made the same way, with its vertices 4 times nearer: the one it is.
				// The game makes the fighters' joints each frame wherever there is room, and both knees use the same
				// texture: the left knee made where the right one was, paired by its place, was blended into the other
				// (the same texture coordinates passed it), Lau's trousers in pieces for a picture as he changed stance.
				// (only when its place's partner is a quarter of its size or more away, and among a few: the knees are 2
				// to 4 of a shape; a swarm of sand or splashes is dozens, and looking through them all for each of them
				// took a picture 24 ms on a Steam Deck)
				if (auto it = was_by_shape.find(now_shape[j]); it != was_by_shape.end() && it->second.size() <= 8 && (from < 0 || best * 16 >= size))
					for (int k : it->second) {
						const Model &q = was[size_t(k)];
						if (k == pair[j] || !q.meshes || q.meshes == m.meshes || !alike(q)) continue;
						bool us = false;
						const float d = apart(q, us);
						if (from < 0 || (by_place ? d * 16 < best : d < best)) { from = k; best = d; uv_same = us; }   // (with no place partner: the nearest)
					}
				// The same texture coordinates, or (a shiny surface's are made each frame) every vertex moved, its move less
				// the model's average move, less than a quarter of the mesh's size: a joint bends and goes with the limbs
				// (Lau's shiny trousers moved 300-765 against sizes of 380-785 as he switched stance, nearly all of it
				// together: by the whole move they weren't blended while the rigid pieces between them were, his legs in
				// pieces for a picture), where one face put in another's place (the select screen's portraits, a face
				// shutting its eyes) moves its vertices every which way, the average little.
				auto bent = [&](const Model &q) {
					double mean[3] = { 0, 0, 0 };
					size_t n = 0;
					auto moved = [&](size_t k, size_t i, float *d) {
						const float *u = (*q.meshes)[k].verts[i].pos, *w = (*m.meshes)[k].verts[i].pos;
						for (int r = 0; r < 3; r++)
							d[r] = (m.matrix.m[r] * w[0] + m.matrix.m[4 + r] * w[1] + m.matrix.m[8 + r] * w[2] + m.matrix.m[12 + r]) -
							       (q.matrix.m[r] * u[0] + q.matrix.m[4 + r] * u[1] + q.matrix.m[8 + r] * u[2] + q.matrix.m[12 + r]);
					};
					for (size_t k = 0; k < m.meshes->size(); k++)
						for (size_t i = 0; i < (*m.meshes)[k].verts.size(); i++) {
							float d[3];
							moved(k, i, d);
							for (int r = 0; r < 3; r++) mean[r] += d[r];
							n++;
						}
					if (n) for (double &x : mean) x /= double(n);
					float most = 0;
					for (size_t k = 0; k < m.meshes->size(); k++)
						for (size_t i = 0; i < (*m.meshes)[k].verts.size(); i++) {
							float d[3], e = 0;
							moved(k, i, d);
							for (int r = 0; r < 3; r++) e += (d[r] - float(mean[r])) * (d[r] - float(mean[r]));
							most = std::max(most, e);
						}
					return most;
				};
				const bool same = from >= 0 && ((uv_same && by_place) || best < size / 16 || bent(was[size_t(from)]) < size / 16);   // (one found by its vertices: near them)
				if (st) (same ? st->blended : st->not_blended)++;
				if (same) {
					np.verts[v][j] = from + 1;
					blend_vertices(was[size_t(from)], m, t);
					// (its vertices are where they are in the view from that one's matrix: the matrix blended from it
					// too, or the joint was put off by the two partners' difference)
					matrix_from = from;
					np.from[v][j] = from;
				}
			}
			const Model &q = was[size_t(matrix_from)];
			for (int i = 0; i < 16; i++) m.matrix.m[i] = q.matrix.m[i] + (m.matrix.m[i] - q.matrix.m[i]) * t;
			m.alpha = q.alpha + (m.alpha - q.alpha) * t;
		}
	}
	st->total = total;
	st->matched = matched;
	np.st = *st;
	np.ok = !(total && matched * 2 < total);
	// the flipbooks: a step from a to b remembered, or the one remembered for a's model carried on (when a's frame
	// was the last plan's b: one frame on)
	{
		const bool chain = plan.b == a.id && a.id && plan.flips.size() == a.viewports.size();
		np.flips.resize(np.paired.size());
		for (size_t v = 0; v < np.paired.size(); v++) {
			np.flips[v].resize(np.paired[v].size());
			for (size_t j = 0; j < np.paired[v].size(); j++) {
				const int i = np.paired[v][j];
				if (i < 0) continue;
				const Flip *pr = chain && size_t(i) < plan.flips[v].size() && plan.flips[v][size_t(i)].age >= 0 ? &plan.flips[v][size_t(i)] : nullptr;
				Flip &f = np.flips[v][j];
				if (flip_step(a.viewports[v].models[size_t(i)], b.viewports[v].models[j])) {
					f.from = a.viewports[v].models[size_t(i)].meshes;
					f.age = 0;
					f.period = pr && pr->age + 1 >= 2 && pr->age + 1 <= 8 ? pr->age + 1 : 0;
				} else if (pr && pr->age + 1 <= 8) {
					f = *pr;
					f.age++;
				}
			}
		}
	}
	if (a.id && b.id) plan = std::move(np);
	if (total && matched * 2 < total) {
		out = b;
		return false;
	}
	if (a.id && b.id) apply_flips(b, t, out);
	return true;
}

} // namespace video
