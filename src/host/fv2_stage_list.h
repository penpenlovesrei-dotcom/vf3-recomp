// The console's STAGE SELECT: which arena the fight happens in, chosen AFTER the two players have picked
// their fighters -- which is where the summer's patched ROM put it, and where Sega left room for it.
//
// WHERE IT GOES, and the measurement that puts it there. The game's six SEL sub-states are SEL_START, SELECT,
// STAGE_SEL_INT, STAGE_SEL_DSP, DECIDE_INT, DECIDE_DSP (docs/fv2-etats.md), and the arcade board jumps
// straight from SELECT to DECIDE_INT -- its two STAGE_SEL states are an EMPTY STUB, one `b` to the
// dispatcher's exit, read in Ghidra the 2026-10-24. The characters are chosen at exactly that step, so this
// page opens there: the game is frozen at DECIDE_INT, the page is answered, and the fight starts in the
// chosen arena. Sega's own slot, with our own screen in it.
//
// THE FORM IS THE SUMMER ROM'S, because the pictures are: a carousel, not a list. One large thumbnail at a
// time with its name underneath and an arrow each side -- select_stage.png, vignette_01..14.png,
// nom_01..14.png, fleche_gauche.png, fleche_droite.png, drawn by Frederic for the patched ROM and handed over
// the 2026-10-25. Fourteen of them, where this port had only ever named twelve arenas (model3.cpp), and one
// of the fourteen is a "???" -- a stage nobody has named yet.
//
// THE NUMBERS ARE NOT YET PROVEN TO BE THE GAME'S. nom_14 reads "The Skies of Armstone", which this port's
// own list calls arena 11, so the ROM's numbering and the owner's twelve-name list do not agree. What the
// page holds is its index, and whether index N is scene N has to be MEASURED arena by arena -- photographed
// and compared with the vignette -- before any name here is trusted. Until then the picture is the truth and
// the number is a guess: see docs/fv2-console.md.
//
// It is a surface like the other three: it freezes the game, takes the keys and the pad, draws over the
// picture the game made, and tells the host only what it wants heard. Nothing here is RmlUi.
#pragma once
#include <SDL3/SDL.h>
#include <string>
#include <vector>
#include "controls.h"
#include "fv2_texte.h"
#include "png.h"

struct fv2_stage_list
{
	static constexpr int STAGES = 14;             // as many as the ROM's pictures
	static constexpr int CANCELLED = -2;          // backed out: the game chooses its own arena, as on the board
	enum { SND_NONE, SND_MOVE, SND_VALIDATE, SND_CANCEL };
	int want_sound = SND_NONE;

	// In fractions of the game's own 4:3 screen, as every other surface here is measured, so the page keeps
	// its place whatever the window does. The pictures' own proportions are kept.
	static constexpr uint8_t DIM_ALPHA = 150;
	static constexpr float BANNER_W = 0.34f, BANNER_Y = 0.085f;
	static constexpr float SHOT_W = 0.620f, SHOT_Y = 0.215f;
	static constexpr float NAME_H = 0.085f, NAME_GAP = 0.030f;
	static constexpr float ARROW_H = 0.115f, ARROW_GAP = 0.030f;
	static constexpr float FOOT_Y = 0.906f, FOOT_L = 0.300f, FOOT_R = 0.715f;
	static constexpr SDL_Color PLAIN = { 247, 246, 246, 255 };

	bool shown = false;
	int sel = 0;                   // 0-based; the scene asked for is sel + 1
	bool ready = false, tried = false;
	SDL_Texture *banner = nullptr, *left = nullptr, *right = nullptr;
	SDL_Texture *shot[STAGES] = {}, *name[STAGES] = {};
	int shot_w[STAGES] = {}, shot_h[STAGES] = {}, name_w[STAGES] = {}, name_h[STAGES] = {};
	int banner_w = 0, banner_h = 0, arrow_w = 0, arrow_h = 0;
	int have = 0;                  // how many stages actually loaded, in order from 1
	fv2_texte *font = nullptr;     // for the two hints along the bottom
	int stick_x = 0;

