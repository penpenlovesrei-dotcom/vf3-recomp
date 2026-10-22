// Fighting Vipers 2: the console's OPTIONS page, drawn over the game's own picture.
//
// Like fv2_mode_list.h and for the same reason, this is a surface of its own and goes nowhere near game_menu:
// the menu under Escape is this port's settings, and a page added to it is a page of settings whatever is
// written in it. What it edits, though, is not this port's anything -- it is the arcade's GAME ASSIGNMENTS,
// written in BOTH the places they live: the backup RAM, which is what makes them LAST across a restart -- and
// which nobody had decoded, docs/fv2-console-options.md says how it was found and proved -- and the working
// copy the running game reads, which is what makes them TAKE EFFECT at once. Writing only the backup means
// restarting the game to have it read, which is what this port's Arcade settings page does for Virtua Fighter
// 3; on a console's options screen that reads as a crash rather than as a setting, the picture going black and
// the window sitting silent while the board boots again.
//
// EVERY NUMBER OF THE LAYOUT IS MEASURED, off a Flycast capture of the Dreamcast version's own OPTIONS screen
// (docs/fv2-dreamcast-menus.md section 3bis), in fractions of the 4:3 picture:
//
//   - the background is option_bg.png AND IT IS NOT DIMMED. Four regions free of interface, against that file
//     put at the same size, come out at ratios of 1.11 to 1.28 -- that is the file itself, to the emulator's
//     gamma. The half measured for the list of modes applies to the game's LIVE picture; this backdrop is a
//     prepared image, already dark by construction, and dimming it again counts the same thing twice;
//   - the labels are flush left at 0.3136 and the values are CENTRED on 0.665, not ranged right;
//   - the chosen line is marked BY COLOUR -- its ink is (252, 252, 10) where the others are (247, 246, 246) --
//     with option_cursor.png's rule under it, from 0.2913 to 0.7295. The rule does not cross the screen, and
//     nothing blinks: the blink in fv2_mode_list.h is the owner's Supermodel menu, not the disc;
//   - the settings rows lean 7.5 degrees and the title does not, and the title is a condensed bold. Both are
//     done in fv2_texte.h, which says how.
//
// TWO THINGS THE DISC DOES THAT THIS PAGE DOES NOT, said here so nobody takes the resemblance for identity.
// Its OPTIONS is not the arcade's thirteen: it shows seven settings, three sub-screens (SOUNDS, CONTROLLER,
// RESULT) and INITIALIZE, with a panel on the right explaining the current entry. And it holds seven lines
// where this holds thirteen, so its row pitch of 0.0584 does not fit and ours is tightened -- the one number
// of this layout that is not off the capture.
#pragma once
#include <SDL3/SDL.h>
#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>
#include "../machine/model3.h"
#include "controls.h"
#include "fv2_texte.h"
#include "png.h"

struct fv2_options
{
	// One setting: where it lives, how wide, what it may hold, and how its value reads. The addresses are the
	// WORKING COPY's, which is how they were measured and how MenuConsole.md names them; Model3 turns each into
	// its place in the backup RAM. The ranges are the test menu's own -- a direct write is bounded by nothing,
	// so they are the only guard.
	struct setting {
		const char *name;
		uint32_t live;
		int bytes, lo, hi, step;
		const char *const *labels;   // or null: then the number itself
	};
	// DIFFICULTY's five names are a RECONSTRUCTION, not a reading: what is measured is that it runs 0 to 4 and
	// that 2 is NORMAL, and the Dreamcast's capture shows VERY EASY at one end. The rest follows from that and
	// could be wrong in its wording, never in which byte it writes.
	static constexpr const char *DIFFICULTY[] = { "VERY EASY", "EASY", "NORMAL", "HARD", "VERY HARD" };
	static constexpr const char *ONOFF[] = { "OFF", "ON" };
	static constexpr const char *COUNTRY[] = { "JAPAN", "U.S.A.", "EXPORT", "ASIA" };

