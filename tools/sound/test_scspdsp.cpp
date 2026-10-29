// A differential test for the SCSP's effects DSP, written for the hole that specialising its body
// left open (scsp/docs/scsp_validation.md): SCSPDSP::Step now compiles two bodies, Run<0> for a program
// that sets none of FRCL, ADRL, YRL or NEGB -- which is the program BOTH games of this port use --
// and Run<USES_ALL> for any other. The games' sound is checked to the byte, so Run<0> is covered;
// the generic body is the path neither game takes, and until this test nothing exercised it.
//
// WHAT IT COMPARES, AND WHAT THAT IS WORTH. The reference below decodes MPRO's bit fields inside
// the step loop, the way MAME's master and Supermodel's still do, with no decoded table, no packed
// controls, no templates and no local copies of DEC / RBL / RBP. So it tests exactly the machinery
// this port added -- Decode, Inst::f, Inst::mf, uses, the two bodies, the loop copies and the
// restrict pointer -- against the plain form the semantics were taken from. It does NOT re-derive
// the chip's behaviour from a hardware document, and PACK/UNPACK are transcribed rather than
// re-reasoned (nothing in this port touches them). A test that says IDENTICAL here means "the port
// still computes what the plain loop computes", which is the question at hand, and not "the chip
// does this".
//
//   make build/bin/test_scspdsp && build/bin/test_scspdsp
//
// It needs no ROM, no sound RAM from a game and no window: the programs are generated from fixed
// seeds, so two runs do the same thing.
#include "../../src/sound/scspdsp.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {

constexpr u32 RAM_WORDS = 0x80000;   // 1 MB, as an SCSP has
constexpr int SAMPLES = 64;          // steps of the whole program, per program tested

// ---- PACK and UNPACK, transcribed from src/sound/scspdsp.cpp (this port changes neither) ----

u16 ref_pack(s32 val)
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

s32 ref_unpack(u16 val)
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

// ---- the reference: one step at a time, MPRO read and masked in the loop ----

struct RefDSP
{
	u16 *ram = nullptr;
	u32 ram_bytes = 0;
	u32 RBP = 0, RBL = 8 * 1024;
	s16 COEF[64] = {};
	u16 MADRS[32] = {};
	u16 MPRO[128 * 4] = {};
	s32 TEMP[128] = {}, MEMS[32] = {}, MIXS[16] = {};
	s16 EXTS[2] = {}, EFREG[16] = {};
	u32 DEC = 0;
	int LastStep = 0;

	u16 rd(u32 a) const { return a < ram_bytes ? ram[a >> 1] : 0; }
	void wr(u32 a, u16 v) { if (a < ram_bytes) ram[a >> 1] = v; }

	void start()
	{
		int i;
		for (i = 127; i >= 0; --i)
			if (MPRO[i * 4] || MPRO[i * 4 + 1] || MPRO[i * 4 + 2] || MPRO[i * 4 + 3])
				break;
		LastStep = i + 1;
	}

