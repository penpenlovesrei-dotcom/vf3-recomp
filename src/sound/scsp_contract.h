// scsp_contract.h -- what an SCSP implementation must provide for this port's sound driver to work.
//
// WHY THIS FILE EXISTS. The plan for getting off MAME's SCSP (docs/scsp_migration_plan.md) names two
// prerequisites: a differential harness, which exists (tools/son/comparer-son.cmd), and an INTERFACE
// CONTRACT, which did not. What the chip must do was known only as prose scattered through a 2800-line
// document. This is that contract, in a form an implementation can be written against and a test can check.
//
// WHAT IT IS AND IS NOT. Every number here comes from one of three places, and each is named at the line:
//
//   (a) the sound driver's own 68000 code, read instruction by instruction (docs/fv2-son-programme.md);
//   (b) a measurement on this port, with the probe and the scene said;
//   (c) the implementation this port runs today (src/sound/scsp.cpp), for field positions.
//
// It is NOT a datasheet. Nothing here was measured on a Model 3. Where the port knowingly departs from the
// hardware it is marked DEVIATION, with the reason, because a reimplementation that silently "fixed" those
// would change what the games sound like.
//
// AND IT IS NOT THE C++ INTERFACE. docs/scsp_architecture.md already describes that -- what `SoundBoard`
// calls on the chip and what the chip calls back, and what of it is MAME's baggage. The two are the two
// halves of the same prerequisite: that document says how an implementation is PLUGGED IN, this header says
// what it must DO.
//
// HOW IT IS KEPT HONEST. tools/son/test_scsp_contract.cpp checks every field below against the macros in
// src/sound/scsp.cpp and every driver constant against the sound ROM. It fails if a macro has moved, and it
// fails LOUDLY if it cannot find one at all -- a contract that silently stops being checked is worse than no
// contract.
#pragma once

#include <cstdint>

