// TRAINING's column of inputs, and the damage opposite it.
//
// THE FORM IS AUGUST'S (MenuConsole.md section 9 quinquies), and so is the picture: inputs.png, 512x512,
// drawn by Frederic. His grid was MEASURED and not assumed -- the eleven drawings were isolated one by one
// by walking the opaque pixels, and their boxes fall on a regular step of 96: the three discs at x 8..92,
// 104..188, 200..284 on y 8..92, the arrows centred at x 48, 144, 240, 336. Three rows of four cells of 96.
//
// THE ORDER OF THE CELLS IS A ROTATION, which is what spares a table: left, up-left, up, up-right on the
// first row of arrows, then right, down-right, down, down-left on the second -- each 45 degrees after the
// one before. The whole cell is drawn, drop shadow and all: the drawings are not all centred the same way
// inside their cells, and re-cropping them one by one would make the column jitter.
//
// THE THREE BUTTONS EACH HAVE THEIR OWN PLACE, taken or empty. Packed against the arrow they slid from one
// line to the next depending on what was held, and a column that dances cannot be read at a glance -- which
// is the only thing it is for.
//
// IT READS THE PLAYER'S INPUTS, NOT THE GAME'S MEMORY, and that is a choice. The column is for reading
// yourself back, so it must show what you DID, including what the game ignored: a move that did not come out
// because a direction was one frame short then shows, and that is the whole point of it.
//
// AND IT IS DRAWN ON THE WHOLE WINDOW, not on the game's 4:3 picture. August's column would not move in 16:9
// through two corrections that were each right in principle, because the cause was elsewhere: the render's
// viewport stays 4:3, so no fraction computed on the game's frame can reach the edge of the screen. The
// lesson he wrote down is worth keeping -- two successive corrections in the right direction do not validate
// a diagnosis -- and the fix is to hang this off `all`, like the end artwork and the frame counter.
#pragma once
#include <SDL3/SDL.h>
#include <string>
#include <vector>
#include "fv2_texte.h"
#include "png.h"

struct fv2_training
{
	static constexpr int CELL = 96;           // the atlas's step, measured on the file
	static constexpr int LINES = 8;           // how many inputs stay on screen
	// In fractions of the WINDOW's height, so the column keeps its proportions whatever its shape. The left
	// margin is a height too, which is what stops it drifting away from the edge as the window widens.
	//
	// AND IT IS ANCHORED AT ITS FOOT, not at its head. August's grew downwards from a fixed top, so the line
	// you had just played moved every time -- and the one thing this column is for is being read at a glance
	// while you fight. Hung from the bottom, the newest line is always at the same place and the older ones
	// slide up out of it.
	static constexpr float MARGIN = 0.028f;
	static constexpr float BOTTOM = 0.945f;   // where the newest line's foot sits
	static constexpr float ROW_H = 0.047f;    // one line's height, arrow and buttons alike
	static constexpr float GAP = 0.008f;      // between the arrow and the first button
	static constexpr float DMG_CAP = 0.026f;  // the damage figure's capitals
	// The ground it is read on. A fight is a bright, busy picture and white arrows on it are a guess; this is
	// the carousel's own band, narrower. Translucent, so the arena still shows through it.
	static constexpr uint8_t BAND_ALPHA = 165;
	static constexpr float BAND_PAD = 0.010f;
	// The older lines fade, so the eye falls on the newest without having to look for it. This is the port's
	// own, not August's: his were all at full.
	static constexpr uint8_t OLD_ALPHA = 115, NEW_ALPHA = 255;
	static constexpr SDL_Color DMG_INK = { 236, 48, 48, 255 };

	// One line of the column: what was held, and what it cost the other player.
	struct line {
		int dir = -1;                 // 0..7 in the atlas's rotation, -1 for neutral
		bool punch = false, kick = false, guard = false;
		int dmg = 0;                  // points taken off the opponent, credited to this line
	};

	bool ready = false, tried = false;
	SDL_Texture *atlas = nullptr;
	int atlas_w = 0, atlas_h = 0;
	fv2_texte *font = nullptr;
	std::vector<line> lines;          // oldest first; the newest is at the bottom
	line held;                        // what is held right now, to tell a change from a repeat

