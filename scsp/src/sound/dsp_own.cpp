#include "dsp_own.h"

#include "sound_prof.h"

#include <algorithm>

namespace {

// Sign extension from a given bit. The DSP works in 24 bits (and Y in 13) inside 32-bit integers.
//
// THE HIGH BITS MUST BE THROWN AWAY BEFORE EXTENDING, and that is the whole point of this form. The first
// version did `(v ^ m) - m`, which is right when the value ALREADY FITS the width -- and wrong otherwise,
// because it lets the bits above through. And the most important case does give it some: `sext(ACC * 2,
// 24)` receives up to 27 bits, ACC carrying 26.
//
// The replay found it 1 064 053 samples in: EFREG[2] was saturating at 0x8000 where it should have been
// zero. A left shift then an arithmetic right shift does both at once.
inline dsp_own::s32 sext(dsp_own::s32 v, unsigned bits)
{
	const unsigned rest = 32 - bits;
	return dsp_own::s32(dsp_own::u32(v) << rest) >> rest;
}

// -----------------------------------------------------------------------------------------------------
// PACK and UNPACK: the delay line's format.
//
// AN EXPONENT FORMAT, and it is a bit layout like a register map -- not an algorithm to invent. A signed
// 24-bit word goes in, sixteen come out: sign at 15, exponent in four bits at 14-11, mantissa in eleven
// bits. The exponent is the number of redundant leading bits, found by looking at where the pattern
// `val ^ (val << 1)` stops being constant -- that is, where the sign stops repeating.
//
// THE WAY BACK IS NOT ITS EXACT INVERSE, and that is deliberate: past exponent 11 the sign bit is copied
// differently. So the round trip loses precision -- 0x0100 comes back as 0x00FF, 0x4000 as 0x4FFF, 0x7FFF
// as 0x67FE -- and those three numbers are recorded in the specification.
// -----------------------------------------------------------------------------------------------------

dsp_own::u16 pack(dsp_own::s32 v)
{
	const int signe = (v >> 23) & 1;
	dsp_own::u32 motif = dsp_own::u32(v ^ (v << 1)) & 0xFFFFFF;
	int exposant = 0;
	for (int k = 0; k < 12; k++) {
		if (motif & 0x800000) break;
		motif <<= 1;
		exposant += 1;
	}
	if (exposant < 12) v = (v << exposant) & 0x3FFFFF;
	else               v <<= 11;
	v >>= 11;
	v &= 0x7FF;
	v |= signe << 15;
	v |= exposant << 11;
	return dsp_own::u16(v);
}

dsp_own::s32 unpack(dsp_own::u16 v)
{
	const int signe = (v >> 15) & 1;
	int exposant = (v >> 11) & 0xF;
	const int mantisse = v & 0x7FF;
	dsp_own::s32 r = mantisse << 11;
	if (exposant > 11) {
		exposant = 11;
		r |= signe << 22;
	} else {
		r |= (signe ^ 1) << 22;
	}
	r |= signe << 23;
	r <<= 8;
	r >>= 8;
	r >>= exposant;
	return r;
}

} // namespace

void dsp_own::Init()
{
	for (auto &v : COEF) v = 0;
	for (auto &v : MADRS) v = 0;
	for (auto &v : MPRO) v = 0;
	for (auto &v : TEMP) v = 0;
	for (auto &v : MEMS) v = 0;
	for (auto &v : MIXS) v = 0;
	for (auto &v : EXTS) v = 0;
	for (auto &v : EFREG) v = 0;
	for (auto &p : m_prog) p = DecodedStep();
	m_last = 0;
	m_acc = m_memval = m_frc_reg = m_y_reg = m_adrs_reg = 0;
	m_dec = 0;
	RBP = 0;
	RBL = 8 * 1024;        // what an RBL of zero means
	m_stopped = true;       // until Start has happened, nothing runs
}

