// Fighting Vipers 2's VERSUS, on the menu's main page (Escape > Versus), above Survival and Random.
//
// THE MODE IS THE BOARD'S, not this port's. Player 2 pressing Start on the select screen steps into player
// 1's game -- the game calls it INTRUDE -- and from there on it is a two-player match, with the round's own
// length, its own arena rule and its own end. So nothing here invents a mode: what is here is the little
// machine that walks the game into it from a menu row, lets the two players choose the arena on the way, and
// stands where GAME OVER stands at the end of it. Every lever it pulls is one of the game's own bytes and was
// measured on this port's console build (model3.cpp carries each measurement with its address).
//
// FOUR THINGS, in the order a match meets them:
//
//   THE ENTRY      the row hands the game a press of Start, as Survival's and Random's rows do, and then ONE
//                  PRESS OF START PLAYER 2, thirty frames after the select screen has been SEEN up. Nothing
//                  else. The thirty frames are there to clear the transition and the press that opened the
//                  screen; the screen has to have been SEEN because THE SUB-STATE BYTE IS STALE WHEN A MODE
//                  BEGINS -- entering SEL it still holds what ADV left there, so a test on "the fighters are
//                  chosen" fires three hundred frames before anybody has chosen one.
//   THE ARENA      the page of fourteen pictures (fv2_stages.h), opened at the one tick where the game has
//                  ASKED for DECIDE_INT and has not run it, with the game parked in Sega's own empty
//                  STAGE_SEL. The arena is held by the stage draw's own table and given back when the game
//                  ends -- or the next game would fight there too.
//   THE END        a decided two-player match used to hand the player GAME OVER, a screen that belongs to a
//                  ladder nobody was climbing. Instead the match's own panel goes up, offering another match.
//                  WHEN is the game's own answer: VIC_DSP, sub-state 18, which a round that settles nothing
//                  never reaches -- there VIC_INT asks for 19 and VIC_DSP is skipped. It is not a threshold
//                  and not a count kept here: it is a state Sega named and reaches nowhere else.
//   THE WAY OUT    the two lines that leave the match: the game walked home by its own roads, its end-of-game
//                  screens shortened by their own timer, behind a veil, because what they are is the end of a
//                  game nobody was playing.
//
// AND TRAINING IS A FIFTH THING ON TOP OF THOSE FOUR, on its own row under Random. It is the console
// build's mode, brought over whole (see TRAINING below).
//
// AND IT IS NOT A SETTING. A row is not a yes and a no: it starts a game in that mode, as choosing a mode on
// a console does. The menu closes, the game is handed a real press of Start, and the mode is over when that
// game is -- which is the shape Survival and Random already have (fv2_modes.h).
#pragma once
#include "../machine/model3.h"
#include <cstdio>
#include <cstdlib>

class Fv2Versus
{
public:
	// The panel's four lines. They are the console build's own four, less RESUME: there is no fight to go back
	// to at the end of a match, and a line that promises one is a line that lies.
	//
	// AND THE LAST ONE IS CALLED EXIT, NOT MAIN MENU, which is Frederic's of the 2026-11-14. The console
	// build's MAIN MENU lands on the console's own list of modes -- ARCADE, V.S., RANDOM and the rest -- and
	// that surface is deliberately not part of this package: the console menu is an extra of the package and
	// not a part of it. So the line has nowhere of that name to go, while what it DOES reach is the game's own
	// title screen, by the attract sequencer's own road, which works here because by then the attract loop is
	// running again and there IS a list to walk off. A line is named for where it arrives.
	enum { L_REMATCH, L_STAGE, L_VIPERS, L_EXIT, L_COUNT };
	static const char *line_name(int l)
	{
		static const char *const N[L_COUNT] = { "REMATCH", "CHANGE STAGE", "CHANGE YOUR VIPER", "EXIT" };
		return l >= 0 && l < L_COUNT ? N[l] : "?";
	}

	// What the host has to put on screen. It reads this, opens the page or the panel, and clears it; nothing
	// here opens anything, because the host owns the surfaces.
	enum { W_NONE, W_STAGE, W_PANEL, W_TRAIN };
	int wants = W_NONE;
	bool veil = false;   // the way out is running: the host hides the game and silences it (see THE WAY OUT)

	// THE PAGE OPENED BY THE PANEL STANDS OVER A FROZEN GAME, not a parked one, and the difference is
	// measured: the park writes sub-state 2, and 2 means Sega's empty STAGE_SEL only in SEL -- in a fight it
	// is ROUND_MASK_DSP. At the end of a match the game is at VIC_DSP with NEXT_PROG_SEL asked for, and one
	// frame of it running is one frame too many: that handler is what sends a decided versus to GAME OVER, or
	// hands the winner a ladder. So the host stops the game for as long as that page is up, which is what the
	// console build does with the same page opened from the same place.
	bool hold_game() const { return stage_from_panel; }
	bool live() const { return at != OFF; }
	bool in_match() const { return at == IN; }

	// ---- the row chosen ----
	void start(m3::Model3 &b)
	{
		at = ASKED;
		wait = 0;
		sel_at = SEL_AWAY;
		sel_frames = 0;
		sel_two_players = true;
		sel_wants_page = true;
		scene = 0;
		stage_held = false;
		over_done = false;
		task_live[0] = task_live[1] = 0;
		leaving = L_OFF;
		veil = false;
		// AND A PLAIN VERSUS IS NOT A TRAINING, which is the one thing this reset owes the mode above it:
		// start_training calls this and turns it on AFTERWARDS, so choosing VERSUS after a training session
		// leaves the mode off here rather than carrying a stopped clock into a fight nobody asked to train in.
		training_on = false;
		leave_training = false;
		train_arm();
		const bool idle = b.fv2_in_attract();
		if (idle) inject_start = 8;
		fprintf(stderr, "Fighting Vipers 2: VERSUS chosen%s\n",
		        idle ? ", and the game handed a press of Start" : " (a game is already running: player 2 steps in)");
	}

