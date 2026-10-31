// The Real3D's textures replaced from a pack. See tex3d_hd.h, which says where the grip is in each rasteriser and
// why the replacements live in extra pages of texture memory.
#include "tex3d_hd.h"
#include "../host/png.h"   // (write_png / read_png: the only PNG code in the tree, used by the headless runner too)
#include <algorithm>
#include <cstdio>
#include <cmath>
#include <cstring>
#include <filesystem>

namespace video {

namespace {

constexpr int PAGE_W = 2048, PAGE_H = 1024;
constexpr int MAX_PAGES = 16;   // 16 pages of 4 MiB of 16-bit texels: the ceiling Supermodel's 4096x4096 atlas has

// FNV-1a 64 over the texels of a texture's level 0, in the order the texture memory holds them. Shared by the
// dump (which names the files) and the pack (which looks them up), so the two cannot drift apart.
uint64_t hash_texels(const uint16_t *mem, int page, int x, int y, int w, int h, bool &flat)
{
	uint64_t k = 0xCBF29CE484222325ull;
	const uint16_t first = mem[size_t(page * PAGE_H + (y & (PAGE_H - 1))) * PAGE_W + (x & (PAGE_W - 1))];
	flat = true;
	for (int j = 0; j < h; j++) {
		const uint16_t *row = mem + size_t(page * PAGE_H + ((y + j) & (PAGE_H - 1))) * PAGE_W;
		for (int i = 0; i < w; i++) {
			const uint16_t v = row[(x + i) & (PAGE_W - 1)];
			if (v != first) flat = false;
			k = (k ^ uint64_t(v & 0xFF)) * 0x100000001B3ull;
			k = (k ^ uint64_t(v >> 8)) * 0x100000001B3ull;
		}
	}
	return k;
}

// The twelve formats the Real3D has, as both rasterisers decode them (r3d_raster.cpp's decode and r3d.hlsl's;
// this is the third copy and must stay the same as them, or the dump stops showing what the screen shows)
void decode_rgba(int type, uint32_t v, uint8_t out[4])
{
	float r = 0, g = 0, b = 0, a = 0;
	auto grey = [&](float y, float al) { r = g = b = y; a = al; };
	switch (type) {
	case 0:
		r = ((v >> 10) & 31) / 31.0f; g = ((v >> 5) & 31) / 31.0f; b = (v & 31) / 31.0f;
		a = 1.0f - float((v >> 15) & 1);
		break;
	case 1: grey((v & 15) / 15.0f, ((v >> 4) & 15) / 15.0f); break;
	case 2: grey(((v >> 4) & 15) / 15.0f, (v & 15) / 15.0f); break;
	case 3: grey(((v >> 8) & 15) / 15.0f, ((v >> 12) & 15) / 15.0f); break;
	case 4: grey(((v >> 12) & 15) / 15.0f, ((v >> 8) & 15) / 15.0f); break;
	case 5: { unsigned t = v & 0xFF; grey(t / 255.0f, t == 255 ? 0.0f : 1.0f); break; }
	case 6: { unsigned t = (v >> 8) & 0xFF; grey(t / 255.0f, t == 255 ? 0.0f : 1.0f); break; }
	case 7:
		r = ((v >> 12) & 15) / 15.0f; g = ((v >> 8) & 15) / 15.0f; b = ((v >> 4) & 15) / 15.0f; a = (v & 15) / 15.0f;
		break;
	case 8: case 9: case 10: case 11: {
		unsigned n = (v >> ((type - 8) * 4)) & 15;
		grey(n / 15.0f, n == 15 ? 0.0f : 1.0f);
		break;
	}
	default: break;
	}
	auto q = [](float c) { return uint8_t(std::lround(std::clamp(c, 0.0f, 1.0f) * 255.0f)); };
	out[0] = q(r); out[1] = q(g); out[2] = q(b); out[3] = q(a);
}

// Which format a replacement is written in: the pages are the hardware's 16-bit texels, so a replacement has to
// be one of the formats the rasterisers decode. The two that carry colour are taken: RGBA4 (7) when the texture
// it stands in for has graded alpha, T1RGB5 (0) -- five bits a channel and one bit of transparency -- otherwise,
// which is what the originals of those formats carry themselves.
int replacement_format(int native_format)
{
	switch (native_format) {
	case 1: case 2: case 3: case 4: case 7: return 7;
	default: return 0;
	}
}

uint16_t pack_texel(int format, const uint8_t rgba[4])
{
	auto to = [](uint8_t v, int bits) { const int m = (1 << bits) - 1; return uint16_t((v * m + 127) / 255); };
	if (format == 7)
		return uint16_t(to(rgba[0], 4) << 12 | to(rgba[1], 4) << 8 | to(rgba[2], 4) << 4 | to(rgba[3], 4));
	return uint16_t((rgba[3] < 128 ? 0x8000 : 0) | to(rgba[0], 5) << 10 | to(rgba[1], 5) << 5 | to(rgba[2], 5));
}

// <hash>_<w>x<h>_f<format>.png, which is what the dump writes
bool parse_name(const std::string &name, uint64_t &hash, int &w, int &h, int &format)
{
	unsigned long long hv = 0;
	int a = 0, b = 0, f = 0, n = 0;
	if (sscanf(name.c_str(), "%llx_%dx%d_f%d.png%n", &hv, &a, &b, &f, &n) != 4 || n != int(name.size()))
		return false;
	if (a <= 0 || b <= 0 || f < 0 || f > 11)
		return false;
	hash = hv; w = a; h = b; format = f;
	return true;
}

} // namespace

// One image of the pack, read and waiting to be laid in a page
struct Tex3dHd::Pending
{
	Key key = {};
	std::vector<uint8_t> rgba;
	int w = 0, h = 0;
	int variant = -1;
	std::string name;
};

Tex3dHd &hd3d()
{
	static Tex3dHd one;
	return one;
}

bool Tex3dHd::dump_to(const std::string &dir, std::string &error)
{
	std::error_code ec;
	std::filesystem::create_directories(dir, ec);
	if (!std::filesystem::is_directory(dir, ec)) {
		error = "the 3D texture dump can't make " + dir;
		return false;
	}
	dump_dir = dir;
	dump_on = true;
	// textures.csv is added to, never started again: two sessions of capture then describe every PNG in the
	// folder, which is what the images themselves do (Supermodel's rewrote it and lost the first session's).
	const std::string csv = dir + "/textures.csv";
	const bool fresh = !std::filesystem::exists(csv, ec);
	if (FILE *f = fopen(csv.c_str(), "a")) {
		if (fresh) fprintf(f, "name,hash,width,height,format,page,x,y,alpha\n");
		fclose(f);
	}
	return true;
}

void Tex3dHd::want_variant(const std::string &rel)
{
	if (rel.empty() || variant_index(rel) >= 0) return;
	Variant v;
	v.rel = rel;
	variants.push_back(std::move(v));
}

int Tex3dHd::variant_index(const std::string &rel) const
{
	for (size_t i = 0; i < variants.size(); i++)
		if (variants[i].rel == rel) return int(i);
	return -1;
}

bool Tex3dHd::variant_found(const std::string &rel) const
{
	const int i = variant_index(rel);
	return i >= 0 && variants[size_t(i)].found;
}

void Tex3dHd::use_variant(const std::string &rel, bool on)
{
	const int i = variant_index(rel);
	if (i < 0 || variants[size_t(i)].on == on) return;
	variants[size_t(i)].on = on;
	seen.clear();   // (what find() worked out for the picture before, it worked out with the other set on)
}

// One folder read: the PNGs in it, nothing below it. variant is -1 for the pack's root.
void Tex3dHd::scan(const std::string &dir, int variant, std::vector<Pending> &pending)
{
	std::error_code ec;
	if (!std::filesystem::is_directory(dir, ec)) return;
	if (variant >= 0) variants[size_t(variant)].found = true;
	std::vector<std::filesystem::path> files;
	for (const auto &e : std::filesystem::directory_iterator(dir, ec))
		if (e.is_regular_file(ec) && e.path().extension() == ".png")
			files.push_back(e.path());
	std::sort(files.begin(), files.end());   // (a folder's order is the filesystem's: laid out the same way every run)
	for (const auto &f : files) {
		const std::string name = f.filename().string();
		// The reader here is the one the portrait folders use (src/host/png.h). This file came with one of its
		// own; they met in a merge and the better of the two stayed -- it takes the forms an editor writes and
		// says plainly why it refuses a file instead of guessing. It hands back packed words with red in the
		// low byte, while place() below wants four bytes a pixel, so they are spread out rather than leaning on
		// the host's byte order.
		std::vector<uint32_t> px;
		std::string why;
		Pending pd;
		pd.variant = variant;
		if (!parse_name(name, pd.key.hash, pd.key.w, pd.key.h, pd.key.format)) {
			fprintf(stderr, "3D texture pack: %s is not named <hash>_<width>x<height>_f<format>.png: left out\n", name.c_str());
			st.rejected++;
			continue;
		}
		// A set's image is named by its set as well, so a line in the log says which of the two was drawn
		pd.name = variant >= 0 ? variants[size_t(variant)].rel + "/" + name : name;
		if (!read_png(f.string().c_str(), px, pd.w, pd.h, why)) {
			fprintf(stderr, "3D texture pack: %s %s: left out\n", pd.name.c_str(), why.c_str());
			st.rejected++;
			continue;
		}
		pd.rgba.resize(px.size() * 4);
		for (size_t i = 0; i < px.size(); i++) {
			const uint32_t v = px[i];
			pd.rgba[i * 4] = uint8_t(v);
			pd.rgba[i * 4 + 1] = uint8_t(v >> 8);
			pd.rgba[i * 4 + 2] = uint8_t(v >> 16);
			pd.rgba[i * 4 + 3] = uint8_t(v >> 24);
		}
		if (pd.w > PAGE_W || pd.h > PAGE_H) {
			fprintf(stderr, "3D texture pack: %s is %d x %d; a page of texture memory is %d x %d: left out\n",
			        pd.name.c_str(), pd.w, pd.h, PAGE_W, PAGE_H);
			st.rejected++;
			continue;
		}
		pending.push_back(std::move(pd));
	}
}

bool Tex3dHd::load(const std::string &dir, std::string &report, bool with_root)
{
	entries.clear();
	seen.clear();
	hd.clear();
	shelf = {};
	st = {};
	for (Variant &v : variants) { v.found = v.on = false; }
	std::error_code ec;
	if (!std::filesystem::is_directory(dir, ec)) {
		report = "no 3D texture pack at " + dir;
		return false;
	}
	std::vector<Pending> pending;
	if (with_root) scan(dir, -1, pending);
	// Only the sets the host asked for. Everything else under the pack folder -- DUMP, a capture put aside, a
	// folder of work in progress -- is none of this file's business, and laying it in pages of texture memory
	// would spend the sixteen there are on images nobody asked to be drawn.
	for (size_t i = 0; i < variants.size(); i++) {
		scan(dir + "/" + variants[i].rel, int(i), pending);
		if (!variants[i].found)
			fprintf(stderr, "3D texture pack: there is no folder %s: that set is not drawn\n", variants[i].rel.c_str());
	}
	// the biggest first, so the shelves pack without gaps
	std::stable_sort(pending.begin(), pending.end(), [](const Pending &a, const Pending &b) { return a.h > b.h; });
	size_t in_sets = 0;
	for (auto &pd : pending) {
		std::vector<Entry> &here = entries[pd.key];
		bool twice = false;
		for (const Entry &e : here) if (e.variant == pd.variant) twice = true;
		if (twice) {
			fprintf(stderr, "3D texture pack: %s is a second file for a texture already loaded: left out\n", pd.name.c_str());
			st.rejected++;
			continue;
		}
		Entry e;
		e.name = pd.name;
		e.variant = pd.variant;
		if (!place(pd.rgba, pd.w, pd.h, replacement_format(pd.key.format), e.place)) {
			fprintf(stderr, "3D texture pack: no room left for %s (%d pages of %d x %d used): left out\n",
			        pd.name.c_str(), page_count(), PAGE_W, PAGE_H);
			st.rejected++;
			continue;
		}
		e.placed = true;
		here.push_back(e);
		st.loaded++;
		if (pd.variant >= 0) in_sets++;
	}
	const std::string sets = in_sets ? " (" + std::to_string(in_sets) + " of them in variant sets)" : std::string();
	char line[200];
	snprintf(line, sizeof(line), "3D texture pack: %zu textures loaded%s, %zu left out, in %d page%s of texture memory, from ",
	         st.loaded, sets.c_str(), st.rejected, page_count(), page_count() == 1 ? "" : "s");
	report = line + dir;   // (the folder last: a long path used to run the rest of the line off the end)
	return st.loaded > 0;
}

bool Tex3dHd::place(const std::vector<uint8_t> &rgba, int w, int h, int format, TexPlace &out)
{
	if (shelf.x + w > PAGE_W) {       // the next shelf up
		shelf.y += shelf.h;
		shelf.x = 0;
		shelf.h = 0;
	}
	if (shelf.y + h > PAGE_H) {       // the next page
		shelf.page++;
		shelf.y = shelf.x = shelf.h = 0;
	}
	if (shelf.page >= MAX_PAGES || h > PAGE_H || w > PAGE_W)
		return false;
	hd.resize(size_t(shelf.page + 1) * PAGE_H * PAGE_W, 0);
	for (int y = 0; y < h; y++) {
		uint16_t *row = hd.data() + size_t(shelf.page * PAGE_H + shelf.y + y) * PAGE_W + shelf.x;
		for (int x = 0; x < w; x++) row[x] = pack_texel(format, &rgba[(size_t(y) * w + x) * 4]);
	}
	out.page = shelf.page;
	out.x = shelf.x;
	out.y = shelf.y;
	out.w = w;
	out.h = h;
	out.format = format;
	out.hd = true;
	shelf.x += w;
	shelf.h = std::max(shelf.h, h);
	return true;
}

bool Tex3dHd::find(const uint16_t *tex, const TexPlace &native, bool translator_map, AlphaUse alpha, TexPlace &out)
{
	if (!tex || native.w <= 0 || native.h <= 0)
		return false;
	// A translator-map polygon reads its texel as an index into the colour its polygon carries (times sixteen),
	// not as a colour: a replacement image would be multiplied out of range. Left alone.
	if (translator_map)
		return false;
	// where the texture is, as one number: x (11 bits), y (10), format (4), page (2), width (12), height (11)
	const uint64_t at = uint64_t(native.x & 0x7FF) | uint64_t(native.y & 0x3FF) << 11 | uint64_t(native.format & 15) << 21 |
		uint64_t(native.page & 3) << 25 | uint64_t(native.w & 0xFFF) << 27 | uint64_t(native.h & 0x7FF) << 39;
	if (auto it = seen.find(at); it != seen.end()) {
		out = it->second.place;
		return it->second.hit;
	}
	Seen s;
	bool flat = false;
	const uint64_t hash = hash_texels(tex, native.page, native.x, native.y, native.w, native.h, flat);
	// A texture of one single texel value is refused by default. Not because every flat texture hashes alike --
	// the hash walks all the texels, so a red square and a blue one differ -- but because a flat square of a
	// given size and colour hashes like ANY flat area of that size and colour, anywhere on the screen: a stage,
	// another fighter. One pack entry would stand in for all of them.
	//
	// `allow_flat` lifts it, and nothing decides that from inside: it is an option, set per run. Kuhn's skin is
	// one flat coloured square, so without it he has no skin to dump and none to replace; with it, whatever flat
	// art the pack carries will also stand in for any other flat area of the same size and colour in that run.
	// That is the whole of the trade, and it is the operator's to make.
	if (flat && !allow_flat) {
		st.flat_refused++;
	} else if (dump_on) {
		dump_one(tex, native, hash, alpha);
	}
	if (!flat || allow_flat) {
		const Key k = { hash, native.w, native.h, native.format };
		if (auto e = entries.find(k); e != entries.end()) {
			// The root's image stands in unless a variant set carrying this texture is switched on, and then
			// that set's image stands in instead (tex3d_hd.h). At most one of them is ever drawn.
			Entry *pick = nullptr;
			for (Entry &c : e->second) {
				if (!c.placed) continue;
				if (c.variant < 0) { if (!pick) pick = &c; continue; }
				if (variants[size_t(c.variant)].on) { pick = &c; break; }
			}
			if (pick) {
				s.hit = true;
				s.place = pick->place;
				st.matched++;
				if (!pick->reported) {
					pick->reported = true;
					fprintf(stderr, "3D texture pack: %s drawn, from page %d at %d,%d\n", pick->name.c_str(),
							native.page, native.x, native.y);
				}
			}
		}
	}
	seen.emplace(at, s);
	out = s.place;
	return s.hit;
}

void Tex3dHd::dump_one(const uint16_t *tex, const TexPlace &native, uint64_t hash, AlphaUse alpha)
{
	static const char *const alpha_name[] = { "opaque", "contour", "graded" };
	char name[64];
	snprintf(name, sizeof(name), "%016llx_%dx%d_f%d.png", (unsigned long long)hash, native.w, native.h, native.format);
	// written once: already here from this run or an earlier one
	if (!dumped_names.insert(name).second) {
		// The same texels drawn again. If the polygon uses their alpha differently this time, the one file on
		// disk cannot carry both, so say so instead of leaving it to be found in the picture.
		if (auto it = dumped_alpha.find(name); it != dumped_alpha.end() && it->second != alpha) {
			fprintf(stderr, "3D texture dump: %s drawn both %s and %s; the file keeps %s\n", name,
					alpha_name[int(it->second)], alpha_name[int(alpha)], alpha_name[int(it->second)]);
			it->second = alpha;   // said once per change, not once per picture
		}
		return;
	}
	dumped_alpha.emplace(name, alpha);
	const std::string path = dump_dir + "/" + name;
	std::error_code ec;
	if (!std::filesystem::exists(path, ec)) {
		std::vector<uint32_t> px(size_t(native.w) * native.h);
		for (int y = 0; y < native.h; y++) {
			const uint16_t *row = tex + size_t(native.page * PAGE_H + ((native.y + y) & (PAGE_H - 1))) * PAGE_W;
			for (int x = 0; x < native.w; x++) {
				uint8_t c[4];
				decode_rgba(native.format, row[(native.x + x) & (PAGE_W - 1)], c);
				// The polygon's use of the alpha, not the texel's: see the header. Opaque is the common case and
				// the one that was wrong -- a texel's top bit that no polygon reads is not transparency.
				switch (alpha) {
				case AlphaUse::Opaque:  c[3] = 0xFF; break;
				case AlphaUse::Contour: c[3] = c[3] < 32 ? 0x00 : 0xFF; break;   // the rasteriser's own threshold
				case AlphaUse::Graded:  break;
				}
				px[size_t(y) * native.w + x] = uint32_t(c[0]) | uint32_t(c[1]) << 8 | uint32_t(c[2]) << 16 | uint32_t(c[3]) << 24;
			}
		}
		write_png_rgba(path.c_str(), px.data(), native.w, native.h);
	}
	st.dumped++;
	if (FILE *f = fopen((dump_dir + "/textures.csv").c_str(), "a")) {
		fprintf(f, "%s,%016llx,%d,%d,%d,%d,%d,%d,%s\n", name, (unsigned long long)hash, native.w, native.h,
				native.format, native.page, native.x, native.y, alpha_name[int(alpha)]);
		fclose(f);
	}
}

} // namespace video
