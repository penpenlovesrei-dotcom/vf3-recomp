// The console's STAGE SELECT for VERSUS: which arena the two players fight in, chosen before the match.
//
// WHY THIS PAGE EXISTS AT ALL, which is a measurement and not a wish. Frederic remembered that the summer's
// patched ROM had introduced a stage select for versus. Read in Sega's code the 2026-10-24, the arcade board
// has none: the SEL dispatcher (0x0000BA74) jumps through a sub-handler table at 0x00024404, and there the two
// states Sega named STAGE_SEL_INT and STAGE_SEL_DSP share one address, 0x0000BBB4, which holds a single
// instruction -- `b 0x0000bad8`, the dispatcher's own exit. An EMPTY STUB. The names are in the debug table
// and the slots are in the handler table and there is nothing inside them. So a stage select for this game is
// a thing to build, not a thing to reach, and this is the page half of it (docs/fv2-console.md).
//
// AND THE LEVER IS THE ONE FROM AUGUST, which this port had written off. `Model3::fv2_hold_stage` patches the
// function that chooses the stage, 0x0007111C, into `li r3,<scene> ; blr`. The note in console_play.h said it
// "does NOT command this" -- true of the FIRST fight of a solo route, which comes from the game's route table
// instead, and that is what was measured then. It is NOT true of a versus: held at scene 3 from frame 1990 to
// 2900 of tools/scripts/fv2-vic-banc.txt, with both patched words read back out of the dump, the versus fought
// in scene 3 where it fought in scene 8 untouched.
//
// It is a surface like the other three -- the list of modes, OPTIONS, the pause menu: it freezes the game,
// takes the keys and the pad, draws with the game's own letters over the picture the game made, and tells the
// host only what it wants heard (want_sound, CANCELLED). Nothing here is RmlUi.
#pragma once
#include <SDL3/SDL.h>
#include <string>
#include <vector>
#include "controls.h"
#include "fv2_texte.h"
#include "png.h"

struct fv2_stage_list
{
	// Entry 0 leaves the choice to the game, which is what a versus does on the board. The twelve others are
	// Sega's scene numbers, and the names are the owner's, measured on screen stage by stage (model3.cpp).
	enum { LINES = 13 };
	static const char *name(int l)
	{
		static const char *const N[LINES] = {
			"THE GAME'S OWN",
			"OLD ARMSTONE TOWN", "DECK OF THE DARK ROYAL", "MARBLE ARENA", "THE GREAT B.M",
			"TERROR FURNACE", "SERPENT'S CAGE", "HANGER OF THE DARK ROYAL", "SECRET CHEMICAL PLANT",
			"CROSS RIVER DAMSITE", "NEO ARMSTONE CITY TOWER", "THE SKIES OF ARMSTONE", "GREAT MUSEUM",
		};
		return l >= 0 && l < LINES ? N[l] : "?";
	}
	// The scene the game wants for a line: 0 means "do not hold anything".
	static int scene_of(int l) { return l >= 1 && l < LINES ? l : 0; }

	static constexpr int CANCELLED = -2;          // backed out: the host puts the list of modes back
	enum { SND_NONE, SND_MOVE, SND_VALIDATE, SND_CANCEL };
	int want_sound = SND_NONE;

	// The same form as the pause menu's panel, in fractions of the game's own 4:3 screen, but centred and
	// wider: thirteen entries do not fit one column, so they run down two, seven then six.
	static constexpr uint8_t DIM_ALPHA = 110;
	static constexpr float PANEL_W = 0.760f, PANEL_H = 0.400f;
	static constexpr float PANEL_TOP = 0.300f;
	static constexpr float BAR_H = 0.052f;
	static constexpr float TEXT_IN = 0.028f;
	static constexpr float ROW_TOP = 0.055f, PITCH = 0.0505f, ROW_CAP = 0.0290f;
	static constexpr float COL_2 = 0.500f;        // the second column, as a fraction of the panel
	static constexpr float FOOT_Y = 0.906f, FOOT_L = 0.300f, FOOT_R = 0.715f;
	static constexpr int COL_ROWS = 7;            // seven on the left, the rest on the right
	static constexpr SDL_Color CHOSEN = { 252, 252, 10, 255 };
	static constexpr SDL_Color PLAIN = { 247, 246, 246, 255 };
	static constexpr SDL_Color PANEL = { 150, 62, 140, 190 };
	static constexpr SDL_Color BAR = { 226, 226, 226, 235 };
	static constexpr SDL_Color BAR_INK = { 32, 32, 32, 255 };

	bool shown = false;
	int sel = 0;
	bool ready = false, tried = false;
	SDL_Texture *cursor = nullptr;
	fv2_texte *font = nullptr;     // the sheet, loaded once by the host and shared
	int stick_x = 0, stick_y = 0;

