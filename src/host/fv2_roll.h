// THE PORT'S OWN STAFF ROLL: the Dreamcast's credits, rising instead of standing still.
//
// WHY IT IS THE PORT'S AND NOT THE GAME'S. Frederic finished the arcade ladder the 2026-10-29 and asked for
// the ending to be remade: "il y a toujours en arriere plan le logo du jeu en couleurs inversees [...] est
// il possible de faire defiler les textes du bas vers le haut ai lieu qu'ils soient fixes ?". The game draws
// its credits as PAGES -- one placed, held 140 frames, replaced -- and the hardware can only scroll a whole
// tile plane, logo and all. So the port takes the screen for the length of the ending and draws the roll
// itself: its own ground, and the pages climbing it.
//
// WHERE THE PAGES COME FROM, and every number here was read out of the Dreamcast's own code rather than
// guessed (docs/fv2-dreamcast-ending.md): the facade resource 0x8C0E5BC8, its pattern list 0x8C0E5808, and
// the eleven rectangles the roll cuts out of three 512 x 512 sheets. Frederic's HD art holds the same three
// sheets at four times the size -- matched by signature, 11.8 against 86 for the next candidate -- so the
// rectangles were cut at that scale into assets/menu-console/ending-text.
//
// THE PROPORTIONS ARE SEGA'S. A page is given here in the pixels of the screen it was laid out on, the
// Dreamcast's 640 x 480, and drawn at the window's own scale. The Programmers' six names are two patterns,
// because the sheet's right edge cuts them in half: the second goes 208 to the right of the first and 24
// lower, which is the first's width and the height of the heading -- again the game's own numbers.
//
// THE SPEED IS OURS. The Dreamcast scrolls nothing: its pages are placed and held. VF3_ENDING_SCROLL sets
// it, in those same 640 x 480 pixels a second, so it can be tried without a build.
#pragma once
#include <SDL3/SDL.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include "folder_name.h"
#include "png.h"

struct fv2_roll
{
	// The screen the Dreamcast laid this out on. Everything below is in its pixels.
	static constexpr float DC_W = 640.0f, DC_H = 480.0f;
	// BETWEEN TWO PAGES, and it is wide because the illustration goes in it. On the disc the pictures and
	// the credits never share the screen: scenes 4 and 6 are the roll, the other eleven are a picture each
	// (docs/fv2-dreamcast-ending.md). So here a page passes, then a picture, then the next page.
	// BETWEEN TWO PAGES. The text and the picture SHARE the screen -- Frederic sent a capture of the disc's
	// own ending the 2026-10-30 and there it is: the credits climbing the left of the screen, the fighter
	// standing the whole height of the right. So the gap is short, and a picture is simply up while its
	// place in the column crosses.
	static constexpr float GAP = 160.0f;
	// PIXELS A SECOND, and the number is the ending's own length. The game's ending runs about 6300 frames,
	// which is 110 seconds; the column is 2272 pixels and has a screen to cross on either side, so 30 a
	// second fills the ending almost exactly (92 s). VF3_ENDING_SCROLL tries another without a build.
	static constexpr float SPEED = 39.0f;
	static constexpr float HD = 4.0f;         // the art is four times the sheets it was cut from

	struct box { int x = 0, y = 0, w = 0, h = 0; };   // what a picture covers in its file
	struct piece {
		SDL_Texture *t = nullptr;
		float dx = 0, dy = 0, w = 0, h = 0;   // where it sits in its page, and its size
	};
	struct page {
		std::vector<piece> parts;
		float w = 0, h = 0;
	};

