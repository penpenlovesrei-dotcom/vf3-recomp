// Fighting Vipers 2's STAGE SELECT: which arena the versus is fought in, chosen AFTER the two players have
// picked their fighters. Carried over from this port's console build, where it was built on the owner's own
// pictures and his own layout; what is here is the same page with the console's letter sheet taken out, the
// names being pictures and not words (fv2_versus.h holds the mode it belongs to).
//
// WHERE IT GOES, and the measurement that puts it there. The game's six SEL sub-states are SEL_START, SELECT,
// STAGE_SEL_INT, STAGE_SEL_DSP, DECIDE_INT, DECIDE_DSP, and the arcade board jumps straight from SELECT to
// DECIDE_INT: its two STAGE_SEL states are an EMPTY STUB, one `b` to the dispatcher's exit. The characters are
// chosen at exactly that step, so this page opens there -- the game PARKED in Sega's own empty sub-state
// (Model3::fv2_park_stage_select) -- and the arena is held before the match reads it ten frames later.
//
// IT IS NOT A SURFACE LIKE THE PORT'S MENU. The menu under Escape freezes the PowerPC; this one leaves the
// game RUNNING. Frozen, the select screen the game had made drained away -- the two fighters vanished, the
// music stopped, and nothing could be heard moving through the page. Parked, the game goes on drawing that
// screen and the sound board goes on playing, because the game never stopped. Nothing here is RmlUi.
//
// THE PICTURES ARE THE OWNER'S, drawn for the patched ROM of the summer: select_stage.png, vignette_01..14,
// nom_01..14, fleche_gauche, fleche_droite, under assets/fv2/stages. Fourteen of them, where this port had
// only ever named twelve arenas -- nom_13 is "???" on a black picture and nom_14 carries nom_11's text over
// the training room's grid. AND THE GAME'S OWN TABLE ONLY GOES SO FAR: the stage draw's 27 entries are free
// at 1 to 3 and 5 to 12, conditional at 4, locked from 13 up (model3.cpp), so the last two pictures have no
// arena behind them. The page offers what it was given; what the draw can honour is the draw's business.
#pragma once
#include <SDL3/SDL.h>
#include <string>
#include <vector>
#include "controls.h"
#include "png.h"

struct fv2_stages
{
	static constexpr int STAGES = 14;             // as many as the pictures
	static constexpr int CANCELLED = -2;          // backed out: the game chooses its own arena, as on the board
	enum { SND_NONE, SND_MOVE, SND_VALIDATE, SND_CANCEL };
	int want_sound = SND_NONE;

	// THE LAYOUT IS THE OWNER'S, down to the tenths, and each number carries the reason it is what it is:
	//
	//   * a BAND across the bottom, dark enough for a green name to read on and light enough that the select
	//     screen behind still shows: it is what tells the player their fighters are still chosen;
	//   * the pictures HANG FROM A COMMON TOP rather than being centred on a common line. The chosen one is
	//     wider, so centring grew it upwards too and the row of tops came out ragged;
	//   * the chosen one is a tenth wider than measured, and the side ones have their OWN width rather than a
	//     fraction of it, so growing the middle does not silently grow the strip;
	//   * the words and the name hang off the chosen picture's edges, not off the screen, so they follow it;
	//   * the arrows are hung off the WINDOW'S two edges, because the window is what has edges: on a wide
	//     screen the game's 4:3 picture ends somewhere in the middle of what the player sees.
	//
	// And the strip sits LOW, in front of the game's own row of portraits rather than above it, the band being
	// translucent so they still show through.
	static constexpr float BAND_TOP = 0.615f, BAND_BOTTOM = 0.975f, STRIP_TOP = 0.685f;
	static constexpr float MAIN_W = 0.2585f;     // 0.235 as measured, plus a tenth
	static constexpr float SIDE_W = 0.130f, GAP = 0.012f;
	static constexpr float SIDE_DIM = 0.34f;     // the side pictures darkened
	static constexpr int SIDE_COUNT = 2;         // two each side, not three: a third passed under an arrow
	static constexpr float ARROW_H = 0.084f, ARROW_EDGE = 0.015f;
	static constexpr float LABEL_H = 0.0315f, NAME_H = 0.0625f;
	static constexpr float CLING = 0.012f;       // one value for both sides: they are one pair
	// How much of a name picture sits BELOW its baseline. The names were padded to a common height so their
	// letters come out the same size, and that padding is not symmetrical: the cap top is at 14 of 106 and the
	// capitals are 67 tall, so 25 of the 106 are the room descenders need. SELECT STAGE has no such room -- it
	// is cut to its own ink -- so bottom-aligning the two BOXES would hang the name higher than the label by
	// exactly that. Lowering it by this aligns what is actually read, the baselines.
	static constexpr float NAME_TAIL = 25.0f / 106.0f;
	static constexpr float RAISE = 0.022f;       // how far above the picture's bottom edge the words sit
	// No line of instructions: the arrows say which way it turns, and the game's own select screen never
	// explained itself either.

