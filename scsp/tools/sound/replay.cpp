// replay: replays a trace of SCSP accesses through one or two cores, and names the first divergence.
//
// Point 2 of step 2 of docs/scsp_migration_plan.md, the last prerequisite: "the same sequence of accesses
// replayed through both [...] and the comparison of the PCM, the observable state, the interrupts and the
// timers -- not just the WAV."
//
//   tools/sound/replay <trace>                 one core, and what it produced
//   tools/sound/replay <trace> <core> <core>   two cores, and their first divergence
//   tools/sound/replay <trace> --pcm <file>    writes the raw PCM (interleaved s32) for external comparison
//   tools/sound/replay <trace> --against <pcm>  against the PCM reference kept at recording time
//
// THE FIRST CHECK TO PASS IS "mame against mame", and it is not a formality: it proves the replay is
// DETERMINISTIC, that is, that the trace really carries everything the chip depends on. If two replays of
// the same core over the same trace diverge, then some state is coming in through a door the trace does
// not record -- and that has to be found before comparing anything else.
#include "../../src/sound/scsp_select.h"
#include "../../src/sound/scsp_trace.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {

constexpr std::uint32_t RAM_BYTES = 0x100000;   // 1 MB, like one chip on the board

// One complete replay: its own RAM, its own core, its own output.
bool one_replay(const char *trace, const char *name, scsp::output &s, std::string &error)
{
	std::vector<std::uint16_t> ram(RAM_BYTES / 2, 0);
	scsp::puits_notant snk(s);
	std::unique_ptr<scsp::core> c = scsp::make(name, 45158400 / 2, ram.data(), RAM_BYTES, snk);
	if (!c) { error = std::string("unknown core: ") + name; return false; }
	snk.suit(c.get());
	c->reset();
	return scsp::replay(trace, *c, ram, s, &error);
}

void summary(const char *name, const scsp::output &s)
{
	printf("  %-6s %zu samples, %zu interrupts, %zu arms, %zu reads, %zu MIDI bytes\n",
	       name, s.pcm.size() / 2, s.irqs.size(), s.armes.size(), s.reads.size(), s.midis.size());
}

} // namespace

int main(int argc, char **argv)
{
	if (argc < 2) {
		printf("replay: replays a trace of SCSP accesses (src/sound/scsp_trace.h for the format)\n\n"
		       "  replay <trace>                      one core, and what it produced\n"
		       "  replay <trace> <core> <core>        two cores, and their first divergence\n"
		       "  replay <trace> --pcm <file>         writes the raw PCM (interleaved s32)\n"
		       "  replay <trace> --against <pcm>       against the reference kept at recording time\n\n"
		       "known cores: %s\n\n"
		       "A trace is obtained with VF3_SCSP_TRACE=f (src/sound/soundboard.cpp): it begins at the\n"
		       "board's reset, and carries the RAM writes without which a replay has no sample to play\n"
		       "at all.\n", scsp::known_names());
		return 2;
	}

	const char *trace = argv[1];

	// --- one replay, with or without the PCM written out
	if (argc == 2 || (argc == 4 && !strcmp(argv[2], "--pcm"))) {
		scsp::output s;
		std::string error;
		if (!one_replay(trace, "mame", s, error)) {
			fprintf(stderr, "replay: %s\n", error.c_str());
			return 1;
		}
		printf("replay of %s by the mame core:\n", trace);
		summary("mame", s);
		if (argc == 4) {
			FILE *f = fopen(argv[3], "wb");
			if (!f) { fprintf(stderr, "replay: cannot write %s\n", argv[3]); return 1; }
			fwrite(s.pcm.data(), sizeof(std::int32_t), s.pcm.size(), f);
			fclose(f);
			printf("  PCM written to %s (%zu bytes)\n", argv[3], s.pcm.size() * sizeof(std::int32_t));
		}
		return 0;
	}

	// --- one replay against the PCM reference the recording kept
	// THIS IS THE CHECK THAT VALIDATES THE TRACE ITSELF, and it has to pass before a comparison between
	// two cores means anything: if the replay does not find the PCM the board produced, then the trace
	// does not carry everything the chip depends on, and a divergence between two cores would become
	// impossible to attribute.
	if (argc == 4 && !strcmp(argv[2], "--against")) {
		scsp::output s;
		std::string error;
		if (!one_replay(trace, "mame", s, error)) { fprintf(stderr, "replay: %s\n", error.c_str()); return 1; }
		FILE *f = fopen(argv[3], "rb");
		if (!f) { fprintf(stderr, "replay: %s does not open\n", argv[3]); return 1; }
		std::vector<std::int32_t> ref;
		std::int32_t buffer[4096];
		for (std::size_t lu; (lu = fread(buffer, sizeof(std::int32_t), 4096, f)) > 0; )
			ref.insert(ref.end(), buffer, buffer + lu);
		fclose(f);
		printf("replay of %s against the reference %s:\n", trace, argv[3]);
		summary("replay", s);
		printf("  %-6s %zu samples\n", "ref", ref.size() / 2);
		if (ref.size() != s.pcm.size()) {
			printf("\nDIVERGENCE -- %zu samples replayed against %zu recorded\n",
			       s.pcm.size() / 2, ref.size() / 2);
			return 1;
		}
		for (std::size_t i = 0; i < ref.size(); i++)
			if (ref[i] != s.pcm[i]) {
				printf("\nDIVERGENCE -- sample %zu %s: replay %d, recorded %d\n",
				       i / 2, (i & 1) ? "right" : "left", s.pcm[i], ref[i]);
				return 1;
			}
		printf("\nBYTE-IDENTICAL over %zu samples: the trace carries everything the chip depends on.\n",
		       ref.size() / 2);
		return 0;
	}

	// --- two replays, and the comparison
	if (argc == 4) {
		scsp::output a, b;
		std::string error;
		if (!one_replay(trace, argv[2], a, error)) { fprintf(stderr, "replay: %s\n", error.c_str()); return 1; }
		if (!one_replay(trace, argv[3], b, error)) { fprintf(stderr, "replay: %s\n", error.c_str()); return 1; }
		printf("replay of %s through two cores:\n", trace);
		summary(argv[2], a);
		summary(argv[3], b);
		const std::string d = scsp::compare(a, b);
		if (d.empty()) {
			printf("\nIDENTICAL: the PCM, the interrupts, the timer arms, the register reads and the\n"
			       "outgoing MIDI agree, sample by sample.\n");
			return 0;
		}
		printf("\nDIVERGENCE -- %s\n", d.c_str());
		return 1;
	}

	fprintf(stderr, "replay: unexpected arguments; run with no argument for help\n");
	return 2;
}
