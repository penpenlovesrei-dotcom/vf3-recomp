// test_scsp_contract: checks src/sound/scsp_contract.h against the two things it claims to describe.
//
// A contract nobody checks drifts, and a drifted contract is worse than none: the next person writes an
// autonomous SCSP against numbers that stopped being true. So this test reads the contract's two sources
// back and compares:
//
//   PART 1 -- every field against the macros in src/sound/scsp.cpp. The implementation is the authority on
//   where a bit lives today; the contract only records it. The comparison is on the (word, shift, mask)
//   triple, in exactly the form the macro writes it.
//
//   PART 2 -- every driver constant against the sound program ROM: the ten words the driver copies into
//   0x418-0x42A, the timer value snd_irq2 reloads, and the 192-word pitch table.
//
// AND IT FAILS LOUDLY WHEN IT CANNOT CHECK. A field whose macro is not found is an ERROR, not a skip; the
// aliases and the hand-checked specials are listed explicitly and counted, so the total of
// matched + aliased + special must equal the number of fields. The lesson of this project's bad days is
// that a filter which silently drops lines looks exactly like a test that passes.
//
// PART 2 is skipped, with a visible line, when build/fv2-sound-68000.bin is absent -- it is extracted from a
// ROM the repository does not carry (tools/sound/extract-sound.py). PART 1 always runs.
#include "scsp_contract.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {

int failures = 0;

void ok(const char *what, const char *detail)
{
	printf("  ok    %-34s %s\n", what, detail);
}

void fail(const char *what, const char *detail)
{
	printf("  FAIL  %-34s %s\n", what, detail);
	++failures;
}

std::string read(const char *path)
{
	FILE *f = fopen(path, "rb");
	if (!f) return {};
	std::string s;
	char buffer[65536];
	size_t n;
	while ((n = fread(buffer, 1, sizeof buffer, f)) > 0) s.append(buffer, n);
	fclose(f);
	return s;
}

// The implementation spells two fields differently from the datasheet name the contract uses.
struct Alias { const char *contract; const char *source; };
constexpr Alias ALIAS[] = {
	{ "KYONEX", "KEYONEX" },
	{ "KYONB",  "KEYONB"  },
	{ "MEM4B",  "MEM4B"   },   // same, listed so the table is the whole story
};

// Fields the implementation does not express as one (shift, mask) macro, checked by hand below.
constexpr const char *SPECIALS[] = { "SA_HI", "SA_LO", "LSA", "LEA" };

bool is_special(const char *name)
{
	for (const char *s : SPECIALS)
		if (!strcmp(s, name)) return true;
	return false;
}

const char *source_name(const char *contract)
{
	for (const Alias &a : ALIAS)
		if (!strcmp(a.contract, contract)) return a.source;
	return contract;
}

// Finds `#define NOM(...) ((...data[0xN] >> 0xS) & 0xMMMM)` and returns the triple.
bool macro(const std::string &src, const char *name, unsigned &word, unsigned &shift, unsigned &mask)
{
	const std::string key = std::string("#define ") + name;
	for (size_t p = src.find(key); p != std::string::npos; p = src.find(key, p + 1)) {
		// the name must end here, so TL does not match TLnnn
		const char after = src[p + key.size()];
		if (after != '(' && after != ' ' && after != '\t') continue;
		const size_t end = src.find('\n', p);
		const std::string line = src.substr(p, end - p);
		const size_t d = line.find("data[");
		if (d == std::string::npos) continue;
		unsigned m = 0, s = 0, k = 0;
		// data[0xN] or data[N], then >> 0xS, then & 0xMMMM
		if (sscanf(line.c_str() + d, "data[0x%x] >> 0x%x) & 0x%x", &m, &s, &k) == 3
		    || sscanf(line.c_str() + d, "data[%u] >> 0x%x) & 0x%x", &m, &s, &k) == 3) {
			word = m; shift = s; mask = k;
			return true;
		}
	}
	return false;
}

