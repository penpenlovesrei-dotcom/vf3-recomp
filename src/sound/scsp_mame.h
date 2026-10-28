// scsp_mame.h -- MAME's SCSP presented through the new interface.
//
// Step 3 of scsp/docs/scsp_migration_plan.md, and the plan says why it comes first: "l'interface posée d'abord
// sur le coeur de MAME : un adaptateur qui présente l'ancien coeur à travers le contrat neuf. C'est le geste
// qui décide si le contrat tient, et il ne coûte rien à défaire."
//
// WHAT IT PROVES, AND WHAT IT DOES NOT. Wrapping the old core cannot prove anything about numbers -- the
// samples are the old core's either way. What it proves is that the SHAPE of scsp_core.h is sufficient: that
// everything this board asks of the chip goes through three sink methods and a pushed run(), with nothing
// left over. The proof is the board driven through it producing BYTE-IDENTICAL sound, which is what
// tools/son/comparer-son.cmd checks.
//
// THREE PLACES WHERE THE OLD CORE AND THE NEW SHAPE DO NOT LINE UP, named here rather than papered over:
//
//   1. TIME. The old core PULLS -- update() asks the host what time it is. The adapter keeps a sample
//      counter of its own and answers that question itself, so run(n) can push. Nothing is lost; the
//      granularity decision simply moves to the board, which is the point.
//
//   2. THE OUTPUT'S SCALE. The old core pushes floats, clamped to +-131072 then divided by 131072 (18-bit),
//      or to +-32768 after a >>2 then divided by 32768 (16-bit). Multiplying by 131072 recovers the integer
//      EXACTLY in both cases -- the divisors are powers of two and the magnitudes fit in a float's mantissa
//      -- and in 16-bit mode gives the 18-bit value with its low two bits zero, which is what the >>2 means.
//      So this adapter always reports on the 18-bit scale, and says so.
//
//   3. MIDI OUT. The new interface carries BYTES; the old core emits BITS (transmit_register_get_data_bit).
//      Reassembling bytes needs the transmitter's baud timing, which lives inside the old core. So this
//      adapter does NOT bridge it: it COUNTS the bits and exposes the count, so the gap is visible instead of
//      silent. It costs nothing today -- the board discards MIDI out anyway -- but the driver does use it in
//      three places (docs/fv2-son-programme.md), so an autonomous core must carry bytes properly.
#pragma once

#include "scsp.h"
#include "scsp_core.h"

#include <cstdint>
#include <memory>
#include <vector>

// MAME's SCSP behind scsp::core. Owns the device; the board reaches the device directly only for this
// port's own extras (group gains, probes, the MADRS mirror), which are not the chip's business and are
// meant to be the last thing to go.
class scsp_mame final : public scsp::core {
public:
	scsp_mame(std::uint32_t clock, std::uint16_t *ram, std::uint32_t ram_bytes, scsp::sink &s);

	void reset() override;
	std::uint16_t read_reg(std::uint32_t word_offset) override;
	void write_reg(std::uint32_t word_offset, std::uint16_t data, std::uint16_t mem_mask) override;
	void run(std::uint32_t n, std::int32_t *out) override;
	void timer_fired(int which) override;
	void midi_in(std::uint8_t byte) override;
	std::uint64_t samples_made() const override { return m_now; }

	// The old class, for the port's extras during the changeover. Nothing in scsp_core.h needs it.
	scsp_device &device() { return *m_dev; }
	// Et en lecture seule, pour qui ne fait que RELIRE un de ces extras -- la trace relit le miroir MADRS
	// pour l'ecrire dans son en-tete (scsp_select.h).
	const scsp_device &device_const() const { return *m_dev; }

	// How many MIDI-out BITS the old core emitted that this adapter could not turn into bytes. Read it
	// rather than assume the link is unused: see note 3 above.
	std::uint64_t midi_out_bits_dropped() const { return m_bits_dropped; }

	// The scale run() reports on, so a caller never has to guess: +-131072, 18 bits signed.
	static constexpr std::int32_t PLEINE_ECHELLE = 131072;

private:
	std::unique_ptr<scsp_device> m_dev;
	scsp::sink &m_sink;
	std::uint64_t m_now = 0;             // samples made, and the answer to the old core's "what time is it"
	std::uint64_t m_bits_dropped = 0;
};
