// The console's PAUSE MENU: Start during a match, over the fight, with the game frozen behind it.
//
// August had this menu and measured its four levers (docs/fv2-console-menu-inventory.md section 2); this
// port never had it, and docs/fv2-console.md said why -- "Escape already does everything that menu would, so
// this was left until the modes are finished". The modes were finished on the 2026-10-23, so here it is.
//
// WHAT "BETTER INTEGRATED" MEANS HERE, since Frederic asked for that and not merely for the four lines:
//
//   * it is a surface like the other two, not a page of this port's menu. The list of modes and the OPTIONS
//     page already freeze the game, take the keys and the pad, and draw over the picture the game made; this
//     one is built the same way and says the same things to the host (want_sound, CANCELLED).
//   * it is drawn with the GAME'S OWN LETTERS, the glyph sheet the OPTIONS page uses, and carries that page's
//     pink cursor rule, hung by its bottom for the same reason. Nothing here is RmlUi.
//   * the fight stays visible under it, taken down to half, which is the Dreamcast's own dimming measured for
//     the list of modes (DIM_ALPHA there, the same number here).
//   * START OPENS IT AND START CLOSES IT. The button that paused is the button that resumes, which is what a
//     console does; B also backs out, as everywhere else in these surfaces.
//
// A LINE WITH NO LEVER IS DRAWN, DIMMED AND STEPPED OVER, which is the rule the OPTIONS page already follows
// and the reason is the same: a menu whose lines come and go is misread at a glance. Two of the four are in
// that state and say below exactly what is missing for them.
#pragma once
#include <SDL3/SDL.h>
#include <algorithm>
#include <string>
#include <vector>
#include "controls.h"
#include "fv2_text.h"
#include "png.h"

struct fv2_pause
{
	// THE LINES, AND THEY ARE NOT THE SAME IN EVERY MODE. August had four, in one list, for a fork that
	// only ever paused a versus. Frederic asked the 2026-10-25 for a line that was missing and for the ones
	// that mean nothing in a given mode to go:
	//
	//   RESUME             was missing. B and Start already went back to the fight, but a pause menu that
	//                      does not say so is a menu you have to guess your way out of. It is first, and it
	//                      is where the cursor starts.
	//   REMATCH            the same two fighters, the same arena, another match (Model3::fv2_rematch).
	//                      NOT IN TRAINING: there the round restarts of itself and the lives are held, so
	//                      the line would promise something that is already happening.
	//   CHANGE STAGE       opens the carousel -- the same page the V.S. line opens -- instead of stepping a
	//                      scene number nobody can picture. ONLY WHERE IT COMMANDS ANYTHING: the arena is
	//                      drawn once a match, so in a versus and in training it is this match's arena that
	//                      changes, while on the arcade's fixed road it would be the next one's. A line that
	//                      changes something else than it says is worse than no line.
	//   CHANGE YOUR VIPER  the match finished off screen, then the select screen with two players -- and
	//                      the Dreamcast's loading picture over the whole road, so that the end of the
	//                      game is neither seen nor heard (fv2_curtain.h, the GOING state).
	//   MAIN MENU          the same road, and THE CONSOLE'S OWN LIST OF MODES at the end of it. It said
	//                      TITLE SCREEN and went to the game's, which on this board means walking through
	//                      its GAME OVER scene -- Frederic saw it the 2026-10-25 and asked for the menu
	//                      instead, which is where a console player expects to land.
	enum { RESUME, REMATCH, CHANGE_STAGE, CHANGE_VIPER, MAIN_MENU, LINES };
	static const char *name(int l)
	{
		static const char *const N[LINES] = { "RESUME", "REMATCH", "CHANGE STAGE", "CHANGE YOUR VIPER",
											  "MAIN MENU" };
		return l >= 0 && l < LINES ? N[l] : "?";
	}
	// Which lines this mode shows, as a mask of 1 << line. The host sets it when it opens the menu, because
	// it is the host that knows which mode the console is in; this file only draws what it is given.
	static constexpr uint32_t ALL = (1u << LINES) - 1u;
	uint32_t on = ALL;
	bool shows(int l) const { return l >= 0 && l < LINES && (on & (1u << l)); }

	static constexpr int CANCELLED = -2;          // backed out: the host resumes the game
	enum { SND_NONE, SND_MOVE, SND_VALIDATE, SND_CANCEL };
	int want_sound = SND_NONE;

