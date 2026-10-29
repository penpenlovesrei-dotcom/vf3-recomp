// dsp_maison.h -- the project's effects DSP: block 5, written against its specification.
//
// Block 5 of docs/scsp_architecture.md §3, and the last of the seven. The other six were written from
// src/sound/scsp_contract.h, which says what the chip MUST do. This one could not be: its instruction set
// exists nowhere WRITTEN DOWN -- no datasheet, no independent specification, only MAME's and Supermodel's
// implementations, both out of ElSemi's same reverse-engineering work (docs/scsp_references.md).
//
// SO THE SPECIFICATION WAS WRITTEN FIRST, on 2026-10-27, and it is against that one that this file exists:
// tools/son/test_scsp_dsp.cpp, eight cases that each lay down a tiny program and RECORD what the DSP makes
// of it, observed through the register window (TEMP, MEMS, MIXS, EFREG) and through sound RAM.
//
// -----------------------------------------------------------------------------------------------------
// WHAT IS VALIDATED, AND BY WHAT. Read this before trusting this file.
//
//   THE SPECIFICATION'S EIGHT CASES cover the memory input chain (MRD, IWT, IRA), the product, the four
//   shifts, MWT, EWT's accumulation and the PACK/UNPACK round trip.
//
//   THE REPLAYER covers everything the GAME exercises, and it covers it byte for byte: 3 219 865 samples
//   per chip over the whole scene of fv2-fight.txt. That is the real test, and it is worth more than any
//   number of unit cases -- but it covers only what VF3's programs use.
//
//   FOUR FIELDS ARE EXERCISED BY NO GAME, and it is a measurement that says so: NONE of the games this
//   port serves sets FRCL, ADRL, YRL or NEGB -- 152 program writes, five distinct shapes, three games
//   including Sega Rally 2, which has an altogether different program (docs/scsp_audit.md §5, 2026-10-28).
//   So there is no program to go and find that would let the replayer judge them.
//
//   THEY ARE NOW TESTED AGAINST THE TARGET, and the two sentences have to be read together:
//   tools/son/test_scsp_dsp.cpp cases 9 to 13, written on 2026-10-28, measure them one by one -- NEGB
//   through B's sign, FRCL and YRL by reading the registers back through YSEL (their only read path), ADRL
//   through the address that ADREB shifts. This core returns the same numbers as MAME's on all five, first
//   time, and four faults injected into this file are each caught by their own case.
//
//   WHAT THAT DOES NOT SAY: that MAME is right. Nobody has measured those four on hardware, and
//   docs/scsp_references.md refuses consensus between emulators as proof. The behaviour this port AIMS at
//   is MAME's (see scsp_maison.h); these cases establish that this DSP reaches it, and nothing more. Case
//   13 adds what no game gave either: that the second body, the one the four trigger, agrees with the
//   first. The count comes from the `dspprog` probe, and it carries its control in the same line --
//   `frcl 0 adrl 0 yrl 0 negb 0` next to `iwt 23 twt 39 mrd 23 mwt 10 ewt 5`, counted by the same code at
//   the same moment (docs/fv2-son-programme.md §4). So those four are WRITTEN but never EXERCISED: another
//   game, or an arena nobody visited, would be the first to try them.
// -----------------------------------------------------------------------------------------------------
//
// AN INSTRUCTION'S ENCODING, four sixteen-bit words per step. This is a hardware layout, no less than a
// register map:
//
//     word 0 : TRA 14-8, TWT 7, TWA 6-0
//     word 1 : XSEL 15, YSEL 14-13, IRA 11-6, IWT 5, IWA 4-0
//     word 2 : TABLE 15, MWT 14, MRD 13, EWT 12, EWA 11-8, ADRL 7, FRCL 6, SHIFT 5-4,
//              YRL 3, NEGB 2, ZERO 1, BSEL 0
//     word 3 : NOFL 15, COEF 14-9, MASA 6-2, ADREB 1, NXADR 0
//
// THE PUBLIC MEMBERS' NAMES ARE THE OTHER DSP'S, deliberately: the project's core (scsp_maison) reaches
// them directly, and swapping DSPs must change nothing there.
#pragma once

#include <cstdint>

class dsp_maison {
public:
	using u8  = std::uint8_t;
	using u16 = std::uint16_t;
	using u32 = std::uint32_t;
	using s16 = std::int16_t;
	using s32 = std::int32_t;
	using s64 = std::int64_t;

