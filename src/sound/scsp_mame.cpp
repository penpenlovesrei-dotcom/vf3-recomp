#include "scsp_mame.h"

#include "scsp_select.h"

#include <algorithm>
#include <cmath>

namespace {

// attoseconds to samples, rounding up as the board does for cycles: a timer must never be due sooner than
// asked, or an interrupt arrives early and the driver's tick drifts.
constexpr unsigned __int128 AS_PER_SEC = (unsigned __int128)1000000000000000000ull;
constexpr unsigned ECHANTILLONS_PAR_SEC = 44100;

std::uint32_t en_echantillons(scsp_device::attotime as)
{
	if (as == scsp_device::NEVER) return 0;
	const unsigned __int128 n = (as * ECHANTILLONS_PAR_SEC + AS_PER_SEC - 1) / AS_PER_SEC;
	return std::uint32_t(n > 0xFFFFFFFFull ? 0xFFFFFFFFull : (unsigned long long)n);
}

} // namespace

scsp_mame::scsp_mame(std::uint32_t clock, std::uint16_t *ram, std::uint32_t ram_bytes, scsp::sink &s)
	: m_sink(s)
{
	scsp_device::host h;
	// The line the old core names, raised or dropped. It names a different one for each source and can drop
	// one while another is up, which is why scsp_core.h carries both and not just a level.
	h.irq = [this](int line, int state) { m_sink.irq(line, state != CLEAR_LINE); };
	// Bits, which this adapter cannot turn into bytes (scsp_mame.h, note 3). Counted so the gap shows.
	h.midi_out = [this](int) { ++m_bits_dropped; };
	h.timer_adjust = [this](int t, scsp_device::attotime delay, scsp_device::attotime period) {
		m_sink.timer(t, en_echantillons(delay), en_echantillons(period));
	};
	// The old core PULLS the time; the adapter answers from its own counter so run() can push.
	//
	// AND IT COUNTS INCLUSIVELY, which cost a sample before the differential harness found it. Its update()
	// does `n = idx - m_written + 1`: it makes the samples of index m_written THROUGH idx, so at clock `idx`
	// it has made idx + 1 of them. The new interface's run(n) means "n samples", full stop. So the index
	// this adapter reports is ONE BEHIND its counter -- m_now is "how many exist", idx is "the last one" --
	// and update() then makes exactly m_now - m_written. The quirk stays here, which is what an adapter is
	// for; neither the board nor a future core has to know about it.
	h.sample_index = [this] { return m_now ? m_now - 1 : 0; };
	h.time_is_zero = [this] { return m_now == 0; };
	m_dev = std::make_unique<scsp_device>(clock, ram, ram_bytes, h);
}

void scsp_mame::reset()
{
	m_now = 0;
	m_bits_dropped = 0;
	m_dev->m_out.clear();
}

std::uint16_t scsp_mame::read_reg(std::uint32_t word_offset)
{
	return m_dev->read(word_offset);
}

void scsp_mame::write_reg(std::uint32_t word_offset, std::uint16_t data, std::uint16_t mem_mask)
{
	m_dev->write(word_offset, data, mem_mask);
}

void scsp_mame::run(std::uint32_t n, std::int32_t *out)
{
	m_now += n;
	m_dev->update();
	// The old core appended 2 * n floats (it makes exactly what the clock asked for). Recovering the
	// integer is exact: the divisor is a power of two and the value fits in a float's mantissa. See
	// scsp_mame.h note 2 for why PLEINE_ECHELLE is the right multiplier in both of the chip's two modes.
	const std::vector<float> &f = m_dev->m_out;
	const std::size_t veut = std::size_t(n) * 2;
	const std::size_t a = std::min(veut, f.size());
	for (std::size_t i = 0; i < a; i++)
		out[i] = std::int32_t(std::lround(double(f[i]) * PLEINE_ECHELLE));
	// If the old core made fewer than asked -- it should not, but a silent short buffer would be read as
	// silence and blamed on the chip -- the rest is zeroed rather than left as whatever was there.
	for (std::size_t i = a; i < veut; i++) out[i] = 0;
	// CONSUME EXACTLY WHAT WAS TAKEN, and keep any remainder. Clearing the whole buffer here threw away
	// anything the old core had made beyond this call's n, and the differential harness found it: one sample
	// pair lost at frame 1006 of fv2-fight.txt, with every sample after it identical once shifted by one.
	// A whole-buffer clear is the kind of mistake that makes a faithful core look unfaithful.
	if (a < m_dev->m_out.size())
		m_dev->m_out.erase(m_dev->m_out.begin(), m_dev->m_out.begin() + std::ptrdiff_t(a));
	else
		m_dev->m_out.clear();
}

void scsp_mame::timer_fired(int which)
{
	m_dev->timer_fired(which);
}

void scsp_mame::midi_in(std::uint8_t byte)
{
	m_dev->midi_in_byte(byte);
}

// -----------------------------------------------------------------------------------------------------
// L'INSCRIPTION AU SELECTEUR, ici et non dans scsp_select.cpp (scsp_select.h dit pourquoi) : un binaire
// qui ne lie pas ce fichier n'a pas de coeur "mame", et son selecteur le dit.
// -----------------------------------------------------------------------------------------------------
namespace {

std::unique_ptr<scsp::core> fabrique_mame(scsp::u32 horloge, scsp::u16 *ram, scsp::u32 ram_octets, scsp::sink &puits)
{
	return std::make_unique<scsp_mame>(horloge, ram, ram_octets, puits);
}

bool pose_miroir_mame(scsp::core &c, bool on)
{
	if (auto *x = dynamic_cast<scsp_mame *>(&c)) { x->device().set_madrs_mirror(on); return true; }
	return false;
}

bool lit_miroir_mame(const scsp::core &c, bool &out)
{
	if (auto *x = dynamic_cast<const scsp_mame *>(&c)) { out = x->device_const().madrs_mirror(); return true; }
	return false;
}

bool lit_gain_mame(const scsp::core &c, float &out)
{
	if (auto *x = dynamic_cast<const scsp_mame *>(&c)) { out = x->device_const().output_gain(); return true; }
	return false;
}

const scsp::inscription INSCRIPTION_MAME = { "mame", fabrique_mame, pose_miroir_mame, lit_miroir_mame, lit_gain_mame };
const scsp::inscrit INSCRIT_MAME{ INSCRIPTION_MAME };

} // namespace
