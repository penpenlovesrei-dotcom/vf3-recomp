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
// AND IT IS NOT A SETTING. A row is not a yes and a no: it starts a game in that mode, as choosing a mode on
// a console does. The menu closes, the game is handed a real press of Start, and the mode is over when that
// game is -- which is the shape Survival and Random already have (fv2_modes.h).
#pragma once
#include "../machine/model3.h"
#include "fv2_home.h"
#include <cstdio>
#include <cstdlib>

class Fv2Versus
{
public:
	// THE WAY HOME IS NOT THIS FILE'S ANY MORE (fv2_home.h): walking a game back to a place where a new
	// game can begin is what every row of the menu needs and not only the panel's two lines, so the
	// machine is shared and the host owns the one copy of it. What is left here is the asking.
	explicit Fv2Versus(Fv2Home &h) : home(h) {}

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
	enum { W_NONE, W_STAGE, W_PANEL };
	int wants = W_NONE;

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
		// AND IT BEGINS AT THE SELECT SCREEN OR IT DOES NOT BEGIN. Player 2 steps into player 1's game on the
		// select screen and nowhere else, so a versus asked for during a match used to wait for a screen that
		// was never coming and show the player nothing -- Frederic's of the 2026-11-14. The game is walked
		// home first now and this is called on arrival (fv2_home.h, main.cpp).
		if (!b.fv2_in_attract()) {
			at = OFF;
			fprintf(stderr, "Fighting Vipers 2: VERSUS chosen, but a game was running -- let go of rather than"
			                " left waiting for a select screen\n");
			return;
		}
		inject_start = 8;
		fprintf(stderr, "Fighting Vipers 2: VERSUS chosen, and the game handed a press of Start\n");
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

		note_tasks(b);
		step_select(b);
		step_over(b);
		// THE ARENA HELD, not written once: the game unpacks and reloads its own code, and the draw marks its
		// own choice used as it takes it. The roster patch's discipline.
		if (scene >= 1) b.fv2_hold_stage(scene);

		// And over when the game is over. A rematch never leaves the fight, so this is the attract loop and
		// nothing else: the arena given back, the panel's own state cleared, the row ready to be chosen again.
		if (at == IN && !home.on() && !running && wants == W_NONE) {
			fprintf(stderr, "Fighting Vipers 2: VERSUS over, the game being over\n");
			put_back(b);
			at = OFF;
		}
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
			// The select screen, and the mode kept: it is the MATCH that was asked to change (again).
			home.go(b, Fv2Home::SELECT, true, Fv2Home::A_VERSUS_AGAIN);
			break;
		case L_EXIT:
			// The game's own title screen, and the mode let go of with it (done): out of the versus, back where
			// a coin and a press of Start start another one.
			home.go(b, Fv2Home::TITLE, true, Fv2Home::A_NOTHING);
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

	// ---- THE TWO ARRIVALS, handed back by the host on the frame the way home ends ----
	//
	// CHANGE YOUR VIPER asks for another versus, which from the attract loop is a new game -- and the select
	// screen with it, two players on it, which is the whole point of the line. The arena is KEPT: a player
	// who asked to change his fighter did not ask to change the stage.
	void again(m3::Model3 &b)
	{
		sel_at = SEL_AWAY;
		sel_frames = 0;
		sel_two_players = true;
		sel_wants_page = false;   // (the arena is already chosen; asking again would be a page to dismiss)
		over_done = false;
		task_live[0] = task_live[1] = 0;
		at = ASKED;
		wait = 0;
		inject_start = 8;
		fprintf(stderr, "Fighting Vipers 2: back to the select screen, two players\n");
	}

	// And EXIT, where the mode is simply over: the arena given back, the panel's own state cleared, the row
	// ready to be chosen again.
	void done(m3::Model3 &b)
	{
		put_back(b);
		at = OFF;
		fprintf(stderr, "Fighting Vipers 2: back at the title screen, VERSUS over\n");
	}
	// The mode let go of from outside -- the row chosen again, something online starting, the port closing.
	void let_go(m3::Model3 &b, const char *why)
	{
		if (at == OFF) return;
		put_back(b);
		at = OFF;
		wants = W_NONE;
		home.give_up(b, why);
		fprintf(stderr, "Fighting Vipers 2: VERSUS let go of (%s)\n", why);
	}

private:
	enum { OFF, ASKED, IN };
	static constexpr int GIVE_UP = 900;        // 15 s of the game's own frames
	static constexpr int SEL_P2_AT = 30;       // frames after the select screen is seen, before player 2's Start

	Fv2Home &home;   // the one way home, the host's (fv2_home.h)

	uint8_t at = OFF;
	int wait = 0;
	int inject_start = 0, inject_start_p2 = 0;
	int scene = 0;                  // the arena held, 0 for none
	bool stage_held = false;
	bool stage_from_panel = false;  // the page was opened by the panel's CHANGE STAGE, not by the select screen
	uint32_t task_live[2] = { 0, 0 };

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
		if (home.on() || wants != W_NONE) return;
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

	void release_stage(m3::Model3 &b)
	{
		if (!stage_held) return;
		b.fv2_release_stage();
		stage_held = false;
	}

	void put_back(m3::Model3 &b)
	{
		release_stage(b);
		// (VS FINISH is the way home's to lend and to give back, and it gives it back on both of its ends --
		// the arrival and the letting go. Nothing of it is owed here.)
		scene = 0;
		stage_from_panel = false;
		over_done = false;
		inject_start = inject_start_p2 = 0;
	}
};