	void step()
	{
		std::fill(std::begin(EFREG), std::end(EFREG), 0);

		s32 ACC = 0, MEMVAL = 0, FRC_REG = 0, Y_REG = 0;
		u32 ADRS_REG = 0;

		for (int st = 0; st < LastStep; ++st)
		{
			u16 const *const I = MPRO + st * 4;
			u32 const TRA = (I[0] >> 8) & 0x7f, TWT = (I[0] >> 7) & 0x01, TWA = I[0] & 0x7f;
			u32 const XSEL = (I[1] >> 15) & 0x01, YSEL = (I[1] >> 13) & 0x03, IRA = (I[1] >> 6) & 0x3f,
			          IWT = (I[1] >> 5) & 0x01, IWA = I[1] & 0x1f;
			u32 const TABLE = (I[2] >> 15) & 0x01, MWT = (I[2] >> 14) & 0x01, MRD = (I[2] >> 13) & 0x01,
			          EWT = (I[2] >> 12) & 0x01, EWA = (I[2] >> 8) & 0x0f, ADRL = (I[2] >> 7) & 0x01,
			          FRCL = (I[2] >> 6) & 0x01, SHIFT = (I[2] >> 4) & 0x03, YRL = (I[2] >> 3) & 0x01,
			          NEGB = (I[2] >> 2) & 0x01, ZERO = (I[2] >> 1) & 0x01, BSEL = I[2] & 0x01;
			u32 const NOFL = (I[3] >> 15) & 0x01, COEFI = (I[3] >> 9) & 0x3f, MASA = (I[3] >> 2) & 0x1f,
			          ADREB = (I[3] >> 1) & 0x01, NXADR = I[3] & 0x01;

			s32 INPUTS;
			if (IRA <= 0x1f)
				INPUTS = MEMS[IRA];
			else if (IRA <= 0x2F)
				INPUTS = MIXS[IRA - 0x20] << 4;
			else if (IRA <= 0x31)
				INPUTS = EXTS[IRA - 0x30] << 8;
			else
				return;   // aborts the whole step: DEC and MIXS are left alone

			INPUTS = util::sext(INPUTS, 24);

			if (IWT)
			{
				MEMS[IWA] = MEMVAL;
				if (IRA == IWA)
					INPUTS = MEMVAL;
			}

			s32 B;
			if (!ZERO)
			{
				if (BSEL)
					B = ACC;
				else
					B = util::sext(TEMP[(TRA + DEC) & 0x7f], 24);
				if (NEGB)
					B = 0 - B;
			}
			else
				B = 0;

			s32 X;
			if (XSEL)
				X = INPUTS;
			else
				X = util::sext(TEMP[(TRA + DEC) & 0x7f], 24);

			s32 Y = 0;
			if (YSEL == 0)
				Y = FRC_REG;
			else if (YSEL == 1)
				Y = COEF[COEFI] >> 3;
			else if (YSEL == 2)
				Y = (Y_REG >> 11) & 0x1fff;
			else if (YSEL == 3)
				Y = (Y_REG >> 4) & 0x0fff;

			if (YRL)
				Y_REG = INPUTS;

			s32 SHIFTED = 0;
			if (SHIFT == 0)
				SHIFTED = std::clamp<s32>(ACC, -0x00800000, 0x007fffff);
			else if (SHIFT == 1)
				SHIFTED = std::clamp<s32>(ACC * 2, -0x00800000, 0x007fffff);
			else if (SHIFT == 2)
				SHIFTED = util::sext(ACC * 2, 24);
			else if (SHIFT == 3)
				SHIFTED = util::sext(ACC, 24);

			Y = util::sext(Y, 13);
			s64 const v = (s64(X) * s64(Y)) >> 12;
			ACC = int(v + B);

			if (TWT)
				TEMP[(TWA + DEC) & 0x7f] = SHIFTED;

			if (FRCL)
			{
				if (SHIFT == 3)
					FRC_REG = SHIFTED & 0x0fff;
				else
					FRC_REG = (SHIFTED >> 11) & 0x1fff;
			}

			if (MRD || MWT)
			{
				u32 ADDR = MADRS[MASA];
				if (!TABLE)
					ADDR += DEC;
				if (ADREB)
					ADDR += ADRS_REG & 0x0FFF;
				if (NXADR)
					ADDR++;
				if (!TABLE)
					ADDR &= RBL - 1;
				else
					ADDR &= 0xffff;
				ADDR += RBP << 12;
				ADDR <<= 1;
				if (MRD && (st & 1))
				{
					if (NOFL)
						MEMVAL = rd(ADDR) << 8;
					else
						MEMVAL = ref_unpack(rd(ADDR));
				}
				if (MWT && (st & 1))
				{
					if (NOFL)
						wr(ADDR, SHIFTED >> 8);
					else
						wr(ADDR, ref_pack(SHIFTED));
				}
			}

			if (ADRL)
			{
				if (SHIFT == 3)
					ADRS_REG = (SHIFTED >> 12) & 0xfff;
				else
					ADRS_REG = INPUTS >> 16;
			}

			if (EWT)
				EFREG[EWA] += SHIFTED >> 8;
		}
		--DEC;
		std::fill(std::begin(MIXS), std::end(MIXS), 0);
	}
};

// ---- programs, from a fixed seed ----

