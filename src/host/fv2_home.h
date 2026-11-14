// The way home: a game taken back to the game's own title screen, or to its own select screen, by the game's
// own roads -- and then whatever was asked for from the menu started there.
//
// WHY IT EXISTS AT ALL is Frederic's of the 2026-11-14: choosing another mode from the menu while a game was
// running did nothing. It did nothing for a reason that was the same in all three rows. A row's whole command
// is a press of Start, and a press of Start starts a game only where the game is waiting for one -- the
// attract loop. Mid-match the rows injected nothing (`if (idle)`), so Survival's and Random's rows wrote their
// flag into the match already being fought, which is not a game in that mode, and the Versus row waited for a
// select screen that was never coming. THE GAME HAS TO BE WHERE A NEW GAME CAN BEGIN before a row means
// anything, and putting it there is this file.
//
// IT IS THE VERSUS'S OWN WAY OUT, MADE COMMON. The end-of-match panel already had to walk a decided match
// home, and all three rows need the same walk from any point of any match, so the machine lives here and the
// panel asks it like everybody else (fv2_versus.h, fv2_modes.h).
//
// TWO PHASES, and the match's own state decides which road the first of them takes:
//
//   ENDING    the match ended, and there are TWO roads to that because the game has two.
//
//             A ROUND BEING FOUGHT is ended by the game's own two conditions -- the round's timer at 1, which
//             its own handler reads as a time-out, and the match's bit on, which is what makes that the end of
//             the MATCH and not of the round (fv2_end_match). Nobody's life is touched. The bit is then put
//             back EVERY frame, because it does not keep: between the round's end and the branch point the
//             game writes that word itself and what it writes is its own count of rounds won, which says the
//             match is not decided. Measured on the console build: asked to end at frame 3000, the game
//             fought another round at 3010 and four more after it.
//
//             A MATCH ALREADY DECIDED has nothing to end, and the branch point that would have ended it is
//             out of reach: the byte it tests falls from 3 to 1 in the very frame it runs, the board's INTRUDE
//             rule handing the winner the game. That road writes what Sega's own three instructions write
//             (fv2_request_game_over) and goes straight to the end-of-game screens. It is the panel's road,
//             and the panel is the only caller that knows in advance which road it is on.
//
//             Either way the judgement, the replay and the winner's pose are CUT SHORT, one cut each, and
//             NEXT_PROG_SEL (19) is left alone -- it is the branch point itself, and what is wanted is for it
//             to run. GAME OVER is then asked for once the fight is behind us and the match reads decided,
//             without which that branch point hands the winner an arcade ladder and the game goes on. Both
//             writes can land in the same frame and the mode request wins, which is the one of the two that
//             was wanted; the cut is what gets there if it does not.
//
//   CLEARING  CONTINUE and GAME OVER are MODES of their own, so the sub-state request does not reach them and
//             their own counter is the only lever. They are walked through and made SHORT rather than jumped
//             over: asking for the title from inside them takes their housekeeping with them and the title
//             comes up glitched. The counter is WATCHED rather than written once, each of those screens arming
//             its own timer as it begins. Over when the SCENE INDEX changes, which is what says the end of the
//             game has gone past -- the task register has read zero since the K.O. and does not tell them
//             apart.
//
// AND A VEIL OVER ALL OF IT, because what is being walked through is the end of a game nobody was playing:
// the host hides the picture and silences the sound while this runs. The console build puts its own pictures
// there; this package has none, so it puts nothing there, which is at least not somebody else's GAME OVER.
#pragma once
#include "../machine/model3.h"
#include <cstdio>
#include <cstdlib>

class Fv2Home
{
public:
	// Where the walk ends. THE TITLE is the game's own lever and it works here where it does nothing from a
	// fight: by now the attract loop is running again, so there IS a list to walk off. THE SELECT SCREEN is
	// one press of Start from where the end screens leave the game, so it is not walked to -- it is asked for,
	// and whoever asked for it hands the press.
	enum dest { TITLE, SELECT };

	// What the walk was for. Carried, not acted on: the host owns the rows and the modes, so the host is what
	// dispatches on arrival. A_MODE_ROW carries the row with it in `row()`.
	enum after { A_NOTHING, A_VERSUS, A_VERSUS_AGAIN, A_MODE_ROW };

	bool on() const { return phase != P_OFF; }
	bool veil() const { return phase != P_OFF; }
	int then() const { return what; }
	int row() const { return which; }
	dest where() const { return going; }

	// Asked for. `decided` is true only where the caller KNOWS the match is settled -- the end-of-match
	// panel's road, where there is nothing to end; anywhere else it is false and the match's own state is what
	// the walk reads.
	void go(m3::Model3 &b, dest where_to, bool already_decided, int then_what, int then_row = -1)
	{
		if (phase != P_OFF) return;   // one walk at a time: the second ask is the first one still running
		going = where_to;
		decided = already_decided;
		what = then_what;
		which = then_row;
		phase = P_ENDING;
		frames = 0;
		cut = -1;
		scene = -1;
		over_asked = false;
		// AND THE DOOR OUT OF A TWO-PLAYER MATCH IS OPENED FIRST, because the game only reads it when the
		// match ends: with both players in, the end of a match asks whether VS FINISH is on and, if it is,
		// asks for GAME OVER. If it is off, the winner is handed the ladder and the game goes on. It is given
		// back at the far side, and only the working copy is touched, so what the player set in the cabinet's
		// own settings is not changed. A one-player game never reads it, and lending it there changes nothing.
		borrow_vs_finish(b);
		if (decided) {
			b.fv2_request_game_over();
			fprintf(stderr, "Fighting Vipers 2: the match was already decided -- GAME OVER asked for, the"
			                " game's own way out of a settled versus\n");
		} else {
			fprintf(stderr, "Fighting Vipers 2: the game asked home (%s) -- the match ended by its own two"
			                " conditions\n", where_to == TITLE ? "the title screen" : "the select screen");
		}
	}

