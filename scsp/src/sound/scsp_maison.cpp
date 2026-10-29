#include "scsp_maison.h"

#include "scsp_contract.h"
#include "scsp_select.h"
#include "sound_prof.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {

// -----------------------------------------------------------------------------------------------------
// READING A SLOT FIELD, and this is where correctness and speed almost ruled each other out.
//
// The first version called scsp_contract::champ_fente(nom) INSIDE THE LOOP: a lookup by string comparison
// over thirty-four entries, for each of the thirty-eight fields read, for each of the thirty-two slots,
// forty-four thousand times a second. It was correct -- the contract stayed the single source of the
// positions -- and the replay confirmed it byte for byte. But the measurement gave the price: 47.80 s
// against 21.05 s for MAME's core on the same trace, that is 2.27 times slower, when the CPU gain is the
// specification's highest priority.
//
// The form below keeps the single source WITHOUT paying for it: the positions are read out of the
// contract's table ONCE, on first use, and stored in integers. A wrong field in the contract therefore
// still breaks the tests, and the hot loop now does no more than a shift and a mask.
// -----------------------------------------------------------------------------------------------------
struct Position {
	unsigned mot = 0, decalage = 0, masque = 0;
	bool bit_en_place = false;
};

// A position, resolved AT COMPILE TIME from the contract's table. `champ_fente` being constexpr, a literal
// name here yields a shift and a mask known to the compiler: the hot loop now does no more than a load, a
// shift and a mask, and the `bit_en_place` test folds away.
constexpr Position position(const char *nom)
{
	Position p;
	const scsp_contract::Champ *c = scsp_contract::champ_fente(nom);
	if (!c) return p;
	p.mot = c->mot;
	p.decalage = c->decalage;
	p.masque = c->masque;
	p.bit_en_place = c->decalage == 0 && (c->masque & (c->masque + 1)) != 0;
	return p;
}

constexpr Position C_ALFOS = position("ALFOS");
constexpr Position C_ALFOWS = position("ALFOWS");
constexpr Position C_AR = position("AR");
constexpr Position C_D1R = position("D1R");
constexpr Position C_D2R = position("D2R");
constexpr Position C_DIPAN = position("DIPAN");
constexpr Position C_DISDL = position("DISDL");
constexpr Position C_DL = position("DL");
constexpr Position C_EFPAN = position("EFPAN");
constexpr Position C_EFSDL = position("EFSDL");
constexpr Position C_EGHOLD = position("EGHOLD");
constexpr Position C_FNS = position("FNS");
constexpr Position C_IMXL = position("IMXL");
constexpr Position C_ISEL = position("ISEL");
constexpr Position C_KRS = position("KRS");
constexpr Position C_KYONB = position("KYONB");
constexpr Position C_LEA = position("LEA");
constexpr Position C_LFOF = position("LFOF");
constexpr Position C_LPCTL = position("LPCTL");
constexpr Position C_LPSLNK = position("LPSLNK");
constexpr Position C_LSA = position("LSA");
constexpr Position C_MDL = position("MDL");
constexpr Position C_MDXSL = position("MDXSL");
constexpr Position C_MDYSL = position("MDYSL");
constexpr Position C_OCT = position("OCT");
constexpr Position C_PCM8B = position("PCM8B");
constexpr Position C_PLFOS = position("PLFOS");
constexpr Position C_PLFOWS = position("PLFOWS");
constexpr Position C_RR = position("RR");
constexpr Position C_SA_HI = position("SA_HI");
constexpr Position C_SA_LO = position("SA_LO");
constexpr Position C_SBCTL = position("SBCTL");
constexpr Position C_SDIR = position("SDIR");
constexpr Position C_SSCTL = position("SSCTL");
constexpr Position C_STWINH = position("STWINH");
constexpr Position C_TL = position("TL");

inline unsigned champ(const scsp::u16 mot[16], const Position &p)
{
	return p.bit_en_place ? unsigned((mot[p.mot] & p.masque) != 0)
	                      : unsigned((mot[p.mot] >> p.decalage) & p.masque);
}

} // namespace


// -----------------------------------------------------------------------------------------------------
// BLOCK 1 -- registers and access semantics.
//
// What the block has to return is recorded in tools/son/test_scsp_regs.cpp: for every word from 0x000 to
// 0x42F, what a write RAISES and what STAYS raised. The whole map reads back as it was written except for
// five words, and those five are the block:
//
//     0x404  the low byte is the incoming MIDI queue: useful mask 0xFF00
//     0x408  MSLC/CA/SGC/EG, recomputed on every read: useful mask 0x0000
//     0x420  SCIPD: the write is an OR of bit 5 alone, which STICKS
//     0x42C  MCIPD: assignment, but the read copies the internal state back -- bit 5 returns
//     0x42E  MCIRE: receives the exception meant for MCIPD, so its bit 5 never goes out
// -----------------------------------------------------------------------------------------------------

scsp_maison::scsp_maison(u32 horloge, u16 *ram, u32 ram_octets, scsp::sink &puits)
	: m_horloge(horloge), m_ram(ram), m_ram_octets(ram_octets), m_puits(puits)
{
	construire_facteurs();
	construire_enveloppe();
	construire_lfo();
	reset();
}

void scsp_maison::reset()
{
	for (auto &f : m_fente) for (auto &w : f) w = 0;
	for (auto &w : m_commun) w = 0;
	for (auto &w : m_anneau) w = 0;
	for (auto &w : m_madrs_haut) w = 0;
	// COEF, MADRS and MPRO are in the DSP, which Init() clears further down.
	m_mcieb = m_mcipd = 0;
	m_midi_e = m_midi_l = 0;
	m_niv_tima = m_niv_timbc = m_niv_midi = m_niv_cpu = m_niv_dma = 0;
	m_mslc = 0;
	m_ptr_anneau = 0;
	for (auto &c : m_compte) c = 0;
	for (auto &p : m_predivise) p = 1;
	m_graine = 0x9d14abd7;
	m_dsp.ram = m_ram;
	m_dsp.ram_bytes = m_ram_octets;
	m_dsp.Init();
	for (auto &v : m_vive) v = Vive();
	m_now = 0;
}

// -----------------------------------------------------------------------------------------------------
// THE ACCESSES
// -----------------------------------------------------------------------------------------------------

scsp_maison::u16 scsp_maison::etat_fente_surveillee() const
{
	// THE MONITORED SLOT'S LIVE STATE, recomputed on every read: the four high bits of the read address,
	// the envelope's state, and the volume inverted.
	//
	// THIS WAS A STUB, AND THE REPLAYER FOUND IT. While only block 1 existed, this function returned a
	// constant -- an idle slot -- and that was defensible: block 1's test only checks the MASK here, that
	// no written bit reads back, which is true whatever the value. But once blocks 2 and 3 were written
	// there was a real state to return, and the stub became wrong without a single unit test moving. It
	// was the replay of a real scene that said so: "read 5292 of word 204: 0040 against 007f". The driver
	// reads this register to find a free slot, so a stub here changes voice allocation -- and the sound,
	// two seconds later, in a way that no longer resembles its cause.
	const Vive &v = m_vive[m_mslc & (FENTES - 1)];
	const u32 ca = (v.cur >> (PAS + 12)) & 0xF;
	const u32 sgc = u32(v.etat) & 3;
	const u32 eg = u32(0x1F - (v.volume >> (EG_PAS + 5))) & 0x1F;
	return u16((ca << 7) | (sgc << 5) | eg);
}