	// --- what the chip exposes, and what the register window reads or writes
	u16 *ram = nullptr;              // sound RAM, in 16-bit words
	u32 ram_bytes = 0;
	u32 RBP = 0;                     // the delay line's origin, in steps of 4 kwords
	u32 RBL = 8 * 1024;              // its size in words

	s16 COEF[64] = {};
	u16 MADRS[32] = {};
	u16 MPRO[128 * 4] = {};
	s32 TEMP[128] = {};              // signed 24-bit
	s32 MEMS[32] = {};               // signed 24-bit
	s32 MIXS[16] = {};               // signed 20-bit
	s16 EXTS[2] = {};                // nothing is plugged into these on this board
	s16 EFREG[16] = {};              // the effects output

	void Init();
	void Start();                    // (re)decodes the whole program and finds its last step
	void Decode(int pas);            // one step, decoded on write and not in the loop
	void SetSample(s32 echantillon, s32 sel, s32 mxl);
	void Step();                     // one sample

private:
	// One decoded step. DECODING ON WRITE is this repository's advance over MAME and Supermodel, which
	// extract the fields on every sample: 112 steps times 44 100 times two chips, for a program that only
	// changes between two arenas (docs/scsp_references.md).
	// ONE WORD OF FLAGS, AND NOT FIFTEEN BOOLEANS. The one-bit fields fit in an integer read once, and the
	// loop tests masks instead of loading fifteen bytes per step. Measured: going from fifteen booleans to
	// this word brought the core down from 22.93 s to what Step's comment states.
	enum Indic : u32 {
		I_TWT = 1u << 0, I_XSEL = 1u << 1, I_IWT = 1u << 2, I_TABLE = 1u << 3,
		I_MWT = 1u << 4, I_MRD = 1u << 5, I_EWT = 1u << 6, I_ZERO = 1u << 7,
		I_BSEL = 1u << 8, I_NOFL = 1u << 9, I_ADREB = 1u << 10, I_NXADR = 1u << 11,
		// The four this game never exercises, grouped so that a single test rules them all out.
		I_FRCL = 1u << 12, I_ADRL = 1u << 13, I_YRL = 1u << 14, I_NEGB = 1u << 15,
		I_RARES = I_FRCL | I_ADRL | I_YRL | I_NEGB,
		I_SHPRE = 1u << 16,          // the shift doubles the accumulator first (SHIFT 1 and 2)
		I_SHECR = 1u << 17,          // it CLAMPS (SHIFT 0 and 1) instead of sign-extending (2 and 3)
	};
	struct Pas {
		u32 f = 0;                   // the one-bit fields
		u8  tra = 0, twa = 0, ira = 0, iwa = 0, ewa = 0, coef = 0, masa = 0;
		u8  shift = 0, ysel = 0;
	};
	Pas m_prog[128] = {};
	int m_dernier = 0;               // how many steps to execute
	bool m_arrete = true;            // before the first Start, the DSP does not run

	// THE BODY SPECIALISED ON WHAT THE PROGRAM ACTUALLY USES, and it is a measurement that demands it:
	// without it the project's core cost 1% of a core more per chip than MAME's, that is a quarter more
	// over the whole sound -- when the CPU gain is the specification's highest priority. `m_rares` says
	// whether A SINGLE step of the program uses FRCL, ADRL, YRL or NEGB; this game's programs use none of
	// them, so the lean version runs, and the four tests disappear from the 112 steps each sample executes.
	//
	// It never comes undone mid-run: a program rewritten to stop using one of the four keeps the full body
	// until the next Start. That costs speed and nothing else.
	bool m_rares = false;
	template <bool RARES> void boucle();

	// The live state. ACC and SHIFTED are two distinct registers, and that is the whole subject: the
	// shifter is ONE STEP BEHIND the accumulator (see Step).
	s32 m_acc = 0;
	s32 m_memval = 0;
	s32 m_frc_reg = 0, m_y_reg = 0, m_adrs_reg = 0;
	u32 m_dec = 0;                   // counts down by one per sample; indexes TEMP and the delay line

	u16 lire_mot(u32 octet) const { return octet < ram_bytes ? ram[octet >> 1] : 0; }
	void ecrire_mot(u32 octet, u16 v) { if (octet < ram_bytes) ram[octet >> 1] = v; }
};
