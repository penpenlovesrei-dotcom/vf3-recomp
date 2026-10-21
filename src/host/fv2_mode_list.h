// Fighting Vipers 2: the list of modes, drawn over the game's own title screen.
//
// WHAT THIS IS NOT, because it was got wrong twice: a page of this port's menu. The menu under Escape is this
// port's settings -- an RmlUi document with its own backdrop over the frozen game -- and a page added to it is a
// page of settings whatever is written in it. This is a surface of its own: the owner's own picture of the seven
// lines, drawn straight over the picture the game made, on the game's real title screen, with nothing else on
// it. Nothing here goes through game_menu.
//
// The picture is assets/menu-console/title_list.png, 709 x 897 RGBA, one band a line: ARCADE, V.S., RANDOM,
// SURVIVAL, NETWORK, TRAINING, OPTIONS. Every number below was measured and is written down in
// docs/fv2-etats.md; none of it is a guess:
//
//   - the seven bands are cut in the middles of the white, not into seven equal parts: the labels are 114 to 118
//     pixels tall, and equal parts would crop the tallest;
//   - the labels are painted LEFT-ALIGNED in the file -- V.S. has its ink centred at 0.17 of the width where
//     NETWORK has it at 0.50 -- so each band is slid sideways to bring its own ink to the middle of the block;
//   - the block sits at 0.26157 of the picture's height, is 0.35557 of it tall, and is centred across. Only the
//     vertical placement comes from that measurement: it was taken on a 3840 x 2160 capture of a 4:3 picture
//     shown wide -- the two were reduced to the same thing, the bounding box of the yellow outline, and the
//     picture placed so the two coincide -- so the width follows the file's own proportions instead.
//
// Then two things the owner asked for on top of the measurement, and they are kept as factors APPLIED to it
// rather than as the figures they work out to: both give the same picture, but this way the measurement stays
// readable and what was added afterwards still says so. The block is grown by 7/6 about its own middle -- the
// figure comes from the list as it was when six lines were drawn and a seventh was written under them, and it
// is kept as the owner left it rather than recomputed for seven -- and lifted by 50/1080 of the height.
//
// It has the game's own sounds under it -- moving, choosing, backing out -- played by the game's own sound
// board, which the host keeps running while the game itself is frozen (the codes are below).
//
// And the picture behind it goes to half while the list is up, as the Dreamcast version does -- measured on two
// captures of that version, the same screen with its menu and without, where 255 becomes 128 on all three
// channels. Black at exactly half, not a figure chosen by eye.
//
// ONE VERSION OF EACH LINE EXISTS. None of the owner's 45 files is a "selected" variant, so the chosen line is
// marked by what it does rather than by what it is: it BLINKS, its alpha falling to 0.5 and back over 45
// frames -- 0.75 s, read from the clock rather than counted -- while the other six stay at full. That is the
// owner's own answer to the same problem.
#pragma once
#include <SDL3/SDL.h>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>
#include "controls.h"
#include "png.h"

struct fv2_mode_list
{
	enum { ARCADE, VERSUS, RANDOM, SURVIVAL, NETWORK, TRAINING, OPTIONS, LINES };
	static const char *name(int line)
	{
		static const char *const NAMES[LINES] = { "ARCADE", "V.S.", "RANDOM", "SURVIVAL", "NETWORK", "TRAINING", "OPTIONS" };
		return line >= 0 && line < LINES ? NAMES[line] : "?";
	}