	// In MenuConsole.md section 9's order, with COUNTRY moved last because it is the one the game will not take
	// from us (it is written where the settings are read, not here) and it reads better out of the way.
	static const std::vector<setting> &table()
	{
		static const std::vector<setting> T = {
			{ "MATCH COUNT 1P",  0x12054C, 1,   2,   5,  1, nullptr },
			{ "MATCH COUNT VS",  0x12054D, 1,   2,   5,  1, nullptr },
			{ "ROUND TIME 1P",   0x120555, 1,  20,  60,  5, nullptr },
			{ "ROUND TIME VS",   0x120556, 1,  20,  60,  5, nullptr },
			{ "DIFFICULTY",      0x12054E, 1,   0,   4,  1, DIFFICULTY },
			{ "ENERGY MAX 1P",   0x120540, 2, 160, 360, 10, nullptr },
			{ "ENERGY MAX VS",   0x120542, 2, 160, 360, 10, nullptr },
			{ "ADVERTISE SOUND", 0x12054F, 1,   0,   1,  1, ONOFF },
			{ "SURVIVAL MODE",   0x120550, 1,   0,   1,  1, ONOFF },
			{ "SUPER K.O.",      0x120554, 1,   0,   1,  1, ONOFF },
			{ "VS FINISH",       0x120558, 1,   0,   1,  1, ONOFF },
			{ "BIKE LOGO",       0x12055A, 1,   0,   1,  1, ONOFF },
			{ "COUNTRY",         0x120557, 1,   0,   3,  1, COUNTRY },
		};
		return T;
	}
	// The factory values, measured twice over: read out of the game's own table at 0x00120D24 in a RAM dump,
	// and found again in the owner's saved backup RAM. INITIALIZE writes these.
	static const std::vector<int> &factory()
	{
		static const std::vector<int> F = { 2, 3, 30, 45, 2, 210, 210, 1, 0, 1, 0, 1, 0 };
		return F;
	}
	static bool read_only(size_t i) { return table()[i].name == std::string("COUNTRY"); }

	// the geometry, in fractions of the game's 4:3 picture (above)
	static constexpr float TITLE_TOP = 0.0996f, TITLE_CAP = 0.0442f;
	static constexpr float LABEL_X = 0.3136f, VALUE_MID = 0.665f;
	static constexpr float RULE_L = 0.2913f, RULE_R = 0.7295f;
	static constexpr float ROW_CAP = 0.0292f;
	// The band the lines live in. The disc puts seven settings at a pitch of 0.0584 from y 0.2128; this page
	// carries eighteen lines -- the thirteen, four doors and INITIALIZE -- so the band is taken as wide as the
	// page allows and the pitch falls out of it. August's rule, and its warning, are on render_options.
	static constexpr float ROWS_TOP = 0.172f, ROWS_BOTTOM = 0.845f;
	static constexpr float INIT_TOP = 0.848f, INIT_HEIGHT = 0.0242f;
	static constexpr float FOOT1 = 0.902f, FOOT2 = 0.940f;

	static constexpr SDL_Color CHOSEN = { 252, 252, 10, 255 };    // the disc's yellow, measured
	static constexpr SDL_Color PLAIN  = { 247, 246, 246, 255 };   // and its white
	static constexpr SDL_Color MUTED  = { 150, 150, 150, 255 };   // COUNTRY: ours, not the disc's

	static constexpr int CANCELLED = -2;   // what handle_event returns when the player backed out

