// THE CONSOLE'S OWN STATE, laid out the way the game lays out its own.
//
// Frederic, the 2026-10-25: "de tout le jeu, aucun etat ou sous etat n'empiete sur les autres, sauf celui que
// l'on a integre. On dirait encore du bricolage, comme sur la rom." He is right, and the fault is nameable.
//
// Sega's board holds one state at a time: twenty-four modes, each with an INT that installs it and a DSP that
// runs it, a table of handlers, and ONE byte that says where the game is (docs/fv2-etats.md). Nothing overlaps
// because nothing coexists.
//
// This port's console layer had no state at all. It had a dozen booleans -- swallow_start, want_list,
// want_stage_page, mode_wanted, versus_trip, stage_page_done, seen_select, stage_from_pause, training_on,
// leaving, leave_for -- which together encoded a state machine nobody had written down, and the game was frozen
// or not by a `stopped` flag with exceptions bolted on one at a time. Every defect of the 2026-10-25 came out
// of that: the list opened on a black frame because nothing said who owned the screen; GAME OVER was heard
// because nothing said who owned the sound; the title came up glitched because a transition was a wait and not
// a state.
//
// So: one state at a time, and each one DECLARES three things.
//
//   runs     whether the PowerPC steps. The game is never half-frozen: either the frame runs or it does not.
//   screen   who owns the picture. GAME: the port draws nothing of its own. OVER: the port draws over the
//            picture the game made, which is what a pause menu is. PORT: the port draws its own ground and the
//            game's picture is NOT DRAWN AT ALL -- not covered, not dimmed, not drawn.
//   controls who has the keys and the pad: the game, or the surface that is up. A property, and not a
//            question asked of each surface in turn -- the four used to be asked one after another, each
//            about its own flag, with nothing saying that only one of them could answer.
//   sound    who owns the sound board. GAME: its commands go through. BOTH: they go through and the port adds
//            its own effects over them. PORT: the game's commands are DROPPED before the board hears them, and
//            only the port is heard.
//
// Those three lines are the whole of it. "Nothing of the game is seen or heard while the port owns the screen"
// stops being an intention and becomes a row in a table -- which is how MENU_IN, the state where the game walks
// home behind the menu, is silent without anybody having to remember to silence it.
//
// AND ONE PLACE WRITES THE GAME'S RAM on the port's account: console_play::ask_for, performed at the top
// of console_play::step -- after the frame that has run, before the frame that will, which is the moment
// the game's own dispatcher reads a request. There is ONE exception, and it is written down rather than
// left to be discovered: the OPTIONS page writes the arcade's own settings (fv2_options.h,
// Model3::fv2_set_option). That page IS the settings, and OPTIONS declares runs = false, so no frame of
// the game runs between its write and its next read: the write is at a defined moment by construction.
//
// The INT/DSP shape is Sega's too, and it is used where it means something: MENU_IN is the menu installing
// itself while the game finishes what it was doing; MENU is the menu running with the game held still. The
// player sees one menu; the port knows which half it is in.
#pragma once
#include <cstdio>

enum class cstate {
	PLAY,         // the game, alone: the port shows nothing and says nothing
	PAUSE,        // the pause menu over a frozen fight
	VS_OVER,      // the end of a two-player match: the same panel, over the winner's pose, with no way back
	STAGE,        // the carousel on the select screen: the game RUNS, parked in Sega's empty STAGE_SEL
	STAGE_HELD,   // the carousel opened from the pause menu: there is no sub-state to park in, so the game is held
	GOING,        // a way out of a match that leads back into the game: the curtain, and the game walks behind it
	MENU_IN,      // the list is up and the game is walking home behind it: seen by nobody, heard by nobody
	MENU,         // the list, with the game held still at its title screen
	OPTIONS,      // the OPTIONS tree, the same way
};

enum class cscreen { GAME, OVER, PORT };
enum class csound { GAME, BOTH, PORT };
enum class cinput { GAME, PORT };

struct crule {
	const char *name;
	bool runs;
	cscreen screen;
	csound sound;
	cinput controls;
};

// The table. One row a state, in the enum's own order, and every property of every state is read from here --
// never decided at the place that happens to need it.
inline const crule &crules(cstate s)
{
	static const crule TABLE[] = {
		{ "PLAY",       true,  cscreen::GAME, csound::GAME, cinput::GAME },
		{ "PAUSE",      false, cscreen::OVER, csound::BOTH, cinput::PORT },
		{ "VS_OVER",    false, cscreen::OVER, csound::BOTH, cinput::PORT },
		{ "STAGE",      true,  cscreen::OVER, csound::BOTH, cinput::PORT },
		{ "STAGE_HELD", false, cscreen::OVER, csound::BOTH, cinput::PORT },
		{ "GOING",      true,  cscreen::PORT, csound::PORT, cinput::PORT },
		{ "MENU_IN",    true,  cscreen::PORT, csound::PORT, cinput::PORT },
		{ "MENU",       false, cscreen::PORT, csound::BOTH, cinput::PORT },
		{ "OPTIONS",    false, cscreen::PORT, csound::BOTH, cinput::PORT },
	};
	return TABLE[int(s)];
}

// Where the console is, and nothing else. It is changed in one line and every change says so in the log, so a
// run can be read back state by state exactly as the game's own modes can.
struct console_ui {
	cstate at = cstate::PLAY;

	void set(cstate s, const char *why)
	{
		if (s == at) return;
		fprintf(stderr, "the console: %s -> %s (%s)\n", crules(at).name, crules(s).name, why ? why : "");
		at = s;
	}

	const crule &rule() const { return crules(at); }
	bool game_runs() const { return rule().runs; }
	bool port_screen() const { return rule().screen == cscreen::PORT; }   // the game's picture is not drawn
	bool over_game() const { return rule().screen == cscreen::OVER; }
	bool game_sound() const { return rule().sound == csound::GAME; }
	bool port_sound() const { return rule().sound == csound::PORT; }      // the game's commands are dropped
	bool port_controls() const { return rule().controls == cinput::PORT; }  // the surface has the keys and the pad
	bool playing() const { return at == cstate::PLAY; }
	bool menu_up() const { return at == cstate::MENU || at == cstate::MENU_IN; }
};
