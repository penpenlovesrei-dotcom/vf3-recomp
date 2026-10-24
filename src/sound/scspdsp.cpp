// license:BSD-3-Clause
// copyright-holders:ElSemi, R. Belmont
// From MAME's sound/scspdsp.cpp; the DSP reads and writes the sound RAM directly.
#include "scspdsp.h"
#include "sound_prof.h"

#include <cstring>


namespace {

u16 PACK(s32 val)
{
	int const sign = BIT(val, 23);
	u32 temp = (val ^ (val << 1)) & 0xFFFFFF;
	int exponent = 0;
	for (int k = 0; k < 12; k++)
	{
		if (temp & 0x800000)
			break;
		temp <<= 1;
		exponent += 1;
	}
	if (exponent < 12)
		val = (val << exponent) & 0x3FFFFF;
	else
		val <<= 11;
	val >>= 11;
	val &= 0x7FF;
	val |= sign << 15;
	val |= exponent << 11;

	return u16(val);
}

static s32 UNPACK(u16 val)
{
	int const sign = BIT(val, 15);
	int exponent = (val >> 11) & 0xF;
	int const mantissa = val & 0x7FF;
	s32 uval = mantissa << 11;
	if (exponent > 11)
	{
		exponent = 11;
		uval |= sign << 22;
	}
	else
	{
		uval |= (sign ^ 1) << 22;
	}
	uval |= sign << 23;
	uval <<= 8;
	uval >>= 8;
	uval >>= exponent;

	return uval;
}

} // anonymous namespace


void SCSPDSP::Init()
{
	u16 *const r = ram;
	const u32 n = ram_bytes;
	std::memset(this, 0, sizeof(*this));
	ram = r;
	ram_bytes = n;
	RBL = (8*1024); // Initial RBL is 0
	Stopped = true;
}

void SCSPDSP::Step()
{
	if (Stopped)
		return;
	// the body without the controls this program never sets (scspdsp.h: uses)
	if (uses == 0)
		Run<0>();
	else
		Run<USES_ALL>();
}

