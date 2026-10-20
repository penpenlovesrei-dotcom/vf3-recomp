#include "fv2_portraits.h"
#include "../host/png.h"
#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <filesystem>

namespace fv2 {

namespace {

// ---- where the sheets are, measured here ----
//
// `tools/vf3run` was not needed for this: the VROM is a ROM, the game never writes it, so the measurement is of
// romset::load's output alone. The probe decoded 256x256 at each of the two offsets the owner's Supermodel-side
// notes give and wrote the result out as PNG, and the pictures are the eleven portraits and the white case.
// Both fingerprints below came out of that run, not out of the notes:
//
//   light at 0x140a6a8: first 16 bytes 25512551255125512551255125512551
//   dark  at 0x1435150: first 16 bytes c340c340c340c340c340c340c340c340
//
// The spacing was measured rather than carried over: walking the mipmap pyramid of the sheet at 0x140a6a8 --
// 256, then 128, 64, 32, 16, 8, 4, 2, each level's texels straight after the last -- ends at exactly 0x1435150,
// where the dark sheet starts. So the pyramid is 0x2AAA8 bytes, the two sheets are back to back, and the
// texture is one the Real3D uploads with its mipmaps (header type 0), not one it builds them for.
//
// The two textures that follow at the same spacing were dumped too, to be sure nothing here runs past the
// sheets into something else: 0x145FBF8 is the digits 0-9, 0x148A6A0 the FIGHTING VIPERS 2 logo. Neither is
// touched.
constexpr uint32_t SHEET_LIGHT = 0x0140A6A8;
constexpr uint32_t SHEET_DARK = 0x01435150;
constexpr uint32_t PYRAMID_BYTES = 0x2AAA8;
const uint16_t FINGERPRINT[2] = { 0x5125, 0x40C3 };   // the first texel of each sheet, little-endian in the VROM

constexpr int SIDE = 256, CASE = 64, COLUMNS = 4, ROWS = 4, SLOTS = COLUMNS * ROWS;

// ---- the texel order inside a tile ----
//
// These are the same three tables as src/machine/model3.cpp's decode8x8 / decode8x4 / decode8x2, and they are
// the whole difficulty of this format: the texels are not stored row by row. They are stored in tiles, and the
// tiles sweep the whole 256-wide sheet, not one 64x64 case at a time -- which is why looking for a portrait in
// the VROM row by row finds the first tile and nothing else.
//
// They are not re-derived here, they are shared on purpose: this file and the Real3D's upload path must agree
// texel for texel or the picture is scrambled, and two copies that can drift apart would be worse than one
// that cannot. What is checked here instead, every time and before anything is written, is that this file's
// encoder exactly undoes its decoder over the whole sheet and the whole pyramid (see round_trips below). A
// sheet that fails that is left alone.
const uint8_t DEC8[64] = { 1, 0, 5, 4, 9, 8, 13, 12, 3, 2, 7, 6, 11, 10, 15, 14, 17, 16, 21, 20, 25, 24, 29, 28,
	19, 18, 23, 22, 27, 26, 31, 30, 33, 32, 37, 36, 41, 40, 45, 44, 35, 34, 39, 38, 43, 42, 47, 46, 49, 48, 53, 52,
	57, 56, 61, 60, 51, 50, 55, 54, 59, 58, 63, 62 };
const uint8_t DEC4[32] = { 1, 0, 5, 4, 3, 2, 7, 6, 9, 8, 13, 12, 11, 10, 15, 14, 17, 16, 21, 20, 19, 18, 23, 22,
	25, 24, 29, 28, 27, 26, 31, 30 };
const uint8_t DEC2[16] = { 1, 0, 3, 2, 5, 4, 7, 6, 9, 8, 11, 10, 13, 12, 15, 14 };

const uint8_t *tile_order(int tx) { return tx == 8 ? DEC8 : tx == 4 ? DEC4 : DEC2; }

// A level's texels, tiled, are `w * h` 16-bit words
size_t level_bytes(int w, int h) { return size_t(w) * h * 2; }

// VROM bytes -> an image of T1RGB5 words, row by row. The texels arrive in tile order, and inside a tile the
// word at index DEC[yy * tx + xx] is the texel at (xx, yy) -- which is what Model3::r3d_store does with the
// same bytes when the Real3D uploads them.
void detile(const uint8_t *src, int w, int h, std::vector<uint16_t> &out)
{
	const int tx = std::min(8, w), ty = std::min(8, h);
	const uint8_t *dec = tile_order(tx);
	out.assign(size_t(w) * h, 0);
	size_t k = 0;
	for (int y0 = 0; y0 < h; y0 += ty)
		for (int x0 = 0; x0 < w; x0 += tx, k += size_t(tx) * ty)
			for (int yy = 0; yy < ty; yy++)
				for (int xx = 0; xx < tx; xx++) {
					const uint8_t *p = src + (k + dec[yy * tx + xx]) * 2;
					out[size_t(y0 + yy) * w + (x0 + xx)] = uint16_t(p[0] | p[1] << 8);
				}
}

// The same journey back: an image of T1RGB5 words -> VROM bytes, little-endian, in tile order.
void tile(const std::vector<uint16_t> &img, int w, int h, uint8_t *dst)
{
	const int tx = std::min(8, w), ty = std::min(8, h);
	const uint8_t *dec = tile_order(tx);
	size_t k = 0;
	for (int y0 = 0; y0 < h; y0 += ty)
		for (int x0 = 0; x0 < w; x0 += tx, k += size_t(tx) * ty)
			for (int yy = 0; yy < ty; yy++)
				for (int xx = 0; xx < tx; xx++) {
					const uint16_t v = img[size_t(y0 + yy) * w + (x0 + xx)];
					uint8_t *p = dst + (k + dec[yy * tx + xx]) * 2;
					p[0] = uint8_t(v);
					p[1] = uint8_t(v >> 8);
				}
}

// ---- T1RGB5 ----
//
// `T RRRRR GGGGG BBBBB`, little-endian in the VROM, T set meaning the texel is the polygon's contour -- what the
// renderer treats as transparent.
//
// Only this direction is here. Turning a texel back into a pixel is what a tool that exports the sheet needs,
// and nothing in the port does that: the renderer reads texels its own way (src/video), and the sheets this
// code was measured against were exported by a throwaway probe, not by the port. The pair that does have to be
// exact both ways is detile/tile above, and round_trips checks it on the sheet's own bytes before every write.
uint16_t rgba_to_texel(uint32_t p)
{
	if ((p >> 24) < 128) return 0x8000;   // the contour bit: what the sheets use for "nothing here"
	const unsigned r = ((p & 0xFF) * 31 + 127) / 255;
	const unsigned g = (((p >> 8) & 0xFF) * 31 + 127) / 255;
	const unsigned b = (((p >> 16) & 0xFF) * 31 + 127) / 255;
	return uint16_t((r << 10) | (g << 5) | b);
}

// ---- the levels ----
//
// A level's size, in the order the Real3D reads them out of the one stream: 256 (level 0), then 128, 64, 32,
// 16, 8, 4, 2. Measured: the eight of them come to 0x2AAA8 bytes, which is the spacing between the two sheets.
struct level { int w, h; uint32_t at; };

std::vector<level> levels()
{
	std::vector<level> out;
	uint32_t at = 0;
	for (int w = SIDE, h = SIDE;; w /= 2, h /= 2) {
		out.push_back({ w, h, at });
		at += uint32_t(level_bytes(w, h));
		if (w < 4 || h < 4) break;
	}
	return out;
}

// One 64x64 case covers a whole number of texels down to the 4x4 level (where the sheet's 4x4 cases are one
// texel each) and half a texel at the 2x2 level. So the 2x2 level is left exactly as Sega wrote it: a case
// cannot be put into it without touching the three next to it, and nothing that small is ever seen anyway.
bool level_has_whole_cases(const level &l) { return l.w >= COLUMNS && l.h >= ROWS; }

// Box filter, RGBA, from a square image to half its side. Straight average of the four texels, alpha included:
// the sheets have no transparent texel in them (every word read back has bit 15 clear), so there is no
// premultiplied-alpha question to get wrong here.
void halve(const std::vector<uint32_t> &src, int w, std::vector<uint32_t> &dst)
{
	const int n = w / 2;
	dst.assign(size_t(n) * n, 0);
	for (int y = 0; y < n; y++)
		for (int x = 0; x < n; x++) {
			unsigned c[4] = { 0, 0, 0, 0 };
			for (int dy = 0; dy < 2; dy++)
				for (int dx = 0; dx < 2; dx++) {
					const uint32_t p = src[size_t(2 * y + dy) * w + (2 * x + dx)];
					for (int k = 0; k < 4; k++) c[k] += (p >> (8 * k)) & 0xFF;
				}
			uint32_t out = 0;
			for (int k = 0; k < 4; k++) out |= ((c[k] + 2) / 4) << (8 * k);
			dst[size_t(y) * n + x] = out;
		}
}

// ---- the check that comes before any write ----
//
// detile and tile are two halves of the same permutation, and a permutation that is wrong in both halves the
// same way would still read the sheet back correctly while writing nonsense into the VROM. So the sheet's own
// bytes are run through both, at every level, and compared with what was there. Anything but an exact match and
// the sheet is left alone.
bool round_trips(const uint8_t *sheet)
{
	std::vector<uint16_t> img;
	std::vector<uint8_t> back;
	for (const level &l : levels()) {
		detile(sheet + l.at, l.w, l.h, img);
		back.assign(level_bytes(l.w, l.h), 0);
		tile(img, l.w, l.h, back.data());
		if (memcmp(back.data(), sheet + l.at, back.size()) != 0) return false;
	}
	return true;
}

// ---- the folder ----

// `light/07.png`, then `claire/07.png`, then `07.png` at the root. The French names are the ones the owner's
// own Supermodel-side tool writes, so a folder it made is taken as it stands.
std::string find_png(const std::filesystem::path &folder, const char *const *subdirs, int slot)
{
	char leaf[16];
	snprintf(leaf, sizeof leaf, "%02d.png", slot);
	std::error_code ec;
	for (int i = 0; subdirs[i]; i++) {
		const std::filesystem::path p = folder / subdirs[i] / leaf;
		if (std::filesystem::is_regular_file(p, ec)) return p.string();
	}
	const std::filesystem::path root = folder / leaf;
	if (std::filesystem::is_regular_file(root, ec)) return root.string();
	return {};
}

void say(std::string &log, const char *fmt, ...)
{
	char line[1024];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(line, sizeof line, fmt, ap);
	va_end(ap);
	log += line;
	log += '\n';
}

// One sheet. Returns how many cases it wrote, or a negative code; writes nothing at all unless at least one
// case was read and the round-trip check passed.
int do_sheet(uint8_t *sheet, const std::filesystem::path &folder, const char *const *subdirs, const char *name,
	std::string &log)
{
	// (the caller has already checked this sheet's fingerprint in the VROM; nothing here runs without that)
	// Read the PNGs first, so a folder with nothing usable in it costs no write
	std::vector<std::vector<uint32_t>> cases(SLOTS);
	int found = 0;
	for (int slot = 0; slot < SLOTS; slot++) {
		const std::string path = find_png(folder, subdirs, slot);
		if (path.empty()) continue;
		std::vector<uint32_t> rgba;
		int w = 0, h = 0;
		std::string why;
		if (!read_png(path.c_str(), rgba, w, h, why)) {
			say(log, "portraits: %s %s -- kept Sega's", path.c_str(), why.c_str());
			continue;
		}
		if (w != CASE || h != CASE) {
			say(log, "portraits: %s is %dx%d, and a case is %dx%d -- kept Sega's", path.c_str(), w, h, CASE, CASE);
			continue;
		}
		cases[slot] = std::move(rgba);
		found++;
	}
	if (!found) {
		say(log, "portraits: the %s sheet has no 00.png to 15.png to put in it -- left as it is", name);
		return 0;
	}
	if (!round_trips(sheet)) {
		say(log, "portraits: the %s sheet doesn't come back the same through this build's own encoder -- nothing written", name);
		return CODEC_DISAGREES;
	}

	// Each level in turn: decode what is there, put the replaced cases into it at that level's scale, encode it
	// back. A case with no PNG is written back from the texels that were read, so its bytes do not change -- the
	// round-trip above is what makes that true rather than hoped for.
	std::vector<uint16_t> img;
	for (const level &l : levels()) {
		if (!level_has_whole_cases(l)) continue;
		detile(sheet + l.at, l.w, l.h, img);
		const int side = l.w / COLUMNS;   // the case's side at this level: 64, 32, 16, 8, 4, 2, 1
		for (int slot = 0; slot < SLOTS; slot++) {
			if (cases[slot].empty()) continue;
			// down to this level's scale, by halving as many times as it takes
			std::vector<uint32_t> a = cases[slot], b;
			for (int w = CASE; w > side; w /= 2) { halve(a, w, b); a.swap(b); }
			const int x0 = (slot % COLUMNS) * side, y0 = (slot / COLUMNS) * side;
			for (int y = 0; y < side; y++)
				for (int x = 0; x < side; x++)
					img[size_t(y0 + y) * l.w + (x0 + x)] = rgba_to_texel(a[size_t(y) * side + x]);
		}
		tile(img, l.w, l.h, sheet + l.at);
	}
	say(log, "portraits: %d of 16 cases replaced in the %s sheet (levels 256 down to 4; the 2x2 level is Sega's)",
		found, name);
	return found;
}

} // namespace

const char *portrait_name(int slot)
{
	// The sheet's own order, which is neither the cursor's nor the fight identifiers': the nine first are
	// alphabetical, then Emi and Tokio after them. It comes from the owner's earlier work and is re-stated here
	// because the sheet this port exported has the portraits in exactly that order, looked at and counted.
	static const char *NAMES[SLOTS] = { "Bahn", "Charlie", "Grace", "Honey", "Jane", "Mahler", "Picky", "Raxel",
		"Sanman", "Emi", "Tokio", "free", "free", "free", "free", "free" };
	return slot >= 0 && slot < SLOTS ? NAMES[slot] : "?";
}

int replace_portraits(const std::string &set, std::vector<uint8_t> &vrom, const std::string &folder, std::string &log)
{
	// The offsets were measured on the Japan Revision A set. The other Fighting Vipers 2 set is a different
	// build and nothing says its VROM is laid out the same, so it is left alone rather than written to at an
	// address that means nothing there -- the same rule Model3::unlock_roster keeps.
	if (set != "fvipers2") return NOT_THIS_GAME;
	std::error_code ec;
	const std::filesystem::path dir(folder);
	if (folder.empty() || !std::filesystem::is_directory(dir, ec)) return NO_FOLDER;
	if (vrom.size() < SHEET_DARK + PYRAMID_BYTES) return NOT_THE_SHEETS;

	// The fingerprint, before anything is written: the first texel of each sheet, as this port's own probe read
	// it out of this port's own VROM. A dump that doesn't match isn't these sheets, and nothing is touched.
	const uint32_t at[2] = { SHEET_LIGHT, SHEET_DARK };
	for (int i = 0; i < 2; i++) {
		const uint16_t first = uint16_t(vrom[at[i]] | vrom[at[i] + 1] << 8);
		if (first != FINGERPRINT[i]) {
			say(log, "portraits: %#x holds %04x, not the %04x this port measured there -- nothing written",
				at[i], first, FINGERPRINT[i]);
			return NOT_THE_SHEETS;
		}
	}

	static const char *LIGHT_DIRS[] = { "light", "claire", nullptr };
	static const char *DARK_DIRS[] = { "dark", "sombre", nullptr };
	const int a = do_sheet(&vrom[SHEET_LIGHT], dir, LIGHT_DIRS, "light", log);
	const int b = do_sheet(&vrom[SHEET_DARK], dir, DARK_DIRS, "dark", log);
	if (a < 0) return a;
	if (b < 0) return b;
	return a + b;
}

int replace_portraits_and_report(const std::string &set, std::vector<uint8_t> &vrom, const std::string &folder, FILE *out)
{
	std::string log;
	const int n = replace_portraits(set, vrom, folder, log);
	switch (n) {
	case NOT_THIS_GAME:
		say(log, "portraits: the select-screen sheets were measured on Fighting Vipers 2 (Japan, Revision A), not on this game -- nothing written");
		break;
	case NO_FOLDER:
		say(log, "portraits: no folder at %s -- nothing written", folder.c_str());
		break;
	default:
		break;
	}
	if (out) fputs(log.c_str(), out);
	return n;
}

} // namespace fv2