void dsp_own::Decode(int step)
{
	if (step < 0 || step >= 128) return;
	const u16 *m = MPRO + step * 4;
	DecodedStep &p = m_prog[step];
	p.tra   = u8((m[0] >> 8) & 0x7F);
	p.twa   = u8(m[0] & 0x7F);
	p.ysel  = u8((m[1] >> 13) & 3);
	p.ira   = u8((m[1] >> 6) & 0x3F);
	p.iwa   = u8(m[1] & 0x1F);
	p.ewa   = u8((m[2] >> 8) & 0xF);
	p.shift = u8((m[2] >> 4) & 3);
	p.coef  = u8((m[3] >> 9) & 0x3F);
	p.masa  = u8((m[3] >> 2) & 0x1F);

	u32 f = 0;
	if ((m[0] >> 7) & 1)  f |= I_TWT;
	if ((m[1] >> 15) & 1) f |= I_XSEL;
	if ((m[1] >> 5) & 1)  f |= I_IWT;
	if ((m[2] >> 15) & 1) f |= I_TABLE;
	if ((m[2] >> 14) & 1) f |= I_MWT;
	if ((m[2] >> 13) & 1) f |= I_MRD;
	if ((m[2] >> 12) & 1) f |= I_EWT;
	if ((m[2] >> 7) & 1)  f |= I_ADRL;
	if ((m[2] >> 6) & 1)  f |= I_FRCL;
	if ((m[2] >> 3) & 1)  f |= I_YRL;
	if ((m[2] >> 2) & 1)  f |= I_NEGB;
	if ((m[2] >> 1) & 1)  f |= I_ZERO;
	if (m[2] & 1)         f |= I_BSEL;
	if ((m[3] >> 15) & 1) f |= I_NOFL;
	if ((m[3] >> 1) & 1)  f |= I_ADREB;
	if (m[3] & 1)         f |= I_NXADR;
	// The shift's two questions, asked once when the program is written.
	if (p.shift == 1 || p.shift == 2) f |= I_SHPRE;
	if (p.shift <= 1)                 f |= I_SHECR;
	p.f = f;
}

void dsp_own::Start()
{
	m_stopped = false;
	// THE LAST USEFUL STEP: the program stops at the last non-zero step. An entirely zero program
	// therefore does nothing at all, which the specification's first case checks.
	int i = 127;
	for (; i >= 0; --i) {
		const u16 *m = MPRO + i * 4;
		if (m[0] || m[1] || m[2] || m[3]) break;
	}
	m_last = i + 1;
	for (int step = 0; step < 128; step++) Decode(step);
	// Recomputed from scratch over the whole program, not accumulated: this is the only moment at which
	// one can know whether the four rare controls are used anywhere.
	m_rare = false;
	for (int step = 0; step < m_last; step++) if (m_prog[step].f & I_RARES) m_rare = true;
}

void dsp_own::SetSample(s32 sample, s32 sel, s32 mxl)
{
	(void) mxl;   // the level is already applied by the caller, as the chip's mix does
	MIXS[sel & 0xF] += sample;
}

void dsp_own::Step()
{
	if (m_stopped) return;
	// The body without the controls this program does not use (dsp_own.h: m_rares).
	if (m_rare) loop<true>();
	else         loop<false>();
}