struct Rng
{
	u32 s;
	explicit Rng(u32 seed) : s(seed ? seed : 1) {}
	u32 next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
	u32 below(u32 n) { return next() % n; }
};

// `force` are bits of SCSPDSP's USES_* that every few steps must set; `forbid` are bits no step may
// set. The rest of each instruction is random, with IRA kept inside its three ranges except where
// `abort_step` says to put a step that aborts.
struct ProgSpec
{
	u32 seed;
	int steps = 48;
	unsigned force = 0, forbid = 0;
	int abort_step = -1;
};

void make_program(const ProgSpec &sp, u16 mpro[128 * 4])
{
	Rng r(sp.seed);
	std::memset(mpro, 0, sizeof(u16) * 128 * 4);
	for (int st = 0; st < sp.steps; st++)
	{
		u16 w[4] = { u16(r.next()), u16(r.next()), u16(r.next()), u16(r.next()) };

		// IRA (w[1] bits 6-11, 6 bits): inside MEMS / MIXS / EXTS, so a step does not abort,
		// unless this is the step meant to abort.
		u32 ira = st == sp.abort_step ? 0x32 + r.below(0x0d) : r.below(0x32);
		w[1] = u16((w[1] & ~0x0FC0) | ((ira & 0x3f) << 6));

		// the four controls of Inst::f's slow path live in w[2]: ADRL bit 7, FRCL bit 6, YRL bit 3,
		// NEGB bit 2. Forced ones land on one step in three; forbidden ones are cleared outright.
		auto set_bit = [&](unsigned flag, int bit) {
			// the draw happens either way, so that forcing and forbidding give programs that differ
			// ONLY in these four bits -- check_controls_bite leans on exactly that
			const bool pick = r.below(3) == 0;
			if (sp.forbid & flag) { w[2] = u16(w[2] & ~(1u << bit)); return; }
			if ((sp.force & flag) && pick) w[2] = u16(w[2] | (1u << bit));
		};
		set_bit(SCSPDSP::USES_ADRL, 7);
		set_bit(SCSPDSP::USES_FRCL, 6);
		set_bit(SCSPDSP::USES_YRL, 3);
		set_bit(SCSPDSP::USES_NEGB, 2);

		// a step of all zeros would shorten the program (LastStep): keep one bit that changes nothing
		// on its own, so the length is the one asked for
		if (!w[0] && !w[1] && !w[2] && !w[3]) w[1] = 0x0040;

		for (int k = 0; k < 4; k++) mpro[st * 4 + k] = w[k];
	}
}

// ---- the two sides, set up identically and stepped together ----

struct Pair
{
	std::vector<u16> ram_a = std::vector<u16>(RAM_WORDS, 0), ram_b = std::vector<u16>(RAM_WORDS, 0);
	SCSPDSP dsp;
	RefDSP ref;

	// Fills both with the same program, the same tables and the same RAM, from the seed.
	void setup(const ProgSpec &sp, u32 rbp, u32 rbl)
	{
		Rng r(sp.seed ^ 0x5bd1e995);
		for (u32 i = 0; i < RAM_WORDS; i++) ram_a[i] = ram_b[i] = u16(r.next());

		u16 mpro[128 * 4];
		make_program(sp, mpro);

		dsp.ram = ram_a.data();
		dsp.ram_bytes = RAM_WORDS * 2;
		dsp.Init();                       // memsets the object, keeps ram/ram_bytes
		dsp.RBP = ref.RBP = rbp;
		dsp.RBL = ref.RBL = rbl;
		ref.ram = ram_b.data();
		ref.ram_bytes = RAM_WORDS * 2;
		std::memset(ref.TEMP, 0, sizeof ref.TEMP);
		std::memset(ref.MEMS, 0, sizeof ref.MEMS);
		std::memset(ref.MIXS, 0, sizeof ref.MIXS);
		std::memset(ref.EFREG, 0, sizeof ref.EFREG);
		ref.DEC = 0;

		for (int i = 0; i < 64; i++) dsp.COEF[i] = ref.COEF[i] = s16(r.next());
		for (int i = 0; i < 32; i++) dsp.MADRS[i] = ref.MADRS[i] = u16(r.next());
		for (int i = 0; i < 2; i++) dsp.EXTS[i] = ref.EXTS[i] = s16(r.next());
		std::memcpy(dsp.MPRO, mpro, sizeof mpro);
		std::memcpy(ref.MPRO, mpro, sizeof mpro);

		dsp.Start();
		ref.start();
	}

