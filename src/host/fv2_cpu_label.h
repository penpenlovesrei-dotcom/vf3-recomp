// TRAINING: the second side is CPU, and the select screen says so.
//
// Frederic, the 2026-10-25: "le P2 qui devrait s'appeler CPU". In training the two fighters are chosen by
// one pair of hands and the second is a dummy, so the game's own "2P" under its cursor is a lie -- a small
// one, but on the one screen where the player is deciding who he is fighting.
//
// WHERE IT IS, AND HOW THAT WAS FOUND. Three readings were wrong before this one, and each is worth a line
// so that nobody pays for them twice:
//
//   the cursor is not the anchor. 0x00580E8D steps by four a column and sixteen photographs of a VERSUS
//   agreed with it -- but in TRAINING, with no hands at all, that byte walked 0x24, 0x28, 0x30, 0x34 while
//   the label never moved from column ten, photographed at three frames. CPU was written beside a 2P that
//   was still there, which is exactly what Frederic saw the 2026-10-28;
//
//   the colour of the finished picture is not the colour of the layer. In a photograph the label is exactly
//   (0, 127, 255); in the buffer it is not, the layer being the hardware's five bits a channel widened and
//   the picture having another step after it -- the band under the portraits reads (57, 49, 115) in the
//   photograph and (49, 41, 106) in the buffer. So the test is a RANGE, not a value;
//
//   and the cursor is in the layer ABOVE the 3D, while the band it sits on is in the one below. Scanning
//   the wrong one found nothing at all, twice.
//
// So: the cursor's own arrow is looked for in the upper layer, over the rows it lives in; its middle is the
// label's middle, measured on photographs (at one cursor position the arrow spans 104..137 and the text
// 107..133; at another, 447..472 for both). The text's own rows are 363 to 375 of the 384. The box covers
// those rows only, and the arrow is left alone -- it is the cursor, and the player needs it.
//
// What is behind the text is the dark band and nothing else: no portrait, no artwork. That is what makes
// covering it honest rather than a patch over a picture. The band is a gradient, so its colour is sampled
// beside the label, in the layer that holds it.
#pragma once
#include <SDL3/SDL.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <string>
#include "fv2_texte.h"

struct fv2_cpu_label
{
	// the arrow's own rows, and the text's, in the game's 4:3 picture
	static constexpr int ARROW_TOP = 348, ARROW_BOTTOM = 362;
	static constexpr int TEXT_TOP = 362, TEXT_BOTTOM = 377;
	static constexpr float HALF = 16.0f;        // half the box, a little wider than the 27 columns "2P" takes
	static constexpr float SAMPLE_DX = 26.0f;   // where the band's colour is read, clear of the text

	// Hardly any red, a middling green, blue at the top of its range: the band fails it on the blue (106 of
	// 255) and the portraits are not in the rows looked at.
	static bool is_ink(uint32_t px)
	{
		if (!(px >> 24)) return false;
		const int r = int(px & 0xFF), g = int((px >> 8) & 0xFF), b = int((px >> 16) & 0xFF);
		return r < 70 && g > 70 && g < 200 && b > 190;
	}

	// Over the picture the game made, and under the console's own surfaces. `mid` is the game's 4:3 screen in
	// the window's pixels; `above` is the layer the cursor is drawn in, `below` the one its band is in. Both
	// are the picture's own width.
	static void render(SDL_Renderer *ren, const SDL_FRect &mid, const uint32_t *above, const uint32_t *below,
					   int w, int h, fv2_texte *font)
	{
		if (!font || !font->ready || !above || !below || w <= 0 || h <= 0) return;
		// THE LONGEST RUN OF A ROW, not the extent of everything azure in the band: taking the extent gave
		// 415..482 where the cursor's own arrow is 447..472 -- something else of that colour sits to its
		// left, and a middle eleven pixels out is a box eleven pixels out, which leaves half of "2P"
		// showing. The arrow is one unbroken run; nothing else in those rows is as long.
		int x0 = 0, x1 = -1;
		for (int y = std::max(0, ARROW_TOP); y < std::min(h, ARROW_BOTTOM); y++) {
			int from = -1;
			for (int x = 0; x <= w; x++) {
				const bool ink = x < w && is_ink(above[size_t(y) * size_t(w) + size_t(x)]);
				if (ink && from < 0) from = x;
				else if (!ink && from >= 0) {
					if (x - 1 - from > x1 - x0) { x0 = from; x1 = x - 1; }
					from = -1;
				}
			}
		}
		if (x1 < 0) return;   // no cursor on this screen: nothing to say and nothing to cover
		const float mx = float(x0 + x1 + 1) * 0.5f;

		// IT SAYS WHERE IT FOUND IT, once a place: a label written at the wrong place looks exactly like one
		// written at the right place to anything but an eye, and a hidden run cannot look at a window.
		static int said = -1;
		if (int(mx) != said) {
			said = int(mx);
			fprintf(stderr, "training: the 2P cursor is at %d..%d of the picture -- CPU written on its middle\n", x0, x1);
		}

		// The band's own colour, read beside the label and on the text's middle row.
		uint8_t r = 49, g = 41, bl = 106;   // (what the buffer reads at the left of the strip, if the sample cannot)
		const int sx = int(mx - SAMPLE_DX), sy = (TEXT_TOP + TEXT_BOTTOM) / 2;
		if (sx >= 0 && sx < w && sy >= 0 && sy < h) {
			const uint32_t px = below[size_t(sy) * size_t(w) + size_t(sx)];
			if (px >> 24) { r = uint8_t(px); g = uint8_t(px >> 8); bl = uint8_t(px >> 16); }
		}
		const float k = mid.w / float(w);
		SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);
		SDL_SetRenderDrawColor(ren, r, g, bl, 255);
		const SDL_FRect box = { mid.x + (mx - HALF) * k, mid.y + float(TEXT_TOP) * k,
								2.0f * HALF * k, float(TEXT_BOTTOM - TEXT_TOP) * k };
		SDL_RenderFillRect(ren, &box);
		SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);

		// and the word in the game's own azure, as tall as the glyphs it stands in for
		const SDL_Color ink = { 0, 127, 255, 255 };
		const float cap = float(TEXT_BOTTOM - TEXT_TOP - 2) * k;
		const float fh = cap / fv2_texte::CAP;
		font->draw(ren, "CPU", mid.x + mx * k - font->width("CPU", fh) * 0.5f,
				   mid.y + float(TEXT_TOP + 1) * k - fv2_texte::CAP_TOP * fh, fh, ink);
	}
};
