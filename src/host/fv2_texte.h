// Fighting Vipers 2: the console menu's own text, drawn from the owner's glyph sheet.
//
// WHY THIS EXISTS. The console's surfaces are pictures -- title_list.png is seven painted bands, and
// fv2_mode_list.h slides each one into place. That works because the seven lines never change. An OPTIONS page
// cannot be a picture: thirteen settings with values that change under the player's hands are text, and this
// port had no text outside RmlUi, which is the Escape menu's -- and the lesson paid twice is that the Escape
// menu is not the console's surface.
//
// Frédéric settled it on the 2026-10-22 by putting police.png among the menu's images rather than among the
// tiles named by their hash. So the console gets its own text, from its own sheet, drawn exactly as the list of
// modes is drawn: straight over the picture the game made, nothing through game_menu.
//
// WHAT THE SHEET IS, measured by tools/fv2/mesurer_police.py and not guessed:
//
//   - 1024 x 384, RGBA, white glyphs on transparent -- so SDL's colour mod tints them to anything;
//   - 16 columns by 6 rows of 64 x 64 cells, ASCII 0x20 to 0x7F in order, one code a cell;
//   - exactly two cells have no ink, 0x20 and 0x7F, which is what confirms the mapping: if the sheet started
//     anywhere else the holes would not fall on the space and the DEL;
//   - the glyphs are CENTRED in their cells, not set flush left. That is the one fact that shapes this file:
//     a proportional line cannot be laid out from the cells, it needs each glyph's own ink box, which is the
//     table below;
//   - the baseline is y = 46 in the cell, the capitals start at y = 13 (so 34 tall), the descenders of g j p q y
//     reach 51 and the Q's tail 49.
//
// Vertically there is nothing to compute: every cell is drawn whole, 64 rows of sheet to one line's height, so
// the glyphs keep the alignment the sheet already gives them. Only the horizontal advance is ours.
//
// THE TWO NUMBERS THAT ARE NOT MEASURED, and they are the only two. The sheet says how wide each glyph's ink is
// but not how far apart they sit, because the author drew a proportional face into fixed cells and the spacing
// died with the cells. TRACK is the gap left between two ink boxes and SPACE is the advance of the space, which
// has no ink to measure. Both are held in cells, so they follow the drawn size, and both were set by eye on the
// page they serve -- said plainly here rather than dressed up as a measurement.
#pragma once
#include <SDL3/SDL.h>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>
#include "png.h"

struct fv2_texte
{
	static constexpr int IMG_W = 1024, IMG_H = 384;
	static constexpr int COLS = 16, ROWS = 6, CELL = 64;
	static constexpr int FIRST = 0x20, LAST = 0x7F;

	// tools/fv2/mesurer_police.py, run on assets/menu-console/police.png:
	// x du bord gauche de l'encre dans la case, puis sa largeur ; 0,0 = pas d'encre
	static constexpr uint8_t INK[96][2] = {
		{ 0, 0}, {28, 7}, {25,13}, {19,25}, {19,24}, {15,34}, {16,32}, {29, 6},   // 0x20   !"#$%&'
		{26,12}, {26,12}, {21,21}, {20,23}, {28, 7}, {25,13}, {28, 7}, {19,24},   // 0x28  ()*+,-./
		{19,25}, {21,24}, {19,24}, {19,24}, {19,25}, {19,25}, {19,25}, {20,23},   // 0x30  01234567
		{19,25}, {19,25}, {28, 7}, {28, 7}, {21,20}, {20,23}, {21,21}, {20,22},   // 0x38  89:;<=>?
		{15,33}, {18,28}, {21,24}, {19,25}, {20,26}, {22,21}, {23,20}, {18,26},   // 0x40  @ABCDEFG
		{20,24}, {29, 6}, {23,15}, {21,25}, {23,20}, {16,31}, {20,24}, {18,27},   // 0x48  HIJKLMNO
		{21,24}, {18,27}, {20,25}, {19,25}, {20,23}, {21,22}, {19,25}, {11,41},   // 0x50  PQRSTUVW
		{19,26}, {18,27}, {19,24}, {28, 8}, {20,24}, {27, 9}, {20,23}, {20,23},   // 0x58  XYZ[\]^_
		{24,12}, {20,22}, {22,21}, {22,20}, {20,21}, {21,22}, {24,14}, {21,23},   // 0x60  `abcdefg
		{22,19}, {29, 6}, {25,12}, {22,22}, {29, 6}, {15,34}, {22,19}, {21,22},   // 0x68  hijklmno
		{22,21}, {20,21}, {27,12}, {21,20}, {24,14}, {22,19}, {21,21}, {16,32},   // 0x70  pqrstuvw
		{21,21}, {21,20}, {23,18}, {25,13}, {28, 7}, {26,12}, {20,23}, { 0, 0},   // 0x78  xyz{|}~
	};