	bool load(SDL_Renderer *ren, const std::string &assets, fv2_texte *sheet, FILE *log)
	{
		if (tried) return ready;
		tried = true;
		font = sheet;
		std::vector<uint32_t> px;
		std::string why;
		const std::string path = assets + "/menu-console/inputs.png";
		if (!read_png(path.c_str(), px, atlas_w, atlas_h, why)) {
			if (log) fprintf(log, "the console's training: %s %s\n", path.c_str(), why.c_str());
			return false;
		}
		atlas = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STATIC, atlas_w, atlas_h);
		if (!atlas) {
			if (log) fprintf(log, "the console's training: no texture for the inputs (%s)\n", SDL_GetError());
			return false;
		}
		SDL_UpdateTexture(atlas, nullptr, px.data(), atlas_w * 4);
		SDL_SetTextureBlendMode(atlas, SDL_BLENDMODE_BLEND);
		SDL_SetTextureScaleMode(atlas, SDL_SCALEMODE_LINEAR);
		ready = true;
		if (log) fprintf(log, "the console's training: the inputs are in, %dx%d\n", atlas_w, atlas_h);
		return true;
	}

	void textures_lost(SDL_Renderer *ren, const std::string &assets)
	{
		if (atlas) { SDL_DestroyTexture(atlas); atlas = nullptr; }
		ready = false;
		tried = false;
		load(ren, assets, font, nullptr);
	}

	void clear() { lines.clear(); held = line(); }

	// The eight directions in the atlas's own order, which is a rotation and not a table: left, up-left, up,
	// up-right, right, down-right, down, down-left.
	static int direction(bool up, bool down, bool left, bool right)
	{
		const int dx = (right ? 1 : 0) - (left ? 1 : 0), dy = (down ? 1 : 0) - (up ? 1 : 0);
		if (!dx && !dy) return -1;
		if (dx < 0 && !dy) return 0;
		if (dx < 0 && dy < 0) return 1;
		if (!dx && dy < 0) return 2;
		if (dx > 0 && dy < 0) return 3;
		if (dx > 0 && !dy) return 4;
		if (dx > 0 && dy > 0) return 5;
		if (!dx && dy > 0) return 6;
		return 7;
	}

	// One frame of the player's own controls. A line is pushed only when what is held CHANGES, so holding a
	// direction is one line and not sixty.
	void feed(bool up, bool down, bool left, bool right, bool punch, bool kick, bool guard)
	{
		line now;
		now.dir = direction(up, down, left, right);
		now.punch = punch;
		now.kick = kick;
		now.guard = guard;
		if (now.dir == held.dir && now.punch == held.punch && now.kick == held.kick && now.guard == held.guard)
			return;
		held = now;
		// A LINE WITH NOTHING ON IT IS NOT A LINE. Once neutral became an empty place rather than a letter
		// (Frederic, the 2026-10-25), going back to neutral pushed a row with nothing drawn on it, and the
		// column came out half blank. The change is still remembered in `held`, so the input after it is
		// still seen as a change; it simply leaves no row behind.
		if (now.dir < 0 && !now.punch && !now.kick && !now.guard) return;
		lines.push_back(now);
		while (lines.size() > LINES) lines.erase(lines.begin());
	}

	// THE DAMAGE, AND ITS ATTRIBUTION IS A HEURISTIC, SAID OUT LOUD. A blow does not take its points on the
	// frame the button goes down -- there is the animation, the reach, the guard -- so by the time the life
	// drops the newest line is often back at neutral, and crediting it would read as "resting did forty
	// points". So it walks back to the LAST LINE THAT HAD A BUTTON. Right on an isolated blow, which is the
	// case a training is for; wrong in a fast series where two attacks overlap, the second one being
	// credited. Following the game's animation instead would be another piece of work.
	void damage(int points)
	{
		if (points <= 0) return;
		for (size_t i = lines.size(); i-- > 0;)
			if (lines[i].punch || lines[i].kick || lines[i].guard) { lines[i].dmg += points; return; }
		if (!lines.empty()) lines.back().dmg += points;
	}

	void render(SDL_Renderer *ren, const SDL_FRect &all, const SDL_FRect &mid)
	{
		(void)mid;
		// AND IT IS DRAWN WITH NOTHING IN IT TOO. The band is a fixed place on the screen, not a thing that
		// grows: Frederic, the 2026-10-25, "la colonne noire varie de taille selon son contenu (elle devrait
		// etre fixe)". So it is always LINES tall and always as wide as its widest line with its damage
		// column, whatever is in it at the moment.
		if (!ready || !atlas) return;
		SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
		const float h = ROW_H * all.h, gap = GAP * all.h, pad = BAND_PAD * all.h;
		const float x0 = all.x + MARGIN * all.h;
		const float cap = DMG_CAP * all.h;
		// As wide as the widest line it will ever hold -- an arrow and the three button places -- so the
		// band does not breathe in and out as the inputs change. The damage column is added only when some
		// line actually carries one, because a band kept open for figures that never come is a band of
		// empty ground, and this one sits on top of the fight.
		const float dmg_x = x0 + h + gap + 3.0f * h + gap;
		const float wide = dmg_x - x0 + (font && font->ready ? font->width("DMG 000", cap / fv2_texte::CAP) : 4.0f * cap);
		const float foot = all.y + BOTTOM * all.h;
		// the band holds its whole height from the first frame; the lines themselves still pile up from the
		// foot, so the newest is always in the same place
		const float head = foot - float(lines.size()) * h;
		const SDL_FRect band = { x0 - pad, foot - float(LINES) * h - pad, wide + 2.0f * pad, float(LINES) * h + 2.0f * pad };
		SDL_SetRenderDrawColor(ren, 0, 0, 0, BAND_ALPHA);
		SDL_RenderFillRect(ren, &band);

		auto cell = [&](int col, int row, float cx, float cy, uint8_t a) {
			const SDL_FRect src = { float(col * CELL), float(row * CELL), float(CELL), float(CELL) };
			const SDL_FRect dst = { cx, cy, h, h };
			SDL_SetTextureAlphaMod(atlas, a);
			SDL_RenderTexture(ren, atlas, &src, &dst);
			SDL_SetTextureAlphaMod(atlas, 255);
		};
		// Oldest at the top, newest hard against the foot, which never moves.
		float y = head;
		for (size_t i = 0; i < lines.size(); i++, y += h) {
			const line &l = lines[i];
			// The newest at full, the ones before it fading back evenly to OLD_ALPHA
			const float share = lines.size() > 1 ? float(i) / float(lines.size() - 1) : 1.0f;
			const uint8_t a = uint8_t(OLD_ALPHA + share * float(NEW_ALPHA - OLD_ALPHA));
			// The direction, then the three buttons each in its OWN place, taken or empty
			// NEUTRAL IS AN EMPTY PLACE, not a mark. August put the letter N there, with the free cell of
			// the atlas's first row reserved for a drawing if one ever came; Frederic chose the empty place
			// the 2026-10-25, so the column shows only what was actually held. The place is still kept --
			// the buttons do not slide left into it -- which is the rule that stops the column dancing.
			if (l.dir >= 0) cell(l.dir % 4, 1 + l.dir / 4, x0, y, a);
			const bool on[3] = { l.punch, l.guard, l.kick };    // the atlas's own order: P, G, K
			for (int i3 = 0; i3 < 3; i3++)
				if (on[i3]) cell(i3, 0, x0 + h + gap + float(i3) * h, y, a);
			if (l.dmg > 0 && font && font->ready) {
				char t[24];
				snprintf(t, sizeof t, "DMG %d", l.dmg);
				// ON THE CELL'S OWN MIDDLE, and that needs the font's own geometry. draw() is given the top
				// of the text's BOX, and the capitals begin CAP_TOP into it and are CAP tall; centring the
				// box left the figures riding high against the arrows, which are centred in their cells.
				// Frederic saw it as the DMG line being out of line with the icons.
				const float fh = cap / fv2_texte::CAP;
				const float top = y + h * 0.5f - cap * 0.5f - fv2_texte::CAP_TOP * fh;
				font->draw(ren, t, dmg_x, top, fh, DMG_INK, a);
			}
		}
	}
};
