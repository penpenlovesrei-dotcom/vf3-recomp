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
#include <cstdio>
#include <cstdlib>

class Fv2Versus
{
public:
	// The panel's four lines. They are the console build's own four, less RESUME: there is no fight to go back
	// to at the end of a match, and a line that promises one is a line that lies.
	enum { L_REMATCH, L_STAGE, L_VIPERS, L_MENU, L_COUNT };
	static const char *line_name(int l)
	{
		static const char *const N[L_COUNT] = { "REMATCH", "CHANGE STAGE", "CHANGE YOUR VIPER", "MAIN MENU" };
		return l >= 0 && l < L_COUNT ? N[l] : "?";
	}

	// What the host has to put on screen. It reads this, opens the page or the panel, and clears it; nothing
	// here opens anything, because the host owns the surfaces.
	enum { W_NONE, W_STAGE, W_PANEL };
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
		const bool idle = b.fv2_in_attract();
		if (idle) inject_start = 8;
		fprintf(stderr, "Fighting Vipers 2: VERSUS chosen%s\n",
		        idle ? ", and the game handed a press of Start" : " (a game is already running: player 2 steps in)");
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
		case L_MENU:
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
	int leave_frames = 0, leave_cut = -1, leave_scene = -1;
	int leave_vs_finish = -1;   // VS FINISH as the player had it, lent for the length of the way out

	void leave_match(m3::Model3 &b, leave_dest what)
	{
		// AND THE DOOR OUT OF A TWO-PLAYER MATCH IS OPENED FIRST, because the game only reads it when the match
		// ends: when both players are in, the end of a match asks whether VS FINISH is on and, if it is, asks
		// for GAME OVER. If it is off, the winner is handed the ladder and the game goes on. It is given back at
		// the far side, and only the working copy is touched, so what the player set in the cabinet's own
		// settings is not changed.
		borrow_vs_finish(b);
		leaving = L_ENDING;
		leave_for = what;
		leave_frames = 0;
		leave_cut = -1;
		leave_scene = -1;
		veil = true;
		// The panel's road: the match is already decided, so there is nothing to end and GAME OVER is asked for
		// with the game's own write, once.
		b.fv2_request_game_over();
		fprintf(stderr, "Fighting Vipers 2: the match was already decided -- GAME OVER asked for, the game's"
		                " own way out of a settled versus\n");
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
		if (leaving == L_ENDING) {
			if (leave_frames % 60 == 0 || getenv("VF3_LEAVE_TRACE"))
				fprintf(stderr, "Fighting Vipers 2: the way out is %d frames old -- mode %d sub %d, the match"
				                " says %s\n", leave_frames, b.fv2_mode(), b.fv2_submode(),
				        b.fv2_match_over() ? "decided" : "not decided");
			// THE MATCH IS OVER WHEN THE GAME SAYS SO, and it says it in TWO ways. The attract loop coming back
			// is one. The other is a mode of its own: CONTINUE (8, 9), RANKING (16, 17), OVER (18, 19), RESULT
			// (22, 23) -- the end-of-game screens, which the game walks to with its tasks still running, so
			// "no game running" goes on saying a game is on.
			const int m = b.fv2_mode();
			const bool end_screen = m == 8 || m == 9 || m == 16 || m == 17 || m == 18 || m == 19 || m == 22 || m == 23;
			if (b.fv2_in_attract() || end_screen) {
				fprintf(stderr, "Fighting Vipers 2: the match is over in %d frames, the end screens next\n",
				        leave_frames);
				leave_scene = b.fv2_scene_index();
				leave_frames = 0;
				leaving = L_CLEARING;
			}
			return;
		}
		// L_CLEARING
		if (leave_for == L_TO_TITLE && b.fv2_on_title()) {
			fprintf(stderr, "Fighting Vipers 2: the title screen reached in %d frames\n", leave_frames);
			leaving = L_OFF;
			arrive(b);
			return;
		}
		// Firmly positive, because an already expired screen reads negative and shortening it would lengthen
		// it; four times our own edge, so our own writing is never written over again. And every frame it is
		// still positive rather than once: each of those screens arms its own timer as it begins, so one write
		// only shortens the first of them.
		if (b.fv2_screen_timer() > 4 * 8) b.fv2_expire_screen();
		if (b.fv2_scene_index() != leave_scene) {
			fprintf(stderr, "Fighting Vipers 2: the end of the game is past in %d frames\n", leave_frames);
			leaving = L_OFF;
			arrive(b);
		}
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
		fprintf(stderr, "Fighting Vipers 2: back to the select screen, two players\n");
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
		scene = 0;
		stage_from_panel = false;
		over_done = false;
		inject_start = inject_start_p2 = 0;
	}
};