	// ANSWERED, AND HELD WHILE THE SOUND IS HEARD. The page used to vanish on the very frame it was answered,
	// so the sound that says "chosen" began under the next screen and the choice was never seen to register.
	// For this long the page stays, the chosen picture blinks, and the keys are dead -- a second press must not
	// choose a second arena. The game is parked and still running underneath, so the music carries on.
	// HOW LONG IS MEASURED: the validation effect is 0x23, it takes chip 1's slots 2 and 3, peaks 32 frames in
	// and is still at a fifth of that peak 67 frames later. Half a second would cut it in its middle.
	static constexpr double HOLD_S = 1.2;
	static constexpr double BLINK_S = 0.15;
	bool locked = false;
	double lock_t = 0;             // seconds since the answer, kept by the host, which owns the clock

	bool shown = false;
	int sel = 0;                   // 0-based; the scene asked for is sel + 1
	bool ready = false, tried = false;
	SDL_Texture *banner = nullptr, *left = nullptr, *right = nullptr;
	SDL_Texture *shot[STAGES] = {}, *name[STAGES] = {};
	int shot_w[STAGES] = {}, shot_h[STAGES] = {}, name_w[STAGES] = {}, name_h[STAGES] = {};
	int banner_w = 0, banner_h = 0, arrow_w = 0, arrow_h = 0;
	int have = 0;                  // how many stages actually loaded, in order from 1
	int stick_x = 0;
	std::string dir;               // where the pictures were looked for (the log says so once)