	// THE FORM IS THE DREAMCAST'S, which Frederic showed on the 2026-10-24: not a page of centred lines like
	// the OPTIONS screen, but a PANEL -- a translucent violet box under a pale header bar that reads "1P
	// PAUSE" -- standing on the side of the player who paused, over a darkened fight, with the two hints
	// split left and right along the bottom.
	//
	// The numbers below are read off that picture in fractions of the game's own 4:3 screen, which is how
	// every other surface here is measured, so the panel keeps its place whatever the window does.
	static constexpr uint8_t DIM_ALPHA = 110;          // the fight stays readable under it
	static constexpr float PANEL_W = 0.355f, PANEL_H = 0.312f;
	static constexpr float PANEL_TOP = 0.400f;         // the box; its header bar sits above this
	static constexpr float PANEL_MARGIN = 0.135f;      // from the picture's edge, on the player's own side
	static constexpr float BAR_H = 0.052f;             // the pale header bar
	static constexpr float TEXT_IN = 0.022f;           // the lines' left margin inside the box
	static constexpr float ROW_TOP = 0.055f, PITCH = 0.0615f, ROW_CAP = 0.0330f;
	static constexpr float FOOT_Y = 0.906f, FOOT_L = 0.300f, FOOT_R = 0.715f;
	static constexpr SDL_Color CHOSEN = { 252, 252, 10, 255 };
	static constexpr SDL_Color PLAIN = { 247, 246, 246, 255 };
	static constexpr SDL_Color MUTED = { 150, 150, 150, 255 };
	static constexpr SDL_Color PANEL = { 150, 62, 140, 190 };     // the box, translucent
	static constexpr SDL_Color BAR = { 226, 226, 226, 235 };      // the header bar
	static constexpr SDL_Color BAR_INK = { 32, 32, 32, 255 };     // and what is written on it

	// AND THE SAME PANEL STANDS AT THE END OF A VERSUS, which is Frederic's of the 2026-10-28: a decided
	// two-player match used to hand the player GAME OVER, a screen that belongs to a ladder nobody was
	// climbing. The lines are already the right four -- REMATCH, CHANGE STAGE, CHANGE YOUR VIPER, MAIN MENU
	// -- so what differs is three things and no more: RESUME is not shown (there is no fight to go back to),
	// the player cannot back out (same reason), and the bar says what this is instead of who paused.
	bool over = false;
	bool shown = false;
	int who = 0;                   // the player who paused: the panel stands on his side, 1P left, 2P right
	int sel = MAIN_MENU;
	bool ready = false, tried = false;
	SDL_Texture *cursor = nullptr;
	fv2_text *font = nullptr;     // the sheet, loaded once by the host and shared
	int stick_x = 0, stick_y = 0;

	// ANSWERED, AND HELD WHILE IT IS HEARD. Frederic's third complaint was that the menu did not respond:
	// MAIN MENU closed it and, for the three and a half seconds the match takes to end off screen,
	// nothing visibly happened -- so he pressed Start again, which paused the game, which stopped the very
	// thing he had asked for. So the chosen line now BLINKS while its sound plays, as a chosen line does
	// everywhere else in these surfaces, and only then is it handed to the host. The host's part -- ignoring
	// Start while a way out is running -- is in main.cpp.
	static constexpr double HOLD_S = 0.45;
	static constexpr double BLINK_S = 0.11;
	bool locked = false;
	double lock_t = 0;             // seconds since the answer, kept by the host, which owns the clock
	void lock() { locked = true; lock_t = 0; }

