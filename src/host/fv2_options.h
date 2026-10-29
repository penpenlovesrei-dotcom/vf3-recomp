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
//     done in fv2_text.h, which says how.
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
#include <functional>
#include <string>
#include <vector>
#include "../machine/model3.h"
#include "controls.h"
#include "fv2_text.h"
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
	// The doors of the OPTIONS page are the title's own lettering, a little smaller: 78 % of its capitals,
	// which is above the settings' 0.0292 and below the title, so the page reads as one hand's work.
	// Frederic, the 2026-10-29: "utilise la meme police que le titre OPTIONS en un peu plus petit".
	static constexpr float DOOR_CAP = TITLE_CAP * 0.78f;
	static constexpr float LABEL_X = 0.3136f, VALUE_MID = 0.665f;
	static constexpr float RULE_L = 0.2913f, RULE_R = 0.7295f;
	static constexpr float ROW_CAP = 0.0292f;
	// The band the lines live in. The disc puts seven settings at a pitch of 0.0584 from y 0.2128; this page
	// carries fourteen on its longest screen, so the band is taken as wide as the page allows and the pitch is
	// as much of it as the lines want, never past the disc's. August's rule is on render_lines.
	static constexpr float ROWS_TOP = 0.172f, ROWS_BOTTOM = 0.845f;
	static constexpr float DISC_PITCH = 0.0584f;   // the Dreamcast's own, measured
	static constexpr float INIT_TOP = 0.848f, INIT_HEIGHT = 0.0242f;
	static constexpr float FOOT1 = 0.902f, FOOT2 = 0.940f;

	static constexpr SDL_Color CHOSEN = { 252, 252, 10, 255 };    // the disc's yellow, measured
	static constexpr SDL_Color PLAIN  = { 247, 246, 246, 255 };   // and its white
	static constexpr SDL_Color MUTED  = { 150, 150, 150, 255 };   // COUNTRY: ours, not the disc's

	static constexpr int CANCELLED = -2;   // what handle_event returns when the player backed out

	// What the last event should be heard as. The surface decides WHAT happened; the host, which owns the
	// sound board, decides what that sounds like and when it can be played. Read and cleared by the host.
	enum { SND_NONE, SND_MOVE, SND_VALIDATE, SND_CANCEL };
	int want_sound = SND_NONE;

	// ---- the tree, rebuilt from the summer's own ----
	//
	// August 2026's OPTIONS was not a flat page of settings, and neither is the Dreamcast's: both carry the
	// settings AND a short list of doors. Read back from
	// Documents/SEGA Model 3/Sauvegardes/2026-08-20_23h04/Src/OSD/SDL/Main.cpp (OptionsEntries / OptionLines),
	// its page was, in this order: the thirteen settings as name and value, then DISPLAY, SOUNDS, CONTROLLER
	// and STATS -- drawn BOLD AND CENTRED "because they lead somewhere rather than setting something" --
	// then INITIALIZE as a picture. The gaps are its own: half a line before the first door, and 1.2
	// before INITIALIZE.
	//
	// The Dreamcast's four are SOUNDS, CONTROLLER, RESULT and INITIALIZE. DISPLAY and STATS are the fork's own,
	// and RESULT is the disc's; we follow August, which is what was asked.
	//
	// What is behind each door, honestly: nothing yet, and that is the next piece of work rather than a
	// pretence. CONTROLLER was title-only in August too -- "THE CONTROLLER PAGE, EMPTY FOR NOW. Asked for on
	// 2026-08-18: the title and nothing else" -- so for that one the tree is already complete. DISPLAY held
	// WIDESCREEN, HD ART, FPS COUNTER and ENDING; SOUNDS the sound test, the twenty musics, the two volumes and
	// the pack; STATS the VS DIAGRAM drawn from the backup's counters.
	//
	// ONE DELIBERATE DIVERGENCE FROM AUGUST, decided by Frédéric on the 2026-10-22. There, the thirteen
	// settings sat on the OPTIONS page itself, which made it eighteen lines long and drove the capitals down
	// to 60 % of the Dreamcast's -- the very density his own notes had predicted could not work. They now sit
	// behind a GAME ASSIGNMENTS door of their own, which is the first of the two ways out he had written down,
	// and which brings OPTIONS back to five lines.
	//
	// INITIALIZE MOVED WITH THEM, and that part is this port's own reasoning: it puts back the factory values
	// of those thirteen and nothing else, so on a page that no longer shows them it would be a button whose
	// effect cannot be seen. August and the disc both keep it at the foot of OPTIONS; they could, because the
	// settings were there.
	// AND THE PORT'S OWN PAGE IS GONE. SCR_GAME held the play style, the ending, the reset and the way out --
	// the lines the port's menu under Escape had and this tree did not, picked by Frederic the 2026-10-27.
	// The 2026-10-29 he moved the two settings onto the pages where they read as settings (PLAY STYLE to
	// DISPLAY, ENDING to GAME ASSIGNMENTS) and took the page away: "ligne et page GAME a supprimer". What
	// was left were two buttons that set nothing, and they now sit at the foot of OPTIONS itself, under a
	// gap -- which is where the disc puts the one button it has, and the only place a pad can still reach
	// them: Escape's menu is the keyboard's.
	enum screen { SCR_OPTIONS, SCR_SETTINGS, SCR_DISPLAY, SCR_SOUNDS, SCR_CONTROLLER, SCR_STATS, SCR_COUNT };
	static const char *screen_name(int s)
	{
		static const char *const N[SCR_COUNT] = { "OPTIONS", "GAME ASSIGNMENTS", "DISPLAY", "SOUNDS",
												  "CONTROLLER", "STATS" };
		return s >= 0 && s < SCR_COUNT ? N[s] : "?";
	}
	// A line of a screen: a setting of the game's, one of this port's own, a door, or the INITIALIZE button.
	struct line {
		enum kind { SETTING, OWN, DOOR, BUTTON } what;
		int which;        // the setting's index, the own line's index, or the screen a door opens
		float gap;        // blank space before it, in lines
	};

	// A line that belongs to THIS PORT rather than to the game: what DISPLAY and SOUNDS are made of. The host
	// fills these in, because they read and write its own settings and this file has no business knowing them.
	//
	// A line whose `step` is null is DRAWN AND STEPPED OVER, which is August's own idiom and its reason: "a
	// line with nothing to toggle is stepped over rather than taken away: a page whose lines slide up and down
	// with what is there would read badly at a glance". That is how the lines this port cannot do yet
	// keep their place without pretending.
	struct own_line {
		std::string name;
		std::function<std::string()> text;   // the value as text, or null for none
		std::function<void(int)> step;       // left and right; null, with no act either: stepped over
		std::function<void()> act;           // A as well as left and right: a button, or starting a test
		std::function<int()> lit;            // a gauge instead of a value: how many arrows are lit
		int total = 0;                       // how many arrows in all
		bool as_button = false;              // drawn as INITIALIZE's painted label rather than as text
		float gap = 0;
		std::function<bool()> cancel;        // B goes to the line first, which keeps it by returning true
		// A LINE THAT SPEAKS FOR ITSELF WANTS NO MENU EFFECT. Starting a track and stopping it are heard;
		// the menu's own "decided" and "backed out" played over the first note and over the silence, which
		// Frederic asked to go the 2026-10-25. It is the line that knows, not the page.
		bool silent = false;
	};
	std::vector<own_line> own[SCR_COUNT];

	const std::vector<line> &page(int s) const { return built[s]; }
	std::vector<line> built[SCR_COUNT];
	void build_pages()
	{
		for (int s = 0; s < SCR_COUNT; s++) built[s].clear();
		built[SCR_OPTIONS] = {
			{ line::DOOR, SCR_SETTINGS, 0 }, { line::DOOR, SCR_DISPLAY, 0 }, { line::DOOR, SCR_SOUNDS, 0 },
			{ line::DOOR, SCR_CONTROLLER, 0 }, { line::DOOR, SCR_STATS, 0 },
		};
		for (int i = 0; i < int(table().size()); i++) built[SCR_SETTINGS].push_back({ line::SETTING, i, 0 });
		for (int s = 0; s < SCR_COUNT; s++)
			for (int i = 0; i < int(own[s].size()); i++)
				built[s].push_back({ line::OWN, i, own[s][size_t(i)].gap });
		// INITIALIZE LAST, AFTER THIS PORT'S OWN LINES. It used to be pushed before them, which was right
		// while GAME ASSIGNMENTS was nothing but the game's thirteen settings; ENDING moved here the
		// 2026-10-29 and would have sat under the button, which is where a page ends.
		built[SCR_SETTINGS].push_back({ line::BUTTON, 0, 1.2f });   // August's own gap before the button
		// AND ONE AT THE FOOT OF OPTIONS, which is where the disc and August both keep it. It left this page
		// the 2026-10-22, when the thirteen settings went behind a door and a button whose effect could not
		// be seen stayed behind; Frederic put it back the 2026-10-29 -- "supprime RESET GAME et remet
		// l'image" -- and it does from here exactly what it does down there: the factory values of those
		// thirteen, which is the game's own idea of initialising and the only one the label stands for.
		built[SCR_OPTIONS].push_back({ line::BUTTON, 0, 1.2f });
	}

	bool shown = false;
	int at = SCR_OPTIONS;        // the screen now up; B goes back up the tree, as August's back stack does
	// One cursor per screen, so coming back up finds the door you went through rather than the first line.
	int sel_of[SCR_COUNT] = { 0 };
	bool ready = false, tried = false;
	bool changed = false;        // something was really written, as against stepped there and back
	SDL_Texture *bg = nullptr, *cursor = nullptr, *init = nullptr;
	SDL_Texture *vol_on = nullptr, *vol_off = nullptr, *vs_table = nullptr;
	// THE TWO TABLES ARE A STACK, NOT A CHOICE, which is August's own word for it: the whole diagram, and
	// over it the page of the character the cursor is on. Both are loaded once and for all -- going from one
	// to the other is a key press, and sending a megabyte of texture up at every press is not.
	SDL_Texture *vs_chara_tex = nullptr;
	int vsc_w = 0, vsc_h = 0;
	int vs_row_sel = 0;        // the row the cursor is on, in the whole diagram
	int vs_chara = -1;         // whose page is open over it, -1 for none
	SDL_Texture *inputs = nullptr;   // the game's own icons for the three buttons and the eight directions
	int init_w = 0, init_h = 0, vs_w = 0, vs_h = 0;
	int stick_x = 0, stick_y = 0;
	// What CONTROLLER draws lit: the host fills this each frame while that screen is up. A mirror of the
	// pad and the keyboard, never an input -- the surface's own keys are read from events, as everywhere.
	enum { L_UP = 1, L_DOWN = 2, L_LEFT = 4, L_RIGHT = 8, L_PUNCH = 16, L_GUARD = 32, L_KICK = 64 };
	uint8_t live[2] = { 0, 0 };
	player_controls *ctl = nullptr;   // the host's bindings: CONTROLLER reads them AND writes them
	int ctl_row = 0, ctl_col = 0;     // which line and which player the cursor is on
	// WHICH PAD BELONGS TO WHICH PLAYER, filled by the host (it is the host that opens them). A capture on
	// 1P's column takes 1P's pad and nothing else, which is what Frederic asked for the 2026-10-25: before
	// it, a button pressed on the second pad while the cursor stood on the first player's column was bound
	// to the FIRST player. 0 means that player has no pad, and then any is taken -- otherwise his column
	// could never be bound at all.
	SDL_JoystickID pad_id[2] = { 0, 0 };
	int player_of_pad(SDL_JoystickID id) const
	{
		for (int p = 0; p < 2; p++) if (pad_id[p] && pad_id[p] == id) return p;
		return -1;
	}
	bool ctl_capturing = false;       // waiting for the key or button that will be the new binding
	bool ctl_changed = false;         // something was really rebound: the host saves and clears this
	int ctl_testing = -1;             // -1: the grid; 0 or 1: that player's mirror, opened by its TEST line
	// What the host lends this page for those three: it owns the pads, and this file has no business
	// knowing what a pad is called or how one is opened.
	std::function<std::string(int)> pad_text;   // that column's pad, as the player should read it
	std::function<void(int)> pad_cycle;         // the next pad for that column
	std::function<void(int)> ctl_defaults;      // that column's controls as they were
	std::function<void()> ctl_swap;             // the two columns' pads exchanged
	fv2_text *font = nullptr;   // the sheet, loaded once by the host and shared

	// The backdrop is authored 16:9 with the 4:3 picture inset in it and flat purple outside; only the inset is
	// drawn. Its bounds were measured on the file: the purple runs to x 447 and from x 3183.
	static constexpr int BG_W = 3630, BG_H = 2048, BG_L = 447, BG_R = 3183;

	bool load(SDL_Renderer *ren, const std::string &assets, fv2_text *sheet, FILE *log)
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
		// The gauge's arrow, lit and unlit, which SOUNDS' two volumes are drawn with -- the owner's own, and
		// the reason the volumes are a row of twenty marks here rather than a number.
		one("option_vol_up_on.png", vol_on, nullptr, nullptr, 0, 0);
		one("option_vol_up_off.png", vol_off, nullptr, nullptr, 0, 0);
		// The VS DIAGRAM's table, which STATS fills in. Its own grid is measured on the file.
		one("vs_global.png", vs_table, &vs_w, &vs_h, 0, 0);
		one("vs_chara.png", vs_chara_tex, &vsc_w, &vsc_h, 0, 0);
		// The icons CONTROLLER lights: three buttons and eight arrows, the game's own. Its boxes below were
		// measured on the file, so the page stays right if it is exported again at another size.
		one("inputs.png", inputs, nullptr, nullptr, 512, 512);
		ready = true;
		build_pages();
		if (log) fprintf(log, "the console's OPTIONS: its pictures and the glyph sheet are in\n");
		return true;
	}

	void textures_lost(SDL_Renderer *ren, const std::string &assets)
	{
		for (SDL_Texture **t : { &bg, &cursor, &init, &vol_on, &vol_off, &vs_table, &vs_chara_tex, &inputs })
			if (*t) { SDL_DestroyTexture(*t); *t = nullptr; }
		ready = false;
		tried = false;
		load(ren, assets, font, nullptr);
	}

	void show()
	{
		if (!ready) return;
		shown = true;
		at = SCR_OPTIONS;
		for (int &s : sel_of) s = 0;
		stick_x = stick_y = 0;
	}
	void hide() { shown = false; at = SCR_OPTIONS; }

	int lines() const { return int(page(at).size()); }
	int &sel() { return sel_of[at]; }
	int sel() const { return sel_of[at]; }
	// A line with nothing to switch is stepped OVER, never dropped: August's rule, and its reason is that a
	// page whose lines slide up and down as settings come and go is misread at a glance.
	bool skipped(int s, int i) const
	{
		const std::vector<line> &P = page(s);
		if (i < 0 || i >= int(P.size())) return false;
		if (P[size_t(i)].what != line::OWN) return false;
		const own_line &o = own[s][size_t(P[size_t(i)].which)];
		return !o.step && !o.act;
	}
	void move(int d)
	{
		const int n = lines();
		if (!n) return;
		for (int k = 0; k < n; k++) {
			sel() = (sel() + n + d) % n;
			if (!skipped(at, sel())) { want_sound = SND_MOVE; return; }
		}
	}

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
		// THE ONE WRITE TO THE GAME'S RAM THAT DOES NOT GO THROUGH console_play::ask_for, and the reason is
		// in console_state.h: this page IS the arcade's settings, and the state it belongs to declares that
		// the game does not run under it, so no frame of the game falls between this write and its reading.
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
			if (sel() < 0 || sel() >= lines()) return -1;
			const line &l = page(at)[size_t(sel())];
			if (l.what == line::BUTTON) { initialize(board); want_sound = SND_VALIDATE; }
			else if (l.what == line::OWN && own[at][size_t(l.which)].act) {
				const own_line &o = own[at][size_t(l.which)];
				o.act();
				if (!o.silent) want_sound = SND_VALIDATE;
			} else if (l.what == line::DOOR) {
				at = l.which;
				sel() = 0;
				if (skipped(at, 0)) move(+1);
				want_sound = SND_VALIDATE;
			}
			return -1;
		};
		// Backing out goes UP ONE SCREEN, which is August's own back stack rather than aiming at the modes
		// list by hand. From OPTIONS itself there is nowhere above, so it closes the page.
		// B goes to the chosen line first, and only then up a screen. A line that has something of its own to
		// undo -- a track the SOUNDS page started playing -- keeps the press by saying so, and the next B
		// leaves. Frederic asked for exactly that: the music plays on A and stops on B.
		auto back = [&] { return go_back(); };

		auto sideways = [&](int d) {
			if (sel() < 0 || sel() >= lines()) return;
			const line &l = page(at)[size_t(sel())];
			if (l.what == line::SETTING) { step(board, size_t(l.which), d); want_sound = SND_MOVE; }
			else if (l.what == line::OWN && own[at][size_t(l.which)].step) {
				own[at][size_t(l.which)].step(d);
				want_sound = SND_MOVE;
			}
		};
		// CONTROLLER IS A MIRROR, SO IT TAKES ALMOST NOTHING. Every direction and every attack button is a
		// thing the player came here to see lit, and the menu's own reading of them made the page useless:
		// left and right passed from one player to the other, punch counted as A, and kick -- which is the
		// pad's B -- walked straight out. Frederic hit the first of those within a minute.
		//
		// So the fix is not a better binding but one less thing to bind: BOTH PLAYERS ARE DRAWN AT ONCE,
		// nothing is chosen, and the only control the screen keeps is the one it never draws -- Start, plus
		// Backspace for a keyboard without a pad. Everything else falls through to the mirror untouched.
		if (at == SCR_CONTROLLER) return controller_event(e);
		if (at == SCR_STATS) return stats_event(e, ctl);
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

		if (at == SCR_STATS) render_stats(ren, mid, board);
		else if (at == SCR_CONTROLLER && inputs) render_controller(ren, mid);
		else if (page(at).empty()) render_child(ren, mid);
		else render_lines(ren, mid, board);

		// THE FOOT IS GONE, and only what the player cannot guess is left. The Dreamcast puts two lines at the
		// bottom of this screen -- "USE THE DIRECTION BUTTON TO SELECT", "A : DECIDE   B : BACK" -- and this
		// page carried them. Frederic, the 2026-10-29: "enleve ce texte en bas d'ecran sur tous les ecrans qui
		// le propose". A cabinet tells a passer-by which buttons exist because he has never touched it; a
		// player who has opened this page has already used both.
		//
		// WHAT STAYS IS THE CAPTURE'S OWN PROMPT on CONTROLLER, which is not a reminder but the only thing
		// that says what the page is waiting for: a press that will be written down, or a pad being watched.
		// Nothing else on the screen says so, and the way out of a capture is not any of the usual two.
		if (at == SCR_CONTROLLER && (ctl_testing >= 0 || ctl_capturing)) {
			const float fh = foot_cell(mid);
			const char *f1 = ctl_testing >= 0 ? "HOLD A DIRECTION OR A BUTTON" : "PRESS THE KEY OR BUTTON TO SET";
			const char *f2 = ctl_testing >= 0 ? "B : BACK" : "START OR BACKSPACE : CANCEL";
			font->draw_mid(ren, f1, mid.x + mid.w * 0.5f, mid.y + FOOT1 * mid.h, fh, PLAIN);
			font->draw_mid(ren, f2, mid.x + mid.w * 0.5f, mid.y + FOOT2 * mid.h, fh, PLAIN);
		}
	}