	// ---- TRAINING, AND IT IS A MODE THIS BOARD DOES NOT HAVE ----
	//
	// Sega's twenty-four settings hold no training and the arcade notes say so, so this is not a switch to find
	// but a thing the port builds -- out of the game's own bytes and nothing else. It is A TWO-PLAYER VERSUS
	// WITH FOUR THINGS ON TOP, and the entry is the Versus row's own road, word for word, which was already
	// proved. All four are the console build's, measured there in October, and they are brought over whole:
	//
	//   THE ARENA         MARBLE ARENA, scene 3, Frederic's choice, held by the stage draw's own table like any
	//                     other versus's arena. The carousel is NOT opened: choosing the arena is the Versus
	//                     row's point, and a training that asked for it before every session would be a page
	//                     to dismiss. (The console build also carries a lever on the game's ROUTE TABLE for
	//                     this, and it is not needed here: held at scene 3 a VERSUS already fights there --
	//                     measured, where a solo route does not, which is what the route table was for.)
	//   THE CLOCK         stopped by THE GAME'S OWN SWITCH: bit 0 of CONFIG_CB_DISP, which FIGHT_DSP tests
	//                     before it decrements anything (Model3::fv2_freeze_timer, which carries the four
	//                     instructions it was read out of). Held and not written once, because that block is a
	//                     working copy the game loads from its backup. Holding the timer's VALUE instead would
	//                     leave open whether the game reads that word or writes it; a condition has no such
	//                     question.
	//   THE TWO LIVES     held, with a DELAY and a FLOOR, and both numbers exist for a reason that was paid
	//                     for. Given back every frame, the life never went DOWN -- the hit was erased on the
	//                     frame it landed, so the damage could not be read, and a training whose damage cannot
	//                     be read is useless. So the delay is how long without new damage before the life
	//                     fills, and the floor is how low it may fall: without it, waiting out a long series
	//                     would mean accepting the K.O., which ends the round, which is exactly what a
	//                     training must not do.
	//   THE ANNOUNCEMENT  cut. GAME's sub-states 4 to 10 are ROUND, SET and READY: they load nothing, and each
	//                     is asked to move on the frame it begins, so every INT still runs its own frame and
	//                     only the waiting goes -- a training fight begins ten frames after the load instead
	//                     of three hundred. THE FIRST FOUR ARE NOT TOUCHED: those are the arena arriving, and
	//                     cutting them gives a black screen with nothing on it but the HUD, measured three
	//                     ways. Cutting the seven took their PICTURES and not their SOUND -- the handlers
	//                     still run their frame and what they say to the sound board is said -- so the two
	//                     lines the announcer speaks are dropped across those frames as well (train_hush, and
	//                     main.cpp is where they are dropped).
	//
	// AND ONE PLAYER CHOOSES BOTH FIGHTERS, which is route_training: the mode opens a two-player versus, so
	// the select screen comes up with two cursors and there is one pair of hands.
	//
	// THE DUMMY'S BEHAVIOUR IS NOT HERE. Nothing stands in for it and nothing pretends to: player 2 is a
	// second pad, and with nobody on it he stands still, which is a dummy of a kind but not a chosen one.
	static constexpr int MARBLE_ARENA = 3;
	bool training_on = false;        // the mode itself, as opposed to the arena it arms
	bool train_fighting = false;     // a round is being fought: what the column of inputs is drawn over
	int train_took[2] = { 0, 0 };    // points taken off each player this frame; the host reads them and clears them
	int train_hush = 0;              // frames in which the announcer's two lines are dropped (main.cpp)

	void start_training(m3::Model3 &b)
	{
		start(b);                  // the entry is the Versus row's, and it says so in the log
		training_on = true;
		train_arm();
		scene = MARBLE_ARENA;      // held by step(), every frame, like any other versus's arena
		stage_held = true;
		sel_wants_page = false;    // (and no carousel: see THE ARENA above)
		fprintf(stderr, "Fighting Vipers 2: TRAINING on top of it -- scene %d, the clock stopped, the lives"
		                " held, the announcement cut\n", MARBLE_ARENA);
	}

	// ---- TRAINING: ONE PLAYER CHOOSES BOTH FIGHTERS ----
	//
	// On the select screen player 1's controls go to his own cursor until he has pressed punch, and to PLAYER
	// 2'S from then on, so the same hands pick the opponent. Nothing of the game is touched: the controls are
	// composed in the host's frame loop and this is handed them there, AFTER the column has read them --
	// routed first, it empties player 1's controls before the column sees them and the column shows nothing of
	// what is being pressed (main.cpp says so where it calls this).
	//
	// Two things had to be learnt by getting them wrong, and both are latches here rather than rules in the
	// host's loop, which is where they were in the console build at first:
	//
	//   the button has to be SEEN RELEASED first. The press that chose the row is still down when the select
	//   screen arrives several frames later, and taken for a choice it made player 1's pad pick player 2's
	//   fighter first -- "the pad in P1 picks P2 first", the 2026-10-25;
	//
	//   and the press that DID choose has to be let go before it is routed on, or one press chooses both: the
	//   directions travel at once, the punch waits for its release.
	void route_training(m3::Model3 &b, m3::Controls &c)
	{
		const int m = b.fv2_mode();
		if (!training_on || (m != 4 && m != 5)) return;   // the select screen's job, and no other screen's
		const bool punch = c.punch[0];
		auto move = [](bool *a) { a[1] = a[1] || a[0]; a[0] = false; };
		if (!train_pick_arm) {
			if (!punch) train_pick_arm = true;
			else c.punch[0] = false;
		} else if (!train_p1_chose) {
			// (this press is his own choice: it stays his, and the routing begins after it)
			if (punch) { train_p1_chose = true; train_punch_held = true; }
		} else {
			move(c.up); move(c.down); move(c.left); move(c.right);
			move(c.kick); move(c.guard);
			if (train_punch_held && !punch) train_punch_held = false;
			if (train_punch_held) c.punch[0] = false;
			else move(c.punch);
		}
	}

