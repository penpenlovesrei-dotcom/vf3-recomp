// The console's PAUSE MENU: Start during a match, over the fight, with the game frozen behind it.
//
// August had this menu and measured its four levers (docs/fv2-menu-console-inventaire.md section 2); this
// port never had it, and docs/fv2-console.md said why -- "Escape already does everything that menu would, so
// this was left until the modes are finished". The modes were finished on the 2026-10-23, so here it is.
//
// WHAT "BETTER INTEGRATED" MEANS HERE, since Frederic asked for that and not merely for the four lines:
//
//   * it is a surface like the other two, not a page of this port's menu. The list of modes and the OPTIONS
//     page already freeze the game, take the keys and the pad, and draw over the picture the game made; this
//     one is built the same way and says the same things to the host (want_sound, CANCELLED).
//   * it is drawn with the GAME'S OWN LETTERS, the glyph sheet the OPTIONS page uses. Nothing here is RmlUi.
//   * the fight stays visible under it and is NOT darkened: August's panel is dark enough to carry a list on
//     its own, and the fight is what the player paused to look at (the form, below).
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
#include "fv2_texte.h"
#include "png.h"

struct fv2_pause
{
	// The four lines are August's, in his order.
	enum { TITLE_SCREEN, CHANGE_VIPER, REMATCH, CHANGE_STAGE, LINES };
	static const char *name(int l)
	{
		static const char *const N[LINES] = { "TITLE SCREEN", "CHANGE YOUR VIPER", "REMATCH", "CHANGE STAGE" };
		return l >= 0 && l < LINES ? N[l] : "?";
	}

	static constexpr int CANCELLED = -2;          // backed out: the host resumes the game
	enum { SND_NONE, SND_MOVE, SND_VALIDATE, SND_CANCEL };
	int want_sound = SND_NONE;

	// THE FORM IS AUGUST'S, COPIED: CMenuOverlay::DrawText's STYLE_PANEL branch, in
	// Documents\SEGA Model 3\SupermodelSrc\Src\OSD\SDL\MenuOverlay.cpp. Frederic asked for it the
	// 2026-10-25, in place of the Dreamcast panel this had before -- a violet box under a pale header bar,
	// standing on the side of the player who paused, over a darkened fight.
	//
	// What August's panel is, in his own words and his own numbers:
	//
	//   * "A window, sized to what it holds, and centred. The game stays visible around it." BOTH dimensions
	//     follow the content, because three lines for a pause and fourteen for the settings cannot share one
	//     size. So nothing below is a box: they are a line's height, a line's pitch, a margin, and the rest
	//     is measured off the words themselves;
	//   * THE SCREEN IS NOT DARKENED. The dark panel is what lifts the list off a running fight, and it does
	//     it alone -- a dim as well would be the same job done twice, and the fight is what the player
	//     paused to look at;
	//   * "A rule top and bottom rather than a full border: it reads as a panel without the corners having
	//     to line up at every window size";
	//   * amber on the chosen line, pale grey-blue on the others, and the heading in white above the first
	//     line, "drawn here rather than as a list entry so the cursor cannot land on it".
	//
	// His widths are in cells of his own font; here they are measured with this port's (fv2_texte::width),
	// which is the same intent said in this port's terms -- the panel is as wide as its longest line plus
	// his three cells of air, never under PANEL_HALF_W_MIN and never over PANEL_HALF_W_MAX.
	static constexpr float PANEL_CENTRE_Y = 0.50f;
	static constexpr float PANEL_HALF_W_MIN = 0.21f, PANEL_HALF_W_MAX = 0.48f;
	static constexpr float PANEL_MARGIN_Y = 0.055f;
	static constexpr float PANEL_LINE_STEP = 0.062f, PANEL_LINE_HEIGHT = 0.060f;
	static constexpr float PANEL_AIR = 0.675f;    // his "longest + 3 cells", as a share of a line's height
	static constexpr float TITLE_GAP = 1.2f;      // the heading, in line-steps above the first line
	static constexpr float RULE_H = 0.004f;
	static constexpr SDL_Color CHOSEN = { 255, 209, 26, 255 };    // amber on a dark ground
	static constexpr SDL_Color PLAIN = { 204, 204, 219, 255 };
	static constexpr SDL_Color MUTED = { 122, 122, 134, 255 };    // a line with no lever behind it
	static constexpr SDL_Color PANEL = { 8, 8, 18, 217 };         // 0.03 0.03 0.07, alpha 0.85
	static constexpr SDL_Color RULE = { 217, 217, 230, 230 };     // 0.85 0.85 0.90, alpha 0.9
	static constexpr SDL_Color TITLE_INK = { 255, 255, 255, 255 };

