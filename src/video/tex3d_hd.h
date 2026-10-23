// High-resolution replacements for the Real3D's textures, as a texture pack: what the game draws is recognised by
// the hash of its content and swapped for an image of any size out of a folder. Off unless asked for, and nothing
// here is reached when it is off.
//
// Where this port's grip is, measured by reading the two rasterisers (and why it is not where Supermodel's is):
//
//   * The CPU rasteriser (src/video/r3d_raster.cpp) samples through `Sampler`, which reads the board's texture
//     memory as 16-bit words -- `tex[(page * 1024 + mip_y[level] + y) * 2048 + mip_x[level] + x]` -- and decodes
//     each word with the polygon's format. Its whole address of a texture is (page, x, y, width, height, format),
//     which it takes from the mesh and the nodes' texture offset.
//   * The GPU renderer (src/gpu/r3d_gpu.cpp, src/gpu/r3d.hlsl) uploads the same texture memory into one raw
//     storage buffer and decodes it in the shader with the same arithmetic, from the same six numbers, which it
//     puts in its per-mesh `Draw` record.
//
// So both rasterisers reach a texture through one address of six numbers, and neither has anything like a texture
// object per texture of the game. What they share is better than a key: `video::Raster::prepare` already builds
// the list of polygons both of them draw (`Raster::draws`), so one pass over that list, done once per picture,
// serves both. That pass is `Raster::resolve_hd`.
//
// Hence the shape of the replacement: not a second sampler in each rasteriser, but **more pages of texture
// memory**. The board has two pages of 2048 x 1024 words; this adds pages of the same size after them, holds the
// pack's images in those, and hands a polygon the address of its replacement instead of its own. The CPU
// rasteriser reads those pages from here (`pages()`); the GPU renderer makes its one buffer that much bigger and
// uploads them once, after the board's two. Nothing else in either rasteriser changes, and both sample the very
// same words by the very same code, so the two pictures stay comparable -- which is the thing this port cannot
// afford to lose (docs/step2-fvipers2.md section 15).
//
// What that costs: the pages are the hardware's 16-bit texels, so a replacement is **higher resolution at the
// game's own colour depth**, not at 24 bits. The native formats are all 16-bit (T1RGB5, RGBA4, A4L4 and the
// rest), so nothing is lost against what the game itself could have shown; a photograph's gradient would band
// exactly as the game's own textures band. The alternative -- an RGBA8 atlas of its own, as Supermodel's HD pack
// has -- needs a new resource and a new branch in the shader, and src/gpu/r3d_shaders.h can only be rebuilt with
// a DXC that generates SPIR-V (the Windows Kits' cannot). The page route needs no shader change at all.
//
// Identity is the content, never the place: the game uploads its textures where it likes and moves them between
// scenes. Identity = (FNV-1a 64 of the level-0 texels, width, height, format); the format is in it because the
// same words read as T1RGB5 or as A4L4 are not the same picture, and it is the polygon that says which.
//
// A texture of one single texel value is refused, in the dump and at the match: its hash is the hash of every
// flat texture of that size, so a pack entry for one would replace anything flat anywhere on the screen.
//
// **The alpha a dumped image carries is the polygon's, not the texel's.** A texel's top bit (T1RGB5) or its
// alpha nibble is only transparency when the polygon asks for it, and src/video/r3d_raster.cpp says so in three
// lines at the end of its texel fetch:
//
//     if (mesh.alpha_test && t.a < 32.0f / 255.0f) continue;   // a cut-out: the texel is dropped
//     if (mesh.texture_alpha) { ... }                          // the alpha grades the blend
//     else if (!mesh.alpha_test) { t.a = 1; }                  // otherwise FORCED OPAQUE
//
// Dumping the decoded alpha regardless, as this did until 2026-10-21, writes transparency the screen never had.
// In Fighting Vipers 2 it shows as a one-pixel comb -- every other column, sometimes every other row, fully
// transparent -- because the game leaves the T bit alternating in texture memory where no polygon reads it.
// Supermodel has the same trouble in its own sheet dump and answers it by writing the whole of texture memory
// twice, `textures_t1rgb5_contour.bmp` and `textures_t1rgb5_opaque.bmp` (Src/Model3/Real3D.cpp), leaving the
// choice to the eye. Here the choice is not needed: the polygon that drew the texture is in hand, so the dump
// writes the alpha that polygon used and says which in textures.csv.
#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace video {

// Where a texture is, for both rasterisers: a page of texture memory, a place in it, a size and a format.
// hd: the page is one of this file's pages (pages()), not one of the board's two.
struct TexPlace
{
	int page = 0, x = 0, y = 0, w = 0, h = 0, format = 0;
	bool hd = false;
};