	// One sample in each, with the same MIXS fed in, then everything observable compared.
	// Returns an empty string when they agree.
	std::string step_and_compare(Rng &r, int sample)
	{
		s32 mixs[16];
		for (int i = 0; i < 16; i++) mixs[i] = s32(r.next()) >> 12;   // 20-bit-ish, as the voices send
		for (int i = 0; i < 16; i++) dsp.MIXS[i] = ref.MIXS[i] = mixs[i];

		dsp.Step();
		ref.step();

		char buf[256];
		auto fail = [&](const char *what, long long a, long long b, int i) {
			snprintf(buf, sizeof buf, "sample %d: %s[%d] gives %lld, the reference %lld", sample, what, i, a, b);
			return std::string(buf);
		};
		for (int i = 0; i < 16; i++)
			if (dsp.EFREG[i] != ref.EFREG[i]) return fail("EFREG", dsp.EFREG[i], ref.EFREG[i], i);
		for (int i = 0; i < 128; i++)
			if (dsp.TEMP[i] != ref.TEMP[i]) return fail("TEMP", dsp.TEMP[i], ref.TEMP[i], i);
		for (int i = 0; i < 32; i++)
			if (dsp.MEMS[i] != ref.MEMS[i]) return fail("MEMS", dsp.MEMS[i], ref.MEMS[i], i);
		if (dsp.DEC != ref.DEC) return fail("DEC", dsp.DEC, ref.DEC, 0);
		for (u32 i = 0; i < RAM_WORDS; i++)
			if (ram_a[i] != ram_b[i]) {
				snprintf(buf, sizeof buf, "sample %d: the delay RAM differs at word %06x: %04x against %04x",
				         sample, i, ram_a[i], ram_b[i]);
				return std::string(buf);
			}
		return {};
	}
};

int failures = 0;

// Runs a program in both, SAMPLES times. `expect_uses` is the set SCSPDSP must have worked out;
// `force_uses` (when not -1) is written over it afterwards, to make Step take the other body on a
// program where both must agree.
void run_case(const char *name, const ProgSpec &sp, unsigned expect_uses, u32 rbp = 0, u32 rbl = 8 * 1024,
              int force_uses = -1)
{
	Pair p;
	p.setup(sp, rbp, rbl);

	const unsigned got = p.dsp.uses;
	const char *body = (force_uses < 0 ? got : unsigned(force_uses)) == 0 ? "Run<0>" : "Run<USES_ALL>";
	if (got != expect_uses) {
		printf("  FAIL  %-34s uses is %u, expected %u\n", name, got, expect_uses);
		failures++;
		return;
	}
	if (force_uses >= 0) p.dsp.uses = unsigned(force_uses);

	Rng r(sp.seed ^ 0x9e3779b9);
	for (int s = 0; s < SAMPLES; s++) {
		const std::string bad = p.step_and_compare(r, s);
		if (!bad.empty()) {
			printf("  FAIL  %-34s %s: %s\n", name, body, bad.c_str());
			failures++;
			return;
		}
	}
	printf("  ok    %-34s %-14s %d steps, %d samples, uses %u\n", name, body, p.dsp.LastStep, SAMPLES, got);
}