	bool shown = false;
	int who = 0;                   // the player who paused: the panel stands on his side, 1P left, 2P right
	int sel = TITLE_SCREEN;
	int stage = 3;                 // CHANGE STAGE's choice; 3 is MARBLE ARENA, the one this port already names
	bool ready = false, tried = false;
	SDL_Texture *cursor = nullptr;
	fv2_texte *font = nullptr;     // the sheet, loaded once by the host and shared
	int stick_x = 0, stick_y = 0;

	// WHICH LINES HAVE A LEVER. Three do now; one still does not, and it is not forgotten -- it is waiting on
	// a measurement, named here so the next session does not have to find the question again:
	//
	//   REMATCH            WIRED the 2026-10-25, by August's own road (Model3::fv2_rematch): the game already
	//                      restarts a fight without the select screen, because it does it between every
	//                      round. Five writes, the request for GAME_INT and four that undo the match just
	//                      fought, and the loser's task handed back.
	//   CHANGE STAGE       the stage picker at 0x0007111C, held.
	//   TITLE SCREEN       August's lever is this port's fv2_attract_to_title -- push the attract sequencer
	//                      to the end of its list with its timer expired -- and MEASURED AGAIN FROM A FIGHT
	//                      the 2026-10-25 it still does nothing: chosen at frame 2100 of a running round,
	//                      the game's mode and sub-state do not move for the next seven hundred frames. The
	//                      sequencer is not running there, so there is no list to walk off.
	//   CHANGE YOUR VIPER  ends the match off screen "in about two seconds, with three bytes" -- the round
	//                      counter 0x00581455, the round timer 0x00581406 and the screen-duration counter
	//                      0x0058FD56. August's note gives the three ADDRESSES and not the three VALUES, and
	//                      a match ended with the wrong ones is a match ended wrongly. STILL NO LEVER.
	//
	// AND REMATCH SETTLES SOMETHING THOSE TWO WERE BLAMED FOR. This port's own note on fv2_request_title
	// said a mode request "commands nothing from inside a fight". It does: REMATCH's request for GAME_INT,
	// written at frame 2100 of a running round, is obeyed at 2101. What fails from a fight is the ATTRACT
	// SEQUENCER, which is a different machine and is simply not turning. The two remaining lines are
	// therefore waiting on a way to END THE MATCH, not on a way to be heard.
	static bool has_lever(int) { return true; }

