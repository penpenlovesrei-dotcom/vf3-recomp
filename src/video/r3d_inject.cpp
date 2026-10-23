// See r3d_inject.h.
#include "r3d_inject.h"
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace video {
namespace {

// The three memories the Real3D reads, in words, and the texture sheet in
// texels. The sizes are the board's own, so an address that is in range for
// the game is in range here.
constexpr size_t POLY_WORDS = 0x100000;    // 4 MB of polygon RAM
constexpr size_t CULL_LO_WORDS = 0x100000; // 4 MB of low culling RAM
constexpr size_t CULL_HI_WORDS = 0x40000;  // 1 MB of high culling RAM
constexpr size_t TEX_TEXELS = 2048 * 2048; // two pages of 2048x1024

// Where vers_real3d.py puts each piece. These are the bases it reports, chosen
// above what the game occupies (0x58FC5, 0x79797 and 0x4DF0 when measured).
constexpr size_t POLY_BASE = 0x60000;
constexpr size_t CULL_LO_BASE = 0x80000;
constexpr size_t CULL_HI_BASE = 0x8000;

// This process is built for the window subsystem, where stderr goes nowhere.
// The note is written beside the scene instead, so the same message is there
// whichever build loaded it.
std::string g_dir;

std::string log_path()
{
	// Toujours une destination : sans VF3_SCENE il n'y a pas de dossier de
	// scene, et c'est precisement le cas qu'on cherche a voir.
	if (!g_dir.empty())
		return g_dir + "injection.log";
	const char *t = getenv("TEMP");
	return std::string(t ? t : ".") + "/vf3_inject.log";
}

void note(const char *fmt, ...)
{
	FILE *f = fopen(log_path().c_str(), "a");
	if (!f)
		return;
	va_list ap;
	va_start(ap, fmt);
	vfprintf(f, fmt, ap);
	va_end(ap);
	fputc(10, f);
	fclose(f);
}

bool read_into(const std::string &path, void *dst, size_t room_bytes, size_t *got)
{
	FILE *f = fopen(path.c_str(), "rb");
	if (!f) {
		note("missing: %s", path.c_str());
		return false;
	}
	fseek(f, 0, SEEK_END);
	long n = ftell(f);
	fseek(f, 0, SEEK_SET);
	if (n <= 0 || size_t(n) > room_bytes) {
		note("%s is %ld bytes, room for %zu", path.c_str(), n, room_bytes);
		fclose(f);
		return false;
	}
	size_t read = fread(dst, 1, size_t(n), f);
	fclose(f);
	if (got)
		*got = read;
	return read == size_t(n);
}

Injected load()
{
	Injected in;
	const char *dir = getenv("VF3_SCENE");
	note("load() appele ; VF3_SCENE = %s", dir ? (*dir ? dir : "(vide)") : "(absent)");
	if (!dir || !*dir)
		return in;
	std::string d(dir);
	if (d.back() != '/' && d.back() != '\\')
		d += '/';
	g_dir = d;
	note("VF3_SCENE = %s", d.c_str());

	in.poly.assign(POLY_WORDS, 0);
	in.cull_lo.assign(CULL_LO_WORDS, 0);
	in.cull_hi.assign(CULL_HI_WORDS, 0);
	in.tex.assign(TEX_TEXELS, 0);

	size_t n_poly = 0, n_lo = 0, n_hi = 0, n_tex = 0;
	bool ok = read_into(d + "polygones.bin", in.poly.data() + POLY_BASE,
			    (POLY_WORDS - POLY_BASE) * 4, &n_poly);
	ok = read_into(d + "noeuds.bin", in.cull_lo.data() + CULL_LO_BASE,
		       (CULL_LO_WORDS - CULL_LO_BASE) * 4, &n_lo) && ok;
	ok = read_into(d + "culling_haut.bin", in.cull_hi.data() + CULL_HI_BASE,
		       (CULL_HI_WORDS - CULL_HI_BASE) * 4, &n_hi) && ok;
	// The sheet is optional: without it the geometry still shows, wearing
	// whatever the game last uploaded.
	read_into(d + "planche.bin", in.tex.data(), 2048 * 1024 * 2, &n_tex);

	if (!ok) {
		note("a piece is missing, the game's own scene is kept");
		in = Injected();
		return in;
	}
	in.ok = true;
	note("loaded: %zu bytes of polygons at word %#zx, %zu of nodes at %#zx, %zu of"
	     " viewport at %#zx, %zu of texels",
	     n_poly, POLY_BASE, n_lo, CULL_LO_BASE, n_hi, CULL_HI_BASE, n_tex);

	// Ce qui bouge, s'il y a lieu. Le fichier est facultatif : sans lui, la scene est simplement fixe.
	if (FILE *f = fopen((d + "anime.txt").c_str(), "r")) {
		char ligne[256];
		while (fgets(ligne, sizeof ligne, f)) {
			if (ligne[0] == '#' || ligne[0] == '\n') continue;
			Anime a;
			if (sscanf(ligne, "%d %d %f %f %f %d %f %d", &a.tex_x, &a.tex_y, &a.d[0], &a.d[1], &a.d[2],
			           &a.periode, &a.pas, &a.copies) >= 6
			    && a.tex_x >= 0 && a.periode > 0)
				in.animes.push_back(a);
		}
		fclose(f);
		for (const Anime &a : in.animes)
			note("anime : la texture en (%d, %d) se deplace de (%g, %g, %g) par image, sur %d,"
			     " en %d copie(s) espacees de %g",
			     a.tex_x, a.tex_y, a.d[0], a.d[1], a.d[2], a.periode, 2 * a.copies + 1, a.pas);
	}
	return in;
}

} // namespace

void injected_note(int models)
{
	static bool once = false;
	if (once)
		return;
	if (models < 0) {
		static bool dit = false;
		if (dit)
			return;
		dit = true;
		once = false;
		note("SceneBuilder::build() a ete atteint");
		return;
	}
	once = true;
	note("the injected viewport yielded %d model(s)", models);
}

const Injected *injected_scene()
{
	static const Injected in = load();
	static long appels = 0;
	if (++appels == 1 || appels == 100)
		note("injected_scene() appele %ld fois, resultat %s", appels,
		     in.ok ? "une scene" : "rien");
	return in.ok ? &in : nullptr;
}

} // namespace video