	// The file, and the measured geometry (docs/fv2-etats.md)
	static constexpr int IMG_W = 709, IMG_H = 897;
	// the bands' edges in the file's own rows, cut in the middles of the white
	static constexpr float BANDS[LINES + 1] = { 0, 124.5f, 254, 381.5f, 511.5f, 640.5f, 769, 897 };
	// where each line's ink has its middle, in fractions of the file's width
	static constexpr float INK_MID[LINES] = { 0.4006f, 0.1728f, 0.4379f, 0.4732f, 0.5035f, 0.4746f, 0.4281f };
	// the block on the title screen, in fractions of the picture's height, as measured
	static constexpr float TOP = 0.26157f, HEIGHT = 0.35557f, CENTRE = 0.5f;   // (the measure gives 0.50026)
	// and the two the owner asked for on top of it (above)
	static constexpr float GROW = 7.0f / 6.0f, LIFT = 50.0f / 1080.0f;
	static constexpr float DRAWN_HEIGHT = HEIGHT * GROW;
	static constexpr float DRAWN_TOP = TOP + (HEIGHT - DRAWN_HEIGHT) * 0.5f - LIFT;
	// The chosen line's blink: 45 frames there and back, never fainter than this. Held as a TIME, not as a
	// count of pictures: the owner's menu counted its own, in a loop that ran at the screen's 60, but this
	// surface is drawn once a pass, and a pass is a refresh (144 a second on some screens) or, with no window
	// to wait for, about a thousand. Counted, the same 45 would be three times too fast on one screen and
	// invisible in a test run. 45 frames at 60 is 0.75 s, and that is what it is everywhere.
	static constexpr double BLINK_SECONDS = 45.0 / 60.0;
	static constexpr float BLINK_FLOOR = 0.5f;
	// how far the picture is taken down behind the list: black at half, measured on the Dreamcast version
	static constexpr uint8_t DIM_ALPHA = 128;

	// ---- the list's own sounds ----
	// The game's own, played by the game's own sound board. Fighting Vipers 2 has no DSB: everything goes
	// through one byte port as MIDI, A0 <bank> <index>, and bank 0x2E is the effects, where the menu's sounds
	// live. The three below are the owner's, each one named and numbered in the game's own SOUND TEST, and the
	// reading of the code was confirmed by the game itself: playing sound 388 from its SOUND TEST sends
	// A0 2E 23. (388 - 0x23 = 391 - 0x26 = 392 - 0x27 = 353, which is what makes the three of them one rule
	// rather than three guesses.)
	static constexpr uint8_t SND_OPEN[3]   = { 0xA0, 0x2E, 0x23 };   // sd_sign_16    388
	static constexpr uint8_t SND_MOVE[3]   = { 0xA0, 0x2E, 0x26 };   // sd_ketsu_00   391
	static constexpr uint8_t SND_CHOOSE[3] = { 0xA0, 0x2E, 0x23 };   // sd_sign_16    388 (the same as opening)
	static constexpr uint8_t SND_CANCEL[3] = { 0xA0, 0x2E, 0x27 };   // sd_sonic00_11 392
	// And the music stops while the list is up, which is the MIDI system reset, one byte: FF. The game sends it
	// itself at the head of every block of sound commands, so it is a driver built to be reset.
	//
	// It is sent LATE, 80 frames after the list opens, and that delay is not a matter of taste. FF restarts a
	// sound still playing before cutting it, measured four ways on the real board: sent 40 frames after the
	// opening bip, the bip is heard twice; at 60, the second copy is much shorter; a whole second after, there
	// is nothing left to restart. So the reset has to outlast the opening sound. Sending it first instead stops
	// the music at once but makes the bip wait for the driver to come back, which was measured and rejected.
	static constexpr uint8_t SND_STOP = 0xFF;
	static constexpr int STOP_DELAY_FRAMES = 80;

	// what handle_event returns when the player backed out, as against a line chosen (0..LINES-1) or -1
	static constexpr int CANCELLED = -2;

	bool shown = false;      // the list is up, and the game is frozen behind it
	int sel = ARCADE;
	bool ready = false, tried = false;
	SDL_Texture *tex = nullptr;
	std::string path;        // where the file was looked for (the log says so once)
	int stick_y = 0;
	uint64_t blink_t0 = 0;   // when the list went up, in SDL's nanoseconds: the blink is read from the clock
	std::vector<uint8_t> midi;   // the bytes waiting for the sound board (the host hands them over each frame)
	int stop_in = -1;            // frames before the music is stopped; 0 once it has been, -1 if it never was

