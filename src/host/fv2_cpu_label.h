// TRAINING: the second side is CPU, and the select screen says so.
//
// Frederic, the 2026-10-25: "le P2 qui devrait s'appeler CPU". In training the two fighters are chosen by
// one pair of hands and the second one is a dummy, so the game's own "2P" under its cursor is a lie -- a
// small one, but on the one screen where the player is deciding who he is fighting.
//
// IT IS WRITTEN WHERE THE GAME WRITES ITS OWN, and that place moves. Everything below was measured on the
// 2026-10-27 out of the game's own picture and its own memory, never guessed:
//
//   the cursor      0x00580E8D, found by watching a range of RAM while a script pressed right (VF3_RAM_WATCH
//                   in main.cpp). It steps by FOUR a column: 0x28, 0x2C, 0x30 ... and the strip holds eleven
//                   portraits, so the column is (v / 4) mod 11. Sixteen positions were photographed and the
//                   law holds for every one of them;
//   where it lands  the label's own pixels, colour (0, 127, 255) exactly, span 26 columns and sit at rows
//                   363 to 375 of the game's 4:3 picture. Their middle is 35.5 for column 0 and 459.5 for
//                   column 10, so 42.4 pixels a column, which is the strip's own step;
//   what is behind  the dark band under the portraits, and nothing else -- no portrait, no artwork. That is
//                   what makes this honest rather than a patch over a picture: the box can be filled with
//                   the band's own colour, sampled beside the label, and the band is a gradient (57, 49,
//                   115) at one end and (65, 57, 115) at the other, so it is sampled and not written down.
//
// The drawing is this port's own glyph sheet in the game's own azure, which is the same choice the console's
// other surfaces make: the Dreamcast's font over the arcade's screen.
#pragma once
#include <SDL3/SDL.h>
#include "../machine/model3.h"
#include "fv2_texte.h"

struct fv2_cpu_label
{
	// the game's 4:3 picture, which every figure below is in
	static constexpr float PIC_W = 496.0f;
	// the label's middle, by column, and the strip's eleven places
	static constexpr float MID_0 = 35.5f, MID_STEP = 42.4f;
	static constexpr int COLS = 11;
	// the glyphs' own rows and half-width, with a pixel of margin each way so nothing of "2P" survives
	static constexpr float TOP = 362.0f, BOTTOM = 376.0f, HALF = 15.0f;
	// where the band's colour is read, beside the label and well clear of it
	static constexpr float SAMPLE_DX = 28.0f, SAMPLE_Y = 369.0f;
	static constexpr uint32_t CURSOR_2P = 0x00580E8D;

	// Over the picture the game made, and under the console's own surfaces. `mid` is the game's 4:3 screen in
	// the window's pixels; `below` is the layer the label is drawn in, which is where the band's colour is.
	static void render(SDL_Renderer *ren, const SDL_FRect &mid, m3::Model3 &b,
					   const uint32_t *below, int below_w, int below_h, fv2_texte *font)
	{
		if (!font || !font->ready || !below) return;
		const int v = b.fv2_option(int(CURSOR_2P), 1);
		if (v < 0) return;
		const int col = (v / 4) % COLS;
		// IT SAYS WHERE IT WENT, once a column: a label drawn at the wrong place looks exactly like a label
		// drawn at the right one to anything but an eye, and this is the kind of thing a hidden run cannot
		// photograph (the window is not shown, so nothing can be read back from it).
		static int said = -1;
		if (col != said) {
			said = col;
			fprintf(stderr, "training: CPU written over 2P, cursor %02X, column %d of %d, middle %.1f of %.0f\n",
				v, col, COLS, MID_0 + MID_STEP * float(col), PIC_W);
		}
		const float mx = MID_0 + MID_STEP * float(col);
		const float k = mid.w / PIC_W;   // the picture's pixels to the window's

		// The band's own colour, read beside the label: it is a gradient, so a figure written down here
		// would be right in one place and wrong in another.
		const int sx = int(mx - SAMPLE_DX), sy = int(SAMPLE_Y);
		uint8_t r = 57, g = 49, bl = 115;   // (what it reads at the left of the strip, if the sample cannot)
		if (sx >= 0 && sx < below_w && sy >= 0 && sy < below_h) {
			const uint32_t px = below[size_t(sy) * size_t(below_w) + size_t(sx)];
			if (px >> 24) { r = uint8_t(px); g = uint8_t(px >> 8); bl = uint8_t(px >> 16); }
		}
		SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);
		SDL_SetRenderDrawColor(ren, r, g, bl, 255);
		const SDL_FRect box = { mid.x + (mx - HALF) * k, mid.y + TOP * k, 2.0f * HALF * k, (BOTTOM - TOP) * k };
		SDL_RenderFillRect(ren, &box);
		SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);

		// and the word in the game's own azure, the same height as the glyphs it stands in for
		const SDL_Color ink = { 0, 127, 255, 255 };
		const float cap = (BOTTOM - TOP - 2.0f) * k;
		const float h = cap / fv2_texte::CAP;
		font->draw(ren, "CPU", mid.x + mx * k - font->width("CPU", h) * 0.5f,
				   mid.y + TOP * k + 1.0f * k - fv2_texte::CAP_TOP * h, h, ink);
	}
};
