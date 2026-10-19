// The player's own ROM set, read straight from the zip: every ROM checked by CRC and laid out as the Model 3
// sees it. Two sets are known: Virtua Fighter 3 (Revision D, MAME's and Supermodel's "vf3", Step 1.0) and
// Fighting Vipers 2 (Japan Revision A, "fvipers2", Step 2.0). Which one a zip holds is found from the CRCs in
// it, so the same binary runs either.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace romset {

struct result
{
	bool ok = false;
	std::string error;               // what's missing or wrong, for the player
	std::string set;                 // MAME's name for the set that was found ("vf3", "fvipers2")
	std::string title;               // the game's title, for the player
	int stepping = 0x10;             // the board: 0x10 Step 1.0, 0x20 Step 2.0
	uint32_t crypto_key = 0;         // the 315-5881 security chip's key, 0 for a set with no security board
	std::vector<uint8_t> crom;       // fixed CROM, 8 MB at 0xFF800000: the program ROMs at the top, banked CROM 0 below
	std::vector<uint8_t> banked;     // banked CROM, 128 MB (the ROMs, mirrored to fill it)
	std::vector<uint8_t> vrom;       // the Real3D's texture ROMs, 64 MB (the ROMs, mirrored to fill it)
	std::vector<uint8_t> sound;      // the sound board's 68000 program, 512 KB
	std::vector<uint8_t> samples;    // the SCSPs' samples, 16 MB (the ROMs, mirrored to fill it)
	// The program the boot puts in RAM at 0, host-order words, for sets whose program is compressed in the CROM
	// (Fighting Vipers 2: a gzip at the fixed CROM's offset 0, decompressed by the game's own boot stub). Empty
	// for a set whose program the boot copies from the CROM as it stands (Virtua Fighter 3): Model3 reads it from
	// the CROM itself.
	std::vector<uint32_t> program;

	bool is_vf3() const { return set == "vf3"; }
	bool is_fvipers2() const { return set == "fvipers2"; }
};

// The sets this build knows, as the file names they are usually kept under ("vf3.zip", ...), the one this
// port was written for first. A program looking for a player's ROMs tries them in this order.
std::vector<std::string> known_zip_names();

// The CROM regions come back as host-order 32-bit words (as the CPU core reads them); the others as the bytes
// of the big-endian words.
result load(const std::string &zip);

} // namespace romset