template <unsigned U>
void SCSPDSP::Run()
{
	std::fill(std::begin(EFREG), std::end(EFREG), 0);

#if 0
	int dump=0;
	FILE *f=nullptr;
	if(dump)
		f=fopen("dsp.txt","wt");
#endif

	s32 ACC = 0;    //26 bit
	s32 MEMVAL = 0;
	s32 FRC_REG = 0;    //13 bit
	s32 Y_REG = 0;      //24 bit
	u32 ADRS_REG = 0;  //13 bit

	// The loop's own copies of what it reads over and over. A step that writes the delay line writes
	// through `ram`, and the compiler has no way to know that the sound RAM is not inside this very
	// object: without these, DEC, RBL, RBP and LastStep have to be loaded again after every such
	// write, and so does every word of TEMP and MEMS. `rp` is restrict because the promise is a true
	// one -- the sound RAM is the 68000's, a separate allocation, and nothing else in this function
	// reaches it. None of the four can change while a step runs: DEC only after the loop, RBL and RBP
	// from the 68000 between samples, LastStep at Start.
	u32 const dec = DEC, rbl = RBL, rbp = RBP;
	int const last = LastStep;
	u16 *const __restrict rp = ram;
	u32 const rbytes = ram_bytes;
	auto const rword = [&](u32 a) -> u16 { return a < rbytes ? rp[a >> 1] : 0; };
	auto const wword = [&](u32 a, u16 v) { if (a < rbytes) rp[a >> 1] = v; };

	for (int step = 0; step < last; ++step)
	{
		const Inst &I = prog[step];
		// the single-bit controls in one word (scspdsp.h: Inst::f), the rest as they are
		u32 const F = I.f;
		u32 const TWT = F & F_TWT, XSEL = F & F_XSEL, IWT = F & F_IWT, EWT = F & F_EWT;
		u32 const MWT = F & F_MWT, MRD = F & F_MRD, ZERO = F & F_ZERO, BSEL = F & F_BSEL;
		u32 const SHIFT = (F & F_SHIFT) >> F_SHIFT_SH, YSEL = (F & F_YSEL) >> F_YSEL_SH;
		u32 const TRA = I.TRA, TWA = I.TWA, IRA = I.IRA, IWA = I.IWA, EWA = I.EWA, COEF = I.COEF;
		// ADRL, FRCL, YRL and NEGB are read where they are used, inside if constexpr (uses);
		// TABLE, ADREB, NXADR, NOFL and MASA only inside the memory block

		//operations are done at 24 bit precision
#if 0
		if (MASA)
			int a=1;
		if (NOFL)
			int a=1;

		//int dump=0;

		if (f)
		{
#define DUMP(v) fprintf(f, " " #v ": %04X", v);
			fprintf(f, "%d: ", step);
			DUMP(ACC);
			DUMP(SHIFTED);
			DUMP(X);
			DUMP(Y);
			DUMP(B);
			DUMP(INPUTS);
			DUMP(MEMVAL);
			DUMP(FRC_REG);
			DUMP(Y_REG);
			DUMP(ADDR);
			DUMP(ADRS_REG);
			fprintf(f, "\n");
#undef DUMP
		}
#endif

		//INPUTS RW
		// colmns97 hits this
		//assert(IRA < 0x32);
		s32 INPUTS; // 24-bit
		if (IRA <= 0x1f)
			INPUTS = MEMS[IRA];
		else if (IRA <= 0x2F)
			INPUTS = MIXS[IRA - 0x20] << 4;  //MIXS is 20 bit
		else if (IRA <= 0x31)
			INPUTS = EXTS[IRA - 0x30] << 8;  //EXTS is 16 bit
		else
			return;

		INPUTS = util::sext(INPUTS, 24);

		if (IWT)
		{
			MEMS[IWA] = MEMVAL;  // MEMVAL was selected in previous MRD
			if (IRA == IWA)
				INPUTS = MEMVAL;
		}

		//Operand sel
		// the same TEMP word feeds B and X: read and sign-extended once, not twice
		s32 const TEMPV = util::sext(TEMP[(TRA + dec) & 0x7f], 24);

		s32 B; // 26-bit
		if (!ZERO)
		{
			B = BSEL ? ACC : TEMPV;
			if constexpr (U & USES_NEGB)
				if (I.NEGB)
					B = 0 - B;
		}
		else
			B = 0;

		s32 const X = XSEL ? INPUTS : TEMPV; // 24-bit

		s32 Y = 0;  //13 bit
		// the COEF case first: 87 of Fighting Vipers 2's 112 steps take it, and 84 of 127 in the fight's
		if (YSEL == 1)
			Y = this->COEF[COEF] >> 3;   //COEF is 16 bits
		else if (YSEL == 0)
			Y = FRC_REG;
		else if (YSEL == 2)
			Y = (Y_REG >> 11) & 0x1fff;
		else if (YSEL == 3)
			Y = (Y_REG >> 4) & 0x0fff;

		if constexpr (U & USES_YRL)
			if (I.YRL)
				Y_REG = INPUTS;

		//Shifter
		s32 SHIFTED = 0;    //24 bit
		if (SHIFT == 0)
			SHIFTED = std::clamp<s32>(ACC, -0x00800000, 0x007fffff);
		else if (SHIFT == 1)
			SHIFTED = std::clamp<s32>(ACC * 2, -0x00800000, 0x007fffff);
		else if (SHIFT == 2)
			SHIFTED = util::sext(ACC * 2, 24);
		else if (SHIFT == 3)
			SHIFTED = util::sext(ACC, 24);

		//ACCUM
		Y = util::sext(Y, 13);

		s64 const v = (s64(X) * s64(Y)) >> 12;
		ACC = int(v + B);

		if (TWT)
			TEMP[(TWA + dec) & 0x7f] = SHIFTED;

		if constexpr (U & USES_FRCL)
			if (I.FRCL)
			{
				if (SHIFT == 3)
					FRC_REG = SHIFTED & 0x0fff;
				else
					FRC_REG = (SHIFTED >> 11) & 0x1fff;
			}

		if (MRD || MWT)
		//if (0)
		{
			u32 const MF = I.mf, TABLE = MF & M_TABLE, NOFL = MF & M_NOFL;
			u32 ADDR = MADRS[I.MASA];
			if (!TABLE)
				ADDR += dec;
			if (MF & M_ADREB)
				ADDR += ADRS_REG & 0x0FFF;
			if (MF & M_NXADR)
				ADDR++;
			if (!TABLE)
				ADDR &= rbl - 1;
			else
				ADDR &= 0xffff;
			ADDR += rbp << 12;
			ADDR <<= 1;
			if (MRD && (step & 1)) //memory only allowed on odd? DoA inserts NOPs on even
			{
				if (NOFL)
					MEMVAL = rword(ADDR) << 8;
				else
					MEMVAL = UNPACK(rword(ADDR));
			}
			if (MWT && (step & 1))
			{
				if (NOFL)
					wword(ADDR, SHIFTED >> 8);
				else
					wword(ADDR, PACK(SHIFTED));
			}
		}

		if constexpr (U & USES_ADRL)
			if (I.ADRL)
			{
				if (SHIFT == 3)
					ADRS_REG = (SHIFTED >> 12) & 0xfff;
				else
					ADRS_REG = INPUTS >> 16;
			}

		if (EWT)
			EFREG[EWA] += SHIFTED >> 8;
	}
	DEC = dec - 1;
	if (sndprof::on) sndprof::dsp_steps += unsigned(last);
	std::fill(std::begin(MIXS), std::end(MIXS), 0);
	//if (f)
		//fclose(f);
}