	// THE COLUMN IS A LIST OF SLOTS, one to a page and one to a picture, taken in turn: page, picture, page,
	// picture ... and when the pages run out -- there are eight of them and Frederic chose fifteen pictures
	// the 2026-10-30 -- the pictures go on, one after another, each with the screen to itself.
	struct slot { int page = -1, face = -1; float h = 0; };
	std::vector<slot> column;
	std::vector<page> pages;
	SDL_Texture *ground = nullptr;
	int ground_w = 0, ground_h = 0;
	// ---- AND THE ILLUSTRATIONS, one a page, coming in from the side ----
	//
	// Frederic, the 2026-10-30: "ajoute les illustrations des personnages a l'ending version dreamcast".
	// They are his own eight, chosen and ordered out of a rip of the disc, in menu-console/ending (the
	// folder fv2_ending.h already reads). One belongs to each page of credits, which is how the disc pairs
	// them: a page of names and a picture of the fighters those names drew.
	//
	// THE SLIDE IS SEGA'S OWN, read out of its script (docs/fv2-dreamcast-ending.md): a picture starts off
	// the screen and comes in at 40 pixels a frame for fifteen frames -- 600 pixels, a quarter of a second
	// -- and the side alternates, 620 to 20 for one and -280 to 320 for the next. Its size is the
	// Dreamcast's too: the drawer lays two tiles of 128 side by side, so 256 by 256.
	//
	// ONE AT A TIME IN MEMORY. Eight of 2048 square would be a hundred and thirty megabytes of texture for
	// the one on screen, which is the reason fv2_ending.h gives for holding only one; the same here.
	// THE PICTURES TAKE THE WHOLE HEIGHT, which is the capture again, and they come in from the LEFT, stop
	// on the RIGHT and leave by the right -- "les illustration arrivent bien de la gauche pour s'arreter a
	// droite, puis ne disparaissent pas mais sortent de l'ecran par la droite".
	static constexpr float FACE = DC_H;       // the picture's height on screen: all of it
	static constexpr float FACE_RIGHT = 6.0f;   // where its right edge rests, from the screen's right
	static constexpr float SLIDE = 600.0f;    // 40 pixels a frame for fifteen
	// The slide is measured in the column's own travel rather than in seconds, the column no longer having
	// a speed of its own: 26 pixels of it, which at any of the endings' lengths is about a quarter second.
	static constexpr float SLIDE_PX = 26.0f;
	static constexpr float TEXT_LEFT = 40.0f;    // the column is left-aligned, a little clear of the edge
	// THEY ARE ALL READ AT THE START, and that is the answer to the stutter: reading a picture of 2560
	// square and making its texture took a frame of its own, every time one came in. They are halved on the
	// way in to a thousand rows -- more than any window shows of them -- so the fifteen together are about
	// sixty megabytes instead of four hundred, and nothing is read while the roll runs.
	static constexpr int FACE_TALL = 1024;
	struct art {
		SDL_Texture *t = nullptr;
		box ink;
		float w = 0, h = 0;                   // what it is drawn at, in the DC's pixels
	};
	std::vector<art> art_of;
	int face_at = -1;
	bool tried = false;
	float speed = SPEED;
	float total = 0;                          // the whole column's height, in the DC's pixels

	bool ready() const { return !pages.empty(); }
	// When the last page has left the top: the roll is over and the screen can go back to the game.
	bool over(double progress) const { return progress >= 1.0; }

	void drop()
	{
		for (page &p : pages)
			for (piece &q : p.parts)
				if (q.t) { SDL_DestroyTexture(q.t); q.t = nullptr; }
		pages.clear();
		if (ground) { SDL_DestroyTexture(ground); ground = nullptr; }
		for (art &a : art_of)
			if (a.t) { SDL_DestroyTexture(a.t); a.t = nullptr; }
		art_of.clear();
		face_at = -1;
		tried = false;
		total = 0;
	}