scsp_maison::u16 scsp_maison::lire_octet_adresse(u32 octet)
{
	octet &= 0xFFFF;

	if (octet < 0x400)
		return m_fente[octet / 0x20][(octet & 0x1F) / 2];

	if (octet < 0x430) {
		const u32 bas = octet & 0x3F;
		switch (bas) {
		case 0x04: case 0x05: {
			// The low byte IS the incoming MIDI queue, and the read POPS it -- the read is what drops
			// the interrupt when the queue empties (scsp_contract.h §12 point 9). An implementation
			// returning the stored word would leave the interrupt raised for ever.
			u16 v = u16(m_commun[0x04 / 2] & 0xFF00);
			v = u16(v | m_midi[m_midi_l]);
			if (m_midi_l != m_midi_e) m_midi_l = (m_midi_l + 1) & (MIDI_PROFONDEUR - 1);
			if (m_midi_l == m_midi_e) {              // the queue is empty: the interrupt drops
				m_puits.irq(m_niv_midi, false);
				m_commun[0x20 / 2] = u16(m_commun[0x20 / 2] & ~0x8);
			}
			m_commun[0x04 / 2] = v;
			return v;
		}
		case 0x08: case 0x09:
			m_commun[0x08 / 2] = etat_fente_surveillee();
			return m_commun[0x08 / 2];
		case 0x2A: case 0x2B:
			m_commun[0x2A / 2] = m_mcieb;
			return m_mcieb;
		case 0x2C: case 0x2D:
			m_commun[0x2C / 2] = m_mcipd;
			return m_mcipd;
		default:
			return m_commun[bas / 2];
		}
	}

	if (octet < 0x600) return 0;                     // 0x430-0x5FF: nothing answers there
	if (octet < 0x700) return u16(m_anneau[(octet - 0x600) / 2]);
	if (octet < 0x780) return u16(m_dsp.COEF[(octet - 0x700) / 2]);
	if (octet < 0x7C0) return m_dsp.MADRS[(octet - 0x780) / 2];
	if (octet < 0x800) {
		const u32 i = (octet - 0x7C0) / 2;
		return m_miroir ? m_dsp.MADRS[i] : m_madrs_haut[i];
	}
	if (octet < 0xC00) return m_dsp.MPRO[(octet - 0x800) / 2];

	// THE DSP'S STATE READS BACK, and that is how it can be specified. TEMP, MEMS, MIXS and EFREG are all
	// exposed in the register window -- read-only, a write beyond 0xC00 going nowhere. The first three
	// carry more than sixteen bits, so each takes TWO words: bit 1 of the address picks the half, the high
	// one first.
	//
	// This piece of the map was missing from block 1, and its test could not say so: its sweep stops at
	// 0x42F. It was while looking for somewhere to observe the DSP that it had to be filled in.
	if (octet < 0xE00) {   // TEMP, 128 words of 24 bits
		const s32 v = m_dsp.TEMP[(octet >> 2) & 0x7F];
		return u16((octet & 2) ? (u32(v) & 0xFFFF) : (u32(v) >> 16));
	}
	if (octet < 0xE80) {   // MEMS, 32 words of 24 bits
		const s32 v = m_dsp.MEMS[(octet >> 2) & 0x1F];
		return u16((octet & 2) ? (u32(v) & 0xFFFF) : (u32(v) >> 16));
	}
	if (octet < 0xEC0) {   // MIXS, 16 words of 20 bits
		const s32 v = m_dsp.MIXS[(octet >> 2) & 0xF];
		return u16((octet & 2) ? (u32(v) & 0xFFFF) : (u32(v) >> 16));
	}
	if (octet < 0xEE0) return u16(m_dsp.EFREG[(octet - 0xEC0) / 2]);
	if (octet < 0xEE4) return u16(m_dsp.EXTS[(octet - 0xEE0) / 2]);
	return 0;
}

void scsp_maison::ecrire_octet_adresse(u32 octet, u16 data, u16 masque)
{
	octet &= 0xFFFF;

	if (octet < 0x400) {
		const u32 fente = octet / 0x20, mot = (octet & 0x1F) / 2;
		m_fente[fente][mot] = u16((m_fente[fente][mot] & ~masque) | (data & masque));
		apres_ecriture_fente(fente, mot);
		return;
	}

	if (octet < 0x430) {
		const u32 bas = octet & 0x3F;
		// THE TWO WORDS WRITTEN BY AN OR, and not by an assignment: SCIPD and -- despite what MAME's
		// comment announces -- MCIRE and not MCIPD (scsp_contract.h §2). Only bit 5 gets through, and
		// since it is an OR, writing zero does not clear it.
		if (bas == 0x20 || bas == 0x2E) m_commun[bas / 2] = u16(m_commun[bas / 2] | (data & 0x20));
		else m_commun[bas / 2] = u16((m_commun[bas / 2] & ~masque) | (data & masque));
		apres_ecriture_commun(bas);
		return;
	}

	if (octet < 0x600) return;
	if (octet < 0x700) {
		s32 &r = m_anneau[(octet - 0x600) / 2];
		r = s32(s16((u16(r) & ~masque) | (data & masque)));
		return;
	}
	auto pose = [&](u16 &w) { w = u16((w & ~masque) | (data & masque)); };
	if (octet < 0x780) {
		s16 &c = m_dsp.COEF[(octet - 0x700) / 2];
		c = s16((u16(c) & ~masque) | (data & masque));
		return;
	}
	if (octet < 0x7C0) { pose(m_dsp.MADRS[(octet - 0x780) / 2]); return; }
	if (octet < 0x800) {
		// THE DISPUTED RANGE. It has its own storage, and the mirror -- a game setting and not a chip
		// one -- copies it into MADRS. Measured in both settings by block 1's test.
		const u32 i = (octet - 0x7C0) / 2;
		pose(m_madrs_haut[i]);
		if (m_miroir) m_dsp.MADRS[i] = m_madrs_haut[i];
		return;
	}
	if (octet < 0xC00) {
		pose(m_dsp.MPRO[(octet - 0x800) / 2]);
		// The program is DECODED once on write and not on every sample: this is this repository's advance
		// over MAME and Supermodel, which decode it in the loop (docs/scsp_references.md).
		m_dsp.Decode((octet - 0x800) / 8);
		if (octet == 0xBF0) m_dsp.Start();
		return;
	}
}

scsp_maison::u16 scsp_maison::read_reg(u32 mot)
{
	return lire_octet_adresse(mot * 2);
}

void scsp_maison::write_reg(u32 mot, u16 data, u16 masque)
{
	// READ, MODIFY, WRITE, and this is not an implementation detail: it is what the chip does as this port
	// knows it, and the READ has side effects. So a byte write to 0x404 pops a byte off the MIDI queue
	// before writing. An implementation doing a direct partial write would pop nothing -- same result in
	// the register, different state in the chip.
	const u32 octet = mot * 2;
	u16 fusion = lire_octet_adresse(octet);
	fusion = u16((fusion & ~masque) | (data & masque));
	ecrire_octet_adresse(octet, fusion, 0xFFFF);
}

// -----------------------------------------------------------------------------------------------------
// WHAT FOLLOWS A WRITE
// -----------------------------------------------------------------------------------------------------

void scsp_maison::niveaux_depuis_scilv()
{
	// THE LEVEL IS ASSEMBLED BIT BY BIT, one bit per SCILV register: bit 0 of the level from SCILV0, bit 1
	// from SCILV1, bit 2 from SCILV2, each taken at the SOURCE's position (scsp_contract.h §12 point 3).
	// And it is recomputed only here -- arming SCIEB before SCILV has been written leaves every level zero.
	const u16 l0 = m_commun[0x24 / 2], l1 = m_commun[0x26 / 2], l2 = m_commun[0x28 / 2];
	auto niveau = [&](unsigned bit) {
		return int(((l0 >> bit) & 1) | (((l1 >> bit) & 1) << 1) | (((l2 >> bit) & 1) << 2));
	};
	m_niv_midi  = niveau(3);    // SCIMID
	m_niv_dma   = niveau(4);    // SCIDMA
	m_niv_cpu   = niveau(5);    // SCIIRQ
	m_niv_tima  = niveau(6);    // SCITMA
	m_niv_timbc = niveau(7);    // SCITMB
}