// What the polygon does with the texel's alpha, which is what the dumped image must carry. Read off the three
// lines of r3d_raster.cpp quoted above, in their order: the contour test comes first, so a polygon that has both
// counts as Contour.
enum class AlphaUse
{
	Opaque,    // neither flag: the rasteriser forces t.a = 1
	Contour,   // alpha_test: a one-bit cut-out, the texel dropped below 32/255
	Graded,    // texture_alpha: the texel's own alpha grades the blend
};

class Tex3dHd
{
public:
	// The dump: every distinct texture a polygon samples written to dir as a PNG named <hash>_<w>x<h>_f<format>.png,
	// with a textures.csv beside it saying where each was found. Entirely passive.
	bool dump_to(const std::string &dir, std::string &error);
	// The pack: dir/<hash>_<w>x<h>_f<format>.png, the name being the key the dump wrote. Any image size; the
	// name's size is the native one, for identification only. report gets a line for the player.
	bool load(const std::string &dir, std::string &report);

	// Lever le refus des textures unies, pour une course et pas pour le jeu : voir find(). Mis par --hd-flats.
	void allow_flat_textures(bool on) { allow_flat = on; }
	bool flat_allowed() const { return allow_flat; }

	bool dumping() const { return dump_on; }
	bool replacing() const { return !entries.empty(); }
	bool active() const { return dump_on || replacing(); }

	// The replacement for a texture a polygon is about to draw, or false for "the native one stands". tex: the
	// board's texture memory (2048 x 2048 words). Called once per polygon per picture, from Raster::resolve_hd,
	// after new_picture().
	bool find(const uint16_t *tex, const TexPlace &native, bool translator_map, AlphaUse alpha, TexPlace &out);
	// Forgets what was worked out for the picture before. The content of a place in texture memory is hashed
	// again every picture, because the game writes over its texture memory as it goes and nothing here is told
	// when it does; within one picture the same place is hashed once however many polygons read it.
	void new_picture() { seen.clear(); }

	// The HD pages: page_count() pages of 1024 rows of 2048 words, laid out exactly as the board's are
	const uint16_t *pages() const { return hd.empty() ? nullptr : hd.data(); }
	int page_count() const { return int(hd.size() / (size_t(1024) * 2048)); }

	// What the last load() and the run since have come to (for the log and the tests)
	struct Stats { size_t loaded = 0, rejected = 0, matched = 0, flat_refused = 0, dumped = 0; };
	const Stats &stats() const { return st; }

private:
	struct Entry
	{
		TexPlace place;              // where it was put in the HD pages
		bool placed = false;
		bool reported = false;       // (said in the log the first time it was drawn)
		std::string name;
	};
	// (hash, width, height, format) is the identity; packed into the map's key
	struct Key
	{
		uint64_t hash;
		int w, h, format;
		bool operator==(const Key &o) const { return hash == o.hash && w == o.w && h == o.h && format == o.format; }
	};
	struct KeyHash
	{
		size_t operator()(const Key &k) const
		{
			return size_t(k.hash ^ (uint64_t(k.w) * 0x9E3779B97F4A7C15ull) ^ (uint64_t(k.h) << 20) ^ (uint64_t(k.format) << 40));
		}
	};
	std::unordered_map<Key, Entry, KeyHash> entries;
	// What find() has already worked out for this picture, by where the texture is (page, x, y, size, format):
	// the same texture is drawn by many polygons and hashing it once a picture is enough. Cleared by new_picture().
	struct Seen { bool hit = false; TexPlace place; };
	std::unordered_map<uint64_t, Seen> seen;
	std::vector<uint16_t> hd;
	// the shelves the images are laid in: per page, the next free row and the row's used width and height
	struct Shelf { int page = 0, y = 0, x = 0, h = 0; };
	Shelf shelf;
	bool dump_on = false;
	bool allow_flat = false;
	std::string dump_dir;
	std::unordered_set<std::string> dumped_names;
	// What alpha each dumped image was written with. The image is written the first time the texture is drawn;
	// if a later polygon draws the same texels with a different use of the alpha, the file cannot be both and a
	// line is printed rather than the picture being quietly wrong.
	std::unordered_map<std::string, AlphaUse> dumped_alpha;
	Stats st;

	bool place(const std::vector<uint8_t> &rgba, int w, int h, int format, TexPlace &out);
	void dump_one(const uint16_t *tex, const TexPlace &native, uint64_t hash, AlphaUse alpha);
};

// The one instance: the option is a whole-run setting, and both rasterisers and the host read it
Tex3dHd &hd3d();

} // namespace video
