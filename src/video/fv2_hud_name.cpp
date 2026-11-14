#include "fv2_hud_name.h"
#include "tilegen.h"
#include "../machine/model3.h"
#include "../host/png.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace video {

namespace {

// THE FIGHTER RECORD, the one the armour's colour comes out of as well (docs/fv2-colours.md): player 1's at
// 0x00700500 and player 2's 0x7000 further on, +184 who he is and +185 his *1P COLOR. These are the match's own
// two bytes, and they are what says whose bar this is: read in a fight, B.M. against Charlie, they gave
// who 9/10 and colour 1/0, frame after frame, which is exactly the pair that was chosen.
constexpr uint32_t RECORD[2] = { 0x00700500, 0x00707500 };
constexpr int WHO = 184, COLOUR = 185;
// Who they are, from the cursor-to-fighter table in docs/fv2-select-names.md: cursor 13 is fighter 9, B.M., and
// cursor 12 is fighter 13, KUHN. The ids are the game's own, not the row's places.
constexpr int BM = 9, KUHN = 13;

// FIGHTING VIPERS 2'S OWN HUD, measured here rather than borrowed. hud_names.cpp describes Virtua Fighter 3's --
// round markers at columns 22-25 and 36-39 of rows 6 and 7, the name pictures of varying width -- and this game
// is laid out differently. Reusing VF3's numbers drew the plate on the round's title card and nothing at all in
// the fight, which is how this was noticed.
//
// The name table dumped whole, row by row, through a fight (B.M. against Charlie, every 60 frames):
//
//   rows 7 and 8, columns  5..14   player 1's name, ten tiles by two, one run of tile numbers (2AD6..2AE9)
//   rows 7 and 8, columns 47..56   player 2's, the same shape (2828..283B)
//   rows 7 and 8, columns 23,25    player 1's round markers, two tiles each (0720,0721 over 0722,0723)
//   rows 7 and 8, columns 35,37    player 2's
//   row  6                         the energy bars themselves, filled right across
//   rows 9-13, columns 5..8        the small portrait beside the name
//
// So the name's cell is TEN TILES WIDE AND FIXED, both players, whatever the name -- B.M. and CHARLIE fill the
// same ten. The rows above and below are other things, which is why the picture's own extent is looked for
// inside rows 7 and 8 only.
constexpr int ROW0 = 7, ROW1 = 8;
constexpr int NAME_C0[2] = { 5, 47 }, NAME_C1[2] = { 14, 56 };
constexpr int MARKER_C[4] = { 23, 25, 35, 37 };
constexpr int S = 2;   // (TileGen::aligned moves the picture 2 pixels up and left)

inline int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

// A map entry's tile, the colour bits off; 0x20 is the HUD's blank (as hud_names.cpp reads it)
uint32_t tile_at(const uint32_t *names, int row, int col)
{
	const uint32_t w = names[row * 32 + col / 2];
	return ((col & 1) ? w : (w >> 16)) & 0x3FFF;
}

// A round marker: two tiles side by side, the second the first's number plus one, on both rows. A round won
// turns a marker red -- other tiles, still a consecutive pair -- so this holds from the first round to the last.
bool marker_at(const uint32_t *names, int c)
{
	for (int row = ROW0; row <= ROW1; row++) {
		const uint32_t a = tile_at(names, row, c);
		if (a == 0 || a == 0x20 || tile_at(names, row, c + 1) != a + 1) return false;
	}
	return true;
}

// The fight's HUD, up and whole: the four markers, and that player's ten tiles all laid. Measured through a
// whole match, the two agree: between rounds and while the HUD slides in, both fail together, and no screen
// outside the fight fills these ten tiles. The task state is asked for as well, 42 being the fight (that state
// also carries the loading logo and the round's title card, which fill none of this).
bool hud_up(const uint32_t *names, int p)
{
	for (int c : MARKER_C)
		if (!marker_at(names, c)) return false;
	for (int row = ROW0; row <= ROW1; row++)
		for (int c = NAME_C0[p]; c <= NAME_C1[p]; c++) {
			const uint32_t t = tile_at(names, row, c);
			if (t == 0 || t == 0x20) return false;
		}
	return true;
}

// Source-over, one picture resampled into a box of the hardware's own picture. The layer is 0xAABBGGRR with
// alpha 0 where it lets the 3D through, so the alpha is composed too and not merely kept -- and the four
// samples are weighted with their alpha already in them, or the transparent side of an edge would drag its
// colour into the letter. (The same resampler the two select-screen names use; the box is what differs.)
void over(std::vector<uint32_t> &dst, int x0, int y0, int bw, int bh, const HudPlate &src)
{
	for (int oy = 0; oy < bh; oy++) {
		const int y = y0 + oy;
		if (y < 0 || y >= H) continue;
		const float fy = (oy + 0.5f) * src.h / bh - 0.5f;
		const int yl = (int)std::floor(fy);
		const float ty = fy - yl;
		const int sy0 = clampi(yl, 0, src.h - 1), sy1 = clampi(yl + 1, 0, src.h - 1);
		for (int ox = 0; ox < bw; ox++) {
			const int x = x0 + ox;
			if (x < 0 || x >= W) continue;
			const float fx = (ox + 0.5f) * src.w / bw - 0.5f;
			const int xl = (int)std::floor(fx);
			const float tx = fx - xl;
			const int sx0 = clampi(xl, 0, src.w - 1), sx1 = clampi(xl + 1, 0, src.w - 1);
			const uint32_t p[4] = { src.px[size_t(sy0) * src.w + sx0], src.px[size_t(sy0) * src.w + sx1],
			                        src.px[size_t(sy1) * src.w + sx0], src.px[size_t(sy1) * src.w + sx1] };
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
			uint32_t &d = dst[size_t(y) * W + x];
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

void read_plate(const std::string &path, HudPlate &pl, const char *whose)
{
	std::string why;
	if (!read_png(path.c_str(), pl.px, pl.w, pl.h, why) || pl.w < 1 || pl.h < 1) {
		fprintf(stderr, "%s on the energy bar: %s not read (%s): that costume keeps the plate the game draws\n",
		        whose, path.c_str(), why.c_str());
		pl.px.clear();
		pl.w = pl.h = 0;
		return;
	}
	pl.have = true;
	fprintf(stderr, "%s on the energy bar: read, %d x %d\n", whose, pl.w, pl.h);
}

} // namespace

void fv2_hud_name_load(const std::string &assets, const std::string &pack, Fv2HudNames &art)
{
	if (art.state) return;
	art.state = -1;
	read_plate(assets + "/fv2/BM_lifebar_color_2.png", art.bm2, "B.M.'s second colour");
	read_plate(pack + "/CHARACTERS/Kuhn/blue/NAME_lifebar_blue.png", art.kuhn[0], "Kuhn's blue");
	read_plate(pack + "/CHARACTERS/Kuhn/black/NAME_lifebar_black.png", art.kuhn[1], "Kuhn's black");
	if (art.bm2.have || art.kuhn[0].have || art.kuhn[1].have) art.state = 1;
}

bool fv2_hud_name(const m3::Model3 &board, std::vector<uint32_t> &above, int kuhn_cycle, const Fv2HudNames &art)
{
	if (art.state != 1) return false;
	if (board.set != "fvipers2") return false;
	if (board.fv2_task_state() != 42) return false;   // the fight (and with it the loading logo and the title card)
	if (above.size() < size_t(W) * H) return false;
	if (board.ram.size() <= RECORD[1] + COLOUR) return false;
	if (board.tile_ram.size() < 0x100000) return false;
	// AND THE PICTURE ABOVE THE 3D MUST NOT BE THE WHOLE SCREEN, which is what finally tells the loading
	// logo from the fight. Nothing in the machine's state does: measured frame by frame from the moment the
	// task turns 42, the logo screen has the same task, the same layer registers, the same mask, the name
	// table already holding the fight's HUD with its ten tiles and its four markers, and layer A already
	// above the 3D. What differs is the picture. The logo is a 2D image filling the screen in that same
	// storey, so everything above the 3D is opaque -- 2976 of 2976 pixels sampled, and the name's ten tiles
	// opaque to the last texel, 1280 of 1280, a solid block and not a plate. In the fight the layers above
	// the 3D hold the HUD and nothing else: 272 to 291 of the same 2976, about a ninth of the screen, and
	// the plate itself 844 of 1280, the rest of its cell letting the stage through. A tenth against a whole,
	// so half the screen is the line, and no fight ever approaches it.
	int lit = 0, seen = 0;
	for (int y = 0; y < H; y += 8)
		for (int x = 0; x < W; x += 8) { seen++; if (above[size_t(y) * W + x] >> 24) lit++; }
	if (lit * 2 > seen) return false;
	// layer A on, and where it belongs: the HUD's own place, unscrolled
	const uint32_t reg = board.tile_regs[0x60 / 4];
	if (!(reg & 0x80000000u) || (reg & 0x3FF) || ((reg >> 16) & 0x1FF)) return false;
	const uint32_t *map = reinterpret_cast<const uint32_t *>(board.tile_ram.data() + 0xF8000);
	bool drawn = false;
	for (int p = 0; p < 2; p++) {
		const uint32_t rec = RECORD[p];
		const int who = board.ram[(rec + WHO) ^ 3];
		const int colour = board.ram[(rec + COLOUR) ^ 3];
		// Which of the three this is, if any. B.M.'s first colour and Kuhn's own two are the game's own
		// business: it draws them right, and nothing here touches them.
		const HudPlate *plate = nullptr;
		if (who == BM && colour == 1) plate = &art.bm2;
		else if (who == KUHN && kuhn_cycle >= 2 && kuhn_cycle <= 3) plate = &art.kuhn[kuhn_cycle - 2];
		if (!plate || !plate->have) continue;
		if (!hud_up(map, p)) continue;
		const int cx0 = NAME_C0[p] * 8 - S, cx1 = (NAME_C1[p] + 1) * 8 - S;
		const int cy0 = ROW0 * 8 - S, cy1 = (ROW1 + 1) * 8 - S;
		if (cx0 < 0 || cx1 > W || cy0 < 0 || cy1 > H) continue;
		// THE PICTURE'S OWN EXTENT INSIDE THAT CELL, not the cell. The game's plate does not fill its ten
		// tiles -- the letters sit in the middle of them with air around -- and the pictures beside the
		// program are cut tight, their letters touching all four edges (measured: 85 x 20 and 89 x 20, opaque
		// from edge to edge). Laid into the whole cell they would come out bigger than the plate they
		// replace. So the layer itself is asked where the old plate is: the pixels the game left standing
		// there are the plate, everything else in the cell is alpha 0 and the 3D shows through.
		int x0 = cx1, x1 = cx0, y0 = cy1, y1 = cy0;
		for (int y = cy0; y < cy1; y++)
			for (int x = cx0; x < cx1; x++)
				if (above[size_t(y) * W + x] >> 24) {
					x0 = std::min(x0, x); x1 = std::max(x1, x + 1);
					y0 = std::min(y0, y); y1 = std::max(y1, y + 1);
				}
		// And nothing there means nothing to replace.
		if (x1 <= x0 || y1 <= y0) continue;
		// Cleared first: the picture's letters are transparent around their edges, so without this the old
		// plate shows through the new one.
		for (int y = y0; y < y1; y++)
			for (int x = x0; x < x1; x++) above[size_t(y) * W + x] = 0;
		over(above, x0, y0, x1 - x0, y1 - y0, *plate);
		drawn = true;
	}
	return drawn;
}

} // namespace video