void scsp_maison::apres_ecriture_commun(u32 bas)
{
	switch (bas) {
	case 0x08: case 0x09:
		// MSLC, the monitored slot: bits 15-11. This port takes it AT THE MOMENT OF THE WRITE and not once
		// per sample as MAME does, because the Model 3's driver writes MSLC and reads straight back to
		// find a free slot -- latched per sample, it reads the previous slot back and the music ends up on
		// half the slots (scsp.cpp's comment says so, and this is a deliberate divergence).
		m_mslc = u8((m_commun[0x08 / 2] >> 11) & 0x1F);
		break;
	case 0x02: case 0x03:
		// The DSP's delay line: its size and its origin in sound RAM.
		m_dsp.RBL = (8 * 1024) << ((m_commun[0x02 / 2] >> 7) & 3);
		m_dsp.RBP = m_commun[0x02 / 2] & 0x3F;
		break;
	case 0x2A: case 0x2B:
		m_mcieb = m_commun[0x2A / 2];
		break;
	case 0x2C: case 0x2D:
		// MCIPD is written by assignment -- but bit 5 written asks for an interrupt on the master side,
		// and since the read copies m_mcipd back, the bit returns. This is the third of the three paths
		// block 1 measured, and it is not the same as SCIPD's nor as MCIRE's.
		if (m_commun[0x2C / 2] & 0x20) m_mcipd = u16(m_mcipd | 0x20);
		break;
	case 0x2E: case 0x2F:
		// MCIRE acknowledges: the bits written are cleared from m_mcipd.
		m_mcipd = u16(m_mcipd & ~m_commun[0x2E / 2]);
		break;
	case 0x24: case 0x25: case 0x26: case 0x27: case 0x28: case 0x29:
		niveaux_depuis_scilv();
		break;
	case 0x16: case 0x17:
		if (m_commun[0x16 / 2] & 0x1000) executer_dma();   // DEXE
		break;
	case 0x18: case 0x19: arme_timer(TIMER_A); break;
	case 0x1A: case 0x1B: arme_timer(TIMER_B); break;
	case 0x1C: case 0x1D: arme_timer(TIMER_C); break;
	case 0x1E: case 0x1F:
		verifier_attente();
		break;
	case 0x20: case 0x21:
		// SCIPD: only an already-armed CPU request restarts the examination. The write itself is an OR of
		// bit 5, done further up.
		if (m_commun[0x1E / 2] & m_commun[0x20 / 2] & 0x20) verifier_attente();
		break;
	case 0x22: case 0x23:
		m_commun[0x20 / 2] = u16(m_commun[0x20 / 2] & ~m_commun[0x22 / 2]);
		acquitter_interruptions();
		break;
	default:
		break;
	}
}

void scsp_maison::apres_ecriture_fente(u32 fente, u32 mot)
{
	// THREE WORDS ARE RECOMPUTED ON WRITE, and not only at key-on. Forgetting one leaves a voice already
	// running on stale values -- which is not heard straight away, and shows up a million samples later.
	if (mot == 8) { calculer_pas(fente); return; }     // OCT and FNS: the read step
	if (mot == 9) { calculer_lfo(fente); return; }     // LFOF, the waveforms, the depths
	if (mot == 5) {
		// LPSLNK/KRS/DL/RR. And the release is recomputed WITH A BASE RATE OF ZERO, which is not what the
		// key-on computation does: that one starts from `octave + 2*KRS + bit of FNS`. The inconsistency
		// is in the original and it is OBSERVABLE -- it is the one the replay found.
		//
		// The trail of the discovery, because it says how these things are found: the replay of a
		// 1500-frame scene produced ONE wrong sample out of 1 149 952, at -967 against -968. The tables
		// were all identical (65536 pan entries, 1024 of curve, 124 of times: zero difference). Replaying
		// the two cores side by side, one sample at a time, dated the gap: sample 894 568, slot 0, the
		// VOLUME in release, the address being right. My step was 353 548 and its 422 293, that is index
		// 61 instead of 62 -- one rate notch. With RR=0x1F and an octave of -1, `clamp(-1 + 62)` against
		// `clamp(0 + 62)`: this very bracket was missing.
		const u16 *r = m_fente[fente];
		Vive &v = m_vive[fente];
		v.rr = m_pas_dr[std::clamp(2 * int(champ(r, C_RR)), 0, 63)];
		v.dl = s32(0x1F - champ(r, C_DL));
		return;
	}
	if (mot != 0) return;

	// KYONEX, THE TRIGGER, AND IT IS GLOBAL: it sets off a sweep of ALL THIRTY-TWO slots and not of the one
	// just written. Every slot carrying KYONB and sitting in RELACHE starts; every slot not carrying it
	// stops. Then the bit lowers itself.
	//
	// AND A SLOT THAT IS PLAYING IS NOT RESTARTED, because the condition requires the RELACHE state: a
	// second KYONEX on a running voice does nothing. A rewrite that restarted on every trigger would make
	// all the game's repeated notes snap (scsp_contract.h §10 point 10).
	const scsp_contract::Champ *kyonex = scsp_contract::champ_fente("KYONEX");
	if (!kyonex || !(m_fente[fente][0] & kyonex->masque)) return;

	for (unsigned i = 0; i < FENTES; i++) {
		const bool kyonb = champ(m_fente[i], C_KYONB) != 0;
		if (kyonb && m_vive[i].etat == RELACHE) demarrer(i);
		else if (!kyonb) arreter(i, true);
	}
	m_fente[fente][0] = u16(m_fente[fente][0] & ~kyonex->masque);
}

// -----------------------------------------------------------------------------------------------------
// BLOCK 2 -- the oscillator, PCM addressing and the four loop modes.
//
// What the block has to produce is recorded in tools/son/test_scsp_fentes.cpp, which reads the exact
// sequence of sample INDICES the read head walks through. The three facts that are not obvious, and that
// the contract holds from measurement and not from a reading (scsp_contract.h §9):
//
//   LSA and LEA count SAMPLES, while the 16-bit read counts BYTES.
//   RAM folds at 1 MB, and a read address loses its bit 0.
//   LOOPING TRIGGERS ONE SAMPLE EARLY, because it examines TWO addresses one of which is a ghost: `nxt`
//   is reassigned on every sample, so what the `nxt` pass WRITES is lost -- but its side effects, the
//   turnaround and the stop, survive. No sample beyond LEA is ever read, and a rewrite applying the loop
//   only to `cur` would shift all four modes.
// -----------------------------------------------------------------------------------------------------

// -----------------------------------------------------------------------------------------------------
// BLOCK 3 -- the envelope.
// -----------------------------------------------------------------------------------------------------