private:
	float foot_cell(const SDL_FRect &mid) const { return ROW_CAP * mid.h / fv2_text::CAP * 0.62f; }

	// ---- STATS: the VS DIAGRAM, drawn rather than the game's own screen opened ----
	//
	// WHERE THE COUNTERS LIVE. They are not in RAM: the game keeps them in the backup, each counter a 16-bit
	// big-endian value in a slot of eight bytes, so six of the eight go unused (the coding was measured here,
	// not assumed from the settings' -- see below). The layout is August's §12:
	//
	//     block(character) = VS_BASE + n * 0x1A8
	//       + 0x000               KO
	//       + 0x008               SKO 1      (the pair below; 0x010 is the one measured)
	//       + 0x010               SKO 2
	//       + 0x018               JUDGE
	//       + 0x020               DRAW       (measured here; see below)
	//       + 0x028 + opponent*8  FIGHT      (sixteen opponents, not fourteen: the row is 0x80 wide)
	//       + 0x0A8 + opponent*8  WIN
	//       + 0x128 + opponent*8  LOSE
	//
	// AND DRAW IS STORED AFTER ALL, at 0x020, which August's note had as nowhere. Four versus matches played
	// through, hidden, each against the backup the one before it wrote, said so. The first three:
	//
	//   * four rounds with neither player moving -- all draws -- put 4 at 0x020 in BOTH blocks, and nothing
	//     at WIN or LOSE;
	//   * three decided rounds then moved FIGHT 4 -> 7, LOSE 0 -> 3 in one block and WIN 0 -> 3 in the other,
	//     KO 0 -> 3 for the winner, and LEFT 0x020 AT 4. A fight total would have followed FIGHT to 7;
	//   * a third match, struck for eight seconds and then left to the clock, moved FIGHT 7 -> 10, one WIN and
	//     one LOSE, 0x018 from 0 to 1 -- that is JUDGE, and the only round it could have come from is the one
	//     that ran out at unequal health -- and 0x020 from 4 to 6, which is the two rounds that ended level.
	//
	// So 0x020 is the draws, and it agrees with FIGHT - WIN - LOSE on every row measured (10 - 0 - 4 = 6, and
	// 10 - 4 - 0 = 6). The stored figure is what is read here, since it is the game's own count rather than a
	// sum of three of its others.
	//
	// AND THE SUPER K.O. IS MEASURED NOW, by a hand that could do it. Neither scripted play nor thirty
	// thousand frames of random mashing ever produced one -- the move wants the player's own armour gone
	// first (Armor Flash: forward, back, forward, back with all three buttons) and the opponent nearly dead
	// -- so Frederic played it. One versus match, and the diff is unmistakable:
	//
	//   0x010 goes from 0 to 1 in the winner's block, and KO, JUDGE and DRAW do not move at all.
	//
	// A win that counts as none of the three ordinary endings and lands in one of the two gaps IS the Super
	// K.O., and that settles the pitch for the pair.
	//
	// AND WHAT SEPARATES THE TWO IS NOT TWO MOVES. Read in Ghidra afterwards (docs/fv2-match-flow.md
	// section 8): FUN_000701C8 is the routine that files a match, and both columns count THE SAME ending --
	// 0x00581452 == 3 either way. What picks the column is the SUPER K.O. SETTING itself:
	//
	//     lbz r0, 0x554(r9)      the setting at 0x00120554
	//     beq -> +0x04           OFF: the first counter   (backup 0x008, SKO 1 here)
	//         -> +0x08           ON:  the second          (backup 0x010, SKO 2 here)
	//
	// So an operator who turns SUPER K.O. off does not stop the ending happening; he moves where it is
	// counted. The two cells hold the Super K.O.s won with the setting off and those won with it on. Which
	// is why Frederic's one, played with the setting on, landed in the second -- and why the order drawn
	// here, the off one first, is the order the game's own code uses.
	//
	// THE COUNTERS ARE 32-BIT, lwz and stw, on a live table at 0x0011F7E0 with a stride of 0xD4 -- exactly
	// half the backup's 0x1A8, the backup being that table spaced two useful bytes in four. In the backup
	// each counter takes eight bytes and the value reads in the FIRST TWO, measured: 00 01 00 00 00 00 00 00
	// for a count of one. So the 16-bit read below is right up to 65535 and would need the other half past
	// that, which no one will reach by playing.
	//
	// ITS BASE HAD TO BE MOVED, and this is the one number not taken from August. Its offsets are into ITS
	// NVRAM dump, not this port's `backup`. The two are related by a constant, found from the fingerprint it
	// prints: at its 0x188 it reads 210 210 120 120 69 180, which is the GAME ASSIGNMENTS block, and that same
	// run of six sits at 0x42 here. So ours is theirs less 0x146, which puts the first character's block at
	// 0x3006 -- a few bytes into the backup's third page, which is exactly where the third block starts.
	//
	// AND IT IS CONFIRMED NOW, on figures a played match wrote (docs/fv2-console-options.md keeps the runs).
	// A versus between CHARLIE and EMI moved five bytes and no others: block 10 at 0x4096 and block 11 at
	// 0x423E, each carrying the other as its opponent, and each pair symmetric -- CHARLIE's LOSE against EMI
	// beside EMI's WIN against CHARLIE. That is the base, the stride and the per-opponent slot all landing at
	// once, which no single counter could have shown.
	//
	// The 16-bit value is two CONTIGUOUS bytes, big-endian, at the slot's +0 and +1: a count of 4 moved the
	// byte at +1 and left +0 alone. Opponents are eight bytes apart, so six of the eight go unused. An ARCADE
	// match writes none of this -- the verification round moved fourteen bytes, all of them elsewhere -- which
	// is the sense in which the screen is a VERSUS diagram.
	static constexpr uint32_t VS_BASE = 0x3006, VS_STRIDE = 0x1A8;
	static constexpr uint32_t VS_KO = 0x000, VS_SKO1 = 0x008, VS_SKO2 = 0x010;
	static constexpr uint32_t VS_JUDGE = 0x018, VS_DRAW = 0x020;
	static constexpr uint32_t VS_FIGHT = 0x028, VS_WIN = 0x0A8, VS_LOSE = 0x128;
	// Which block holds which of the screen's rows. August's own table, every line of it measured by playing.
	static const int *vs_block_of_row()
	{
		static const int B[14] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 10, 11, 13, 14, 9 };
		return B;
	}
	// The grid of vs_global.png, in fractions of the file, found by its white rules. Fractions and not pixels,
	// so re-exporting the picture at another size changes nothing.
	static constexpr float VS_L = 327.0f / 2240.0f, VS_R = 2219.0f / 2240.0f;
	static constexpr float VS_T = 99.0f / 1504.0f, VS_B = 1391.0f / 1504.0f;
	static constexpr int VS_COLS = 8;    // FIGHT WIN LOSE DRAW KO SKO1 SKO2 JUDGE
	static constexpr int VS_ROWS = 14;

	struct vs_row { uint32_t fight, win, lose, draw, ko, sko1, sko2, judge; };
	vs_row read_vs(m3::Model3 &board, int row) const
	{
		vs_row r = { 0, 0, 0, 0, 0, 0, 0, 0 };
		const int block = vs_block_of_row()[row];
		auto u16 = [&](uint32_t off) -> uint32_t {
			return off + 1 < board.backup.size() ? uint32_t(board.backup[off]) << 8 | board.backup[off + 1] : 0;
		};
		const uint32_t base = VS_BASE + uint32_t(block) * VS_STRIDE;
		for (int adv = 0; adv < 16; adv++) {
			r.fight += u16(base + VS_FIGHT + uint32_t(adv) * 8);
			r.win += u16(base + VS_WIN + uint32_t(adv) * 8);
			r.lose += u16(base + VS_LOSE + uint32_t(adv) * 8);
		}
		r.draw = u16(base + VS_DRAW);
		r.ko = u16(base + VS_KO);
		r.sko1 = u16(base + VS_SKO1);
		r.sko2 = u16(base + VS_SKO2);
		r.judge = u16(base + VS_JUDGE);
		return r;
	}

	// ---- STATS: the whole diagram, and a page for the character the cursor is on ----
	//
	// Up and down move over the fourteen, A opens that one's page and B comes back to the diagram; on the
	// page itself NOTHING answers the stick, which is August's own rule and his reason: "the band says this
	// line is the chosen one; where no line can be, it says something false". The band
	// keeps blinking on the row the page came from, which is the row B falls back onto.
	int stats_event(const SDL_Event &e, const player_controls ctl[2])
	{
		auto bound = [&](int action, SDL_Scancode sc) {
			for (int p = 0; p < 2; p++) if (ctl[p].key[action] == sc) return true;
			return false;
		};
		auto up = [&](int d) {
			if (vs_chara >= 0) return;
			vs_row_sel = (vs_row_sel + VS_ROWS + d) % VS_ROWS;
			want_sound = SND_MOVE;
		};
		auto open_it = [&] {
			if (vs_chara >= 0 || !vs_chara_tex) return;
			vs_chara = vs_row_sel;
			want_sound = SND_VALIDATE;
		};
		auto back = [&]() -> int {
			if (vs_chara < 0) return go_back();
			vs_chara = -1;
			want_sound = SND_CANCEL;
			return -1;
		};
		switch (e.type) {
		case SDL_EVENT_KEY_DOWN: {
			if (e.key.repeat) return -1;
			const SDL_Scancode sc = e.key.scancode;
			if (sc == SDL_SCANCODE_UP || bound(C_UP, sc)) up(-1);
			else if (sc == SDL_SCANCODE_DOWN || bound(C_DOWN, sc)) up(+1);
			else if (sc == SDL_SCANCODE_RETURN || sc == SDL_SCANCODE_KP_ENTER || sc == SDL_SCANCODE_SPACE
					 || bound(C_PUNCH, sc)) open_it();
			else if (sc == SDL_SCANCODE_BACKSPACE || bound(C_KICK, sc)) return back();
			return -1;
		}
		case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
			switch (e.gbutton.button) {
			case SDL_GAMEPAD_BUTTON_DPAD_UP: up(-1); break;
			case SDL_GAMEPAD_BUTTON_DPAD_DOWN: up(+1); break;
			case SDL_GAMEPAD_BUTTON_SOUTH: open_it(); break;
			case SDL_GAMEPAD_BUTTON_EAST: return back();
			default: break;
			}
			return -1;
		case SDL_EVENT_GAMEPAD_AXIS_MOTION:
			if (e.gaxis.axis == SDL_GAMEPAD_AXIS_LEFTY) {
				const int dir = e.gaxis.value < -16000 ? -1 : e.gaxis.value > 16000 ? 1 : 0;
				if (dir && dir != stick_y) up(dir);
				stick_y = dir;
			}
			return -1;
		default: return -1;
		}
	}

	// THE GRID OF vs_chara.png, measured on its HEADER STRIP and on its COLUMN OF NAMES and nowhere else --
	// August's own warning, and the reason is in his notes: the logo's watermark makes false rules all over
	// the rest of the picture, and a first pass read columns 118 apart where the true ones are 237.
	//
	//     names 7..327     five columns of 237, to 1511
	//     header 1..91     fourteen rows of 92, to 1383, then TOTAL
	static constexpr float VSC_L = 327.0f / 1523.0f, VSC_W = 237.0f / 1523.0f;
	// AND THE TOP IS 107.5, NOT 91. August's figure is where his own picture's header ENDS; the rule under
	// it is 16 pixels thick, and reading the rules of this very file -- the rows that are mostly dark across
	// the data columns -- puts the first cell's edge at 107.5 and the pitch at 92.08. Sixteen pixels is a
	// sixth of a row, which is exactly what Frederic saw: the figures riding high in their cells.
	static constexpr float VSC_T = 107.5f / 1508.0f, VSC_H = 92.08f / 1508.0f;
	static constexpr int VSC_COLS = 5;   // FIGHT WIN LOSE DRAW RATIO

	// A character's page: his fourteen lines against the others, raw out of the matrix. NOTHING IS SUMMED
	// here but the TOTAL row -- the whole diagram is the sum, this is the data. KO and JUDGE have no column
	// and that is right: they are totals for a character, and there is nothing to put on a line that speaks
	// of one opponent. DRAW is what the three columns leave: fights less wins less losses.
	void render_chara(SDL_Renderer *ren, const SDL_FRect &mid, m3::Model3 &board)
	{
		const float top = 0.165f * mid.h, bot = 0.885f * mid.h;
		float th = bot - top, tw = th * float(vsc_w) / float(vsc_h ? vsc_h : 1);
		if (tw > mid.w * 0.98f) { tw = mid.w * 0.98f; th = tw * float(vsc_h) / float(vsc_w ? vsc_w : 1); }
		const SDL_FRect dst = { mid.x + (mid.w - tw) * 0.5f, mid.y + top + (bot - top - th) * 0.5f, tw, th };
		SDL_RenderTexture(ren, vs_chara_tex, nullptr, &dst);

		const int block = vs_block_of_row()[vs_chara];
		const uint32_t base = VS_BASE + uint32_t(block) * VS_STRIDE;
		auto u16 = [&](uint32_t off) -> uint32_t {
			return off + 1 < board.backup.size() ? uint32_t(board.backup[off]) << 8 | board.backup[off + 1] : 0;
		};
		const float cw = VSC_W * tw, rh = VSC_H * th;
		const float h = rh * 0.62f / fv2_text::CAP;
		// Black on this one, where the whole diagram is white: these cells are white and those are dark.
		// The colour belongs to the picture, not to a rule in the drawing code.
		const SDL_Color ink = { 20, 20, 30, 255 };
		uint32_t tf = 0, tw_ = 0, tl = 0;
		for (int r = 0; r < VS_ROWS; r++) {
			const int adv = vs_block_of_row()[r];
			const uint32_t f = u16(base + VS_FIGHT + uint32_t(adv) * 8);
			const uint32_t w = u16(base + VS_WIN + uint32_t(adv) * 8);
			const uint32_t l = u16(base + VS_LOSE + uint32_t(adv) * 8);
			tf += f; tw_ += w; tl += l;
			draw_chara_row(ren, f, w, l, dst.x + VSC_L * tw, dst.y + (VSC_T + float(r) * VSC_H) * th, cw, rh, h, ink);
		}
		draw_chara_row(ren, tf, tw_, tl, dst.x + VSC_L * tw, dst.y + (VSC_T + float(VS_ROWS) * VSC_H) * th, cw, rh, h, ink);
	}

	// The ratio is ALWAYS a percentage with two decimals, 0.00% included. A first version put a dash where
	// there had been no fight -- nought out of nought not being nought per cent -- but the original screen
	// writes 0.00%, and it is the original that settles it here.
	void draw_chara_row(SDL_Renderer *ren, uint32_t f, uint32_t w, uint32_t l, float gl, float y,
						float cw, float rh, float h, SDL_Color ink)
	{
		char ratio[16];
		snprintf(ratio, sizeof ratio, "%.2f%%", f ? 100.0 * double(w) / double(f) : 0.0);
		const std::string cells[VSC_COLS] = {
			std::to_string(f), std::to_string(w), std::to_string(l),
			std::to_string(f > w + l ? f - w - l : 0u), std::string(ratio),
		};
		// EACH FIGURE FITS ITS OWN CELL. The ratio is the longest thing on the page -- "100.00%" against a
		// column measured for four digits -- and it ran out through the right-hand rule. So a cell that is
		// too narrow for its text shrinks the text rather than letting it out: the column is the picture's,
		// and the picture is what was measured.
		for (int c = 0; c < VSC_COLS; c++)
			draw_fitted(ren, cells[size_t(c)].c_str(), gl + (c + 0.5f) * cw, y + rh * 0.5f, cw * 0.90f, h, ink);
	}

	void render_stats(SDL_Renderer *ren, const SDL_FRect &mid, m3::Model3 &board)
	{
		if (!vs_table) { render_child(ren, mid); return; }
		// A page open over the diagram: the two are a stack. The band below goes on blinking on the row it
		// came from, which is where B will put the cursor back.
		if (vs_chara >= 0 && vs_chara_tex) { render_chara(ren, mid, board); return; }
		// The table fills the picture under the title and above the foot, keeping its own proportions.
		const float top = 0.165f * mid.h, bot = 0.885f * mid.h;
		float th = bot - top, tw = th * float(vs_w) / float(vs_h ? vs_h : 1);
		if (tw > mid.w * 0.98f) { tw = mid.w * 0.98f; th = tw * float(vs_h) / float(vs_w ? vs_w : 1); }
		const SDL_FRect dst = { mid.x + (mid.w - tw) * 0.5f, mid.y + top + (bot - top - th) * 0.5f, tw, th };
		SDL_RenderTexture(ren, vs_table, nullptr, &dst);

		// A row of the grid is a fifteenth of it: the fourteen characters and TOTAL underneath.
		const float gl = dst.x + VS_L * tw, gr = dst.x + VS_R * tw;
		const float gt = dst.y + VS_T * th, gb = dst.y + VS_B * th;
		const float cw = (gr - gl) / VS_COLS, rh = (gb - gt) / VS_ROWS;
		const float h = rh * 0.62f / fv2_text::CAP;
		vs_row total = { 0, 0, 0, 0, 0, 0, 0, 0 };
		for (int r = 0; r < VS_ROWS; r++) {
			const vs_row v = read_vs(board, r);
			total.fight += v.fight; total.win += v.win; total.lose += v.lose;
			total.draw += v.draw; total.ko += v.ko; total.judge += v.judge;
			total.sko1 += v.sko1; total.sko2 += v.sko2;
			// THE NAME IS MARKED, not only the figures. The names are part of the picture, so the mark is a
			// wash of red laid over that one cell -- which is what Frederic asked for, and it says "this
			// line" where a yellow figure only says "this figure is different".
			if (r == vs_row_sel) {
				SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
				SDL_SetRenderDrawColor(ren, 210, 30, 40, 125);
				const SDL_FRect nm = { dst.x + 0.010f * tw, gt + r * rh, gl - (dst.x + 0.010f * tw), rh };
				SDL_RenderFillRect(ren, &nm);
			}
			draw_vs_row(ren, v, gl, gt + r * rh, cw, rh, h, r == vs_row_sel ? CHOSEN : PLAIN);
		}
		// TOTAL IS NOT THE CURSOR. Both were yellow while the cursor was new, and two yellow rows say two
		// different things in one colour: the sum has its own, paler and cooler.
		static constexpr SDL_Color SUM = { 150, 230, 255, 255 };
		draw_vs_row(ren, total, gl, gt + VS_ROWS * rh, cw, rh, h, SUM);
	}

	// All eight columns carry a figure now. SKO 1 and SKO 2 were blank while nobody could say where they
	// lived -- August's own list of loose ends had them "shown at zero with nobody knowing where they live" --
	// and a played Super K.O. answered it (above).
	void draw_vs_row(SDL_Renderer *ren, const vs_row &v, float gl, float y, float cw, float rh, float h,
					 SDL_Color col)
	{
		const uint32_t n[VS_COLS] = { v.fight, v.win, v.lose, v.draw, v.ko, v.sko1, v.sko2, v.judge };
		for (int c = 0; c < VS_COLS; c++)
			draw_fitted(ren, std::to_string(n[c]).c_str(), gl + (c + 0.5f) * cw, y + rh * 0.5f, cw * 0.90f, h, col);
	}

	// Centred on (cx, cy), at height h, or at whatever smaller height makes it fit in `room`.
	void draw_fitted(SDL_Renderer *ren, const char *t, float cx, float cy, float room, float h, SDL_Color col)
	{
		const float w = font->width(t, h);
		const float k = w > room && w > 0 ? room / w : 1.0f;
		font->draw_mid(ren, t, cx, cy - h * k * 0.5f, h * k, col);
	}

	// ---- CONTROLLER: the pad, drawn with the game's own icons, lit as it is held ----
	//
	// August's page was its title and nothing else, asked for that way on the 2026-08-18, and the three other
	// doors have since been filled. What belongs behind this one is not a setting -- this port's bindings are
	// edited in its own menu, which has the room for it -- but the thing a console options screen has that a
	// settings list cannot: A MIRROR. The eight directions and the three buttons, lit while they are held, so
	// a player can see at a glance that a pad is seen, which button is punch, and whether a stick's diagonals
	// come through. Left and right pass from 1P to 2P.
	//
	// The icons are the game's own (assets/menu-console/inputs.png, 512 x 512), and the boxes below were
	// measured on its ink rather than guessed at a grid: the arrows are not all the same width -- 71 for the
	// straight ones, 59 for the diagonals, 70 for up and down -- and forcing them into one box would fatten
	// the diagonals. Each is drawn at a common HEIGHT, which is what keeps them a set.
	struct icon { int x, y, w, h; };
	static const icon *icons()
	{
		// P, G, K, then left, up-left, up, up-right, right, down-right, down, down-left
		static const icon I[11] = {
			{   8,   8,  80, 80 }, { 104,   8,  80, 80 }, { 200,   8,  80, 80 },
			{   5, 105,  71, 74 }, { 116, 105,  59, 74 }, { 205, 105,  70, 74 }, { 305, 105,  59, 74 },
			{  16, 205,  71, 74 }, { 113, 205,  59, 74 }, { 205, 205,  70, 74 }, { 308, 205,  59, 74 },
		};
		return I;
	}
	enum { IC_P, IC_G, IC_K, IC_LEFT, IC_UL, IC_UP, IC_UR, IC_RIGHT, IC_DR, IC_DOWN, IC_DL };

	void draw_icon(SDL_Renderer *ren, int which, float cx, float cy, float h, bool lit)
	{
		const icon &i = icons()[which];
		const float w = h * float(i.w) / float(i.h);
		const SDL_FRect src = { float(i.x), float(i.y), float(i.w), float(i.h) };
		const SDL_FRect dst = { cx - w * 0.5f, cy - h * 0.5f, w, h };
		// Unlit is the same icon, dimmed: a second artwork for "not pressed" would be a second thing to keep
		// in step with the first.
		SDL_SetTextureAlphaMod(inputs, lit ? 255 : 70);
		SDL_SetTextureColorMod(inputs, 255, 255, 255);
		SDL_RenderTexture(ren, inputs, &src, &dst);
		SDL_SetTextureAlphaMod(inputs, 255);
	}

	// The eight things this page binds, in the order it lists them. The four directions carry no pad binding
	// in this port -- the d-pad and the left stick always move, whatever is set -- so their pad cell reads as
	// a dash unless someone binds one on purpose.
	struct ctl_row_def { int action; const char *name; int icon; uint8_t bit; };
	static const ctl_row_def *ctl_rows()
	{
		static const ctl_row_def R[9] = {
			{ C_UP,    "UP",    IC_UP,    L_UP },     { C_DOWN,  "DOWN",  IC_DOWN,  L_DOWN },
			{ C_LEFT,  "LEFT",  IC_LEFT,  L_LEFT },   { C_RIGHT, "RIGHT", IC_RIGHT, L_RIGHT },
			{ C_PUNCH, "PUNCH", IC_P,     L_PUNCH },  { C_GUARD, "GUARD", IC_G,     L_GUARD },
			{ C_KICK,  "KICK",  IC_K,     L_KICK },   { C_START, "START", -1,       0 },
			// SELECT is this port's COIN. The board has Start, Coin, Test and Service and no Select at all,
			// so the name is the console pad's and the lever is the one it stands on there -- the button a
			// player reaches for to put a credit in. It carries no icon: the sheet draws what a fighter does.
			{ C_COIN,  "SELECT", -1,      0 },
		};
		return R;
	}
	// Eight bindings, and a ninth line that binds nothing: TEST, one under each column, which opens that
	// player's mirror. The editor says what is bound; the mirror says it arrives. Frederic asked for exactly
	// that split, and it is the right one -- the first version of this page was the mirror alone, and setting
	// nothing made it useless; the mirror hung off the editor is worth having.
	static constexpr int CTL_TEST = 9;    // the row index of that line
	// ---- AND THE THREE THINGS THE PORT'S OWN MENU COULD DO THAT THIS PAGE COULD NOT ----
	//
	// The keyboard was never missing: a capture on a cell takes a key as readily as a pad button, and the
	// cell shows both. What was missing is everything ABOUT the pads rather than their buttons -- which
	// pad is this player's, putting a player's controls back as they were, and swapping the two players'
	// pads. Frederic listed them the 2026-10-27 off the port's own menu, where all three already live.
	//
	// They bind nothing, so they carry no action: A works them, and left and right go on meaning "the
	// other player" as they do on every other line of this grid. SWAP belongs to neither column, so it is
	// drawn once, between them.
	static constexpr int CTL_PAD = 10, CTL_DEFAULTS = 11, CTL_SWAP = 12;
	static constexpr int CTL_ROWS = 13;
	// THE BAND THIS GRID LIVES IN, and the blank each line wants before it. The pitch was a figure --
	// 0.058, the disc's own -- and with ten rows it fitted. With thirteen it does not: the last line
	// lands at 0.921 of the page and the footer is at 0.902, so SWAP PADS would have been written over
	// "A : SET   B : BACK". Computed rather than seen, the evening the three lines were added.
	//
	// So the band is the room available and the pitch is as much of it as the lines want, never past the
	// disc's -- which is word for word what render_lines does, and for the same reason.
	static constexpr float CTL_TOP = 0.225f, CTL_BOTTOM = 0.88f, CTL_PITCH = 0.058f;
	static float ctl_gap(int r)
	{
		return r == CTL_TEST ? 0.35f : r == CTL_PAD ? 0.30f : r == CTL_SWAP ? 0.18f : 0.0f;
	}

	// What a cell says: the key, then the pad button when one is bound.
	std::string binding_text(int player, int action) const
	{
		if (!ctl) return "-";
		std::string out;
		const SDL_Scancode sc = ctl[size_t(player)].key[action];
		if (sc != SDL_SCANCODE_UNKNOWN) {
			const char *n = SDL_GetScancodeName(sc);
			for (const char *q = n ? n : ""; *q && out.size() < 9; q++) out += char(toupper((unsigned char)*q));
		}
		const int pb = ctl[size_t(player)].pad[action];
		if (pb != PAD_NONE) {
			std::string pl = player_controls::pad_label(pb);
			for (char &c : pl) c = char(toupper((unsigned char)c));
			if (pl.size() > 9) pl.resize(9);
			out += out.empty() ? pl : " / " + pl;
		}
		return out.empty() ? "-" : out;
	}

	int go_back()
	{
		if (sel() >= 0 && sel() < lines()) {
			const line &l = page(at)[size_t(sel())];
			if (l.what == line::OWN) {
				const own_line &o = own[at][size_t(l.which)];
				// Asked BEFORE the sound is decided, so a line that speaks for itself is not spoken over
				if (o.cancel && o.cancel()) { if (!o.silent) want_sound = SND_CANCEL; return -1; }
			}
		}
		want_sound = SND_CANCEL;
		if (at == SCR_OPTIONS) return CANCELLED;
		at = SCR_OPTIONS;
		return -1;
	}

	// ONE EVENT ON THE CONTROLLER PAGE.
	//
	// The first version of this screen was a mirror and nothing else, and Frederic's verdict was the right
	// one: it checked the buttons and set nothing. An options page that cannot set anything is not one.
	//
	// Making it a binder also dissolves the trouble the mirror had. A mirror must leave every control free,
	// which left it only Start; a binder WANTS to swallow whatever is pressed, because that press is the
	// answer to its question. So: up and down walk the eight lines, left and right pass from 1P to 2P, A
	// opens a line for a new binding, and then the very next key or pad button becomes it. Backspace calls
	// that off, which is why Backspace is the one key that cannot be bound here.
	int controller_event(const SDL_Event &e)
	{
		// THE MIRROR TAKES ALMOST NOTHING, for the reason that sank the first version of this page: every
		// direction and every attack button is a thing the player came here to see lit. Only Start and
		// Backspace are kept, and they go back to the grid.
		if (ctl_testing >= 0) {
			const bool leave = (e.type == SDL_EVENT_KEY_DOWN && !e.key.repeat
								&& (e.key.scancode == SDL_SCANCODE_BACKSPACE || e.key.scancode == SDL_SCANCODE_RETURN
									|| e.key.scancode == SDL_SCANCODE_KP_ENTER))
							   || (e.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN
								   && (e.gbutton.button == SDL_GAMEPAD_BUTTON_EAST
									   || e.gbutton.button == SDL_GAMEPAD_BUTTON_START));
			if (leave) { ctl_testing = -1; want_sound = SND_CANCEL; }
			return -1;
		}
		if (ctl_capturing) {
			const int action = ctl_rows()[size_t(ctl_row)].action;
			if (e.type == SDL_EVENT_KEY_DOWN && !e.key.repeat) {
				ctl_capturing = false;
				if (e.key.scancode == SDL_SCANCODE_BACKSPACE) { want_sound = SND_CANCEL; return -1; }
				if (ctl) ctl[size_t(ctl_col)].key[action] = e.key.scancode;
				ctl_changed = true;
				want_sound = SND_VALIDATE;
			} else if (e.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) {
				// Only this player's own pad (see pad_id). Another player's press is not his to bind, and
				// it does not call the capture off either: it simply is not for him.
				if (pad_id[size_t(ctl_col)] && e.gbutton.which != pad_id[size_t(ctl_col)]) return -1;
				ctl_capturing = false;
				// START CALLS IT OFF, it does not bind. A capture that swallows everything leaves a pad with
				// no way out, and the first hand to try this page proved it: Start pressed to leave became
				// the kick button. Start is this page's way out and the game's own button; it is not for binding.
				if (e.gbutton.button == SDL_GAMEPAD_BUTTON_START) { want_sound = SND_CANCEL; return -1; }
				if (ctl) ctl[size_t(ctl_col)].pad[action] = int(e.gbutton.button);
				ctl_changed = true;
				want_sound = SND_VALIDATE;
			}
			return -1;
		}
		if (e.type == SDL_EVENT_KEY_DOWN && !e.key.repeat) {
			switch (e.key.scancode) {
			case SDL_SCANCODE_UP: ctl_row = (ctl_row + CTL_ROWS - 1) % CTL_ROWS; want_sound = SND_MOVE; return -1;
			case SDL_SCANCODE_DOWN: ctl_row = (ctl_row + 1) % CTL_ROWS; want_sound = SND_MOVE; return -1;
			case SDL_SCANCODE_LEFT: case SDL_SCANCODE_RIGHT: ctl_col ^= 1; want_sound = SND_MOVE; return -1;
			case SDL_SCANCODE_RETURN: case SDL_SCANCODE_KP_ENTER: case SDL_SCANCODE_SPACE:
				ctl_act(); return -1;
			case SDL_SCANCODE_BACKSPACE: return go_back();
			default: break;
			}
			// THE WAY OUT IS THE CANCEL BUTTON AND NOT START, which Frederic asked for the 2026-10-25:
			// Start is the game's own button and a page you leave with it is a page you leave by accident.
			// The key a player has set as his KICK is this port's cancel everywhere else in these surfaces.
			for (int p = 0; p < 2; p++) if (ctl && ctl[p].key[C_KICK] == e.key.scancode) return go_back();
			return -1;
		}
		if (e.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) {
			// A PAD STANDS ON ITS OWN PLAYER'S COLUMN AND CANNOT LEAVE IT. Frederic, the 2026-10-25: "1P must
			// only be able to change 1P, and 2P only 2P". So a press on a known pad moves the cursor to
			// that player's column before anything else, and left and right do nothing from a pad -- there
			// is nowhere for it to go. The KEYBOARD keeps them: it belongs to neither player, and without
			// them a player with no pad could never reach the other column at all.
			if (const int w = player_of_pad(e.gbutton.which); w >= 0) ctl_col = w;
			switch (e.gbutton.button) {
			case SDL_GAMEPAD_BUTTON_DPAD_UP: ctl_row = (ctl_row + CTL_ROWS - 1) % CTL_ROWS; want_sound = SND_MOVE; break;
			case SDL_GAMEPAD_BUTTON_DPAD_DOWN: ctl_row = (ctl_row + 1) % CTL_ROWS; want_sound = SND_MOVE; break;
			case SDL_GAMEPAD_BUTTON_DPAD_LEFT: case SDL_GAMEPAD_BUTTON_DPAD_RIGHT:
				if (player_of_pad(e.gbutton.which) < 0) { ctl_col ^= 1; want_sound = SND_MOVE; }
				break;
			case SDL_GAMEPAD_BUTTON_SOUTH: ctl_act(); break;
			case SDL_GAMEPAD_BUTTON_EAST: return go_back();   // the cancel button, not Start (see above)
			default: break;
			}
			return -1;
		}
		if (e.type == SDL_EVENT_GAMEPAD_AXIS_MOTION) {
			const int dir = e.gaxis.value < -16000 ? -1 : e.gaxis.value > 16000 ? 1 : 0;
			const int w = player_of_pad(e.gaxis.which);
			if (e.gaxis.axis == SDL_GAMEPAD_AXIS_LEFTY) {
				if (dir && dir != stick_y) {
					if (w >= 0) ctl_col = w;
					ctl_row = (ctl_row + CTL_ROWS + dir) % CTL_ROWS;
					want_sound = SND_MOVE;
				}
				stick_y = dir;
			} else if (e.gaxis.axis == SDL_GAMEPAD_AXIS_LEFTX) {
				if (dir && dir != stick_x && w < 0) { ctl_col ^= 1; want_sound = SND_MOVE; }
				stick_x = dir;
			}
		}
		return -1;
	}

	// A opens a capture on a binding line, the mirror on TEST, and works the three that bind nothing.
	void ctl_act()
	{
		want_sound = SND_VALIDATE;
		if (ctl_row == CTL_TEST) { ctl_testing = ctl_col; return; }
		if (ctl_row == CTL_PAD) { if (pad_cycle) pad_cycle(ctl_col); return; }
		if (ctl_row == CTL_DEFAULTS) { if (ctl_defaults) ctl_defaults(ctl_col); ctl_changed = true; return; }
		if (ctl_row == CTL_SWAP) { if (ctl_swap) ctl_swap(); return; }
		ctl_capturing = true;
	}

	// THE MIRROR, which this page began as: the eight directions and the three buttons of one player, lit
	// while they are held. A diagonal lights on its two, and a straight arrow stays dark while a diagonal is
	// held -- which is what makes a stick's corner unmistakable.
	void render_mirror(SDL_Renderer *ren, const SDL_FRect &mid)
	{
		const int player = ctl_testing;
		const uint8_t v = live[size_t(player)];
		const float cap = ROW_CAP * mid.h;
		const float h = cap / fv2_text::CAP;

		char who[8];
		snprintf(who, sizeof who, "%dP", player + 1);
		font->draw_mid(ren, who, mid.x + mid.w * 0.5f, mid.y + 0.215f * mid.h, h, CHOSEN);

		const float ih = 0.075f * mid.h, pitch = ih * 1.3f;
		const float ccx = mid.x + 0.5f * mid.w, ccy = mid.y + 0.445f * mid.h;
		struct place { int which; int dx, dy; uint8_t a, b; };
		static const place P8[8] = {
			{ IC_UP,    0, -1, L_UP,    0 },  { IC_UR,    1, -1, L_UP,   L_RIGHT },
			{ IC_RIGHT, 1,  0, L_RIGHT, 0 },  { IC_DR,    1,  1, L_DOWN, L_RIGHT },
			{ IC_DOWN,  0,  1, L_DOWN,  0 },  { IC_DL,   -1,  1, L_DOWN, L_LEFT },
			{ IC_LEFT, -1,  0, L_LEFT,  0 },  { IC_UL,   -1, -1, L_UP,   L_LEFT },
		};
		for (const place &q : P8) {
			const bool lit = q.b ? ((v & q.a) && (v & q.b)) : ((v & q.a) != 0 && (v & dirmask(q.a)) == 0);
			draw_icon(ren, q.which, ccx + float(q.dx) * pitch, ccy + float(q.dy) * pitch, ih, lit);
		}

		static const int B3[3] = { IC_P, IC_G, IC_K };
		static const int ACT[3] = { C_PUNCH, C_GUARD, C_KICK };
		static const uint8_t BIT[3] = { L_PUNCH, L_GUARD, L_KICK };
		const float by = ccy + pitch + ih * 1.35f, bpitch = ih * 1.6f;
		for (int i = 0; i < 3; i++) {
			const float bx = ccx + float(i - 1) * bpitch;
			draw_icon(ren, B3[i], bx, by, ih, (v & BIT[i]) != 0);
			font->draw_mid(ren, binding_text(player, ACT[i]).c_str(), bx, by + ih * 0.72f, h * 0.58f,
						   (v & BIT[i]) ? CHOSEN : MUTED);
		}
	}

	// The grid: an icon that lights while its control is held, the action's name, and the two players'
	// bindings side by side. The icon is the mirror the first version was made of, kept because it answers
	// the question a binding list cannot -- "is this actually arriving?" -- and it costs no room of its own.
	void render_controller(SDL_Renderer *ren, const SDL_FRect &mid)
	{
		if (ctl_testing >= 0) { render_mirror(ren, mid); return; }
		const float cap = ROW_CAP * mid.h * 0.78f;
		const float h = cap / fv2_text::CAP;
		float slots = 0;
		for (int r = 0; r < CTL_ROWS; r++) slots += (r ? 1.0f : 0.0f) + ctl_gap(r);
		const float band = (CTL_BOTTOM - CTL_TOP) * mid.h;
		const float pitch = std::min(CTL_PITCH * mid.h, band / (slots > 0 ? slots : 1.0f));
		const float top = CTL_TOP * mid.h;
		const float icon_x = mid.x + 0.205f * mid.w, name_x = mid.x + 0.265f * mid.w;
		const float col_x[2] = { mid.x + 0.545f * mid.w, mid.x + 0.745f * mid.w };
		const float ih = pitch * 0.72f;

		font->draw_mid(ren, "1P", col_x[0], mid.y + top - pitch, h, ctl_col == 0 ? CHOSEN : MUTED);
		font->draw_mid(ren, "2P", col_x[1], mid.y + top - pitch, h, ctl_col == 1 ? CHOSEN : MUTED);

		// The blank each line wants is part of the pitch now (ctl_gap), so a line is where the ones above
		// it put it -- the same walk render_lines makes, and the reason the grid fits its band again.
		float ink = mid.y + top;
		for (int r = 0; r < CTL_ROWS; r++) {
			ink += (r ? pitch : 0.0f) + ctl_gap(r) * pitch;
			const float y = ink;
			if (r == ctl_row)
				rule(ren, mid, y + h * ((16.0f + 31.0f) / fv2_text::CELL + 0.12f), h, 0.145f, 0.805f);
			// The TEST line binds nothing: it carries no name of its own and says TEST under each column.
			if (r == CTL_TEST) {
				for (int c = 0; c < 2; c++)
					font->draw_mid(ren, "TEST", col_x[size_t(c)], y, h,
								   r == ctl_row && c == ctl_col ? CHOSEN : PLAIN);
				continue;
			}
			// The three that bind nothing: the pad under each column, that column's controls put back, and
			// the swap, which belongs to neither column and is drawn between them.
			if (r == CTL_PAD || r == CTL_DEFAULTS || r == CTL_SWAP) {
				if (r == CTL_SWAP) {
					font->draw_mid(ren, "SWAP PADS", (col_x[0] + col_x[1]) * 0.5f, y, h,
								   r == ctl_row ? CHOSEN : PLAIN);
					continue;
				}
				font->draw(ren, r == CTL_PAD ? "PAD" : "DEFAULTS", name_x, y, h, r == ctl_row ? CHOSEN : PLAIN);
				for (int c = 0; c < 2; c++) {
					const bool here = r == ctl_row && c == ctl_col;
					const std::string t = r == CTL_DEFAULTS ? std::string("PUT BACK")
										: pad_text ? pad_text(c) : std::string("-");
					font->draw_mid(ren, t.c_str(), col_x[size_t(c)], y, h, here ? CHOSEN : PLAIN);
				}
				continue;
			}
			const ctl_row_def &d = ctl_rows()[size_t(r)];
			// LIT ALWAYS on this page: here the icon says which line this is, not whether anything is held.
			// The mirror behind TEST is what answers that, and it answers it better, being large.
			if (d.icon >= 0) draw_icon(ren, d.icon, icon_x, y + cap * 0.45f, ih, true);
			font->draw(ren, d.name, name_x, y, h, r == ctl_row ? CHOSEN : PLAIN);
			for (int c = 0; c < 2; c++) {
				const bool here = r == ctl_row && c == ctl_col;
				const std::string t = here && ctl_capturing ? std::string("...") : binding_text(c, d.action);
				font->draw_mid(ren, t.c_str(), col_x[size_t(c)], y, h, here ? CHOSEN : PLAIN);
			}
		}
	}

	// The other three straight directions, for the test above: a straight arrow stays dark while the stick
	// is in a corner, so the corner is unmistakable.
	static uint8_t dirmask(uint8_t a)
	{
		if (a == L_UP || a == L_DOWN) return L_LEFT | L_RIGHT;
		return L_UP | L_DOWN;
	}

	// A screen behind a door. Nothing is drawn but its title and the way out -- which is exactly what August's
	// CONTROLLER page was, and all that is honest for the other three until their contents are built. A page
	// that pretended to hold something would be worse than one that says it is empty.
	void render_child(SDL_Renderer *ren, const SDL_FRect &mid)
	{
		const float h = ROW_CAP * mid.h / fv2_text::CAP;
		static const char *const SOON[SCR_COUNT] = {
			"", "", "WIDESCREEN, HD ART, FPS COUNTER", "SOUND TEST, MUSIC, VOLUMES, PACK",
			"", "THE VS DIAGRAM",
		};
		const char *soon = at >= 0 && at < SCR_COUNT ? SOON[at] : "";
		if (*soon) font->draw_mid(ren, soon, mid.x + mid.w * 0.5f, mid.y + 0.45f * mid.h, h * 0.8f, MUTED);
	}

	// A screen that has lines: OPTIONS' five doors, or GAME ASSIGNMENTS' thirteen settings and its button.
	//
	// A PAGE TOO LONG LOOKS FOR ROOM BEFORE IT SHRINKS, which is August's rule and was paid for there: raise
	// the band first and only then let the text give, "otherwise every increase of the font was cancelled by
	// the tightening -- two effects that cancel exactly, and a correction that seems to do nothing".
	// So the band is the room available, the pitch is as much of it as the lines want up to the Dreamcast's
	// own, and the cap follows the pitch, never past the disc's. A SHORT PAGE DOES NOT STRETCH: it keeps the
	// disc's pitch and sits in the middle of the band, which is why both screens read as the same menu.
	void render_lines(SDL_Renderer *ren, const SDL_FRect &mid, m3::Model3 &board)
	{
		const std::vector<line> &P = page(at);
		float slots = 0;
		for (size_t i = 0; i < P.size(); i++) slots += (i ? 1.0f : 0.0f) + P[i].gap;
		const float band = (ROWS_BOTTOM - ROWS_TOP) * mid.h;
		const float pitch = std::min(DISC_PITCH * mid.h, band / (slots > 0 ? slots : 1));
		const float cap = std::min(ROW_CAP * mid.h, pitch * 0.50f);
		const float h = cap / fv2_text::CAP;

		float ink_top = mid.y + ROWS_TOP * mid.h + (band - pitch * slots) * 0.5f;
		for (size_t i = 0; i < P.size(); i++) {
			const line &l = P[i];
			ink_top += (i ? pitch : 0) + l.gap * pitch;
			const float y = ink_top - fv2_text::CAP_TOP * h;   // the cell's top, the ink starting at 16/64
			const bool on = int(i) == sel();
			if (l.what == line::BUTTON) {
				button_at(ren, mid, ink_top, cap, h, on);
				continue;
			}
			// the rule's line just under the capitals, which end at (16 + 31) / 64 of the cell
			if (on && l.what != line::DOOR) rule(ren, mid, y + h * ((16.0f + 31.0f) / fv2_text::CELL + 0.24f), h);
			if (l.what == line::DOOR) {
				// THE DOORS ARE THE TITLE'S OWN LETTERING, and nothing is drawn under them. They were bold,
				// slanted and underlined by the pink rule like any chosen line; Frederic, the 2026-10-29:
				// "enleve la ligne rose qui souligne, et utilise la meme police que le titre OPTIONS en un
				// peu plus petit". The yellow already says which line is chosen -- the rule said it twice --
				// and a door that is the title a size down says what it opens onto before it is opened.
				font->draw_title(ren, screen_name(l.which), mid.x + mid.w * 0.5f, ink_top, DOOR_CAP * mid.h,
								 on ? CHOSEN : PLAIN);
				continue;
			}
			if (l.what == line::OWN) {
				const own_line &o = own[at][size_t(l.which)];
				const SDL_Color col = on ? CHOSEN : ((o.step || o.act) ? PLAIN : MUTED);
				// A BUTTON THAT IS NOT INITIALIZE IS WRITTEN, NOT PAINTED. `as_button` meant one thing when
				// it was made -- the painted INITIALIZE label -- and the port's own buttons took it to mean
				// "a line that acts rather than sets", so RESET GAME and QUIT came out both reading
				// INITIALIZE on the page they sat on. Seen the 2026-10-29, when they moved to the foot of
				// OPTIONS. The painted label belongs to the name it spells; any other name is drawn in the
				// sheet's own letters, centred and thickened, as the doors above them are.
				if (o.as_button) {
					if (o.name == "INITIALIZE") { button_at(ren, mid, ink_top, cap, h, on); continue; }
					const float w = font->width(o.name.c_str(), h, 1.0f, fv2_text::THICK);
					font->draw(ren, o.name.c_str(), mid.x + mid.w * 0.5f - w * 0.5f, y, h, col, 255,
							   fv2_text::SLANT, 1.0f, fv2_text::THICK);
					continue;
				}
				font->draw(ren, o.name.c_str(), mid.x + LABEL_X * mid.w, y, h, col, 255, fv2_text::SLANT);
				if (o.lit && o.total > 0) gauge(ren, mid, y, h, o.lit(), o.total);
				else if (o.text) font->draw_mid(ren, o.text().c_str(), mid.x + VALUE_MID * mid.w, y, h, col,
												255, fv2_text::SLANT);
				continue;
			}
			const size_t k = size_t(l.which);
			const SDL_Color col = on ? CHOSEN : (read_only(k) ? MUTED : PLAIN);
			font->draw(ren, table()[k].name, mid.x + LABEL_X * mid.w, y, h, col, 255, fv2_text::SLANT);
			font->draw_mid(ren, value_text(board, k).c_str(), mid.x + VALUE_MID * mid.w, y, h, col, 255,
						   fv2_text::SLANT);
		}
	}

	// INITIALIZE is a painted label, not text. On the settings page it is the game's own idea -- 0x00020F74
	// puts the factory values back and 0x00021008 writes them to the backup -- and this does the same by the
	// table; on SOUNDS it is the volumes'.
	void button_at(SDL_Renderer *ren, const SDL_FRect &mid, float top, float cap, float h, bool on)
	{
		if (!init) return;
		const float bh = cap * 1.45f;
		const float bw = bh * float(init_w) / float(init_h ? init_h : 1);
		const SDL_FRect dst = { mid.x + mid.w * 0.5f - bw * 0.5f, top, bw, bh };
		if (on) rule(ren, mid, dst.y + bh * 1.30f, h);
		SDL_SetTextureAlphaMod(init, uint8_t(on ? 255 : 190));
		SDL_RenderTexture(ren, init, nullptr, &dst);
		SDL_SetTextureAlphaMod(init, 255);
	}

	// A volume, drawn as the owner drew it: a row of arrows, lit up to the setting. One per ten, nought to two
	// hundred, which is where his twenty steps come from.
	// The segments keep the owner's own colours: they are drawn lit or unlit, not tinted to the line. Tinting
	// them to the chosen line's yellow was the first try and it said nothing, the lit segment being yellow
	// already.
	static constexpr float GAUGE_L = 0.560f, GAUGE_R = 0.790f;   // between the labels and the rule's right end
	void gauge(SDL_Renderer *ren, const SDL_FRect &mid, float y, float h, int lit, int total)
	{
		if (!vol_on || !vol_off || total <= 0) return;
		const float step = (GAUGE_R - GAUGE_L) * mid.w / float(total);
		const float ah = h * 0.60f, aw = std::min(step * 0.92f, ah * 28.0f / 64.0f);
		float x = mid.x + GAUGE_L * mid.w;
		for (int i = 0; i < total; i++, x += step) {
			const SDL_FRect dst = { x, y + h * 0.22f, aw, ah };
			SDL_RenderTexture(ren, i < lit ? vol_on : vol_off, nullptr, &dst);
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
	// The pink rule under the chosen line, hung by its BOTTOM: its tick rises above the line, so placing it
	// by its top underlines the row below. The span is the settings page's by default; CONTROLLER passes its
	// own, being a wider grid.
	void rule(SDL_Renderer *ren, const SDL_FRect &mid, float bottom, float h, float l = RULE_L, float r = RULE_R)
	{
		const float rh = h * 0.26f;
		const SDL_FRect dst = { mid.x + l * mid.w, bottom - rh, (r - l) * mid.w, rh };
		SDL_RenderTexture(ren, cursor, nullptr, &dst);
	}
};
