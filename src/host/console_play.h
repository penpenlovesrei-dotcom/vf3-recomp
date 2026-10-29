#pragma once
#include "../machine/model3.h"
#include "console_state.h"
#include <vector>
#include <vector>

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
	// WHERE THE CONSOLE IS, one state at a time, the way the game holds its own (console_state.h). Every
	// question this layer used to answer with a handful of booleans -- does the game run, who owns the
	// screen, who owns the sound -- is a property of this one value, read from its table.
	console_ui ui;

	// ---- WHAT THE PORT ASKS OF THE GAME, and the ONE place any of it is written ----
	//
	// Frederic's fourth rule of the 2026-10-25. The surfaces used to write the game's RAM themselves,
	// each from wherever it happened to be: the pause menu ended the match from its own answer, the
	// carousel restarted the fight from its, the list asked for the title from a third place, and the
	// carousel's park was renewed from the host's frame loop. Four hands in the same memory, at four
	// different moments of the frame.
	//
	// Now a surface says WHAT IT WANTS and this layer does it, in order, at the top of step() -- which is
	// the moment that already belonged to it: after the frame that has run, before the frame that will.
	// The game's own dispatcher reads a request once a tick, so a write placed here is read by the very
	// next tick and by no other.
	enum class ask { end_match, rematch, title_now, stage_free, vs_hold };
	std::vector<ask> asked;
	void ask_for(ask a) { asked.push_back(a); }

	// AND A STATE IS ASKED FOR THE SAME WAY. The state is this layer's (console_state.h) but only one
	// place in the host may change it, because changing it opens and closes surfaces -- so what happens
	// here says WHICH state it wants and the host enters it, exactly as a surface says what it wants
	// written and this layer writes it. Without this the end of a way out of a match was read back by
	// the host in two ifs of its own, which is the same thing said twice.
	cstate want_state = cstate::PLAY;
	const char *want_why = nullptr;
	void ask_state(cstate s, const char *why) { want_state = s; want_why = why; }

	void do_asked(m3::Model3 &b)
	{
		for (ask a : asked) switch (a) {
		case ask::end_match:
			fprintf(stderr, "the console: the match asked to end, the game's own exit says %d\n", b.fv2_end_match());
			break;
		case ask::rematch:
			fprintf(stderr, "the console: a rematch asked for, the game says %d (1 written, 0 no life block to \n"
				"trust), the tasks seen fighting were %08X and %08X\n",
				b.fv2_rematch(task_live[0], task_live[1]), task_live[0], task_live[1]);
			break;
		case ask::title_now:
			b.fv2_attract_to_title();
			fprintf(stderr, "the console: the title screen asked for from the attract loop\n");
			break;
		case ask::vs_hold:
			// The winner's pose asked to move on and the panel went up instead: the request is put back to
			// the sub-state the game is actually in, so NEXT_PROG_SEL is not left waiting to run the moment
			// the game is let go of (step_vs_over).
			b.fv2_hold_submode();
			break;
		case ask::stage_free:
			b.fv2_leave_stage_select();
			fprintf(stderr, "the console: the select screen let go of\n");
			break;
		}
		asked.clear();
	}
	bool start_was = false;       // the player's Start last frame: this works on the edge, not the level
	bool swallow_start = true;    // Start is held back until it has been seen released
	int inject_start = 0;         // frames of Start still to hand the game
	int inject_start_p2 = 0;      // the same for player 2 (VERSUS)

	// Survival is two bytes, and the order matters. Written during the attract loop the flag stops the game
	// starting at all -- the injected Start is ignored and no credit is ever taken -- so it is armed when it is
	// chosen, written only once a game is actually running, and then held every frame, because nothing says the
	// game does not clear it when a fight begins (the roster patch had to be held for the same reason). It is
	// put back when the game ends: left on, it blocks the next game, and ARCADE stops working.
	// (the three values this carried, and the two counters beside them, are the holds machine's now:
	// H_SURVIVAL, down by step_holds)

	// ---- RANDOM and VERSUS: both hang off the select screen's own timing ----
	// Which is why neither could be wired until the game's state machine was read: what was missing was not
	// the lever but knowing WHEN the select screen comes up and WHEN the fighter is chosen. Both are states:
	// the mode is SEL (4 or 5), and the choice is its sub-state reaching SEL_SUB_DECIDE, 4 (fv2-states.md,
	// fv2-match-flow.md).
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
	// (H_RANDOM in the holds machine)

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
	// does NOT command THIS: patched and held -- `li r3,3 ; blr` read back out of the running game -- the game
	// picked stage 9 all the same. What commands a game's FIRST arena is one byte of the game's own route
	// table (Model3::fv2_hold_training_stage), found by watching who writes the scene index and reading the
	// dozen instructions above that write.
	//
	// AND THE SENTENCE THAT USED TO FOLLOW WAS TOO WIDE, corrected the 2026-10-24: it said the picker "is
	// reached only by a rematch THROUGH the select screen". That is true of the first fight of a solo route,
	// which is what was measured. It is NOT true of a VERSUS, where the picker commands the arena outright --
	// held at scene 3 from frame 1990 to 2900 of tools/scripts/fv2-vic-bench.txt, both patched words read back
	// out of the dump, the versus fought in scene 3 where it fought in scene 8 untouched. That is what the
	// stage select below rests on, and it is why the route table is NOT used for it: held at 3, a versus
	// still fought in scene 8.
	//
	// AND THE REST OF IT, built the 2026-10-25 on the form August gave it (MenuConsole.md section 9
	// quinquies) and on this port's own reading of the game for the substance:
	//
	//   the entry      TRAINING opens a two-player versus, by the V.S. line's own road, word for word;
	//   the clock      stopped by the GAME'S OWN SWITCH -- bit 0 of CONFIG_CB_DISP, which FIGHT_DSP tests
	//                  before it decrements anything. August held the timer's value, which left open the
	//                  question his own notes called the trap of section 12: does the game read this word
	//                  or write it? A condition has no such question;
	//   the two lives  held, with a DELAY and a FLOOR, and both numbers are his and exist for his reasons.
	//                  His first version gave the life back every frame and the verdict was "it comes back
	//                  too fast": it never went DOWN, the hit was erased on the frame it landed, and the
	//                  mode was useless because its damage could not be read. So: the delay is how many
	//                  frames without new damage before the life fills, and the floor is how low it may go
	//                  -- without it, waiting would mean accepting the K.O. at the end of a long series,
	//                  which ends the round, which is exactly what a training must not do.
	//   leaving        the mode goes OUT the moment a way out of a match begins. His two functions, each
	//                  right on its own, held each other: the way out finishes the match by emptying the
	//                  lives, and the training filled them every frame, so the player was locked in.
	//
	// The dummy's behaviour is not here. Nothing stands in for it yet and nothing pretends to.
	static constexpr int MARBLE_ARENA = 3;
	// (H_TRAINING in the holds machine)
	bool training_on = false;            // the mode itself, as opposed to the arena it arms
	static constexpr int TRAIN_DELAY = 48;    // eight tenths of a second without new damage before the fill
	static constexpr int TRAIN_FLOOR = 25;    // per cent of the starting health, below which the life is held
	int train_was[2] = { -1, -1 };       // the life as we last left it: our own writing is never damage
	int train_quiet[2] = { 0, 0 };       // frames since this player last took a hit
	// What the host needs from this frame to draw the column: whether a round is being fought at all, and
	// the points each player has just had taken off. The host reads them and clears them.
	bool train_fighting = false;
	int train_took[2] = { 0, 0 };        // points taken off player 1 and player 2 on this frame
	// ONE PLAYER CHOOSES BOTH FIGHTERS. The mode opens a two-player versus, so the select screen comes up
	// with two cursors -- and there is one pair of hands. Player 1 picks his own, and from that press on his
	// controls are routed to player 2's cursor, so the same hands pick the opponent. The latch is the host's
	// own: it SEES the press, so it needs no byte of the game's to tell it.
	bool train_p1_chose = false;
	// AND THE PRESS THAT CHOSE HIM HAS TO BE LET GO FIRST. It is held for several frames; the frame after
	// the latch it was routed to player 2, who chose whoever his cursor stood on -- so one press chose
	// both, and what Frederic saw was his pad picking 2P first. The directions travel at once; the punch
	// waits until it has been released.
	bool train_punch_held = false;
	// AND THE PUNCH THAT OPENED THE SELECT SCREEN IS NOT A CHOICE. The press that chose TRAINING in the
	// console's own list is still down when the game arrives on SELECT several frames later, so the latch
	// above closed on it at once, with nobody's fighter chosen -- and from that frame on player 1's pad
	// was driving player 2's cursor. That is exactly what Frederic reported the 2026-10-25: "the pad in P1
	// picks P2 first". So the button has to be SEEN RELEASED on the select screen before any
	// press of it counts, which is what this says.
	bool train_pick_arm = false;
	// WHETHER THE CLOCK IS STILL STOPPED BY US. The freeze is a bit of the game's CONFIG block, held while
	// training runs; when the mode goes out -- and leaving a match turns it out -- nobody put it back, so
	// the clock stayed stopped for ever. That is why MAIN MENU (TITLE SCREEN as the line was called then)
	// did nothing in training: fv2_end_match
	// sets GAME_TIMER to 1 and waits for FIGHT_DSP to count it down, and FIGHT_DSP does not count at all
	// while that bit is set. The match could not end, so there was nothing to leave.
	bool train_froze = false;
	// THE ANNOUNCER IS NOT WANTED IN TRAINING. Cutting the seven states of the announcement took their
	// PICTURES away, not their sounds: the handlers still run their own frame each, and what they say to
	// the sound board is said. Measured on a hidden training run, round start at frame 2826:
	//   2826 ROUND_DSP      A0 00 01   A1 0B 03
	//   2828 SET_INT_POSE   A0 00 03   BC 50 00   BD 50 00   A0 2B 03
	//   2829 SET_DSP        A0 28 13
	//   2831 READY_DSP      A0 2B 0E
	//   2833 FIGHT_DSP      A0 10 0C   A1 0B 03   A0 2B 0F
	// A0 <bank> <index> is the shape the owner established by sending them and listening (main.cpp):
	// bank 0x10 is the music -- 0x0C is this fight's, and it must go through -- and 0x30 is the voice the
	// SEGA logo's skip already drops. The two lines heard before a fight are the two 0x2B indices that
	// differ with the state, 0x0E at READY and 0x0F at FIGHT. This counts the frames in which such a
	// command is dropped: from the announcement's first state to a little past the fight's first frame,
	// and never during the fight itself, where the same bank carries what the blows say.
	int train_hush = 0;
	// WHAT STAYS BEFORE A FIGHT IS THE LOAD, and only the load: the four ROUND_MASK sub-states. The
	// seven that follow them -- ROUND, SET, READY -- are the announcement, they load nothing, and
	// step_training cuts them, which is why a training fight now begins ten frames after the load ends
	// instead of three hundred. The paragraph below is why the four themselves cannot go.
	//
	// Asking GAME's sub-states 0 to 3 to move on -- which is all fv2_cut_submode does -- gave a BLACK
	// SCREEN with nothing on it but the HUD, three times over: all four cut, ROUND_MASK_DSP alone, and
	// ROUND_MASK_DSP at its forty-first frame, which is where this port's own note had cut it.
	//
	// So the four were READ, out of a RAM dump with tools/fv2/disasm_ppc.py, the handler table being at
	// 0x000243B4 (0x000244F4 is the table of NAMES, which is not the same thing):
	//
	//   ROUND_MASK_INT  0x0000BD2C  clears three fields, plays three sounds, sets the round's health
	//   ROUND_MASK_IN   0x0000BEAC  if ROUND_NO is 1 and this is neither a rematch nor Survival, calls
	//                               0x0005ECAC and 0x0005AF90 -- and that is the LOADING, done once a
	//                               match and not once a round
	//   ROUND_MASK_DSP  0x0000BF00  `lwz r3,[0x002750B8] ; cmpwi r3,0 ; bne <stay>` -- it does nothing
	//                               at all but WAIT for that counter to fall to zero, and only then
	//                               arms the next task and asks for the next sub-state
	//   ROUND_MASK_OUT  0x0000BF48  two calls, then on
	//
	// 0x002750B8 is work outstanding: read out of three dumps it holds 9, 2 and 0 at different moments.
	// So ROUND_MASK_DSP's length IS the load's length, and cutting it starts the fight on an arena that
	// has not arrived -- which is exactly the black screen. There is nothing to shorten here: between
	// rounds, where nothing is loaded, the game already passes through in no time.
	//
	// What could still be done is hide the BANNER that is drawn over the load. That does not make the
	// fight start any sooner; it replaces a screen that says what is coming with a blank one, and
	// Frederic liked the blank one less when it was tried the 2026-10-25.

	// ---- THE TRIP THROUGH THE SELECT SCREEN: one state, and it is read from the game's own ----
	//
	// Six flags did this: at_select with three values, two frame counters, seen_select, stage_page_done
	// and versus_trip, in two blocks of step() that each worked out where the game was for themselves --
	// and a comment on versus_trip explaining that at_select could not be used because it was cleared
	// thirty frames in. That is the overlapping Frederic named: two readings of one screen, neither of
	// them the game's.
	//
	// The game's own reading is three moments, and both jobs hang off them (docs/fv2-states.md):
	//
	//   AWAY   not on SEL at all (modes 4 and 5 are SEL_INT and SEL_DSP);
	//   SEEN   the screen is up and has been SEEN up -- sub-state 0 or 1, SEL_START or SELECT. That
	//          condition is not pedantry: THE SUB-STATE BYTE IS STALE WHEN A MODE BEGINS, and it cost a
	//          measurement. Entering SEL it still held what ADV had left there -- 5 -- so a test on
	//          "sub >= 4" fired at frame 1202, right after V.S. was chosen and three hundred frames
	//          before anyone picked a fighter;
	//   DONE   the fighters are chosen. AND IT IS THE REQUEST THAT SAYS SO, not the sub-state: SELECT
	//          writes DECIDE_INT into SUBMODE_REQUEST and Sega's dispatcher acts on it the NEXT tick
	//          (model3.cpp), so reading the request catches the game in the one tick where DECIDE_INT
	//          has been asked for and has not run. The page opens there and the game is parked in
	//          Sega's empty STAGE_SEL on the spot, overwriting the request before the game reads it:
	//          DECIDE_INT then runs exactly once, when the page lets go. Reading MODE_SUB, as this did
	//          first, caught it a tick late and ran DECIDE_INT twice. (sub >= 4 is the net under it.)
	//
	// And what the trip WANTS is two plain questions asked when the mode was chosen, instead of a mode
	// enum consulted later: bring player 2 in, and open the carousel once the fighters are picked.
	enum { SEL_AWAY, SEL_SEEN, SEL_DONE };
	uint8_t sel_at = SEL_AWAY;
	int sel_frames = 0;           // frames since the screen was seen
	bool sel_two_players = false; // this trip brings player 2 in (V.S. and TRAINING)
	bool sel_wants_page = false;  // this trip opens the console's carousel between the fighters and the fight
	// Thirty frames before player 2's Start, to clear the transition and the press that opened the screen.
	static constexpr int SEL_P2_AT = 30;

	void step_select(m3::Model3 &b, m3::Controls &out)
	{
		(void)out;
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
		sel_wants_page = false;   // (spent: a later game asks for it again by choosing V.S. again)
		// THE CAROUSEL IS ASKED FOR AS ANY STATE IS (console_state.h): the host enters it, and refuses it
		// if the page is not there -- in which case it lets the select screen go again, since the game is
		// parked on this very tick, which is the whole of the paragraph above.
		ask_state(cstate::STAGE, "the fighters are chosen");
		b.fv2_park_stage_select();
	}

	// WHETHER THIS GAME IS A VERSUS IS THE GAME'S TO ANSWER, and it does: Model3::fv2_players, the byte the
	// match's own branch point asks first, reads 1 for one player and 3 for two (measured on three
	// scripted roads the 2026-10-27). This layer kept two flags for that question -- one set when V.S.
	// was chosen, one to put off clearing it, with a paragraph explaining that the first could not be
	// cleared when the game reached its attract loop because the injected Start had not been read yet.
	// Both are gone: the pause menu asks the game.
	void choose_arcade() { inject_start = 8; swallow_start = true; }
	// (the flag is read HERE, before anything of this port has touched it, and handed to the hold)
	void choose_survival(m3::Model3 &b) { arm_hold(H_SURVIVAL, b.fv2_survival()); choose_arcade(); }
	void choose_random() { arm_hold(H_RANDOM); choose_arcade(); }
	// TRAINING is a two-player versus with the mode on top: the entry is the V.S. line's, word for word,
	// which was already proved. The arena is still held to Marble Arena by the route byte.
	void choose_training()
	{
		arm_hold(H_TRAINING);
		training_on = true;
		for (int i = 0; i < 2; i++) { train_was[i] = -1; train_quiet[i] = 0; train_took[i] = 0; }
		choose_versus(MARBLE_ARENA);
		// AND NO CAROUSEL. The stage page belongs to the V.S. line, where choosing the arena is the point;
		// training has its own and asking for it twice would be a page to dismiss before every session.
		sel_wants_page = false;
	}
	// ---- VERSUS's arena ----
	// The page (fv2_stage_list.h) hands the scene in, 0 meaning the game chooses as it does on the board.
	// The lever is the picker, HELD exactly as TRAINING holds its route byte and for the same reason -- the
	// game unpacks and reloads its own code, so the patch goes back whenever Sega's words come back -- and
	// GIVEN BACK when the game ends, or the next game would fight there too.
	int vs_scene = 0;   // (the holding itself is H_STAGE in the holds machine)

	// THE MOMENT THE CHARACTERS ARE CHOSEN, which is where the summer's ROM put its stage select and
	// where Sega left room for one. The six SEL sub-states are SEL_START, SELECT, STAGE_SEL_INT,
	// STAGE_SEL_DSP, DECIDE_INT, DECIDE_DSP, and the board steps SELECT (1) straight to DECIDE_INT (4) --
	// its two STAGE_SEL states are an empty stub. So the page opens on DECIDE_INT, the game frozen under
	// it, and the stage is held before the match reads it ten frames later at ROUND_MASK_INT.

	// THE TWO TASK REGISTERS AS THEY READ WHILE A FIGHT IS RUNNING, kept so the pause menu's REMATCH can
	// hand back whichever of them the game has stopped. The versus is player 2 stepping into player 1's
	// arcade game -- the game calls it INTRUDE -- and its rule is that the winner inherits the game, so at
	// the end of a match the loser's task falls to 0 and that pad goes dead. Read here and nowhere else,
	// because a value read after the match is over is the stopped one, which is exactly what must not be
	// handed back. 0 means "never seen fighting", and then fv2_rematch leaves that one alone.
	uint32_t task_live[2] = { 0, 0 };

	// ---- LEAVING A MATCH, in two phases, and the game does the work in both ----
	//
	// ENDING    one write, Model3::fv2_end_match: the game's own round timer put at 1, so FIGHT_DSP ends
	//           the round itself, and the bit NEXT_PROG_SEL branches on set, so that handler decides the
	//           match is over instead of opening another round. Then the judgement, the replay and the
	//           winner's pose are each cut short ONCE by asking for the next sub-state -- the game's own
	//           way of moving between them. Over when the attract loop comes back.
	// CLEARING  CONTINUE and GAME OVER are MODES of their own, so the sub-state request does not reach
	//           them and their own counter is the only lever. It is WATCHED rather than written once: the
	//           CONTINUE window is installed only after the defeat, so an early write lands before it
	//           exists and the game lays its own over it. Each time the counter climbs above our edge, a
	//           new screen has just given itself a duration. Over when the SCENE INDEX changes, which is
	//           what says the end of the game has gone past -- entry1 has read zero since the K.O. and
	//           does not tell the two apart.
	// Then the destination: the title screen, or the select screen for CHANGE YOUR VIPER.
	enum leave_phase { L_OFF, L_ENDING, L_CLEARING };
	enum leave_dest { L_TO_TITLE, L_TO_SELECT };
	leave_phase leaving = L_OFF;
	leave_dest leave_for = L_TO_TITLE;
	int leave_frames = 0, leave_cut = -1, leave_scene = -1;
	// whether GAME OVER has been asked for on this way out: once, and only once
	int leave_cut_over = -1;
	// VS FINISH as the player had it, lent for the length of the way out (model3.h). -1: not borrowed.
	int leave_vs_finish = -1;
	// AND WHETHER THE MATCH WAS ALREADY DECIDED when the way out began -- the end-of-match panel's road.
	// There is nothing to end there, and the branch point that would have ended it is out of reach: see
	// Model3::fv2_request_game_over. So that road asks for GAME OVER with the game's own write and goes
	// straight to the end-of-game screens.
	bool leave_decided = false;
	// whether the training is to be put back at the far side (see leave_match)
	bool leave_training = false;
	static constexpr int LEAVE_GIVE_UP = 3600;   // a minute: a match that will not end is let go of

	void leave_match(leave_dest what, bool decided = false)
	{
		// LEAVING A MATCH TURNS THE TRAINING OFF, and not merely suspends its holding: the way out finishes
		// the match, and a training that went on filling the lives every frame would lock the player in --
		// measured in August's fork, where the two held each other.
		//
		// EXCEPT WHERE THE WAY OUT LEADS BACK INTO A FIGHT, which is Frederic's of the 2026-10-28: CHANGE
		// YOUR VIPER in training was taking the training with it -- the clock ran, the lives fell, the
		// column was gone -- and a player who asked to change his fighter did not ask to leave the mode.
		// So that road REMEMBERS, and arrive() starts another training rather than a versus. MAIN MENU
		// still turns it off: the menu is where a mode is chosen, and choosing one is how it comes back.
		leave_training = training_on && what == L_TO_SELECT;
		training_on = false;
		// AND THE DOOR OUT OF A TWO-PLAYER MATCH IS OPENED FIRST, because the game only reads it when the
		// match ends. Read at 0x0000DBD0 the 2026-10-27: when both players are in, the end of a match asks
		// whether VS FINISH is on and, if it is, asks for mode 18 -- GAME OVER. If it is off, the winner is
		// handed the ladder and the game goes on, which is what a scripted versus did for four runs while
		// the way out waited. The setting is put back at the far side (arrive), and only the working copy is
		// touched, so what the player chose in OPTIONS is not changed.
		leaving = L_ENDING;
		leave_for = what;
		leave_decided = decided;
		leave_frames = 0;
		leave_cut = -1;
		leave_cut_over = -1;
		leave_scene = -1;
	}

	// Borrowed on the way in, given back at the far side. Both say so: a setting left on would change what
	// the next versus does at its end, which is exactly the surprise this layer is built not to spring.
	void borrow_vs_finish(m3::Model3 &b)
	{
		if (leave_vs_finish >= 0) return;
		leave_vs_finish = b.fv2_vs_finish();
		if (leave_vs_finish < 0) return;
		if (leave_vs_finish == 0) b.fv2_set_vs_finish(1);
		fprintf(stderr, "the console: VS FINISH was %d, lent as 1 for the way out\n", leave_vs_finish);
	}
	void return_vs_finish(m3::Model3 &b)
	{
		if (leave_vs_finish < 0) return;
		b.fv2_set_vs_finish(leave_vs_finish);
		fprintf(stderr, "the console: VS FINISH given back as %d\n", leave_vs_finish);
		leave_vs_finish = -1;
	}

	// The page answered: held at once, because by now the game has left its attract loop. And the game let
	// go of STAGE_SEL in the same breath -- it has been running all along, parked in Sega's empty sub-state,
	// so answering the page is only a matter of asking for DECIDE_INT, the one it would have gone to itself.
	void stage_chosen(m3::Model3 &b, int scene)
	{
		// NOTHING IS WRITTEN HERE ANY MORE: the page's answer is ASKED FOR, and both writes happen where
		// every write of this layer happens -- at the top of step(), on the frame boundary.
		(void)b;
		ask_for(ask::stage_free);
		vs_scene = scene >= 1 ? scene : 0;
		if (!vs_scene) {
			drop_hold(H_STAGE);
			fprintf(stderr, "the console's stage: nothing held, the game picks its own\n");
			return;
		}
		arm_hold(H_STAGE);
	}

	void choose_versus(int scene = 0)
	{
		// what this trip through the select screen wants of it, asked here and nowhere else
		sel_at = SEL_AWAY;
		sel_frames = 0;
		sel_two_players = true;
		sel_wants_page = true;
		vs_scene = scene >= 1 && scene <= 14 ? scene : 0;
		if (vs_scene) arm_hold(H_STAGE); else drop_hold(H_STAGE);
		choose_arcade();
	}

	// ---- what Start does, and it is two jobs told apart by one question ----
	// Is the game's own title screen on screen? Model3::fv2_on_title answers it: the mode is ADV and the
	// sub-state is FV2_TITLE_INT or FV2_TITLE_DSP, measured by freezing the game one sub-state at a time and
	// looking at the screen (docs/fv2-states.md). That is NOT the question fv2_in_attract above answers -- "is a
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
	// road -- finish the match off screen -- which is written down in docs/fv2-states.md and is not wired here.
	// cut: the sub-state the game is in was ended early instead of the menu opening, which is what the
	// original does with this button during the fight's presentation and during the replay (take_start)
	enum class start_for { none, list, title, pause, cut };

	// THE MENU IS THE FIGHT'S AND NOTHING ELSE'S. Frederic, the 2026-10-24: "the menu must only be
	// reachable during the fight phase", and everywhere else Start is the game's own
	// button again. So of the twenty GAME sub-states Sega named (docs/fv2-states.md, 0x000244F4):
	// (docs/fv2-states.md, the twenty GAME sub-states at 0x000244F4):
	//
	//   0 ROUND_MASK_INT  1 ROUND_MASK_IN  2 ROUND_MASK_DSP  3 ROUND_MASK_OUT   the STAGE n / VS name screen
	//   4 ROUND_INT       5 ROUND_DSP                                           the round's own announcement
	//   6 SET_INT         7 SET_INT_POSE   8 SET_DSP                            the stage and the fighters placed
	//   9 READY_INT      10 READY_DSP                                           "ready"
	//                                                                          -> the port cuts the state
	//  11 FIGHT_INT      12 FIGHT_DSP                 THE FIGHT                -> the pause menu, here only
	//  13 JUDGE          14 JUDGE_DSP   15 REPLAY_INT  16 REPLAY_DSP
	//  17 VIC_INT        18 VIC_DSP     19 NEXT_PROG_SEL                        -> the game's own button
	//
	// WHY THE STATES AFTER THE FIGHT GET THE GAME'S BUTTON AND NOT A LEVER OF MINE. I tried cutting the
	// replay the way the presentation is cut, and it ends
	// the state all right -- Frederic's log shows sub-state 16 cut at frame 3561 -- but ROUND 2 THEN FROZE in
	// SET_INT_POSE, a state that lasts one frame of its own accord. Measured from the same log: his three
	// presses at frames 4228, 4256 and 4270 all report sub-state 7, so the game was already stuck there before
	// the first of them and those presses are him trying to shake it loose, not the cause. Asking for the next
	// sub-state is not how the game ends its replay -- it leaves something of the replay running, and the next
	// round's setup waits for it. And my reason for reaching for a lever at all was wrong: I had measured
	// that a press reaching the game does nothing during ROUND_MASK_DSP and took that for everywhere. The
	// replay is a different handler, and it was never measured. So nothing of this port touches these
	// states: the press goes to the game and Sega's handler does what it does.
	//
	// The presentation keeps the cut because there the game really does ignore the button -- measured, a real
	// Start at image 2360 of tools/scripts/fv2-start-presentation.txt leaves the state trace identical -- and
	// because Frederic asked for it and tried it: sub-states 8 and 10 cut, the fight started normally.
	static bool cut_states(const m3::Model3 &b)
	{
		if (b.fv2_mode() != 6 && b.fv2_mode() != 7) return false;   // GAME_INT, GAME_DSP
		const int s = b.fv2_submode();
		return s >= 0 && s <= 10;   // the presentation: the game ignores the button here, so we act
	}

	// The fight, and the fight alone: FIGHT_INT and FIGHT_DSP.
	static bool fight_states(const m3::Model3 &b)
	{
		if (b.fv2_mode() != 6 && b.fv2_mode() != 7) return false;   // GAME_INT, GAME_DSP
		const int s = b.fv2_submode();
		return s == 11 || s == 12;
	}

	start_for take_start(m3::Model3 &b, bool pressed, bool list_open)
	{
		// While the list is up it is the list's button, not the game's: withheld whatever it does there.
		if (list_open) { start_was = pressed; swallow_start = true; return start_for::none; }
		start_for what = start_for::none;
		if (swallow_start) { if (!pressed) swallow_start = false; }   // (held back until it has been let go)
		else if (pressed && !start_was) {
			// A GAME IS RUNNING: THE BUTTON IS THE GAME'S, and this layer keeps its hands off it.
			//
			// Asking for the title screen from a select screen or a fight was always known to leave the game
			// running underneath -- the comment below says so -- but what that looks like was never measured.
			// Frederic found it in versus: Start during a fight, and the title screen comes back SHREDDED,
			// its tile layers torn across each other, the roster names printed through the logo, the whole
			// picture in stripes. Measured here the same way, frame 3001 of a versus fight.
			//
			// There is no good screen to ask the GAME for from inside a game, so nothing is asked of it. What
			// opens instead is this port's own pause menu, over the fight, with the game frozen behind it --
			// which is what a console does with that button and what August's work had (fv2_pause.h). The
			// press is withheld, as every press this layer acts on is.
			//
			// EXCEPT IN THE FIGHT'S PRESENTATION AND THE REPLAY, where Frederic asks for the original's
			// behaviour: not the menu, but the state cut short (cut_states above).
			// WHERE THIS LAYER MAY TAKE THE BUTTON AT ALL, and it is a MODE, not a task.
			//
			// This used to ask fv2_in_attract -- whether the entry1 task is stopped -- and that is NOT the same
			// question. On CONTINUE, on the rankings, on GAME OVER and on the result screen THE TASK IS ALREADY
			// STOPPED while the game goes on drawing screens of its own. So a press there was read as "bring the
			// title screen back", and the attract sequencer was restarted from the middle of a screen the game
			// had not finished: the title came back SHREDDED -- the tile layers torn across each other, the
			// roster's names printed through the logo, the whole picture in stripes.
			//
			// Frederic photographed it the 2026-10-28 and found it twice in one sitting: Start during the LAST
			// round's replay, where the match is already decided so the task is already down, and Start on the
			// CONTINUE screen -- where that button is the one that CONTINUES THE GAME, which is why he reported
			// it as "no continue". Both are the same mistake and both gave the same torn title.
			//
			// Sega's own answer is the mode: ADV, 2 and 3, IS the attract loop and nothing else is
			// (docs/fv2-states.md). Everywhere else the game is showing something of its own and the button is
			// its own, exactly as it already was for the judgement, the replay and the select screen.
			const int now_mode = b.fv2_mode();
			if (now_mode != 2 && now_mode != 3) {
				// In the fight's presentation and in the replay the press CUTS the state instead of opening the
				// menu. Measured first: letting the press through to the game does nothing at all -- a real Start
				// at image 2360 of the bench road left ROUND_MASK_DSP running to 2450 exactly as without it, so
				// the game does not read this button there. What does work is the game's own road between its
				// handlers, the sub-state request: fv2_cut_submode, and the state ends on the next frame.
				start_was = pressed;
				if (fight_states(b)) { swallow_start = true; return start_for::pause; }
				if (cut_states(b)) { swallow_start = true; b.fv2_cut_submode(); return start_for::cut; }
				// everywhere else out of the fight -- the judgement, the replay, the victory, the select
				// screen: nothing withheld, nothing done, the game has its button back
				return start_for::none;
			}
			what = b.fv2_on_title() ? start_for::list : start_for::title;
			// Two roads to the title screen, and they are not interchangeable. From the attract loop, the
			// game's own: the sequencer is pushed to the end of its list and walks off it, which installs the
			// title screen the way the game does on its own -- and, measured with the picture probe, leaves
			// no 3D behind. The mode request shows the title screen just as fast but does not stop what was
			// drawing: 397 models still on a screen the game draws with none, and the demonstrations that
			// follow come out with fighters missing or blackened. Elsewhere -- the select screen, a fight --
			// the sequencer is not running, so the request is all there is, and it does not end the game
			// either (docs/fv2-states.md).
			if (what == start_for::title) {
				// From the attract loop, the game's own way out of whatever is drawing: ask the loop to start
				// over. That is what the replay itself does when it ends (0x0000B7C0 writes SUBMODE_REQUEST =
				// 0), and the restart is where the game clears the tasks that draw, two sub-states later. Then
				// the title screen is asked for as soon as that housekeeping is done -- the SEGA logo -- so
				// the player does not have to sit through it.
				// (ADV and nothing else by now, so there IS a sequencer with a list to walk off: the mode
				// request, which does not stop what was drawing, is no longer reached from here at all)
				b.fv2_restart_attract();
				skip_logo = SKIP_GIVE_UP;
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
	// THE ENDING IS ASKED FOR STRAIGHT AWAY, and the wait that used to sit here is withdrawn.
	//
	// It was put in on a measurement that did not hold. Poking MODE_REQUEST a frame at a time and
	// photographing ALWAYS AT FRAME 1900, the roll looked broken when asked at 1201-1210 and clean from 1212,
	// so twenty frames seemed to be what the title screen needed to finish installing. It is not: THE ENDING
	// ALTERNATES, about 225 frames of credits over the viper and then about 225 of a 3D passage, so moving the
	// request moves which of the two frame 1900 falls in. Photographed at the SAME OFFSET from ENDING_INT
	// instead -- +300 and +650 -- a request at 1201 and one at 1250 give the same two pictures, byte for byte
	// the same sizes. The request frame makes no difference at all.
	//
	// What the pictures really say is in the next comment, and it is not about timing.
	void ask_ending(m3::Model3 &b) { b.fv2_request_ending(); }

	// And asking for the REAL one: the game's own ending at the end of the match being played, with its
	// replays. Armed here, laid in step() at the first frame the game says the match is over -- which is the
	// moment measured in model3.cpp, and the reason this is not simply written on the spot.
	bool want_real_ending = false;
	// ---- THE DREAMCAST'S ENDING: which of its pictures belongs on screen right now ----
	//
	// The roll is a list of segments, each one a page of credits then a replay (model3.cpp). The picture
	// stands in for the replay. ending_art is what the host should show, -1 for nothing, and it is set here
	// because this is where the game's state is already being read every frame.
	//
	// August's two rules, both of which he paid for:
	//   * only ENDING_DSP is listened to. Through ENDING_INT the sequence pointer runs down its list a
	//     value a frame, so a boundary read there is twelve boundaries, and no threshold is needed to tell
	//     the installing apart from the roll.
	//   * the picture goes up on the replay byte's RISE and comes down at the SEGMENT'S BOUNDARY, never on
	//     the byte's fall: the byte drops two frames before the segment ends and the game is still showing
	//     its replay in those two, so taking the picture away on the fall let a flash of replay through.
	int ending_art = -1;
	int ending_seg = 0;          // which segment: 1 is the first
	int ending_ptr = -1;         // the sequence pointer as it last read
	int ending_replay = 0;       // the replay byte as it last read
	void step_ending(m3::Model3 &b)
	{
		const int mode = b.fv2_mode();
		const bool dsp = mode == 13;      // ENDING_DSP; ENDING_INT is 12 and is followed without counting
		if (mode != 12 && !dsp) { ending_art = -1; ending_seg = 0; ending_ptr = -1; ending_replay = 0; return; }
		const int ptr = b.fv2_ending_seq(), rep = b.fv2_ending_replay();
		if (!dsp) { ending_ptr = ptr; ending_replay = rep; return; }
		if (ptr != ending_ptr) {          // a boundary: the picture comes down here and the segment advances
			ending_ptr = ptr;
			ending_seg++;
			ending_art = -1;
		}
		if (rep != ending_replay) {
			ending_replay = rep;
			if (rep && ending_seg > 0) ending_art = ending_seg - 1;   // the replay has begun: its picture stands in
		}
	}
	void arm_real_ending() { want_real_ending = true; }

	// Once a frame, before the game runs. Returns what to hand the game on top of the player's own controls.
	// One frame of TRAINING. The game is a two-player versus and never knows it is anything else: nothing
	// is diverted, no state is invented, and turning the mode off gives an ordinary fight back on the next
	// frame.
	// ---- TRAINING: ONE PLAYER CHOOSES BOTH FIGHTERS ----
	//
	// On the select screen player 1's controls go to his own cursor until he has pressed punch, and to
	// PLAYER 2'S from then on, so the same hands pick the opponent. Nothing of the game is touched: the
	// controls are composed in the host's frame loop and this is handed them there, after the column has
	// read them (main.cpp says why that order is not free).
	//
	// Two things had to be learnt by getting them wrong, and both are latches here rather than rules in
	// the host's loop, which is where they were:
	//
	//   the button has to be SEEN RELEASED first. The press that chose TRAINING in the console's list is
	//   still down when the select screen arrives several frames later, and taken for a choice it made
	//   player 1's pad pick player 2's fighter first -- "the pad in P1 picks P2 first",
	//   the 2026-10-25. It reaches the game no more than it counts, or it would validate player 1's
	//   fighter on the frame the screen appeared, wherever the cursor happened to stand;
	//
	//   and the press that DID choose has to be let go before it is routed on, or one press chooses
	//   both: the directions travel at once, the punch waits for its release.
	void route_training(m3::Model3 &b, m3::Controls &live, m3::Controls &script)
	{
		const int m = b.fv2_mode();
		if (!training_on || (m != 4 && m != 5)) return;   // the select screen's job, and no other screen's
		const bool punch = live.punch[0] || script.punch[0];
		auto move = [](bool *a) { a[1] = a[1] || a[0]; a[0] = false; };
		if (!train_pick_arm) {
			if (!punch) train_pick_arm = true;
			else { live.punch[0] = false; script.punch[0] = false; }
		} else if (!train_p1_chose) {
			// (this press is his own choice: it stays his, and the routing begins after it)
			if (punch) { train_p1_chose = true; train_punch_held = true; }
		} else {
			move(live.up); move(live.down); move(live.left); move(live.right);
			move(live.kick); move(live.guard);
			move(script.up); move(script.down); move(script.left); move(script.right);
			move(script.kick); move(script.guard);
			if (train_punch_held && !punch) train_punch_held = false;
			if (train_punch_held) { live.punch[0] = false; script.punch[0] = false; }
			else { move(live.punch); move(script.punch); }
		}
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
		// THE PHASES BEFORE THE FIGHT, CUT -- and these are not the load. GAME's twenty sub-states
		// (docs/fv2-states.md) are ROUND_MASK_INT, _IN, _DSP, _OUT (0..3), then ROUND_INT, ROUND_DSP,
		// SET_INT, SET_INT_POSE, SET_DSP, READY_INT, READY_DSP (4..10), then FIGHT_INT and FIGHT_DSP.
		// The first four are the arena arriving, and cutting them gave the black screen written down
		// above; the seven that follow are the announcement -- ROUND X, the pose, READY -- and nothing
		// is loaded in them. Each is asked to move on the frame it begins, so every INT still runs its
		// own frame and only the waiting goes. Training only: an ordinary fight keeps its announcement.
		if ((m == 6 || m == 7) && su >= 4 && su <= 10) b.fv2_cut_submode();
		// and the window the announcer is dropped in, opened by those same states (above)
		if ((m == 6 || m == 7) && su >= 4 && su <= 10) train_hush = 12;
		else if (train_hush > 0) train_hush--;
		const bool fighting = (m == 6 || m == 7) && (su == 11 || su == 12);
		train_fighting = fighting;
		// The clock, held by the game's own switch. Held rather than written once because that CONFIG block
		// is a working copy the game loads from its backup.
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
			// DAMAGE IS WHAT THE GAME TOOK OFF SINCE WE LAST LEFT IT, never what we put there ourselves:
			// every write below sets train_was to what it wrote, so the next frame compares against ours.
			const int hit = train_was[k] - now;
			if (hit > 0) {
				train_took[k] += hit;
				train_quiet[k] = 0;
			} else if (++train_quiet[k] > 60 * 60) {
				train_quiet[k] = 60 * 60;
			}
			int want = now;
			if (now < floor) want = floor;                      // never low enough to be knocked out
			else if (train_quiet[k] >= TRAIN_DELAY) want = max;  // quiet long enough: the life comes back
			if (want != now) b.fv2_set_life(who, want);
			train_was[k] = want;
		}
	}

	// One frame of the way out of a match. Nothing here counts rounds: the attract loop is what says the
	// match is over, and the scene index is what says the end-of-game screens are past.
	void step_leaving(m3::Model3 &b)
	{
		if (leaving == L_OFF) return;
		borrow_vs_finish(b);
		if (++leave_frames >= LEAVE_GIVE_UP) {
			fprintf(stderr, "the console: the match would not end in a minute -- the writing stops\n");
			return_vs_finish(b);
			// AND IT DOES NOT LEAVE THE PORT IN MID-AIR. Giving up used to clear `leaving` and nothing else,
			// so a MENU_IN that was waiting for the way home waited for ever: the list over a running game,
			// no line of it working, the music the game's. Whatever became of the match, the destination is
			// reached.
			leaving = L_OFF;
			arrive(b);
			return;
		}
		if (leaving == L_ENDING) {
			// A WAY OUT THAT IS TAKING LONG SAYS SO, and says what the game is doing meanwhile. Without this
			// a match that will not end is a minute of silence and then one line saying it gave up.
			if (leave_frames % 60 == 0 || (getenv("VF3_LEAVE_TRACE") && leave_frames < 60))
				fprintf(stderr, "the console: the way out is %d frames old -- mode %d sub %d, the match says %s"
					" (players %d, out %d, 145C %d, 145F %d)\n",
					leave_frames, b.fv2_mode(), b.fv2_submode(), b.fv2_match_over() ? "decided" : "not decided",
					b.fv2_option(0x0058145A, 1), b.fv2_option(0x00581457, 1),
					b.fv2_option(0x0058145C, 1), b.fv2_option(0x0058145F, 1));
			// THE MATCH IS OVER WHEN THE GAME SAYS SO, and it says it in TWO ways. The attract loop coming
			// back is one. The other is a mode of its own: CONTINUE (8, 9), RANKING (16, 17), OVER (18, 19),
			// RESULT (22, 23) -- the end-of-game screens, which the game walks to with its tasks still
			// running, so fv2_in_attract goes on saying a game is on. Measured the 2026-10-28 on a SURVIVAL
			// exit, which is where it bites: MAIN MENU left the game sitting in mode 23, its score screen,
			// and the way out waited there until it gave up. Survival's own byte sends it to RESULT where an
			// arcade game goes to CONTINUE ([0x0058145F] reads 1 there and 0 in arcade), so the two roads
			// part at exactly the place this was blind to.
			const int m = b.fv2_mode();
			const bool end_screen = m == 8 || m == 9 || m == 16 || m == 17 || m == 18 || m == 19 || m == 22 || m == 23;
			if (b.fv2_in_attract() || end_screen) {
				fprintf(stderr, "the console: the match is over in %d frames (%.1f s), the end screens next\n",
						leave_frames, double(leave_frames) / 57.5);
				leave_scene = b.fv2_scene_index();
				leave_frames = 0;
				leaving = L_CLEARING;
				return;
			}
			// AND A MATCH THAT WAS ALREADY DECIDED HAS NOTHING TO END. This is the end-of-match panel's road:
			// the game is standing at VIC_DSP with the match settled, and the branch point that would send it
			// to GAME OVER no longer sees a two-player game to send (Model3::fv2_request_game_over says why,
			// and the measurement that showed it). So the port writes what that road writes, once, and the
			// test just above catches mode 18 on the next tick.
			if (leave_decided) {
				if (leave_cut < 0) {
					leave_cut = 18;   // (spent: the one write this road makes)
					b.fv2_request_game_over();
					fprintf(stderr, "the console: the match was already decided -- GAME OVER asked for,"
						" the game's own way out of a settled versus\n");
				}
				return;
			}
			// THE JUDGEMENT, THE REPLAY AND THE WINNER'S POSE, EACH CUT SHORT -- once each, by asking for the
			// next sub-state, which is exactly what the game's own handlers do between themselves. Sub 13 is
			// JUDGE and 18 is VIC_DSP; NEXT_PROG_SEL (19) is LEFT ALONE, because it is the branch point
			// fv2_end_match wrote for, and cutting it would be cutting the decision itself.
			// THE MATCH IS HELD DECIDED while the end of it runs. fv2_end_match wrote the bit once; between
			// the round's end and the branch point the game writes that word itself, and a hidden run of the
			// 2026-10-25 caught it fighting five more rounds for exactly that reason (model3.cpp).
			if ((b.fv2_mode() == 6 || b.fv2_mode() == 7)) b.fv2_hold_match_over();
			const int su = b.fv2_submode();
			// AND THE END IS ASKED FOR, NOT HOPED FOR, which is Frederic's of the 2026-10-29: from ARCADE the
			// main menu came up with not one working line, the music went on, and backing out dropped him into
			// the game IN A DIFFERENT STAGE.
			//
						// All three are one thing, and the different stage names it: THE LADDER WENT ON. And it is not a
			// question of who was winning -- Frederic said so and his own log proves it. This way out ends the
			// round by putting the game's own clock at 1, so the round ends on the CLOCK and nobody is knocked
			// out: the players-out byte stays 0. NEXT_PROG_SEL then asks whether the players out are one fewer
			// than the players playing -- 1 == 0 + 1 in a one-player game, always -- and hands the last one
			// standing the next opponent (0x0000DB8C). In ARCADE that road is taken EVERY TIME.
			//
			// The test above waits for the attract loop or an end screen; neither ever comes; the state stays
			// MENU_IN, where the list is drawn over a game that is still running. Every line of it is held
			// until the way out is over, the music is the game's because MENU was never reached, and B gives
			// the game back -- on the ladder's next stage. Read off his log of the 2026-10-29: sub 2 at sixty
			// frames, sub 8 at a hundred and twenty, sub 12 from four hundred and eighty, for a thousand frames.
			//
// So once the game says the match is DECIDED, the port writes what the game's own road writes when
			// it ends a game: MODE_REQUEST = 18, GAME OVER (Model3::fv2_request_game_over). It is asked for
			// after the judgement has begun, so the round's own end is not cut out from under it, and once.
			// THE MOMENT: the fight itself is over. Not `su >= 13` -- Frederic's own log shows the game at
			// ROUND_MASK sixty frames after the press, so the judgement had come and gone and a NEW ROUND
			// was loading. Anything but FIGHT_INT and FIGHT_DSP, then, which is every way the round can end.
			if (leave_cut_over < 0 && b.fv2_match_over() && (su < 11 || su > 12)) {
				leave_cut_over = 1;
				b.fv2_request_game_over();
				fprintf(stderr, "the console: the match is decided -- GAME OVER asked for, the game's own\n"
					"  way out (the ladder would have gone on otherwise)\n");
			}
			if (su >= 13 && su <= 18 && su != leave_cut) {
				leave_cut = su;
				b.fv2_cut_submode();
			}
			// AND THE END OF THE GAME IS WALKED THROUGH, NOT JUMPED OVER, and that is a correction of this
			// same evening. Asking for the title from CONTINUE, RANKING, OVER or RESULT -- modes of their own,
			// docs/fv2-states.md -- does take those screens away, and it takes their housekeeping with them: the
			// title came up GLITCHED, which Frederic saw at once. Those modes are left to run and are made
			// SHORT instead, by the game's own lever, below.
			return;
		}
		// L_CLEARING: the end-of-game screens. CONTINUE and then GAME OVER are modes of their own, and the
		// only lever there is their screen timer -- asking for the title from inside them takes their
		// housekeeping with them and the title comes up GLITCHED, which Frederic saw the 2026-10-25. So
		// they are walked through and made SHORT, and what hides them is the port's own screen: the list's
		// ground where this leads to the menu, the Dreamcast's loading picture where it leads back into the
		// game (console_state.h, MENU_IN and GOING). Nothing of them is seen or heard either way.
		if (leave_for == L_TO_TITLE && b.fv2_on_title()) {
			fprintf(stderr, "the console: the title screen reached in %d frames\n", leave_frames);
			leaving = L_OFF;
			arrive(b);
			return;
		}
		// Firmly positive, because an already expired screen is negative and shortening it would
		// lengthen it; four times our own edge, so our own writing is never written over again.
		// and every frame it is still positive, rather than once: each of those screens arms its own timer
		// as it begins, so one write only shortens the first of them.
		if (b.fv2_screen_timer() > 4 * 8) b.fv2_expire_screen();
		if (b.fv2_scene_index() != leave_scene) {
			fprintf(stderr, "the console: the end of the game is past in %d frames (%.1f s)\n",
					leave_frames, double(leave_frames) / 57.5);
			leaving = L_OFF;
			arrive(b);
			return;
		}
	}

	// Where the way out leads. The title screen is the game's own lever, and it WORKS here where it does
	// nothing from a fight: by now the attract loop is running again, so there IS a list to walk off.
	// CHANGE YOUR VIPER hands the game a Start instead, which from the attract loop is a new game -- and the
	// select screen with it, two players on it, which is the whole point of the line.
	void arrive(m3::Model3 &b)
	{
		return_vs_finish(b);
		if (leave_for == L_TO_TITLE) {
			if (!b.fv2_on_title()) b.fv2_attract_to_title();
			// THE LIST IS ALREADY UP: MAIN MENU put it there on the frame it was chosen, and MENU_IN is the
			// half this walk home has been happening behind. Now that the game is where it was going, the
			// other half is asked for (console_state.h).
			if (ui.at == cstate::MENU_IN) ask_state(cstate::MENU, "the game is home");
			fprintf(stderr, "the console: back at the title screen\n");
		} else {
			// the mode comes back with the match, where the match is what was being changed (leave_match)
			if (leave_training) { leave_training = false; choose_training(); }
			else choose_versus();
			// and the curtain comes down: the select screen is the game's own again
			if (ui.at == cstate::GOING) ask_state(cstate::PLAY, "the select screen is reached");
			fprintf(stderr, "the console: back to the select screen, %s\n",
				training_on ? "another training" : "two players");
		}
	}

	// ---- WHAT THIS PORT HOLDS IN THE GAME WHILE A MODE RUNS: four things, ONE machine ----
	//
	// Survival's flag, RANDOM_CHARA, the training's mark and the arena the draw must pick were four
	// little state machines written one after another, each with its own three-valued enum, its own
	// counter, its own copy of the same give-up, and its own switch at the end of step(). They are the
	// same shape, and it is the shape the measurements imposed, not a choice:
	//
	//   ARMED   chosen, and nothing written yet. Survival's flag written during the attract loop stops
	//           the game starting at all -- measured -- so nothing may be written until a game is
	//           actually running, and the wait is given up on after HOLD_GIVE_UP frames rather than
	//           guessed at;
	//   LIVE    written. Two of the four have to be put back EVERY frame, because the game reloads what
	//           they sit in: the roster patch's discipline (Survival's flag, and the draw's table, which
	//           lives in code the game unpacks again). The other two are written once;
	//   and back to OFF when the game ends, with what was there before put back -- left on, Survival's
	//           flag blocks the next game and ARCADE stops working.
	//
	// So: one table, one machine, and each of the four says only what it writes.
	enum { H_SURVIVAL, H_RANDOM, H_TRAINING, H_STAGE, H_COUNT };
	enum { H_OFF, H_ARMED, H_LIVE };
	static const char *hold_name(int h)
	{
		static const char *const N[H_COUNT] = { "Survival", "RANDOM", "TRAINING", "the arena" };
		return h >= 0 && h < H_COUNT ? N[h] : "?";
	}
	// which of them the game takes back if they are not put back: measured, not assumed (see above)
	static bool hold_renews(int h) { return h == H_SURVIVAL || h == H_STAGE; }
	static constexpr int HOLD_GIVE_UP = 300;
	uint8_t hold_at[H_COUNT] = {};
	int hold_wait[H_COUNT] = {};
	int hold_was[H_COUNT] = {};     // what the game had there before this port touched it
	int stage_frames = 0, stage_repatch = 0;

	void arm_hold(int h, int was = 0) { hold_at[h] = H_ARMED; hold_wait[h] = 0; hold_was[h] = was; }
	void drop_hold(int h) { hold_at[h] = H_OFF; }

	// What each one writes when a game begins, and nothing else belongs here.
	void hold_begin(m3::Model3 &b, int h)
	{
		switch (h) {
		case H_SURVIVAL: b.fv2_set_survival(true); break;
		case H_RANDOM:
			// read HERE and not when the line was chosen: it is the running game's value that has to go back
			hold_was[H_RANDOM] = b.fv2_random_chara();
			b.fv2_set_random_chara(true);
			break;
		case H_TRAINING: break;   // the mode writes nothing of its own: its holding is step_training's
		case H_STAGE: {
			// IT SAYS WHAT IT DID. A patch that was not applied leaves a picture that looks perfectly
			// normal -- the game simply picks its own arena -- so a silent hold reads like a working one.
			const int said = b.fv2_hold_stage(vs_scene);
			stage_frames = stage_repatch = 0;
			fprintf(stderr, "the console's stage: holding scene %d, the picker says %d (1 patched, 0 nothing to do)\n",
				vs_scene, said);
			break;
		}
		}
	}

	// What each one puts back when the game ends.
	void hold_end(m3::Model3 &b, int h)
	{
		switch (h) {
		case H_SURVIVAL: b.fv2_set_survival(hold_was[H_SURVIVAL] != 0); break;
		case H_RANDOM: b.fv2_set_random_chara(hold_was[H_RANDOM] > 0); break;
		case H_TRAINING:
			// THE MODE GOES OUT WITH THE GAME. Leaving a match already turned it off (leave_match); a
			// training that ends by itself used to leave it on, and the next game would have been one too.
			training_on = false;
			break;
		case H_STAGE:
			// Sega's own words back, and the native code made again: the native code was made from OUR two
			// words, so putting Sega's back is a mismatch in its turn.
			b.fv2_release_stage();
			break;
		}
	}

	// What each one has to put back every frame, for those that need it.
	void hold_renew(m3::Model3 &b, int h)
	{
		if (h == H_SURVIVAL) b.fv2_set_survival(true);
		else if (h == H_STAGE) {
			// (measured: how often the patch really has to be laid again)
			if (b.fv2_hold_stage(vs_scene) > 0) ++stage_repatch;
			if (++stage_frames % 300 == 0)
				fprintf(stderr, "the console's stage: %d frames held, %d of them re-patched\n", stage_frames, stage_repatch);
		}
	}

	void step_holds(m3::Model3 &b)
	{
		const bool running = !b.fv2_in_attract();
		for (int h = 0; h < H_COUNT; h++) switch (hold_at[h]) {
		case H_ARMED:
			if (running) {
				// IT SAYS THAT IT TOOK. A hold that never went in looks exactly like a game played without it,
				// which is how Survival's first version went unnoticed for a day.
				hold_at[h] = H_LIVE;
				hold_begin(b, h);
				fprintf(stderr, "the console: %s is held, the game having started\n", hold_name(h));
			}
			else if (++hold_wait[h] > HOLD_GIVE_UP) {
				hold_at[h] = H_OFF;
				fprintf(stderr, "the console: %s was armed and no game started in %d frames -- let go of\n",
					hold_name(h), HOLD_GIVE_UP);
			}
			break;
		case H_LIVE:
			if (!running) {
				hold_end(b, h);
				hold_at[h] = H_OFF;
				fprintf(stderr, "the console: %s put back, the game being over\n", hold_name(h));
			}
			else if (hold_renews(h)) hold_renew(b, h);
			break;
		default: break;
		}
	}

	// The two task registers, read ONLY while the round is actually being fought (GAME's FIGHT_INT and
	// FIGHT_DSP, sub-states 11 and 12): see task_live above for why no other moment will do.
	void note_tasks(m3::Model3 &b)
	{
		const int m = b.fv2_mode(), su = b.fv2_submode();
		if (m != 6 && m != 7) return;
		if (su != 11 && su != 12) return;
		if (const int t1 = b.fv2_task_state(); t1 > 0) task_live[0] = uint32_t(t1);
		if (const int t2 = b.fv2_task2_state(); t2 > 0) task_live[1] = uint32_t(t2);
	}

	// The road that skips the SEGA logo, once the scene the attract loop was drawing is tidy.
	void step_logo(m3::Model3 &b)
	{
		if (skip_logo <= 0) return;
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

	// The game's own ending, once the game says the MATCH is over. Earlier than that and the next round's
	// branch point takes it with nothing recorded to replay; later is too late, the branch has read the
	// bit already. Written once and not held, as every request here is.
	void step_real_ending(m3::Model3 &b)
	{
		if (!want_real_ending || !b.fv2_match_over()) return;
		b.fv2_finish_ladder();
		want_real_ending = false;
		fprintf(stderr, "the console ENDING: the match is over -- the game's own ending asked for\n");
	}

	// ---- THE END OF A TWO-PLAYER MATCH IS A STATE OF ITS OWN ----
	//
	// Frederic, the 2026-10-28: "VERSUS MODE: at the end of the fight, in place of the GAME OVER sub-state,
	// make a sub-state of versus mode's own offering REMATCH, CHANGE VIPERS, CHANGE STAGE, MAIN MENU."
	//
	// WHEN, and it is the game's own answer rather than a count of rounds kept here. Measured the 2026-10-28
	// on tools/scripts/fv2-vs-end.txt -- a versus fought to its end with VF3_VS_TRACE=1 -- where the end of
	// every round reads the same four states and then parts in two:
	//
	//   a round that settles nothing   13 JUDGE, 14 JUDGE_DSP, 15 REPLAY_INT, 16 REPLAY_DSP, then 17 VIC_INT
	//                                  ASKS FOR 19 -- VIC_DSP skipped -- and 19 NEXT_PROG_SEL asks for 6:
	//                                  another round;
	//   the round that decides it      the same four, then 17 VIC_INT ASKS FOR 18, and 18 VIC_DSP runs.
	//
	// So VIC_DSP, sub-state 18, happens ONLY at the end of a decided match. It is not a threshold and not a
	// count of this port's own: it is a state Sega named and reaches nowhere else, which is what makes it the
	// door.
	//
	// AND THE PANEL GOES UP AT ITS END, not at its beginning: the winner's pose is played out whole, and the
	// moment caught is the one where VIC_DSP has ASKED for NEXT_PROG_SEL and the dispatcher has not acted yet
	// -- the same tick the carousel is opened on (step_select). One tick later and NEXT_PROG_SEL has already
	// decided: GAME OVER if the board's VS FINISH is on, the winner handed the ladder if it is off. Measured
	// in that same run, VS FINISH being off there -- 19 ran, the players byte fell from 3 to 1, and the game
	// carried the winner into an arcade ladder. Neither road is taken now.
	//
	// AND ONLY FOR TWO PLAYERS, which the game says as well (fv2_players reads 3): the arcade ladder's own
	// matches end at VIC_DSP too, and there the end of a match is the ladder's business and not the player's.
	// Training is a two-player versus on this board and is left out by name -- the dummy has no rematch to ask
	// for, and that mode's own pause menu is the way out of it.
	bool vs_over_done = false;    // this match's panel has been shown: one a match, not one a tick
	void step_vs_over(m3::Model3 &b)
	{
		const int m = b.fv2_mode(), su = b.fv2_submode();
		if (m != 6 && m != 7) { vs_over_done = false; return; }   // off GAME: the next match gets its own
		// AND A ROUND BEING FOUGHT IS WHAT ARMS IT AGAIN. Leaving GAME is not enough: REMATCH and CHANGE
		// STAGE both go back into a match by asking for GAME_INT (Model3::fv2_rematch), so the mode never
		// leaves 6/7 and the second match of a session would have found this already spent.
		if (su == 11 || su == 12) vs_over_done = false;
		if (su != 18 || vs_over_done) return;                     // VIC_DSP, and nowhere else
		if (b.fv2_submode_request() != 19) return;                // ... at the tick it asks to move on
		if (training_on || leaving != L_OFF) return;              // the mode and the way out have their own roads
		if (b.fv2_players() != 3 || !b.fv2_match_over()) return;
		vs_over_done = true;
		ask_for(ask::vs_hold);
		ask_state(cstate::VS_OVER, "the versus match is decided");
		fprintf(stderr, "the console: the versus is decided at VIC_DSP -- the match's own panel, not GAME OVER\n");
	}

	// ---- VF3_VS_TRACE: what the end of a two-player match looks like, frame by frame ----
	//
	// The versus panel below has to open at a moment that is at once LATE enough to BE the end of the
	// match and EARLY enough that NEXT_PROG_SEL has not decided anything yet -- that handler is the one
	// that sends a finished versus to GAME OVER, and the whole point of the panel is to stand where
	// GAME OVER stood. Which frame that is is MEASURED here, not supposed: it prints one line each time
	// the sub-state changes from the judgement on, with what the game has asked for next, whether it
	// already calls the match decided, how many are playing and the rounds each side has lost.
	int vs_trace_was = -1;
	void step_vs_trace(m3::Model3 &b)
	{
		static const bool tracing = getenv("VF3_VS_TRACE") != nullptr;
		if (!tracing) return;
		const int m = b.fv2_mode(), su = b.fv2_submode();
		const bool fighting = m == 6 || m == 7;
		if (fighting && su < 13) return;      // the round itself says nothing about the end of the match
		const int key = fighting ? 100 + su : m;
		if (key == vs_trace_was) return;
		vs_trace_was = key;
		fprintf(stderr, "vs: mode %d sub %d -> asked %d, the match is %s, %d playing, rounds lost %d/%d\n",
			m, su, b.fv2_submode_request(), b.fv2_match_over() ? "decided" : "open", b.fv2_players(),
			b.fv2_option(0x00581455, 1), b.fv2_option(0x0058FDBA, 1));
	}

	// The presses this layer hands the game, eight frames each (see the top for why eight).
	void inject(m3::Controls &out)
	{
		if (inject_start > 0) { out.start[0] = true; inject_start--; }
		if (inject_start_p2 > 0) { out.start[1] = true; inject_start_p2--; }
	}

	// ---- ONE FRAME OF THE CONSOLE LAYER, and the order is the whole of this function ----
	//
	// It is written down because it is not free, and because two of the faults of the 2026-10-25 were
	// nothing but an order: the writes go in first, on the frame boundary, where the game's dispatcher
	// will read them; what reads the game comes after, so it reads this frame and not the last one.
	void step(m3::Model3 &b, m3::Controls &out)
	{
		out = m3::Controls();
		if (!on) return;
		do_asked(b);                                              // what the surfaces asked for
		if (ui.at == cstate::STAGE) b.fv2_park_stage_select();    // the hold that state wants renewed
		step_leaving(b);                                          // the way out of a match, if one runs
		step_training(b);                                         // the clock, the lives, the column's reading
		step_ending(b);                                           // which picture of the Dreamcast's ending
		note_tasks(b);                                            // the two task registers, while they fight
		step_select(b, out);                                      // the trip through the select screen
		step_logo(b);                                             // the road that skips the SEGA logo
		step_vs_over(b);                                          // the end of a two-player match
		step_vs_trace(b);                                         // VF3_VS_TRACE: the end of a match, read
		step_real_ending(b);                                      // the game's own ending, when asked for
		inject(out);                                              // the presses handed to the game
		step_holds(b);                                            // what this port holds while a mode runs
	}
};
