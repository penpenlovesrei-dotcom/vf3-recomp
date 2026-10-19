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

	bool open(const std::string &path)
	{
		// vf3.zip is about 112 MB and fvipers2.zip about 96 MB: anything over 512 MB isn't a set, and isn't read
		std::error_code ec;
		const uintmax_t size = std::filesystem::file_size(path, ec);
		if (ec || size < 22 || size > (512u << 20)) return false;
		FILE *f = fopen(path.c_str(), "rb");
		if (!f) return false;
		const long n = long(size);
		data.resize(size_t(n));
		const bool got = fread(data.data(), 1, data.size(), f) == data.size();
		fclose(f);
		if (!got) return false;
		long eocd = -1;
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

// ---- the sets: each ROM goes into its region in 2-byte chunks every `stride` bytes, byte-swapped or not ----

enum region_id { CROM, BANKED, VROM, SOUND, SAMPLES, N_REGIONS };

struct rom { const char *name; uint32_t crc; region_id region; uint32_t offset; uint32_t stride; uint32_t chunk; bool swap; };

// Each set names its own program ROMs -- which is what tells one revision of a game from another -- and then the
// data ROMs, which revisions of the same game share.

// Virtua Fighter 3, Revision D (Step 1.0). The program ROMs are 512 KB each: the fixed CROM's top 2 MB, with the
// start of banked CROM 0 below them.
const rom VF3_PROGRAM[] = {
	{ "epr-19230d.20", 0x43C08240, CROM, 0, 8, 2, true },
	{ "epr-19229d.19", 0x6773F715, CROM, 2, 8, 2, true },
	{ "epr-19228d.18", 0xA2470C78, CROM, 4, 8, 2, true },
	{ "epr-19227d.17", 0x8B650966, CROM, 6, 8, 2, true },
};

const rom VF3_DATA[] = {
	{ "mpr-19196.4", 0xF386B850, BANKED, 0x0000000, 8, 2, true },
	{ "mpr-19195.3", 0xBD5E27A3, BANKED, 0x0000002, 8, 2, true },
	{ "mpr-19194.2", 0x66254702, BANKED, 0x0000004, 8, 2, true },
	{ "mpr-19193.1", 0x7BAB33D2, BANKED, 0x0000006, 8, 2, true },
	{ "mpr-19200.8", 0x74941091, BANKED, 0x1000000, 8, 2, true },
	{ "mpr-19199.7", 0x9F80D6FE, BANKED, 0x1000002, 8, 2, true },
	{ "mpr-19198.6", 0xD8EE5032, BANKED, 0x1000004, 8, 2, true },
	{ "mpr-19197.5", 0xA22D76C9, BANKED, 0x1000006, 8, 2, true },
	{ "mpr-19204.12", 0x2F93310A, BANKED, 0x2000000, 8, 2, true },
	{ "mpr-19203.11", 0x0AFA6334, BANKED, 0x2000002, 8, 2, true },
	{ "mpr-19202.10", 0xAAA086C6, BANKED, 0x2000004, 8, 2, true },
	{ "mpr-19201.9", 0x7C4A8C31, BANKED, 0x2000006, 8, 2, true },
	{ "mpr-19208.16", 0x08F30F71, BANKED, 0x3000000, 8, 2, true },
	{ "mpr-19207.15", 0x2CE1612D, BANKED, 0x3000002, 8, 2, true },
	{ "mpr-19206.14", 0x71A98D73, BANKED, 0x3000004, 8, 2, true },
	{ "mpr-19205.13", 0x199C328E, BANKED, 0x3000006, 8, 2, true },
	{ "mpr-19211.26", 0x9C8F5DF1, VROM, 0, 32, 2, false },
	{ "mpr-19212.27", 0x75036234, VROM, 2, 32, 2, false },
	{ "mpr-19213.28", 0x67B123CF, VROM, 4, 32, 2, false },
	{ "mpr-19214.29", 0xA6F5576B, VROM, 6, 32, 2, false },
	{ "mpr-19215.30", 0xC6FD9F0D, VROM, 8, 32, 2, false },
	{ "mpr-19216.31", 0x201BB1ED, VROM, 10, 32, 2, false },
	{ "mpr-19217.32", 0x4DADD41A, VROM, 12, 32, 2, false },
	{ "mpr-19218.33", 0xCFF91953, VROM, 14, 32, 2, false },
	{ "mpr-19219.34", 0xC610D521, VROM, 16, 32, 2, false },
	{ "mpr-19220.35", 0xE62924D0, VROM, 18, 32, 2, false },
	{ "mpr-19221.36", 0x24F83E3C, VROM, 20, 32, 2, false },
	{ "mpr-19222.37", 0x61A6AA7D, VROM, 22, 32, 2, false },
	{ "mpr-19223.38", 0x1A8C1980, VROM, 24, 32, 2, false },
	{ "mpr-19224.39", 0x0A79A1BD, VROM, 26, 32, 2, false },
	{ "mpr-19225.40", 0x91A985EB, VROM, 28, 32, 2, false },
	{ "mpr-19226.41", 0x00091722, VROM, 30, 32, 2, false },
	{ "epr-19231.21", 0xB416FE96, SOUND, 0, 2, 2, false },
	{ "mpr-19209.22", 0x3715E38C, SAMPLES, 0x000000, 2, 2, false },
	{ "mpr-19210.24", 0xC03D6502, SAMPLES, 0x400000, 2, 2, false },
};

// Fighting Vipers 2, Japan Revision A (Step 2.0). The program ROMs are 2 MB each: the fixed CROM's whole 8 MB,
// with no banked CROM below them. Its 16 texture ROMs fill the Real3D's 64 MB on their own, and its four sample
// ROMs the SCSPs' 16 MB; neither is mirrored. (Layout: Supermodel's Config/Games.xml, read as documentation.)
const rom FV2_PROGRAM[] = {
	{ "epr-20599a.20", 0x9DF02AB9, CROM, 0, 8, 2, true },
	{ "epr-20598a.19", 0x87BD070F, CROM, 2, 8, 2, true },
	{ "epr-20597a.18", 0x6FCEE322, CROM, 4, 8, 2, true },
	{ "epr-20596a.17", 0x969AB801, CROM, 6, 8, 2, true },
};

// Fighting Vipers 2, Japan, the first release ("fvipers2o"): the same board and the same data ROMs, four other
// program ROMs. Its program is a gzip in the CROM like Revision A's, and the loader reads whichever one is there,
// so nothing else about it is special. Not run here: no dump of this revision on hand to try it with.
const rom FV2O_PROGRAM[] = {
	{ "epr-20599.20", 0x69A0009D, CROM, 0, 8, 2, true },
	{ "epr-20598.19", 0x71EC5183, CROM, 2, 8, 2, true },
	{ "epr-20597.18", 0x4279DE19, CROM, 4, 8, 2, true },
	{ "epr-20596.17", 0xA311B4AF, CROM, 6, 8, 2, true },
};

const rom FV2_DATA[] = {
	{ "mpr-20563.4", 0x999848AC, BANKED, 0x0000000, 8, 2, true },
	{ "mpr-20562.3", 0x96E4942E, BANKED, 0x0000002, 8, 2, true },
	{ "mpr-20561.2", 0x38A0F112, BANKED, 0x0000004, 8, 2, true },
	{ "mpr-20560.1", 0xB0F6584D, BANKED, 0x0000006, 8, 2, true },
	{ "mpr-20567.8", 0x80F4EBA7, BANKED, 0x1000000, 8, 2, true },
	{ "mpr-20566.7", 0x2901883B, BANKED, 0x1000002, 8, 2, true },
	{ "mpr-20565.6", 0xD6BBE638, BANKED, 0x1000004, 8, 2, true },
	{ "mpr-20564.5", 0xBE69FCA0, BANKED, 0x1000006, 8, 2, true },
	{ "mpr-20571.12", 0x40B459AF, BANKED, 0x2000000, 8, 2, true },
	{ "mpr-20570.11", 0x2C0D91FC, BANKED, 0x2000002, 8, 2, true },
	{ "mpr-20569.10", 0x136C014F, BANKED, 0x2000004, 8, 2, true },
	{ "mpr-20568.9", 0xFF23CF1C, BANKED, 0x2000006, 8, 2, true },
	{ "mpr-20575.16", 0xEBC99D8A, BANKED, 0x3000000, 8, 2, true },
	{ "mpr-20574.15", 0x68567771, BANKED, 0x3000002, 8, 2, true },
	{ "mpr-20573.14", 0xE0DEE793, BANKED, 0x3000004, 8, 2, true },
	{ "mpr-20572.13", 0xD4A41A0B, BANKED, 0x3000006, 8, 2, true },
	{ "mpr-20580.26", 0x6D42775E, VROM, 0, 32, 2, false },
	{ "mpr-20581.27", 0xAC9EEC04, VROM, 2, 32, 2, false },
	{ "mpr-20582.28", 0xB202F7BD, VROM, 4, 32, 2, false },
	{ "mpr-20583.29", 0x0D6D508A, VROM, 6, 32, 2, false },
	{ "mpr-20584.30", 0xECCF4DE6, VROM, 8, 32, 2, false },
	{ "mpr-20585.31", 0xB383F4E5, VROM, 10, 32, 2, false },
	{ "mpr-20586.32", 0xE7CD5DFB, VROM, 12, 32, 2, false },
	{ "mpr-20587.33", 0xE2B2ABE1, VROM, 14, 32, 2, false },
	{ "mpr-20588.34", 0x84F4162D, VROM, 16, 32, 2, false },
	{ "mpr-20589.35", 0x4E653D02, VROM, 18, 32, 2, false },
	{ "mpr-20590.36", 0x527049BE, VROM, 20, 32, 2, false },
	{ "mpr-20591.37", 0x3BE20243, VROM, 22, 32, 2, false },
	{ "mpr-20592.38", 0xD7985B28, VROM, 24, 32, 2, false },
	{ "mpr-20593.39", 0xE670C4D3, VROM, 26, 32, 2, false },
	{ "mpr-20594.40", 0x35578240, VROM, 28, 32, 2, false },
	{ "mpr-20595.41", 0x1D4A2CAD, VROM, 30, 32, 2, false },
	{ "epr-20600a.21", 0xF0E7DB7E, SOUND, 0, 1, 1, false },
	// the four sample ROMs are not in the order of their numbers
	{ "mpr-20576", 0x1EEB540B, SAMPLES, 0x000000, 1, 1, false },
	{ "mpr-20578", 0xD222F2D4, SAMPLES, 0x400000, 1, 1, false },
	{ "mpr-20577", 0x3B236187, SAMPLES, 0x800000, 1, 1, false },
	{ "mpr-20579", 0x08788436, SAMPLES, 0xC00000, 1, 1, false },
};

struct set
{
	const char *name;             // MAME's name for it
	const char *title;            // for the player
	int stepping;                 // 0x10 Step 1.0, 0x20 Step 2.0
	uint32_t crypto_key;          // the 315-5881 security chip's key (0: the set has no security board)
	bool gzip_program;            // the program is a gzip at the fixed CROM's offset 0, which the boot unpacks
	size_t size[N_REGIONS];       // how much of each region the set's ROMs fill
	const rom *program;           // the program ROMs, this revision's own
	size_t n_program;
	const rom *data;              // the data ROMs, shared by the revisions of the game
	size_t n_data;

	size_t count() const { return n_program + n_data; }
	const rom &at(size_t i) const { return i < n_program ? program[i] : data[i - n_program]; }
};

#define LIST(a) a, sizeof(a) / sizeof(rom)
const set SETS[] = {
	{ "vf3", "Virtua Fighter 3 (Revision D)", 0x10, 0, false,
	  { 0x200000, 0x4000000, 0x2000000, 0x80000, 0x800000 }, LIST(VF3_PROGRAM), LIST(VF3_DATA) },
	{ "fvipers2", "Fighting Vipers 2 (Japan, Revision A)", 0x20, 0x29260E96, true,
	  { 0x800000, 0x4000000, 0x4000000, 0x80000, 0x1000000 }, LIST(FV2_PROGRAM), LIST(FV2_DATA) },
	{ "fvipers2o", "Fighting Vipers 2 (Japan)", 0x20, 0x29260E96, true,
	  { 0x800000, 0x4000000, 0x4000000, 0x80000, 0x1000000 }, LIST(FV2O_PROGRAM), LIST(FV2_DATA) },
};
#undef LIST

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

} // namespace