	// ---- one game frame, after the frame has run; `allowed` is false in an online match ----
	void step(m3::Model3 &b, bool allowed)
	{
		if (at == OFF) return;
		if (!allowed) { let_go(b, "an online match"); return; }

		const bool running = !b.fv2_in_attract();
		if (at == ASKED) {
			if (running) {
				at = IN;
				fprintf(stderr, "Fighting Vipers 2: VERSUS is in, the game having started\n");
			} else if (++wait > GIVE_UP) {
				fprintf(stderr, "Fighting Vipers 2: VERSUS was chosen and no game started in %d frames -- let go of\n",
				        GIVE_UP);
				at = OFF;
				return;
			}
		}

		step_leaving(b);
		step_training(b);   // the clock, the two lives, and what the column is to read
		note_tasks(b);
		step_select(b);
		step_over(b);
		// THE ARENA HELD, not written once: the game unpacks and reloads its own code, and the draw marks its
		// own choice used as it takes it. The roster patch's discipline.
		if (scene >= 1) b.fv2_hold_stage(scene);

		// And over when the game is over. A rematch never leaves the fight, so this is the attract loop and
		// nothing else: the arena given back, the panel's own state cleared, the row ready to be chosen again.
		if (at == IN && leaving == L_OFF && !running && wants == W_NONE) {
			fprintf(stderr, "Fighting Vipers 2: VERSUS over, the game being over\n");
			put_back(b);
			at = OFF;
		}
	}

	// ---- START DURING A TRAINING IS THE PAUSE, AND NOT THE GAME'S ----
	//
	// Frederic's of the 2026-11-15: a training needs a way out that is not the end of a match, because a
	// training has no end of a match -- the lives are held, so the round never settles and the panel that
	// stands where GAME OVER stands is never reached. Start is the button a console puts that page behind, and
	// it is the button the console build of this port puts its own pause menu behind.
	//
	// THE PRESS IS SWALLOWED FOR THE WHOLE ROUND and not only on the frame the page opens. A two-player match
	// has both players in already, so there is nothing for Start to do in a fight -- and a press that reached
	// the game on the frames after the page opened would be a press the game read while the port was acting on
	// it. Both pads, because either player may ask.
	//
	// Called from where the host composes the frame's controls, and it returns true on the EDGE: the page is
	// opened once a press, not once a frame of a press held down.
	bool train_pause_start(m3::Controls &c)
	{
		if (!training_on || !train_fighting) { train_start_was = false; return false; }
		const bool now = c.start[0] || c.start[1];
		c.start[0] = c.start[1] = false;
		const bool edge = now && !train_start_was;
		train_start_was = now;
		if (!edge || leaving != L_OFF || wants != W_NONE) return false;
		wants = W_TRAIN;
		fprintf(stderr, "Fighting Vipers 2: Start in a training -- the page, the round held under it\n");
		return true;
	}

	// EXIT from that page, and it is NOT the panel's road. The panel's match is already decided and its way out
	// asks for GAME OVER with the game's own write; a training's round is being FOUGHT, and the road for that
	// one is the game's own end of a round -- see leave_match's second argument.
	void train_pause_exit(m3::Model3 &b)
	{
		fprintf(stderr, "Fighting Vipers 2's training: EXIT chosen, the round still being fought\n");
		leave_match(b, L_TO_TITLE, false);
	}

	// The frame's inputs, after the player's and the script's.
	void controls(m3::Controls &c)
	{
		if (inject_start > 0) { c.start[0] = true; inject_start--; }
		if (inject_start_p2 > 0) { c.start[1] = true; inject_start_p2--; }
	}

	// ---- the stage page answered ----
	// The scene is 1 to 14 as the page counts its pictures; 0 means the game picks its own, as on the board.
	// AND THE SELECT SCREEN IS LET GO OF IN THE SAME BREATH: it has been running all along, parked in Sega's
	// empty sub-state, so answering the page is only a matter of asking for DECIDE_INT -- the one it would have
	// gone to itself.
	void stage_chosen(m3::Model3 &b, int chose)
	{
		scene = chose >= 1 && chose <= 26 ? chose : 0;
		if (scene) {
			const int said = b.fv2_hold_stage(scene);
			fprintf(stderr, "Fighting Vipers 2's stage: %d held, the draw's table says %d\n", scene, said);
		} else {
			release_stage(b);
			fprintf(stderr, "Fighting Vipers 2's stage: nothing held, the game picks its own\n");
		}
		stage_held = scene >= 1;
		// FROM THE PANEL THE FIGHT HAS TO BE RESTARTED, or nothing is seen to happen: holding the draw's table
		// decides the arena the game LOADS, and a fight already has its own loaded. The game loads one at round
		// 1, so the arena changes by the road a rematch takes.
		if (stage_from_panel) {
			stage_from_panel = false;
			rematch(b);
		} else {
			b.fv2_leave_stage_select();
		}
	}

	// Backed out of the page: the game chooses its own arena, which is what it does on the board.
	void stage_cancelled(m3::Model3 &b)
	{
		if (stage_from_panel) {
			// AND THERE IS NOWHERE TO GO BACK TO at the end of a match: the panel is what this page was opened
			// from, and the game is standing one tick before the branch point that would send it to GAME OVER.
			// So backing out here asks for the panel again rather than letting the game go.
			stage_from_panel = false;
			hold_panel(b);
			wants = W_PANEL;
			fprintf(stderr, "Fighting Vipers 2's stage: backed out -- the end-of-match panel again\n");
			return;
		}
		release_stage(b);
		scene = 0;
		b.fv2_leave_stage_select();
		fprintf(stderr, "Fighting Vipers 2's stage: backed out, the game picks its own\n");
	}