	// The two set by eye (above), in cells so they follow the drawn size
	static constexpr float TRACK = 7.0f / CELL;    // the gap between two ink boxes
	static constexpr float SPACE = 16.0f / CELL;   // the space's own advance, which has no ink

	// Where the baseline and the capitals sit in a cell, for placing a line against something else
	static constexpr float BASELINE = 46.0f / CELL, CAP_TOP = 13.0f / CELL;

	bool ready = false, tried = false;
	SDL_Texture *tex = nullptr;
	std::string path;

	// Reads the sheet once. False (and a line in the log) if it isn't there, and then every draw is a no-op:
	// a page that cannot write its labels must not draw half of itself.
	bool load(SDL_Renderer *ren, const std::string &assets, FILE *log)
	{
		if (tried) return ready;
		tried = true;
		path = assets + "/menu-console/police.png";
		std::vector<uint32_t> px;
		int w = 0, h = 0;
		std::string why;
		if (!read_png(path.c_str(), px, w, h, why)) {
			if (log) fprintf(log, "the console's text: %s %s -- the pages that need it stay shut\n", path.c_str(), why.c_str());
			return false;
		}
		if (w != IMG_W || h != IMG_H) {
			if (log) fprintf(log, "the console's text: %s is %dx%d, and the glyph boxes were measured on %dx%d -- the pages that need it stay shut\n",
							 path.c_str(), w, h, IMG_W, IMG_H);
			return false;
		}
		// read_png gives red in the low byte: that is ABGR8888 as SDL names it
		tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STATIC, w, h);
		if (!tex) {
			if (log) fprintf(log, "the console's text: no texture for %s (%s)\n", path.c_str(), SDL_GetError());
			return false;
		}
		SDL_UpdateTexture(tex, nullptr, px.data(), w * 4);
		SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
		SDL_SetTextureScaleMode(tex, SDL_SCALEMODE_LINEAR);
		ready = true;
		if (log) fprintf(log, "the console's text: %s, %dx%d, %d glyphs\n", path.c_str(), w, h, COLS * ROWS);
		return true;
	}

	// The renderer let go of its textures (a device reset): made again from the file.
	void textures_lost(SDL_Renderer *ren, const std::string &assets)
	{
		if (tex) SDL_DestroyTexture(tex);
		tex = nullptr;
		ready = false;
		tried = false;
		load(ren, assets, nullptr);
	}

	// Anything the sheet has no cell for is drawn as a space rather than dropped: a line that loses a character
	// silently is worse to look at than one with a hole, and the hole says where to look.
	static int cell_of(unsigned char c) { return c >= FIRST && c <= LAST ? int(c) - FIRST : 0; }

	// How wide a line comes out, at a line height of `h` pixels (that is, `h` for the whole 64-row cell).
	float width(const char *s, float h) const
	{
		float x = 0;
		bool first = true;
		for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
			const int i = cell_of(*p);
			if (!first) x += TRACK * h;
			first = false;
			x += (INK[i][1] ? INK[i][1] / float(CELL) : SPACE) * h;
		}
		return x;
	}

	// One line, its left edge at x and its cell's top at y, the cell drawn h tall. The sheet is white, so the
	// colour is a tint; alpha rides on top of it.
	void draw(SDL_Renderer *ren, const char *s, float x, float y, float h, SDL_Color col, uint8_t alpha = 255)
	{
		if (!ready || !tex) return;
		SDL_SetTextureColorMod(tex, col.r, col.g, col.b);
		SDL_SetTextureAlphaMod(tex, alpha);
		bool first = true;
		for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
			const int i = cell_of(*p);
			if (!first) x += TRACK * h;
			first = false;
			if (!INK[i][1]) { x += SPACE * h; continue; }
			const SDL_FRect src = { float((i % COLS) * CELL + INK[i][0]), float((i / COLS) * CELL),
									float(INK[i][1]), float(CELL) };
			const float w = INK[i][1] / float(CELL) * h;
			const SDL_FRect dst = { x, y, w, h };
			SDL_RenderTexture(ren, tex, &src, &dst);
			x += w;
		}
		SDL_SetTextureColorMod(tex, 255, 255, 255);
		SDL_SetTextureAlphaMod(tex, 255);
	}

	// The same, ending at x instead of starting there -- what a column of values needs to line up on its right.
	void draw_right(SDL_Renderer *ren, const char *s, float x, float y, float h, SDL_Color col, uint8_t alpha = 255)
	{
		draw(ren, s, x - width(s, h), y, h, col, alpha);
	}
};
