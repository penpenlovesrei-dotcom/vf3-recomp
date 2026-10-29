# -*- coding: utf-8 -*-
# THE VARIANTS TO BE JUDGED ON THE INSTRUCTION COUNTER.
#
# Each one is a transformation of dsp_maison.cpp that must be EQUIVALENT down to behaviour -- which the ten
# tests verify before it is kept. Each attacks the INSTRUCTION COUNT of a step's unavoidable path, which is
# where the gap sits (+19.6% in the loop, docs/scsp_audit.md §4 nonies).
#
# This file measures nothing: it builds one exe per variant. The judging needs an administrator window, and
# it is mesurer-variantes.cmd that does it, one capture per exe.
#
# TWO OF THE FIVE WERE ADOPTED INTO THE SOURCE on 2026-10-28, so their "before" fragment no longer exists:
# `shift_deux_bits` and `ysel_coef_premier`, hence `tout` and `gagnantes` with them. The builder will
# refuse them by saying the fragment is not there, and that refusal is correct -- they are kept here as the
# record of what was tried and what it was worth. A fragment that stops matching is also what happens when
# the comments around it are reworded: translating this subsystem into English broke the two others until
# their text was brought back into line below, and the builder said so rather than measuring nothing.

# --- the source fragments, as they stand today
ENTREE = """		// --- the input
		s32 entree;
		if (I.ira <= 0x1F)      entree = MEMS[I.ira];
		else if (I.ira <= 0x2F) entree = MIXS[I.ira - 0x20] << 4;    // MIXS is 20 bits
		else if (I.ira <= 0x31) entree = EXTS[I.ira - 0x30] << 8;    // EXTS is 16 of them
		else return;                                                 // beyond that, the step gives up
		entree = sext(entree, 24);
"""

# The four-case switch on YSEL, as it was BEFORE `ysel_coef_premier` was adopted. Kept for the record.
YSEL = """		s32 y = 0;
		switch (I.ysel) {
		case 0: y = m_frc_reg; break;
		case 1: y = COEF[I.coef] >> 3; break;       // COEF fait 16 bits, Y en veut 13
		case 2: y = (m_y_reg >> 11) & 0x1FFF; break;
		case 3: y = (m_y_reg >> 4) & 0x0FFF; break;
		}"""

SORTIES = """		if ((f & I_TWT)) TEMP[(I.twa + dec) & 0x7F] = shifted;"""