	// One picture into a texture. Returns null and says why rather than failing silently: a page that draws
	// nothing looks exactly like a page that was never asked for.
	static SDL_Texture *one(SDL_Renderer *ren, const std::string &path, int &w, int &h, FILE *log)
	{
		std::vector<uint32_t> px;
		std::string why;
		if (!read_png(path.c_str(), px, w, h, why)) {
			if (log) fprintf(log, "the console's stage select: %s %s\n", path.c_str(), why.c_str());
			return nullptr;
		}
		SDL_Texture *t = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STATIC, w, h);
		if (!t) {
			if (log) fprintf(log, "the console's stage select: no texture for %s (%s)\n", path.c_str(), SDL_GetError());
			return nullptr;
		}
		SDL_UpdateTexture(t, nullptr, px.data(), w * 4);
		SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
		SDL_SetTextureScaleMode(t, SDL_SCALEMODE_LINEAR);
		return t;
	}

	bool load(SDL_Renderer *ren, const std::string &assets, fv2_texte *sheet, FILE *log)
	{
		if (tried) return ready;
		tried = true;
		font = sheet;
		const std::string dir = assets + "/menu-console/decors/";
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
		if (log) fprintf(log, "the console's stage select: %d stages, %s banner, %s arrows\n",
						 have, banner ? "its" : "NO", left && right ? "its" : "NO");
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
		load(ren, assets, font, nullptr);
	}

	void show()
	{
		if (!ready) return;
		shown = true;
		stick_x = 0;
	}
	void hide() { shown = false; }

	void move(int d)
	{
		if (have <= 0) return;
		sel = (sel + have + d) % have;
		want_sound = SND_MOVE;
	}

	// One event while the page is up. Returns the stage (0-based) when one is chosen, CANCELLED when the
	// player backs out -- which means the game picks its own arena, as it does on the board -- or -1.
	int handle_event(const SDL_Event &e, const player_controls ctl[2])
	{
		if (!shown) return -1;
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
			else if (sc == SDL_SCANCODE_ESCAPE) return -1;   // (Escape belongs to the host's own menu)
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

	// Over the picture the game made. mid is the game's own 4:3 screen, which every number above is a
	// fraction of; the pictures keep their own proportions inside it.
	void render(SDL_Renderer *ren, const SDL_FRect &all, const SDL_FRect &mid)
	{
		if (!shown || !ready || have <= 0) return;
		SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
		SDL_SetRenderDrawColor(ren, 0, 0, 0, DIM_ALPHA);
		SDL_RenderFillRect(ren, &all);

		auto put = [&](SDL_Texture *t, float cx, float top, float w, float h) {
			if (!t) return;
			const SDL_FRect r = { cx - w * 0.5f, top, w, h };
			SDL_RenderTexture(ren, t, nullptr, &r);
		};
		const float cx = mid.x + mid.w * 0.5f;

		if (banner && banner_h > 0) {
			const float w = BANNER_W * mid.w, h = w * float(banner_h) / float(banner_w);
			put(banner, cx, mid.y + BANNER_Y * mid.h, w, h);
		}

		const int i = sel >= 0 && sel < have ? sel : 0;
		float shot_bottom = mid.y + SHOT_Y * mid.h;
		if (shot[i] && shot_w[i] > 0) {
			const float w = SHOT_W * mid.w, h = w * float(shot_h[i]) / float(shot_w[i]);
			const float top = mid.y + SHOT_Y * mid.h;
			put(shot[i], cx, top, w, h);
			shot_bottom = top + h;
			// the arrows, one each side of the picture, centred on it
			if (left && right && arrow_h > 0) {
				const float ah = ARROW_H * mid.h, aw = ah * float(arrow_w) / float(arrow_h);
				const float ay = top + (h - ah) * 0.5f;
				const SDL_FRect l = { cx - w * 0.5f - ARROW_GAP * mid.w - aw, ay, aw, ah };
				const SDL_FRect r = { cx + w * 0.5f + ARROW_GAP * mid.w, ay, aw, ah };
				SDL_RenderTexture(ren, left, nullptr, &l);
				SDL_RenderTexture(ren, right, nullptr, &r);
			}
		}

		if (name[i] && name_h[i] > 0) {
			const float h = NAME_H * mid.h, w = h * float(name_w[i]) / float(name_h[i]);
			put(name[i], cx, shot_bottom + NAME_GAP * mid.h, w, h);
		}

		if (font && font->ready) {
			const float fh = 0.0290f * mid.h / fv2_texte::CAP * 0.58f;
			font->draw_mid(ren, "LEFT AND RIGHT TO CHOOSE THE STAGE", mid.x + FOOT_L * mid.w, mid.y + FOOT_Y * mid.h, fh, PLAIN);
			font->draw_mid(ren, "A TO DECIDE, START FOR THE GAME'S OWN", mid.x + FOOT_R * mid.w, mid.y + FOOT_Y * mid.h, fh, PLAIN);
		}
	}
};