result load(const std::string &path)
{
	result res;
	zip z;
	if (!z.open(path)) { res.error = "Couldn't read " + path + " as a zip file."; return res; }

	// Which set the zip holds: the one most of whose ROMs are in it by CRC, counting a program ROM for much more
	// than a data ROM, because the program ROMs are what tell one revision of a game from another (two revisions
	// share their data ROMs, and a zip holding both revisions' programs is read as the later one)
	const set *s = nullptr;
	size_t best = 0;
	for (const set &candidate : SETS) {
		size_t found = 0;
		for (size_t i = 0; i < candidate.count(); i++)
			for (const entry &x : z.entries)
				if (x.crc == candidate.at(i).crc) { found += i < candidate.n_program ? 100 : 1; break; }
		if (found > best) { best = found; s = &candidate; }
	}
	if (!s) {
		res.error = "This zip holds none of the sets this port knows: Virtua Fighter 3 (Revision D), \"vf3\", "
		            "or Fighting Vipers 2 (Japan), \"fvipers2\" and \"fvipers2o\".";
		return res;
	}

	std::vector<uint8_t> region[N_REGIONS];
	for (int r = 0; r < N_REGIONS; r++) region[r].assign(s->size[r], 0);
	int bad = 0;
	std::vector<uint8_t> d;
	for (size_t i = 0; i < s->count(); i++) {
		const rom &ro = s->at(i);
		const entry *e = nullptr;
		for (const entry &x : z.entries) if (x.crc == ro.crc && x.name == ro.name) { e = &x; break; }
		if (!e) for (const entry &x : z.entries) if (x.crc == ro.crc) { e = &x; break; }
		if (!e || !z.read(*e, d)) {
			if (!bad++)
				res.error = std::string("This isn't a complete ") + s->title + " set, \"" + s->name + "\". Not usable:";
			res.error += std::string(" ") + ro.name;
			continue;
		}
		place(region[ro.region], ro, d);
	}
	if (bad) return res;

	res.set = s->name;
	res.title = s->title;
	res.stepping = s->stepping;
	res.crypto_key = s->crypto_key;

	// The program the boot unpacks, read before the CROM becomes host-order words
	if (s->gzip_program) {
		std::string why;
		res.program = unpack_program(region[CROM], why);
		if (res.program.empty()) {
			res.error = std::string("This ") + s->title + " set loaded, but " + why;
			return res;
		}
	}

	// The fixed CROM: the program ROMs in its top, the start of banked CROM below them where they don't fill it
	res.crom.assign(HELD[CROM], 0);
	memcpy(&res.crom[HELD[CROM] - s->size[CROM]], region[CROM].data(), s->size[CROM]);
	if (s->size[CROM] < HELD[CROM])
		memcpy(&res.crom[0], region[BANKED].data(), HELD[CROM] - s->size[CROM]);
	mirror(res.banked, HELD[BANKED], region[BANKED], s->size[BANKED]);
	to_host_words(res.crom);
	to_host_words(res.banked);
	mirror(res.vrom, HELD[VROM], region[VROM], s->size[VROM]);
	res.sound = std::move(region[SOUND]);
	mirror(res.samples, HELD[SAMPLES], region[SAMPLES], s->size[SAMPLES]);
	res.ok = true;
	return res;
}

} // namespace romset