void part1()
{
	printf("-- PART 1: the fields against src/sound/scsp.cpp --\n");
	const std::string src = read("src/sound/scsp.cpp");
	if (src.empty()) {
		fail("src/sound/scsp.cpp", "not found or empty -- part 1 cannot run");
		return;
	}
	unsigned seen = 0, specials = 0;
	struct Block { const char *what; const scsp_contract::Field *c; size_t n; };
	const Block blocks[] = {
		{ "slot",   scsp_contract::SLOT,  sizeof scsp_contract::SLOT  / sizeof(scsp_contract::Field) },
		{ "common", scsp_contract::COMMON, sizeof scsp_contract::COMMON / sizeof(scsp_contract::Field) },
	};
	for (const Block &b : blocks)
		for (size_t i = 0; i < b.n; i++) {
			const scsp_contract::Field &f = b.c[i];
			if (is_special(f.name)) { ++specials; continue; }
			unsigned word = 0, shift = 0, mask = 0;
			char detail[160];
			if (!macro(src, source_name(f.name), word, shift, mask)) {
				snprintf(detail, sizeof detail, "no %s macro in the implementation", source_name(f.name));
				fail(f.name, detail);
				continue;
			}
			if (word != f.word || shift != f.shift_by || mask != f.mask) {
				snprintf(detail, sizeof detail,
				         "contract word %x shift %x mask %04x; implementation word %x shift %x mask %04x",
				         f.word, f.shift_by, f.mask, word, shift, mask);
				fail(f.name, detail);
				continue;
			}
			snprintf(detail, sizeof detail, "%s word %x, shift %x, mask %04x", b.what, word, shift, mask);
			ok(f.name, detail);
			++seen;
		}
	// SA, LSA and LEA are whole words in the implementation, not (shift, mask) fields.
	{
		char detail[160];
		const bool a = src.find("#define SA(slot)") != std::string::npos
		            && src.find("(slot->udata.data[0x0] & 0xF) << 16") != std::string::npos
		            && src.find("| (slot->udata.data[0x1])") != std::string::npos;
		snprintf(detail, sizeof detail, "SA = (data[0] & 0xF) << 16 | data[1], as the contract splits it");
		a ? ok("SA_HI + SA_LO", detail) : fail("SA_HI + SA_LO", "the SA macro has changed shape");
		// Tolerant of whitespace: the macro is found, then data[0xN] on its line, and N is read. An
		// exact string comparison had failed here on two spaces, which this test caught -- that is
		// its job, but the rigidity was mine and not the implementation's.
		const auto whole_word = [&src](const char *name, unsigned expected) {
			const std::string key = std::string("#define ") + name + "(slot)";
			const size_t p = src.find(key);
			char detail[160];
			if (p == std::string::npos) { fail(name, "macro not found"); return; }
			const size_t end = src.find('\n', p);
			const std::string line = src.substr(p, end - p);
			unsigned n = 0;
			const size_t d = line.find("data[");
			if (d == std::string::npos || sscanf(line.c_str() + d, "data[0x%x])", &n) != 1) {
				snprintf(detail, sizeof detail, "the macro is no longer a whole word: %s", line.c_str());
				fail(name, detail);
				return;
			}
			if (n != expected) {
				snprintf(detail, sizeof detail, "contract data[0x%x], implementation data[0x%x]", expected, n);
				fail(name, detail);
				return;
			}
			snprintf(detail, sizeof detail, "a whole word, data[0x%x]", n);
			ok(name, detail);
		};
		whole_word("LSA", 0x2);
		whole_word("LEA", 0x3);
	}
	const unsigned total = unsigned(blocks[0].n + blocks[1].n);
	char detail[160];
	snprintf(detail, sizeof detail, "%u compared + %u handled apart = %u fields in the contract",
	         seen, specials, total);
	(seen + specials == total) ? ok("the count closes", detail)
	                           : fail("the count closes", detail);
}