	// ---- the tree, rebuilt from the summer's own ----
	//
	// August 2026's OPTIONS was not a flat page of settings, and neither is the Dreamcast's: both carry the
	// settings AND a short list of doors. Read back from
	// Documents/SEGA Model 3/Sauvegardes/2026-08-20_23h04/Src/OSD/SDL/Main.cpp (OptionsEntries / OptionLines),
	// its page was, in this order: the thirteen settings as name and value, then DISPLAY, SOUNDS, CONTROLLER
	// and STATS -- drawn BOLD AND CENTRED "parce qu'elles mènent quelque part plutôt que de régler quelque
	// chose" -- then INITIALIZE as a picture. The gaps are its own: half a line before the first door, and 1.2
	// before INITIALIZE.
	//
	// The Dreamcast's four are SOUNDS, CONTROLLER, RESULT and INITIALIZE. DISPLAY and STATS are the fork's own,
	// and RESULT is the disc's; we follow August, which is what was asked.
	//
	// What is behind each door, honestly: nothing yet, and that is the next piece of work rather than a
	// pretence. CONTROLLER was title-only in August too -- "LA PAGE CONTROLLER, VIDE POUR L'INSTANT. Demandée
	// le 2026-08-18 : le titre et rien d'autre" -- so for that one the tree is already complete. DISPLAY held
	// WIDESCREEN, HD ART, FPS COUNTER and ENDING; SOUNDS the sound test, the twenty musics, the two volumes and
	// the pack; STATS the VS DIAGRAM drawn from the backup's counters.
	enum screen { SCR_OPTIONS, SCR_DISPLAY, SCR_SOUNDS, SCR_CONTROLLER, SCR_STATS, SCR_COUNT };
	static const char *screen_name(int s)
	{
		static const char *const N[SCR_COUNT] = { "OPTIONS", "DISPLAY", "SOUNDS", "CONTROLLER", "STATS" };
		return s >= 0 && s < SCR_COUNT ? N[s] : "?";
	}
	// A line of the OPTIONS page: a setting, a door, or the INITIALIZE button.
	struct line {
		enum kind { SETTING, DOOR, BUTTON } what;
		int which;        // the setting's index, or the screen a door opens
		float gap;        // blank space before it, in lines
	};
	static const std::vector<line> &page()
	{
		static const std::vector<line> P = [] {
			std::vector<line> p;
			for (int i = 0; i < int(table().size()); i++) p.push_back({ line::SETTING, i, 0 });
			p.push_back({ line::DOOR, SCR_DISPLAY, 0.5f });
			p.push_back({ line::DOOR, SCR_SOUNDS, 0 });
			p.push_back({ line::DOOR, SCR_CONTROLLER, 0 });
			p.push_back({ line::DOOR, SCR_STATS, 0 });
			p.push_back({ line::BUTTON, 0, 1.2f });
			return p;
		}();
		return P;
	}

	bool shown = false;
	int at = SCR_OPTIONS;        // the screen now up; B goes back up the tree, as August's back stack does
	int sel = 0;
	bool ready = false, tried = false;
	bool changed = false;        // something was really written, as against stepped there and back
	SDL_Texture *bg = nullptr, *cursor = nullptr, *init = nullptr;
	int init_w = 0, init_h = 0;
	int stick_x = 0, stick_y = 0;
	fv2_texte *font = nullptr;   // the sheet, loaded once by the host and shared

	// The backdrop is authored 16:9 with the 4:3 picture inset in it and flat purple outside; only the inset is
	// drawn. Its bounds were measured on the file: the purple runs to x 447 and from x 3183.
	static constexpr int BG_W = 3630, BG_H = 2048, BG_L = 447, BG_R = 3183;

