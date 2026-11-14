#include "fv2_kuhn_name.h"
#include "tilegen.h"
#include "../machine/model3.h"
#include "../host/png.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace video {

namespace {

// Player 1's roster cursor, and player 2's beside it (docs/fv2-roster.md, and the roster patch's own notes).
constexpr uint32_t CURSOR = 0x00279668;
// The cursor that is Kuhn. The row the roster patch opens runs 0 to 13, and his name is the one Sega drew:
// cursor 12 is id 13, KUHN, chrome-lettered. (The second Kuhn, id 12, sits at cursor 14 under BAHN's borrowed
// plate, and that cursor is past the patched row's right-hand bound: it cannot be reached here.)
constexpr int KUHN = 12;

// WHERE THE NAME GOES, AND HOW BIG, in the picture's own 496 x 384 -- the same box B.M.'s name was measured
// into on the console branch, which is the name table's own place: columns 2 and 34 (x 16 and 272) of rows 28
// to 35 (y 224 to 287). The pictures are a 1:1 cut of a Dreamcast screen, 640 wide against this screen's 496,
// so they are resampled into the box rather than drawn pixel for pixel.
constexpr int NAME_W = 208, NAME_H = 51;
constexpr int NAME_Y = 227, NAME_X_1P = 16, NAME_X_2P = NAME_X_1P + 32 * 8;
// THE NUDGE, IN THE PICTURE'S OWN PIXELS. The box above is the name table's cell; the letters inside it are
// not the same in both pictures. Sega's KUHN starts at x 22 and its outline at 18, while the pack's picture is
// a tight crop whose first lit column IS its edge, so laid in the cell at 16 it sits left of his and a little
// high. Measured on the port's own 496 x 384 frame, the two cursors on Kuhn: Sega x 18..228 y 228..278, the
// pack x 16..228 y 228..277. These two numbers move the drawn picture only -- the cell, the clear and the
// other player's cell are where the game put them, and the nudge works out the same at every window size
// because it is counted in the picture, not on the screen.
constexpr int NAME_DX = 5, NAME_DY = 2;
// What is cleared before the picture goes down: the whole of the name strip across the picture's width, rows
// 28 to 35 of the name table. Sega's KUHN is drawn there and the picture's letters are transparent around
// their edges, so without this the chrome shows through the blue. The strip holds the two names and nothing
// else (the portraits are above it, the fighter below), so clearing the width of it takes one name without
// reaching anything the other player needs -- and both players see the same wheel anyway.
constexpr int STRIP_Y0 = 224, STRIP_Y1 = 288;

inline int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

// The name strip let through to the 3D again: alpha 0 is what the layer carries where the game drew nothing.
void clear_box(std::vector<uint32_t> &dst, int dw, int x0, int x1, int y0, int y1)
{
	for (int y = y0; y < y1; y++)
		for (int x = x0; x < x1; x++)
			dst[size_t(y) * dw + x] = 0;
}

// Source-over, one picture into one layer, resampled into a box of NAME_W x NAME_H of the hardware's own
// picture and drawn at `scale` times that (the HD 2D plane is the same picture larger: tilegen.h). The layer is
// 0xAABBGGRR with alpha 0 where it lets the 3D through, so the alpha is composed too and not merely kept --
// and the four samples are weighted with their alpha already in them, or the transparent side of an edge would
// drag its colour into the letter.
void over(std::vector<uint32_t> &dst, int dw, int dh, int scale, int x0, int y0,
          const std::vector<uint32_t> &src, int sw, int sh)
{
	const int ow = NAME_W * scale, oh = NAME_H * scale;
	for (int oy = 0; oy < oh; oy++) {
		const int y = y0 * scale + oy;
		if (y < 0 || y >= dh * scale) continue;
		const float fy = (oy + 0.5f) * sh / oh - 0.5f;
		const int yl = (int)std::floor(fy);
		const float ty = fy - yl;
		const int sy0 = clampi(yl, 0, sh - 1), sy1 = clampi(yl + 1, 0, sh - 1);
		for (int ox = 0; ox < ow; ox++) {
			const int x = x0 * scale + ox;
			if (x < 0 || x >= dw * scale) continue;
			const float fx = (ox + 0.5f) * sw / ow - 0.5f;
			const int xl = (int)std::floor(fx);
			const float tx = fx - xl;
			const int sx0 = clampi(xl, 0, sw - 1), sx1 = clampi(xl + 1, 0, sw - 1);
			const uint32_t p[4] = { src[size_t(sy0) * sw + sx0], src[size_t(sy0) * sw + sx1],
			                        src[size_t(sy1) * sw + sx0], src[size_t(sy1) * sw + sx1] };
			const float wgt[4] = { (1 - tx) * (1 - ty), tx * (1 - ty), (1 - tx) * ty, tx * ty };
			float ca = 0, cr = 0, cg = 0, cb = 0;
			for (int i = 0; i < 4; i++) {
				const float a = wgt[i] * ((p[i] >> 24) / 255.0f);
				ca += a;
				cr += a * (p[i] & 0xFF);
				cg += a * ((p[i] >> 8) & 0xFF);
				cb += a * ((p[i] >> 16) & 0xFF);
			}
			const uint32_t sa = (uint32_t)clampi((int)std::lround(ca * 255.0f), 0, 255);
			if (!sa) continue;
			const uint32_t sr = (uint32_t)clampi((int)std::lround(cr / ca), 0, 255);
			const uint32_t sg = (uint32_t)clampi((int)std::lround(cg / ca), 0, 255);
			const uint32_t sb = (uint32_t)clampi((int)std::lround(cb / ca), 0, 255);
			uint32_t &d = dst[size_t(y) * dw * scale + x];
			if (sa == 255) {
				d = (255u << 24) | (sb << 16) | (sg << 8) | sr;
				continue;
			}
			const uint32_t da = d >> 24;
			const uint32_t oa = sa + da * (255 - sa) / 255;
			auto mix = [&](uint32_t s, int shift) {
				const uint32_t dc = (d >> shift) & 0xFF;
				const uint32_t c = s * sa + dc * da * (255 - sa) / 255;
				return oa ? (c / oa) & 0xFF : 0u;
			};
			d = (oa << 24) | (mix(sb, 16) << 16) | (mix(sg, 8) << 8) | mix(sr, 0);
		}
	}
}

} // namespace

