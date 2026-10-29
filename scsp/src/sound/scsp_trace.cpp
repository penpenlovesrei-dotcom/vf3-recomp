#include "scsp_trace.h"

#include "scsp_select.h"

#include <cinttypes>
#include <cstring>

namespace scsp {

namespace {

// Beyond this, the trace does not start at a reset and the initial sweep would be a RAM dump.
constexpr unsigned MAX_NONZERO_WORDS = 4096;

} // namespace

recorder::recorder(core &inner, const char *fichier, unsigned chip, u32 clock,
                           const u16 *ram, u32 ram_bytes)
	: m_inner(inner)
{
	m_f = fopen(fichier, "w");
	if (!m_f) {
		fprintf(stderr, "scsp_trace: cannot write %s; the trace is abandoned\n", fichier);
		return;
	}
	// The initial sweep, and its guard: a trace must start at a reset, otherwise the RAM a replay would
	// rebuild is not the one the chip had.
	const u32 words = ram_bytes / 2;
	unsigned n = 0;
	for (u32 i = 0; i < words; i++) if (ram[i]) n++;
	if (n > MAX_NONZERO_WORDS) {
		fprintf(stderr, "scsp_trace: %u non-zero RAM words at the start (more than %u): this trace does not"
		                " begin at a reset, and a replay would not have the right RAM. Abandoned.\n",
		        n, MAX_NONZERO_WORDS);
		fclose(m_f);
		m_f = nullptr;
		return;
	}
	// THE MADRS MIRROR IN THE HEADER, and it is the only chip state that does not arrive through a call:
	// it is set before the trace starts, it CHANGES THE SOUND, and a replay that assumed it diverged from
	// the board with no way to attribute it (scsp_select.h). It is READ BACK from the chip rather than
	// taken as a parameter: that way there is nothing to remember to tell the recorder, which is the point.
	bool carried = false;
	const bool mirror = madrs_mirror_of(m_inner, &carried);
	if (!carried)
		fprintf(stderr, "scsp_trace: this core does not carry the MADRS mirror; the trace records the default"
		                " (true), which the replay will therefore set as well\n");
	fprintf(m_f, "# scsp-trace 3 chip=%u clock=%u ram=%u mirror=%u\n", chip, clock, ram_bytes,
	        unsigned(mirror));
	for (u32 i = 0; i < words; i++)
		if (ram[i]) fprintf(m_f, "d %x %x ffff\n", i, ram[i]);

	// The PCM alongside: enough to prove the trace is complete (scsp_trace.h). Its absence does not stop
	// the recording -- a trace without a reference is still replayable, it is just less demonstrable.
	const std::string pcm = std::string(fichier) + ".pcm";
	m_pcm = fopen(pcm.c_str(), "wb");
	if (!m_pcm)
		fprintf(stderr, "scsp_trace: %s does not open; the trace will have no PCM reference\n", pcm.c_str());
}

recorder::~recorder()
{
	if (m_f) fclose(m_f);
	if (m_pcm) fclose(m_pcm);
}

void recorder::ram_written(u32 word, u16 data, u16 mask)
{
	if (m_f) fprintf(m_f, "d %x %x %x\n", word, data, mask);
}

void recorder::reset()
{
	// No line: a reset mid-trace would change the RAM under the replay without its knowing. In any case
	// the board only rebuilds its cores at SoundBoard::reset, where the trace starts over.
	m_inner.reset();
}

u16 recorder::read_reg(u32 word)
{
	if (m_f) fprintf(m_f, "r %x\n", word);
	return m_inner.read_reg(word);
}

void recorder::write_reg(u32 word, u16 data, u16 mask)
{
	if (m_f) fprintf(m_f, "w %x %x %x\n", word, data, mask);
	m_inner.write_reg(word, data, mask);
}

void recorder::run(u32 n, s32 *out)
{
	if (m_f) fprintf(m_f, "s %u\n", n);
	m_inner.run(n, out);
	// AFTER the call, because what is kept is the result and not the request.
	if (m_pcm && n) fwrite(out, sizeof(s32), std::size_t(n) * 2, m_pcm);
}

void recorder::timer_fired(int which)
{
	if (m_f) fprintf(m_f, "t %d\n", which);
	m_inner.timer_fired(which);
}

void recorder::midi_in(u8 byte)
{
	if (m_f) fprintf(m_f, "m %x\n", byte);
	m_inner.midi_in(byte);
}

bool replay(const char *fichier, core &c, std::vector<u16> &ram, output &s, std::string *error)
{
	auto say = [error](const std::string &what) { if (error) *error = what; return false; };

	FILE *f = fopen(fichier, "r");
	if (!f) return say(std::string("trace not found: ") + fichier);

	char line[256];
	if (!fgets(line, sizeof line, f)) { fclose(f); return say("empty trace"); }
	unsigned chip = 0, clock = 0, ram_bytes = 0, version = 0, mirror = 1;
	const int fields = sscanf(line, "# scsp-trace %u chip=%u clock=%u ram=%u mirror=%u",
	                          &version, &chip, &clock, &ram_bytes, &mirror);
	// A HEADER IN THE OLD SPELLING IS REFUSED, NOT GUESSED AT, and that rule has already paid for itself
	// once. Version 1 did not state the MADRS mirror, which CHANGES THE SOUND; assuming it is precisely
	// what made a replay diverge with no way to say whether it was the core or the trace. Version 2 said
	// it, under French keys -- `puce=`, `horloge=`, `miroir=` -- which version 3 writes in English. The
	// sscanf above matches neither, so both land here with a sentence that says what to do rather than a
	// chip configured from zeros. A clear failure beats a false measurement.
	if (version == 1 || version == 2) {
		fclose(f);
		return say("version " + std::to_string(version) + " trace: version 1 does not state the MADRS"
		           " mirror, which changes the sound (scsp_select.h), and version 2 states it under the"
		           " old key names. Record it again with VF3_SCSP_TRACE; version 3 is what this build"
		           " reads.");
	}
	if (fields != 5 || version != 3) {
		fclose(f);
		return say("unknown header: " + std::string(line));
	}
	// The mirror BEFORE the first line: words 0x7C0-0x7FF do not go to the same place depending on it, and
	// the first of those writes comes early in the scene (sample 29 211, measured on 2026-10-28).
	if (!set_madrs_mirror_on(c, mirror != 0)) {
		fclose(f);
		return say("this core does not carry the MADRS mirror, which the trace asks for (scsp_select.h)");
	}
	if (ram_bytes / 2 != ram.size())
		return say("the trace says " + std::to_string(ram_bytes) + " bytes of RAM, the replay has "
		           + std::to_string(ram.size() * 2)), fclose(f), false;

	// run()'s buffer, grown on demand: nothing is allocated per sample.
	std::vector<s32> buffer;
	unsigned long lineno = 1;
	while (fgets(line, sizeof line, f)) {
		lineno++;
		const char k = line[0];
		if (k == '#' || k == '\n' || k == '\0') continue;
		unsigned a = 0, b = 0, d = 0;
		switch (k) {
		case 'd':
			if (sscanf(line + 1, "%x %x %x", &a, &b, &d) != 3) goto unreadable;
			if (a >= ram.size()) { fclose(f); return say("line " + std::to_string(lineno)
			                                             + ": RAM word out of bounds"); }
			ram[a] = u16((ram[a] & ~u16(d)) | (u16(b) & u16(d)));
			break;
		case 'w':
			if (sscanf(line + 1, "%x %x %x", &a, &b, &d) != 3) goto unreadable;
			c.write_reg(a, u16(b), u16(d));
			break;
		case 'r': {
			if (sscanf(line + 1, "%x", &a) != 1) goto unreadable;
			const u16 v = c.read_reg(a);
			s.reads.push_back({ c.samples_made(), a, v });
			break;
		}
		case 'm':
			if (sscanf(line + 1, "%x", &a) != 1) goto unreadable;
			c.midi_in(u8(a));
			break;
		case 't':
			if (sscanf(line + 1, "%u", &a) != 1) goto unreadable;
			c.timer_fired(int(a));
			break;
		case 's': {
			if (sscanf(line + 1, "%u", &a) != 1) goto unreadable;
			if (buffer.size() < std::size_t(a) * 2) buffer.resize(std::size_t(a) * 2);
			c.run(a, buffer.data());
			s.pcm.insert(s.pcm.end(), buffer.begin(), buffer.begin() + std::ptrdiff_t(a) * 2);
			break;
		}
		default:
		unreadable:
			fclose(f);
			return say("line " + std::to_string(lineno) + " unreadable: " + std::string(line));
		}
	}
	fclose(f);
	return true;
}

std::string compare(const output &a, const output &b)
{
	char t[256];

	// THE PCM FIRST, because it is the only stream in which every sample counts and it is the one that
	// locates all the rest: a sample index is the shared coordinate of the four other lists.
	if (a.pcm.size() != b.pcm.size()) {
		snprintf(t, sizeof t, "PCM: %zu values against %zu", a.pcm.size(), b.pcm.size());
		return t;
	}
	for (std::size_t i = 0; i < a.pcm.size(); i++)
		if (a.pcm[i] != b.pcm[i]) {
			snprintf(t, sizeof t, "PCM: sample %zu %s, %d against %d",
			         i / 2, (i & 1) ? "right" : "left", a.pcm[i], b.pcm[i]);
			return t;
		}

	// THE INTERRUPTS, which are the other half of the subject: they are how the 68000 learns the time, and
	// a single one too many or too few changes the rest of the driver's program -- hence the music, two
	// seconds later, in a way that no longer resembles its cause.
	if (a.irqs.size() != b.irqs.size()) {
		snprintf(t, sizeof t, "interrupts: %zu against %zu", a.irqs.size(), b.irqs.size());
		return t;
	}
	for (std::size_t i = 0; i < a.irqs.size(); i++) {
		const output::Irq &x = a.irqs[i], &y = b.irqs[i];
		if (x.smp != y.smp || x.level != y.level || x.state != y.state) {
			snprintf(t, sizeof t, "interrupt %zu: %s%d at sample %" PRIu64
			                      " against %s%d at %" PRIu64,
			         i, x.state ? "+" : "-", x.level, x.smp, y.state ? "+" : "-", y.level, y.smp);
			return t;
		}
	}

	if (a.armes.size() != b.armes.size()) {
		snprintf(t, sizeof t, "timer arms: %zu against %zu", a.armes.size(), b.armes.size());
		return t;
	}
	for (std::size_t i = 0; i < a.armes.size(); i++) {
		const output::Arm &x = a.armes[i], &y = b.armes[i];
		if (x.smp != y.smp || x.which != y.which || x.samples != y.samples
		    || x.periode != y.periode) {
			snprintf(t, sizeof t, "arm %zu: timer %d for %u (period %u) at sample %" PRIu64
			                      " against timer %d for %u (period %u) at %" PRIu64,
			         i, x.which, x.samples, x.periode, x.smp,
			         y.which, y.samples, y.periode, y.smp);
			return t;
		}
	}

	if (a.reads.size() != b.reads.size()) {
		snprintf(t, sizeof t, "register reads: %zu against %zu", a.reads.size(), b.reads.size());
		return t;
	}
	for (std::size_t i = 0; i < a.reads.size(); i++) {
		const output::Read &x = a.reads[i], &y = b.reads[i];
		if (x.word != y.word || x.value != y.value) {
			snprintf(t, sizeof t, "read %zu of word %03x: %04x against %04x (sample %" PRIu64 ")",
			         i, x.word, x.value, y.value, x.smp);
			return t;
		}
	}

	if (a.midis.size() != b.midis.size()) {
		snprintf(t, sizeof t, "outgoing MIDI bytes: %zu against %zu", a.midis.size(), b.midis.size());
		return t;
	}
	for (std::size_t i = 0; i < a.midis.size(); i++)
		if (a.midis[i].byte != b.midis[i].byte || a.midis[i].smp != b.midis[i].smp) {
			snprintf(t, sizeof t, "MIDI byte %zu: %02x at sample %" PRIu64 " against %02x at %" PRIu64,
			         i, a.midis[i].byte, a.midis[i].smp, b.midis[i].byte, b.midis[i].smp);
			return t;
		}

	return {};
}

} // namespace scsp