void SCSPDSP::SetSample(s32 sample, int SEL, int MXL)
{
	//MIXS[SEL] += sample << (MXL + 1)/*7*/;
	MIXS[SEL] += sample;
	//if (MXL)
		//int a = 1;
}

void SCSPDSP::Start()
{
	Stopped = false;
	int i;
	for (i = 127; i >= 0; --i)
	{
		u16 const *const IPtr = MPRO + (i * 4);
		if (IPtr[0] || IPtr[1] || IPtr[2] || IPtr[3])
			break;
	}
	LastStep = i + 1;
	uses = 0;   // worked out afresh over the whole program (scspdsp.h)
	for (int step = 0; step < 128; step++) Decode(step);
	if (sndprof::on) {   // what the program is made of, so an optimisation aims at what is there
		int n[16] = {};
		for (int step = 0; step < LastStep; step++) {
			const Inst &I = prog[step];
			n[0] += I.IWT; n[1] += I.TWT; n[2] += I.FRCL; n[3] += I.ADRL; n[4] += I.YRL;
			n[5] += I.MRD; n[6] += I.MWT; n[7] += (I.MRD || I.MWT); n[8] += ((I.MRD || I.MWT) && (step & 1));
			n[9] += I.EWT; n[10] += I.ZERO; n[11] += I.BSEL; n[12] += I.NEGB; n[13] += I.XSEL;
			n[14] += (I.YSEL == 1); n[15] += (I.IRA <= 0x1f);
		}
		fprintf(stderr, "dspprog %d steps: iwt %d twt %d frcl %d adrl %d yrl %d | mrd %d mwt %d mem %d mem-odd %d | ewt %d | zero %d bsel %d negb %d xsel %d ysel=coef %d ira<mems %d\n",
		        LastStep, n[0], n[1], n[2], n[3], n[4], n[5], n[6], n[7], n[8], n[9], n[10], n[11], n[12], n[13], n[14], n[15]);
	}
}

void SCSPDSP::Decode(int step)
{
	{
		u16 const *const IPtr = MPRO + (step * 4);
		Inst &I = prog[step];
		I.TRA = (IPtr[0] >> 8) & 0x7f; I.TWT = (IPtr[0] >> 7) & 0x01; I.TWA = IPtr[0] & 0x7f;
		I.XSEL = (IPtr[1] >> 15) & 0x01; I.YSEL = (IPtr[1] >> 13) & 0x03; I.IRA = (IPtr[1] >> 6) & 0x3f;
		I.IWT = (IPtr[1] >> 5) & 0x01; I.IWA = IPtr[1] & 0x1f;
		I.TABLE = (IPtr[2] >> 15) & 0x01; I.MWT = (IPtr[2] >> 14) & 0x01; I.MRD = (IPtr[2] >> 13) & 0x01;
		I.EWT = (IPtr[2] >> 12) & 0x01; I.EWA = (IPtr[2] >> 8) & 0x0f; I.ADRL = (IPtr[2] >> 7) & 0x01;
		I.FRCL = (IPtr[2] >> 6) & 0x01; I.SHIFT = (IPtr[2] >> 4) & 0x03; I.YRL = (IPtr[2] >> 3) & 0x01;
		I.NEGB = (IPtr[2] >> 2) & 0x01; I.ZERO = (IPtr[2] >> 1) & 0x01; I.BSEL = IPtr[2] & 0x01;
		I.NOFL = (IPtr[3] >> 15) & 0x01; I.COEF = (IPtr[3] >> 9) & 0x3f; I.MASA = (IPtr[3] >> 2) & 0x1f;
		I.ADREB = (IPtr[3] >> 1) & 0x01; I.NXADR = IPtr[3] & 0x01;
		// the same controls again, grouped for the loop (scspdsp.h: Inst::f, Inst::mf)
		I.f = u16((I.ZERO ? F_ZERO : 0) | (I.BSEL ? F_BSEL : 0) | (I.XSEL ? F_XSEL : 0) | (I.TWT ? F_TWT : 0)
		          | (I.IWT ? F_IWT : 0) | (I.EWT ? F_EWT : 0) | (I.MRD ? F_MRD : 0) | (I.MWT ? F_MWT : 0)
		          | (u32(I.SHIFT) << F_SHIFT_SH) | (u32(I.YSEL) << F_YSEL_SH));
		I.mf = u8((I.TABLE ? M_TABLE : 0) | (I.ADREB ? M_ADREB : 0) | (I.NXADR ? M_NXADR : 0) | (I.NOFL ? M_NOFL : 0));
		// only ever added to, never taken away: a step rewritten to stop using one of these keeps
		// the full body until the next Start, which costs speed and nothing else
		if (I.FRCL) uses |= USES_FRCL;
		if (I.ADRL) uses |= USES_ADRL;
		if (I.YRL) uses |= USES_YRL;
		if (I.NEGB) uses |= USES_NEGB;
	}
}