void part2()
{
	printf("\n-- PART 2: the driver's constants against the ROM --\n");
	const std::string rom = read("build/fv2-sound-68000.bin");
	if (rom.size() < 0x80000) {
		printf("  skipped build/fv2-sound-68000.bin is absent: tools/sound/extract-sound.py makes it\n");
		printf("          from a ROM the repository does not carry. Part 1 did run.\n");
		return;
	}
	const auto u16 = [&rom](unsigned a) -> unsigned {
		const unsigned o = a - 0x600000;
		return unsigned(uint8_t(rom[o])) << 8 | unsigned(uint8_t(rom[o + 1]));
	};
	char detail[200];

	// The ten words 0x600288 copies into 0x418-0x42A.
	bool all = true;
	for (unsigned i = 0; i < 10; i++)
		if (u16(0x6002D4 + 2 * i) != scsp_contract::INIT_418_42A[i]) {
			snprintf(detail, sizeof detail, "word %u: ROM %04x, contract %04x",
			         i, u16(0x6002D4 + 2 * i), scsp_contract::INIT_418_42A[i]);
			fail("the ten words at 0x6002D4", detail);
			all = false;
			break;
		}
	if (all) ok("the ten words at 0x6002D4", "0x418 to 0x42A, identical to the contract");

	// The value snd_irq2 reloads at every tick.
	if (u16(0x60806A) == scsp_contract::TIMER_A_COUNT)
		ok("[0x60806A], the tick reloaded", "0x00CE, that is 50 samples and 882 Hz");
	else {
		snprintf(detail, sizeof detail, "ROM %04x, contract %02x", u16(0x60806A), scsp_contract::TIMER_A_COUNT);
		fail("[0x60806A], the tick reloaded", detail);
	}

	// The three levels, recomputed from the contract's SCILV.
	for (const scsp_contract::Source &s : scsp_contract::SOURCES) {
		if (!s.armed) {
			const bool clear = (scsp_contract::INIT_418_42A[3] >> s.bit & 1) == 0;
			clear ? ok(s.name, "not armed in SCIEB, as the contract says")
			      : fail(s.name, "the contract says it is not armed, SCIEB finds it armed");
			continue;
		}
		const unsigned n = scsp_contract::level_of(s.bit);
		const bool armed = (scsp_contract::INIT_418_42A[3] >> s.bit & 1) != 0;
		if (n == s.level && armed) {
			snprintf(detail, sizeof detail, "bit %u, armed, level %u", s.bit, n);
			ok(s.name, detail);
		} else {
			snprintf(detail, sizeof detail, "bit %u: armed %d, level computed %u, contract %u",
			         s.bit, int(armed), n, s.level);
			fail(s.name, detail);
		}
	}

	// The pitch table: 192 ascending words, then the wrap.
	{
		const unsigned base = scsp_contract::FNS_ROM_TABLE, n = scsp_contract::WORDS_PER_OCTAVE;
		bool ascending = true;
		unsigned top = 0;
		for (unsigned k = 0; k + 1 < n; k++) {
			if (u16(base + 2 * k) > u16(base + 2 * (k + 1))) { ascending = false; break; }
			if (u16(base + 2 * k) > top) top = u16(base + 2 * k);
		}
		if (u16(base + 2 * (n - 1)) > top) top = u16(base + 2 * (n - 1));
		const bool loop = u16(base + 2 * n) < u16(base + 2 * (n - 1));
		const bool fits = top <= 0x3FF;
		if (ascending && loop && fits) {
			snprintf(detail, sizeof detail,
			         "%u ascending words up to %04x, and the %uth wraps: one octave",
			         n, top, n);
			ok("the table at 0x603D62", detail);
		} else {
			snprintf(detail, sizeof detail, "ascending %d, wrap %d, fits in FNS %d (max %04x)",
			         int(ascending), int(loop), int(fits), top);
			fail("the table at 0x603D62", detail);
		}
	}
}

} // namespace

int main()
{
	printf("test_scsp_contract: src/sound/scsp_contract.h against the implementation and the ROM\n\n");
	part1();
	part2();
	printf("\n");
	if (failures) {
		printf("%d failure(s). The contract and what it describes have diverged: correct one or the\n", failures);
		printf("other, and not the test.\n");
		return 1;
	}
	printf("all cases pass\n");
	return 0;
}