	// One picture into a texture. Returns null and says why rather than failing silently: a page that draws
	// nothing looks exactly like a page that was never asked for.
	static SDL_Texture *one(SDL_Renderer *ren, const std::string &path, int &w, int &h, FILE *log)
	{
		std::vector<uint32_t> px;
		std::string why;
		if (!read_png(path.c_str(), px, w, h, why)) {
			if (log) fprintf(log, "Fighting Vipers 2's stage select: %s %s\n", path.c_str(), why.c_str());
			return nullptr;
		}
		SDL_Texture *t = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STATIC, w, h);
		if (!t) {
			if (log) fprintf(log, "Fighting Vipers 2's stage select: no texture for %s (%s)\n", path.c_str(), SDL_GetError());
			return nullptr;
		}
		SDL_UpdateTexture(t, nullptr, px.data(), w * 4);
		SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
		SDL_SetTextureScaleMode(t, SDL_SCALEMODE_LINEAR);
		return t;
	}

	bool load(SDL_Renderer *ren, const std::string &assets, FILE *log)
	{
		if (tried) return ready;
		tried = true;
		dir = assets + "/fv2/stages/";
		banner = one(ren, dir + "select_stage.png", banner_w, banner_h, log);
		left = one(ren, dir + "fleche_gauche.png", arrow_w, arrow_h, log);
		int rw = 0, rh = 0;
		right = one(ren, dir + "fleche_droite.png", rw, rh, log);
		for (int i = 0; i < STAGES; i++) {
			char f[64];
			snprintf(f, sizeof f, "vignette_%02d.png", i + 1);
			shot[i] = one(ren, dir + f, shot_w[i], shot_h[i], log);
			snprintf(f, sizeof f, "nom_%02d.png", i + 1);
			name[i] = one(ren, dir + f, name_w[i], name_h[i], log);
			if (!shot[i]) break;      // the stages run from 1 with no gaps: the first missing one ends the list
			have = i + 1;
		}
		ready = have > 0;
		if (log) fprintf(log, "Fighting Vipers 2's stage select: %d stages in %s, %s banner, %s arrows\n",
		                 have, dir.c_str(), banner ? "its" : "NO", left && right ? "its" : "NO");
		return ready;
	}

	void textures_lost(SDL_Renderer *ren, const std::string &assets)
	{
		auto drop = [](SDL_Texture *&t) { if (t) { SDL_DestroyTexture(t); t = nullptr; } };
		drop(banner); drop(left); drop(right);
		for (int i = 0; i < STAGES; i++) { drop(shot[i]); drop(name[i]); }
		have = 0;
		ready = false;
		tried = false;
		load(ren, assets, nullptr);
	}

	void show()
	{
		if (!ready) return;
		shown = true;
		stick_x = 0;
		locked = false;
		lock_t = 0;
	}
	void hide() { shown = false; locked = false; lock_t = 0; }
	void lock() { locked = true; lock_t = 0; }

	void move(int d)
	{
		if (have <= 0) return;
		sel = (sel + have + d) % have;
		want_sound = SND_MOVE;
	}

	// One event while the page is up. Returns the stage (0-based) when one is chosen, CANCELLED when the player
	// backs out -- which means the game picks its own arena, as it does on the board -- or -1.
	int handle_event(const SDL_Event &e, const player_controls ctl[2])
	{
		if (!shown || locked) return -1;      // (answered: the hold above belongs to the sound)
		auto bound = [&](int action, SDL_Scancode sc) {
			for (int p = 0; p < 2; p++) if (ctl[p].key[action] == sc) return true;
			return false;
		};
		switch (e.type) {
		case SDL_EVENT_KEY_DOWN: {
			if (e.key.repeat) return -1;
			const SDL_Scancode sc = e.key.scancode;
			if (sc == SDL_SCANCODE_LEFT || bound(C_LEFT, sc)) move(-1);
			else if (sc == SDL_SCANCODE_RIGHT || bound(C_RIGHT, sc)) move(+1);
			else if (sc == SDL_SCANCODE_UP || bound(C_UP, sc)) move(-1);
			else if (sc == SDL_SCANCODE_DOWN || bound(C_DOWN, sc)) move(+1);
			else if (sc == SDL_SCANCODE_ESCAPE) return -1;   // (Escape belongs to the port's own menu)
			else if (bound(C_START, sc)) { want_sound = SND_CANCEL; return CANCELLED; }
			else if (sc == SDL_SCANCODE_BACKSPACE || bound(C_KICK, sc)) { want_sound = SND_CANCEL; return CANCELLED; }
			else if (sc == SDL_SCANCODE_RETURN || sc == SDL_SCANCODE_KP_ENTER || sc == SDL_SCANCODE_SPACE
			         || bound(C_PUNCH, sc)) {
				want_sound = SND_VALIDATE;
				return sel;
			}
			return -1;
		}
		case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
			switch (e.gbutton.button) {
			case SDL_GAMEPAD_BUTTON_DPAD_LEFT: case SDL_GAMEPAD_BUTTON_DPAD_UP: move(-1); break;
			case SDL_GAMEPAD_BUTTON_DPAD_RIGHT: case SDL_GAMEPAD_BUTTON_DPAD_DOWN: move(+1); break;
			case SDL_GAMEPAD_BUTTON_START: case SDL_GAMEPAD_BUTTON_EAST:
				want_sound = SND_CANCEL;
				return CANCELLED;
			case SDL_GAMEPAD_BUTTON_SOUTH:
				want_sound = SND_VALIDATE;
				return sel;
			default: break;
			}
			return -1;
		case SDL_EVENT_GAMEPAD_AXIS_MOTION: {
			if (e.gaxis.axis != SDL_GAMEPAD_AXIS_LEFTX) return -1;
			const int dir = e.gaxis.value < -16000 ? -1 : e.gaxis.value > 16000 ? 1 : 0;
			if (dir && dir != stick_x) move(dir);
			stick_x = dir;
			return -1;
		}
		default: return -1;
		}
	}

	// Over the picture the game made. `mid` is the game's own 4:3 screen, which every number above is a
	// fraction of; `all` is the whole window, which the band and the arrows reach to.
	void render(SDL_Renderer *ren, const SDL_FRect &all, const SDL_FRect &mid)
	{
		if (!shown || !ready || have <= 0) return;
		SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);

		const int cur = sel >= 0 && sel < have ? sel : 0;
		const float mx = mid.x + mid.w * 0.5f;
		const float strip_top = mid.y + STRIP_TOP * mid.h;

		// The ground, across the WHOLE window and not just the game's 4:3 picture: the strip and the arrows both
		// reach past that picture's edges, and a band stopping short of them left them floating on the
		// letterbox. The screen is not dimmed -- only this band is laid, and the select screen shows above it.
		const SDL_FRect band = { all.x, mid.y + BAND_TOP * mid.h, all.w, (BAND_BOTTOM - BAND_TOP) * mid.h };
		SDL_SetRenderDrawColor(ren, 0, 0, 0, 158);     // 0.62 of full
		SDL_RenderFillRect(ren, &band);

		// The sides first, then the middle over them: the chosen one may overlap its neighbours, and drawing it
		// last is what lets it.
		for (int pass = 0; pass < 2; pass++) {
			for (int d = -SIDE_COUNT; d <= SIDE_COUNT; d++) {
				const bool middle = d == 0;
				if (middle != (pass == 1)) continue;
				const int i = ((cur + d) % have + have) % have;
				if (!shot[i] || shot_w[i] <= 0) continue;
				const float w = (middle ? MAIN_W : SIDE_W) * mid.w;
				const float h = w * float(shot_h[i]) / float(shot_w[i]);
				// Each step out spans half of each of the two widths it crosses, so neighbours keep the same gap
				// whatever their sizes.
				float cx = mx;
				for (int k = 1; k <= (d < 0 ? -d : d); k++) {
					const float a = (k == 1 ? MAIN_W : SIDE_W) * mid.w;
					cx += (d < 0 ? -1.0f : 1.0f) * ((a + SIDE_W * mid.w) * 0.5f + GAP * mid.w);
				}
				const bool off = middle && locked && int(lock_t / BLINK_S) % 2 == 1;
				const uint8_t dim = off ? uint8_t(SIDE_DIM * 255.0f) : middle ? 255 : uint8_t(SIDE_DIM * 255.0f);
				SDL_SetTextureColorMod(shot[i], dim, dim, dim);
				const SDL_FRect r = { cx - w * 0.5f, strip_top, w, h };
				SDL_RenderTexture(ren, shot[i], nullptr, &r);
				SDL_SetTextureColorMod(shot[i], 255, 255, 255);
			}
		}

		// The chosen picture's own edges, which the words hang off
		const float main_w = MAIN_W * mid.w;
		const float main_h = shot_w[cur] > 0 ? main_w * float(shot_h[cur]) / float(shot_w[cur]) : 0.0f;
		const float main_bottom = strip_top + main_h;
		const float main_left = mx - main_w * 0.5f, main_right = mx + main_w * 0.5f;

		// The name, hard against the right edge, sitting on the baseline the label shares
		if (name[cur] && name_h[cur] > 0) {
			const float h = NAME_H * mid.h, w = h * float(name_w[cur]) / float(name_h[cur]);
			const float bottom = main_bottom - RAISE * mid.h + NAME_TAIL * h;
			const SDL_FRect r = { main_right + CLING * mid.w, bottom - h, w, h };
			SDL_RenderTexture(ren, name[cur], nullptr, &r);
		}

		// SELECT STAGE, hard against the left edge and on that same baseline
		if (banner && banner_h > 0) {
			const float h = LABEL_H * mid.h, w = h * float(banner_w) / float(banner_h);
			const SDL_FRect r = { main_left - CLING * mid.w - w, main_bottom - RAISE * mid.h - h, w, h };
			SDL_RenderTexture(ren, banner, nullptr, &r);
		}

		// The arrows, hung off the window's two edges and centred on the row's height
		if (left && right && arrow_h > 0 && shot_w[cur] > 0) {
			const float h = ARROW_H * mid.h, w = h * float(arrow_w) / float(arrow_h);
			const float side_h = SIDE_W * mid.w * float(shot_h[cur]) / float(shot_w[cur]);
			const float my = strip_top + side_h * 0.5f;
			const float edge = ARROW_EDGE * all.w;
			const SDL_FRect l = { all.x + edge, my - h * 0.5f, w, h };
			const SDL_FRect r = { all.x + all.w - edge - w, my - h * 0.5f, w, h };
			SDL_RenderTexture(ren, left, nullptr, &l);
			SDL_RenderTexture(ren, right, nullptr, &r);
		}
	}
};