	// ---- the end-of-match panel answered ----
	void panel_chosen(m3::Model3 &b, int line)
	{
		fprintf(stderr, "Fighting Vipers 2's end of match: %s chosen\n", line_name(line));
		switch (line) {
		case L_REMATCH:
			rematch(b);
			break;
		case L_STAGE:
			// THE PAGE, the very one the row opens, instead of a scene number stepped sideways with no picture
			// to go by. Its answer restarts the match in what was chosen (stage_chosen above).
			stage_from_panel = true;
			wants = W_STAGE;
			break;
		case L_VIPERS:
			leave_match(b, L_TO_SELECT);
			break;
		case L_EXIT:
			// The game's own title screen, and the mode let go of with it (arrive): out of the versus, back where
			// a coin and a press of Start start another one.
			leave_match(b, L_TO_TITLE);
			break;
		default:
			break;
		}
	}

	// THE PANEL CLOSED WITHOUT AN ANSWER, and there is nowhere to go: the match is decided and the fight it
	// would go back to is over. So it is armed again rather than the game let go -- one frame runs, the
	// winner's pose asks to move on again, and the panel is back on the very next step. Which is the console
	// build's rule said in this package's own terms: there, the panel simply refuses to be backed out of.
	void panel_dismissed() { over_done = false; }

	// ---- THE WAY OUT ASKED FOR BY A MEASURING RUN (VF3_FV2_GO_HOME=<frame>, main.cpp) ----
	//
	// Not a line of the menu and not reachable by playing: the panel wants a DECIDED TWO-PLAYER match, which
	// a scripted run cannot stage -- player 2 is a person. And the walk that panel opens is the one piece of
	// this file that no amount of reading settles, which is how it shipped with a black screen in it.
	//
	// So the harness asks for the walk at a frame of its own choosing, out of a ONE-player game whose
	// end-of-game screens are the very ones it has to get through. Pointed at the one-player equivalent of
	// the panel's own moment -- VIC_DSP with the match decided, which the trace puts at frame 7182 of
	// tools/scripts/fv2-end-of-game.txt -- the only thing it does not reproduce is the players byte reading 3
	// instead of 1, and nothing in either phase asks that byte anything.
	void measure_way_out(m3::Model3 &b, bool to_select)
	{
		if (leaving != L_OFF) return;
		at = IN;
		fprintf(stderr, "Fighting Vipers 2: the way out asked for by a measuring run (%s)\n",
		        to_select ? "the select screen" : "the title screen");
		leave_match(b, to_select ? L_TO_SELECT : L_TO_TITLE);
	}
	// The mode let go of from outside -- the row chosen again, something online starting, the port closing.
	void let_go(m3::Model3 &b, const char *why)
	{
		if (at == OFF) return;
		put_back(b);
		at = OFF;
		wants = W_NONE;
		veil = false;
		leaving = L_OFF;
		fprintf(stderr, "Fighting Vipers 2: VERSUS let go of (%s)\n", why);
	}

private:
	enum { OFF, ASKED, IN };
	static constexpr int GIVE_UP = 900;        // 15 s of the game's own frames
	static constexpr int SEL_P2_AT = 30;       // frames after the select screen is seen, before player 2's Start
	static constexpr int LEAVE_GIVE_UP = 3600; // a minute: a match that will not end is let go of

	uint8_t at = OFF;
	int wait = 0;
	int inject_start = 0, inject_start_p2 = 0;
	int scene = 0;                  // the arena held, 0 for none
	bool stage_held = false;
	bool stage_from_panel = false;  // the page was opened by the panel's CHANGE STAGE, not by the select screen
	uint32_t task_live[2] = { 0, 0 };

	// ---- TRAINING's own state, and one frame of it ----
	static constexpr int TRAIN_DELAY = 48;    // eight tenths of a second without new damage before the fill
	static constexpr int TRAIN_FLOOR = 25;    // per cent of the starting health, below which the life is held
	int train_was[2] = { -1, -1 };       // the life as we last left it: our own writing is never damage
	int train_quiet[2] = { 0, 0 };       // frames since this player last took a hit
	bool train_p1_chose = false;         // player 1 has picked his own fighter: from here his pad drives 2P's cursor
	bool train_punch_held = false;       // ... and that press has to be let go before it is routed on
	bool train_pick_arm = false;         // ... and the press that opened the screen is not a choice at all
	bool train_froze = false;            // whether the round's clock is stopped BY US, and so to be given back
	bool leave_training = false;         // whether the way out now running leads back into a training
	bool train_start_was = false;        // Start as it read last frame: the page opens on the press, not on the hold

	void train_arm()
	{
		for (int i = 0; i < 2; i++) { train_was[i] = -1; train_quiet[i] = 0; train_took[i] = 0; }
		train_p1_chose = train_punch_held = train_pick_arm = false;
		train_hush = 0;
	}