	bool load(SDL_Renderer *ren, const std::string &assets, fv2_texte *sheet, FILE *log)
	{
		if (tried) return ready;
		tried = true;
		font = sheet;
		if (!font || !font->ready) {
			if (log) fprintf(log, "the console's OPTIONS: no glyph sheet -- the page stays shut\n");
			return false;
		}
		auto one = [&](const char *name, SDL_Texture *&into, int *w, int *h, int want_w, int want_h) {
			const std::string path = assets + "/menu-console/" + name;
			std::vector<uint32_t> px;
			int iw = 0, ih = 0;
			std::string why;
			if (!read_png(path.c_str(), px, iw, ih, why)) {
				if (log) fprintf(log, "the console's OPTIONS: %s %s -- the page stays shut\n", path.c_str(), why.c_str());
				return false;
			}
			if (want_w && (iw != want_w || ih != want_h)) {
				if (log) fprintf(log, "the console's OPTIONS: %s is %dx%d, and the geometry was measured on %dx%d -- the page stays shut\n",
								 path.c_str(), iw, ih, want_w, want_h);
				return false;
			}
			into = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STATIC, iw, ih);
			if (!into) {
				if (log) fprintf(log, "the console's OPTIONS: no texture for %s (%s)\n", path.c_str(), SDL_GetError());
				return false;
			}
			SDL_UpdateTexture(into, nullptr, px.data(), iw * 4);
			SDL_SetTextureBlendMode(into, SDL_BLENDMODE_BLEND);
			SDL_SetTextureScaleMode(into, SDL_SCALEMODE_LINEAR);
			if (w) *w = iw;
			if (h) *h = ih;
			return true;
		};
		if (!one("option_bg.png", bg, nullptr, nullptr, BG_W, BG_H)) return false;
		if (!one("option_cursor.png", cursor, nullptr, nullptr, 0, 0)) return false;
		if (!one("option_initialize.png", init, &init_w, &init_h, 0, 0)) return false;
		ready = true;
		if (log) fprintf(log, "the console's OPTIONS: its three pictures and the glyph sheet are in\n");
		return true;
	}

	void textures_lost(SDL_Renderer *ren, const std::string &assets)
	{
		for (SDL_Texture **t : { &bg, &cursor, &init })
			if (*t) { SDL_DestroyTexture(*t); *t = nullptr; }
		ready = false;
		tried = false;
		load(ren, assets, font, nullptr);
	}

	void show() { if (ready) { shown = true; at = SCR_OPTIONS; sel = 0; stick_x = stick_y = 0; } }
	void hide() { shown = false; at = SCR_OPTIONS; }

	int lines() const { return at == SCR_OPTIONS ? int(page().size()) : 0; }
	void move(int d) { if (lines()) sel = (sel + lines() + d) % lines(); }

	// What a line reads as now. -1 means the backup does not hold a usable value, and then the line shows "-"
	// and will not step: better a dash than a number nobody can account for.
	int value(m3::Model3 &board, size_t i) const
	{
		const setting &s = table()[i];
		const int v = board.fv2_option(int(s.live), s.bytes);
		return v < s.lo || v > s.hi ? -1 : v;
	}
	std::string value_text(m3::Model3 &board, size_t i) const
	{
		const int v = value(board, i);
		if (v < 0) return "-";
		const setting &s = table()[i];
		return s.labels ? std::string(s.labels[v - s.lo]) : std::to_string(v);
	}
	void step(m3::Model3 &board, size_t i, int d)
	{
		if (read_only(i)) return;
		const setting &s = table()[i];
		const int v = value(board, i);
		if (v < 0) return;
		const int n = std::clamp(v + d * s.step, s.lo, s.hi);
		if (n != v && board.fv2_set_option(int(s.live), s.bytes, n)) changed = true;
	}
	void initialize(m3::Model3 &board)
	{
		for (size_t i = 0; i < table().size(); i++)
			if (board.fv2_set_option(int(table()[i].live), table()[i].bytes, factory()[i])) changed = true;
	}

	// One event while the page is up. Up and down move, left and right change, the pad's A or Start acts on
	// INITIALIZE, the pad's B or Backspace backs out -- which is what the Dreamcast tells the player at the
	// foot of this very screen.
	int handle_event(const SDL_Event &e, const player_controls ctl[2], m3::Model3 &board)
	{
		if (!shown) return -1;
		auto bound = [&](int action, SDL_Scancode sc) {
			for (int p = 0; p < 2; p++) if (ctl[p].key[action] == sc) return true;
			return false;
		};
		// Choosing: a door opens its screen, INITIALIZE puts the factory values back, a setting does nothing
		// (it is changed with left and right).
		auto act = [&] {
			if (at != SCR_OPTIONS || sel < 0 || sel >= lines()) return -1;
			const line &l = page()[size_t(sel)];
			if (l.what == line::BUTTON) initialize(board);
			else if (l.what == line::DOOR) at = l.which;
			return -1;
		};
		// Backing out goes UP ONE SCREEN, which is August's own back stack rather than aiming at the modes
		// list by hand. From OPTIONS itself there is nowhere above, so it closes the page.
		auto back = [&] {
			if (at == SCR_OPTIONS) return CANCELLED;
			at = SCR_OPTIONS;
			return -1;
		};
		auto sideways = [&](int d) {
			if (at != SCR_OPTIONS || sel < 0 || sel >= lines()) return;
			const line &l = page()[size_t(sel)];
			if (l.what == line::SETTING) step(board, size_t(l.which), d);
		};
		switch (e.type) {
		case SDL_EVENT_KEY_DOWN: {
			if (e.key.repeat) return -1;
			const SDL_Scancode sc = e.key.scancode;
			if (sc == SDL_SCANCODE_UP || bound(C_UP, sc)) move(-1);
			else if (sc == SDL_SCANCODE_DOWN || bound(C_DOWN, sc)) move(+1);
			else if (sc == SDL_SCANCODE_LEFT || bound(C_LEFT, sc)) sideways(-1);
			else if (sc == SDL_SCANCODE_RIGHT || bound(C_RIGHT, sc)) sideways(+1);
			else if (sc == SDL_SCANCODE_RETURN || sc == SDL_SCANCODE_KP_ENTER || sc == SDL_SCANCODE_SPACE
					 || bound(C_START, sc) || bound(C_PUNCH, sc))
				return act();
			else if (sc == SDL_SCANCODE_BACKSPACE || bound(C_KICK, sc))
				return back();
			return -1;
		}
		case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
			switch (e.gbutton.button) {
			case SDL_GAMEPAD_BUTTON_DPAD_UP: move(-1); break;
			case SDL_GAMEPAD_BUTTON_DPAD_DOWN: move(+1); break;
			case SDL_GAMEPAD_BUTTON_DPAD_LEFT: sideways(-1); break;
			case SDL_GAMEPAD_BUTTON_DPAD_RIGHT: sideways(+1); break;
			case SDL_GAMEPAD_BUTTON_START: case SDL_GAMEPAD_BUTTON_SOUTH: return act();
			case SDL_GAMEPAD_BUTTON_EAST: return back();
			default: break;
			}
			return -1;
		case SDL_EVENT_GAMEPAD_AXIS_MOTION: {
			const int dir = e.gaxis.value < -16000 ? -1 : e.gaxis.value > 16000 ? 1 : 0;
			if (e.gaxis.axis == SDL_GAMEPAD_AXIS_LEFTY) {
				if (dir && dir != stick_y) move(dir);
				stick_y = dir;
			} else if (e.gaxis.axis == SDL_GAMEPAD_AXIS_LEFTX) {
				if (dir && dir != stick_x) sideways(dir);
				stick_x = dir;
			}
			return -1;
		}
		default: return -1;
		}
	}

	// Over the picture the game made. `mid` is the game's own 4:3 screen in the window's pixels, which is what
	// every fraction above was measured against. Nothing is dimmed: see the top.
	void render(SDL_Renderer *ren, const SDL_FRect &mid, m3::Model3 &board)
	{
		if (!shown || !ready || !font) return;
		const SDL_FRect src = { float(BG_L), 0, float(BG_R - BG_L), float(BG_H) };
		SDL_RenderTexture(ren, bg, &src, &mid);

		font->draw_title(ren, screen_name(at), mid.x + mid.w * 0.5f, mid.y + TITLE_TOP * mid.h,
						 TITLE_CAP * mid.h, PLAIN);

		if (at == SCR_OPTIONS) render_options(ren, mid, board);
		else render_child(ren, mid);

		// The foot the Dreamcast puts on this very screen, in three lines there and two here.
		const float fh = foot_cell(mid);
		font->draw_mid(ren, "USE THE DIRECTION BUTTON TO SELECT", mid.x + mid.w * 0.5f, mid.y + FOOT1 * mid.h, fh, PLAIN);
		font->draw_mid(ren, "A : DECIDE        B : BACK", mid.x + mid.w * 0.5f, mid.y + FOOT2 * mid.h, fh, PLAIN);
	}