	void play(const uint8_t (&s)[3]) { midi.insert(midi.end(), s, s + 3); }
	// Once a sound frame, while the list is up: the delayed stop comes due here, and whatever is waiting goes.
	std::vector<uint8_t> frame_midi()
	{
		if (stop_in > 0 && --stop_in == 0) midi.push_back(SND_STOP);
		return take_midi();
	}
	// The music was actually stopped, so there is something to put back if the player backs out.
	bool music_stopped() const { return stop_in == 0; }
	// What the host hands the sound board on the next of its frames, and empties here.
	std::vector<uint8_t> take_midi()
	{
		std::vector<uint8_t> out;
		out.swap(midi);
		return out;
	}

	// Reads the picture once. False (and a line in the log) if it isn't there: then the list never opens, and
	// Start goes on doing the other half of its job.
	bool load(SDL_Renderer *ren, const std::string &assets, FILE *log)
	{
		if (tried) return ready;
		tried = true;
		path = assets + "/menu-console/title_list.png";
		std::vector<uint32_t> px;
		int w = 0, h = 0;
		std::string why;
		if (!read_png(path.c_str(), px, w, h, why)) {
			if (log) fprintf(log, "the console's list of modes: %s %s -- Start cannot open it\n", path.c_str(), why.c_str());
			return false;
		}
		if (w != IMG_W || h != IMG_H) {
			if (log) fprintf(log, "the console's list of modes: %s is %dx%d, and the geometry was measured on %dx%d -- Start cannot open it\n",
							 path.c_str(), w, h, IMG_W, IMG_H);
			return false;
		}
		// read_png gives red in the low byte: that is ABGR8888 as SDL names it
		tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STATIC, w, h);
		if (!tex) {
			if (log) fprintf(log, "the console's list of modes: no texture for %s (%s)\n", path.c_str(), SDL_GetError());
			return false;
		}
		SDL_UpdateTexture(tex, nullptr, px.data(), w * 4);
		SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
		SDL_SetTextureScaleMode(tex, SDL_SCALEMODE_LINEAR);
		ready = true;
		if (log) fprintf(log, "the console's list of modes: %s, %dx%d\n", path.c_str(), w, h);
		return true;
	}

	// The renderer let go of its textures (a device reset): made again from the file.
	void textures_lost(SDL_Renderer *ren, const std::string &assets)
	{
		if (tex) SDL_DestroyTexture(tex);
		tex = nullptr;
		ready = false;
		tried = false;
		load(ren, assets, nullptr);
	}

	void show()
	{
		if (!ready) return;
		shown = true;
		sel = ARCADE;
		stick_y = 0;
		blink_t0 = SDL_GetTicksNS();   // (so the chosen line starts at full, and the blink starts there)
		play(SND_OPEN);
		stop_in = STOP_DELAY_FRAMES;
	}
	void hide() { shown = false; }

	// The cursor goes round. The owner's own list does not, and for a reason that is not ours: its last line is
	// QUIT GAME, and going round put quitting the emulator one press above ARCADE. Nothing here quits anything.
	void move(int d) { sel = (sel + LINES + d) % LINES; play(SND_MOVE); }
	int chose() { play(SND_CHOOSE); return sel; }
	int cancel() { play(SND_CANCEL); return CANCELLED; }

	// One event while the list is up. Returns the line chosen, CANCELLED, or -1 for anything else.
	//
	// Up and down move; Start, the pad's A, Enter or the space bar choose; the pad's B or Backspace back out,
	// which is what the Dreamcast version itself tells the player at the foot of that screen -- "Use the A
	// Button to decide. Use the B Button to back." On the keyboard the two are the player's own punch and kick
	// bindings, because that is where A and B sit on the pad in this port (punch is South, kick is East), so
	// whichever the player uses, the same two buttons decide and back.
	//
	// The press that OPENED the list cannot choose a line with the same stroke, and nothing here has to guard
	// against it: SDL updates the keyboard it is asked about while the events are polled, at the top of the
	// pass, and the list is opened later in that same pass, from the game's own frame. So that press's key-down
	// was handed to the pass before this list existed, and the next key-down is a second press.
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
			else if (sc == SDL_SCANCODE_RETURN || sc == SDL_SCANCODE_KP_ENTER || sc == SDL_SCANCODE_SPACE
					 || bound(C_START, sc) || bound(C_PUNCH, sc))
				return chose();
			else if (sc == SDL_SCANCODE_BACKSPACE || bound(C_KICK, sc))
				return cancel();
			return -1;
		}
		case SDL_EVENT_KEY_UP:
			return -1;
		case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
			switch (e.gbutton.button) {
			case SDL_GAMEPAD_BUTTON_DPAD_UP: move(-1); break;
			case SDL_GAMEPAD_BUTTON_DPAD_DOWN: move(+1); break;
			case SDL_GAMEPAD_BUTTON_START: case SDL_GAMEPAD_BUTTON_SOUTH: return chose();
			case SDL_GAMEPAD_BUTTON_EAST: return cancel();
			default: break;
			}
			return -1;
		case SDL_EVENT_GAMEPAD_BUTTON_UP:
			return -1;
		case SDL_EVENT_GAMEPAD_AXIS_MOTION: {
			if (e.gaxis.axis != SDL_GAMEPAD_AXIS_LEFTY) return -1;
			const int dir = e.gaxis.value < -16000 ? -1 : e.gaxis.value > 16000 ? 1 : 0;
			if (dir && dir != stick_y) move(dir);
			stick_y = dir;
			return -1;
		}
		default: return -1;
		}
	}

	// Over the picture the game made. `all` is the whole picture, which is darkened; `mid` is the game's own
	// 4:3 screen in the window's pixels, which is what the list's placement was measured against.
	void render(SDL_Renderer *ren, const SDL_FRect &all, const SDL_FRect &mid)
	{
		if (!shown || !ready || !tex) return;
		// The screen goes to half while the list is up, as the Dreamcast version does. Measured on two captures
		// of that version, the same screen with the menu and without: 255 becomes 128 on all three channels,
		// 0.502 in the median over 56000 pixels. So it is black at exactly half, not a figure chosen by eye.
		SDL_BlendMode was = SDL_BLENDMODE_NONE;
		SDL_GetRenderDrawBlendMode(ren, &was);
		SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
		SDL_SetRenderDrawColor(ren, 0, 0, 0, DIM_ALPHA);
		SDL_RenderFillRect(ren, &all);
		SDL_SetRenderDrawBlendMode(ren, was);
		const float block_h = DRAWN_HEIGHT * mid.h;
		const float block_w = block_h * float(IMG_W) / float(IMG_H);
		const float block_x = mid.x + mid.w * CENTRE - block_w * 0.5f;
		const float block_y = mid.y + DRAWN_TOP * mid.h;
		const double secs = double(SDL_GetTicksNS() - blink_t0) / 1e9;
		const float phase = float(std::fmod(secs, BLINK_SECONDS) / BLINK_SECONDS);
		const float chosen = BLINK_FLOOR + (1 - BLINK_FLOOR) * 0.5f * (1 + std::cos(6.2831853f * phase));
		for (int i = 0; i < LINES; i++) {
			const SDL_FRect src = { 0, BANDS[i], float(IMG_W), BANDS[i + 1] - BANDS[i] };
			// each band slid sideways so its own ink lands in the middle of the block
			const SDL_FRect dst = { block_x + (0.5f - INK_MID[i]) * block_w,
									block_y + BANDS[i] / float(IMG_H) * block_h,
									block_w,
									(BANDS[i + 1] - BANDS[i]) / float(IMG_H) * block_h };
			SDL_SetTextureAlphaMod(tex, uint8_t(255 * (i == sel ? chosen : 1.0f)));
			SDL_RenderTexture(ren, tex, &src, &dst);
		}
		SDL_SetTextureAlphaMod(tex, 255);
	}
};