void scsp_maison::construire_enveloppe()
{
	// THE TWO TIME TABLES, in milliseconds, read off hardware. The first two entries are an infinite time
	// (the note never rises nor falls), and the attack table ends in two zeros that mean "immediate" -- a
	// special case, not a limit of the curve.
	static const double MS_AR[64] = {
		100000, 100000, 8100, 6900, 6000, 4800, 4000, 3400, 3000, 2400, 2000, 1700, 1500,
		1200, 1000, 860, 760, 600, 500, 430, 380, 300, 250, 220, 190, 150, 130, 110, 95,
		76, 63, 55, 47, 38, 31, 27, 24, 19, 15, 13, 12, 9.4, 7.9, 6.8, 6.0, 4.7, 3.8, 3.4, 3.0, 2.4,
		2.0, 1.8, 1.6, 1.3, 1.1, 0.93, 0.85, 0.65, 0.53, 0.44, 0.40, 0.35, 0.0, 0.0 };
	static const double MS_DR[64] = {
		100000, 100000, 118200, 101300, 88600, 70900, 59100, 50700, 44300, 35500, 29600, 25300, 22200, 17700,
		14800, 12700, 11100, 8900, 7400, 6300, 5500, 4400, 3700, 3200, 2800, 2200, 1800, 1600, 1400, 1100,
		920, 790, 690, 550, 460, 390, 340, 270, 230, 200, 170, 140, 110, 98, 85, 68, 57, 49, 43, 34,
		28, 25, 22, 18, 14, 12, 11, 8.5, 7.1, 6.1, 5.4, 4.3, 3.6, 3.1 };

	m_pas_ar[0] = m_pas_dr[0] = m_pas_ar[1] = m_pas_dr[1] = 0;   // infinite time
	for (unsigned i = 2; i < 64; i++) {
		const double echelle = double(1 << EG_PAS);
		m_pas_ar[i] = MS_AR[i] != 0.0 ? s32((1023 * 1000.0) / (44100.0 * MS_AR[i]) * echelle)
		                              : s32(1024 << EG_PAS);
		m_pas_dr[i] = s32((1023 * 1000.0) / (44100.0 * MS_DR[i]) * echelle);
	}

	// THE ATTENUATION CURVE: 3/32 dB per step, 95.9 dB end to end. It is the one every state except the
	// attack goes through.
	for (unsigned i = 0; i < 0x400; i++) {
		const float db = float(3 * (int(i) - 0x3FF)) / 32.0f;
		m_courbe_eg[i] = s32(powf(10.0f, db / 20.0f) * float(1 << PAS));
	}
}

// -----------------------------------------------------------------------------------------------------
// BLOCK 4 -- the two LFOs.
// -----------------------------------------------------------------------------------------------------

void scsp_maison::construire_lfo()
{
	// THE FOUR WAVEFORMS, and they are not four variants of one thing: the sawtooth is monotonic, the
	// square has only two values, the triangle rises and falls, the noise is drawn from a congruential
	// generator AT INITIALISATION -- so fixed for the device's lifetime, and not redrawn each time round.
	for (int i = 0; i < 256; i++) {
		m_lfo_scie_a[i] = 255 - i;
		m_lfo_scie_p[i] = i < 128 ? i : i - 256;

		m_lfo_carre_a[i] = i < 128 ? 255 : 0;
		m_lfo_carre_p[i] = i < 128 ? 127 : -128;

		m_lfo_tri_a[i] = i < 128 ? 255 - (i * 2) : (i * 2) - 256;
		m_lfo_tri_p[i] = i < 64 ? i * 2 : (i < 128 ? 255 - i * 2 : (i < 192 ? 256 - i * 2 : i * 2 - 511));

		const int bruit = int(tirage() & 0xFF);
		m_lfo_bruit_a[i] = bruit;
		m_lfo_bruit_p[i] = 128 - bruit;
	}

	// THE TWO DEPTH SCALES, read off hardware as well: the vibrato in CENTS, the tremolo in DECIBELS.
	static const float CENTS[8] = { 0.0f, 7.0f, 13.5f, 27.0f, 55.0f, 112.0f, 230.0f, 494.0f };
	static const float DECIBELS[8] = { 0.0f, 0.4f, 0.8f, 1.5f, 3.0f, 6.0f, 12.0f, 24.0f };
	for (int s = 0; s < 8; s++) {
		for (int i = -128; i < 128; i++)
			m_echelle_p[s][i + 128] = int(256.0f * powf(2.0f, ((CENTS[s] * float(i)) / 128.0f) / 1200.0f));
		for (int i = 0; i < 256; i++)
			m_echelle_a[s][i] = int(256.0f * powf(10.0f, ((-DECIBELS[s] * float(i)) / 256.0f) / 20.0f));
	}
}

void scsp_maison::calculer_lfo(unsigned i)
{
	const u16 *r = m_fente[i];
	Vive &v = m_vive[i];

	// THE THIRTY-TWO RATES, in hertz -- and EIGHT OF THEM DO NOTHING. The phase step is
	// `(u32)(256 * f * 256 / 44100)`, and the conversion TRUNCATES: f >= 44100/65536 = 0.673 Hz is needed
	// for the step to be at least one. The first eight entries are below that, so their phase never moves.
	// A rewrite keeping the phase in float, or rounding, would make eight rates vibrate that this port
	// leaves still (scsp_contract.h §11 point 2).
	static const float HZ[32] = {
		0.17f, 0.19f, 0.23f, 0.27f, 0.34f, 0.39f, 0.45f, 0.55f, 0.68f, 0.78f, 0.92f, 1.10f,
		1.39f, 1.60f, 1.87f, 2.27f, 2.87f, 3.31f, 3.92f, 4.79f, 6.15f, 7.18f, 8.60f, 10.8f,
		14.4f, 17.2f, 21.5f, 28.7f, 43.1f, 57.4f, 86.1f, 172.3f };
	const unsigned lfof = champ(r, C_LFOF);
	const u32 pas = u32(float(1 << 8) * (HZ[lfof & 31] * 256.0f / 44100.0f));

	const unsigned plfos = champ(r, C_PLFOS), alfos = champ(r, C_ALFOS);
	if (plfos != 0) {
		const int *tables[4] = { m_lfo_scie_p, m_lfo_carre_p, m_lfo_tri_p, m_lfo_bruit_p };
		v.plfo.pas = pas;
		v.plfo.table = tables[champ(r, C_PLFOWS) & 3];
		v.plfo.echelle = m_echelle_p[plfos & 7];
	}
	if (alfos != 0) {
		const int *tables[4] = { m_lfo_scie_a, m_lfo_carre_a, m_lfo_tri_a, m_lfo_bruit_a };
		v.alfo.pas = pas;
		v.alfo.table = tables[champ(r, C_ALFOWS) & 3];
		v.alfo.echelle = m_echelle_a[alfos & 7];
	}
}

scsp_maison::s32 scsp_maison::pas_lfo(Lfo &l)
{
	l.phase = u16(l.phase + l.pas);          // u16: the type wraps the phase
	const int p = l.table[l.phase >> 8];
	return s32(l.echelle[p + 128]) << (PAS - 8);
}

scsp_maison::s32 scsp_maison::gain_lfo(Lfo &l)
{
	l.phase = u16(l.phase + l.pas);
	const int p = l.table[l.phase >> 8];
	return s32(l.echelle[p]) << (PAS - 8);
}

void scsp_maison::calculer_enveloppe(unsigned i)
{
	const u16 *r = m_fente[i];
	Vive &v = m_vive[i];

	// KRS=0xF MEANS ZERO, and not "ignore KRS": neither the octave nor FNS's bit gets through. Measured at
	// OCT=2: KRS=0 saturates in 56 samples, KRS=0xF in 83 -- exactly OCT=0's figure, so the octave is
	// indeed thrown away and not reduced (scsp_contract.h §10 point 9).
	const unsigned krs = champ(r, C_KRS);
	const int octave = (int(champ(r, C_OCT)) ^ 8) - 8;
	const int rate = krs != 0xF ? octave + 2 * int(krs) + int((champ(r, C_FNS) >> 9) & 1) : 0;
	auto pas_ar = [&](unsigned champ_valeur) {
		return m_pas_ar[std::clamp(rate + 2 * int(champ_valeur), 0, 63)];
	};
	auto pas_dr = [&](unsigned champ_valeur) {
		return m_pas_dr[std::clamp(rate + 2 * int(champ_valeur), 0, 63)];
	};

	// A VOICE DOES NOT START FROM SILENCE: 0x17F out of 1023, that is 37% of the way. With AR=0 the attack
	// is infinite and the multiplier stays at 383 << 2 = 1532, which is how the test reads this constant
	// back (scsp_contract.h §10 point 2).
	v.volume = 0x17F << EG_PAS;
	v.ar  = pas_ar(champ(r, C_AR));
	v.d1r = pas_dr(champ(r, C_D1R));
	v.d2r = pas_dr(champ(r, C_D2R));
	v.rr  = pas_dr(champ(r, C_RR));
	v.dl  = s32(0x1F - champ(r, C_DL));   // INVERTED
	v.eghold = champ(r, C_EGHOLD) != 0;
}