void kuhn_name_load(const std::string &pack, KuhnName &art)
{
	if (art.state) return;
	art.state = -1;
	static const char *FILES[2] = { "CHARACTERS/Kuhn/blue/NAME_BIG_blue.png",
		                        "CHARACTERS/Kuhn/black/NAME_BIG_black.png" };
	for (int i = 0; i < 2; i++) {
		std::string why;
		const std::string path = pack + "/" + FILES[i];
		if (!read_png(path.c_str(), art.px[i], art.w[i], art.h[i], why) || art.w[i] < 1 || art.h[i] < 1) {
			// Not an error: the colour is still worn, his name simply stays the one Sega drew.
			fprintf(stderr, "Kuhn's name: %s not read (%s): his name stays as Sega drew it\n", path.c_str(), why.c_str());
			art.px[i].clear();
			continue;
		}
		art.have[i] = true;
		fprintf(stderr, "Kuhn's name: the %s one read, %d x %d, drawn %d x %d\n",
		        i ? "black" : "blue", art.w[i], art.h[i], NAME_W, NAME_H);
	}
	if (art.have[0] || art.have[1]) art.state = 1;
}

bool kuhn_name(const m3::Model3 &board, std::vector<uint32_t> &above, int cycle, const KuhnName &art)
{
	// Sega's own two colours are the wheel's 0 and 1, and they are the game's own name: nothing to draw.
	if (art.state != 1 || cycle < 2 || cycle > 3) return false;
	const int which = cycle - 2;
	if (!art.have[which]) return false;
	if (board.set != "fvipers2") return false;
	// The select screen and nowhere else: the same bytes mean something else on another screen.
	//
	// THREE STATES, NOT ONE, and they were measured one frame at a time (entry1 at 0x00700000, printed on every
	// change of it through a whole choice and the fight after it). 5 is the screen with the cursor live on it. The
	// press that chooses a fighter puts 45 there for ONE FRAME, and then 43 for about a second: that is still the
	// same picture -- the timer at 00, PRESS START BUTTON, the fighter posing, his name in the same cell -- and
	// drawing on 5 alone is what made the name snap back to Sega's gold at the very moment of choosing. 42 comes
	// next and holds the loading logo AND the whole fight, so it must not draw. Measured: 5 to frame 2601, 45 at
	// 2602, 43 from 2603 to 2665, 42 from 2666 to the end of the round.
	const int state = board.fv2_task_state();
	if (state != 5 && state != 45 && state != 43) return false;
	if (above.size() < size_t(W) * H) return false;
	if (board.ram.size() <= CURSOR + 1) return false;
	bool drawn = false;
	for (int p = 0; p < 2; p++) {
		if (board.ram[(CURSOR + p) ^ 3] != KUHN) continue;
		const int x0 = p ? NAME_X_2P : NAME_X_1P;
		// Cleared from the cell's own left edge and as far right as the nudge carries the picture: clearing
		// from the nudged edge instead would leave the first columns of Sega's chrome K standing beside it.
		clear_box(above, W, x0, std::min(x0 + NAME_W + NAME_DX, int(W)), STRIP_Y0, STRIP_Y1);
		over(above, W, H, 1, x0 + NAME_DX, NAME_Y + NAME_DY, art.px[which], art.w[which], art.h[which]);
		drawn = true;
	}
	return drawn;
}

} // namespace video