template <bool RARES>
void dsp_own::loop()
{

	// EFREG STARTS FROM ZERO ON EVERY SAMPLE. EWT adds, so without this clearing the effects output would
	// accumulate without end.
	for (auto &v : EFREG) v = 0;

	const u32 dec = m_dec, rbl = RBL, rbp = RBP;
	const int dernier = m_last;

	for (int step = 0; step < dernier; ++step) {
		const DecodedStep &I = m_prog[step];
		const u32 f = I.f;

		// --- the input
		s32 input;
		if (I.ira <= 0x1F)      input = MEMS[I.ira];
		else if (I.ira <= 0x2F) input = MIXS[I.ira - 0x20] << 4;    // MIXS is 20 bits
		else if (I.ira <= 0x31) input = EXTS[I.ira - 0x30] << 8;    // EXTS is 16 of them
		else return;                                                 // beyond that, the step gives up
		input = sext(input, 24);

		if ((f & I_IWT)) {
			// MEMVAL comes from an MRD of a PREVIOUS step: the memory read does not arrive in the same step.
			MEMS[I.iwa] = m_memval;
			if (I.ira == I.iwa) input = m_memval;
		}

		// --- the operands. The same TEMP word feeds B and X: read and extended once.
		const s32 tempv = sext(TEMP[(I.tra + dec) & 0x7F], 24);

		s32 b = 0;
		if (!(f & I_ZERO)) {
			b = (f & I_BSEL) ? m_acc : tempv;
			if constexpr (RARES) if ((f & I_NEGB)) b = 0 - b;
		}
		const s32 x = (f & I_XSEL) ? input : tempv;

		// COEF FIRST, as in MAME and for its reason: 87 of the game's 112 steps take it
		// (docs/scsp_audit.md §5), and it is case 1, hence the second arm of a switch written in order.
		s32 y;
		if (I.ysel == 1)      y = COEF[I.coef] >> 3;       // COEF is 16 bits, Y wants 13
		else if (I.ysel == 0) y = m_frc_reg;
		else if (I.ysel == 2) y = (m_y_reg >> 11) & 0x1FFF;
		else                  y = (m_y_reg >> 4) & 0x0FFF;
		if constexpr (RARES) if ((f & I_YRL)) m_y_reg = input;

		// --- THE SHIFTER, AND IT IS ONE STEP BEHIND THE ACCUMULATOR.
		//
		// This is this block's central fact, and the specification brought it out: `shifted` is computed
		// from the PREVIOUS step's ACC, before this one is overwritten further down. So the step that does
		// a product cannot write its result -- the next step is needed for TWT, MWT, EWT, FRCL or ADRL to
		// see it. An implementation that shifted the freshly computed ACC would be wrong everywhere, and
		// no reading of it shouts that: it is the ORDER of the lines that says so.
		//
		// The first two shifts CLAMP to signed 24 bits, the other two SIGN-EXTEND -- hence wrap around.
		// The difference between saturating and snapping round is the whole point of the field.
		const s32 av = (f & I_SHPRE) ? (m_acc * 2) : m_acc;
		const s32 shifted = (f & I_SHECR) ? std::clamp<s32>(av, -0x00800000, 0x007FFFFF) : sext(av, 24);

		// --- the product, with Y in signed 13 bits
		//
		// THE SUM IS DONE IN 64 BITS AND THEN TRUNCATED, not the other way round: truncating the product
		// before adding B is not the same thing as soon as either of them leaves a 32-bit integer's range.
		y = sext(y, 13);
		const s64 product = (s64(x) * s64(y)) >> 12;
		m_acc = s32(product + b);

		if ((f & I_TWT)) TEMP[(I.twa + dec) & 0x7F] = shifted;

		if constexpr (RARES) if ((f & I_FRCL)) m_frc_reg = (I.shift == 3) ? (shifted & 0x0FFF) : ((shifted >> 11) & 0x1FFF);

		// --- the delay line
		if ((f & I_MRD) || (f & I_MWT)) {
			u32 address = MADRS[I.masa];
			if (!(f & I_TABLE)) address += dec;
			if ((f & I_ADREB)) address += u32(m_adrs_reg) & 0x0FFF;
			if ((f & I_NXADR)) address++;
			// Without TABLE, the address wraps inside the delay line; with it, it sweeps 64 kwords.
			if (!(f & I_TABLE)) address &= rbl - 1;
			else          address &= 0xFFFF;
			address += rbp << 12;
			address <<= 1;
			// AND THE ACCESSES ONLY HAPPEN ON AN ODD STEP. An MRD at step 0 reads nothing: that is why the
			// programs interleave empty steps.
			if (step & 1) {
				if ((f & I_MRD)) m_memval = (f & I_NOFL) ? (s32(read_word(address)) << 8) : unpack(read_word(address));
				if ((f & I_MWT)) write_word(address, (f & I_NOFL) ? u16(shifted >> 8) : pack(shifted));
			}
		}

		if constexpr (RARES) if ((f & I_ADRL)) m_adrs_reg = (I.shift == 3) ? ((shifted >> 12) & 0x0FFF) : (input >> 16);

		// EWT ADDS, it does not assign: several steps can feed the same effects output.
		if ((f & I_EWT)) EFREG[I.ewa] = s16(EFREG[I.ewa] + s16(shifted >> 8));
	}

	m_dec = dec - 1;
	if (sndprof::on) sndprof::dsp_steps += unsigned(dernier);
	// MIXS is cleared at the END: the slots fill it for the next sample.
	for (auto &v : MIXS) v = 0;
}