	void step_training(m3::Model3 &b)
	{
		train_fighting = false;
		if (!training_on) {
			train_hush = 0;
			train_p1_chose = false;
			train_punch_held = false;
			train_pick_arm = false;
			if (train_froze) { b.fv2_freeze_timer(false); train_froze = false; }
			return;
		}
		const int m = b.fv2_mode(), su = b.fv2_submode();
		if (m != 4 && m != 5) {
			// off the select screen none of the three latches means anything, and the next trip through it
			// starts again with the button to be released first
			train_p1_chose = false;
			train_punch_held = false;
			train_pick_arm = false;
		}
		// THE PHASES BEFORE THE FIGHT, CUT -- AND THESE ARE NOT THE LOAD. GAME's twenty sub-states are
		// ROUND_MASK_INT, _IN, _DSP, _OUT (0..3), then ROUND_INT, ROUND_DSP, SET_INT, SET_INT_POSE, SET_DSP,
		// READY_INT, READY_DSP (4..10), then FIGHT_INT and FIGHT_DSP. The first four are the arena arriving:
		// ROUND_MASK_DSP does nothing but WAIT for the work outstanding at 0x002750B8 to fall to zero, so its
		// length IS the load's length and cutting it starts the fight on an arena that has not arrived -- the
		// black screen with nothing on it but the HUD, got three times on the console build. The seven that
		// follow load nothing: they are ROUND X, the pose and READY, and they are what goes.
		if ((m == 6 || m == 7) && su >= 4 && su <= 10) b.fv2_cut_submode();
		// and the window the announcer is dropped in, opened by those same states (see THE ANNOUNCEMENT)
		if ((m == 6 || m == 7) && su >= 4 && su <= 10) train_hush = 12;
		else if (train_hush > 0) train_hush--;
		const bool fighting = (m == 6 || m == 7) && (su == 11 || su == 12);
		train_fighting = fighting;
		// The clock, held by the game's own switch. Held rather than written once because that CONFIG block is
		// a working copy the game loads from its backup.
		b.fv2_freeze_timer(fighting);
		train_froze = fighting;
		if (!fighting) {
			for (int i = 0; i < 2; i++) { train_was[i] = -1; train_quiet[i] = 0; }
			return;
		}
		for (int who = 1; who <= 2; who++) {
			const int k = who - 1;
			const int max = b.fv2_life_max(who), now = b.fv2_life(who);
			if (max <= 0 || now < 0) { train_was[k] = -1; continue; }
			const int floor = max * TRAIN_FLOOR / 100;
			if (train_was[k] < 0) { train_was[k] = now; train_quiet[k] = 0; }
			// DAMAGE IS WHAT THE GAME TOOK OFF SINCE WE LAST LEFT IT, never what we put there ourselves: every
			// write below sets train_was to what it wrote, so the next frame compares against ours.
			const int hit = train_was[k] - now;
			if (hit > 0) {
				train_took[k] += hit;
				train_quiet[k] = 0;
			} else if (++train_quiet[k] > 60 * 60) {
				train_quiet[k] = 60 * 60;
			}
			int want = now;
			if (now < floor) want = floor;                       // never low enough to be knocked out
			else if (train_quiet[k] >= TRAIN_DELAY) want = max;   // quiet long enough: the life comes back
			if (want != now) b.fv2_set_life(who, want);
			train_was[k] = want;
		}
	}

	// ---- the trip through the select screen ----
	//
	//   AWAY   not on SEL at all (modes 4 and 5 are SEL_INT and SEL_DSP);
	//   SEEN   the screen is up and HAS BEEN SEEN up -- sub-state 0 or 1, SEL_START or SELECT. That condition
	//          is not pedantry: the sub-state byte is stale when a mode begins, and entering SEL it still held
	//          what ADV had left there;
	//   DONE   the fighters are chosen. AND IT IS THE REQUEST THAT SAYS SO, not the sub-state: SELECT writes
	//          DECIDE_INT into SUBMODE_REQUEST and Sega's dispatcher acts on it the NEXT tick, so reading the
	//          request catches the game in the one tick where DECIDE_INT has been asked for and has not run.
	//          The page opens there and the game is parked on the spot, overwriting the request before the game
	//          reads it: DECIDE_INT then runs exactly once, when the page lets go. Reading the sub-state
	//          instead catches it a tick late and runs DECIDE_INT twice.
	enum { SEL_AWAY, SEL_SEEN, SEL_DONE };
	uint8_t sel_at = SEL_AWAY;
	int sel_frames = 0;
	bool sel_two_players = false;
	bool sel_wants_page = false;

	void step_select(m3::Model3 &b)
	{
		const int m = b.fv2_mode(), su = b.fv2_submode(), ask = b.fv2_submode_request();
		if (m != 4 && m != 5) { sel_at = SEL_AWAY; sel_frames = 0; return; }
		if (sel_at == SEL_AWAY) {
			if (su == 0 || su == 1) { sel_at = SEL_SEEN; sel_frames = 0; }
			return;
		}
		if (sel_at != SEL_SEEN) return;
		++sel_frames;
		if (sel_two_players && sel_frames == SEL_P2_AT) {
			inject_start_p2 = 8;
			sel_two_players = false;
		}
		if (ask < 4 && su < 4) return;   // the fighters are not chosen yet
		sel_at = SEL_DONE;
		if (!sel_wants_page) return;
		sel_wants_page = false;          // (spent: a later game asks for it again by choosing the row again)
		wants = W_STAGE;
		b.fv2_park_stage_select();
	}

	// THE TWO TASK REGISTERS AS THEY READ WHILE A FIGHT RUNS. The versus is player 2 stepping into player 1's
	// game, and the board's rule is that the WINNER INHERITS it -- so at the end of a match the loser's task
	// falls to 0 and that pad goes dead. Read here and nowhere else, because a value read after the match is
	// over is the stopped one, which is exactly what must not be handed back (Model3::fv2_rematch).
	void note_tasks(m3::Model3 &b)
	{
		const int m = b.fv2_mode(), su = b.fv2_submode();
		if ((m != 6 && m != 7) || (su != 11 && su != 12)) return;   // FIGHT_INT and FIGHT_DSP: the round itself
		const int t1 = b.fv2_task_state(), t2 = b.fv2_task2_state();
		if (t1 > 0) task_live[0] = uint32_t(t1);
		if (t2 > 0) task_live[1] = uint32_t(t2);
	}

