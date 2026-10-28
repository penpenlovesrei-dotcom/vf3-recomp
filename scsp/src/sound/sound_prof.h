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

// La sonde ne s'allume que sur `1` exactement, et elle le DIT quand la variable est posee autrement :
// une sonde qui ne sort rien sans dire pourquoi coute une demi-journee a qui la cherche.
inline const bool on = [] {
	const char *e = getenv("VF3_SND_PROF");
	if (e && *e && !(e[0] == '1' && !e[1]))
		fprintf(stderr, "sndprof: VF3_SND_PROF=\"%s\" n'allume rien -- la sonde ne connait que \"1\"\n", e);
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

// `out` dit sur quelle sortie le rapport tombe, parce que ce n'est pas la meme des deux cotes : l'hote
// envoie tout sur stderr (log.txt, et tools/son/mesurer-hote.cmd y cherche la ligne), tandis que vf3run
// imprime sa ligne `prof` sur stdout -- et une redirection qui garde l'une en perdant l'autre a deja fait
// croire que la sonde ne sortait rien.
inline void report(const char *what, int frames, FILE *out = stderr)
{
	if (!on || frames <= 0) return;
	// Rien compte du tout : la carte son n'a pas tourne une seule fois sur ces images. Le dire, parce
	// qu'une ligne de zeros se lit comme une mesure et n'en est pas une -- et que c'est le cas normal
	// de vf3run sans --wav, qui n'appelle alors jamais SoundBoard::run().
	if (!hits[RUN]) {
		fprintf(out, "sndprof %s: rien mesure -- la carte son n'a pas tourne sur ces %d images"
		             " (vf3run ne la fait tourner qu'avec --wav)\n", what, frames);
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
