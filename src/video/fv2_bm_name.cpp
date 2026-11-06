#include "fv2_bm_name.h"
#include "tilegen.h"
#include "../machine/model3.h"
#include "../host/png.h"
#include <cmath>
#include <cstdio>

namespace video {

namespace {

// Player 1's roster cursor, and player 2's beside it (docs/fv2-roster.md, docs/fv2-select-names.md).
constexpr uint32_t CURSOR = 0x00279668;
constexpr int BM = 13;   // the cursor that is B.M.

// HIS COLOUR, measured here the 2026-11-04 rather than reasoned about.
//
// Two runs of the headless runner on the select screen, one with a press of Up and one without, and the 8 MB of
// RAM dumped from each (`vf3run --ram-at`): inside 0x279600-0x2796FF exactly two bytes move, 0x279670 and
// 0x279674, both 0 -> 1. Two more runs with both players in, one where only player 2 presses Up and one where
// only player 1 does, say which byte is whose:
//
//   player 1's Up   ->  0x279670 and 0x279674
//   player 2's Up   ->  0x279671 and 0x279675
//
// So the colour is one byte a player, laid out exactly as the cursors are, with a second copy four bytes on
// (not chased: one of the two is enough, and the first is the one beside the cursors). `& 1` because this
// reads a toggle and draws one of two pictures; whatever else the game may put there, B.M. has two colours.
constexpr uint32_t COLOUR = 0x00279670;

// WHERE THE NAME GOES, AND HOW BIG, in the picture's own 496 x 384.
//
// Both were measured the 2026-11-06 on a Dreamcast capture Frederic sent -- the same screen, where Sega did
// draw his name. The picture itself was laid over that capture at every size, each side reduced to "is this
// the logo's gold", and the size that covered it best wins: 642 x 153 of the emulator's 1528 x 1146 window,
// 98 % of the gold on both sides agreeing.
//
//   Dreamcast, 640 x 480   269 x 64 at (20, 284)        player 2 at x 349
//   here,      496 x 384   208 x 51 at (16, 227)        player 2 at x 272
//
// The two readings fall on the name table's own columns -- 2 for player 1, 34 for player 2, so x 16 and 272,
// measured 15.6 and 270.4 -- and inside the name strip, rows 28 to 35, y 224 to 287 (docs/fv2-select-names.md).
// So the logo is laid where a name tile would be, not beside it.
//
// The pictures beside the program are a 1:1 cut of that Dreamcast screen, 269 x 64, which is why they came out
// a third too wide drawn pixel for pixel: 640 / 496. They are not resized on the disc. They are resampled into
// the box below each frame, so a sharper picture of the same artwork would land in the same place and be drawn
// better, not larger.
constexpr int NAME_W = 208, NAME_H = 51;
constexpr int NAME_Y = 227, NAME_X_1P = 16, NAME_X_2P = NAME_X_1P + 32 * 8;

inline int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

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

void bm_name_load(const std::string &assets, BMName &art)
{
	if (art.state) return;
	art.state = -1;
	const std::string dir = assets + "/fv2/";
	static const char *FILES[2] = { "BM_color_1.png", "BM_color_2.png" };
	for (int i = 0; i < 2; i++) {
		int w = 0, h = 0;
		std::string why;
		if (!read_png((dir + FILES[i]).c_str(), art.px[i], w, h, why)) {
			fprintf(stderr, "B.M.'s name: %s%s not read (%s): his name stays as Sega left it, blank\n",
			        dir.c_str(), FILES[i], why.c_str());
			return;
		}
		// The two colours are the same picture twice over, so a mismatch is a mistake in the folder, not a
		// thing to half-draw.
		if (i && (w != art.w || h != art.h)) {
			fprintf(stderr, "B.M.'s name: %s is %d x %d and %s is %d x %d: the two colours must be one size\n",
			        FILES[0], art.w, art.h, FILES[i], w, h);
			return;
		}
		art.w = w;
		art.h = h;
	}
	if (art.w < 1 || art.h < 1) return;
	art.state = 1;
	fprintf(stderr, "B.M.'s name: his two colours read, %d x %d, drawn %d x %d\n",
	        art.w, art.h, NAME_W, NAME_H);
}

bool bm_name(const m3::Model3 &board, std::vector<uint32_t> &above, int scale, const BMName &art)
{
	if (art.state != 1 || scale < 1) return false;
	if (board.set != "fvipers2") return false;
	// The select screen and nowhere else: the same bytes mean something else on another screen.
	if (board.fv2_task_state() != 5) return false;
	if (above.size() < size_t(W) * H * scale * scale) return false;
	if (board.ram.size() <= COLOUR + 1) return false;
	bool drawn = false;
	for (int p = 0; p < 2; p++) {
		if (board.ram[(CURSOR + p) ^ 3] != BM) continue;
		const int colour = board.ram[(COLOUR + p) ^ 3] & 1;
		over(above, W, H, scale, p ? NAME_X_2P : NAME_X_1P, NAME_Y, art.px[colour], art.w, art.h);
		drawn = true;
	}
	return drawn;
}

} // namespace video