	// Every game frame, once the frame has run. True EXACTLY ONCE, on the frame the game arrives: that is the
	// frame the host does what the walk was for.
	bool step(m3::Model3 &b)
	{
		if (phase == P_OFF) return false;
		if (++frames > GIVE_UP) {
			fprintf(stderr, "Fighting Vipers 2: the way home gave up after %d frames -- mode %d sub %d\n",
			        frames, b.fv2_mode(), b.fv2_submode());
			return arrive(b);
		}
		if (phase == P_ENDING) {
			if (frames % 60 == 0 || getenv("VF3_HOME_TRACE"))
				fprintf(stderr, "Fighting Vipers 2: the way home is %d frames old -- mode %d sub %d, the match"
				                " says %s\n", frames, b.fv2_mode(), b.fv2_submode(),
				        b.fv2_match_over() ? "decided" : "not decided");
			const int m = b.fv2_mode(), su = b.fv2_submode();
			if (m == 6 || m == 7) {
				b.fv2_hold_match_over();                              // it does not keep: see ENDING, above
				if (!decided && (su == 11 || su == 12)) b.fv2_end_match();
				if (b.fv2_match_over() && su > 12 && !over_asked) {
					b.fv2_request_game_over();
					over_asked = true;
				}
				// The judgement, the replay and the winner's pose, one cut each. 19 is NEXT_PROG_SEL and is
				// left alone.
				if (su >= 13 && su <= 18 && su != cut) {
					b.fv2_cut_submode();
					cut = su;
				}
			}
			// THE MATCH IS OVER WHEN THE GAME SAYS SO, and it says it in TWO ways. The attract loop coming
			// back is one. The other is a mode of its own: CONTINUE (8, 9), RANKING (16, 17), OVER (18, 19),
			// RESULT (22, 23) -- the end-of-game screens, which the game walks to with its tasks still
			// running, so "no game running" goes on saying a game is on.
			const bool end_screen = m == 8 || m == 9 || m == 16 || m == 17 || m == 18 || m == 19
			                        || m == 22 || m == 23;
			if (b.fv2_in_attract() || end_screen) {
				fprintf(stderr, "Fighting Vipers 2: the match is over in %d frames, the end screens next\n",
				        frames);
				scene = b.fv2_scene_index();
				frames = 0;
				phase = P_CLEARING;
			}
			return false;
		}
		// P_CLEARING
		if (going == TITLE && b.fv2_on_title()) {
			fprintf(stderr, "Fighting Vipers 2: the title screen reached in %d frames\n", frames);
			return arrive(b);
		}
		// Firmly positive, because an already expired screen reads negative and shortening it would lengthen
		// it; four times our own edge, so our own writing is never written over again. And every frame it is
		// still positive rather than once: each of those screens arms its own timer as it begins, so one write
		// only shortens the first of them.
		if (b.fv2_screen_timer() > 4 * 8) b.fv2_expire_screen();
		if (b.fv2_scene_index() != scene) {
			fprintf(stderr, "Fighting Vipers 2: the end of the game is past in %d frames\n", frames);
			return arrive(b);
		}
		return false;
	}

	// The walk abandoned where it stands: what was borrowed is given back and nothing is dispatched. For the
	// cases that take the game out from under it -- an online match beginning, another row chosen while it
	// runs, the port's own reset.
	void give_up(m3::Model3 &b, const char *why)
	{
		if (phase == P_OFF) return;
		fprintf(stderr, "Fighting Vipers 2: the way home let go of after %d frames (%s)\n", frames, why);
		return_vs_finish(b);
		phase = P_OFF;
		what = A_NOTHING;
		which = -1;
	}

private:
	enum phase_t { P_OFF, P_ENDING, P_CLEARING };
	static constexpr int GIVE_UP = 3600;   // a minute: a match that will not end is let go of

	phase_t phase = P_OFF;
	dest going = TITLE;
	int what = A_NOTHING, which = -1;
	bool decided = false, over_asked = false;
	int frames = 0, cut = -1, scene = -1;
	int vs_finish_was = -1;   // VS FINISH as the player had it, lent for the length of the walk

	void borrow_vs_finish(m3::Model3 &b)
	{
		if (vs_finish_was >= 0) return;
		vs_finish_was = b.fv2_vs_finish();
		if (vs_finish_was < 0) return;
		if (vs_finish_was == 0) b.fv2_set_vs_finish(1);
		fprintf(stderr, "Fighting Vipers 2: VS FINISH was %d, lent as 1 for the way home\n", vs_finish_was);
	}
	void return_vs_finish(m3::Model3 &b)
	{
		if (vs_finish_was < 0) return;
		b.fv2_set_vs_finish(vs_finish_was);
		fprintf(stderr, "Fighting Vipers 2: VS FINISH given back as %d\n", vs_finish_was);
		vs_finish_was = -1;
	}

	// Arrived. The title screen is asked for by the attract sequencer's own road if the walk did not land on
	// it by itself; the select screen is not walked to at all, the attract loop being one press of Start away
	// from it and that press belonging to whoever asked.
	bool arrive(m3::Model3 &b)
	{
		return_vs_finish(b);
		phase = P_OFF;
		if (going == TITLE && !b.fv2_on_title()) b.fv2_attract_to_title();
		fprintf(stderr, "Fighting Vipers 2: home (%s)\n", going == TITLE ? "the title screen" : "the attract loop");
		return true;
	}
};
