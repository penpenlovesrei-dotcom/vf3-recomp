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