	void rematch(m3::Model3 &b)
	{
		over_done = false;
		const int said = b.fv2_rematch(task_live[0], task_live[1]);
		fprintf(stderr, "Fighting Vipers 2: a rematch asked for, the game says %d (1 written, 0 no life block to"
		                " trust); the tasks seen fighting were %08X and %08X\n", said, task_live[0], task_live[1]);
	}

	// ---- the end of a two-player match ----
	bool over_done = false;   // this match's panel has been shown: one a match, not one a tick
	void step_over(m3::Model3 &b)
	{
		if (leaving != L_OFF || wants != W_NONE) return;
		const int m = b.fv2_mode(), su = b.fv2_submode();
		if (m != 6 && m != 7) { over_done = false; return; }   // off GAME: the next match gets its own
		// AND A ROUND BEING FOUGHT IS WHAT ARMS IT AGAIN. Leaving GAME is not enough: a rematch goes back into
		// a match by asking for GAME_INT, so the mode never leaves 6/7 and the second match of a session would
		// have found this already spent.
		if (su == 11 || su == 12) over_done = false;
		if (su != 18 || over_done) return;                     // VIC_DSP, and nowhere else
		if (b.fv2_submode_request() != 19) return;             // ... at the tick it asks to move on
		// AND ONLY FOR TWO PLAYERS, which the game says itself: the arcade ladder's own matches end at VIC_DSP
		// too, and there the end of a match is the ladder's business and not the player's.
		if (b.fv2_players() != 3 || !b.fv2_match_over()) return;
		over_done = true;
		hold_panel(b);
		wants = W_PANEL;
		fprintf(stderr, "Fighting Vipers 2: the versus is decided at VIC_DSP -- the match's own panel,"
		                " not GAME OVER\n");
	}

	// THE GAME HELD WHERE IT STANDS while the panel is up. The winner's pose asked to move on and the panel
	// went up instead, so the request is put back to the sub-state the game is actually in -- otherwise
	// NEXT_PROG_SEL is left waiting to run the instant the panel goes, and what it does then depends on VS
	// FINISH: GAME OVER if it is on, the winner handed an arcade ladder if it is off.
	//
	// ONE WRITE AND NOT A HOLD, which is where this parts from the console build: the panel is a page of this
	// port's own menu, and the menu stops the game. Nothing advances under it, so nothing has to be renewed.
	void hold_panel(m3::Model3 &b) { b.fv2_hold_submode(); }

	// ---- LEAVING A MATCH, in two phases, and the game does the work in both ----
	//
	// ENDING    the game's own exit. From a round being fought that is fv2_end_match -- the round timer put at
	//           1, so FIGHT_DSP ends the round itself, and the bit NEXT_PROG_SEL branches on set, so that
	//           handler decides the match is over instead of opening another round. FROM THE PANEL THERE IS
	//           NOTHING TO END: the match is already decided and the branch point that would have ended it is
	//           out of reach -- the byte it tests, 0x0058145A, falls from 3 to 1 in the very frame it runs, the
	//           INTRUDE rule handing the winner the game. So that road writes what Sega's own three
	//           instructions write and goes straight to the end-of-game screens.
	// CLEARING  CONTINUE and GAME OVER are MODES of their own, so the sub-state request does not reach them and
	//           their own counter is the only lever. They are walked through and made SHORT rather than jumped
	//           over: asking for the title from inside them takes their housekeeping with them and the title
	//           comes up glitched. It is WATCHED rather than written once, each of those screens arming its own
	//           timer as it begins. Over when the SCENE INDEX changes, which is what says the end of the game
	//           has gone past -- the task register has read zero since the K.O. and does not tell them apart.
	//
	// AND THE VEIL OVER ALL OF IT, because what is being walked through is the end of a game nobody was
	// playing: the host hides the picture and silences the sound while this runs (see `veil`). The console
	// build puts its own pictures there; this package has none, so it puts nothing there, which is at least not
	// somebody else's GAME OVER.
	enum leave_phase { L_OFF, L_ENDING, L_CLEARING };
	enum leave_dest { L_TO_TITLE, L_TO_SELECT };
	leave_phase leaving = L_OFF;
	leave_dest leave_for = L_TO_TITLE;
	static constexpr int FV2_TIMER_EDGE = 8;   // (model3.h's own edge: an eighth of a second, firmly positive)
	int leave_frames = 0, leave_cut = -1;
	bool leave_decided = true;   // the panel's road (nothing to end) or a round being fought
	int leave_cut_over = -1;     // GAME OVER asked for on the round's road: once, and only once
	int leave_end = -1;          // what the game's own end of a round said, the one time it is written
	int leave_vs_finish = -1;   // VS FINISH as the player had it, lent for the length of the way out

