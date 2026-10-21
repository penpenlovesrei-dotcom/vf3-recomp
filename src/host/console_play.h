#pragma once
#include "../machine/model3.h"

// Fighting Vipers 2's console way: a console's own menu over an arcade game.
//
// The whole of it rests on one decision, and it is the one that makes the rest work: the menu NEVER reads the
// credit counter and never writes to it. Choosing a mode hands the game a real press of Start, and the game
// takes a credit, or none, by its own settings. So the menu costs nothing to open, cancelling costs nothing,
// and it works the same whether the board is on credits or on free play -- without this port ever having had to
// find Fighting Vipers 2's free-play byte, which it has not.
//
// Where the game is, is one comparison: Model3::fv2_in_attract(), the entry1 task's state being zero. It is not
// a screen detector. Which attract screen is up -- warning, logo, title, demo, rankings -- does not matter,
// because the menu covers the screen itself. And when in doubt the safe side is "attract": putting a menu over
// a demonstration costs nothing, freezing a game nobody is playing is a fault anyone can see.
//
// Starting a mode is injection, through the same mixing point the input scripts use (frame_inputs). An injected
// press lasts 8 frames: a human press of Start was measured at 7 to 12 frames, so 8 is indistinguishable from
// one, and the game samples its inputs at least once a frame.
//
// A console has no coin slot, and this is what the console way got wrong at first. Playing it, the menu opened,
// a mode was chosen, and nothing happened: measured straight after, ARCADE chosen with no coin leaves the entry1
// task at 0 -- still the attract loop -- while the same run with a coin first reaches the select screen. The
// Start was being handed over correctly and the game was ignoring it, having no credit.
//
// The answer was not to feed the game coins, which was tried and is not what a console does. It was to find
// where Fighting Vipers 2 keeps its COIN/CREDIT SETTING and put the board on FREE PLAY, as the PC way already
// did for Virtua Fighter 3. That is one byte of the backup RAM (Model3::coin_offset), found by driving the
// game's own test menu from an input script. The console way is the PC way plus a menu, so it comes up on free
// play, and a mode chosen here costs nothing -- which is the whole point of a console.
//
// The press that worked the menu must not reach the game. Controls are not handed back when the menu closes but
// when the player lets go -- otherwise the button that chose ARCADE would still be down on the next frame and
// would pick a fighter the instant the select screen appeared.
struct console_play
{
	bool on = false;              // the console way is set, and this is the game it was measured on
	bool start_was = false;       // the player's Start last frame: this works on the edge, not the level
	bool swallow_start = true;    // Start is held back until it has been seen released
	int inject_start = 0;         // frames of Start still to hand the game
	int inject_start_p2 = 0;      // the same for player 2 (VERSUS)

	// Survival is two bytes, and the order matters. Written during the attract loop the flag stops the game
	// starting at all -- the injected Start is ignored and no credit is ever taken -- so it is armed when it is
	// chosen, written only once a game is actually running, and then held every frame, because nothing says the
	// game does not clear it when a fight begins (the roster patch had to be held for the same reason). It is
	// put back when the game ends: left on, it blocks the next game, and ARCADE stops working.
	enum { OFF, ARMED, HELD } survival = OFF;
	int armed_frames = 0;         // given up after 300: the game never started
	int survival_was = 0;         // what the flag was before the menu touched it

	void choose_arcade() { inject_start = 8; swallow_start = true; }
	void choose_survival(m3::Model3 &b) { survival = ARMED; armed_frames = 0; survival_was = b.fv2_survival(); choose_arcade(); }
	void choose_versus() { inject_start = 8; inject_start_p2 = 0; swallow_start = true; }

	// ---- what Start does, and it is two jobs told apart by one question ----
	// Is the game's own title screen on screen? Model3::fv2_on_title answers it: the mode is ADV and the
	// sub-state is FV2_TITLE_INT or FV2_TITLE_DSP, measured by freezing the game one sub-state at a time and
	// looking at the screen (docs/fv2-etats.md). That is NOT the question fv2_in_attract above answers -- "is a
	// game running" is true of the warning, the SEGA logo, the title, the presentations, the demos and the
	// rankings alike, and all but one of those are the wrong screen for a list of modes.
	//
	//   on the title screen -> the list of modes opens over it (the caller draws it; nothing here does);
	//   anywhere else       -> the game is asked for its own title screen.
	//
	// The press itself never reaches the game, either way, and the next one is not read until this one has been
	// let go: the button that opened the list would otherwise still be down on the frame the list hands the game
	// a Start of its own, and would pick a fighter the instant the select screen appeared.
	//
	// What "asked" means, and why it is not forced: the dispatcher at 0x0000ABC4 copies MODE_REQUEST into the
	// mode before it jumps, so the request is written ONCE and never held -- holding one is what broke the
	// select screen and its countdown, twice. It takes two frames.
	//
	// Where it works, and how far: measured the 2026-10-21 with vf3run --modes --console-start, one press at a
	// time. The title screen comes back ONE frame later from the SEGA logo, from the demonstration, from the
	// select screen and from inside a fight alike -- the last three had never been tried, and the attract
	// sequencer's own two levers, which do nothing from a fight, had been taken to speak for this one as well.
	//
	// But a request moves the SCREEN, not the game: read back afterwards the entry1 task is still in its
	// playing state (0x2A from a fight, 0x2D from the select screen), so the title screen comes back over a
	// game that is still running and fv2_in_attract stays false. Leaving a fight properly is the game's own
	// road -- finish the match off screen -- which is written down in docs/fv2-etats.md and is not wired here.
	enum class start_for { none, list, title };
	start_for take_start(m3::Model3 &b, bool pressed, bool list_open)
	{
		// While the list is up it is the list's button, not the game's: withheld whatever it does there.
		if (list_open) { start_was = pressed; swallow_start = true; return start_for::none; }
		start_for what = start_for::none;
		if (swallow_start) { if (!pressed) swallow_start = false; }   // (held back until it has been let go)
		else if (pressed && !start_was) {
			what = b.fv2_on_title() ? start_for::list : start_for::title;
			if (what == start_for::title) b.fv2_request_title();
			swallow_start = true;
		}
		start_was = pressed;
		return what;
	}

	// Once a frame, before the game runs. Returns what to hand the game on top of the player's own controls.
	void step(m3::Model3 &b, m3::Controls &out)
	{
		out = m3::Controls();
		if (!on) return;
		if (inject_start > 0) { out.start[0] = true; inject_start--; }
		if (inject_start_p2 > 0) { out.start[1] = true; inject_start_p2--; }
		switch (survival) {
		case ARMED:
			// nothing written yet: waiting for the game to actually be running
			if (!b.fv2_in_attract()) { survival = HELD; b.fv2_set_survival(true); }
			else if (++armed_frames > 300) survival = OFF;   // (the game never started: give up rather than guess)
			break;
		case HELD:
			if (b.fv2_in_attract()) { b.fv2_set_survival(survival_was != 0); survival = OFF; }
			else b.fv2_set_survival(true);
			break;
		case OFF: break;
		}
	}
};
