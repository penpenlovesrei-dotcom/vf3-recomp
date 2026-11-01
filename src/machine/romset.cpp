#include "romset.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <zlib.h>

namespace romset {

namespace {

// ---- a minimal zip reader: the central directory, stored and deflated entries (as VF2's) ----

struct entry { std::string name; uint32_t crc = 0, csize = 0, usize = 0, local = 0; uint16_t method = 0; };

uint16_t le16(const uint8_t *p) { return uint16_t(p[0] | p[1] << 8); }
uint32_t le32(const uint8_t *p) { return uint32_t(p[0] | p[1] << 8 | p[2] << 16 | uint32_t(p[3]) << 24); }

struct zip
{
	std::vector<uint8_t> data;
	std::vector<entry> entries;

	// listing: only the directory (the names and CRCs), not the ROMs: the zip's last 1 MB read, where the directory is
	// (39 names: a few KB); a zip whose directory starts before that is read whole
	bool open(const std::string &path, bool listing = false)
	{
		// vf3.zip is about 112 MB and fvipers2.zip about 96 MB: anything over 512 MB isn't a set, and isn't read
		std::error_code ec;
		const uintmax_t size = std::filesystem::file_size(path, ec);
		if (ec || size < 22 || size > (512u << 20)) return false;
		FILE *f = fopen(path.c_str(), "rb");
		if (!f) return false;
		size_t from = listing && size > (1u << 20) ? size_t(size) - (1u << 20) : 0;
		data.assign(size_t(size), 0);
		const bool got = fseek(f, long(from), SEEK_SET) == 0 && fread(data.data() + from, 1, data.size() - from, f) == data.size() - from;
		if (!got) { fclose(f); return false; }
		const long n = long(size);
		long eocd = -1;
		for (long i = n - 22; i >= long(from) && i >= n - 22 - 65535; i--)
			if (le32(&data[i]) == 0x06054b50) { eocd = i; break; }
		if (eocd >= 0 && from > 0 && le32(&data[eocd + 16]) < from) {   // (the directory further back: the rest read too)
			const bool rest = fseek(f, 0, SEEK_SET) == 0 && fread(data.data(), 1, from, f) == from;
			if (!rest) { fclose(f); return false; }
		}
		fclose(f);
		eocd = -1;
		for (long i = n - 22; i >= 0 && i >= n - 22 - 65535; i--)
			if (le32(&data[i]) == 0x06054b50) { eocd = i; break; }
		if (eocd < 0) return false;
		const uint16_t count = le16(&data[eocd + 10]);
		size_t at = le32(&data[eocd + 16]);
		for (int k = 0; k < count; k++) {
			if (at + 46 > data.size() || le32(&data[at]) != 0x02014b50) return false;
			entry e;
			e.method = le16(&data[at + 10]);
			e.crc = le32(&data[at + 16]);
			e.csize = le32(&data[at + 20]);
			e.usize = le32(&data[at + 24]);
			const uint16_t nlen = le16(&data[at + 28]), xlen = le16(&data[at + 30]), clen = le16(&data[at + 32]);
			e.local = le32(&data[at + 42]);
			if (at + 46 + nlen > data.size()) return false;
			e.name.assign(reinterpret_cast<const char *>(&data[at + 46]), nlen);
			const size_t slash = e.name.find_last_of('/');
			if (slash != std::string::npos) e.name = e.name.substr(slash + 1);
			entries.push_back(e);
			at += 46 + nlen + xlen + clen;
		}
		return true;
	}

