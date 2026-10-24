// license:BSD-3-Clause
// copyright-holders:ElSemi, R. Belmont
// The SCSP's effects DSP, from MAME's sound/scspdsp.h (unchanged but for its memory: the sound
// RAM it shares with the 68000, instead of an address_space).
#pragma once
#include "../core/mame_shim.h"

//the DSP Context
struct SCSPDSP
{
//Config
	u16 *ram;        // the SCSP's address space: sound RAM, ram_bytes long (16-bit words, native order)
	u32 ram_bytes;
	u32 RBP; //Ring buf pointer
	u32 RBL; //Delay ram (Ring buffer) size in words

//context

	s16 COEF[64];     //16 bit signed
	u16 MADRS[32];   //offsets (in words), 16 bit
	u16 MPRO[128*4]; //128 steps 64 bit
	s32 TEMP[128];    //TEMP regs,24 bit signed
	s32 MEMS[32]; //MEMS regs,24 bit signed
	u32 DEC;

//input
	s32 MIXS[16]; //MIXS, 24 bit signed
	s16 EXTS[2];  //External inputs (CDDA)    16 bit signed

//output
	s16 EFREG[16];    //EFREG, 16 bit signed

	bool Stopped;
	int LastStep;

	// The program decoded once (by Start, when it is written) rather than at every step of every sample
	struct Inst {
		u8 TRA, TWT, TWA, XSEL, YSEL, IRA, IWT, IWA, TABLE, MWT, MRD, EWT, EWA, ADRL, FRCL, SHIFT, YRL, NEGB, ZERO, BSEL,
			NOFL, COEF, MASA, ADREB, NXADR;
		// The same controls again, the ones the loop reads at nearly every step, packed into one
		// word: a step was fetching eight separate bytes to test eight single bits, which is eight
		// loads where one does. The fields above stay as they are -- Decode and the program's own
		// report read them, and they say what they are.
		u16 f;
		// TABLE, ADREB, NXADR and NOFL, read only inside the memory block (a third of the steps)
		u8 mf;
	};
	enum {   // Inst::f
		F_ZERO = 0x001, F_BSEL = 0x002, F_XSEL = 0x004, F_TWT = 0x008,
		F_IWT = 0x010, F_EWT = 0x020, F_MRD = 0x040, F_MWT = 0x080,
		F_SHIFT = 0x300, F_SHIFT_SH = 8,   // 2 bits
		F_YSEL = 0xC00, F_YSEL_SH = 10,    // 2 bits
	};
	enum { M_TABLE = 1, M_ADREB = 2, M_NXADR = 4, M_NOFL = 8 };   // Inst::mf
	Inst prog[128];

	// Which of the four seldom-used controls the program as written touches. Fighting Vipers 2's
	// reverb -- 112 steps, measured -- sets none of FRCL, ADRL, YRL or NEGB at any step, and so
	// does Virtua Fighter 3's: testing for each of them at every step of every sample is four
	// branches a step for nothing, and it also keeps FRC_REG, Y_REG and ADRS_REG alive when they
	// can only ever be zero. Step runs a body without them for such a program, and the full body
	// for any other, so what comes out is the same either way.
	// Start works the set out over the whole program; Decode only ever ADDS what one step gains,
	// because a bit set when it need not be costs the slower body and nothing else.
	enum { USES_FRCL = 1, USES_ADRL = 2, USES_YRL = 4, USES_NEGB = 8, USES_ALL = 15 };
	unsigned uses;

	void Init();
	void SetSample(s32 sample, s32 SEL, s32 MXL);
	void Step();
	template <unsigned U> void Run();   // Step's body, with the controls U says the program needs
	void Start();
	void Decode(int step);   // one step of MPRO into prog (Start does them all)

	u16 read_word(u32 a) const { return a < ram_bytes ? ram[a >> 1] : 0; }
	void write_word(u32 a, u16 v) { if (a < ram_bytes) ram[a >> 1] = v; }
};
