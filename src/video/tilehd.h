// HD 2D: the tile plane drawn at a multiple of its native resolution, with single tiles and whole layers
// replaced by images from a folder, texture-pack fashion. See docs/fv2-hd-2d.md.
//
// Nothing here runs unless the player asks for it: TileGen keeps a scale of 1 and no pack by default, and at
// scale 1 with no pack not one instruction of the rasteriser changes (src/video/tilegen.cpp, the `hd` guard).
//
// The identity of a tile is NOT its pattern index. That index is a byte offset into the tile RAM and the game
// moves its patterns about as it streams them; on Fighting Vipers 2 the patterns do not exist anywhere in the
// ROM in the clear at all -- the program is a gzip the boot inflates, and the tiles only appear in the tile RAM
// at run time (measured in `fv2mod/HANDOFF.md` section 9 bis, read as documentation). So the identity is the
// pair (hash of the pattern's bytes, palette block of the name-table entry), and the palette is part of it
// because it comes from the entry rather than the pattern: one pattern can be drawn in several colours, and in
// 4-bit mode bits of the pattern index are reused inside the palette index.
//
// The hashes below are the only place these names are made, and the dump writes the file names the pack later
// looks up: a dump and a pack that disagreed by one bit would match nothing at all, in silence.
#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

namespace video {

// A picture in memory: RGBA8 with red in the low byte, as the rest of the port keeps its pictures. Alpha 0 is
// "nothing drawn here".
struct HDImage {
	int w = 0, h = 0;
	std::vector<uint32_t> px;
	bool screen = false;   // (a layer image: the whole 512-tile map, or the visible screen -- see TilePack::load)
	const uint32_t *row(int y) const { return px.data() + size_t(y) * size_t(w); }
};

// A name-table entry's pattern bytes, hashed: 32 bytes in 4-bit mode, 64 in 8-bit, read as the device stores
// them (little-endian 32-bit words, tilegen.cpp's header comment)
uint64_t tile_hash(const uint32_t *vram, uint32_t entry, bool four);
// its palette block, which the entry carries and the pattern does not
inline uint32_t tile_palette(uint32_t entry, bool four) { return four ? (entry & 0x7FF0) : (entry & 0x7F00); }
// a layer's whole name table (2048 words at 0xF8000 + layer * 0x2000), hashed: the identity of a layout
uint64_t layer_hash(const uint32_t *vram, int layer);

// one texel of an entry, as the hardware reads it (bit 15 set: see-through). The rasteriser has its own copies
// of this for the pixels it actually shows; this one is for the dump's and the pack's own pictures, which are
// not on the hot path and must not make the rasteriser carry a function pointer.
inline uint32_t hd_texel(const uint32_t *vram, const uint32_t *pal, uint32_t e, bool four, int fy, int fx)
{
	if (four) {
		const uint32_t pat = (((e & 0x3FFF) << 1) | (e >> 15)) * 8;
		return pal[((e & 0x7FF0) | ((vram[(pat + fy) & 0x3FFFF] >> ((7 - fx) * 4)) & 0xF)) & 0x7FFF];
	}
	const uint32_t pat = (e & 0x3FFF) * 16;
	return pal[((e & 0x7F00) | ((vram[(pat + fy * 2 + (fx >> 2)) & 0x3FFFF] >> ((3 - (fx & 3)) * 8)) & 0xFF)) & 0x7FFF];
}

// A 15-bit hardware colour as RGBA8, with no colour offset applied: the dump's and the pack's pictures are the
// art as the game holds it, not as a fade happens to be showing it
inline uint32_t hd_colour(uint32_t v)
{
	if (v & 0x8000) return 0;
	const uint32_t r = (v & 31) * 255 / 31, g = ((v >> 5) & 31) * 255 / 31, b = ((v >> 10) & 31) * 255 / 31;
	return 0xFF000000u | (b << 16) | (g << 8) | r;
}

// The passive dump: the game is played, and everything the tile generator actually draws is written out once,
// deduplicated by content. Nothing to press, and no period -- that is the convention the owner's Supermodel
// work settled on, and the reason for it is that a tile is only taken if it was really drawn.
class TileDump
{
public:
	// false (with why) if the folders couldn't be made
	bool open(const std::string &dir, std::string &why);
	bool on() const { return open_; }
	// a tile the rasteriser is about to draw
	void tile(const uint32_t *vram, const uint32_t *pal, uint32_t entry, bool four);
	// the four layers' layouts, once a frame, before its lines
	void frame(const uint32_t *vram, const uint32_t *pal, const uint32_t *regs, uint64_t frame);
	// the counts, for the log
	size_t tiles() const { return seen_tiles.size(); }
	size_t layouts() const { return n_layouts; }
	void close();

private:
	bool open_ = false;
	std::string dir;
	std::map<uint64_t, bool> seen_tiles;   // ((hash, palette, bpp) packed -> written)
	// A layout is only written once the SAME name table has stood for two frames. Without that filter the
	// intermediate states are collected while the game is still building the screen -- the owner measured 91
	// layouts instead of 39 over one attract run in Supermodel, and the same filter is kept here.
	uint64_t last_hash[4] = { ~0ull, ~0ull, ~0ull, ~0ull };
	std::map<uint64_t, bool> seen_layers[4];
	size_t n_layouts = 0;
	static constexpr size_t MAX_PER_LAYER = 256;   // (a game rewriting its name table every frame mustn't fill the disk)
	std::vector<std::string> index;   // index.csv's lines, written at the end
	std::vector<std::string> tile_index;
};

// The pack: the images the dump's file names point at, looked up by content. There is no manifest -- the file
// name IS the key, and it is the one the dump wrote. The gesture is: take a file the dump wrote, draw it
// larger, put it back under the same name.
class TilePack
{
public:
	// `dir` holds tiles/ and layers/ as the dump writes them. Every image must be at `scale`: a tile is
	// 8*scale square, a layer image either 512*scale square (the whole tile map -- the one to use for a layer
	// that scrolls) or W*scale by H*scale (the visible screen). Anything else is refused by name. `report` is
	// the line for the log.
	bool load(const std::string &dir, int scale, std::string &report);
	bool empty() const { return tiles.empty() && layers.empty(); }
	// the images, not the hashes: one pattern can be in the pack several times, once per palette
	size_t n_tiles() const { size_t n = 0; for (const auto &[h, v] : tiles) n += v.size(); return n; }
	size_t n_layers() const { size_t n = 0; for (const auto &[h, v] : layers) n += v.size(); return n; }
	// the image for a tile, its own palette first and then one written for any palette (_pANY); null if none
	const HDImage *tile(uint64_t hash, uint32_t pal, bool four) const;
	const HDImage *layer(int n, uint64_t hash) const;

private:
	// Looked up by content hash, and then the few images of that hash are read through for the palette and the
	// depth: the three parts of a tile's identity do not fit in one 64-bit key, and a key that mixed them could
	// collide, which would show the wrong art with nothing to say so.
	struct TileEntry { uint32_t pal; bool four, any; HDImage img; };
	struct LayerEntry { int layer; HDImage img; };
	std::unordered_map<uint64_t, std::vector<TileEntry>> tiles;
	std::unordered_map<uint64_t, std::vector<LayerEntry>> layers;
};

} // namespace video