private:
	float foot_cell(const SDL_FRect &mid) const { return ROW_CAP * mid.h / fv2_texte::CAP * 0.62f; }

	// A screen behind a door. Nothing is drawn but its title and the way out -- which is exactly what August's
	// CONTROLLER page was, and all that is honest for the other three until their contents are built. A page
	// that pretended to hold something would be worse than one that says it is empty.
	void render_child(SDL_Renderer *ren, const SDL_FRect &mid)
	{
		const float h = ROW_CAP * mid.h / fv2_texte::CAP;
		static const char *const SOON[SCR_COUNT] = {
			"", "WIDESCREEN, HD ART, FPS COUNTER, ENDING", "SOUND TEST, MUSIC, VOLUMES, PACK",
			"", "THE VS DIAGRAM",
		};
		const char *soon = at >= 0 && at < SCR_COUNT ? SOON[at] : "";
		if (*soon) font->draw_mid(ren, soon, mid.x + mid.w * 0.5f, mid.y + 0.45f * mid.h, h * 0.8f, MUTED);
	}

	// The OPTIONS page: the thirteen settings, then the doors, then INITIALIZE.
	//
	// A PAGE TOO LONG LOOKS FOR ROOM BEFORE IT SHRINKS, which is August's rule and was paid for there: raise
	// the band first and only then let the text give, "sinon toute augmentation de la police était annulée par
	// le resserrement -- deux effets qui s'annulent exactement, et une correction qui semble ne rien faire".
	// So the band is fixed here and the pitch falls out of it; the cap follows the pitch, capped at the
	// Dreamcast's own so a short page never grows past the disc.
	void render_options(SDL_Renderer *ren, const SDL_FRect &mid, m3::Model3 &board)
	{
		const std::vector<line> &P = page();
		float slots = 0;
		for (size_t i = 0; i < P.size(); i++) slots += (i ? 1.0f : 0.0f) + P[i].gap;
		const float pitch = (ROWS_BOTTOM - ROWS_TOP) * mid.h / (slots > 0 ? slots : 1);
		const float cap = std::min(ROW_CAP * mid.h, pitch * 0.50f);
		const float h = cap / fv2_texte::CAP;

		float ink_top = mid.y + ROWS_TOP * mid.h;
		for (size_t i = 0; i < P.size(); i++) {
			const line &l = P[i];
			ink_top += (i ? pitch : 0) + l.gap * pitch;
			const float y = ink_top - fv2_texte::CAP_TOP * h;   // the cell's top, the ink starting at 16/64
			const bool on = int(i) == sel;
			if (l.what == line::BUTTON) {
				// INITIALIZE is a painted label, not text, and it is the game's own idea: 0x00020F74 puts the
				// factory values back and 0x00021008 writes them to the backup. This does the same by the table.
				const float bh = cap * 1.45f;
				const float bw = bh * float(init_w) / float(init_h ? init_h : 1);
				const SDL_FRect dst = { mid.x + mid.w * 0.5f - bw * 0.5f, ink_top, bw, bh };
				if (on) rule(ren, mid, dst.y + bh * 1.30f, h);
				SDL_SetTextureAlphaMod(init, uint8_t(on ? 255 : 190));
				SDL_RenderTexture(ren, init, nullptr, &dst);
				SDL_SetTextureAlphaMod(init, 255);
				continue;
			}
			// the rule's line just under the capitals, which end at (16 + 31) / 64 of the cell
			if (on) rule(ren, mid, y + h * ((16.0f + 31.0f) / fv2_texte::CELL + 0.24f), h);
			if (l.what == line::DOOR) {
				// The doors are drawn BOLD AND CENTRED, as August drew them, "parce qu'elles mènent quelque
				// part plutôt que de régler quelque chose". Bold here is the title's own thickening, without
				// the condensing: the sheet has one weight and this is how it gets another.
				const float w = font->width(screen_name(l.which), h, 1.0f, fv2_texte::THICK);
				font->draw(ren, screen_name(l.which), mid.x + mid.w * 0.5f - w * 0.5f, y, h,
						   on ? CHOSEN : PLAIN, 255, fv2_texte::SLANT, 1.0f, fv2_texte::THICK);
				continue;
			}
			const size_t k = size_t(l.which);
			const SDL_Color col = on ? CHOSEN : (read_only(k) ? MUTED : PLAIN);
			font->draw(ren, table()[k].name, mid.x + LABEL_X * mid.w, y, h, col, 255, fv2_texte::SLANT);
			font->draw_mid(ren, value_text(board, k).c_str(), mid.x + VALUE_MID * mid.w, y, h, col, 255,
						   fv2_texte::SLANT);
		}
	}

	// The rule under the chosen line. `bottom` is where its VISIBLE LINE goes, and the picture is HUNG FROM
	// THERE rather than placed from its top. That is the one thing to know about this image, and it cost the
	// owner a session in August: option_cursor.png carries a small vertical tick that RISES ABOVE the line, so
	// the line itself is at the picture's bottom. Placed from the top, it underlines the next row -- which is
	// exactly what the first draft here did, and which was "fixed" by nudging a constant until it looked
	// right. That is not the same as fixing it: at another size it would have come back.
	//
	// It is also stretched to the column and SQUASHED in height: at its own proportions (1005 x 50) it would
	// stand taller than a row and bite into the next one. It stays a rule; only its tick flattens.
	void rule(SDL_Renderer *ren, const SDL_FRect &mid, float bottom, float h)
	{
		const float rh = h * 0.26f;
		const SDL_FRect dst = { mid.x + RULE_L * mid.w, bottom - rh, (RULE_R - RULE_L) * mid.w, rh };
		SDL_RenderTexture(ren, cursor, nullptr, &dst);
	}
};
