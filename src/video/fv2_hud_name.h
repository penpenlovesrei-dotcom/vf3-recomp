// Fighting Vipers 2: the name on the energy bar, for the costumes the game has no name for.
//
// The fight's HUD draws each fighter's name as a picture of tiles beside his bar, and it draws ONE per fighter:
// the picture is the fighter's, not the costume's. That is right for everybody the game shipped, and wrong in
// exactly three places this package made:
//
//   B.M. in his second colour   the game draws his gold plate for both of his two, so the red one is never seen
//   Kuhn in blue, Kuhn in black the two this package adds; the game knows only his chrome and his gold
//
// So the port draws over those three, from pictures beside the program and in the texture pack -- the same way
// the select screen's names are drawn (fv2_bm_name.h, fv2_kuhn_name.h), into the layers above the 3D, never
// into the board, so no save state and no rollback can differ by it.
//
// WHERE: Fighting Vipers 2's own HUD, measured and written down in fv2_hud_name.cpp -- rows 7 and 8, ten
// tiles a name, with the round markers between them. Virtua Fighter 3's HUD (hud_names.cpp) is laid out
// differently and borrowing its numbers drew the plate on the round's title card and nothing in the fight.
// The plate is then fitted to the picture the game left standing in that cell, not to the cell.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace m3 { class Model3; }

namespace video {

struct HudPlate
{
	int w = 0, h = 0;
	std::vector<uint32_t> px;      // 0xAABBGGRR, alpha 0 where the HUD shows through
	bool have = false;
};

struct Fv2HudNames
{
	HudPlate bm2;         // B.M.'s second colour, the red plate
	HudPlate kuhn[2];     // the wheel's positions 2 and 3: the blue, then the black
	int state = 0;        // 0 not read yet, 1 something was read, -1 nothing to draw
};

// Read once: B.M.'s from <assets>/fv2/BM_lifebar_color_2.png, Kuhn's from the texture pack beside his skins,
// <pack>/CHARACTERS/Kuhn/{blue,black}/NAME_lifebar_{blue,black}.png. A missing picture is not an error -- that
// costume then keeps the plate the game draws, and the reason is said once.
void fv2_hud_name_load(const std::string &assets, const std::string &pack, Fv2HudNames &art);

// above: the layers above the 3D as TileGen::aligned gives them (W x H). kuhn_cycle: the colour wheel's place,
// 0-3 (Game::kuhn_cycle), since positions 2 and 3 are the port's own and no byte of the game holds them. False
// if nothing was drawn.
bool fv2_hud_name(const m3::Model3 &board, std::vector<uint32_t> &above, int kuhn_cycle, const Fv2HudNames &art);

} // namespace video
