// THE CURTAIN: the Dreamcast's own loading picture, drawn while the game is taken from one screen to another.
//
// It exists because of a state, not the other way round. CHANGE YOUR VIPER finishes the match off screen and
// walks the game back to its select screen -- and the end of a game is CONTINUE and then GAME OVER, each a mode
// of its own, so the player watched them go by and heard them, which is the very fault Frederic reported about
// MAIN MENU the 2026-10-25. MAIN MENU had an answer because the list has a ground of its own to put in front;
// this road had nothing to put there, and so had nothing.
//
// assets/menu-console/loading.png is what the Dreamcast version puts there: the emblem on its grey gradient.
// The file is authored like the others, 16:9 with the picture inset and flat purple (119, 85, 170) outside, but
// its own inset is NOT the one measured on option_bg.png: scanned row by row, its content runs from column 492
// to column 3136, 2645 wide against 2048 high -- 1.2915, where 4:3 is 1.3333. Drawn into the game's 4:3 frame
// it is therefore stretched by three parts in a hundred, which on an emblem over a gradient is not visible;
// keeping its own proportions would mean bars of some colour around it, and a gradient has no colour to pick.
#pragma once
#include <SDL3/SDL.h>
#include <string>
#include <vector>
#include "png.h"

struct fv2_curtain
{
	// the file, and the bounds of its picture inside it, measured on the file itself (above)
	static constexpr int IMG_W = 3630, IMG_H = 2048, INK_L = 492, INK_R = 3136;

	bool ready = false, tried = false;
	SDL_Texture *tex = nullptr;

	bool load(SDL_Renderer *ren, const std::string &assets, FILE *log)
	{
		if (tried) return ready;
		tried = true;
		const std::string path = assets + "/menu-console/loading.png";
		std::vector<uint32_t> px;
		int w = 0, h = 0;
		std::string why;
		if (!read_png(path.c_str(), px, w, h, why) || w != IMG_W || h != IMG_H) {
			if (log) fprintf(log, "the console's curtain: %s %s -- the game shows through instead\n",
							 path.c_str(), why.empty() ? "is not the measured size" : why.c_str());
			return false;
		}
		tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STATIC, w, h);
		if (!tex) {
			if (log) fprintf(log, "the console's curtain: no texture for %s (%s)\n", path.c_str(), SDL_GetError());
			return false;
		}
		SDL_UpdateTexture(tex, nullptr, px.data(), w * 4);
		SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
		SDL_SetTextureScaleMode(tex, SDL_SCALEMODE_LINEAR);
		ready = true;
		if (log) fprintf(log, "the console's curtain: %s\n", path.c_str());
		return true;
	}

	void textures_lost(SDL_Renderer *ren, const std::string &assets)
	{
		if (tex) SDL_DestroyTexture(tex);
		tex = nullptr;
		ready = false;
		tried = false;
		load(ren, assets, nullptr);
	}

	// Black over the whole window, the picture in the game's own 4:3 place: the same shape as every other
	// surface of this port, so a wide window shows the same black sides here as it does under the list.
	void render(SDL_Renderer *ren, const SDL_FRect &all, const SDL_FRect &mid)
	{
		if (!ready || !tex) return;
		SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
		SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
		SDL_RenderFillRect(ren, &all);
		const SDL_FRect src = { float(INK_L), 0, float(INK_R - INK_L + 1), float(IMG_H) };
		SDL_RenderTexture(ren, tex, &src, &mid);
	}
};
