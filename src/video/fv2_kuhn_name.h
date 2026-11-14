// Fighting Vipers 2: KUHN's name on the select screen, in his two extra colours.
//
// His costume's extra colours are textures (src/video/tex3d_hd.h, and the wheel in main.cpp), but his NAME is
// not: the names on that screen are tiles, laid by the game's own routine from its own table
// (docs/fv2-select-names.md in the console branch), and Sega drew exactly one KUHN -- the chrome-lettered one.
// A texture pack cannot reach it, because it is not a texture.
//
// So the port draws the other two over it, the way Dural's cell is drawn (dural_cell.h): into the layers above
// the 3D as they are made, never into the board, so a save state cannot differ by it and nothing of the game's
// own state moves. The game's own name is cleared from its strip first -- the picture has transparent letters
// and the chrome underneath would show through every one of them.
//
// The two pictures live beside the two folders of textures they belong to, so a player who drops his colours in
// has dropped the names in with them:
//
//     <pack>/CHARACTERS/Kuhn/blue/NAME_BIG_blue.png
//     <pack>/CHARACTERS/Kuhn/black/NAME_BIG_black.png
//
// Where they are not, nothing is drawn and Sega's name stays: the colour then shows on the costume alone.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace m3 { class Model3; }

namespace video {

struct KuhnName
{
	int w[2] = { 0, 0 }, h[2] = { 0, 0 };
	std::vector<uint32_t> px[2];   // the blue (wheel 2) and the black (wheel 3), 0xAABBGGRR
	bool have[2] = { false, false };
	int state = 0;                 // 0 not read yet, 1 at least one read, -1 nothing to draw
};

// Reads the two pictures once, from the texture pack's own two folders. Missing files are not an error: the
// name then stays as Sega drew it, and the reason is said once.
void kuhn_name_load(const std::string &pack, KuhnName &art);

// above: the layers above the 3D as TileGen::aligned gives them (W x H). Draws the extra colour's name for each
// player whose cursor is on Kuhn, when the wheel is on one of the two extra colours (cycle 2 or 3). False if
// this is not Fighting Vipers 2, not the select screen, nobody is on him, or the wheel is on Sega's own two.
bool kuhn_name(const m3::Model3 &board, std::vector<uint32_t> &above, int cycle, const KuhnName &art);

} // namespace video