	bool load(SDL_Renderer *ren, const std::string &assets, fv2_text *sheet, FILE *log)
	{
		if (tried) return ready;
		tried = true;
		font = sheet;
		if (!font || !font->ready) {
			if (log) fprintf(log, "the console's pause menu: no glyph sheet -- Start cannot open it\n");
			return false;
		}
		// The same cursor rule as the OPTIONS page, read again rather than borrowed: each surface owns what
		// it draws, which is why either can be built or dropped without touching the other.
		const std::string path = assets + "/menu-console/option_cursor.png";
		std::vector<uint32_t> px;
		int w = 0, h = 0;
		std::string why;
		if (!read_png(path.c_str(), px, w, h, why)) {
			if (log) fprintf(log, "the console's pause menu: %s %s -- Start cannot open it\n", path.c_str(), why.c_str());
			return false;
		}
		cursor = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STATIC, w, h);
		if (!cursor) {
			if (log) fprintf(log, "the console's pause menu: no texture for %s (%s)\n", path.c_str(), SDL_GetError());
			return false;
		}
		SDL_UpdateTexture(cursor, nullptr, px.data(), w * 4);
		SDL_SetTextureBlendMode(cursor, SDL_BLENDMODE_BLEND);
		SDL_SetTextureScaleMode(cursor, SDL_SCALEMODE_LINEAR);
		ready = true;
		if (log) fprintf(log, "the console's pause menu: its cursor and the glyph sheet are in\n");
		return true;
	}

	void textures_lost(SDL_Renderer *ren, const std::string &assets)
	{
		if (cursor) { SDL_DestroyTexture(cursor); cursor = nullptr; }
		ready = false;
		tried = false;
		load(ren, assets, font, nullptr);
	}

	void show(int player)
	{
		if (!ready) return;
		shown = true;
		over = false;
		who = player ? 1 : 0;
		sel = RESUME;
		locked = false;
		lock_t = 0;
		stick_x = stick_y = 0;
		if (!shows(sel)) move(+1);
	}
	void hide() { shown = false; locked = false; lock_t = 0; }

	// The end-of-match panel: the four lines that command this match, and no way out but one of them.
	void show_over()
	{
		if (!ready) return;
		shown = true;
		over = true;
		who = 0;
		on = ALL & ~(1u << RESUME);
		sel = REMATCH;
		locked = false;
		lock_t = 0;
		stick_x = stick_y = 0;
	}

	void move(int d)
	{
		for (int i = 0; i < LINES; i++) {
			sel = (sel + LINES + d) % LINES;
			if (shows(sel)) break;
		}
		want_sound = SND_MOVE;
	}

	// One event while the menu is up. Returns a line (0..LINES-1) when one is chosen, CANCELLED when the
	// player backs out, -1 otherwise.
	int handle_event(const SDL_Event &e, const player_controls ctl[2])
	{
		if (!shown || locked) return -1;      // answered: the hold below belongs to the sound
		auto bound = [&](int action, SDL_Scancode sc) {
			for (int p = 0; p < 2; p++) if (ctl[p].key[action] == sc) return true;
			return false;
		};
		switch (e.type) {
		case SDL_EVENT_KEY_DOWN: {
			if (e.key.repeat) return -1;
			const SDL_Scancode sc = e.key.scancode;
			if (sc == SDL_SCANCODE_UP || bound(C_UP, sc)) move(-1);
			else if (sc == SDL_SCANCODE_DOWN || bound(C_DOWN, sc)) move(+1);
			// Start closes what Start opened. It is listed before A so that a player whose Start is also a
			// game button cannot choose a line with it by accident.
			else if (bound(C_START, sc)) { if (over) return -1; want_sound = SND_CANCEL; return CANCELLED; }
			else if (sc == SDL_SCANCODE_BACKSPACE || bound(C_KICK, sc)) { if (over) return -1; want_sound = SND_CANCEL; return CANCELLED; }
			else if (sc == SDL_SCANCODE_RETURN || sc == SDL_SCANCODE_KP_ENTER || sc == SDL_SCANCODE_SPACE
					 || bound(C_PUNCH, sc)) {
				if (!shows(sel)) return -1;
				want_sound = sel == RESUME ? SND_CANCEL : SND_VALIDATE;
				return sel;
			}
			return -1;
		}
		case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
			switch (e.gbutton.button) {
			case SDL_GAMEPAD_BUTTON_DPAD_UP: move(-1); break;
			case SDL_GAMEPAD_BUTTON_DPAD_DOWN: move(+1); break;
			case SDL_GAMEPAD_BUTTON_START: case SDL_GAMEPAD_BUTTON_EAST:
				if (over) return -1;
				want_sound = SND_CANCEL;
				return CANCELLED;
			case SDL_GAMEPAD_BUTTON_SOUTH:
				if (!shows(sel)) return -1;
				want_sound = sel == RESUME ? SND_CANCEL : SND_VALIDATE;
				return sel;
			default: break;
			}
			return -1;
		case SDL_EVENT_GAMEPAD_AXIS_MOTION: {
			const int dir = e.gaxis.value < -16000 ? -1 : e.gaxis.value > 16000 ? 1 : 0;
			if (e.gaxis.axis == SDL_GAMEPAD_AXIS_LEFTY) {
				if (dir && dir != stick_y) move(dir);
				stick_y = dir;
			}
			return -1;
		}
		default: return -1;
		}
	}

	// Over the picture the game made: the fight darkened, then the panel on the pauser's side. mid is the
	// game's own 4:3 screen, which every number above is a fraction of.
	void render(SDL_Renderer *ren, const SDL_FRect &all, const SDL_FRect &mid)
	{
		if (!shown || !ready || !font) return;
		SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
		SDL_SetRenderDrawColor(ren, 0, 0, 0, DIM_ALPHA);
		SDL_RenderFillRect(ren, &all);

		// THE BOX FOLLOWS ITS CONTENT, both ways. PANEL_W and PANEL_H were measured on the Dreamcast's
		// picture, which has four lines and none longer than REMATCH; CHANGE YOUR VIPER ran straight out
		// through the right edge, and a fifth line out through the bottom. So the height follows the lines
		// shown and the width follows the longest of them, never under what was measured.
		const float cap0 = ROW_CAP * mid.h, line_h0 = cap0 / fv2_text::CAP;
		int n = 0;
		float longest = 0;
		for (int i = 0; i < LINES; i++)
			if (shows(i)) { n++; longest = std::max(longest, font->width(name(i), line_h0)); }
		const float w = std::max(PANEL_W * mid.w, longest + 2.0f * TEXT_IN * mid.w);
		const float h = (PANEL_H + float(n - 4) * PITCH) * mid.h;
		const float x = who ? mid.x + mid.w * (1.0f - PANEL_MARGIN) - w : mid.x + mid.w * PANEL_MARGIN;
		const float y = mid.y + PANEL_TOP * mid.h;
		const float bar = BAR_H * mid.h;

		// The header bar, then the box under it. The bar is drawn first so the box's edge sits against it.
		const SDL_FRect bar_r = { x, y - bar, w, bar };
		SDL_SetRenderDrawColor(ren, BAR.r, BAR.g, BAR.b, BAR.a);
		SDL_RenderFillRect(ren, &bar_r);
		const SDL_FRect box = { x, y, w, h };
		SDL_SetRenderDrawColor(ren, PANEL.r, PANEL.g, PANEL.b, PANEL.a);
		SDL_RenderFillRect(ren, &box);
		// The pale upright the Dreamcast draws down the panel's left edge.
		const SDL_FRect edge = { x - bar * 0.09f, y - bar, bar * 0.09f, h + bar };
		SDL_SetRenderDrawColor(ren, BAR.r, BAR.g, BAR.b, BAR.a);
		SDL_RenderFillRect(ren, &edge);

		// "1P PAUSE" on the bar, in its own ink: the player first, hard against the left, then the word.
		const float bar_cap = bar * 0.56f, bar_h = bar_cap / fv2_text::CAP;
		const char *side = over ? "VS" : who ? "2P" : "1P";
		font->draw(ren, side, x + TEXT_IN * mid.w * 0.5f, y - bar + bar * 0.20f, bar_h, BAR_INK);
		font->draw(ren, over ? "BATTLE OVER" : "PAUSE", x + w * 0.30f, y - bar + bar * 0.20f, bar_h, BAR_INK);

		// The lines this mode shows, left-aligned inside the box as they are there, and only those: a line
		// that means nothing here is not greyed out, it is absent, and the ones below it move up. August
		// kept his four in place because all four were his; these differ from mode to mode, and a list with
		// holes in it reads as a list of things that are broken.
		const float cap = ROW_CAP * mid.h, line_h = cap / fv2_text::CAP;
		int row = 0;
		for (int i = 0; i < LINES; i++) {
			if (!shows(i)) continue;
			// Once answered the chosen line blinks, which is how a chosen line says so everywhere else here
			const bool off = i == sel && locked && int(lock_t / BLINK_S) % 2 == 1;
			const SDL_Color col = i == sel ? (off ? PLAIN : CHOSEN) : PLAIN;
			const float ly = y + (ROW_TOP + float(row++) * PITCH) * mid.h;
			font->draw(ren, name(i), x + TEXT_IN * mid.w, ly, line_h, col);
		}

		// THE TWO HINTS ARE GONE. The Dreamcast splits them along the bottom of its own menus and this one
		// followed; Frederic, the 2026-10-29: "enleve ce texte en bas d'ecran sur tous les ecrans qui le
		// propose". A player holding the pad that opened this menu knows which button decides.
	}

private:
};
