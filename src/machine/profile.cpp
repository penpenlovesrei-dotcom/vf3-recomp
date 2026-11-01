// The programs' profiles (profile.h)
#include "profile.h"
#include "../rec/ppc_rec.h"
#include <cstring>

namespace m3 {

namespace {

// Revision D's 39 ROMs (MAME's vf3), each checked by CRC: the 4 program ROMs (epr-19227d to 19230d), banked CROM,
// the Real3D's textures, the sound board's program and its samples
const RomFile REV_D_ROMS[] = {
	{ "epr-19230d.20", 0x43C08240, ROM_CROM, 0, 8, 2, true },
	{ "epr-19229d.19", 0x6773F715, ROM_CROM, 2, 8, 2, true },
	{ "epr-19228d.18", 0xA2470C78, ROM_CROM, 4, 8, 2, true },
	{ "epr-19227d.17", 0x8B650966, ROM_CROM, 6, 8, 2, true },
	{ "mpr-19196.4", 0xF386B850, ROM_BANKED, 0x0000000, 8, 2, true },
	{ "mpr-19195.3", 0xBD5E27A3, ROM_BANKED, 0x0000002, 8, 2, true },
	{ "mpr-19194.2", 0x66254702, ROM_BANKED, 0x0000004, 8, 2, true },
	{ "mpr-19193.1", 0x7BAB33D2, ROM_BANKED, 0x0000006, 8, 2, true },
	{ "mpr-19200.8", 0x74941091, ROM_BANKED, 0x1000000, 8, 2, true },
	{ "mpr-19199.7", 0x9F80D6FE, ROM_BANKED, 0x1000002, 8, 2, true },
	{ "mpr-19198.6", 0xD8EE5032, ROM_BANKED, 0x1000004, 8, 2, true },
	{ "mpr-19197.5", 0xA22D76C9, ROM_BANKED, 0x1000006, 8, 2, true },
	{ "mpr-19204.12", 0x2F93310A, ROM_BANKED, 0x2000000, 8, 2, true },
	{ "mpr-19203.11", 0x0AFA6334, ROM_BANKED, 0x2000002, 8, 2, true },
	{ "mpr-19202.10", 0xAAA086C6, ROM_BANKED, 0x2000004, 8, 2, true },
	{ "mpr-19201.9", 0x7C4A8C31, ROM_BANKED, 0x2000006, 8, 2, true },
	{ "mpr-19208.16", 0x08F30F71, ROM_BANKED, 0x3000000, 8, 2, true },
	{ "mpr-19207.15", 0x2CE1612D, ROM_BANKED, 0x3000002, 8, 2, true },
	{ "mpr-19206.14", 0x71A98D73, ROM_BANKED, 0x3000004, 8, 2, true },
	{ "mpr-19205.13", 0x199C328E, ROM_BANKED, 0x3000006, 8, 2, true },
	{ "mpr-19211.26", 0x9C8F5DF1, ROM_VROM, 0, 32, 2, false },
	{ "mpr-19212.27", 0x75036234, ROM_VROM, 2, 32, 2, false },
	{ "mpr-19213.28", 0x67B123CF, ROM_VROM, 4, 32, 2, false },
	{ "mpr-19214.29", 0xA6F5576B, ROM_VROM, 6, 32, 2, false },
	{ "mpr-19215.30", 0xC6FD9F0D, ROM_VROM, 8, 32, 2, false },
	{ "mpr-19216.31", 0x201BB1ED, ROM_VROM, 10, 32, 2, false },
	{ "mpr-19217.32", 0x4DADD41A, ROM_VROM, 12, 32, 2, false },
	{ "mpr-19218.33", 0xCFF91953, ROM_VROM, 14, 32, 2, false },
	{ "mpr-19219.34", 0xC610D521, ROM_VROM, 16, 32, 2, false },
	{ "mpr-19220.35", 0xE62924D0, ROM_VROM, 18, 32, 2, false },
	{ "mpr-19221.36", 0x24F83E3C, ROM_VROM, 20, 32, 2, false },
	{ "mpr-19222.37", 0x61A6AA7D, ROM_VROM, 22, 32, 2, false },
	{ "mpr-19223.38", 0x1A8C1980, ROM_VROM, 24, 32, 2, false },
	{ "mpr-19224.39", 0x0A79A1BD, ROM_VROM, 26, 32, 2, false },
	{ "mpr-19225.40", 0x91A985EB, ROM_VROM, 28, 32, 2, false },
	{ "mpr-19226.41", 0x00091722, ROM_VROM, 30, 32, 2, false },
	{ "epr-19231.21", 0xB416FE96, ROM_SOUND, 0, 2, 2, false },
	{ "mpr-19209.22", 0x3715E38C, ROM_SAMPLES, 0x000000, 2, 2, false },
	{ "mpr-19210.24", 0xC03D6502, ROM_SAMPLES, 0x400000, 2, 2, false },
};

// Team Battle's (MAME's vf3tb, a clone of vf3): its own program (epr-20126 to 20129) and first 16 MB of banked CROM
// (mpr-20130 to 20133), placed as Revision D's; the rest Revision D's
const RomFile TB_ROMS[] = {
	{ "epr-20129.20", 0x0DB897CE, ROM_CROM, 0, 8, 2, true },
	{ "epr-20128.19", 0xFFBDBDC5, ROM_CROM, 2, 8, 2, true },
	{ "epr-20127.18", 0x5C0F694B, ROM_CROM, 4, 8, 2, true },
	{ "epr-20126.17", 0x27ECD3B0, ROM_CROM, 6, 8, 2, true },
	{ "mpr-20133.4", 0x3D9B5171, ROM_BANKED, 0x0000000, 8, 2, true },
	{ "mpr-20132.3", 0xF7557474, ROM_BANKED, 0x0000002, 8, 2, true },
	{ "mpr-20131.2", 0x51FA69F1, ROM_BANKED, 0x0000004, 8, 2, true },
	{ "mpr-20130.1", 0x40640446, ROM_BANKED, 0x0000006, 8, 2, true },
	{ "mpr-19200.8", 0x74941091, ROM_BANKED, 0x1000000, 8, 2, true },
	{ "mpr-19199.7", 0x9F80D6FE, ROM_BANKED, 0x1000002, 8, 2, true },
	{ "mpr-19198.6", 0xD8EE5032, ROM_BANKED, 0x1000004, 8, 2, true },
	{ "mpr-19197.5", 0xA22D76C9, ROM_BANKED, 0x1000006, 8, 2, true },
	{ "mpr-19204.12", 0x2F93310A, ROM_BANKED, 0x2000000, 8, 2, true },
	{ "mpr-19203.11", 0x0AFA6334, ROM_BANKED, 0x2000002, 8, 2, true },
	{ "mpr-19202.10", 0xAAA086C6, ROM_BANKED, 0x2000004, 8, 2, true },
	{ "mpr-19201.9", 0x7C4A8C31, ROM_BANKED, 0x2000006, 8, 2, true },
	{ "mpr-19208.16", 0x08F30F71, ROM_BANKED, 0x3000000, 8, 2, true },
	{ "mpr-19207.15", 0x2CE1612D, ROM_BANKED, 0x3000002, 8, 2, true },
	{ "mpr-19206.14", 0x71A98D73, ROM_BANKED, 0x3000004, 8, 2, true },
	{ "mpr-19205.13", 0x199C328E, ROM_BANKED, 0x3000006, 8, 2, true },
	{ "mpr-19211.26", 0x9C8F5DF1, ROM_VROM, 0, 32, 2, false },
	{ "mpr-19212.27", 0x75036234, ROM_VROM, 2, 32, 2, false },
	{ "mpr-19213.28", 0x67B123CF, ROM_VROM, 4, 32, 2, false },
	{ "mpr-19214.29", 0xA6F5576B, ROM_VROM, 6, 32, 2, false },
	{ "mpr-19215.30", 0xC6FD9F0D, ROM_VROM, 8, 32, 2, false },
	{ "mpr-19216.31", 0x201BB1ED, ROM_VROM, 10, 32, 2, false },
	{ "mpr-19217.32", 0x4DADD41A, ROM_VROM, 12, 32, 2, false },
	{ "mpr-19218.33", 0xCFF91953, ROM_VROM, 14, 32, 2, false },
	{ "mpr-19219.34", 0xC610D521, ROM_VROM, 16, 32, 2, false },
	{ "mpr-19220.35", 0xE62924D0, ROM_VROM, 18, 32, 2, false },
	{ "mpr-19221.36", 0x24F83E3C, ROM_VROM, 20, 32, 2, false },
	{ "mpr-19222.37", 0x61A6AA7D, ROM_VROM, 22, 32, 2, false },
	{ "mpr-19223.38", 0x1A8C1980, ROM_VROM, 24, 32, 2, false },
	{ "mpr-19224.39", 0x0A79A1BD, ROM_VROM, 26, 32, 2, false },
	{ "mpr-19225.40", 0x91A985EB, ROM_VROM, 28, 32, 2, false },
	{ "mpr-19226.41", 0x00091722, ROM_VROM, 30, 32, 2, false },
	{ "epr-19231.21", 0xB416FE96, ROM_SOUND, 0, 2, 2, false },
	{ "mpr-19209.22", 0x3715E38C, ROM_SAMPLES, 0x000000, 2, 2, false },
	{ "mpr-19210.24", 0xC03D6502, ROM_SAMPLES, 0x400000, 2, 2, false },
};

} // namespace

// Fighting Vipers 2, Japan Revision A (Step 2.0). The program ROMs are 2 MB each: the fixed CROM's whole 8 MB,
// with no banked CROM below them. Its 16 texture ROMs fill the Real3D's 64 MB on their own, and its four sample
// ROMs the SCSPs' 16 MB; neither is mirrored. (Layout: Supermodel's Config/Games.xml, read as documentation.)
// The 4 program ROMs and the data ROMs the two sets share, in one table.
const RomFile FV2_ROMS[] = {
	{ "epr-20599a.20", 0x9DF02AB9, ROM_CROM, 0, 8, 2, true },
	{ "epr-20598a.19", 0x87BD070F, ROM_CROM, 2, 8, 2, true },
	{ "epr-20597a.18", 0x6FCEE322, ROM_CROM, 4, 8, 2, true },
	{ "epr-20596a.17", 0x969AB801, ROM_CROM, 6, 8, 2, true },
	{ "mpr-20563.4", 0x999848AC, ROM_BANKED, 0x0000000, 8, 2, true },
	{ "mpr-20562.3", 0x96E4942E, ROM_BANKED, 0x0000002, 8, 2, true },
	{ "mpr-20561.2", 0x38A0F112, ROM_BANKED, 0x0000004, 8, 2, true },
	{ "mpr-20560.1", 0xB0F6584D, ROM_BANKED, 0x0000006, 8, 2, true },
	{ "mpr-20567.8", 0x80F4EBA7, ROM_BANKED, 0x1000000, 8, 2, true },
	{ "mpr-20566.7", 0x2901883B, ROM_BANKED, 0x1000002, 8, 2, true },
	{ "mpr-20565.6", 0xD6BBE638, ROM_BANKED, 0x1000004, 8, 2, true },
	{ "mpr-20564.5", 0xBE69FCA0, ROM_BANKED, 0x1000006, 8, 2, true },
	{ "mpr-20571.12", 0x40B459AF, ROM_BANKED, 0x2000000, 8, 2, true },
	{ "mpr-20570.11", 0x2C0D91FC, ROM_BANKED, 0x2000002, 8, 2, true },
	{ "mpr-20569.10", 0x136C014F, ROM_BANKED, 0x2000004, 8, 2, true },
	{ "mpr-20568.9", 0xFF23CF1C, ROM_BANKED, 0x2000006, 8, 2, true },
	{ "mpr-20575.16", 0xEBC99D8A, ROM_BANKED, 0x3000000, 8, 2, true },
	{ "mpr-20574.15", 0x68567771, ROM_BANKED, 0x3000002, 8, 2, true },
	{ "mpr-20573.14", 0xE0DEE793, ROM_BANKED, 0x3000004, 8, 2, true },
	{ "mpr-20572.13", 0xD4A41A0B, ROM_BANKED, 0x3000006, 8, 2, true },
	{ "mpr-20580.26", 0x6D42775E, ROM_VROM, 0, 32, 2, false },
	{ "mpr-20581.27", 0xAC9EEC04, ROM_VROM, 2, 32, 2, false },
	{ "mpr-20582.28", 0xB202F7BD, ROM_VROM, 4, 32, 2, false },
	{ "mpr-20583.29", 0x0D6D508A, ROM_VROM, 6, 32, 2, false },
	{ "mpr-20584.30", 0xECCF4DE6, ROM_VROM, 8, 32, 2, false },
	{ "mpr-20585.31", 0xB383F4E5, ROM_VROM, 10, 32, 2, false },
	{ "mpr-20586.32", 0xE7CD5DFB, ROM_VROM, 12, 32, 2, false },
	{ "mpr-20587.33", 0xE2B2ABE1, ROM_VROM, 14, 32, 2, false },
	{ "mpr-20588.34", 0x84F4162D, ROM_VROM, 16, 32, 2, false },
	{ "mpr-20589.35", 0x4E653D02, ROM_VROM, 18, 32, 2, false },
	{ "mpr-20590.36", 0x527049BE, ROM_VROM, 20, 32, 2, false },
	{ "mpr-20591.37", 0x3BE20243, ROM_VROM, 22, 32, 2, false },
	{ "mpr-20592.38", 0xD7985B28, ROM_VROM, 24, 32, 2, false },
	{ "mpr-20593.39", 0xE670C4D3, ROM_VROM, 26, 32, 2, false },
	{ "mpr-20594.40", 0x35578240, ROM_VROM, 28, 32, 2, false },
	{ "mpr-20595.41", 0x1D4A2CAD, ROM_VROM, 30, 32, 2, false },
	{ "epr-20600a.21", 0xF0E7DB7E, ROM_SOUND, 0, 1, 1, false },
	{ "mpr-20576", 0x1EEB540B, ROM_SAMPLES, 0x000000, 1, 1, false },
	{ "mpr-20578", 0xD222F2D4, ROM_SAMPLES, 0x400000, 1, 1, false },
	{ "mpr-20577", 0x3B236187, ROM_SAMPLES, 0x800000, 1, 1, false },
	{ "mpr-20579", 0x08788436, ROM_SAMPLES, 0xC00000, 1, 1, false },
};

// Fighting Vipers 2, Japan, the first release ("fvipers2o"): the same board and the same data ROMs, four other
// program ROMs. Its program is a gzip in the CROM like Revision A's, and the loader reads whichever one is there,
// so nothing else about it is special. Not run here: no dump of this revision on hand to try it with.
const RomFile FV2O_ROMS[] = {
	{ "epr-20599.20", 0x69A0009D, ROM_CROM, 0, 8, 2, true },
	{ "epr-20598.19", 0x71EC5183, ROM_CROM, 2, 8, 2, true },
	{ "epr-20597.18", 0x4279DE19, ROM_CROM, 4, 8, 2, true },
	{ "epr-20596.17", 0xA311B4AF, ROM_CROM, 6, 8, 2, true },
	{ "mpr-20563.4", 0x999848AC, ROM_BANKED, 0x0000000, 8, 2, true },
	{ "mpr-20562.3", 0x96E4942E, ROM_BANKED, 0x0000002, 8, 2, true },
	{ "mpr-20561.2", 0x38A0F112, ROM_BANKED, 0x0000004, 8, 2, true },
	{ "mpr-20560.1", 0xB0F6584D, ROM_BANKED, 0x0000006, 8, 2, true },
	{ "mpr-20567.8", 0x80F4EBA7, ROM_BANKED, 0x1000000, 8, 2, true },
	{ "mpr-20566.7", 0x2901883B, ROM_BANKED, 0x1000002, 8, 2, true },
	{ "mpr-20565.6", 0xD6BBE638, ROM_BANKED, 0x1000004, 8, 2, true },
	{ "mpr-20564.5", 0xBE69FCA0, ROM_BANKED, 0x1000006, 8, 2, true },
	{ "mpr-20571.12", 0x40B459AF, ROM_BANKED, 0x2000000, 8, 2, true },
	{ "mpr-20570.11", 0x2C0D91FC, ROM_BANKED, 0x2000002, 8, 2, true },
	{ "mpr-20569.10", 0x136C014F, ROM_BANKED, 0x2000004, 8, 2, true },
	{ "mpr-20568.9", 0xFF23CF1C, ROM_BANKED, 0x2000006, 8, 2, true },
	{ "mpr-20575.16", 0xEBC99D8A, ROM_BANKED, 0x3000000, 8, 2, true },
	{ "mpr-20574.15", 0x68567771, ROM_BANKED, 0x3000002, 8, 2, true },
	{ "mpr-20573.14", 0xE0DEE793, ROM_BANKED, 0x3000004, 8, 2, true },
	{ "mpr-20572.13", 0xD4A41A0B, ROM_BANKED, 0x3000006, 8, 2, true },
	{ "mpr-20580.26", 0x6D42775E, ROM_VROM, 0, 32, 2, false },
	{ "mpr-20581.27", 0xAC9EEC04, ROM_VROM, 2, 32, 2, false },
	{ "mpr-20582.28", 0xB202F7BD, ROM_VROM, 4, 32, 2, false },
	{ "mpr-20583.29", 0x0D6D508A, ROM_VROM, 6, 32, 2, false },
	{ "mpr-20584.30", 0xECCF4DE6, ROM_VROM, 8, 32, 2, false },
	{ "mpr-20585.31", 0xB383F4E5, ROM_VROM, 10, 32, 2, false },
	{ "mpr-20586.32", 0xE7CD5DFB, ROM_VROM, 12, 32, 2, false },
	{ "mpr-20587.33", 0xE2B2ABE1, ROM_VROM, 14, 32, 2, false },
	{ "mpr-20588.34", 0x84F4162D, ROM_VROM, 16, 32, 2, false },
	{ "mpr-20589.35", 0x4E653D02, ROM_VROM, 18, 32, 2, false },
	{ "mpr-20590.36", 0x527049BE, ROM_VROM, 20, 32, 2, false },
	{ "mpr-20591.37", 0x3BE20243, ROM_VROM, 22, 32, 2, false },
	{ "mpr-20592.38", 0xD7985B28, ROM_VROM, 24, 32, 2, false },
	{ "mpr-20593.39", 0xE670C4D3, ROM_VROM, 26, 32, 2, false },
	{ "mpr-20594.40", 0x35578240, ROM_VROM, 28, 32, 2, false },
	{ "mpr-20595.41", 0x1D4A2CAD, ROM_VROM, 30, 32, 2, false },
	{ "epr-20600a.21", 0xF0E7DB7E, ROM_SOUND, 0, 1, 1, false },
	{ "mpr-20576", 0x1EEB540B, ROM_SAMPLES, 0x000000, 1, 1, false },
	{ "mpr-20578", 0xD222F2D4, ROM_SAMPLES, 0x400000, 1, 1, false },
	{ "mpr-20577", 0x3B236187, ROM_SAMPLES, 0x800000, 1, 1, false },
	{ "mpr-20579", 0x08788436, ROM_SAMPLES, 0xC00000, 1, 1, false },
};

// Sega Rally 2 (Export, Step 2.0). The same board as Fighting Vipers 2 and no security chip, but only
// three groups of banked CROM (48 MB, mirrored to fill the 128), 8 MB of samples mirrored to 16 as
// Virtua Fighter 3's are, and two regions this port has nowhere to put: the MPEG music board's program
// and music (the DSB, which this port does not have, so the game plays without its music) and the
// drive board's. Its program is read like any other: whether it is a gzip is found in the CROM.
// (Layout: Supermodel's Config/Games.xml, read as documentation.)
const RomFile SR2_ROMS[] = {
	{ "epr-20635.20", 0x7937473F, ROM_CROM, 0x0000000, 8, 2, true },
	{ "epr-20634.19", 0x45A09245, ROM_CROM, 0x0000002, 8, 2, true },
	{ "epr-20633.18", 0xF5A24F24, ROM_CROM, 0x0000004, 8, 2, true },
	{ "epr-20632.17", 0x6829A801, ROM_CROM, 0x0000006, 8, 2, true },
	{ "mpr-20605.4", 0x00513401, ROM_BANKED, 0x0000000, 8, 2, true },
	{ "mpr-20604.3", 0x99C5F396, ROM_BANKED, 0x0000002, 8, 2, true },
	{ "mpr-20603.2", 0xAD0D8EB8, ROM_BANKED, 0x0000004, 8, 2, true },
	{ "mpr-20602.1", 0x60CFA72A, ROM_BANKED, 0x0000006, 8, 2, true },
	{ "mpr-20609.8", 0xC03CC0E5, ROM_BANKED, 0x1000000, 8, 2, true },
	{ "mpr-20608.7", 0x0C9B0571, ROM_BANKED, 0x1000002, 8, 2, true },
	{ "mpr-20607.6", 0x6DA85AA3, ROM_BANKED, 0x1000004, 8, 2, true },
	{ "mpr-20606.5", 0x072498FD, ROM_BANKED, 0x1000006, 8, 2, true },
	{ "mpr-20613.12", 0x2938C0D9, ROM_BANKED, 0x2000000, 8, 2, true },
	{ "mpr-20612.11", 0x721A44B6, ROM_BANKED, 0x2000002, 8, 2, true },
	{ "mpr-20611.10", 0x5D9F8BA2, ROM_BANKED, 0x2000004, 8, 2, true },
	{ "mpr-20621.31", 0x3A99148F, ROM_VROM, 0x000000A, 32, 2, false },
	{ "mpr-20622.32", 0x0618F056, ROM_VROM, 0x000000C, 32, 2, false },
	{ "mpr-20623.33", 0xCCF31B85, ROM_VROM, 0x000000E, 32, 2, false },
	{ "mpr-20624.34", 0x90F30936, ROM_VROM, 0x0000010, 32, 2, false },
	{ "mpr-20625.35", 0x04F804FA, ROM_VROM, 0x0000012, 32, 2, false },
	{ "mpr-20626.36", 0x2D6C97D6, ROM_VROM, 0x0000014, 32, 2, false },
	{ "mpr-20627.37", 0xA14EE871, ROM_VROM, 0x0000016, 32, 2, false },
	{ "mpr-20628.38", 0xBBA829A3, ROM_VROM, 0x0000018, 32, 2, false },
	{ "mpr-20629.39", 0xEAD2EB31, ROM_VROM, 0x000001A, 32, 2, false },
	{ "mpr-20630.40", 0xCC5881B8, ROM_VROM, 0x000001C, 32, 2, false },
	{ "mpr-20631.41", 0x5CB69FFD, ROM_VROM, 0x000001E, 32, 2, false },
	{ "epr-20636.21", 0x7139EBF8, ROM_SOUND, 0x0000000, 1, 1, false },
	{ "mpr-20614.22", 0xA3930E4A, ROM_SAMPLES, 0x0000000, 1, 1, false },
	{ "mpr-20615.24", 0x62E8A94A, ROM_SAMPLES, 0x0400000, 1, 1, false },
};

const Profile VF3_REV_D = {
	"vf3", "Virtua Fighter 3 (Revision D)", "Virtua Fighter 3", nullptr, true, REV_D_ROMS, sizeof REV_D_ROMS / sizeof REV_D_ROMS[0],
	{ 0x10, 0, false, { 0x200000, 0x4000000, 0x2000000, 0x80000, 0x800000 } },
	// the boot copy (docs/step1-measure.md)
	0x710000, 0xA0000,
	// FREE PLAY: the fight's line at 0x7CAE0; the attract's picture 0x4018 (table 0x7CB50), drawn by bl 0x33300 at 0x4E9C0
	{ 0x7CACA, 0x7CAE0, 0x7CAF6, 0x7CB50, 0x4018, 0x4E9C0, 0x4BFE4941 },
	// the coin setting (found by setting each in the test menu)
	{ 0x1E033, 0x1E037, 0x1E03B },
	// GAME ASSIGNMENTS: rounds vs CPU 2, vs player 3, difficulty 6, demo sound 7, energies 8 and 12, stage select
	// 0x17, country 0x1E
	{ 0x1E000, 2, 3, 6, 7, 8, 12, 0x17, 0x1E },
	960, 0x102002, 0x106080, 0xE4046, 0x10601A, 0x10601B, 0x106029,
	0x108045,   // (found by choosing each fighter and comparing; 0-12 the select's, 13 Dural: issue #12)
	// (1 the boot, 3 the attract, 4 Start pressed, 5 the select, a challenger's and the stage select, 6-7 the VS screen
	// and the fight: logged through a 1P run and a challenger's; the scene byte above is 4 on player 2's and a
	// challenger's select, so it's no use there)
	0x102008,
	{ 0x1E0C, 0xFFB30004, 0x161614, 0x1617B4 },
	ppcrec::vf3_map, ppcrec::vf3_map_count,
	"",
	&REV_D_DURAL,
	// (r13 0x108000 - 0x1F50: the routines at 0x1ECFC-0x1ED54, seed * 0x10DCD + 1, the select's and the attract's)
	0x1060B0,
};

// Team Battle: its ROMs, and the addresses found so far; what isn't known yet is 0 (that feature off). Online: only
// with Team Battle (the game's name "vf3tb" and its ROM's hash), its select by the mode byte (below).
const Profile VF3_TB = {
	"vf3tb", "Virtua Fighter 3 Team Battle", "Virtua Fighter 3 Team Battle", "vf3", true, TB_ROMS, sizeof TB_ROMS / sizeof TB_ROMS[0],
	{ 0x10, 0, false, { 0x200000, 0x4000000, 0x2000000, 0x80000, 0x800000 } },
	0x710000, 0xA0000,
	// FREE PLAY: Revision D's text 0x61F0 on (0x82CD0), its picture's table 0x82D50, drawn by bl 0x37FFC at 0x5419C (the
	// same code as Revision D's 0x4E990-0x4E9C0, with one more argument): found by matching Revision D's code
	{ 0x82CBA, 0x82CD0, 0x82CE6, 0x82D50, 0x4018, 0x5419C, 0x4BFE3E61 },
	// the coin setting: Revision D's (FREE PLAY there shows FREE PLAY and Start needs no credit: checked)
	{ 0x1E033, 0x1E037, 0x1E03B },
	// GAME ASSIGNMENTS: Revision D's places (its test menu shows a change there: MATCH COUNT(1P)), a marker 0x2E31 in
	// the halves Revision D leaves 0; it has more (drink, naked, VS finish, energy recovery) not here yet. Its STAGE
	// SELECT: 0 CHALLENGER (Revision D's on). Its default mode at 0x23: 1 a plain Start's 1 vs 1 (Up + Start then 3 vs 3),
	// 0 3 vs 3 (Down + Start 1 vs 1); read as Start's pressed (found setting each byte there in turn, vf3run's VF3_POKE)
	{ 0x1E000, 2, 3, 6, 7, 8, 12, 0x17, 0x1E, 0x23 },
	// the test menu's byte (4 while it's up: checked), the versus count and the score: Revision D's (the same code
	// refers to them, r13-relative: tools found every reference unchanged); the select's byte is the mode's (5 from
	// Start through the select and the stage select, as Revision D's: what online's lockstep needs, a frame never
	// run on a guess there; Revision D's own 0xE4046 is 4 on player 2's select)
	960, 0x102002, 0x106080, 0x102008, 0x10601A, 0x10601B, 0x106029,
	0x108045,   // (the select's column: Revision D's place, the same code (0x305DC) loads it: checked choosing Dural there)
	0x102008,   // (3, 5, 7 in the attract, the select and a fight, as Revision D's)
	// the backdrops: Revision D's (a fight's sky found with no tile on screen wrong, as Revision D's: checked on the dojo),
	// but the backdrop's place at 0x16195C (Revision D's 0x1617B4 holds something else here: the routine that writes
	// the map, Revision D's 0x5CF00 at 0x624FC, sets r17 to 0x16195C where Revision D's sets it to 0x1617B4; checked:
	// +4, +8 follow the map's place found, Jeffry's beach)
	{ 0x1E0C, 0xFFB30004, 0x161614, 0x16195C },
	ppcrec::vf3tb_map, ppcrec::vf3tb_map_count,
	"vf3tb",
	&TB_DURAL,
	0x1060B0,   // (Revision D's: the same routines at 0x20DD4-0x20E30, r13 the same)
};

// FIGHTING VIPERS 2, the second program this port runs (Step 2.0 against Virtua Fighter 3's Step 1.0). What the
// profile says of it is what this branch had measured; the rest is 0, which is how Team Battle's unknowns are
// written above, and the online fields wait for an adapter of its own (there is no fv2_netplay yet).
//
// Its program is not copied from the CROM at all: the fixed CROM holds ONE GZIP FILE from offset 0, which the
// game's own boot stub inflates into RAM at 0 (romset.cpp's unpack_program; the header names it foo.bin,
// 897,285 bytes for 2,701,982 of program). So program_at and program_size are 0 and the loader hands the
// unpacked image over instead -- the board never needed it, only the recompiler does.
const Profile FV2_REV_A = {
	"fvipers2", "Fighting Vipers 2 (Japan, Revision A)", "Fighting Vipers 2", nullptr, true,
	FV2_ROMS, sizeof FV2_ROMS / sizeof FV2_ROMS[0],
	{ 0x20, 0x29260E96, true, { 0x800000, 0x4000000, 0x4000000, 0x80000, 0x1000000 } },
	0, 0,                              // the program: a gzip the boot stub inflates (above)
	{ 0, 0, 0, 0, 0, 0, 0 },           // FREE PLAY: never looked for on this program (its console way uses the setting)
	{ 0x17, 0, 0 },                    // the coin setting: one byte, and that is all of it on this board
	{ 0x1E000, 0, 0, 0, 0, 0, 0, 0, 0, 0 },   // GAME ASSIGNMENTS: the block's place; its fields are src/host/fv2_options.h's
	960, 0x102002,                     // the boot, unseen; the test menu's byte (4 while it is up)
	// The online fields, measured the 2026-11-01 over two-player runs of tools/scripts/fv2-versus.txt (free play,
	// both players in). The matches between two players since power-on: not known here, so the stages' list starts
	// where the game puts it (set_versus_count does nothing with 0). The select: the low byte of Sega's entry1 task
	// register at 0x00700000, which model3.cpp measured as 0 through the whole attract loop, 5 on the select screen
	// and 0x2A in a fight -- exactly what in_select asks of it. The score: 0x00581454 and 0x00581455, the two
	// players' rounds won, both back to 0 when the match is over; in a run where only player 1 played, 0x00581454
	// went 1, 2, 3 (this game is first to three) and 0x00581455 never moved, which is what says which is whose.
	// Who is in: 0x0058145A, bit 0 player 1 and bit 1 player 2, as Virtua Fighter 3's -- it read 3 with both
	// players in and 1 the moment player 2 was knocked out of the match, so it is a bitmask and not, as a comment
	// in model3.cpp had it from the code alone, a count of how many are playing. The select's column and the mode
	// byte are Dural's business and the backdrops', and this game has neither.
	0, 0x00700003, 0x00581454, 0x00581455, 0x0058145A,
	0, 0,
	{ 0, 0, 0, 0 },                    // the backdrops: this port draws Fighting Vipers 2's its own way
	ppcrec::fv2_map, ppcrec::fv2_map_count,
	"",                                // the player's data beside Virtua Fighter 3's, told apart by name (main.cpp)
	nullptr,                           // no Dural to play as here (two Kuhns is another matter)
	0,
};

// The other set. Its program ROMs differ, so every address of its program is another program's: nothing of it was
// measured, and nothing is claimed. It loads and runs; the extras this port adds to Revision A are not offered.
const Profile FV2O_REV = {
	"fvipers2o", "Fighting Vipers 2 (Japan)", "Fighting Vipers 2", "fvipers2", false,
	FV2O_ROMS, sizeof FV2O_ROMS / sizeof FV2O_ROMS[0],
	{ 0x20, 0x29260E96, true, { 0x800000, 0x4000000, 0x4000000, 0x80000, 0x1000000 } },
	0, 0,
	{ 0, 0, 0, 0, 0, 0, 0 },
	{ 0, 0, 0 },
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	0, 0,
	0, 0, 0, 0, 0, 0, 0,
	{ 0, 0, 0, 0 },
	nullptr, 0,
	"",
	nullptr,
	0,
};

const Profile SRALLY2 = {
	"srally2", "Sega Rally 2 (Export)", "Sega Rally 2", nullptr, false, SR2_ROMS, sizeof SR2_ROMS / sizeof SR2_ROMS[0],
	{ 0x20, 0, false, { 0x800000, 0x3000000, 0x4000000, 0x80000, 0x800000 } },
	0, 0,
	{ 0, 0, 0, 0, 0, 0, 0 },
	{ 0, 0, 0 },
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	0, 0,
	0, 0, 0, 0, 0, 0, 0,
	{ 0, 0, 0, 0 },
	nullptr, 0,
	"",
	nullptr,
	0,
};

const Profile *const PROFILES[] = { &VF3_REV_D, &VF3_TB, &FV2_REV_A, &FV2O_REV, &SRALLY2, nullptr };

const Profile *find_profile(const char *set)
{
	for (const Profile *const *p = PROFILES; *p; p++)
		if (set && strcmp((*p)->set, set) == 0) return *p;
	return nullptr;
}

} // namespace m3
