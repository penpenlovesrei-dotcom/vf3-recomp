// scsp_trace.h -- a trace of SCSP accesses: recording one, replaying one, comparing two replays.
//
// Point 2 of step 2 of docs/scsp_migration_plan.md, the last prerequisite that was missing: "both cores
// in the same binary, the same sequence of accesses replayed through both [...] and the comparison of the
// PCM, the observable state, the interrupts and the timers -- not just the WAV."
//
// WHY THE EXISTING HARNESS IS NOT ENOUGH. tools/son/comparer-son.cmd compares the FINAL MIX, two WAVs. It
// has proved its worth -- it found one missing pair of samples out of 3 219 865 -- but it only sees what
// comes out of the speaker. A rewritten block can be wrong in a way the mix hides: an interrupt one sample
// too early, a timer armed for the wrong duration, a register read that returns something else. The 68000
// then reacts differently, and the divergence surfaces three arenas later in a shape that has nothing left
// to do with its cause. Here we compare what the CHIP did, not what we hear.
//
// AND VF3_SCSP_WLOG IS NOT ENOUGH EITHER, although the plan offers it as a starting point. It records the
// frame, not the sample -- so the instant of a write within the frame is lost, and that is precisely what
// decides where the interrupts fall. It records only chip 0, and neither the reads, nor incoming MIDI, nor
// the timers that fire, nor the writes to sound RAM -- without which a replay has no sample to play at
// all. The format below is therefore new, and the old log stays what it is: a log to read.
//
// THE FORMAT, text, one line per event, so that it reads and diffs like the project's other logs. The
// events are in the order the board made them, which is the whole point:
//
//     # scsp-trace 2 puce=N horloge=H ram=B miroir=0|1
//     d <word> <data> <mask>    a write to sound RAM (word = index of the WORD in RAM)
//     w <word> <data> <mask>    write_reg
//     r <word>                  read_reg (a read has side effects, and its value compares)
//     m <byte>                  midi_in
//     t <which>                 timer_fired
//     s <n>                     run(n): n samples
//
// AND THE RECORDER ALSO KEEPS THE PCM, in a second file `<trace>.pcm` (interleaved s32). This is not a
// convenience: it is the only way to PROVE that a trace carries everything the chip depends on. Replaying
// the trace and getting the same PCM byte for byte establishes that no state comes in through a door the
// trace does not record. Without that check, a replay that diverges leaves two explanations -- the core is
// different, or the trace is incomplete -- and there is no way to pick.
//
// `miroir` ARRIVED LATER, AND WHY HAS TO BE SAID, because it is the kind of thing nobody works out twice.
// The MADRS mirror is not an ACCESS: it is set once, before the trace starts, it does not go through
// scsp::core (it is the game's business, not the chip's), and IT CHANGES THE SOUND -- true for Virtua
// Fighter 3, false for Fighting Vipers 2. So a trace that does not state it does NOT carry everything the
// chip depends on, and the check below passed all the same: over 300 frames, because the first write that
// tells them apart comes later. Measured on 2026-10-28, over the whole scene: the replay diverged at
// sample 772 067, and the same scene recorded with the mirror forced replayed byte-identical
// (docs/scsp_validation.md §3 c). The recorder READS it back from the chip, the replay SETS it, through
// the single door of scsp_select.h -- and a version 1 trace is REFUSED rather than guessed at.
//
// THE LESSON, for next time: state that does not arrive through a call does not go into a trace of calls.
// The port's three other extras (per-group gains, energy probe, key-on log) do not change the sound at the
// default setting, and that is the only reason they are not in the header; if one of them started to
// count, it would have to go in as well.
//
// A TRACE STARTS AT RESET, and the recorder checks that rather than assuming it: on startup it sweeps RAM
// and writes one `d` per non-zero word. After a reset there are eight of them -- the 68000's vectors, which
// the board copies there. In the middle of a game there would be hundreds of thousands, and the recorder
// then refuses with a clear message instead of writing a 500 MB file.
#pragma once

#include "scsp_core.h"

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace scsp {

// What a core PRODUCED during a replay: everything that leaves the chip, timestamped in samples.
struct sortie {
	struct Irq  { std::uint64_t ech; int niveau; bool etat; };
	struct Midi { std::uint64_t ech; u8 octet; };
	struct Arme { std::uint64_t ech; int lequel; u32 echantillons, periode; };
	struct Lue  { std::uint64_t ech; u32 mot; u16 valeur; };

	std::vector<Irq>  irqs;
	std::vector<Midi> midis;
	std::vector<Arme> armes;
	std::vector<Lue>  lues;     // the values returned by read_reg
	std::vector<s32>  pcm;      // interleaved left/right, on the 18-bit scale
};

// The sink that records everything, for a replay. It asks the core for the instant, so it must be given it.
class puits_notant final : public sink {
public:
	explicit puits_notant(sortie &s) : m_s(s) {}
	void suit(const core *c) { m_c = c; }

	void irq(int niveau, bool etat) override { m_s.irqs.push_back({ ou(), niveau, etat }); }
	void midi_out(u8 octet) override { m_s.midis.push_back({ ou(), octet }); }
	void timer(int lequel, u32 ech, u32 per) override { m_s.armes.push_back({ ou(), lequel, ech, per }); }

private:
	std::uint64_t ou() const { return m_c ? m_c->samples_made() : 0; }
	sortie &m_s;
	const core *m_c = nullptr;
};

// THE RECORDER: a core that passes everything on to another one and writes the trace on the way through.
//
// It is a decorator and not a change to the board, for a substantive reason: if the recording lived in
// soundboard.cpp it would have to be placed at eight sites, and the ninth call added one day would escape
// the trace with nothing to say so. Here there is only one door.
class enregistreur final : public core {
public:
	// `dessous` must outlive the recorder. `ram` is used for the initial sweep. The PCM goes into
	// `<file>.pcm`.
	enregistreur(core &dessous, const char *fichier, unsigned puce, u32 horloge,
	             const u16 *ram, u32 ram_octets);
	~enregistreur() override;

	// True if the file is open and the initial sweep was accepted. False: the trace is abandoned and a
	// message has already gone to stderr -- the recorder then merely passes the calls through.
	bool bon() const { return m_f != nullptr; }

	// A write to sound RAM, which the board makes without going through the core.
	void ram_ecrite(u32 mot, u16 data, u16 masque);

	void reset() override;
	u16  read_reg(u32 mot) override;
	void write_reg(u32 mot, u16 data, u16 masque) override;
	void run(u32 n, s32 *out) override;
	void timer_fired(int lequel) override;
	void midi_in(u8 octet) override;
	std::uint64_t samples_made() const override { return m_dessous.samples_made(); }

private:
	core &m_dessous;
	FILE *m_f = nullptr;
	FILE *m_pcm = nullptr;
};

// THE REPLAYER: reads a trace, drives `c`, fills `s`.
//
// `ram` is the RAM `c` was given at construction: the `d` lines go into it. `erreur` receives the reason
// for a failure (file missing, unknown header, unreadable line); nothing is guessed silently.
bool rejoue(const char *fichier, core &c, std::vector<u16> &ram, sortie &s, std::string *erreur);

// THE COMPARISON: returns an empty string if the two outputs are identical, otherwise THE FIRST
// divergence, named and located. Only one, because after the first the two cores are no longer telling the
// same story and the rest teaches nothing.
std::string compare(const sortie &a, const sortie &b);

} // namespace scsp