	bool load(SDL_Renderer *ren, const std::string &assets, fv2_texte *sheet, FILE *log)
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
		who = player ? 1 : 0;
		sel = TITLE_SCREEN;
		stick_x = stick_y = 0;
	}
	void hide() { shown = false; }

	void move(int d)
	{
		for (int i = 0; i < LINES; i++) {
			sel = (sel + LINES + d) % LINES;
			if (has_lever(sel)) break;      // a line with no lever is stepped OVER, never dropped
		}
		want_sound = SND_MOVE;
	}

	// One event while the menu is up. Returns a line (0..LINES-1) when one is chosen, CANCELLED when the
	// player backs out, -1 otherwise.
	int handle_event(const SDL_Event &e, const player_controls ctl[2])
	{
		if (!shown) return -1;
		auto bound = [&](int action, SDL_Scancode sc) {
			for (int p = 0; p < 2; p++) if (ctl[p].key[action] == sc) return true;
			return false;
		};
		auto sideways = [&](int d) {
			if (sel != CHANGE_STAGE) return;
			stage = (stage + 16 + d) % 16;
			want_sound = SND_MOVE;
		};
		switch (e.type) {
		case SDL_EVENT_KEY_DOWN: {
			if (e.key.repeat) return -1;
			const SDL_Scancode sc = e.key.scancode;
			if (sc == SDL_SCANCODE_UP || bound(C_UP, sc)) move(-1);
			else if (sc == SDL_SCANCODE_DOWN || bound(C_DOWN, sc)) move(+1);
			else if (sc == SDL_SCANCODE_LEFT || bound(C_LEFT, sc)) sideways(-1);
			else if (sc == SDL_SCANCODE_RIGHT || bound(C_RIGHT, sc)) sideways(+1);
			// Start closes what Start opened. It is listed before A so that a player whose Start is also a
			// game button cannot choose a line with it by accident.
			else if (bound(C_START, sc)) { want_sound = SND_CANCEL; return CANCELLED; }
			else if (sc == SDL_SCANCODE_BACKSPACE || bound(C_KICK, sc)) { want_sound = SND_CANCEL; return CANCELLED; }
			else if (sc == SDL_SCANCODE_RETURN || sc == SDL_SCANCODE_KP_ENTER || sc == SDL_SCANCODE_SPACE
					 || bound(C_PUNCH, sc)) {
				if (!has_lever(sel)) return -1;
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
				if (!has_lever(sel)) return -1;
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

	// Over the picture the game made: the fight darkened, then the panel on the pauser's side. mid is the
	// game's own 4:3 screen, which every number above is a fraction of.
	void render(SDL_Renderer *ren, const SDL_FRect &all, const SDL_FRect &mid)
	{
		if (!shown || !ready || !font) return;
		(void)all;      // the game is NOT darkened: the panel is what lifts the list off it (see above)
		SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);

		// Every line first, as it will be written, so the panel can be measured against them rather than
		// guessed at. CHANGE STAGE carries its scene number, which is part of how wide it is.
		char lines[LINES][40];
		for (int i = 0; i < LINES; i++) {
			if (i == CHANGE_STAGE) snprintf(lines[i], sizeof lines[i], "%s %d", name(i), stage);
			else snprintf(lines[i], sizeof lines[i], "%s", name(i));
		}
		char head[16];
		snprintf(head, sizeof head, "%s PAUSE", who ? "2P" : "1P");

		const float cap = PANEL_LINE_HEIGHT * mid.h, line_h = cap / fv2_texte::CAP;
		const float step = PANEL_LINE_STEP * mid.h;
		const float span = step * float(LINES - 1);
		const float mx = mid.x + mid.w * 0.5f;

		// The block of lines sits on the panel's centre, and the heading a step and a fifth above the first
		// of them -- inside the panel, which grows upwards to hold it.
		const float first_y = mid.y + PANEL_CENTRE_Y * mid.h - span * 0.5f - cap * 0.5f;
		const float title_y = first_y - step * TITLE_GAP;
		const float top = title_y - PANEL_MARGIN_Y * mid.h;
		const float bottom = first_y + span + cap + PANEL_MARGIN_Y * mid.h;

		// As wide as its longest line and his three cells of air, held between his two limits
		float widest = font->width(head, line_h);
		for (int i = 0; i < LINES; i++) widest = std::max(widest, font->width(lines[i], line_h));
		float half_w = widest * 0.5f + PANEL_AIR * cap;
		half_w = std::clamp(half_w, PANEL_HALF_W_MIN * mid.w, PANEL_HALF_W_MAX * mid.w);

		const SDL_FRect box = { mx - half_w, top, half_w * 2.0f, bottom - top };
		SDL_SetRenderDrawColor(ren, PANEL.r, PANEL.g, PANEL.b, PANEL.a);
		SDL_RenderFillRect(ren, &box);
		// A rule top and bottom rather than a full border: no corners to line up at every window size.
		const float rule = RULE_H * mid.h;
		SDL_SetRenderDrawColor(ren, RULE.r, RULE.g, RULE.b, RULE.a);
		const SDL_FRect r_top = { box.x, top, box.w, rule };
		const SDL_FRect r_bot = { box.x, bottom - rule, box.w, rule };
		SDL_RenderFillRect(ren, &r_top);
		SDL_RenderFillRect(ren, &r_bot);

		// The heading, white, centred, and not a list entry: the cursor cannot land on it.
		font->draw_mid(ren, head, mx, title_y, line_h * 1.15f, TITLE_INK);

		// The lines, centred, amber where the cursor is and grey where no lever has been found yet
		for (int i = 0; i < LINES; i++) {
			const SDL_Color col = !has_lever(i) ? MUTED : i == sel ? CHOSEN : PLAIN;
			font->draw_mid(ren, lines[i], mx, first_y + float(i) * step, line_h, col);
		}
	}

private:
};