VARIANTES = [

 ("temoin", "the code as it stands, so the other five have something to compare against", []),

 # 1. A baseline step does NONE of the four outputs: TWT, MRD, MWT, EWT are all zero. It tests them one by
 #    one all the same, so four tests for nothing. A single grouped test rules them all out.
 ("sortie_groupee",
  "one mask test for the four outputs and the two rare ones, instead of six",
  [(SORTIES,
    """		// ONE TEST FOR THE FOUR OUTPUTS. A step that writes nothing -- and there are many -- used to
		// test them one by one. The grouped mask rules them out at once, and the individual tests stay
		// inside for the steps that do write.
		// I_FRCL AND I_ADRL ARE IN THE MASK, and it took cases 9 to 13 to say so: their writes live
		// BETWEEN TWT's and EWT's, so a mask grouping only the four outputs would have skipped them for
		// a step that sets just one of them. In the lean body both bits are zero: the mask there is
		// therefore exactly the four outputs.
		if ((f & (I_TWT | I_MRD | I_MWT | I_EWT | I_FRCL | I_ADRL))) {
		if ((f & I_TWT)) TEMP[(I.twa + dec) & 0x7F] = shifted;"""),
   ("""		// EWT ADDS, it does not assign: several steps can feed the same effects output.
		if ((f & I_EWT)) EFREG[I.ewa] = s16(EFREG[I.ewa] + s16(shifted >> 8));
	}""",
    """		// EWT ADDS, it does not assign: several steps can feed the same effects output.
		if ((f & I_EWT)) EFREG[I.ewa] = s16(EFREG[I.ewa] + s16(shifted >> 8));
		}
	}""")]),

 # 2. `entree` is computed on every step, and in the baseline it serves NO purpose: XSEL is zero, IWT zero,
 #    and the two rare readers are outside the lean body. Three comparisons, a load and a sign extension
 #    for a discarded value. The give-up, for its part, depends only on the program: Start finds it once.
 ("entree_paresseuse",
  "the input computed only by the steps that read it; the give-up resolved at Start",
  [(ENTREE,
    """		// --- the input, COMPUTED ONLY IF THIS STEP READS IT. In the baseline it serves no purpose:
		// XSEL is zero, IWT zero, and the two rare readers are outside the lean body. The give-up (IRA
		// beyond 0x31) depends only on the program, so Start finds it once: see m_abandon.
		s32 entree = 0;
		if ((f & (I_XSEL | I_IWT)) || (RARES && (f & (I_YRL | I_ADRL)))) {
			if (I.ira <= 0x1F)      entree = MEMS[I.ira];
			else if (I.ira <= 0x2F) entree = MIXS[I.ira - 0x20] << 4;    // MIXS is 20 bits
			else                    entree = EXTS[I.ira - 0x30] << 8;    // EXTS is 16 of them
			entree = sext(entree, 24);
		}
""")]),

 # 3. ADOPTED on 2026-10-28. The four-case `switch` on SHIFT asks two independent questions: double first,
 #    then clamp or sign-extend. Asked as such, the four cases no longer need a switch -- and WITHOUT
 #    growing the decoded step, since the two bits go into the flags word.
 ("shift_deux_bits",
  "the SHIFT switch replaced by two bits of the flags word",
  [("""		s32 shifted = 0;
		switch (I.shift) {
		case 0: shifted = std::clamp<s32>(m_acc, -0x00800000, 0x007FFFFF); break;
		case 1: shifted = std::clamp<s32>(m_acc * 2, -0x00800000, 0x007FFFFF); break;
		case 2: shifted = sext(m_acc * 2, 24); break;
		case 3: shifted = sext(m_acc, 24); break;
		}""",
    """		const s32 av = (f & I_SHPRE) ? (m_acc * 2) : m_acc;
		const s32 shifted = (f & I_SHECR) ? std::clamp<s32>(av, -0x00800000, 0x007FFFFF) : sext(av, 24);"""),
   ("""	if ((m[3] >> 1) & 1)  f |= I_ADREB;
	if (m[3] & 1)         f |= I_NXADR;
	p.f = f;""",
    """	if ((m[3] >> 1) & 1)  f |= I_ADREB;
	if (m[3] & 1)         f |= I_NXADR;
	// The shift's two questions, asked once when the program is written.
	if (p.shift == 1 || p.shift == 2) f |= I_SHPRE;
	if (p.shift <= 1)                 f |= I_SHECR;
	p.f = f;"""),
   ("""		I_FRCL = 1u << 12, I_ADRL = 1u << 13, I_YRL = 1u << 14, I_NEGB = 1u << 15,
		I_RARES = I_FRCL | I_ADRL | I_YRL | I_NEGB,""",
    """		I_FRCL = 1u << 12, I_ADRL = 1u << 13, I_YRL = 1u << 14, I_NEGB = 1u << 15,
		I_RARES = I_FRCL | I_ADRL | I_YRL | I_NEGB,
		I_SHPRE = 1u << 16,          // the shift doubles the accumulator first (SHIFT 1 and 2)
		I_SHECR = 1u << 17,          // it CLAMPS (SHIFT 0 and 1) instead of sign-extending (2 and 3)""",
    'h')]),

 # 4. ADOPTED on 2026-10-28. MAME tests YSEL == 1 FIRST, and its comment says why: 87 of the game's 112
 #    steps take it. The project's switch put case 0 first, which is the rarest in the game. No effect on
 #    the baseline (where YSEL is zero everywhere) -- it is the game's program this would aim at.
 ("ysel_coef_premier",
  "COEF tested first in the choice of Y, as in MAME",
  [(YSEL,
    """		// COEF FIRST, as in MAME and for its reason: 87 of the game's 112 steps take it
		// (docs/scsp_audit.md §5), and it is case 1, hence the second arm of a switch written in order.
		s32 y;
		if (I.ysel == 1)      y = COEF[I.coef] >> 3;       // COEF is 16 bits, Y wants 13
		else if (I.ysel == 0) y = m_frc_reg;
		else if (I.ysel == 2) y = (m_y_reg >> 11) & 0x1FFF;
		else                  y = (m_y_reg >> 4) & 0x0FFF;""")]),

]

# ALL FOUR TOGETHER, and it has to be built AFTER the list and not inside it. They touch disjoint regions
# of a step -- the input, the choice of Y, the shift, the outputs -- so nothing prevents carrying them all.
# Measured on 2026-10-28: -15.00%, -6.02%, -3.03% and -1.55% of instructions removed each, against the
# control.
#
# WRITTEN INSIDE THE LITERAL FIRST, through `VARIANTES[1:]`, it was EMPTY: the name is not yet bound at
# that point, and the scripts that read this file by `exec` pre-define it to []. So the variant passed the
# ten tests and measured exactly like the control, which looked like a result.
VARIANTES.append(("tout", "the four that won ON THE BASELINE, together",
                  [p for _, _, patches in VARIANTES[1:] for p in patches]))

# AND THE ONES THAT WIN WHERE IT COUNTS. Measured on 2026-10-28 on the instruction counter, over BOTH
# programs, and the ranking almost entirely reverses:
#
#                        baseline     the game's shape
#   ysel_coef_premier    -1.55%           -3.06%       <- wins on both sides
#   shift_deux_bits      -3.03%           -1.10%       <- wins on both sides
#   sortie_groupee       -6.02%           +0.91%
#   tout                                  +3.27%
#   entree_paresseuse   -15.00%           +4.75%       <- the baseline's big winner, the game's worst
#
# `entree_paresseuse` adds one test per step to avoid computing an input that, in the real program, 37
# steps out of 112 need through XSEL and 23 through IWT (docs/scsp_audit.md §5). So it pays the test AND
# the computation. In the baseline, XSEL is zero EVERYWHERE: the input is computed there and then thrown
# away 112 times per sample, and that is the only place where its laziness pays.
#
# The baseline locates a cost; it does not say what a change is worth. It took both programs.
NOMS_GAGNANTES = ('ysel_coef_premier', 'shift_deux_bits')
VARIANTES.append(("gagnantes", "the two that win on the game's program",
                  [p for n, _, patches in VARIANTES for p in patches if n in NOMS_GAGNANTES]))