namespace scsp_contract {

// A field as the implementation reads it: value = (word[mot] >> decalage) & masque. The triple is kept in
// exactly that form, and not as a bit range, so the test can compare it to the macro character for
// character. `bits_hauts`/`bits_bas` in the comments give the human-readable range.
struct Champ {
	const char *nom;
	unsigned    mot;          // word index inside the block (byte offset = 2 * mot)
	unsigned    decalage;
	unsigned    masque;
};

// ---------------------------------------------------------------------------------------------------
// 1. A SLOT. 32 of them per chip, 0x20 bytes each, at 0x000-0x3FF of the register window.
//    (c) for the positions; the driver's use of each is in the comment.
// ---------------------------------------------------------------------------------------------------
inline constexpr Champ FENTE[] = {
	// word 0x0 (byte 0x00) -- key on, sample control, SA's high nibble
	{ "KYONEX", 0x0, 0x0, 0x1000 },  // bit 12. The driver pulls the trigger with `ori.b #$8, $0(a5)` (0x6010FC)
	{ "KYONB",  0x0, 0x0, 0x0800 },  // bit 11
	{ "SBCTL",  0x0, 0x9, 0x0003 },  // bits 10-9
	{ "SSCTL",  0x0, 0x7, 0x0003 },  // bits  8-7
	{ "LPCTL",  0x0, 0x5, 0x0003 },  // bits  6-5
	{ "PCM8B",  0x0, 0x0, 0x0010 },  // bit 4
	{ "SA_HI",  0x0, 0x0, 0x000F },  // bits 3-0: SA[19:16]
	{ "SA_LO",  0x1, 0x0, 0xFFFF },  // word 0x2: SA[15:0]
	{ "LSA",    0x2, 0x0, 0xFFFF },  // word 0x4
	{ "LEA",    0x3, 0x0, 0xFFFF },  // word 0x6
	// word 0x4 (byte 0x08) -- the attack half of the envelope. Written by CC 0x30/0x32/0x33 into a RAM
	// template, not straight to the chip (docs 5 bis undecies).
	{ "D2R",    0x4, 0xB, 0x001F },  // bits 15-11
	{ "D1R",    0x4, 0x6, 0x001F },  // bits 10-6
	{ "EGHOLD", 0x4, 0x0, 0x0020 },  // bit 5
	{ "AR",     0x4, 0x0, 0x001F },  // bits 4-0
	// word 0x5 (byte 0x0A) -- the release half. CC 0x31 writes DL, CC 0x34 writes RR.
	{ "LPSLNK", 0x5, 0x0, 0x4000 },  // bit 14
	{ "KRS",    0x5, 0xA, 0x000F },  // bits 13-10
	{ "DL",     0x5, 0x5, 0x001F },  // bits  9-5
	{ "RR",     0x5, 0x0, 0x001F },  // bits  4-0
	// word 0x6 (byte 0x0C) -- level, and the write-inhibit that matters for FM (see section 5)
	{ "STWINH", 0x6, 0x0, 0x0200 },  // bit 9
	{ "SDIR",   0x6, 0x0, 0x0100 },  // bit 8
	{ "TL",     0x6, 0x0, 0x00FF },  // bits 7-0
	// word 0x7 (byte 0x0E) -- FM. The driver arms it from an instrument byte (0x60153A).
	{ "MDL",    0x7, 0xC, 0x000F },  // bits 15-12
	{ "MDXSL",  0x7, 0x6, 0x003F },  // bits 11-6
	{ "MDYSL",  0x7, 0x0, 0x003F },  // bits  5-0
	// word 0x8 (byte 0x10) -- pitch. Two different driver paths write it (section 4).
	{ "OCT",    0x8, 0xB, 0x000F },  // bits 14-11, signed
	{ "FNS",    0x8, 0x0, 0x03FF },  // bits  9-0
	// word 0x9 (byte 0x12) -- the LFO. CC 1, 2, 0x14, 0x15 and 0x18 each edit one field of it.
	{ "LFORE",  0x9, 0x0, 0x8000 },  // bit 15. The driver masks it OFF when setting the others (andi.w #$7fff)
	{ "LFOF",   0x9, 0xA, 0x001F },  // bits 14-10
	{ "PLFOWS", 0x9, 0x8, 0x0003 },  // bits  9-8
	{ "PLFOS",  0x9, 0x5, 0x0006 },  // DEVIATION, see PLFOS_ECART below
	{ "ALFOWS", 0x9, 0x3, 0x0003 },  // bits  4-3
	{ "ALFOS",  0x9, 0x0, 0x0007 },  // bits  2-0
	// word 0xA (byte 0x14) -- the send INTO the effects DSP. CC 0x2B picks the input, CC 0x2A the level.
	{ "ISEL",   0xA, 0x3, 0x000F },  // bits 6-3
	{ "IMXL",   0xA, 0x0, 0x0007 },  // bits 2-0
	// word 0xB (byte 0x16) -- ALL FOUR output levels and pans in one word. The driver writes the two
	// halves as separate BYTES: CC 10 writes byte 0x16 (direct), 0x602EB0 writes byte 0x17 (effect).
	{ "DISDL",  0xB, 0xD, 0x0007 },  // bits 15-13
	{ "DIPAN",  0xB, 0x8, 0x001F },  // bits 12-8
	{ "EFSDL",  0xB, 0x5, 0x0007 },  // bits  7-5
	{ "EFPAN",  0xB, 0x0, 0x001F },  // bits  4-0
};

// DEVIATION, and it is deliberate. PLFOS is read WITHOUT ITS LOW BIT, as Supermodel does, set by ear for
// Virtua Fighter 3 and backed by a measurement that src/sound/scsp.cpp records at the macro: VF3's music has
// voices with PLFOS 1 (+-7 cents of vibrato) that, taken literally and with MAME's truncated pitch-LFO
// table, played 3 cents flat and made the reverb they feed ring up to 10 dB longer on Lion's theme. A
// reimplementation that reads all three bits will NOT sound like this port.
inline constexpr char PLFOS_ECART[] =
	"PLFOS is masked 0x0006 after a shift of 5, so its low bit is dropped; 3, 5, 7 arrive as 2, 4, 6";

// ---------------------------------------------------------------------------------------------------
// 2. THE COMMON REGISTERS, at 0x400 and up. (c) for the positions.
// ---------------------------------------------------------------------------------------------------
inline constexpr Champ COMMUN[] = {
	{ "MEM4B",   0x0, 0x0, 0x0200 },  // 0x400 bit 9. The driver writes 0x300 here (0x60016C): MEM4B and
	{ "DAC18B",  0x0, 0x0, 0x0100 },  // 0x400 bit 8   DAC18B both set, MVOL left at 0 and raised later
	{ "MVOL",    0x0, 0x0, 0x000F },  // 0x400 bits 3-0
	{ "RBL",     0x1, 0x7, 0x0003 },  // 0x402 bits 8-7: the DSP delay line's length
	{ "RBP",     0x1, 0x0, 0x003F },  // 0x402 bits 5-0: its base
	{ "MOFULL",  0x2, 0x0, 0x1000 },  // 0x404 bit 12. The driver SPINS on this before writing MOBUF
	{ "MOEMPTY", 0x2, 0x0, 0x0800 },  // 0x404 bit 11
	{ "MIOVF",   0x2, 0x0, 0x0400 },  // 0x404 bit 10
	{ "MIFULL",  0x2, 0x0, 0x0200 },  // 0x404 bit  9
	{ "MIEMPTY", 0x2, 0x0, 0x0100 },  // 0x404 bit  8
};

// Byte offsets the driver uses by name, where a field table would say less than the address does.
inline constexpr unsigned MIBUF   = 0x404;   // MIDI in
inline constexpr unsigned MOBUF   = 0x406;   // MIDI out; the driver writes the LOW byte, 0x407
inline constexpr unsigned MSLC_CA = 0x408;
inline constexpr unsigned TIMA    = 0x418;   // count in bits 7-0, prescaler in 10-8
inline constexpr unsigned TIMB    = 0x41A;
inline constexpr unsigned TIMC    = 0x41C;
inline constexpr unsigned SCIEB   = 0x41E;
inline constexpr unsigned SCIPD   = 0x420;
inline constexpr unsigned SCIRE   = 0x422;
inline constexpr unsigned SCILV0  = 0x424;
inline constexpr unsigned SCILV1  = 0x426;
inline constexpr unsigned SCILV2  = 0x428;
inline constexpr unsigned MCIEB   = 0x42A;

// ---------------------------------------------------------------------------------------------------
// 3. THE INTERRUPT CONTRACT. (a) read in the driver, and (c) for which bit each source raises.
//
// This is the part a reimplementation gets wrong most cheaply and most audibly: the music's tempo is one
// of these timers, and the driver's whole sequencer hangs off it.
//
// The driver writes ten words in one run at reset (0x600288 copies them from 0x6002D4 into 0x418-0x42A),
// so the configuration is not inferred from scattered writes -- it is one table, and these are its values.
// ---------------------------------------------------------------------------------------------------
struct Source { const char *nom; unsigned bit; unsigned niveau; bool armee; };

inline constexpr Source SOURCES[] = {
	{ "MIDI in", 3, 3, true  },   // SCIPD bit 3 (c); level from SCILV0/1/2 below
	{ "timer A", 6, 2, true  },   // timerA_cb sets 0x40 (c). THE MUSIC TICK.
	{ "timer B", 7, 1, true  },   // timerB_cb sets 0x80 (c)
	{ "timer C", 8, 0, false },   // timerC_cb sets 0x100 (c); SCIEB 0x00C8 leaves bit 8 clear
};

// The ten words the driver writes, in order, 0x418 to 0x42A. (a), 0x6002D4.
inline constexpr std::uint16_t INIT_418_42A[10] = {
	0x00CE,   // 0x418 TIMA  -- count 0xCE, prescaler 0: 0x100-0xCE = 50 prescaler ticks
	0x03C6,   // 0x41A TIMB  -- count 0xC6, prescaler 3
	0x0100,   // 0x41C TIMC  -- count 0x00, prescaler 1
	0x00C8,   // 0x41E SCIEB -- bits 3, 6, 7: MIDI in, timer A, timer B. Timer C is NOT armed
	0x0000,   // 0x420 SCIPD
	0x00C8,   // 0x422 SCIRE
	0x0088,   // 0x424 SCILV0
	0x0048,   // 0x426 SCILV1
	0x0000,   // 0x428 SCILV2
	0x0020,   // 0x42A MCIEB -- bit 5, towards the other side
};

// The level of a source is its bit read across the three SCILV, low word first:
//   niveau = (SCILV2[bit] << 2) | (SCILV1[bit] << 1) | SCILV0[bit]
// With 0x88 / 0x48 / 0x00 that gives MIDI in 3, timer A 2, timer B 1 -- the three SOURCES above.
inline constexpr unsigned niveau_de(unsigned bit)
{
	return ((INIT_418_42A[8] >> bit & 1) << 2) | ((INIT_418_42A[7] >> bit & 1) << 1)
	     | (INIT_418_42A[6] >> bit & 1);
}

// Timer A's period, three ways, and all three agree -- which is why the number is worth trusting:
//   (a) the ten-word table above says TIMA = 0xCE with prescaler 0;
//   (a) snd_irq2 reloads TIMA from [0x60806A] on every tick, and that ROM word is 0x00CE;
//   (b) src/sound/scsp.cpp already measured "50 samples, 882 a second, a sample short".
inline constexpr unsigned TIMER_A_COMPTE      = 0xCE;
inline constexpr unsigned TIMER_A_ECHANTILLONS = 0x100 - TIMER_A_COMPTE;   // 50
inline constexpr double   TIMER_A_HZ          = 44100.0 / TIMER_A_ECHANTILLONS;   // 882

// ---------------------------------------------------------------------------------------------------
// 4. PITCH. (a) read in 0x601390, (b) the table checked against a sine fit.
//
// The driver's internal pitch unit is a SIXTEENTH OF A SEMITONE, and it says so twice: the modulus it
// normalises by is 0x180 = 384 = 12 * 32 BYTES of a word table (so 16 words per semitone), and the ROM
// table at 0x603D62 restarts at its 192nd word. Two independent statements of the same number.
//
// An implementation does not need this -- it is the driver's arithmetic, not the chip's. It is here
// because a reimplementation of the DRIVER would, and because it pins what OCT/FNS must accept.
// ---------------------------------------------------------------------------------------------------
inline constexpr unsigned PAS_PAR_DEMI_TON   = 16;
inline constexpr unsigned MOTS_PAR_OCTAVE    = 12 * PAS_PAR_DEMI_TON;   // 192
inline constexpr unsigned MODULO_OCTAVE_OCTETS = 2 * MOTS_PAR_OCTAVE;   // 0x180
inline constexpr unsigned TABLE_FNS_ROM      = 0x603D62;                // in the sound program ROM
inline constexpr unsigned BIAIS_OCT          = 0x4000;                  // added before masking 0x7800
inline constexpr unsigned MASQUE_OCT_FNS     = 0x7BFF;                  // bits 15 and 10 are unused

// TWO WRITERS, TWO FORMULAS, and an implementation of the driver needs both:
//   0x601390  note on: the semitone count times 32, plus a 1/16 offset, through the ROM table above;
//   0x603928  velocity: (velocity * sensitivity) >> 8 + the instrument's base pitch, then (x<<1 + 0x4000).
// The same doubling exists on TL (section 6). This port does not have to care -- it runs the driver's own
// 68000 code -- but an autonomous core would.

// ---------------------------------------------------------------------------------------------------
// 5. THE FM RING BUFFER, and the one thing about it that is easy to get wrong.
//
// MDXSL and MDYSL index a ring of 64 entries that the audio path fills with recent output. The driver
// ALSO writes entries itself, at register 0x600 + slot*2 and 0x640 + slot*2 (0x60153A), to drive an FM
// voice from values of its own rather than from live audio.
//
// (b) Measured: in Fighting Vipers 2's arena 2, from frame 3783, 993 such writes -- and STWINH was set on
// the target slot for ALL 993, no exception. So an implementation MUST honour STWINH as "do not let the
// audio path overwrite this entry", or the driver's values are lost and that voice's FM is wrong.
// Eleven of the twelve arenas never write the ring buffer at all, which is why it took forcing an arena
// to find this (docs 4 sexies).
// ---------------------------------------------------------------------------------------------------
inline constexpr unsigned RINGBUF_BASE   = 0x600;
inline constexpr unsigned RINGBUF_BASE_2 = 0x640;
inline constexpr unsigned RINGBUF_ENTREES = 64;

// ---------------------------------------------------------------------------------------------------
// 6. THE EFFECTS DSP.
//
// Layout (a), and the one disputed range named for what it is:
//   0x700-0x77F  COEF,  64 words
//   0x780-0x7BF  MADRS, 32 words
//   0x7C0-0x7FF  DISPUTED: this port and MAME disagree on whether the hardware mirrors MADRS here
//   0x800-0xBFF  MPRO,  128 steps of 8 bytes
//
// What this document CAN say about 0x7C0-0x7FF, and it is about the driver and not the chip:
//   (a) the driver clears 0x700-0xBFF in one linear sweep and loads a preset the same way, so a hardware
//       mirror would destroy every preset during its own loading;
//   (b) the parameter interpreter at 0x602B32 writes 59 distinct addresses over all three presets, from
//       0x714 to 0x794, and NONE of them is in 0x7C0-0x7FF.
// Neither says what the chip does. That needs a Model 3, and is marked as out of reach, not open.
//
// (b) Measured, and it bears on cost: ~112 DSP steps per sample per chip, 44100 times a second. Under
// --rec the sound board is 30.5 % of a frame's work and the DSP is about half of that.
// ---------------------------------------------------------------------------------------------------
inline constexpr unsigned COEF_BASE  = 0x700, COEF_MOTS  = 64;
inline constexpr unsigned MADRS_BASE = 0x780, MADRS_MOTS = 32;
inline constexpr unsigned CONTESTE_BASE = 0x7C0, CONTESTE_MOTS = 32;
inline constexpr unsigned MPRO_BASE  = 0x800, MPRO_PAS   = 128;
inline constexpr unsigned PAS_DSP_PAR_ECHANTILLON = 112;   // (b)

// ---------------------------------------------------------------------------------------------------
// 7. WHAT THE DRIVER ASSUMES ABOUT THE BOARD, as opposed to the chip.
// ---------------------------------------------------------------------------------------------------
inline constexpr unsigned FENTES_PAR_PUCE = 32;
inline constexpr unsigned PUCES           = 2;
// A bank register outside the chips, at 0x400001, selects which half of the 16 MB of sample ROM is
// visible. (a) The sample loader at 0x60288A rewrites it FOR EVERY SAMPLE of a set, not once per set,
// because two samples of one set can be in two different halves.
inline constexpr unsigned REGISTRE_BANQUE = 0x400001;
// (a) The chips play only from their own RAM; the 68000 copies samples in, 512 bytes a pass, from the
// banked window (the unrolled copy at 0x601F3C). An implementation that reads samples straight from ROM
// would not need the copy -- and would then not be running this driver.

} // namespace scsp_contract