	bool read(const entry &e, std::vector<uint8_t> &out) const
	{
		const size_t local = e.local;
		if (local + 30 > data.size() || le32(&data[local]) != 0x04034b50) return false;
		const size_t start = local + 30 + le16(&data[local + 26]) + le16(&data[local + 28]);
		if (start + size_t(e.csize) > data.size() || e.usize > (64u << 20)) return false;
		out.resize(e.usize);
		if (e.method == 0) {
			if (e.csize != e.usize) return false;
			memcpy(out.data(), &data[start], e.usize);
		} else if (e.method == 8) {
			z_stream z{};
			if (inflateInit2(&z, -MAX_WBITS) != Z_OK) return false;
			z.next_in = const_cast<Bytef *>(&data[start]);
			z.avail_in = e.csize;
			z.next_out = out.data();
			z.avail_out = e.usize;
			const int r = inflate(&z, Z_FINISH);
			inflateEnd(&z);
			if (r != Z_STREAM_END || z.total_out != e.usize) return false;
		} else {
			return false;
		}
		return uint32_t(crc32(0, out.data(), uInt(out.size()))) == e.crc;
	}
};

// The sets, their ROM tables and what board each is: all of it is the programs' profiles now
// (src/machine/profile.h). This file lays out whatever profile it is handed.

enum region_id { CROM = m3::ROM_CROM, BANKED = m3::ROM_BANKED, VROM = m3::ROM_VROM, SOUND = m3::ROM_SOUND,
	SAMPLES = m3::ROM_SAMPLES, N_REGIONS = m3::ROM_REGIONS };

using rom = m3::RomFile;

// Each region as the board holds it: the ROMs, then mirrored over and over to fill it (every set leaves at least
// its banked CROM half empty, and the board mirrors it)
const size_t HELD[N_REGIONS] = { 0x800000, 0x8000000, 0x4000000, 0x80000, 0x1000000 };

void place(std::vector<uint8_t> &r, const rom &ro, const std::vector<uint8_t> &d)
{
	for (size_t i = 0, at = ro.offset; i + ro.chunk <= d.size() && at + ro.chunk <= r.size(); i += ro.chunk, at += ro.stride)
		for (uint32_t j = 0; j < ro.chunk; j++)
			r[at + j] = d[i + (ro.swap ? ro.chunk - 1 - j : j)];
}

void mirror(std::vector<uint8_t> &out, size_t size, const std::vector<uint8_t> &src, size_t filled)
{
	out.assign(size, 0);
	if (!filled) return;
	for (size_t at = 0; at < size; at += filled)
		memcpy(&out[at], src.data(), std::min(filled, size - at));
}

void to_host_words(std::vector<uint8_t> &v)
{
	for (size_t i = 0; i + 3 < v.size(); i += 4) {
		uint32_t w = uint32_t(v[i]) << 24 | uint32_t(v[i + 1]) << 16 | uint32_t(v[i + 2]) << 8 | v[i + 3];
		memcpy(&v[i], &w, 4);
	}
}

// The program in the CROM is one whole gzip file from offset 0 (Fighting Vipers 2: the header names it foo.bin,
// compressed 26 March 1998; 897,285 bytes of gzip for 2,701,982 of program). The game's own boot stub unpacks it
// into RAM at 0 with zlib's inflate, so the board doesn't need this to run: it is the image the recompiler is
// built from. Unpacked here from the region's big-endian bytes, before they become host-order words, and handed
// back as the host-order words the image has in RAM. Empty if the CROM doesn't start with a gzip that unpacks.
std::vector<uint32_t> unpack_program(const std::vector<uint8_t> &crom_region, std::string &error)
{
	std::vector<uint32_t> out;
	const uint8_t *d = crom_region.data();
	const size_t n = crom_region.size();
	if (n < 18 || d[0] != 0x1F || d[1] != 0x8B || d[2] != 8) {
		error = "the program ROMs don't start with a gzip file (not the set this port knows).";
		return out;
	}
	const uint8_t flags = d[3];
	size_t at = 10;
	if (flags & 4) {                                  // FEXTRA
		if (at + 2 > n) { error = "the gzip header in the program ROMs is cut short."; return out; }
		at += 2 + size_t(d[at] | d[at + 1] << 8);
	}
	for (int field = 0; field < 2; field++)           // FNAME, FCOMMENT
		if (flags & (field == 0 ? 8 : 16)) {
			while (at < n && d[at]) at++;
			at++;
		}
	if (flags & 2) at += 2;                           // FHCRC
	if (at >= n) { error = "the gzip header in the program ROMs is cut short."; return out; }

	std::vector<uint8_t> raw;
	raw.resize(8u << 20);                             // the image goes in 8 MB of RAM, so it can't be bigger
	z_stream z{};
	if (inflateInit2(&z, -MAX_WBITS) != Z_OK) { error = "couldn't start on the program's gzip."; return out; }
	z.next_in = const_cast<Bytef *>(d + at);
	z.avail_in = uInt(n - at);
	z.next_out = raw.data();
	z.avail_out = uInt(raw.size());
	const int r = inflate(&z, Z_FINISH);
	const size_t got = z.total_out;
	const size_t left = z.avail_in;
	inflateEnd(&z);
	if (r != Z_STREAM_END || !got) { error = "the program's gzip didn't unpack."; return out; }
	// the gzip footer: the CRC32 and the size of what was packed, both little-endian, right after the stream
	const uint8_t *foot = d + n - left;
	if (left < 8) { error = "the program's gzip has no footer."; return out; }
	const uint32_t want_crc = uint32_t(foot[0] | foot[1] << 8 | foot[2] << 16 | uint32_t(foot[3]) << 24);
	const uint32_t want_size = uint32_t(foot[4] | foot[5] << 8 | foot[6] << 16 | uint32_t(foot[7]) << 24);
	if (want_size != got || uint32_t(crc32(0, raw.data(), uInt(got))) != want_crc) {
		error = "the program's gzip doesn't check out against its own footer.";
		return out;
	}
	out.resize((got + 3) / 4);
	for (size_t i = 0; i < out.size(); i++) {
		const size_t b = i * 4;
		out[i] = uint32_t(b + 0 < got ? raw[b + 0] : 0) << 24 | uint32_t(b + 1 < got ? raw[b + 1] : 0) << 16 |
		         uint32_t(b + 2 < got ? raw[b + 2] : 0) << 8 | uint32_t(b + 3 < got ? raw[b + 3] : 0);
	}
	return out;
}

// A clone's other ROMs, from its parent's zip beside it (a split set: vf3tb.zip with only Team Battle's own ROMs,
// vf3.zip in the same folder)
std::string parent_zip(const std::string &path, const m3::Profile &profile)
{
	if (!profile.parent) return {};
	const std::filesystem::path p = std::filesystem::path(path).parent_path() / (std::string(profile.parent) + ".zip");
	std::error_code ec;
	return std::filesystem::is_regular_file(p, ec) && !std::filesystem::equivalent(p, path, ec) ? p.string() : std::string();
}

} // namespace

// The zip names this build knows, for the chooser that asks the player for a ROM: one per program, the clones'
// parents among them, since a split set needs both zips beside each other.
std::vector<std::string> known_zip_names()
{
	std::vector<std::string> names;
	for (const m3::Profile *const *p = m3::PROFILES; *p; p++) names.push_back(std::string((*p)->set) + ".zip");
	return names;
}

std::vector<const m3::Profile *> sets_in(const std::string &path)
{
	std::vector<const m3::Profile *> out;
	zip z;
	if (!z.open(path, true)) return out;
	for (const m3::Profile *const *p = m3::PROFILES; *p; p++) {
		zip parent;
		const std::string pz = parent_zip(path, **p);
		const bool has_parent = !pz.empty() && parent.open(pz, true);
		// AND AT LEAST ONE OF THEM IN THIS ZIP. A clone is looked for in its parent's zip beside it, which is
		// what a split set needs; but crediting the parent for EVERY rom makes a clone match any zip at all that
		// happens to sit beside its parent. With two games in one ROM folder -- vf3.zip and fvipers2.zip -- Team
		// Battle was found in Fighting Vipers 2's zip, and the wrong program was loaded. A set has to bring
		// something of its own.
		bool all = true, own = false;
		for (size_t i = 0; i < (*p)->rom_count && all; i++) {
			bool here = false, there = false;
			for (const entry &x : z.entries) here |= x.crc == (*p)->roms[i].crc;
			if (has_parent) for (const entry &x : parent.entries) there |= x.crc == (*p)->roms[i].crc;
			own |= here;
			all = here || there;
		}
		if (all && own) out.push_back(*p);
	}
	return out;
}

result load(const std::string &path, const m3::Profile &profile)
{
	result res;
	zip z, parent;
	if (!z.open(path)) { res.error = "Couldn't read " + path + " as a zip file."; return res; }
	const std::string pz = parent_zip(path, profile);
	const bool has_parent = !pz.empty() && parent.open(pz);
	std::vector<uint8_t> region[N_REGIONS];
	for (int r = 0; r < N_REGIONS; r++) region[r].assign(profile.board.region[r], 0);
	int bad = 0;
	std::vector<uint8_t> d;
	for (size_t i = 0; i < profile.rom_count; i++) {
		const rom &ro = profile.roms[i];
		const entry *e = nullptr;
		const zip *from = &z;
		for (const entry &x : z.entries) if (x.crc == ro.crc && x.name == ro.name) { e = &x; break; }
		if (!e) for (const entry &x : z.entries) if (x.crc == ro.crc) { e = &x; break; }
		if (!e && has_parent) for (const entry &x : parent.entries) if (x.crc == ro.crc) { e = &x; from = &parent; break; }
		if (!e || !from->read(*e, d)) {
			if (!bad++) res.error = std::string("This isn't a complete ") + profile.title + " set, \"" + profile.set + "\". Not usable:";
			res.error += std::string(" ") + ro.name;
			continue;
		}
		place(region[ro.region], ro, d);
	}
	if (bad) return res;

	res.set = profile.set;
	res.title = profile.title;
	res.stepping = profile.board.stepping;
	res.crypto_key = profile.board.crypto_key;

	// The program the boot unpacks, read before the CROM becomes host-order words
	if (profile.board.gzip_program) {
		std::string why;
		res.program = unpack_program(region[CROM], why);
		if (res.program.empty()) {
			res.error = std::string("This ") + profile.title + " set loaded, but " + why;
			return res;
		}
	}

	// The fixed CROM: the program ROMs in its top, the start of banked CROM below them where they don't fill it
	res.crom.assign(HELD[CROM], 0);
	memcpy(&res.crom[HELD[CROM] - profile.board.region[CROM]], region[CROM].data(), profile.board.region[CROM]);
	if (profile.board.region[CROM] < HELD[CROM])
		memcpy(&res.crom[0], region[BANKED].data(), HELD[CROM] - profile.board.region[CROM]);
	// The banked CROM, as the reference build lays it out: a set of 64 MB or less goes into a 64 MB window with
	// the rest of that window left at zero, and the whole window -- zeros included -- is repeated to fill the
	// 128 MB the banking reaches; only a set bigger than 64 MB fills all 128 on its own. It matters for a set
	// that fills neither: Sega Rally 2's 48 MB leaves 16 MB of zeros that the game reads, and repeating its
	// 48 MB instead would put ROM where the board has none.
	{
		const size_t window = profile.board.region[BANKED] <= 0x4000000 ? 0x4000000 : HELD[BANKED];
		std::vector<uint8_t> win(window, 0);
		memcpy(win.data(), region[BANKED].data(), std::min(window, profile.board.region[BANKED]));
		mirror(res.banked, HELD[BANKED], win, window);
	}
	to_host_words(res.crom);
	to_host_words(res.banked);
	mirror(res.vrom, HELD[VROM], region[VROM], profile.board.region[VROM]);
	res.sound = std::move(region[SOUND]);
	mirror(res.samples, HELD[SAMPLES], region[SAMPLES], profile.board.region[SAMPLES]);
	res.ok = true;
	return res;
}

} // namespace romset
