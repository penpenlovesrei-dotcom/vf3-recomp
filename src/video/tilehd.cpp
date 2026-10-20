// HD 2D: the hashes, the passive dump and the pack. See tilehd.h and docs/fv2-hd-2d.md.
//
// The PNG reader and writer here are the port's own, over the zlib the build already links (-lz in the
// Makefile): src/host/png.h writes RGB only, and nothing in the tree could read a PNG back. Written are
// 8-bit RGBA, read are 8-bit RGB, RGBA, greyscale, grey+alpha and palettised, not interlaced -- which is what
// GIMP and the other editors write by default.
#include "tilehd.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <zlib.h>

namespace video {

// the screen the tile generator fills (tilegen.h: the hardware fills 496 columns and not one more)
static constexpr int SCREEN_W = 496, SCREEN_H = 384;

uint64_t tile_hash(const uint32_t *vram, uint32_t entry, bool four)
{
	// FNV-1a over the pattern's words, low byte first: our own name, so only the dump and the pack need agree
	uint64_t h = 0xcbf29ce484222325ull;
	const uint32_t at = four ? (((entry & 0x3FFF) << 1) | (entry >> 15)) * 8 : (entry & 0x3FFF) * 16;
	const int n = four ? 8 : 16;
	for (int i = 0; i < n; i++) {
		const uint32_t w = vram[(at + uint32_t(i)) & 0x3FFFF];
		for (int sh = 0; sh < 32; sh += 8) h = (h ^ ((w >> sh) & 0xFF)) * 0x100000001b3ull;
	}
	return h;
}

uint64_t layer_hash(const uint32_t *vram, int layer)
{
	uint64_t h = 0xcbf29ce484222325ull;
	const uint32_t *names = vram + (0xF8000 + uint32_t(layer) * 0x2000) / 4;
	for (int i = 0; i < 2048; i++) {
		const uint32_t w = names[i];
		for (int sh = 0; sh < 32; sh += 8) h = (h ^ ((w >> sh) & 0xFF)) * 0x100000001b3ull;
	}
	return h;
}

namespace {

std::string hex16(uint64_t v)
{
	char b[17];
	snprintf(b, sizeof b, "%016llx", (unsigned long long)v);
	return b;
}

std::string hex4(uint32_t v)
{
	char b[8];
	snprintf(b, sizeof b, "%04x", v & 0xFFFF);
	return b;
}

// ---- PNG, written: 8-bit RGBA, one IDAT ----

bool write_rgba(const std::string &path, const uint32_t *rgba, int w, int h)
{
	std::vector<uint8_t> raw;
	raw.reserve(size_t(h) * (size_t(w) * 4 + 1));
	for (int y = 0; y < h; y++) {
		raw.push_back(0);   // filter: none (the pictures are small and the owner opens them in an editor)
		for (int x = 0; x < w; x++) {
			const uint32_t p = rgba[size_t(y) * size_t(w) + size_t(x)];
			raw.push_back(uint8_t(p)); raw.push_back(uint8_t(p >> 8)); raw.push_back(uint8_t(p >> 16)); raw.push_back(uint8_t(p >> 24));
		}
	}
	uLongf zlen = compressBound(uLong(raw.size()));
	std::vector<uint8_t> z(zlen);
	if (compress2(z.data(), &zlen, raw.data(), uLong(raw.size()), 6) != Z_OK) return false;
	FILE *f = fopen(path.c_str(), "wb");
	if (!f) return false;
	auto be32 = [&](uint32_t v) { const uint8_t b[4] = { uint8_t(v >> 24), uint8_t(v >> 16), uint8_t(v >> 8), uint8_t(v) }; fwrite(b, 1, 4, f); };
	auto chunk = [&](const char *type, const uint8_t *data, uint32_t len) {
		be32(len);
		uLong crc = crc32(0, (const Bytef *)type, 4);
		fwrite(type, 1, 4, f);
		if (len) { fwrite(data, 1, len, f); crc = crc32(crc, data, len); }
		be32(uint32_t(crc));
	};
	static const uint8_t sig[8] = { 0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n' };
	fwrite(sig, 1, 8, f);
	const uint8_t ihdr[13] = { uint8_t(w >> 24), uint8_t(w >> 16), uint8_t(w >> 8), uint8_t(w),
		uint8_t(h >> 24), uint8_t(h >> 16), uint8_t(h >> 8), uint8_t(h), 8, 6, 0, 0, 0 };
	chunk("IHDR", ihdr, 13);
	chunk("IDAT", z.data(), uint32_t(zlen));
	chunk("IEND", nullptr, 0);
	const bool ok = fclose(f) == 0;
	return ok;
}

// ---- PNG, read ----

uint32_t be32_at(const uint8_t *p) { return uint32_t(p[0]) << 24 | uint32_t(p[1]) << 16 | uint32_t(p[2]) << 8 | p[3]; }

// why: empty on success
bool read_rgba(const std::string &path, HDImage &out, std::string &why)
{
	FILE *f = fopen(path.c_str(), "rb");
	if (!f) { why = "couldn't be opened"; return false; }
	std::vector<uint8_t> file;
	uint8_t buf[65536];
	for (size_t n; (n = fread(buf, 1, sizeof buf, f)) > 0;) file.insert(file.end(), buf, buf + n);
	fclose(f);
	static const uint8_t sig[8] = { 0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n' };
	if (file.size() < 8 + 25 || memcmp(file.data(), sig, 8) != 0) { why = "isn't a PNG"; return false; }
	int w = 0, h = 0, depth = 0, type = -1, interlace = 0;
	std::vector<uint8_t> idat, plte, trns;
	for (size_t i = 8; i + 12 <= file.size();) {
		const uint32_t len = be32_at(&file[i]);
		if (len > file.size() || i + 12 + len > file.size()) break;
		const char *t = reinterpret_cast<const char *>(&file[i + 4]);
		const uint8_t *d = &file[i + 8];
		if (!memcmp(t, "IHDR", 4) && len >= 13) {
			w = int(be32_at(d)); h = int(be32_at(d + 4));
			depth = d[8]; type = d[9]; interlace = d[12];
		} else if (!memcmp(t, "IDAT", 4)) idat.insert(idat.end(), d, d + len);
		else if (!memcmp(t, "PLTE", 4)) plte.assign(d, d + len);
		else if (!memcmp(t, "tRNS", 4)) trns.assign(d, d + len);
		else if (!memcmp(t, "IEND", 4)) break;
		i += 12 + len;
	}
	if (w <= 0 || h <= 0 || w > 16384 || h > 16384) { why = "has no size"; return false; }
	if (depth != 8 || interlace != 0 || (type != 0 && type != 2 && type != 3 && type != 4 && type != 6)) {
		why = "isn't an 8-bit-per-channel PNG without interlacing (RGB, RGBA, grey, grey+alpha or palettised)";
		return false;
	}
	if (type == 3 && plte.size() < 3) { why = "is palettised with no palette"; return false; }
	const int ch = type == 0 ? 1 : type == 2 ? 3 : type == 3 ? 1 : type == 4 ? 2 : 4;
	const size_t stride = size_t(w) * size_t(ch);
	std::vector<uint8_t> raw(size_t(h) * (stride + 1));
	uLongf got = uLongf(raw.size());
	if (uncompress(raw.data(), &got, idat.data(), uLong(idat.size())) != Z_OK || got != raw.size()) {
		why = "couldn't be unpacked (truncated?)";
		return false;
	}
	// undo the per-row filters, in place over the rows' pixel bytes
	std::vector<uint8_t> px(size_t(h) * stride);
	for (int y = 0; y < h; y++) {
		const uint8_t fl = raw[size_t(y) * (stride + 1)];
		const uint8_t *src = &raw[size_t(y) * (stride + 1) + 1];
		uint8_t *dst = &px[size_t(y) * stride];
		const uint8_t *up = y ? &px[size_t(y - 1) * stride] : nullptr;
		for (size_t x = 0; x < stride; x++) {
			const int a = x >= size_t(ch) ? dst[x - size_t(ch)] : 0;
			const int b = up ? up[x] : 0;
			const int c = (up && x >= size_t(ch)) ? up[x - size_t(ch)] : 0;
			int v = src[x];
			switch (fl) {
			case 0: break;
			case 1: v += a; break;
			case 2: v += b; break;
			case 3: v += (a + b) / 2; break;
			case 4: {
				const int p = a + b - c;
				const int pa = std::abs(p - a), pb = std::abs(p - b), pc = std::abs(p - c);
				v += (pa <= pb && pa <= pc) ? a : (pb <= pc ? b : c);
				break;
			}
			default: why = "uses a row filter that isn't in the PNG standard"; return false;
			}
			dst[x] = uint8_t(v);
		}
	}
	out.w = w; out.h = h;
	out.px.assign(size_t(w) * size_t(h), 0);
	for (int y = 0; y < h; y++)
		for (int x = 0; x < w; x++) {
			const uint8_t *p = &px[size_t(y) * stride + size_t(x) * size_t(ch)];
			uint32_t r, g, b, a = 255;
			if (type == 0) { r = g = b = p[0]; }
			else if (type == 4) { r = g = b = p[0]; a = p[1]; }
			else if (type == 3) {
				const size_t at = size_t(p[0]) * 3;
				if (at + 2 >= plte.size()) { r = g = b = 0; }
				else { r = plte[at]; g = plte[at + 1]; b = plte[at + 2]; }
				if (size_t(p[0]) < trns.size()) a = trns[p[0]];
			} else { r = p[0]; g = p[1]; b = p[2]; if (ch == 4) a = p[3]; }
			out.px[size_t(y) * size_t(w) + size_t(x)] = a << 24 | b << 16 | g << 8 | r;
		}
	why.clear();
	return true;
}

} // namespace

// ---- the dump ----

bool TileDump::open(const std::string &d, std::string &why)
{
	std::error_code ec;
	std::filesystem::create_directories(std::filesystem::path(d) / "tiles", ec);
	if (ec) { why = ec.message(); return false; }
	std::filesystem::create_directories(std::filesystem::path(d) / "layers", ec);
	if (ec) { why = ec.message(); return false; }
	dir = d;
	open_ = true;
	why.clear();
	return true;
}

void TileDump::tile(const uint32_t *vram, const uint32_t *pal, uint32_t entry, bool four)
{
	if (!open_) return;
	const uint64_t hash = tile_hash(vram, entry, four);
	const uint32_t p = tile_palette(entry, four);
	const uint64_t key = hash * 1000003ull + (uint64_t(p) << 1) + (four ? 1 : 0);
	if (seen_tiles.count(key)) return;
	seen_tiles[key] = true;
	const std::string name = hex16(hash) + "_p" + hex4(p) + "_" + (four ? "4" : "8") + "bpp.png";
	uint32_t px[64];
	int filled = 0;
	for (int fy = 0; fy < 8; fy++)
		for (int fx = 0; fx < 8; fx++) {
			const uint32_t c = hd_colour(hd_texel(vram, pal, entry, four, fy, fx));
			px[fy * 8 + fx] = c;
			filled += (c >> 24) != 0;
		}
	write_rgba(dir + "/tiles/" + name, px, 8, 8);
	tile_index.push_back(name + "," + hex16(hash) + "," + hex4(p) + "," + (four ? "4" : "8") + "," + std::to_string(filled));
}

void TileDump::frame(const uint32_t *vram, const uint32_t *pal, const uint32_t *regs, uint64_t frame)
{
	if (!open_) return;
	for (int layer = 0; layer < 4; layer++) {
		if (!(regs[0x60 / 4 + layer] & 0x80000000u)) { last_hash[layer] = ~0ull; continue; }
		const uint64_t h = layer_hash(vram, layer);
		const uint64_t was = last_hash[layer];
		last_hash[layer] = h;
		if (was != h) continue;   // the stability filter: the same name table two frames running, or nothing
		if (seen_layers[layer].count(h)) continue;
		if (seen_layers[layer].size() >= MAX_PER_LAYER) continue;
		seen_layers[layer][h] = true;
		const bool four = (regs[0x20 / 4] >> (12 + layer)) & 1;
		const uint32_t *names = vram + (0xF8000 + uint32_t(layer) * 0x2000) / 4;
		// the whole tile map, 64 by 64 tiles: not the visible window, or a layer that scrolls would be cut
		std::vector<uint32_t> img(512 * 512, 0);
		int filled = 0;
		std::vector<uint64_t> cell(size_t(64) * 64);
		std::map<uint64_t, std::string> legend;
		for (int row = 0; row < 64; row++)
			for (int col = 0; col < 64; col++) {
				const uint32_t word = names[row * 32 + (col >> 1)];
				const uint32_t e = (col & 1) ? (word & 0xFFFF) : (word >> 16);
				const uint64_t th = tile_hash(vram, e, four);
				cell[size_t(row) * 64 + size_t(col)] = th;
				legend[th] = hex16(th) + "_p" + hex4(tile_palette(e, four)) + "_" + (four ? "4" : "8") + "bpp.png";
				bool any = false;
				for (int fy = 0; fy < 8; fy++)
					for (int fx = 0; fx < 8; fx++) {
						const uint32_t c = hd_colour(hd_texel(vram, pal, e, four, fy, fx));
						img[size_t(row * 8 + fy) * 512 + size_t(col * 8 + fx)] = c;
						any = any || (c >> 24) != 0;
					}
				filled += any;
			}
		if (!filled) continue;   // an empty layer isn't written: the unused ones would be megabytes of nothing
		const std::string base = "L" + std::to_string(layer) + "_" + hex16(h);
		write_rgba(dir + "/layers/" + base + ".png", img.data(), 512, 512);
		// the grid, for finding the file of something seen on screen: cell (x, y) of the map is at (x*8, y*8)
		// pixels in the picture above, and the legend at the end names its tile's file exactly
		if (FILE *t = fopen((dir + "/layers/" + base + ".txt").c_str(), "w")) {
			fprintf(t, "# layer %d, %s, name-table hash %s, first seen on frame %llu\n",
			        layer, four ? "4bpp" : "8bpp", hex16(h).c_str(), (unsigned long long)frame);
			fprintf(t, "# registers: 0x20=%08x 0x40=%08x 0x44=%08x 0x%02x=%08x (scroll), filled cells %d of 4096\n",
			        regs[0x20 / 4], regs[0x40 / 4], regs[0x44 / 4], 0x60 + 4 * layer, regs[0x60 / 4 + layer], filled);
			fprintf(t, "# the grid: 64 columns by 64 rows, the first 4 hex digits of each cell's tile hash\n");
			for (int row = 0; row < 64; row++) {
				fprintf(t, "%2d:", row);
				for (int col = 0; col < 64; col++) fprintf(t, " %04llx", (unsigned long long)(cell[size_t(row) * 64 + size_t(col)] >> 48));
				fprintf(t, "\n");
			}
			fprintf(t, "# legend: short hash -> the tile's file in tiles/\n");
			for (const auto &[th, nm] : legend) fprintf(t, "%04llx %s\n", (unsigned long long)(th >> 48), nm.c_str());
			fclose(t);
		}
		index.push_back(base + ".png," + std::to_string(layer) + "," + hex16(h) + "," + std::to_string(filled) + "," + std::to_string(frame));
		n_layouts++;
	}
}

void TileDump::close()
{
	if (!open_) return;
	// index.csv, sorted by filled_cells: the busiest screens first, which is where to start looking
	std::vector<std::string> lines = index;
	std::sort(lines.begin(), lines.end(), [](const std::string &a, const std::string &b) {
		auto cells = [](const std::string &s) {
			size_t at = 0;
			for (int i = 0; i < 3; i++) at = s.find(',', at) + 1;
			return atol(s.c_str() + at);
		};
		return cells(a) > cells(b);
	});
	if (FILE *f = fopen((dir + "/layers/index.csv").c_str(), "w")) {
		fprintf(f, "file,layer,name_table_hash,filled_cells,first_frame\n");
		for (const std::string &l : lines) fprintf(f, "%s\n", l.c_str());
		fclose(f);
	}
	if (FILE *f = fopen((dir + "/tiles.csv").c_str(), "w")) {
		fprintf(f, "file,pattern_hash,palette,bpp,texels_drawn\n");
		for (const std::string &l : tile_index) fprintf(f, "%s\n", l.c_str());
		fclose(f);
	}
	open_ = false;
}

// ---- the pack ----

bool TilePack::load(const std::string &dir, int scale, std::string &report)
{
	tiles.clear();
	layers.clear();
	size_t rejected = 0;
	std::string why;
	std::error_code ec;
	const std::filesystem::path tdir = std::filesystem::path(dir) / "tiles", ldir = std::filesystem::path(dir) / "layers";
	auto ext_png = [](const std::filesystem::path &p) {
		std::string e = p.extension().string();
		for (char &c : e) c = char(tolower(c));
		return e == ".png";
	};
	for (const auto &de : std::filesystem::directory_iterator(tdir, ec)) {
		if (!de.is_regular_file() || !ext_png(de.path())) continue;
		// <hash16>_p<pal4|ANY>_<4|8>bpp.png, as the dump writes it
		const std::string stem = de.path().stem().string();
		char hs[64] = {}, ps[64] = {};
		int bpp = 0;
		if (sscanf(stem.c_str(), "%16[0-9a-fA-F]_p%15[0-9a-fA-FnyNY]_%dbpp", hs, ps, &bpp) != 3 || (bpp != 4 && bpp != 8)) {
			report = "tiles/" + de.path().filename().string() + ": the name isn't <hash>_p<palette>_<4|8>bpp.png as the dump writes it";
			rejected++;
			continue;
		}
		HDImage img;
		if (!read_rgba(de.path().string(), img, why)) {
			report = "tiles/" + de.path().filename().string() + ": " + why;
			rejected++;
			continue;
		}
		if (img.w != 8 * scale || img.h != 8 * scale) {
			char b[192];
			snprintf(b, sizeof b, "tiles/%s: %dx%d, but a tile at scale %d is %dx%d",
			         de.path().filename().string().c_str(), img.w, img.h, scale, 8 * scale, 8 * scale);
			report = b;
			rejected++;
			continue;
		}
		const bool any = ps[0] == 'A' || ps[0] == 'a';
		TileEntry te{ any ? 0u : uint32_t(strtoul(ps, nullptr, 16)), bpp == 4, any, std::move(img) };
		tiles[strtoull(hs, nullptr, 16)].push_back(std::move(te));
	}
	for (const auto &de : std::filesystem::directory_iterator(ldir, ec)) {
		if (!de.is_regular_file() || !ext_png(de.path())) continue;
		const std::string stem = de.path().stem().string();
		char hs[64] = {};
		int layer = -1;
		if (sscanf(stem.c_str(), "L%d_%16[0-9a-fA-F]", &layer, hs) != 2 || layer < 0 || layer > 3) {
			report = "layers/" + de.path().filename().string() + ": the name isn't L<0-3>_<hash>.png as the dump writes it";
			rejected++;
			continue;
		}
		HDImage img;
		if (!read_rgba(de.path().string(), img, why)) {
			report = "layers/" + de.path().filename().string() + ": " + why;
			rejected++;
			continue;
		}
		// Two shapes. The whole tile map (512 tiles square) scrolls and wraps as the hardware's does, and is the
		// one for a layer that moves; the visible screen is the handy one for a screen that stands still, and it
		// does not scroll at all.
		if (img.w == 512 * scale && img.h == 512 * scale) img.screen = false;
		else if (img.w == SCREEN_W * scale && img.h == SCREEN_H * scale) img.screen = true;
		else {
			char b[256];
			snprintf(b, sizeof b, "layers/%s: %dx%d, but a layer at scale %d is %dx%d (the whole tile map) or %dx%d (the visible screen)",
			         de.path().filename().string().c_str(), img.w, img.h, scale, 512 * scale, 512 * scale, SCREEN_W * scale, SCREEN_H * scale);
			report = b;
			rejected++;
			continue;
		}
		layers[strtoull(hs, nullptr, 16)].push_back(LayerEntry{ layer, std::move(img) });
	}
	char b[320];
	snprintf(b, sizeof b, "tile pack: %zu tiles and %zu layers from %s at scale %d, %zu refused%s%s",
	         n_tiles(), n_layers(), dir.c_str(), scale, rejected, rejected ? " -- first: " : "", rejected ? report.c_str() : "");
	report = b;
	return !empty();
}

const HDImage *TilePack::tile(uint64_t hash, uint32_t pal, bool four) const
{
	auto it = tiles.find(hash);
	if (it == tiles.end()) return nullptr;
	const HDImage *fallback = nullptr;
	for (const TileEntry &e : it->second) {
		if (e.four != four) continue;
		if (e.any) { fallback = &e.img; continue; }
		if (e.pal == pal) return &e.img;
	}
	return fallback;   // _pANY: the same art whatever the colours (the owner's convention)
}

const HDImage *TilePack::layer(int n, uint64_t hash) const
{
	auto it = layers.find(hash);
	if (it == layers.end()) return nullptr;
	for (const LayerEntry &e : it->second)
		if (e.layer == n) return &e.img;
	return nullptr;
}

} // namespace video