// The four controls must still DO something in the generic body: the same program with them and
// without must not give the same delay RAM. Without this, a body that silently dropped them all
// would pass every comparison above by agreeing with a reference that... no: the reference keeps
// them. This guards the weaker case -- a generator that never managed to set them, so that the
// "with" program is secretly the "without" one and the test proves nothing.
void check_controls_bite(const char *name, const ProgSpec &with)
{
	ProgSpec without = with;
	without.force = 0;
	without.forbid = SCSPDSP::USES_ALL;

	Pair a, b;
	a.setup(with, 0, 8 * 1024);
	b.setup(without, 0, 8 * 1024);
	if (a.dsp.uses != SCSPDSP::USES_ALL || b.dsp.uses != 0) {
		printf("  FAIL  %-34s the two programs do not differ by uses (%u / %u)\n",
		       name, a.dsp.uses, b.dsp.uses);
		failures++;
		return;
	}
	Rng ra(with.seed ^ 0x9e3779b9), rb(with.seed ^ 0x9e3779b9);
	for (int s = 0; s < SAMPLES; s++) { a.step_and_compare(ra, s); b.step_and_compare(rb, s); }
	if (a.ram_a == b.ram_a) {
		printf("  FAIL  %-34s the four controls change nothing in the delay RAM\n", name);
		failures++;
		return;
	}
	int n = 0;
	for (u32 i = 0; i < RAM_WORDS; i++) n += a.ram_a[i] != b.ram_a[i];
	printf("  ok    %-34s %-14s %d delay RAM words change with the four\n", name, "effect", n);
}

// A step rewritten after Start, through Decode alone: uses must GAIN the bit (scspdsp.h says it is
// only ever added to), so the next Step takes the generic body and still matches the reference.
void check_decode_gains_a_bit(const char *name)
{
	ProgSpec sp{ 0xC0FFEE, 48, 0, SCSPDSP::USES_ALL, -1 };
	Pair p;
	p.setup(sp, 0, 8 * 1024);
	if (p.dsp.uses != 0) {
		printf("  FAIL  %-34s uses is %u before the rewrite, expected 0\n", name, p.dsp.uses);
		failures++;
		return;
	}
	// step 4's FRCL (w[2] bit 6), written as the 68000 writes MPRO: the word, then Decode on the step
	const int st = 4;
	const u16 w2 = u16(p.dsp.MPRO[st * 4 + 2] | (1u << 6));
	p.dsp.MPRO[st * 4 + 2] = p.ref.MPRO[st * 4 + 2] = w2;
	p.dsp.Decode(st);
	if (!(p.dsp.uses & SCSPDSP::USES_FRCL)) {
		printf("  FAIL  %-34s Decode did not add USES_FRCL (uses %u)\n", name, p.dsp.uses);
		failures++;
		return;
	}
	Rng r(sp.seed ^ 0x9e3779b9);
	for (int s = 0; s < SAMPLES; s++) {
		const std::string bad = p.step_and_compare(r, s);
		if (!bad.empty()) {
			printf("  FAIL  %-34s %s\n", name, bad.c_str());
			failures++;
			return;
		}
	}
	printf("  ok    %-34s %-14s uses goes from 0 to %u with no Start\n", name, "Run<USES_ALL>", p.dsp.uses);
}

// Start works `uses` out afresh over the whole program; Decode only ever adds (scspdsp.h). So an
// object that has run a program needing all four must come back to 0 when given a program needing
// none -- and nothing else holds that claim, because a fresh object is zeroed by Init and the
// difference only shows on a reused one. A stale bit would be SAFE (it picks the slow body, never
// the wrong one), which is exactly why the claim needs a test rather than a crash to reveal it.
void check_start_recomputes_uses(const char *name)
{
	Pair p;
	p.setup({ 0xBEEF, 48, SCSPDSP::USES_ALL, 0 }, 0, 8 * 1024);
	if (p.dsp.uses != SCSPDSP::USES_ALL) {
		printf("  FAIL  %-34s uses is %u on the first program, expected %u\n",
		       name, p.dsp.uses, SCSPDSP::USES_ALL);
		failures++;
		return;
	}
	Rng r1(0xBEEF ^ 0x9e3779b9);
	for (int s = 0; s < 8; s++) {
		const std::string bad = p.step_and_compare(r1, s);
		if (!bad.empty()) { printf("  FAIL  %-34s %s\n", name, bad.c_str()); failures++; return; }
	}

	// the same object, handed a program that needs none of the four
	u16 mpro[128 * 4];
	make_program({ 0x1234, 48, 0, SCSPDSP::USES_ALL }, mpro);
	std::memcpy(p.dsp.MPRO, mpro, sizeof mpro);
	std::memcpy(p.ref.MPRO, mpro, sizeof mpro);
	p.dsp.Start();
	p.ref.start();
	if (p.dsp.uses != 0) {
		printf("  FAIL  %-34s uses stays at %u after a Start on a program that sets none\n",
		       name, p.dsp.uses);
		failures++;
		return;
	}
	Rng r2(0x1234 ^ 0x9e3779b9);
	for (int s = 0; s < SAMPLES; s++) {
		const std::string bad = p.step_and_compare(r2, s);
		if (!bad.empty()) { printf("  FAIL  %-34s %s\n", name, bad.c_str()); failures++; return; }
	}
	printf("  ok    %-34s %-14s uses comes back from %u to 0 on the next Start\n", name, "Run<0>", SCSPDSP::USES_ALL);
}

} // namespace

