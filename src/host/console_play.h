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

	// ---- RANDOM and VERSUS: both hang off the select screen's own timing ----
	// Which is why neither could be wired until the game's state machine was read: what was missing was not
	// the lever but knowing WHEN the select screen comes up and WHEN the fighter is chosen. Both are states:
	// the mode is SEL (4 or 5), and the choice is its sub-state reaching SEL_SUB_DECIDE, 4 (fv2-etats.md,
	// fv2-flux-de-match.md).
	//
	// The levers themselves are the owner's, recorded by hand in August and not deduced (MenuConsole.md §8):
	//
	//   RANDOM   Start is HELD, from 30 frames after the select screen appears until 90 frames after the
	//            player's choice. The recorded manoeuvre is 109 frames of Start before the fighter is
	//            validated and 89 after -- and the part that counts is the one BEFORE. A first attempt over
	//            there waited for the choice to start holding, and so missed it entirely.
	//
	//            MEASURED HERE ON THE 2026-10-22, AND IT DOES NOTHING. Held by this machinery over that
	//            window, the game's whole memory at the frame of the choice is byte-identical to a run that
	//            held nothing. The control says it is not this code: an input script holding Start over the
	//            same window gives the same zero. So the hold reaches the game and the game ignores it, and
	//            something else is missing -- as Survival needed its operator setting beside its flag. The
	//            menu's RANDOM line is left doing nothing until that half is found.
	//   VERSUS   one press of Start player 2, 30 frames after that same appearance. Nothing else.
	//
	// The thirty frames are there to clear the transition and the Start that opened the screen, and the hold
	// is renewed every frame rather than asked for once, because how long the player takes is not known in
	// advance. It is held, not forced: nothing is written to the game's memory on either road.
	// RANDOM, laid out like Survival: armed when it is chosen, written once a game is actually running, put
	// back when the game ends. Two things were measured about it, and both are the opposite of Survival's:
	//
	//   it does NOT have to be held      written once at frame 2000, the select screen still says RANDOM MODE
	//                                    a hundred frames later;
	//   and it has no attract-loop trap  written from frame 1800, in the middle of the attract loop, the game
	//                                    still starts (GAME_INT at 2296), where Survival's flag there stops it
	//                                    starting at all.
	//
	// The arming is kept all the same: it costs nothing, it is the shape Survival already uses, and it is what
	// gives a moment to put the flag back when the game ends. The flag itself is Sega's own RANDOM_CHARA
	// (model3.cpp says how it was found).
	enum { R_OFF, R_ARMED, R_SET } random_mode = R_OFF;
	int random_armed = 0;
	int random_was = 0;

	// ---- TRAINING, first half: the arena ----
	// There is no training MODE on this board -- Sega's twenty-four hold none, and the arcade notes say so --
	// so this is not a switch to find but a thing this port builds. The first half is which arena the fight
	// happens in, and Frédéric's choice for now is MARBLE ARENA, scene 3.
	//
	// It is a patch on the function that draws the stage (Model3::fv2_hold_stage), so it is HELD, not written
	// once: the game unpacks and reloads its own code, and the patch has to be put back whenever Sega's words
	// come back. That is the roster patch's discipline. It is released when the game ends.
	//
	// The first lever tried was the stage picker at 0x0007111C, the one the owner's CHANGE STAGE uses, and it
	// does NOT command this: patched and held -- `li r3,3 ; blr` read back out of the running game -- the game
	// picked stage 9 all the same. The picker is reached only by a rematch THROUGH the select screen. What
	// commands a game's first arena is one byte of the game's own route table (Model3::fv2_hold_training_stage),
	// found by watching who writes the scene index and reading the dozen instructions above that write.
	//
	// What is NOT here either, and should not be mistaken for it: everything else a training mode is -- the
	// timer that does not run out, the health that comes back, the dummy's behaviour. The owner has a draft
	// of that in the patched ROM of the summer, and the Dreamcast version has the real thing; neither has
	// been read yet.
	static constexpr int MARBLE_ARENA = 3;
	enum { T_OFF, T_ARMED, T_HELD } training = T_OFF;
	int training_armed = 0;

	enum { SEL_NOTHING, SEL_RANDOM, SEL_VERSUS } at_select = SEL_NOTHING;
	int sel_frames = -1;          // frames since the select screen appeared; -1 until it does
	int since_choice = -1;        // frames since the fighter was chosen; -1 until it is

	void choose_arcade() { inject_start = 8; swallow_start = true; }
	void choose_survival(m3::Model3 &b) { survival = ARMED; armed_frames = 0; survival_was = b.fv2_survival(); choose_arcade(); }
	void choose_random() { random_mode = R_ARMED; random_armed = 0; random_was = 0; choose_arcade(); }
	void choose_training() { training = T_ARMED; training_armed = 0; choose_arcade(); }
	void choose_versus() { at_select = SEL_VERSUS; sel_frames = -1; since_choice = -1; choose_arcade(); }

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
			// Two roads to the title screen, and they are not interchangeable. From the attract loop, the
			// game's own: the sequencer is pushed to the end of its list and walks off it, which installs the
			// title screen the way the game does on its own -- and, measured with the picture probe, leaves
			// no 3D behind. The mode request shows the title screen just as fast but does not stop what was
			// drawing: 397 models still on a screen the game draws with none, and the demonstrations that
			// follow come out with fighters missing or blackened. Elsewhere -- the select screen, a fight --
			// the sequencer is not running, so the request is all there is, and it does not end the game
			// either (docs/fv2-etats.md).
			if (what == start_for::title) {
				// From the attract loop, the game's own way out of whatever is drawing: ask the loop to start
				// over. That is what the replay itself does when it ends (0x0000B7C0 writes SUBMODE_REQUEST =
				// 0), and the restart is where the game clears the tasks that draw, two sub-states later. Then
				// the title screen is asked for as soon as that housekeeping is done -- the SEGA logo -- so
				// the player does not have to sit through it.
				if (b.fv2_in_attract()) { b.fv2_restart_attract(); skip_logo = SKIP_GIVE_UP; }
				else b.fv2_request_title();
			}
			swallow_start = true;
		}
		start_was = pressed;
		return what;
	}

	// The game's own road to the title screen goes by way of the SEGA logo, which takes its time: measured, the
	// title screen landed 400 frames after the press instead of one. The two are not alternatives, though --
	// they can be used one after the other. The sequencer's road is what TIDIES THE SCENE (the demonstration
	// stops drawing), and once that is done the mode request carries nothing over, because there is nothing
	// left to carry: measured at 0 models from the first frame of the logo. So: the game's road to clean up,
	// then the request to skip the waiting, taken as soon as the demonstration is over.
	//
	// Asked while the logo is already up, the scene is clean on the spot and the title screen comes one frame
	// later, as it always did. The count is a way out, not a timer: if that moment never comes the request is
	// dropped rather than held, because a request held is what broke the select screen twice.
	static constexpr int SKIP_GIVE_UP = 600;
	int skip_logo = 0;        // frames left to wait for the scene to be tidy before asking for the title
	// THE ENDING IS ASKED FOR FROM THE TITLE SCREEN, AND NOT STRAIGHT AWAY. Measured with vf3run --shots,
	// poking MODE_REQUEST one frame at a time, with the title screen installed at frame 1193:
	//
	//   asked at 1201, 1205, 1210  ->  a demonstration's stage, white, with "Old Armstone Town" over it and
	//                                  "v.s. GRACE" under: the screen the title was replacing is still drawing
	//   asked at 1215, 1220, 1250  ->  the staff roll, clean, over the viper
	//
	// So the title screen takes about twenty frames to finish installing itself, and the ending asked before
	// that lands on top of what has not finished leaving. Forty frames is the wait here: twice what was
	// measured, and the title screen lasts about 190, so there is room.
	//
	// The attract loop's restart was tried first, since that is the road that TIDIES for the title screen --
	// and it does not serve here: asked at 1206 after a restart, the same broken stage came out. The ending
	// does not want a tidy scene, it wants a finished title screen.
	static constexpr int ENDING_WAIT = 40;
	int ending_in = 0;        // frames left on the title screen before the ending is asked for; 0: nothing pending

	// The console's DISPLAY page asking to watch the game's own ending.
	void ask_ending(m3::Model3 &) { ending_in = ENDING_WAIT; }

	// Once a frame, before the game runs. Returns what to hand the game on top of the player's own controls.
	void step(m3::Model3 &b, m3::Controls &out)
	{
		out = m3::Controls();
		if (!on) return;
		if (skip_logo > 0) {
			const int mode = b.fv2_mode(), sub = b.fv2_submode();
			// The SEGA logo, and not a sub-state before it: the tasks that draw are cleared BETWEEN sub-state
			// 1 and sub-state 2 (model3.cpp), so this is the first moment the scene is the game's own idea of
			// empty. Asking any earlier would carry the old one over, which is the whole defect.
			const bool tidy = (mode == 2 || mode == 3) && (sub == 2 || sub == 3);
			// (tests: VF3_NO_SKIP=1 restarts the attract loop and then lets the game walk to its own title
			// screen, logo and all. For telling a fault that belongs to the restart from one that belongs to
			// asking for the title early -- the dark fighters of the 2026-10-23 are one or the other.)
			static const bool no_skip = getenv("VF3_NO_SKIP") != nullptr;
			if (tidy && no_skip) skip_logo = 0;
			else if (tidy) { b.fv2_request_title(); skip_logo = 0; }
			else skip_logo--;
		}
		// The ending, once the title screen has had its twenty-odd frames (above). Counted only while the
		// title screen is really up: if the player's own Start has carried the game somewhere else in the
		// meantime, the request is dropped rather than held.
		if (ending_in > 0) {
			if (!b.fv2_on_title()) ending_in = 0;
			else if (--ending_in == 0) b.fv2_request_ending();
		}
		if (inject_start > 0) { out.start[0] = true; inject_start--; }
		if (inject_start_p2 > 0) { out.start[1] = true; inject_start_p2--; }
		if (at_select != SEL_NOTHING) {
			const int mode = b.fv2_mode();
			const bool on_select = mode == 4 || mode == 5;   // SEL_INT, SEL_DSP
			if (!on_select) {
				// Before the screen: wait. After it: the work is done either way, and a hold that outlived
				// its screen would be a hold on whatever came next.
				if (sel_frames >= 0) at_select = SEL_NOTHING;
			} else {
				sel_frames++;
				if (at_select == SEL_VERSUS) {
					if (sel_frames == 30) { inject_start_p2 = 8; at_select = SEL_NOTHING; }
				} else {
					constexpr int SEL_SUB_DECIDE = 4;
					if (since_choice >= 0) since_choice++;
					else if (b.fv2_submode() >= SEL_SUB_DECIDE) since_choice = 0;
					if (since_choice > 90) at_select = SEL_NOTHING;        // (90 frames past the choice: let go)
					else if (sel_frames >= 30) out.start[0] = true;        // held, every frame
				}
			}
		}
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
		switch (random_mode) {
		case R_ARMED:
			// nothing written yet: waiting for the game to actually be running, as Survival does
			if (!b.fv2_in_attract()) {
				random_was = b.fv2_random_chara();
				b.fv2_set_random_chara(true);
				random_mode = R_SET;
			}
			else if (++random_armed > 300) random_mode = R_OFF;   // (the game never started: give up)
			break;
		case R_SET:
			// written once and not held; put back when the game ends, so it does not carry into the next one
			if (b.fv2_in_attract()) { b.fv2_set_random_chara(random_was > 0); random_mode = R_OFF; }
			break;
		case R_OFF: break;
		}
		switch (training) {
		case T_ARMED:
			// nothing touched yet: the same wait as Survival, for the same reason -- the game is still in its
			// attract loop when the line is chosen. The arena is read when a match starts, well after this.
			if (!b.fv2_in_attract()) { b.fv2_hold_training_stage(MARBLE_ARENA); training = T_HELD; }
			else if (++training_armed > 300) training = T_OFF;   // (the game never started: give up)
			break;
		case T_HELD:
			// Given back when the game ends: left in place it would send the NEXT game to the same arena too.
			if (b.fv2_in_attract()) { b.fv2_release_training_stage(); training = T_OFF; }
			else b.fv2_hold_training_stage(MARBLE_ARENA);
			break;
		case T_OFF: break;
		}
	}
};