scsp_maison::s32 scsp_maison::avancer_enveloppe(unsigned i)
{
	Vive &v = m_vive[i];
	const u16 *r = m_fente[i];

	switch (v.etat) {
	case ATTAQUE:
		v.volume += v.ar;
		if (v.volume >= (0x3FF << EG_PAS)) {
			// LPSLNK LOCKS THIS TRANSITION IN: with it the state does not change on saturation, it stays
			// ATTAQUE and only leaves through the oscillator, when LSA is passed (block 2).
			if (!champ(r, C_LPSLNK)) {
				v.etat = CHUTE1;
				// A large enough D1R skips CHUTE1 entirely.
				if (v.d1r >= (1024 << EG_PAS)) v.etat = CHUTE2;
			}
			v.volume = 0x3FF << EG_PAS;
		}
		// EGHOLD HOLDS THE OUTPUT, NOT THE VOLUME: the volume still rises internally, but the output is at
		// full level from the first sample. A slow attack then only serves to delay CHUTE1.
		if (v.eghold) return 0x3FF << (PAS - 10);
		break;
	case CHUTE1:
		// THE EXIT TEST COMES AFTER THE DECREMENT, so CHUTE1 ALWAYS decrements once, even when DL=0 makes
		// it give way on the first tick: the plateau is then 4052 and not 4096. A rewrite testing before
		// decrementing would pass every other case (scsp_contract.h §10 point 4).
		v.volume -= v.d1r;
		if (v.volume <= 0) v.volume = 0;
		if ((v.volume >> (EG_PAS + 5)) <= v.dl) v.etat = CHUTE2;
		break;
	case CHUTE2:
		// IT IS THE REGISTER THAT IS CONSULTED, not the computed step: a non-zero D2R can give a zero step
		// at a low rate, and the output would then NOT freeze, it would fall by zero per sample. Same
		// result to the ear, not the same state (scsp_contract.h §10 point 6).
		if (champ(r, C_D2R) == 0) return (v.volume >> EG_PAS) << (PAS - 10);
		v.volume -= v.d2r;
		if (v.volume <= 0) v.volume = 0;
		break;
	case RELACHE:
		v.volume -= v.rr;
		if (v.volume <= 0) {
			v.volume = 0;
			arreter(i, false);   // without keyoff: the slot becomes inactive for good
		}
		break;
	default:
		return 1 << PAS;
	}
	return (v.volume >> EG_PAS) << (PAS - 10);
}

void scsp_maison::demarrer(unsigned i)
{
	Vive &v = m_vive[i];
	v.active = true;
	v.cur = 0;
	v.nxt = 1u << PAS;
	v.arriere = false;
	calculer_pas(i);
	calculer_enveloppe(i);
	calculer_lfo(i);
	v.etat = ATTAQUE;
}

void scsp_maison::arreter(unsigned i, bool keyoff)
{
	// Key-off does not cut the sound: it moves the envelope into RELACHE. The slot only becomes inactive
	// when the volume reaches zero, and it is then a stop WITHOUT keyoff that puts it out (block 3).
	if (keyoff) m_vive[i].etat = RELACHE;
	else m_vive[i].active = false;
	m_fente[i][0] = u16(m_fente[i][0] & ~0x800);   // KYONB drops in both cases
}

void scsp_maison::calculer_pas(unsigned i)
{
	// OCT IS A SIGNED FOUR-BIT INTEGER, in a representation biased by EXCLUSIVE OR: `(OCT ^ 8) - 8`, so
	// field 0xF is -1 and field 0x8 is -8. An implementation reading it as unsigned would play eight
	// octaves too high on half the notes (scsp_contract.h §11 point 1).
	const u16 *r = m_fente[i];
	const int octave = (int(champ(r, C_OCT)) ^ 8) - 8 + PAS - 10;
	u32 fn = champ(r, C_FNS) + (1u << 10);
	m_vive[i].pas = octave >= 0 ? (fn << octave) : (fn >> -octave);
}

scsp_maison::u8 scsp_maison::lire_octet_ram(u32 octet) const
{
	// The fold at 1 MB, and big-endian order: byte 0 is the HIGH byte of word 0, like the 68000 that fills
	// this RAM.
	octet &= 0xFFFFF;
	if (octet >= m_ram_octets) return 0;
	return u8(m_ram[octet >> 1] >> ((octet & 1) ? 0 : 8));
}

scsp_maison::u16 scsp_maison::lire_mot_ram(u32 octet) const
{
	// `& 0xFFFFE`: the fold at 1 MB AND the loss of bit 0. Both are observable (block 2, case 7).
	octet &= 0xFFFFE;
	if (octet >= m_ram_octets) return 0;
	return m_ram[octet >> 1];
}