int main()
{
	printf("test_scspdsp: SCSPDSP::Step's two bodies against a reference that decodes inside the loop\n");
	printf("%d samples per program, %d words of delay RAM compared after each one\n\n", SAMPLES, RAM_WORDS);

	printf("-- the lean body, the one both games take --\n");
	run_case("none of the four", { 0x1234, 48, 0, SCSPDSP::USES_ALL }, 0);
	run_case("none of the four, another seed", { 0x7A5E, 112, 0, SCSPDSP::USES_ALL }, 0);

	printf("\n-- THE GENERIC BODY, which nothing exercised --\n");
	// the same program, but Step forced onto the other body: the two must give the same thing
	run_case("none of the four, body forced", { 0x1234, 48, 0, SCSPDSP::USES_ALL }, 0, 0, 8 * 1024,
	         SCSPDSP::USES_ALL);
	run_case("the four together", { 0xBEEF, 48, SCSPDSP::USES_ALL, 0 }, SCSPDSP::USES_ALL);
	run_case("the four, long program", { 0x51D5, 112, SCSPDSP::USES_ALL, 0 }, SCSPDSP::USES_ALL);
	run_case("FRCL alone", { 0xF001, 48, SCSPDSP::USES_FRCL, SCSPDSP::USES_ALL ^ SCSPDSP::USES_FRCL },
	         SCSPDSP::USES_FRCL);
	run_case("ADRL alone", { 0xA002, 48, SCSPDSP::USES_ADRL, SCSPDSP::USES_ALL ^ SCSPDSP::USES_ADRL },
	         SCSPDSP::USES_ADRL);
	run_case("YRL alone", { 0x9003, 48, SCSPDSP::USES_YRL, SCSPDSP::USES_ALL ^ SCSPDSP::USES_YRL },
	         SCSPDSP::USES_YRL);
	run_case("NEGB alone", { 0x1804, 48, SCSPDSP::USES_NEGB, SCSPDSP::USES_ALL ^ SCSPDSP::USES_NEGB },
	         SCSPDSP::USES_NEGB);

	printf("\n-- the delay line's addressing, and the step that gives up --\n");
	run_case("RBL 64k, RBP 3, the four", { 0xD00D, 64, SCSPDSP::USES_ALL, 0 }, SCSPDSP::USES_ALL,
	         3, 64 * 1024);
	run_case("RBL 16k, RBP 1, the four", { 0x16B1, 64, SCSPDSP::USES_ALL, 0 }, SCSPDSP::USES_ALL,
	         1, 16 * 1024);
	// IRA > 0x31 gives up the whole step: neither DEC nor MIXS moves, and the reference does the same
	run_case("IRA out of range at step 20", { 0xAB07, 48, SCSPDSP::USES_ALL, 0, 20 }, SCSPDSP::USES_ALL);
	run_case("IRA out of range at step 0", { 0xAB08, 48, SCSPDSP::USES_ALL, 0, 0 }, SCSPDSP::USES_ALL);

	printf("\n-- what the four controls change, and uses gained with no Start --\n");
	check_controls_bite("the four bite", { 0xBEEF, 48, SCSPDSP::USES_ALL, 0 });
	check_decode_gains_a_bit("Decode adds a bit");
	check_start_recomputes_uses("Start recomputes uses");

	printf("\n%s\n", failures ? "*** SOME CASES FAILED ***" : "all cases pass");
	return failures ? 1 : 0;
}