	void leave_match(m3::Model3 &b, leave_dest what, bool decided = true)
	{
		// AND THE DOOR OUT OF A TWO-PLAYER MATCH IS OPENED FIRST, because the game only reads it when the match
		// ends: when both players are in, the end of a match asks whether VS FINISH is on and, if it is, asks
		// for GAME OVER. If it is off, the winner is handed the ladder and the game goes on. It is given back at
		// the far side, and only the working copy is touched, so what the player set in the cabinet's own
		// settings is not changed.
		// LEAVING A MATCH TURNS THE TRAINING OFF, and not merely suspends its holding: the way out finishes
		// the match, and a training that went on filling the lives every frame would lock the player in -- the
		// two held each other, measured on the console build.
		//
		// EXCEPT WHERE THE WAY OUT LEADS BACK INTO A FIGHT, which is Frederic's of the 2026-10-28: CHANGE YOUR
		// VIPER in a training was taking the training with it -- the clock ran, the lives fell, the column was
		// gone -- and a player who asked to change his fighter did not ask to leave the mode. So that road
		// REMEMBERS, and arrive() starts another training rather than a plain versus. EXIT still turns it off:
		// the menu is where a mode is chosen, and choosing one is how it comes back.
		leave_training = training_on && what == L_TO_SELECT;
		training_on = false;
		borrow_vs_finish(b);
		leaving = L_ENDING;
		leave_for = what;
		leave_decided = decided;
		leave_frames = 0;
		leave_cut = -1;
		leave_cut_over = -1;
		leave_end = -1;
		veil = true;
		// AND THE ROAD IS CHOSEN HERE, because the two are not the same thing at all.
		//
		// THE PANEL'S ROAD: the match is already decided, there is nothing to end, and the branch point that
		// would have ended it is out of reach (Model3::fv2_request_game_over says why). So GAME OVER is asked
		// for with the game's own write, once, and the dispatcher takes the game out of GAME on the next tick.
		//
		// THE ROUND'S ROAD is the other one, and it is what Start in a training needs: the round is being
		// FOUGHT, so there IS something to end, and the game ends it itself once it is told the two words its
		// own handlers read. That is step_leaving's business below, every frame, because one of those words
		// does not keep.
		if (leave_decided) {
			b.fv2_request_game_over();
			fprintf(stderr, "Fighting Vipers 2: the match was already decided -- GAME OVER asked for, the game's"
			                " own way out of a settled versus\n");
		}
	}

	void borrow_vs_finish(m3::Model3 &b)
	{
		if (leave_vs_finish >= 0) return;
		leave_vs_finish = b.fv2_vs_finish();
		if (leave_vs_finish < 0) return;
		if (leave_vs_finish == 0) b.fv2_set_vs_finish(1);
		fprintf(stderr, "Fighting Vipers 2: VS FINISH was %d, lent as 1 for the way out\n", leave_vs_finish);
	}
	void return_vs_finish(m3::Model3 &b)
	{
		if (leave_vs_finish < 0) return;
		b.fv2_set_vs_finish(leave_vs_finish);
		fprintf(stderr, "Fighting Vipers 2: VS FINISH given back as %d\n", leave_vs_finish);
		leave_vs_finish = -1;
	}

	void step_leaving(m3::Model3 &b)
	{
		if (leaving == L_OFF) return;
		if (++leave_frames > LEAVE_GIVE_UP) {
			fprintf(stderr, "Fighting Vipers 2: the way out gave up after %d frames -- mode %d sub %d\n",
			        leave_frames, b.fv2_mode(), b.fv2_submode());
			leaving = L_OFF;
			arrive(b);
			return;
		}
		const int m = b.fv2_mode();
		if (getenv("VF3_LEAVE_TRACE") || leave_frames % 60 == 0)
			fprintf(stderr, "Fighting Vipers 2: the way out, %s, %d frames -- mode %d sub %d, screen %d,"
			                " title %d, the match says %s\n", leaving == L_ENDING ? "ending" : "clearing",
			        leave_frames, m, b.fv2_submode(), b.fv2_screen_timer(), b.fv2_on_title() ? 1 : 0,
			        b.fv2_match_over() ? "decided" : "not decided");
		if (leaving == L_ENDING) {
			// AND A ROUND BEING FOUGHT IS ENDED BY THE GAME ITSELF, which is the road Start in a training takes
			// (leave_match). Three things, all of them the game's own and all measured on the console build:
			//
			//   fv2_end_match, ONCE: the round's own clock put at 1, so FIGHT_DSP ends the round on the frame it
			//   reaches 0 and marks the time-out itself -- nobody is knocked out and nobody's life is touched --
			//   and the bit NEXT_PROG_SEL branches on set, so that handler decides the match is over instead of
			//   opening another round.
			//
			//   THE MATCH HELD DECIDED, every frame while the game is still in GAME, because that bit DOES NOT
			//   KEEP: between the round's end and the branch point the game writes that word itself, and what it
			//   writes there is its own count of the rounds won. A hidden run caught the game fighting five more
			//   rounds for exactly that reason.
			//
			//   AND GAME OVER ASKED FOR ONCE THE FIGHT ITSELF IS OVER -- any sub-state but FIGHT_INT and
			//   FIGHT_DSP -- because the round ended on the CLOCK, so the players-out byte stayed 0, and
			//   NEXT_PROG_SEL's own arithmetic then hands the last one standing the next opponent. Left to
			//   itself the ladder would simply go on, which is what it did from ARCADE on the console build:
			//   the way out waited for an attract loop that never came while the game fought on.
			//
			// The judgement and the winner's pose are each cut short once, by asking for the next sub-state,
			// which is what the game's own handlers do between themselves. NEXT_PROG_SEL (19) is left alone: it
			// is the branch point all of this was written for, and cutting it would cut the decision itself.
			//
			// AND THE FREEZE HAS TO GO WITH IT, which is the trap this road had in the console build: the clock
			// put at 1 is counted down by FIGHT_DSP, and FIGHT_DSP does not count AT ALL while the training's
			// freeze bit is set. The match could not end, so there was nothing to leave. leave_match turns the
			// mode off and step_training gives the bit back in the same frame as the write below.
			if (!leave_decided && (m == 6 || m == 7)) {
				if (leave_end < 0) {
					leave_end = b.fv2_end_match();
					fprintf(stderr, "Fighting Vipers 2: the round ended by the game's own two words, which say"
					                " %d (1 written, -1 not a round being fought)\n", leave_end);
				}
				b.fv2_hold_match_over();
				const int su = b.fv2_submode();
				if (leave_cut_over < 0 && b.fv2_match_over() && (su < 11 || su > 12)) {
					leave_cut_over = 1;
					b.fv2_request_game_over();
					fprintf(stderr, "Fighting Vipers 2: the match is decided -- GAME OVER asked for, or the"
					                " ladder would have gone on\n");
				}
				if (su >= 13 && su <= 18 && su != leave_cut) {
					leave_cut = su;
					b.fv2_cut_submode();
				}
			}
			// OUT OF GAME, AND THE MODE IS WHAT SAYS SO.
			//
			// It used to be "no game running", and THAT WAS THE BLACK SCREEN Frederic got from CHANGE YOUR
			// VIPER. The task register at 0x00700000 READS ZERO FROM THE K.O. ONWARDS -- measured over a whole
			// one-player game, tools/scripts/fv2-end-of-game.txt: at frame 6731 the deciding K.O. landed and
			// the register was already 0 with the mode still 7 and GAME OVER a thousand frames away. So this
			// phase ended on its own FIRST frame, before the game had left the fight at all, and everything
			// after it ran in the wrong order behind a veil nobody could see past.
			//
			// The mode does not lie: 6 and 7 are GAME, and the game is out of the match when it is neither.
			// The end-of-game screens are modes of their own -- CONTINUE 8 and 9, RANKING 16 and 17, OVER 18
			// and 19, RESULT 22 and 23 -- and the attract loop is 2 and 3, so every road out of a fight shows
			// up here as a mode that is not 6 or 7.
			if (m == 6 || m == 7) return;
			fprintf(stderr, "Fighting Vipers 2: the match is over in %d frames (mode %d), the end screens"
			                " next\n", leave_frames, m);
			leave_frames = 0;
			leaving = L_CLEARING;
			return;
		}
		// L_CLEARING, AND THE TITLE SCREEN IS THE END OF IT FOR BOTH ROADS.
		//
		// IT USED TO BE THE SCENE INDEX CHANGING, and that number is the ARENA'S. Measured over the same run:
		// it was 9 from the K.O. at 6731 all the way to 8276 -- unchanged through CONTINUE, through GAME OVER
		// and into the attract loop's first screens -- and it moved only at 8468, when the title came up. To 1
		// that time, and to 21 on two other passes of the same run, because what it holds is whichever demo
		// the attract loop happened to load next. So the phase waited a thousand frames for a byte that says
		// nothing at all about the end of a game, and on another pass it might have waited out the minute.
		//
		// THE TITLE SCREEN SAYS IT, and says it the same way every time: mode 2 or 3 at ADV_TITLE, which that
		// run shows at 1193, 8468 and 13285 and nowhere else. The attract loop reaches it BY ITSELF, in about
		// 196 frames from ADV_INT, and every screen on the way keeps its own counter -- so shortening that
		// counter every frame is what turns the 196 into a handful, and it is the only lever this phase ever
		// needed. Firmly positive, because an already expired screen reads negative and shortening it would
		// lengthen it; four times our own edge, so our own writing is never written over again. And every
		// frame rather than once, because each of those screens arms its own timer as it begins.
		//
		// It also means CHANGE YOUR VIPER arrives where a press of Start MEANS something, which is the other
		// half of the same bug: the old road handed the game a Start wherever the scene index happened to
		// move, and a Start on a ranking screen starts nothing.
		if (b.fv2_screen_timer() > 4 * FV2_TIMER_EDGE) b.fv2_expire_screen();
		if (!b.fv2_on_title()) return;
		fprintf(stderr, "Fighting Vipers 2: the title screen reached in %d frames\n", leave_frames);
		leaving = L_OFF;
		arrive(b);
	}

