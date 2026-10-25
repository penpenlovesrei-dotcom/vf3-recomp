// THE DREAMCAST'S ENDING: its illustrations over the game's own staff roll.
//
// WHAT THE TWO ENDINGS ARE. The arcade's roll alternates pages of credits with replays of the matches the
// player has just won. The Dreamcast's shows ARTWORK in place of those replays -- the characters' own
// illustrations and the design sheets. Frederic asked the 2026-10-25 for the console's OPTIONS to choose
// between the two, which is what August's own fork offered: "ENDING : DREAMCAST / ARCADE".
//
// The pictures are his, chosen and put in order out of a rip of the Dreamcast disc (eight of forty-one),
// and they are named for the segment they belong to: 01_charlie, 02_emi, 03_grace_honey, 04_raxel_picky,
// 05_bahn, 06_tokio_sanman, 07_jane_mahler, 08_bm_kuhn.
//
// AND THE TIMING IS THE GAME'S OWN, not a count of frames. Two bytes say everything (model3.cpp):
//
//   ENDING_SEQ_PTR     0x00580E8D   where the roll is in its list of segments. It moves ONLY at a segment
//                                   boundary -- except through the twelve frames of ENDING_INT, where it
//                                   runs down its list a value a frame, which is why only ENDING_DSP is
//                                   listened to and no threshold is needed anywhere.
//   ENDING_REPLAY_ON   0x0070E603   which half of the segment is up: the page of credits, or the replay.
//
// THE PICTURE GOES UP ON THE REPLAY'S RISE AND COMES DOWN AT THE SEGMENT'S BOUNDARY -- not on the byte's
// fall, and that is August's correction, made because Frederic saw the fault: the byte drops TWO FRAMES
// before the segment ends and the game is still showing its replay in those two, so taking the picture away
// on the fall let a flash of replay through before the next page of credits.
#pragma once
#include <SDL3/SDL.h>
#include <string>
#include <vector>
#include "png.h"

struct fv2_fin
{
	static constexpr int PICTURES = 8;       // one a segment, in the order of the roll

	bool want = false;                       // the player's choice: the Dreamcast's ending or the arcade's
	bool tried = false;
	std::vector<std::string> paths;          // the files, in their own order; empty when there are none
	SDL_Texture *tex = nullptr;              // only the one being shown is held: they are 2048 square
	int tex_w = 0, tex_h = 0;
	int at = -1;                             // which picture is up, -1 for none
	// The page's own ground, read from its first pixel. What is beside the picture is filled with it rather
	// than with black: the page is 3:2 and a window is not, and black bars down the sides of a white page
	// read as a picture that failed to load.
	uint8_t edge[3] = { 0, 0, 0 };

	// The folder is read once and only its NAMES are kept. Eight pictures of 2048 square would be a hundred
	// and twenty megabytes of texture held for the one that is on screen, so they are read as they come.
	void find(const std::string &assets, FILE *log)
	{
		if (tried) return;
		tried = true;
		const std::string dir = assets + "/menu-console/fin/";
		static const char *const NAMES[PICTURES] = {
			"01_charlie.png", "02_emi.png", "03_grace_honey.png", "04_raxel_picky.png",
			"05_bahn.png", "06_tokio_sanman.png", "07_jane_mahler.png", "08_bm_kuhn.png",
		};
		for (const char *n : NAMES) paths.push_back(dir + n);
		if (log) fprintf(log, "the console's ending: %d pictures named in %s\n", PICTURES, dir.c_str());
	}

	bool have() const { return !paths.empty(); }

	void drop()
	{
		if (tex) { SDL_DestroyTexture(tex); tex = nullptr; }
		at = -1;
	}

	// The picture for a segment, read now and the one before it let go. A file that is not there leaves the
	// screen as it was rather than blanking it: a missing picture is the game's own replay showing, which is
	// the arcade's ending, which is not a failure.
	void show(SDL_Renderer *ren, int index, FILE *log)
	{
		if (index == at) return;
		drop();
		if (index < 0 || index >= int(paths.size())) return;
		std::vector<uint32_t> px;
		std::string why;
		if (!read_png(paths[size_t(index)].c_str(), px, tex_w, tex_h, why)) {
			if (log) fprintf(log, "the console's ending: %s %s\n", paths[size_t(index)].c_str(), why.c_str());
			return;
		}
		tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STATIC, tex_w, tex_h);
		if (!tex) {
			if (log) fprintf(log, "the console's ending: no texture for %s (%s)\n", paths[size_t(index)].c_str(), SDL_GetError());
			return;
		}
		const uint32_t c0 = px.empty() ? 0u : px[0];
		edge[0] = uint8_t(c0); edge[1] = uint8_t(c0 >> 8); edge[2] = uint8_t(c0 >> 16);
		SDL_UpdateTexture(tex, nullptr, px.data(), tex_w * 4);
		SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
		SDL_SetTextureScaleMode(tex, SDL_SCALEMODE_LINEAR);
		at = index;
		if (log) fprintf(log, "the console's ending: picture %d up\n", index + 1);
	}

	// Over the whole window and not over the game's 4:3 picture: it stands in for the replay, which fills
	// the screen, and in widescreen the frame's edge is not the screen's. Its own proportions are kept and
	// it is fitted inside, because these are square and nothing is gained by stretching them.
	void render(SDL_Renderer *ren, const SDL_FRect &all)
	{
		if (!tex || tex_w <= 0 || tex_h <= 0) return;
		SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
		SDL_SetRenderDrawColor(ren, edge[0], edge[1], edge[2], 255);
		SDL_RenderFillRect(ren, &all);
		const float k = std::min(all.w / float(tex_w), all.h / float(tex_h));
		const float w = float(tex_w) * k, h = float(tex_h) * k;
		const SDL_FRect r = { all.x + (all.w - w) * 0.5f, all.y + (all.h - h) * 0.5f, w, h };
		SDL_RenderTexture(ren, tex, nullptr, &r);
	}
};
