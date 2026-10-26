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

#include <cstddef>
#include <cstdint>

namespace scsp_contract {

// A field as the implementation reads it: value = (word[mot] >> decalage) & masque. The triple is kept in
// exactly that form, and not as a bit range, so the test can compare it to the macro character for
// character. The comments give the human-readable range.
//
// CAREFUL, AND THIS COST AN HOUR ON 2026-10-26: the table MIXES TWO CONVENTIONS, because the implementation
// does. Most fields are a value to shift -- `(word >> 6) & 0x3F`. But every single-bit field is written
// `(word >> 0) & 0x1000`: THE BIT IN PLACE, a mask with no shift. KYONEX, KYONB, PCM8B, EGHOLD, LPSLNK,
// LFORE, STWINH and SDIR are of that kind.
//
// So a reader that blindly does `(value & masque) << decalage` writes ZERO for those. That is exactly what
// happened the first time tools/son/test_scsp_core.cpp ran: the voice stayed silent and the failure message
// blamed the contract, while the fault was in the caller. The two are told apart by the SHAPE of the mask --
// a contiguous low-bit mask (1, 3, 7, 0x1F, 0x3F...) is a value, anything else is a bit in place:
//
//     const bool bit_en_place = (masque & (masque + 1)) != 0;
//
// The convention is kept rather than normalised, because the contract's job is to say what the
// implementation does, and its test compares the two literally.
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
inline constexpr unsigned MCIPD   = 0x42C;
inline constexpr unsigned MCIRE   = 0x42E;

// AND THESE FOUR DO NOT ALL BEHAVE THE SAME WAY in the implementation we are leaving, which matters because
// a rewrite that makes them uniform changes behaviour without meaning to. Measured 2026-10-26 by
// tools/son/test_scsp_regs.cpp, which keeps the three cases:
//
//   0x420 SCIPD : the write is an OR of bit 5 only, so bit 5 can be SET and never cleared by a write.
//                 That is the intended exception; scsp.cpp says so in a comment.
//   0x422 SCIRE : an ordinary whole register (mask 0xFFFF). ResetInterrupts reads it, and the driver WRITES
//                 it twice -- 0x80 at 0x600792 and 0x40 at 0x6032FE: the timer acknowledgement.
//   0x42C MCIPD : a plain assignment -- but UpdateReg sees bit 5 and calls MainCheckPendingIRQ(0x20), and a
//                 READ copies m_mcipd back over the word. So bit 5 also comes back, by another route.
//   0x42E MCIRE : receives the OR exception that the comment intends FOR MCIPD. Its bit 5 therefore can
//                 never be cleared, while its twin SCIRE is an ordinary register. The asymmetry is MAME's,
//                 not the chip's as far as any reading can tell: no datasheet we have says it.
//
// WHAT THIS COSTS VF3: nothing measurable, and that is itself a measurement rather than a hope. All 28
// accesses to the SCSP's common registers in the whole reached program are absolute long addresses
// (desas_son.py --cherche 100400 1005ff), and not one of them names 0x42C or 0x42E; the only other form is
// the `lea $100418, a0` at 0x60028C, whose loop copies exactly the ten words 0x418-0x42A that section 3's
// table holds. So the driver never touches MCIPD or MCIRE. A rewrite may reproduce the asymmetry or fix it
// deliberately -- but it should not discover it by ear, and it should not "clean it up" believing it is
// unobservable on other hardware.
//
// AND THE SEARCH THAT PROVED IT HAD TO BE AIMED FIRST: `--cherche 0x42c` returns nothing, but so does
// `--cherche 0x418`, a register the driver demonstrably writes. The board maps the chip at 0x100000
// (soundboard.h), so the program names 0x100418, never 0x418. A probe that answers "0 found" to a question
// it cannot see is worse than no probe.

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

// ---------------------------------------------------------------------------------------------------
// 8. WRITING A FIELD, which the two conventions of section 0 make less obvious than it looks.
//
// This lives here and not in a test because BOTH tests need it, and two copies of a rule about which
// fields shift and which do not is two chances to disagree. The first version of it was in
// test_scsp_core.cpp alone, and it shifted blindly: KYONB went out as zero, the voice stayed silent, and
// the failure message accused the contract.
// ---------------------------------------------------------------------------------------------------
inline const Champ *cherche(const Champ *table, std::size_t n, const char *nom)
{
	for (std::size_t i = 0; i < n; i++) {
		const char *a = table[i].nom, *b = nom;
		while (*a && *a == *b) { ++a; ++b; }
		if (!*a && !*b) return table + i;
	}
	return nullptr;
}

// A slot's field, by name, or nullptr. The caller decides what a missing name means: a test should die,
// since a name that is not in the table is a typo and not a condition.
inline const Champ *champ_fente(const char *nom)
{
	return cherche(FENTE, sizeof FENTE / sizeof *FENTE, nom);
}

// Places `valeur` in the 16 words of a slot, where this table says the field lives.
//
// Returns false if the name is unknown, so a caller cannot silently write nothing. The two conventions are
// told apart by the SHAPE of the mask: a run of low contiguous bits (1, 3, 7, 0x1F, 0xFF...) is a value to
// shift, anything else is a bit already in place.
inline bool pose(std::uint16_t mot[16], const char *nom, unsigned valeur)
{
	const Champ *c = champ_fente(nom);
	if (!c) return false;
	const bool bit_en_place = (c->masque & (c->masque + 1)) != 0;
	if (bit_en_place) {
		if (valeur) mot[c->mot] = std::uint16_t(mot[c->mot] | c->masque);
		else        mot[c->mot] = std::uint16_t(mot[c->mot] & ~c->masque);
		return true;
	}
	mot[c->mot] = std::uint16_t((mot[c->mot] & ~(c->masque << c->decalage))
	                            | ((valeur & c->masque) << c->decalage));
	return true;
}

// ---------------------------------------------------------------------------------------------------
// 9. THE OSCILLATOR'S LOOPING, which is not what reading the code suggests.
//
// Measured 2026-10-26 by tools/son/test_scsp_fentes.cpp, which keeps the exact index sequences. Three
// facts, and a reimplementation that misses the third plays one sample too many in every loop:
//
//   (1) LSA AND LEA COUNT SAMPLES, while the 16-bit fetch counts BYTES. The fetch takes
//       `(cur_addr >> (SHIFT-1)) & ~1` -- two bytes per sample -- but the loop comparison takes
//       `cur_addr >> SHIFT`, one index. With LEA = 8 the period is 8 samples, not 4; measured.
//
//   (2) THE RAM FOLDS AT 1 MB AND SA LOSES ITS LOW BIT. read_word does `a &= 0xffffe`. So the sample
//       after the last word of RAM is the FIRST word of RAM, and an odd SA reads the same word as SA-1.
//
//   (3) THE LOOP TEST RUNS ON TWO ADDRESSES, AND ONE OF THEM IS A GHOST. The `for (addr_select...)` loop
//       in UpdateSlot applies the looping to `cur_addr` and to `nxt_addr`. But the line just above it is
//       `slot->nxt_addr = slot->cur_addr + (1 << SHIFT)`: nxt_addr is reassigned every sample, so
//       whatever the nxt_addr pass WRITES into the address is thrown away next tick -- while its SIDE
//       EFFECTS, `Backwards = 1` and `StopSlot`, survive. Since nxt_addr is always one index ahead, every
//       one of those side effects fires ONE SAMPLE EARLY, and no sample beyond LEA is ever read:
//           LPCTL=0 (no loop)   stops after index 6 with LEA=8, not after 7;
//           LPCTL=2 (reverse)   goes 0,1,2,3 then 6,5,4,7,6,5,4...  -- never 8;
//           LPCTL=3 (ping-pong) goes 0..7 then 6,5,4,5,6,7,6...     -- never 8.
//       Reading the code alone predicts an index 8 in modes 2 and 3. It is not there. A rewrite that
//       applies the looping to cur_addr only -- the obvious simplification -- shifts all four modes.
//
// Also measured, and smaller: PCM8B takes the HIGH byte of a word first (read_byte is big-endian, as the
// 68000 that fills the RAM) and sends it out `<< 8`, not stretched over 16 bits; SBCTL bit 0 is
// `sample ^= 0x7FFF` WITHOUT truncation while bit 1 is `(s16)(sample ^ 0x8000)` WITH it, so the two
// together give the negative minus one (1000 -> -1001); and SSCTL=3 returns from UpdateSlot before
// anything, so unlike SSCTL=2 it does not even advance the read head.

// ---------------------------------------------------------------------------------------------------
// 10. THE ENVELOPE, whose shape is two arithmetics and whose times are a table of constants.
//
// Measured 2026-10-26 by tools/son/test_scsp_adsr.cpp, which reads the multiplier sample by sample.
//
//   (1) TWO ARITHMETICS, not one. In UpdateSlot the ATTACK state multiplies the sample by the RAW VOLUME
//       -- `sample * EG_Update(slot) >> SHIFT`, linear -- while every other state goes through
//       m_EG_TABLE, a decibel curve of 3/32 dB per step, 95.9 dB end to end. The two nearly meet but not
//       quite: at saturation the attack path yields 4092 and the table 4096, so the FIRST sample of every
//       voice comes out 4 parts in 4096 below the next. Measured, not rounding noise.
//
//   (2) A VOICE DOES NOT START FROM SILENCE. StartSlot sets `EG.volume = 0x17F << EG_SHIFT`: 383 of 1023,
//       37 % of the way up. With AR=0 (m_ARTABLE[0] = 0, "infinite time") the multiplier sits at
//       383 << 2 = 1532 forever, which is how the test reads the constant back out.
//
//   (3) THE TIMES ARE CONSTANTS, NOT A CURVE. ARTimes/DRTimes are 64 values measured on hardware; the
//       gaps between neighbours follow no simple law. Reusing them is a fidelity CHOICE, and the test
//       pins it down in samples so the choice cannot be made by accident: with KRS=0, OCT=0, FNS=0, the
//       attack saturates after 1297 samples at AR=0x10, 332 at 0x14, 83 at 0x18, 24 at 0x1C, 1 at 0x1F.
//       A reimplementation that fits an exponential to these will differ here, which is the point.
//
//   (4) DECAY1 ALWAYS DECREMENTS ONCE, even when it does not last. Its exit test
//       (`volume >> (EG_SHIFT+5) <= EG.DL`) runs AFTER `volume -= EG.D1R`. So DL=0, which makes
//       EG.DL = 0x1F and the test true on the first tick, still loses one D1R step: the plateau reads
//       4052, not 4096. One index, 0.09 dB -- and a rewrite that tests before decrementing would pass
//       every other case here.
//
//   (5) DL IS INVERTED (`EG.DL = 0x1f - DL`), so the plateau where D2R takes over is m_EG_TABLE at index
//       (0x1F - DL) * 32 + 31: 258 for DL=8, 16 for DL=0x10, 1 for DL=0x18.
//
//   (6) DECAY2 CONSULTS THE REGISTER, NOT THE TABLE. `if (D2R(slot) == 0) return` reads the register
//       field, not EG.D2R. The two can disagree -- a non-zero D2R can give a zero EG.D2R at a low rate --
//       and then the level does not freeze, it falls by zero per sample. It sounds the same and is not.
//
//   (7) EGHOLD HOLDS THE OUTPUT, NOT THE VOLUME. In ATTACK, `if (EG.EGHOLD) return 0x3ff << (SHIFT-10)`
//       comes first: the volume still climbs, but the output is full from the first sample. A slow attack
//       then does nothing audible except delay DECAY1.
//
//   (8) LPSLNK ENDS THE ATTACK FROM THE OSCILLATOR, and it is the one place where this block depends on
//       section 9's. The ATTACK-to-DECAY1 transition is inside `if (!LPSLNK(slot))`, so with LPSLNK the
//       state stays ATTACK when the volume saturates; it leaves only through the loop's
//       `if (addr1 >= LSA && !Backwards)`. With LSA=20 that is NINETEEN samples on the attack path, not
//       twenty: the LPSLNK test runs BEFORE the envelope is applied, so the twentieth sample is already
//       using the table.
//
//   (9) KRS=0xF MEANS ZERO, not "ignore KRS". `rate = octave + 2*KRS + ((FNS>>9)&1)` except at 0xF, where
//       rate = 0 outright -- octave and the FNS bit included. Measured at OCT=2, AR=0x18: KRS=0 saturates
//       in 56 samples, KRS=1 in 45, the FNS bit alone in 50, and KRS=0xF in 83 -- which is exactly the
//       OCT=0 figure, so the octave really is discarded and not halved.
//
//  (10) A SECOND KYONEX ON A SOUNDING VOICE DOES NOTHING. The scan starts a slot only
//       `if (KEYONB(s2) && s2->EG.state == SCSP_RELEASE)`. A rewrite that restarted on every KYONEX
//       would make every repeated note in the game click.

} // namespace scsp_contract