	bool load(SDL_Renderer *ren, const std::string &assets, fv2_texte *sheet, FILE *log)
	{
		if (tried) return ready;
		tried = true;
		font = sheet;
		if (!font || !font->ready) {
			if (log) fprintf(log, "the console's stage select: no glyph sheet -- VERSUS starts without it\n");
			return false;
		}
		const std::string path = assets + "/menu-console/option_cursor.png";
		std::vector<uint32_t> px;
		int w = 0, h = 0;
		std::string why;
		if (!read_png(path.c_str(), px, w, h, why)) {
			if (log) fprintf(log, "the console's stage select: %s %s -- VERSUS starts without it\n", path.c_str(), why.c_str());
			return false;
		}
		cursor = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STATIC, w, h);
		if (!cursor) {
			if (log) fprintf(log, "the console's stage select: no texture for %s (%s)\n", path.c_str(), SDL_GetError());
			return false;
		}
		SDL_UpdateTexture(cursor, nullptr, px.data(), w * 4);
		SDL_SetTextureBlendMode(cursor, SDL_BLENDMODE_BLEND);
		SDL_SetTextureScaleMode(cursor, SDL_SCALEMODE_LINEAR);
		ready = true;
		if (log) fprintf(log, "the console's stage select: its cursor and the glyph sheet are in\n");
		return true;
	}

	void textures_lost(SDL_Renderer *ren, const std::string &assets)
	{
		if (cursor) { SDL_DestroyTexture(cursor); cursor = nullptr; }
		ready = false;
		tried = false;
		load(ren, assets, font, nullptr);
	}

	void show()
	{
		if (!ready) return;
		shown = true;
		stick_x = stick_y = 0;
	}
	void hide() { shown = false; }

	// Down and up run within a column; left and right change column, keeping the row where it is. Every line
	// has a lever here -- entry 0 included, whose lever is to hold nothing -- so nothing is stepped over.
	void move(int d)
	{
		sel = (sel + LINES + d) % LINES;
		want_sound = SND_MOVE;
	}
	void sideways(int d)
	{
		const int row = sel % COL_ROWS, col = sel / COL_ROWS;
		const int to = (col + 2 + d) % 2;
		const int at = to * COL_ROWS + row;
		sel = at < LINES ? at : LINES - 1;
		want_sound = SND_MOVE;
	}

	// One event while the page is up. Returns a line (0..LINES-1) when one is chosen, CANCELLED when the
	// player backs out, -1 otherwise.
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
			if (sc == SDL_SCANCODE_UP || bound(C_UP, sc)) move(-1);
			else if (sc == SDL_SCANCODE_DOWN || bound(C_DOWN, sc)) move(+1);
			else if (sc == SDL_SCANCODE_LEFT || bound(C_LEFT, sc)) sideways(-1);
			else if (sc == SDL_SCANCODE_RIGHT || bound(C_RIGHT, sc)) sideways(+1);
			// Start backs out, as it closes the pause menu: listed before A so a player whose Start is also a
			// game button cannot choose a stage with it by accident.
			else if (bound(C_START, sc)) { want_sound = SND_CANCEL; return CANCELLED; }
			else if (sc == SDL_SCANCODE_ESCAPE) return -1;   // (Escape belongs to the host's own menu)
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
			case SDL_GAMEPAD_BUTTON_DPAD_UP: move(-1); break;
			case SDL_GAMEPAD_BUTTON_DPAD_DOWN: move(+1); break;
			case SDL_GAMEPAD_BUTTON_DPAD_LEFT: sideways(-1); break;
			case SDL_GAMEPAD_BUTTON_DPAD_RIGHT: sideways(+1); break;
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

	// Over the picture the game made: darkened, then the panel. mid is the game's own 4:3 screen, which every
	// number above is a fraction of.
	void render(SDL_Renderer *ren, const SDL_FRect &all, const SDL_FRect &mid)
	{
		if (!shown || !ready || !font) return;
		SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
		SDL_SetRenderDrawColor(ren, 0, 0, 0, DIM_ALPHA);
		SDL_RenderFillRect(ren, &all);

		const float w = PANEL_W * mid.w, h = PANEL_H * mid.h;
		const float x = mid.x + (mid.w - w) * 0.5f;
		const float y = mid.y + PANEL_TOP * mid.h;
		const float bar = BAR_H * mid.h;

		const SDL_FRect bar_r = { x, y - bar, w, bar };
		SDL_SetRenderDrawColor(ren, BAR.r, BAR.g, BAR.b, BAR.a);
		SDL_RenderFillRect(ren, &bar_r);
		const SDL_FRect box = { x, y, w, h };
		SDL_SetRenderDrawColor(ren, PANEL.r, PANEL.g, PANEL.b, PANEL.a);
		SDL_RenderFillRect(ren, &box);
		const SDL_FRect edge = { x - bar * 0.09f, y - bar, bar * 0.09f, h + bar };
		SDL_SetRenderDrawColor(ren, BAR.r, BAR.g, BAR.b, BAR.a);
		SDL_RenderFillRect(ren, &edge);

		const float bar_cap = bar * 0.56f, bar_h = bar_cap / fv2_texte::CAP;
		font->draw(ren, "VS", x + TEXT_IN * mid.w * 0.5f, y - bar + bar * 0.20f, bar_h, BAR_INK);
		font->draw(ren, "STAGE SELECT", x + w * 0.16f, y - bar + bar * 0.20f, bar_h, BAR_INK);

		const float cap = ROW_CAP * mid.h, line_h = cap / fv2_texte::CAP;
		for (int i = 0; i < LINES; i++) {
			const int row = i % COL_ROWS, col = i / COL_ROWS;
			const float lx = x + TEXT_IN * mid.w + (col ? COL_2 * w : 0.0f);
			const float ly = y + (ROW_TOP + float(row) * PITCH) * mid.h;
			font->draw(ren, name(i), lx, ly, line_h, i == sel ? CHOSEN : PLAIN);
		}

		const float fh = ROW_CAP * mid.h / fv2_texte::CAP * 0.58f;
		font->draw_mid(ren, "USE THE DIRECTION BUTTON TO SELECT", mid.x + FOOT_L * mid.w, mid.y + FOOT_Y * mid.h, fh, PLAIN);
		font->draw_mid(ren, "USE THE A BUTTON TO DECIDE", mid.x + FOOT_R * mid.w, mid.y + FOOT_Y * mid.h, fh, PLAIN);
	}
};
