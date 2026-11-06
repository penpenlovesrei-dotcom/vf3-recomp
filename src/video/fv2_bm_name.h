// Fighting Vipers 2: B.M.'s name on the select screen, which Sega left blank.
//
// The names are tiles, not 3D textures, and B.M.'s cell is empty on purpose: at 0x0006045C the game tests the
// cursor against 13 and skips the name's own routine, because the lookup would hand him MAHLER's
// (docs/fv2-select-names.md). Between a wrong label and no label Sega chose none.
//
// So the port draws his, from two pictures read beside the program -- one for each of his two colours, the
// colour being the game's own toggle on the select screen (one press of Up gives the other). Drawn into the
// layers above the 3D, where the name's own strip is, exactly as Dural's cell is drawn (dural_cell.h): only
// the picture, never the board, so a save state or a rollback cannot differ by it.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace m3 { class Model3; }

namespace video {

struct BMName
{
	int w = 0, h = 0;              // the pictures' size (they must agree)
	std::vector<uint32_t> px[2];   // colour 0 and colour 1, 0xAABBGGRR, alpha 0 where see-through
	int state = 0;                 // 0 not read yet, 1 read, -1 nothing to draw
};

// Reads <assets>/fv2/BM_color_1.png and BM_color_2.png once. Missing files are not an error: the name then
// stays as Sega left it, and the reason is said once.
void bm_name_load(const std::string &assets, BMName &art);

// above: the layers above the 3D as TileGen::aligned gives them (W x H at scale 1, W*scale x H*scale for the
// HD 2D plane). Draws his name for each player whose cursor is on him, in the colour that player's toggle is
// on. False if this is not Fighting Vipers 2, not the select screen, or nobody is on him.
bool bm_name(const m3::Model3 &board, std::vector<uint32_t> &above, int scale, const BMName &art);

} // namespace video
