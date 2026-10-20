// Fighting Vipers 2's select-screen portraits, replaced from a folder of PNG files at start-up.
//
// The portraits are not in the 2D tile layer: they are Real3D textures, and the VROM they live in is not
// compressed (unlike the program, which is a gzip in the CROM). So there is nothing to unpack and nothing to
// wait for -- the sheets are rewritten in the VROM image this port holds in memory, once, before the board
// runs. Nothing is written to the player's zip or anywhere else on disk.
//
// What was measured here, in this port's own VROM, before any of this was written: see docs/fv2-portraits.md.
#pragma once
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace fv2 {

// Why a call did nothing, when it did nothing. Anything >= 0 from replace_portraits is the number of 64x64
// cases it wrote.
enum {
	NOT_THIS_GAME = -1,     // not the Fighting Vipers 2 set these offsets were measured on
	NO_FOLDER = -2,         // the folder the player named isn't there
	NOT_THE_SHEETS = -3,    // the bytes at the measured offsets aren't the portrait sheets (another dump, or a bug)
	CODEC_DISAGREES = -4,   // the round-trip check failed: the encoder doesn't undo the decoder (nothing written)
};

// Replaces cases of the two portrait sheets in `vrom` (64 MB, as romset::load lays it out) with the PNG files
// in `folder`. `set` is the romset's name: anything but "fvipers2" is left alone, because the offsets were
// measured on that set and mean nothing in another.
//
// The folder: `light/NN.png` (or `claire/NN.png`) for the sheet the select screen draws unselected, and
// `dark/NN.png` (or `sombre/NN.png`) for the darker copy; `NN.png` at the folder's root stands in for either
// when that sheet has no file of its own. NN is 00 to 15, the sheet's own order -- which is neither the
// cursor's nor the fight identifiers'. Each file is one 64x64 image. A case with no file keeps Sega's bytes,
// at every mipmap level.
//
// Every line it has to say goes into `log` (each ending in a newline), for the caller to print where it prints.
int replace_portraits(const std::string &set, std::vector<uint8_t> &vrom, const std::string &folder, std::string &log);

// The same thing, with everything it has to say printed to `out` -- so the headless runner and the windowed
// host report it in the same words, including the reasons it did nothing. Returns what replace_portraits did.
int replace_portraits_and_report(const std::string &set, std::vector<uint8_t> &vrom, const std::string &folder, FILE *out);

// The sheet order, for messages and for docs/fv2-portraits.md. Index 11 to 15 are the sheet's free cases:
// white, and nothing draws them as the game ships.
const char *portrait_name(int slot);

} // namespace fv2