scsp_maison::s32 scsp_maison::une_fente(unsigned i)
{
	Vive &v = m_vive[i];
	const u16 *r = m_fente[i];

	const unsigned ssctl = champ(r, C_SSCTL);
	// SSCTL=3 RETURNS BEFORE ANYTHING ELSE, so it does not even advance the read head -- unlike SSCTL=2,
	// which also returns silence but walks the source. The difference only shows later (block 2, case 8).
	if (ssctl == 3) return 0;

	u32 pas = v.pas;
	// THE PLFO MODULATES THE STEP, and its PLFOS field loses its low bit -- a deliberate divergence taken
	// over from Supermodel and tuned by ear for VF3: the fields go in pairs, 0 with 1, 2 with 3, 4 with 5,
	// 6 with 7 (docs/scsp_audit.md §2 and scsp_contract.h §11 point 5). The guard is `!= 0` on the
	// ALREADY masked field, so PLFOS=1 does not modulate at all.
	if (champ(r, C_PLFOS) != 0) {
		pas = u32((std::uint64_t(pas) * u32(pas_lfo(v.plfo))) >> PAS);
	}

	const bool pcm8 = champ(r, C_PCM8B) != 0;
	u32 a1, a2;
	if (pcm8) { a1 = v.cur >> PAS; a2 = v.nxt >> PAS; }
	else      { a1 = (v.cur >> (PAS - 1)) & ~1u; a2 = (v.nxt >> (PAS - 1)) & ~1u; }

	// Ring modulation is block 4; its GUARD covers three fields and not MDL alone, so it is already
	// written here so that block 2 does not contradict it.
	const unsigned mdl = champ(r, C_MDL), mdxsl = champ(r, C_MDXSL), mdysl = champ(r, C_MDYSL);
	if (mdl != 0 || mdxsl != 0 || mdysl != 0) {
		s32 smp = (m_anneau[(m_ptr_anneau + mdxsl) & 63] + m_anneau[(m_ptr_anneau + mdysl) & 63]) / 2;
		smp <<= 0xA;
		// Depth 24 and MDL's low bit thrown away: a deliberate divergence, taken over from Supermodel and
		// tuned by ear for VF3 (docs/scsp_audit.md §2, point 2).
		smp >>= 0x18 - int(mdl & 0xE);
		if (!pcm8) smp <<= 1;
		a1 += u32(smp); a2 += u32(smp);
	}

	s32 ech = 0;
	if (ssctl == 0) {
		const u32 sa = (champ(r, C_SA_HI) << 16) | champ(r, C_SA_LO);
		const s32 frac = s32(v.cur & ((1u << PAS) - 1)), plein = 1 << PAS;
		if (pcm8) {
			// One byte per sample, placed in the HIGH eight bits: it is not stretched over sixteen.
			const s32 p1 = s32(std::int8_t(lire_octet_ram(sa + a1)));
			const s32 p2 = s32(std::int8_t(lire_octet_ram(sa + a2)));
			ech = ((p1 << 8) * (plein - frac) + (p2 << 8) * frac) >> PAS;
		} else {
			const s32 p1 = s32(s16(lire_mot_ram(sa + a1)));
			const s32 p2 = s32(s16(lire_mot_ram(sa + a2)));
			ech = (p1 * (plein - frac) + p2 * frac) >> PAS;
		}
	} else if (ssctl == 1) {
		ech = s32(s16(tirage() & 0xFFFF));
	}

	// SBCTL: TWO INVERSIONS THAT DO NOT RESEMBLE EACH OTHER. Bit 0 is an exclusive OR on the integer,
	// WITHOUT truncation; bit 1 truncates to sixteen bits. The two together give the opposite to within
	// one: 1000 comes out at -1001, which block 2 measures.
	const unsigned sbctl = champ(r, C_SBCTL);
	if (sbctl & 1) ech ^= 0x7FFF;
	if (sbctl & 2) ech = s32(s16(u16(ech) ^ 0x8000));

	// --- the step, AFTER the read
	if (v.arriere) v.cur -= pas; else v.cur += pas;
	v.nxt = v.cur + (1u << PAS);

	a1 = v.cur >> PAS;
	a2 = v.nxt >> PAS;

	const u32 lsa = champ(r, C_LSA), lea = champ(r, C_LEA);
	const unsigned lpctl = champ(r, C_LPCTL);

	// LPSLNK: the attack does not end at volume saturation but when LSA is passed (block 3).
	if (a1 >= lsa && !v.arriere && champ(r, C_LPSLNK) && v.etat == ATTAQUE) v.etat = CHUTE1;

	// THE LOOPING, ON BOTH ADDRESSES. See the block's header: the `nxt` pass writes into the void, but its
	// side effects are the ones that shift all four modes by one sample.
	for (int k = 0; k < 2; k++) {
		u32 &adresse = k ? v.nxt : v.cur;
		const u32 a = k ? a2 : a1;
		s32 reste;
		switch (lpctl) {
		case 0:   // no loop
			if (a >= lsa && a >= lea) { v.active = false; m_fente[i][0] = u16(m_fente[i][0] & ~0x800); }
			break;
		case 1:   // normal loop
			if (a >= lea) {
				reste = s32(adresse - (lea << PAS));
				adresse = (lsa << PAS) + u32(reste);
			}
			break;
		case 2:   // reverse loop
			if (a >= lsa && !v.arriere) {
				reste = s32(adresse - (lsa << PAS));
				adresse = (lea << PAS) - u32(reste);
				v.arriere = true;
			} else if ((a < lsa || (adresse & 0x80000000u)) && v.arriere) {
				reste = s32((lsa << PAS) - adresse);
				adresse = (lea << PAS) - u32(reste);
			}
			break;
		case 3:   // ping-pong
			if (a >= lea) {
				reste = s32(adresse - (lea << PAS));
				adresse = (lea << PAS) - u32(reste);
				v.arriere = true;
			} else if ((a < lsa || (adresse & 0x80000000u)) && v.arriere) {
				reste = s32((lsa << PAS) - adresse);
				adresse = (lsa << PAS) + u32(reste);
				v.arriere = false;
			}
			break;
		}
	}

	if (m_dit) fprintf(stderr, "  diag.osc slot %2u : raw %7d  state %d  volume %9d  alfos %u plfos %u cur %u\n",
	                   i, ech, v.etat, v.volume, champ(r, C_ALFOS), champ(r, C_PLFOS), v.cur);
	// SDIR SKIPS THE ENVELOPE AND THE ALFO. It is the door through which block 2 measures itself without
	// block 3, and it is also why TL is set to zero further down: a slot in SDIR goes past all the
	// attenuation (docs/scsp_audit.md §2, point 6).
	if (!champ(r, C_SDIR)) {
		// THE ALFO ATTENUATES BEFORE THE ENVELOPE, and its guard also stops the phase advancing: with
		// ALFOS=0 the function is not called at all.
		if (champ(r, C_ALFOS) != 0) ech = (ech * gain_lfo(v.alfo)) >> PAS;
		//
		// TWO ARITHMETICS, AND IT IS BLOCK 3'S CENTRAL FACT. The ATTAQUE state multiplies by the RAW
		// VOLUME -- linearly -- when everything else goes through the decibel curve. The two almost meet:
		// at saturation one gives 4092 and the other 4096, so that the first sample of any voice comes out
		// four thousandths lower than the next. Measured, not rounding noise (scsp_contract.h §10 point 1).
		if (v.etat == ATTAQUE) ech = (ech * avancer_enveloppe(i)) >> PAS;
		else {
			const s32 brut = avancer_enveloppe(i);
			if (m_dit) fprintf(stderr, "  diag.eg  slot %2u : eg %6d -> index %4d -> curve %6d\n",
			                   i, brut, brut >> (PAS - 10), m_courbe_eg[brut >> (PAS - 10)]);
			ech = (ech * m_courbe_eg[brut >> (PAS - 10)]) >> PAS;
		}
	}

	// The modulation ring, which the slot feeds unless STWINH forbids it. The gain applied here is TL's at
	// full output level, or zero when SDIR sets TL aside.
	if (!champ(r, C_STWINH)) {
		s32 g, d;
		gains(champ(r, C_SDIR) ? 0 : champ(r, C_TL), 0, 7, g, d);
		m_anneau[m_ptr_anneau] = s32(s16((ech * g) >> (PAS + 1)));
	}

	return ech;
}

// -----------------------------------------------------------------------------------------------------
// THE MIX, what it takes for a sample to come out (block 7 will complete it)
// -----------------------------------------------------------------------------------------------------