	// Where the way out leads. The title screen is the game's own lever and it WORKS here where it does nothing
	// from a fight: by now the attract loop is running again, so there IS a list to walk off. CHANGE YOUR VIPER
	// asks for another versus instead, which from the attract loop is a new game -- and the select screen with
	// it, two players on it, which is the whole point of the line.
	void arrive(m3::Model3 &b)
	{
		return_vs_finish(b);
		veil = false;
		if (leave_for == L_TO_TITLE) {
			if (!b.fv2_on_title()) b.fv2_attract_to_title();
			put_back(b);
			at = OFF;
			fprintf(stderr, "Fighting Vipers 2: back at the title screen, VERSUS over\n");
			return;
		}
		// The mode comes back with the match, the match being what was asked to change. The arena is kept: a
		// player who asked to change his fighter did not ask to change the stage.
		sel_at = SEL_AWAY;
		sel_frames = 0;
		sel_two_players = true;
		sel_wants_page = false;   // (the arena is already chosen; asking again would be a page to dismiss)
		over_done = false;
		task_live[0] = task_live[1] = 0;
		at = ASKED;
		wait = 0;
		inject_start = 8;
		// and the training put back where the way out was asked for from inside one (see leave_match)
		if (leave_training) {
			leave_training = false;
			training_on = true;
			train_arm();
		}
		fprintf(stderr, "Fighting Vipers 2: back to the select screen, two players%s\n",
		        training_on ? ", training again" : "");
	}

	void release_stage(m3::Model3 &b)
	{
		if (!stage_held) return;
		b.fv2_release_stage();
		stage_held = false;
	}

	void put_back(m3::Model3 &b)
	{
		release_stage(b);
		return_vs_finish(b);
		// THE MODE GOES OUT WITH THE GAME, and the clock is given back with it. Leaving a match already turned
		// it off (leave_match); a training that ends by ITSELF -- the game over, the attract loop back -- used
		// to leave it on, and the next game would have been one too. The switch is the game's own byte and a
		// switch left set is a round with no clock in a game nobody asked to train in.
		training_on = false;
		leave_training = false;
		train_fighting = false;
		train_hush = 0;
		if (train_froze) { b.fv2_freeze_timer(false); train_froze = false; }
		scene = 0;
		stage_from_panel = false;
		over_done = false;
		inject_start = inject_start_p2 = 0;
	}
};
