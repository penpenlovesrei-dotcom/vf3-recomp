// TRAINING: the second side is CPU, and the select screen says so.
//
// Frederic, the 2026-10-25: "the P2 that ought to be called CPU". In training the two fighters are chosen by
// one pair of hands and the second is a dummy, so the game's own "2P" under its cursor is a lie -- a small
// one, but on the one screen where the player is deciding who he is fighting.
//
// ---- THE ANCHOR IS THE GAME'S OWN BYTE, AND THE THREE VERSIONS BEFORE THIS ONE LOOKED AT PIXELS ----
//
// They scanned the tile layer for the cursor's cyan arrow and drew the label on the longest run of it. That
// cannot work, and the tile dump says why in one picture: on the select screen LAYER 0 HOLDS THE TWO
// FIGHTERS' NAMES AND NOTHING ELSE (EMI, CHARLIE), layer 2 holds the backdrop, and the arrows and the "2P"
// are drawn by the Real3D, like the portraits. So the scan was looking for something that is not in the
// buffer it was reading. Measured the 2026-10-28, it found a ONE-PIXEL run:
//
//     training: the 2P cursor is at 431..431 of the picture
//
// where the arrow really spans 441 to 478. The box landed twenty pixels off and the real "2P" stayed where
// it was -- which is exactly what Frederic saw: "the CPU marking is very badly fitted in, P2 is still
// visible".
//
// ---- WHERE IT REALLY IS, read from the code and then measured ----
//
// 0x00279668 is player 1's roster cursor, read at 0x00060458 (docs/fv2-select-names.md). Asking
// tools/fv2/xrefs_ppc.py who else that neighbourhood touches shows the byte beside it read in the same
// breath everywhere -- 0x0005FCB4 and 0x0005FD10, 0x0005FFEC and 0x00060074 -- and both written together by
// the same initialisation at 0x00060D64 and 0x00060D80. That is player 2's: 0x00279669.
//
// Then it was MEASURED rather than believed. tools/scripts/fv2-cursor-2p.txt drives the cursor across the
// roster in training and photographs it; the byte steps 0x00, 0x01, 0x02 ... one per column, on SEL, and the
// arrow follows:
//
//     byte 2  -> the "2P" spans 107.5 .. 133.1 of the picture's 496, middle 120.3
//     byte 4  ->              192.6 .. 218.2,                        middle 205.4
//     byte 10 ->              447.2 .. 472.9,                        middle 460.0
//
// Three points, a straight line through them, and the third checks the line drawn through the other two:
// x = 35.5 + 42.45 * column, the text 25.6 wide, rows 363 to 375 of the 384. The arrow's own middle agrees
// with the text's to within six tenths of a pixel at all three, so one number serves both.
//
// What is behind the text is the backdrop and nothing else -- (49, 41, 106) read off the picture at rows
// 366 to 375 -- which is what makes covering it honest rather than a patch over a picture. It is sampled
// beside the label all the same, because the backdrop is a gradient elsewhere on that row.
//
// WHAT THE PHOTOGRAPH STILL SHOWS, and it is left rather than hidden: the box is a FLAT fill, and the
// backdrop behind it carries the ghost of the word VIPERS, so the patch cuts two of those letters short.
// Redrawing the backdrop from its own texture instead would be exact -- except that the 2D layer and the
// 3D are not mapped down the picture the same way (the backdrop band is drawn with the COLUMN scale, this
// label with the ROW scale, which is what the photographs agree with), so the letters would land somewhere
// else. Measuring that mapping is a job of its own and is not done here.
//
// WHAT THIS DOES NOT HANDLE: a roster that scrolls. Eleven columns fit the picture (0 to 10) and the twelfth
// would be drawn off its right edge. With the hidden fighters patched in the strip has more than eleven, and
// what the game does with the extra ones has not been measured. The label is simply not drawn past the edge.
#pragma once
#include <SDL3/SDL.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include "fv2_text.h"

struct fv2_cpu_label
{
	// Player 2's roster cursor, counted in columns from zero (see the head of this file).
	static constexpr int CURSOR_2P = 0x00279669;

	// The strip's geometry in the picture's own 496 columns, fitted on three photographs.
	static constexpr float X0 = 35.5f, PITCH = 42.45f;
	static constexpr int COLUMNS = 11;          // what fits the picture; past that nothing is drawn
	// The text's own rows, measured: nothing at 362, ink from 363 to 375, nothing at 376.
	static constexpr int TEXT_TOP = 363, TEXT_BOTTOM = 376;
	static constexpr float HALF = 15.0f;        // half the box: 30 columns over a "2P" that is 25.6 wide
	static constexpr float SAMPLE_DX = 26.0f;   // where the backdrop's colour is read, clear of the text

	// Over the picture the game made, and under the console's own surfaces. `mid` is the game's 4:3 screen in
	// the window's pixels; `below` is the layer the backdrop is in, at the picture's own size.
	static void render(SDL_Renderer *ren, const SDL_FRect &mid, int column, const uint32_t *below,
					   int w, int h, fv2_text *font)
	{
		if (!font || !font->ready || !below || w <= 0 || h <= 0) return;
		if (column < 0 || column >= COLUMNS) return;   // off the picture: nothing to say and nothing to cover
		const float mx = X0 + PITCH * float(column);

		// IT SAYS WHERE IT PUT IT, once a column: a label written at the wrong place looks exactly like one
		// written at the right place to anything but an eye, and a hidden run cannot look at a window.
		static int said = -1;
		if (column != said) {
			said = column;
			fprintf(stderr, "training: the 2P cursor is on column %d -- CPU written at %.1f of the picture\n",
					column, mx);
		}

		// The backdrop's own colour, read beside the label and on the text's middle row.
		uint8_t r = 49, g = 41, bl = 106;   // (what the picture reads there, if the sample cannot)
		const int sx = int(mx - SAMPLE_DX), sy = (TEXT_TOP + TEXT_BOTTOM) / 2;
		if (sx >= 0 && sx < w && sy >= 0 && sy < h) {
			const uint32_t px = below[size_t(sy) * size_t(w) + size_t(sx)];
			if (px >> 24) { r = uint8_t(px); g = uint8_t(px >> 8); bl = uint8_t(px >> 16); }
		}
		// TWO SCALES, NOT ONE, and the version before this one used one. The layer is 496 by 384, which is
		// 1.292, and it is shown as 4:3, which is 1.333: the picture is stretched ACROSS more than DOWN, so
		// a row multiplied by the column scale lands eleven rows too low. That is what the first photograph
		// of this showed -- the word written under the "2P" instead of over it, the x being right.
		const float kx = mid.w / float(w), ky = mid.h / float(h);
		SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);
		SDL_SetRenderDrawColor(ren, r, g, bl, 255);
		const SDL_FRect box = { mid.x + (mx - HALF) * kx, mid.y + float(TEXT_TOP) * ky,
								2.0f * HALF * kx, float(TEXT_BOTTOM - TEXT_TOP) * ky };
		SDL_RenderFillRect(ren, &box);
		SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);

		// and the word in the game's own azure, as tall as the glyphs it stands in for
		const SDL_Color ink = { 0, 127, 255, 255 };
		const float cap = float(TEXT_BOTTOM - TEXT_TOP - 2) * ky;
		const float fh = cap / fv2_text::CAP;
		font->draw(ren, "CPU", mid.x + mx * kx - font->width("CPU", fh) * 0.5f,
				   mid.y + float(TEXT_TOP + 1) * ky - fv2_text::CAP_TOP * fh, fh, ink);
	}
};