void scsp_maison::construire_facteurs()
{
	// THREE DECIBEL TABLES THE SEGA WAY, and none of them is a formula: each is a sum of terms chosen bit
	// by bit. TL's first three terms break the doubling rule -- 0.4 then 0.8 then 1.5 and not 1.6 -- which
	// a "clean" rewrite rounds off without noticing. Values from the contract, and consequences frozen in
	// integers by tools/son/test_scsp_mix.cpp.
	static const float DB_TL[8] = { 0.4f, 0.8f, 1.5f, 3.0f, 6.0f, 12.0f, 24.0f, 48.0f };
	static const float DB_PAN[4] = { 3.0f, 6.0f, 12.0f, 24.0f };
	// SDLT's entry 0 is SILENCE and not a very low level.
	static const float DB_SDL[8] = { 0.0f, -36.0f, -30.0f, -24.0f, -18.0f, -12.0f, -6.0f, 0.0f };

	float f_tl[256], f_pan[32], f_sdl[8];
	for (unsigned i = 0; i < 256; i++) {
		float db = 0.0f;
		for (unsigned b = 0; b < 8; b++) if (i & (1u << b)) db -= DB_TL[b];
		f_tl[i] = powf(10.0f, db / 20.0f);
	}
	for (unsigned i = 0; i < 32; i++) {
		float db = 0.0f;
		for (unsigned b = 0; b < 4; b++) if (i & (1u << b)) db -= DB_PAN[b];
		// 0x0F is NOT the sum of the four bits: it is a hard cut. A special case in the middle of an
		// otherwise additive table, and a rewrite that summed would let -45 dB leak through.
		f_pan[i] = (i & 0xF) == 0xF ? 0.0f : powf(10.0f, db / 20.0f);
	}
	for (unsigned i = 0; i < 8; i++) f_sdl[i] = i ? powf(10.0f, DB_SDL[i] / 20.0f) : 0.0f;

	// THE FACTOR OF FOUR IS IN THE TABLE, and that is where it comes from: the mix returns four times the
	// sample at full level in the centre. Neither the adapter nor the output stage has anything to do
	// with it.
	//
	// ONE TRUNCATION ONLY, of the whole product, and the scale LAST: that is what returns the same
	// integers as the original (23196 at -3 dB, 2066 at -24 dB). Three integer tables multiplied together
	// would truncate three times, and a different order would not always land on the same integer.
	for (unsigned i = 0; i < 0x10000; i++) {
		const unsigned tl = i & 0xFF, pan = (i >> 8) & 0x1F, sdl = (i >> 13) & 7;
		const float p = (pan & 0x10) == 0 ? f_pan[pan] : 1.0f;
		m_gain[i] = s32(float(1 << PAS) * (4.0f * p * f_tl[tl] * f_sdl[sdl]));
	}
}

void scsp_maison::run(u32 n, s32 *out)
{
	// sound_prof.h's TWO SCOPES, at the same places as in MAME's core: this one answers to
	// DoMasterSamples (scsp.cpp), the DSP's further down to m_DSP.Step(). Without them VF3_SND_PROF's
	// report only carried these shares for MAME's core, and on this one it gave 97% to the 68000, zero to
	// the voices and zero to the DSP -- not a hole, but a WRONG figure, since the nested share the formula
	// subtracts from the 68000 was then zero. Measured on 2026-10-28.
	sndprof::Scope prof_scsp(sndprof::SCSP);

	// VF3_MAISON_DIAG=<index>: one sample broken down term by term -- each slot, its raw sample, its gain,
	// its contribution, then the effects returns and the total. This is not building-site leftovers: it is
	// the probe that found the release's off-by-one, and it will serve again. une_fente's two `m_dit`
	// tests are always false in normal running, hence predicted, and the measurement does not see them.
	static const char *diag_s = getenv("VF3_MAISON_DIAG");
	static const std::uint64_t diag = diag_s ? strtoull(diag_s, nullptr, 10) : ~0ull;

	for (u32 e = 0; e < n; e++) {
		s32 gauche = 0, droite = 0;
		const bool dit = (m_now + e) == diag;
		m_dit = dit;
		for (unsigned i = 0; i < FENTES; i++) {
			if (!m_vive[i].active) { m_ptr_anneau = (m_ptr_anneau + 1) & 63; continue; }
			const s32 ech = une_fente(i);
			const u16 *r = m_fente[i];
			// SDIR sets TL to zero in the mix as well, and not only the envelope aside: a slot programmed
			// with SDIR and a large TL would otherwise be wrongly silent (docs/scsp_audit.md §2,
			// point 6).
			s32 g, d;
			gains(champ(r, C_SDIR) ? 0 : champ(r, C_TL), champ(r, C_DIPAN), champ(r, C_DISDL), g, d);
			if (dit) fprintf(stderr, "diag slot %2u : smp %7d  g %6d  share %7d  (tl %3u pan %2u sdl %u)\n",
			                 i, ech, g, (ech * g) >> PAS, champ(r, C_SDIR) ? 0 : champ(r, C_TL),
			                 champ(r, C_DIPAN), champ(r, C_DISDL));
			gauche += (ech * g) >> PAS;
			droite += (ech * d) >> PAS;
			// The send to the DSP: same attenuation, but through IMXL, and two bits more.
			s32 ig, id;
			gains(champ(r, C_SDIR) ? 0 : champ(r, C_TL), 0, champ(r, C_IMXL), ig, id);
			m_dsp.SetSample((ech * ig) >> (PAS - 2), s32(champ(r, C_ISEL)), s32(champ(r, C_IMXL)));
			m_ptr_anneau = (m_ptr_anneau + 1) & 63;
		}
		{
			sndprof::Scope prof_dsp(sndprof::DSP);
			m_dsp.Step();
		}

		// The effects output: the FIRST SIXTEEN slots only, each carrying the level and the pan from its
		// own register (EFSDL/EFPAN).
		for (unsigned i = 0; i < 16; i++) {
			const unsigned efsdl = champ(m_fente[i], C_EFSDL);
			if (!efsdl) continue;
			s32 g, d;
			gains(0, champ(m_fente[i], C_EFPAN), efsdl, g, d);
			if (dit) fprintf(stderr, "diag efreg %2u : %6d  g %6d  share %7d\n",
			                 i, m_dsp.EFREG[i], g, (s32(m_dsp.EFREG[i]) * g) >> PAS);
			gauche += (s32(m_dsp.EFREG[i]) * g) >> PAS;
			droite += (s32(m_dsp.EFREG[i]) * d) >> PAS;
		}

		// The output stage. DAC18B does not change the GAIN but the RESOLUTION: without it the sum's two
		// low bits are thrown away, the factor of four between the two denominators cancelling the shift.
		// The clipping is asymmetric, +131071 and not +131072, like any signed integer.
		const bool dac18b = (m_commun[0] & 0x0100) != 0;
		if (!dac18b) { gauche >>= 2; droite >>= 2; }
		const s32 borne = dac18b ? 131072 : 32768;
		gauche = std::clamp(gauche, -borne, borne - 1);
		droite = std::clamp(droite, -borne, borne - 1);
		if (!dac18b) { gauche *= 4; droite *= 4; }
		if (dit) fprintf(stderr, "diag total left %d\n", gauche);
		if (out) { out[e * 2] = gauche; out[e * 2 + 1] = droite; }
	}
	m_now += n;
}

// -----------------------------------------------------------------------------------------------------
// BLOCK 6 -- the timers, the five interrupt lines and the DMA.
// -----------------------------------------------------------------------------------------------------

scsp_maison::u32 scsp_maison::duree_timer(u16 reg)
{
	// THE "+1" IS A DELIBERATE DIVERGENCE, and the port's third after PLFOS and MDL: MAME writes the same
	// thing WITHOUT it. It is this +1 that put VF3's tick (timer A at 0xCE) back to 882 Hz instead of 900,
	// correcting music that ran 0.8% fast against Supermodel (scsp_contract.h §12 point 1).
	return (u32(255 - (reg & 0xFF)) << ((reg >> 8) & 7)) + 1;
}

void scsp_maison::arme_timer(int lequel)
{
	static const u32 OCTET[3] = { 0x18, 0x1A, 0x1C };
	const u16 reg = m_commun[OCTET[lequel] / 2];
	m_predivise[lequel] = 1u << ((reg >> 8) & 7);
	m_compte[lequel] = u16((reg & 0xFF) << 8);
	// A COUNT OF 255 DOES NOT ARM. This is not a zero duration, it is an absence of arming: the sink
	// receives nothing at all, and not a timer(..., 1, 0).
	if ((reg & 0xFF) != 255) m_puits.timer(lequel, duree_timer(reg), 0);
}

