// Where the sound board's time goes, in TSC cycles, so a measurement can name a part instead of
// the whole. Off unless VF3_SND_PROF=1 is in the environment; then every scope costs two rdtsc
// (about 20 cycles), and the report prints what that comes to per frame, so the overhead is
// never silent.
//
// The scopes nest, which is the whole difficulty: the samples are not made in a phase of their
// own. The 68000 triggers them from inside its own slices, every time it touches a chip (MAME's
// stream brought up to now, scsp_device::update), and the rest are made by the two update()
// calls that close the frame. So SCSP counts the sample making wherever it happens, FLUSH counts
// the frame-closing pair, and the report takes the nested time back out to give each part its
// own share:
//     loop+timers = RUN - M68K - MIX - FLUSH
//     68000       = M68K - (SCSP - FLUSH)      the 68000 without the samples it triggered
//     voices      = SCSP - DSP
//     dsp         = DSP
//     mix         = MIX
#pragma once
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__)
#include <x86intrin.h>
#define SNDPROF_TSC 1
#endif

namespace sndprof {

enum Part { RUN, M68K, SCSP, DSP, MIX, FLUSH, N_PARTS };

// The probe only turns on for `1` exactly, and it SAYS SO when the variable is set to anything else: a
// probe that produces nothing without saying why costs half a day to whoever goes looking for it.
inline const bool on = [] {
	const char *e = getenv("VF3_SND_PROF");
	if (e && *e && !(e[0] == '1' && !e[1]))
		fprintf(stderr, "sndprof: VF3_SND_PROF=\"%s\" turns nothing on -- the probe only knows \"1\"\n", e);
	return e && e[0] == '1' && !e[1];
}();
inline uint64_t acc[N_PARTS] = {};
inline uint64_t hits[N_PARTS] = {};
// steps the effects DSP actually ran (its program's length, per sample per chip): what the dsp
// share is spread over, added once a sample so the loop itself is untouched
inline uint64_t dsp_steps = 0;

inline uint64_t tick()
{
#ifdef SNDPROF_TSC
	return __rdtsc();
#else
	return 0;
#endif
}

struct Scope
{
	const Part p;
	const uint64_t t0;
	explicit Scope(Part part) : p(part), t0(on ? tick() : 0) {}
	~Scope() { if (on) { acc[p] += tick() - t0; ++hits[p]; } }
	Scope(const Scope &) = delete;
	Scope &operator=(const Scope &) = delete;
};

// `out` says which stream the report lands on, because it is not the same on both sides: the host sends
// everything to stderr (log.txt, and tools/son/mesurer-hote.cmd looks for the line there), while vf3run
// prints its `prof` line on stdout -- and a redirection that keeps one while losing the other has already
// made it look as though the probe produced nothing.
inline void report(const char *what, int frames, FILE *out = stderr)
{
	if (!on || frames <= 0) return;
	// Nothing counted at all: the sound board did not run once over these frames. Say so, because a line
	// of zeros reads like a measurement and is not one -- and because it is the normal case for vf3run
	// without --wav, which then never calls SoundBoard::run().
	if (!hits[RUN]) {
		fprintf(out, "sndprof %s: nothing measured -- the sound board did not run over these %d frames"
		             " (vf3run only runs it with --wav)\n", what, frames);
		for (int i = 0; i < N_PARTS; i++) acc[i] = hits[i] = 0;
		dsp_steps = 0;
		return;
	}
	const double r = double(acc[RUN]);
	const int64_t own[5] = {
		int64_t(acc[RUN]) - int64_t(acc[M68K]) - int64_t(acc[MIX]) - int64_t(acc[FLUSH]),
		int64_t(acc[M68K]) - (int64_t(acc[SCSP]) - int64_t(acc[FLUSH])),
		int64_t(acc[SCSP]) - int64_t(acc[DSP]),
		int64_t(acc[DSP]),
		int64_t(acc[MIX]),
	};
	static const char *const own_names[5] = { "loop+timers", "68000", "voices", "dsp", "mix" };
	uint64_t n_scopes = 0;
	for (int i = 0; i < N_PARTS; i++) n_scopes += hits[i];
	fprintf(out, "sndprof %s: %.0f kcycles a frame, own shares:", what, r / frames / 1000.0);
	for (int i = 0; i < 5; i++)
		fprintf(out, " %s %.1f%%", own_names[i], r > 0 ? 100.0 * double(own[i]) / r : 0.0);
	fprintf(out, " (%llu scopes a frame, about %.0f kcycles of rdtsc in them; %llu dsp steps a frame, %.1f cycles each)\n",
	        (unsigned long long)(n_scopes / unsigned(frames)), 20.0 * double(n_scopes) / frames / 1000.0,
	        (unsigned long long)(dsp_steps / unsigned(frames)), dsp_steps ? double(acc[DSP]) / double(dsp_steps) : 0.0);
	for (int i = 0; i < N_PARTS; i++) acc[i] = hits[i] = 0;
	dsp_steps = 0;
}

} // namespace sndprof