	// One PNG, as a texture, with its size in the DC's own pixels (the art being four times that).
	// `tall` is the most rows the texture may keep. The art is 2048 to 2560 square and is drawn at most the
	// height of the window, so the rest is paid for and never seen: at 2560 a picture is 26 MB of texture,
	// and reading it is what Frederic feels -- "l'apparition des illustrations fait ralentir le jeu un court
	// instant". Halved until it fits, which keeps the box filter exact.
	static SDL_Texture *read(SDL_Renderer *ren, const std::string &path, float &w, float &h, FILE *log,
							 box *ink = nullptr, int tall = 0)
	{
		std::vector<uint32_t> px;
		int pw = 0, ph = 0;
		std::string why;
		if (!read_png(path.c_str(), px, pw, ph, why)) {
			if (log) fprintf(log, "the console's roll: %s %s\n", path.c_str(), why.c_str());
			return nullptr;
		}
		int shrink = 1;
		while (tall > 0 && ph / shrink > tall && (pw / shrink) % 2 == 0 && (ph / shrink) % 2 == 0) shrink *= 2;
		if (shrink > 1) {
			const int nw = pw / shrink, nh = ph / shrink;
			std::vector<uint32_t> small(size_t(nw) * size_t(nh));
			for (int yy = 0; yy < nh; yy++)
				for (int xx = 0; xx < nw; xx++) {
					uint32_t r = 0, g = 0, b = 0, a = 0;
					for (int j = 0; j < shrink; j++)
						for (int i = 0; i < shrink; i++) {
							const uint32_t c = px[size_t(yy * shrink + j) * size_t(pw) + size_t(xx * shrink + i)];
							const uint32_t ca = c >> 24;
							r += (c & 0xFF) * ca; g += ((c >> 8) & 0xFF) * ca; b += ((c >> 16) & 0xFF) * ca;
							a += ca;
						}
					// COLOURS AVERAGED WEIGHTED BY ALPHA, or the transparent border's black bleeds into the
					// figure's edge and leaves it rimmed.
					const uint32_t aa = a / uint32_t(shrink * shrink);
					const uint32_t rr = a ? r / a : 0, gg = a ? g / a : 0, bb = a ? b / a : 0;
					small[size_t(yy) * size_t(nw) + size_t(xx)] = (aa << 24) | (bb << 16) | (gg << 8) | rr;
				}
			px.swap(small);
			pw = nw; ph = nh;
		}
		SDL_Texture *t = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STATIC, pw, ph);
		if (!t) {
			if (log) fprintf(log, "the console's roll: no texture for %s (%s)\n", path.c_str(), SDL_GetError());
			return nullptr;
		}
		SDL_UpdateTexture(t, nullptr, px.data(), pw * 4);
		SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
		SDL_SetTextureScaleMode(t, SDL_SCALEMODE_LINEAR);
		if (ink) {
			// what the picture actually covers: the first and last row and column with anything on them
			int x0 = pw, y0 = ph, x1 = -1, y1 = -1;
			for (int yy = 0; yy < ph; yy++)
				for (int xx = 0; xx < pw; xx++)
					if (px[size_t(yy) * size_t(pw) + size_t(xx)] >> 24) {
						if (xx < x0) x0 = xx;
						if (xx > x1) x1 = xx;
						if (yy < y0) y0 = yy;
						if (yy > y1) y1 = yy;
					}
			if (x1 >= x0 && y1 >= y0) *ink = { x0, y0, x1 - x0 + 1, y1 - y0 + 1 };
			else *ink = { 0, 0, pw, ph };
		}
		w = float(pw * shrink) / HD;
		h = float(ph * shrink) / HD;
		return t;
	}

	void load(SDL_Renderer *ren, const std::string &assets, FILE *log)
	{
		if (tried) return;
		tried = true;
		if (const char *s = getenv("VF3_ENDING_SCROLL")) {
			const float v = float(atof(s));
			if (v > 0) speed = v;
		}
		const std::string dir = assets + "/menu-console/ending-text/";
		// The eight pages the Dreamcast's script asks for, in its order: ranks 80, 81, 82, 84 ... 88.
		static const char *const NAMES[8] = {
			"page1_r80.png", "page2_r81.png", "page3_r82.png", "page4_r84.png",
			"page5_r85.png", "page6_r86.png", "page7_r87.png", "page8_r88.png",
		};
		for (int i = 0; i < 8; i++) {
			page p;
			piece a;
			a.t = read(ren, dir + NAMES[i], a.w, a.h, log);
			if (!a.t) { drop(); return; }
			p.parts.push_back(a);
			p.w = a.w;
			p.h = a.h;
			// THE PROGRAMMERS' NAMES ARE TWO PATTERNS. 208 to the right and 24 lower: the game's own
			// placement, read off its call -- rank 82 at the page's place, rank 83 at (378, 161) when the
			// page itself is at (170, 137).
			if (i == 2) {
				piece b;
				b.t = read(ren, dir + "extra_r83.png", b.w, b.h, log);
				if (b.t) {
					b.dx = 208.0f;
					b.dy = 24.0f;
					p.parts.push_back(b);
					p.w = std::max(p.w, b.dx + b.w);
					p.h = std::max(p.h, b.dy + b.h);
				}
			}
			pages.push_back(p);
		}
		total = 0;
		(void)0;   // (the column is laid below, once the pictures are known)
		// the illustrations, by name, out of the folder fv2_ending.h reads (its old name is still taken)
		{
			// THE DISC'S OWN ART, not the first version's. Frederic, the 2026-10-30: "les illustrations sont
			// ici [...] DC_ending\HD, ne reprends pas celles de la premiere version du ending dreamcast".
			// They are in menu-console/ending-art, numbered: the folder is read in NAME ORDER, so renaming
			// a file is how the order is changed, and the first eight are the ones the roll has room for --
			// one in each gap between two pages.
			const std::string fin = assets + "/menu-console/ending-art/";
			std::vector<std::string> names;
			std::error_code ec2;
			for (const auto &e : std::filesystem::directory_iterator(fin, ec2)) {
				if (!e.is_regular_file(ec2)) continue;
				std::string ext = e.path().extension().string();
				for (char &c : ext) c = char(tolower((unsigned char)c));
				if (ext == ".png") names.push_back(e.path().string());
			}
			std::sort(names.begin(), names.end());
			for (const std::string &n : names) {
				art a;
				float fw = 0, fh = 0;
				a.t = read(ren, n, fw, fh, log, &a.ink, FACE_TALL);
				if (!a.t) continue;
				// drawn to its own ink and to the screen's whole height
				const float k2 = FACE / float(std::max(1, a.ink.h));
				a.w = float(a.ink.w) * k2;
				a.h = FACE;
				art_of.push_back(a);
			}
		}
		// the column: a page, then a picture, then a page ... and the pictures left over after them
		column.clear();
		total = 0;
		for (size_t i = 0; i < std::max(pages.size(), art_of.size()); i++) {
			if (i < pages.size()) { column.push_back({ int(i), -1, pages[i].h }); total += pages[i].h; }
			if (i < art_of.size()) { column.push_back({ -1, int(i), GAP }); total += GAP; }
		}
		float gw = 0, gh = 0;
		ground = read(ren, dir + "ground.png", gw, gh, log);
		ground_w = int(gw * HD);
		ground_h = int(gh * HD);
		if (log)
			fprintf(log, "the console's roll: %d pages, %d illustrations, %.0f pixels of column, %.0f a second (%.0f s), the ground %s\n",
					int(pages.size()), int(art_of.size()), total, speed, (DC_H + total) / speed, ground ? "is in" : "is missing");
	}

	// Over the whole window. `progress` is how far through its ending the GAME is, 0 to 1 (console_play.h
	// counts it in the game's own segments), so the roll takes exactly as long as the ending does -- long
	// when the replays have matches to play, short when they have none, and never cut off.
	void render(SDL_Renderer *ren, const SDL_FRect &all, double progress)
	{
		if (pages.empty()) return;
		// The ground covers the window, its own shape kept: a credit roll with bars down the sides reads as
		// a picture that failed, which is the note fv2_ending.h already makes about its own pages.
		SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);
		SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
		SDL_RenderFillRect(ren, &all);
		if (ground && ground_w > 0 && ground_h > 0) {
			const float k = std::max(all.w / float(ground_w), all.h / float(ground_h));
			const float w = float(ground_w) * k, h = float(ground_h) * k;
			const SDL_FRect to = { all.x + (all.w - w) * 0.5f, all.y + (all.h - h) * 0.5f, w, h };
			SDL_RenderTexture(ren, ground, nullptr, &to);
		}
		SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
		// The window's height is the Dreamcast's 480: that is what keeps Sega's proportions whatever the
		// window's shape, and the column is centred across it.
		const float k = all.h / DC_H;
		// THE ILLUSTRATION OF THE PAGE THAT IS UP. The topmost visible page owns the screen's picture, and
		// it comes in from the side as the disc brings it in: 600 pixels in fifteen frames, from the right
		// for one page and from the left for the next.
		int want = -1;
		double face_u = 0, face_w_s = 0;
		const float y0 = DC_H - float(progress) * (DC_H + total);
		{
			// WHICH PICTURE IS UP, AND FOR HOW LONG. A picture takes its turn when its place in the column
			// passes the middle of the screen, and keeps it until the NEXT one gets there -- so there is
			// always exactly one, it comes in, it stays while its page of credits goes by, and it leaves.
			//
			// The window used to be the slot's own crossing plus half a screen either side, which at this
			// speed is twenty-eight seconds: the first picture held the screen for a quarter of the roll
			// and the others never had their turn. Frederic, the 2026-10-30: "il n'y a plus aucun
			// defilement des illustrations!!".
			float yy = y0;
			float here = 0, next_y = 0;
			bool found = false;
			for (const slot &sl : column) {
				if (sl.face >= 0) {
					if (yy <= DC_H * 0.5f) { want = sl.face; here = yy; found = true; }
					else if (found) { next_y = yy; break; }
				}
				yy += sl.h;
			}
			if (found) {
				face_u = double(DC_H * 0.5f - here);                       // how far past the middle it is
				face_w_s = next_y > 0 ? double(next_y - here) : face_u + 2.0 * SLIDE_PX;
			}
		}
		face_at = want;
		const art *a = want >= 0 && want < int(art_of.size()) ? &art_of[size_t(want)] : nullptr;
		if (a && a->t) {
			// IN FROM THE LEFT, RESTING ON THE RIGHT, OUT BY THE RIGHT -- and the right is THE WINDOW'S, not
			// the 4:3 frame's. Frederic asked twice for the picture further right and the text further left:
			// the second time it was not the margins that were wrong but the frame they were measured in.
			// On a 16:9 window the Dreamcast's 640 occupies the middle, so a picture hard against its right
			// edge still has an eighth of the screen beyond it.
			const float w = a->w * k, h = a->h * k;
			const float rest = all.x + all.w - FACE_RIGHT * k - w;
			float fx = rest;
			if (face_u < SLIDE_PX) {
				const float t = float(std::clamp(face_u / SLIDE_PX, 0.0, 1.0));
				fx = (all.x - w) + (rest - (all.x - w)) * t;
			} else if (face_u > face_w_s - SLIDE_PX) {
				const float t = float(std::clamp((face_u - (face_w_s - SLIDE_PX)) / SLIDE_PX, 0.0, 1.0));
				fx = rest + (all.x + all.w - rest) * t;
			}
			const SDL_FRect src = { float(a->ink.x), float(a->ink.y), float(a->ink.w), float(a->ink.h) };
			const SDL_FRect to = { fx, all.y, w, h };
			SDL_RenderTexture(ren, a->t, &src, &to);
		}
		float y = y0;
		for (const slot &sl : column) {
			if (sl.page >= 0) {
				const page &p = pages[size_t(sl.page)];
				if (y + p.h > 0 && y < DC_H) {
					// left-aligned ON THE WINDOW, so the column begins where the screen does
					const float x0 = all.x + TEXT_LEFT * k;
					for (const piece &q : p.parts) {
						if (!q.t) continue;
						const SDL_FRect to = { x0 + q.dx * k, all.y + (y + q.dy) * k, q.w * k, q.h * k };
						SDL_RenderTexture(ren, q.t, nullptr, &to);
					}
				}
			}
			y += sl.h;
		}
	}
};
