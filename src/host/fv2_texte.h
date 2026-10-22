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
	static constexpr float TRACK = 5.0f / CELL;    // the gap between two ink boxes
	static constexpr float SPACE = 16.0f / CELL;   // the space's own advance, which has no ink

	// The capitals and the digits all occupy y 16..46 in the cell -- the 35 of them share one box, the Q's
	// tail apart -- so this is the whole of what a line of this page needs to place itself.
	static constexpr float CAP_TOP = 16.0f / CELL, CAP = 31.0f / CELL;

	// THE DREAMCAST'S OWN TWO FACES, measured from a capture of its OPTIONS screen and written up in
	// docs/fv2-dreamcast-menus.md section 3bis. They are two, and this sheet is one of them.
	//
	// Its settings rows LEAN. Deskewing each line and keeping the angle that makes the stems sharpest gives
	// 7.0 to 8.25 degrees, median about 7.5. And the sheet is that face: the weight, as the median run of ink
	// across a row at half height over the cap height, is 0.194 here (the I, 6 px on 31) against the disc's
	// 0.195 (8 on 41). The same face without the slant, so SLANT puts the slant back.
	static constexpr float SLANT = 0.1316f;   // tan(7.5 degrees)
	//
	// Its title does NOT lean, and is a condensed bold this sheet has no weight for: "OPTIONS" is 4.143 cap
	// heights wide at a weight of 0.254, where this sheet is 6.000 and 0.194. Condensing by c and thickening
	// HORIZONTALLY by s cap-heights solves both at once -- a horizontal thickening leaves the height alone --
	//     6.000*c + s = 4.143 ;  0.194*c + s = 0.254
	// giving the two below. Horizontally and not with a square growth: that was tried first and dropped,
	// because it rounds the corners and closes the counters, which the disc has sharp and open. A condensed
	// bold carries its weight on the vertical stems anyway.
	static constexpr float COND = 0.6698f, THICK = 0.1241f;

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
	float width(const char *s, float h) const { return width(s, h, 1.0f, 0.0f); }

	// One glyph as a parallelogram: the top edge slid right by `slant` of the height. SDL has no sheared blit,
	// and a rotation is not a shear, so the four corners go through RenderGeometry. With slant 0 this is the
	// same rectangle SDL_RenderTexture would draw.
	void quad(SDL_Renderer *ren, const SDL_FRect &src, const SDL_FRect &dst, float slant, SDL_FColor c)
	{
		const float u0 = src.x / IMG_W, u1 = (src.x + src.w) / IMG_W;
		const float v0 = src.y / IMG_H, v1 = (src.y + src.h) / IMG_H;
		const float d = slant * dst.h;
		SDL_Vertex v[4] = {
			{ { dst.x + d,         dst.y         }, c, { u0, v0 } },
			{ { dst.x + d + dst.w, dst.y         }, c, { u1, v0 } },
			{ { dst.x + dst.w,     dst.y + dst.h }, c, { u1, v1 } },
			{ { dst.x,             dst.y + dst.h }, c, { u0, v1 } },
		};
		static const int IDX[6] = { 0, 1, 2, 0, 2, 3 };
		SDL_RenderGeometry(ren, tex, v, 4, IDX, 6);
	}

	// One line, its left edge at x and its cell's top at y, the cell drawn h tall. The sheet is white, so the
	// colour is a tint. `slant` leans it (SLANT for the disc's settings rows, 0 upright), `squeeze` narrows it
	// and `thicken` smears it sideways in cap-heights -- the last two are the title's, and both are 0/1 here.
	void draw(SDL_Renderer *ren, const char *s, float x, float y, float h, SDL_Color col, uint8_t alpha = 255,
			  float slant = 0, float squeeze = 1, float thicken = 0)
	{
		if (!ready || !tex) return;
		const SDL_FColor c = { col.r / 255.0f, col.g / 255.0f, col.b / 255.0f, alpha / 255.0f };
		const float smear = thicken * CAP * h;              // in pixels
		const int copies = smear > 0 ? int(smear) + 1 : 1;  // one a pixel, which fills it
		for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
			const int i = cell_of(*p);
			if (p != (const unsigned char *)s) x += TRACK * h * squeeze;
			if (!INK[i][1]) { x += SPACE * h * squeeze; continue; }
			const SDL_FRect src = { float((i % COLS) * CELL + INK[i][0]), float((i / COLS) * CELL),
									float(INK[i][1]), float(CELL) };
			const float w = INK[i][1] / float(CELL) * h * squeeze;
			for (int k = 0; k < copies; k++)
				quad(ren, src, { x + smear * k / float(copies > 1 ? copies - 1 : 1), y, w, h }, slant, c);
			x += w + smear;
		}
	}

	// How wide `draw` comes out with the same arguments.
	float width(const char *s, float h, float squeeze, float thicken) const
	{
		const float smear = thicken * CAP * h;
		float x = 0;
		for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
			const int i = cell_of(*p);
			if (p != (const unsigned char *)s) x += TRACK * h * squeeze;
			x += INK[i][1] ? INK[i][1] / float(CELL) * h * squeeze + smear : SPACE * h * squeeze;
		}
		return x;
	}

	// The disc's title: condensed, thickened sideways, upright, centred on x. `cap` is the height of the
	// capitals, which is what was measured, rather than the cell's.
	void draw_title(SDL_Renderer *ren, const char *s, float x_mid, float cap_top, float cap, SDL_Color col,
					uint8_t alpha = 255)
	{
		const float h = cap / CAP;
		draw(ren, s, x_mid - width(s, h, COND, THICK) * 0.5f, cap_top - CAP_TOP * h, h, col, alpha, 0, COND, THICK);
	}

	// Centred on x -- what the disc does with this page's values, which sit on a column of their own.
	void draw_mid(SDL_Renderer *ren, const char *s, float x_mid, float y, float h, SDL_Color col,
				  uint8_t alpha = 255, float slant = 0)
	{
		draw(ren, s, x_mid - width(s, h) * 0.5f, y, h, col, alpha, slant);
	}
};