void scsp_maison::verifier_attente()
{
	u16 attente = m_commun[0x20 / 2];
	const u16 armes = m_commun[0x1E / 2];
	if (m_midi_l != m_midi_e) {               // a byte waits in the queue: bit 3 goes up
		m_commun[0x20 / 2] = u16(m_commun[0x20 / 2] | 8);
		attente = u16(attente | 8);
	}
	// EARLY RETURN: with nothing pending, NO line is touched. That is what distinguishes "nothing pends"
	// from "something pends that nothing arms" -- only the second drops line 0, and the first version of
	// block 6's test asserted the opposite (scsp_contract.h §12 point 5).
	if (!attente) return;

	// ONE LINE AT A TIME, in a fixed order, and we RETURN at the first one that pends and is armed. If A
	// and B both pend, it is A's level that goes up and B's that is never seen.
	if (attente & armes & 0x20) { m_puits.irq(m_niv_cpu, true); return; }
	if ((attente & 0x40) && (armes & 0x40)) { m_puits.irq(m_niv_tima, true); return; }
	if ((attente & 0x80) && (armes & 0x80)) { m_puits.irq(m_niv_timbc, true); return; }
	if ((attente & 0x100) && (armes & 0x100)) { m_puits.irq(m_niv_timbc, true); return; }
	if ((attente & 0x8) && (armes & 0x8)) { m_puits.irq(m_niv_midi, true); return; }

	// THE FIFTH LINE. It is none of the four sources, and it is the one that made the interface get
	// redrawn: scsp_core.h carries a level AND a state because the chip drops four lines independently,
	// plus this one.
	m_puits.irq(0, false);
}

void scsp_maison::acquitter_interruptions()
{
	const u16 remise = m_commun[0x22 / 2];
	if (remise & 0x40) m_puits.irq(m_niv_tima, false);
	if (remise & 0x180) m_puits.irq(m_niv_timbc, false);
	if (remise & 0x20) {
		m_commun[0x20 / 2] = u16(m_commun[0x20 / 2] & ~0x20);
		m_puits.irq(m_niv_cpu, false);
	}
	if (remise & 0x8) m_puits.irq(m_niv_midi, false);
	verifier_attente();

	// AN ALREADY EXPIRED TIMER GOES STRAIGHT BACK TO PENDING, its counter having stayed at 0xFFFF. The
	// original's comment attributes this to real hardware and notes in the same sentence that another game
	// disagrees -- so it is a CHOICE, recorded as such (scsp_contract.h §12 point 6).
	if (m_compte[0] == 0xFFFF) m_commun[0x20 / 2] = u16(m_commun[0x20 / 2] | 0x40);
	if (m_compte[1] == 0xFFFF) m_commun[0x20 / 2] = u16(m_commun[0x20 / 2] | 0x80);
	if (m_compte[2] == 0xFFFF) m_commun[0x20 / 2] = u16(m_commun[0x20 / 2] | 0x100);
}

void scsp_maison::executer_dma()
{
	// DMEA: twenty bits, the low sixteen from 0x412 and the high four from bits 15-12 of 0x414.
	// DRGA: twelve bits in the register window. DTLG: the length in BYTES.
	u32 dmea = u32(m_commun[0x12 / 2] & 0xFFFE) | (u32(m_commun[0x14 / 2] & 0xF000) << 4);
	u32 drga = u32(m_commun[0x14 / 2] & 0x0FFE);
	const u32 dtlg = u32(m_commun[0x16 / 2] & 0x0FFE);
	const bool vers_la_ram = (m_commun[0x16 / 2] & 0x2000) != 0;   // DDIR
	const bool zeros = (m_commun[0x16 / 2] & 0x4000) != 0;         // DGATE

	// A DMA CANNOT OVERWRITE ITS OWN PARAMETERS, and only in one direction: they are saved when the
	// destination is the register window, since it could contain those very registers.
	const bool proteger = !vers_la_ram;
	const u16 sauve[3] = { m_commun[0x12 / 2], m_commun[0x14 / 2], m_commun[0x16 / 2] };

	for (u32 k = 0; k < dtlg; k += 2) {
		if (vers_la_ram) {
			const u16 v = zeros ? u16(0) : lire_octet_adresse(drga);
			const u32 a = dmea & 0xFFFFE;
			if (a < m_ram_octets) m_ram[a >> 1] = v;
			dmea += 2;
			if (!zeros) drga += 2;
		} else {
			const u16 v = zeros ? u16(0) : lire_mot_ram(dmea);
			ecrire_octet_adresse(drga, v, 0xFFFF);
			drga += 2;
			if (!zeros) dmea += 2;
		}
	}

	if (proteger) {
		m_commun[0x12 / 2] = sauve[0];
		m_commun[0x14 / 2] = sauve[1];
		m_commun[0x16 / 2] = sauve[2];
	}
	m_commun[0x16 / 2] = u16(m_commun[0x16 / 2] & ~0x1000);   // DEXE lowers itself
	if (m_commun[0x1E / 2] & 0x10) m_puits.irq(m_niv_dma, true);
}

void scsp_maison::timer_fired(int lequel)
{
	static const u32 OCTET[3] = { 0x18, 0x1A, 0x1C };
	static const u16 BIT[3] = { 0x40, 0x80, 0x100 };
	if (lequel < 0 || lequel > 2) return;

	m_compte[lequel] = 0xFFFF;
	m_commun[0x20 / 2] = u16(m_commun[0x20 / 2] | BIT[lequel]);
	// The reloaded counter reads back: the low byte of the timer's register goes to 0xFF.
	m_commun[OCTET[lequel] / 2] = u16((m_commun[OCTET[lequel] / 2] & 0xFF00) | (m_compte[lequel] >> 8));

	verifier_attente();
	// TIMER A ALSO PENDS ON THE MASTER SIDE, B AND C DO NOT. Nothing in VF3 reads MCIPD, but making them
	// uniform would change an observable state for nothing (scsp_contract.h §12 point 7).
	if (lequel == TIMER_A) maitre_attente(0x40);
}

void scsp_maison::midi_in(u8 octet)
{
	// The queue fills from now on: this is the counterpart of the read of 0x404, and the two together make
	// a complete behaviour. What is missing is the interrupt, which needs block 6's levels.
	m_midi[m_midi_e] = octet;
	m_midi_e = (m_midi_e + 1) & (MIDI_PROFONDEUR - 1);
	m_commun[0x20 / 2] = u16(m_commun[0x20 / 2] | 0x8);
	verifier_attente();
}

// -----------------------------------------------------------------------------------------------------
// REGISTRATION WITH THE SELECTOR, here and not in scsp_select.cpp: this is what makes the selector know
// no concrete class, hence that a binary linked without MAME does not have MAME (scsp_select.h). The last
// three return false when the core passed is not this one -- the dynamic_cast lives in the only file that
// knows the type.
// -----------------------------------------------------------------------------------------------------
namespace {

std::unique_ptr<scsp::core> fabrique(scsp::u32 horloge, scsp::u16 *ram, scsp::u32 ram_octets, scsp::sink &puits)
{
	return std::make_unique<scsp_maison>(horloge, ram, ram_octets, puits);
}

bool pose_miroir(scsp::core &c, bool on)
{
	if (auto *x = dynamic_cast<scsp_maison *>(&c)) { x->set_madrs_mirror(on); return true; }
	return false;
}

bool lit_miroir(const scsp::core &c, bool &out)
{
	if (auto *x = dynamic_cast<const scsp_maison *>(&c)) { out = x->madrs_mirror(); return true; }
	return false;
}

bool lit_gain(const scsp::core &c, float &out)
{
	if (auto *x = dynamic_cast<const scsp_maison *>(&c)) { out = x->output_gain(); return true; }
	return false;
}

const scsp::inscription INSCRIPTION = { "maison", fabrique, pose_miroir, lit_miroir, lit_gain };
const scsp::inscrit INSCRIT{ INSCRIPTION };

} // namespace
