// Fighting Vipers 2's SURVIVAL and RANDOM, on the menu's main page (Escape > Survival, Escape > Random).
//
// A row is not a setting with a yes and a no: it starts a game in that mode, as choosing a mode on a console
// does. The menu closes, the game is handed a real press of Start, and the mode is over when that game is.
//
// The cabinet keeps both behind the operator's buttons, where no player falls over them. Neither is a hand
// manoeuvre to reproduce: each is a byte the game itself reads (model3.h says which, and what was measured
// about them). What is here is the little machine around those bytes, and it is the shape the measurements
// imposed, not a choice:
//
//   ASKED   the row was chosen and nothing is written yet. Survival's flag written during the attract loop
//           stops the game starting at all -- measured -- so nothing may be written before a game is
//           actually running. The press of Start is what makes one start; if none has after GIVE_UP frames
//           (a game that would not take the Start, a player who walked away), the mode is let go of rather
//           than left waiting for a game an hour later.
//   IN      written, the game having started. Survival's goes in again every frame, because nothing says
//           the game does not clear it when a fight begins -- the roster patch's discipline. RANDOM's is
//           written once: measured, the select screen still says RANDOM MODE a hundred frames later.
//   and OFF again when that game ends, with what the game had there before put back. Left on, Survival's
//           flag blocks the next game and an ordinary start stops working. Also off at once in an online
//           match: both of these write a byte of the running game, and the two sides play the same game.
//
// It says what it did at every step. A mode that never went in looks exactly like a game played without it,
// which is how the console way's first Survival went unnoticed for a day.
#pragma once
#include "../machine/model3.h"
#include <cstdio>

class Fv2Modes
{
public:
	enum { SURVIVAL, RANDOM, COUNT };
	static const char *name(int m) { return m == SURVIVAL ? "SURVIVAL MODE" : m == RANDOM ? "RANDOM MODE" : "?"; }

	// The row chosen: this game in that mode. The other mode goes, whatever it was doing -- two modes asked
	// for at once is not a thing the player asked for, and the second row is a change of mind, not an extra.
	void start(int m, m3::Model3 &b)
	{
		if (m < 0 || m >= COUNT) return;
		for (int o = 0; o < COUNT; o++) if (o != m && at[o] != OFF) let_go(b, o, "another mode chosen");
		at[m] = ASKED;
		wait[m] = 0;
		was[m] = 0;
		const bool idle = b.fv2_in_attract();
		if (idle) inject_start = 8;
		fprintf(stderr, "Fighting Vipers 2: %s chosen%s\n", name(m),
		        idle ? ", and the game handed a press of Start" : " (a game is already running: it takes at once)");
	}

	// Both let go of, whatever they were doing: the Versus row does it for the same reason a row of this
	// page does, two modes asked for at once not being a thing the player asked for (fv2_versus.h).
	void let_go_all(m3::Model3 &b, const char *why)
	{
		for (int m = 0; m < COUNT; m++) if (at[m] != OFF) let_go(b, m, why);
	}
	// Every game frame, once the frame has run. `allowed` is false in an online match.
	void step(m3::Model3 &b, bool allowed)
	{
		const bool running = !b.fv2_in_attract();
		for (int m = 0; m < COUNT; m++) {
			if (at[m] == OFF) continue;
			if (!allowed) { let_go(b, m, "an online match"); continue; }
			if (at[m] == ASKED && running) {
				at[m] = IN;
				// read HERE and not when the row was chosen: it is the running game's value that has to go back
				if (m == SURVIVAL) { was[m] = b.fv2_survival(); b.fv2_set_survival(true); }
				else { was[m] = b.fv2_random_chara(); b.fv2_set_random_chara(true); }
				fprintf(stderr, "Fighting Vipers 2: %s is in, the game having started (it was %d)\n", name(m), was[m]);
			} else if (at[m] == ASKED) {
				if (++wait[m] > GIVE_UP) {
					at[m] = OFF;
					fprintf(stderr, "Fighting Vipers 2: %s was chosen and no game started in %d frames -- let go of\n",
					        name(m), GIVE_UP);
				}
			} else if (!running) {
				put_back(b, m);
				at[m] = OFF;
				fprintf(stderr, "Fighting Vipers 2: %s put back, the game being over\n", name(m));
			} else if (m == SURVIVAL) {
				b.fv2_set_survival(true);   // (the one that has to be put in again, every frame)
			}
		}
	}

	// The frame's inputs, after the player's and the script's: the press of Start a chosen row hands the game.
	void controls(m3::Controls &c)
	{
		if (inject_start > 0) { c.start[0] = true; inject_start--; }
	}

private:
	enum { OFF, ASKED, IN };
	static constexpr int GIVE_UP = 900;   // 15 s of the game's own frames

	void let_go(m3::Model3 &b, int m, const char *why)
	{
		if (at[m] == IN) put_back(b, m);
		at[m] = OFF;
		fprintf(stderr, "Fighting Vipers 2: %s let go of (%s)\n", name(m), why);
	}
	void put_back(m3::Model3 &b, int m)
	{
		if (m == SURVIVAL) b.fv2_set_survival(was[m] > 0);
		else b.fv2_set_random_chara(was[m] > 0);
	}

	uint8_t at[COUNT] = {};
	int wait[COUNT] = {};
	int was[COUNT] = {};    // what the game had there before this port wrote it
	int inject_start = 0;   // frames of Start still to hand the game
};
