// Desktop host: an SDL3 window with VF3's picture in it (widescreen in the fights), keyboard and gamepads for
// both players, pacing at the Model 3's 57.52 Hz (by the sound queue when there is sound, else the clock), smooth
// motion (a picture for each refresh of the screen, up to 120 a second), the in-game menu (as VF1's and VF2's),
// and the board's NVRAM (backup RAM and EEPROM) and the menu's settings kept in a data folder.
//
//   vf3 [vf3.zip] [--arcade | --console | --pc] [--full-roster] [--fullscreen | --window] [--no-sound] [--volume N] [--music N] [--effects N]
//       [--scale N] [--aspect fit|4:3|16:10|16:9|21:9] [--show-boot] [--interp] [--roms <folder>]
//
// --volume, --music and --effects (0-100) as the menu's Sound page: the overall volume, and the music's and the
// effects' and voices' within it.
//
// The game's PowerPC code is recompiled to native code from the player's ROM as it starts (src/rec, about 0.2
// s); --interp (or VF3_INTERP=1) runs it on the interpreter instead, the reference it matches frame for frame.
//
// The game and the data folder (the saves, settings.ini, Screenshots, log.txt): a zip named on the command line,
// with vf3_data beside it; else the player's own, roms/vf3.zip in their data folder, which is the program's own
// folder in portable mode (a file named portable.txt beside the program or in the folder above, or beside a Mac's .app: nothing is read
// or written anywhere else), else a "roms" folder's beside the program (the Steam Deck package keeps everything
// in its folder), else the source tree's roms/vf3.zip (from the current folder: vf3_data beside it), else the
// system's (~/Library/Application Support, %APPDATA%, ~/.local/share: "Virtua Fighter 3 Recomp"). The player's
// own is asked for the first time: a file picker for MAME's vf3.zip (Revision D), checked, then copied into
// roms/. A ROM folder of the player's own (--roms <folder>, VF3_ROMS, or roms_folder = <folder> in settings.ini,
// in that order) is looked in first for vf3.zip, and never written to unless a set is picked while it has none.
//
// The menu (Esc, or on a pad View tapped, L3 + R3 or the touchpad click) pauses the game and its sound: Display
// (render scale, antialiasing, picture shape, 3D renderer, smooth motion, background and screen filters, FPS,
// fullscreen), Sound (volume, music, effects and voices), Game (the play style -- PC, arcade, or console for the
// game that has one -- and reset), Arcade settings
// (the arcade's own, and its service menu), Controls (each player's pad, keys and pad buttons) and Quit. Its
// choices are kept in settings.ini in the data folder ("key = value"), written when the menu closes (and within
// 3 s of a hotkey's change); the command line wins over them.
//
// The default is the PC way of playing: no boot to sit through (it runs unseen, about two seconds), and free play
// (the game's own coin setting FREE PLAY, the arcade's kept in settings.ini's arcade_coins and given back for the
// arcade way; its FREE PLAY notices hidden), so Start starts. --arcade (or Game > Play style: Arcade) shows the
// boot and takes coins (the 5 and 6 keys, or the Coin bindings).
//
// --console (Game > Play style: Console, Fighting Vipers 2 only) is the PC way plus a console's own menu over
// the game: Start during the attract loop picks a mode, as the Dreamcast version does. It never touches the
// credit -- choosing a mode hands the game a real Start -- so a coin is still needed for that game, whose free
// play this port does not know. See docs/fv2-console.md.
//
// --scale N (1-6) draws the 3D at N times the arcade's 384 lines; the 2D (text, the HUD) is the arcade's,
// enlarged. Until one is chosen, the scale fits the window as it first opens. F5 steps through them.
// The player's settings are kept as set (as VF1's and VF2's): the governor that held smooth motion to 60 and lowered the
// scale when the computer couldn't keep up runs only in tests (VF3_GOVERN).
//
// --aspect sets the picture's shape; F6 steps through them. The default, fit, follows the window's shape (between
// 4:3 and 48:9). Wider than 4:3, the 3D's view opens out to the sides (the same vertical view) where it covers the
// whole screen (the fights); the 2D stays in the middle 4:3.
//
// Keys:  player 1: arrows, Z guard, X punch, C kick, V escape, 1 start
//        player 2: I J K L, B guard, N punch, M kick, comma escape, 2 start (both remapped in Controls)
//        5 / 6 coins, F2 test, 9 service, Esc the menu, P pause, F3 reset, F5 render scale, F6 picture shape,
//        F9 the timings in the title bar, F11 (or F, Alt+Enter) fullscreen, F12 a screenshot (the picture, into Screenshots in the
//        data folder), Tab held: as fast as it goes. In the arcade's test menu Down moves and Start selects.
// Pads:  d-pad or left stick, X guard, A punch, B kick, Y escape, LT guard+punch, RB punch+kick, LB guard+kick,
//        RT all three, Start; Menu (View tapped by default), L3 + R3 or the touchpad click: the menu (View
//        + Start too; View tapped is a coin only when it's set as Coin); View + LB: render scale; View + RB:
//        picture shape.
//
// --hd-3d draws a texture pack over the game's own 3D textures: PNGs of any size in texture_remplacement in the
// data folder, each named by the hash of the texture it stands in for, which is the name the dump writes
// (--hd-3d-dump, into texture_remplacement/DUMP). --no-hd-3d turns it off again, --hd-3d-pack <folder> and
// --hd-3d-dump <folder> use folders of their own. docs/fv2-hd-3d.md says how it works and what it costs; without
// --hd-3d nothing of it runs and every picture is the one it was.
//
// Testing without a display:
//   VF3_HIDDEN=1         no window on screen, no sound out (the board still runs); never a dialog
//   VF3_INTERP=1         the PowerPC on the interpreter (as --interp)
//   VF3_QUIT_AFTER=N     quit after N game frames, writing build/host_last.png (the frame) and
//                        build/host_window.png (the window as drawn); such runs save nothing
//   VF3_QUIT_AFTER_S=N   quit after N seconds of real time, writing the same two pictures. The clock runs while
//                        the game is frozen, so this arrives where VF3_QUIT_AFTER cannot: anything that freezes
//                        the game (the menu, the console's list of modes) stops its frames. Use it with a
//                        VF3_QUIT_AFTER out of reach, which is what marks a run as a test run.
//   VF3_SCRIPT=file      inputs from an input script (src/host/input_script.h), frame by frame from the boot;
//                        its shots go to build/host_<name>.png
//   VF3_WINDOW=WxH       the window's size in points
//   VF3_DATA=dir         the data folder
//   VF3_PACE_LOG=1       the pacing and the pictures' times every 2 s
//   VF3_KEYS=k@F,...     presses key k (an SDL key name) when game frame F is reached; the window 3 frames later
//                        goes to build/host_window_F.png
//   VF3_MENU_KEYS=k,...  presses these keys (esc, up, down, left, right, enter, or SDL key names) 6 passes of the
//                        loop apart from pass 30
//   VF3_MIDI_LOG=N       every block of bytes the GAME sends its sound board, with the mode it was in and the
//                        loudest sample of the frame before; and every Nth frame besides, so silence is on the
//                        record too (N=1: every frame)
//   VF3_MIDI_SEND=h@F,.. hands the sound board those bytes (hex, no spaces) at game frame F, as if the game had
//                        written them: a way to ask the board a question and hear the answer in VF3_MIDI_LOG's
//                        peaks, without a line of menu code resting on the guess
//   VF3_KEYS_S=k@sec,... presses key k at that many seconds of real time. The one hook that can drive something
//                        drawn over a FROZEN game: VF3_KEYS goes by the game's frames, which stop there, and
//                        VF3_MENU_KEYS goes off in the first second. Seconds, because the clock is the only
//                        thing still moving. Use with VF3_QUIT_AFTER_S.
//   VF3_SHOT_AT=n,...    the window as drawn at these passes of the loop, to build/host_window_at_N.png
//   VF3_LIVE=n           the console CONTROLLER page lit as if those bits were held (1 up, 2 down, 4 left,
//                        8 right, 16 punch, 32 guard, 64 kick): pushed key events cannot reach it
//   VF3_OPTION_LOG=1     what the console OPTIONS page writes: the setting, and the cooked copy it keeps in step
//   VF3_TIMER_LOG=1      the match's own clock: the cooked word a round starts from, and the seconds left
//                        (model3.h, fv2_recook_option). The one way to see the page write rather than trust it.
//   VF3_ROMS=folder      the player's ROM folder (as --roms)
//   VF3_3D=cpu|gpu|auto  the 3D renderer for this run (as Display > 3D renderer)
//   VF3_GPU3D_NO_PROBE=1 no start-up probe of Direct3D 12 (a quad drawn and read back) before the GPU's check
//   VF3_GPU3D_PROBE_ANY=1 the probe on any driver, not only Direct3D 12
//   VF3_GPU_CHECK_DIR=d  the GPU's start-up check's pictures (gpu.png, cpu.png) into d
//   VF3_WIDE_SKY=0       the stages' 2D sky kept to the middle 4:3 (tests)
//   VF3_SKY_LAG=1        the stages' sky as the game scrolls it (3 frames behind the camera: sky_lead.h)
//   VF3_RECORD=dir       a recording (a trailer's footage): from game frame VF3_RECORD_FROM (default 0; before it as
//                        fast as it goes) to VF3_RECORD_TO, 60 pictures a second by the game's clock, not the
//                        real one (smooth motion's in-betweens), the window's whole picture to dir/video.mp4 (by
//                        ffmpeg) and the sound to dir/sound.wav, in step. Use with VF3_HIDDEN and VF3_WINDOW.
//   VF3_GOVERN=1         the governor on (off for players and tests: the settings as set)
//   VF3_THREADS=N        threads drawing the 3D (default: the cores less 1, 12 at most)
//   VF3_TEST_LOAD=ms:s   each picture ms longer (by the scale's square, from 1x) for the first s seconds after the
//                        start's grace (the governor's tests)
#include "version.h"      // VF3_VERSION, VF3_DATE (made by the Makefile from git)
#include "../machine/fv2_portraits.h"
#include "../machine/model3.h"
#include "../machine/romset.h"
#include "../rec/ppc_rec.h"
#include "../sound/soundboard.h"
#include "../video/tilegen.h"
#include "../video/sky_lead.h"
#include "../video/r3d_scene.h"
#include "../video/r3d_raster.h"
#include "../video/parallel.h"
#include "png.h"
#include "input_script.h"
#include "console_play.h"
#include "fv2_mode_list.h"
#include "fv2_options.h"
#include "fv2_pause.h"
#include "fv2_stage_list.h"
#include "fv2_training.h"
#include "fv2_fin.h"
#include "fv2_texte.h"
#include "menu.h"
#include "controls.h"
#include "sysinfo.h"
#include "d3d11_map.h"
#include "../gpu/r3d_gpu.h"
#include <SDL3/SDL.h>
#ifdef _WIN32
#include <SDL3/SDL_main.h>   // WinMain, and argv as UTF-8
#endif
#include <algorithm>
#include <cctype>
#include <array>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <map>
#include <atomic>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#if defined(__APPLE__)
#include <pthread.h>
#include <sys/qos.h>
#endif
#include <vector>
#ifdef _WIN32
#include <io.h>
#define isatty _isatty
#define fileno _fileno
#else
#include <unistd.h>
#endif

// Every message box goes through here: a test run (VF3_HIDDEN) never shows one (nor a file picker); the message
// goes to stderr (log.txt without a console) instead.
static bool message_box(SDL_MessageBoxFlags flags, const char *title, const std::string &msg)
{
	if (getenv("VF3_HIDDEN")) { fprintf(stderr, "(hidden: no dialog) %s\n", msg.c_str()); return false; }
	return SDL_ShowSimpleMessageBox(flags, title, msg.c_str(), nullptr);
}

namespace {

constexpr double FRAME_HZ = 57.524160;       // the Model 3's frame rate (the reference's timing)
constexpr int SOUND_RATE = 44100;
constexpr float LEVEL = 4.0f;                 // --volume 100: the board's output raised 12 dB (it peaks well under full scale), limited
constexpr int MAX_SCALE = 6;
constexpr int BOOT_FRAMES = 960;              // the boot, up to the SEGA logo (the PC way: unseen)

std::string find_assets()
{
	const char *base = SDL_GetBasePath();
	const std::string b = base ? base : "";
	for (const std::string &d : { b + "assets", b + "../../assets", std::string("assets") })
		if (FILE *f = fopen((d + "/ui/menu.rml").c_str(), "rb")) { fclose(f); return d; }
	return "assets";
}

// A folder as the player writes it: "~" (or "~/...") is their home folder; quotes around it are dropped; a
// relative one is taken from `base` (settings.ini's folder) or the current folder.
std::string player_folder(std::string f, const std::string &base = std::string())
{
	while (!f.empty() && (f.back() == ' ' || f.back() == '\t')) f.pop_back();
	if (f.size() >= 2 && (f[0] == '"' || f[0] == '\'') && f.back() == f[0]) f = f.substr(1, f.size() - 2);
	if (f.empty()) return f;
	if (f[0] == '~' && (f.size() == 1 || f[1] == '/' || f[1] == '\\')) {
		const char *h = getenv("HOME");
		if (!h || !*h) h = getenv("USERPROFILE");
		if (h && *h) f = std::string(h) + f.substr(1);
	}
	std::filesystem::path p(f);
	std::error_code ec;
	if (p.is_relative()) p = base.empty() ? std::filesystem::absolute(p, ec) : std::filesystem::path(base) / p;
	p = p.lexically_normal();
	if (!p.has_filename() && p.has_relative_path()) p = p.parent_path();
	return p.string();
}

// The program's folder, with a separator at the end: a Mac app's is the folder the .app is in
std::string program_folder()
{
	const char *base = SDL_GetBasePath();
	std::string b = base ? base : "";
	const std::string in_app = ".app/Contents/Resources/";
	if (b.size() > in_app.size() && b.compare(b.size() - in_app.size(), in_app.size(), in_app) == 0) {
		b.resize(b.size() - in_app.size());
		b.resize(b.find_last_of('/') + 1);
	}
	return b;
}

// Portable mode, as the player asks for it: a file named portable.txt beside the program, or in the folder above it
// (its name as found, else null). Any contents; "portable" and Windows' portable.txt.txt (a New Text Document renamed
// with its extension hidden) count too. The folder above: the download unzips into a folder of its own, and a
// portable.txt put in the folder it was unzipped to went unseen (VF2, a player's report). Looked for with SDL (UTF-8
// names, as on a drive whose folders have accents), not std::filesystem (Windows' code page there). As VF2's.
const char *portable_marker(const std::string &folder)
{
	if (folder.empty()) return nullptr;
	std::string up = folder;
	while (!up.empty() && (up.back() == '/' || up.back() == '\\')) up.pop_back();
	const size_t cut = up.find_last_of("/\\");
	up = cut == std::string::npos ? std::string() : up.substr(0, cut + 1);
	static const char *const NAMES[] = { "portable.txt", "portable", "portable.txt.txt" };
	static const char *const BESIDE[] = { "portable.txt beside the program", "portable beside the program", "portable.txt.txt beside the program" };
	static const char *const ABOVE[] = { "portable.txt in the folder above", "portable in the folder above", "portable.txt.txt in the folder above" };
	for (int above = 0; above < 2; above++) {
		const std::string &dir = above ? up : folder;
		if (dir.empty()) continue;
		for (int k = 0; k < 3; k++) {
			SDL_PathInfo info;
			if (SDL_GetPathInfo((dir + NAMES[k]).c_str(), &info) && info.type == SDL_PATHTYPE_FILE) return above ? ABOVE[k] : BESIDE[k];
		}
	}
	return nullptr;
}

const float ASPECTS[] = { 0.0f, 4.0f / 3.0f, 16.0f / 10.0f, 16.0f / 9.0f, 64.0f / 27.0f };
const char *const ASPECT_NAMES[] = { "fit", "4:3", "16:10", "16:9", "21:9" };
constexpr int N_ASPECTS = 5;

bool g_pc_mode = true;

struct pad_state
{
	bool back_was = false, back_combo = false, start_was = false;
	bool menu_was = false, touch_was = false, sticks_was = false;   // the menu's other buttons (edges)
	int coin_frames = 0, start_seq = -1;
};

enum class hotkey { none, menu, scale, aspect };

// The cabinet's own buttons (the players' are in controls.h, set in the menu)
struct sys_key { SDL_Scancode key; int what; };
enum { SK_COIN1, SK_COIN2, SK_TEST, SK_SERVICE };
const sys_key SYS_KEYS[] = {
	{ SDL_SCANCODE_5, SK_COIN1 }, { SDL_SCANCODE_6, SK_COIN2 }, { SDL_SCANCODE_F2, SK_TEST }, { SDL_SCANCODE_9, SK_SERVICE },
};

// The controls for the next frame, from the keyboard and the pads, and the hotkeys pressed on a pad. The PC way's
// Start: two coins (a credit, as the game is set), then Start.
hotkey read_inputs(m3::Controls &c, SDL_Gamepad *pads[2], pad_state ps[2], const player_controls ctl[2])
{
	c = m3::Controls();
	const bool *ks = SDL_GetKeyboardState(nullptr);
	for (const auto &k : SYS_KEYS)
		if (ks[k.key]) {
			if (k.what == SK_COIN1) c.coin[0] = true;
			if (k.what == SK_COIN2) c.coin[1] = true;
			if (k.what == SK_TEST) c.test[0] = true;
			if (k.what == SK_SERVICE) c.service[0] = true;
		}
	auto key = [&](SDL_Scancode k) { return k != SDL_SCANCODE_UNKNOWN && ks[k]; };
	hotkey hk = hotkey::none;
	static bool key_start_was[2], key_coin_was[2], pad_coin_was[2], key_menu_was[2];
	for (int p = 0; p < 2; p++) {
		pad_state &st = ps[p];
		bool up = false, down = false, left = false, right = false, g = false, pu = false, k = false, e = false;
		auto act = [&](int a) {
			switch (a) {
			case C_UP: up = true; break;
			case C_DOWN: down = true; break;
			case C_LEFT: left = true; break;
			case C_RIGHT: right = true; break;
			case C_GUARD: g = true; break;
			case C_PUNCH: pu = true; break;
			case C_KICK: k = true; break;
			case C_ESCAPE: e = true; break;
			case C_GUARD_PUNCH: g = pu = true; break;
			case C_PUNCH_KICK: pu = k = true; break;
			case C_GUARD_KICK: g = k = true; break;
			case C_ALL_THREE: g = pu = k = true; break;
			default: break;
			}
		};
		auto press_start = [&] {
			st.start_seq = 0;
		};
		for (int a = 0; a < C_START; a++)
			if (key(ctl[p].key[a])) act(a);
		const bool kstart = key(ctl[p].key[C_START]);
		bool start_held = kstart;   // (held, as the cabinet's: a character chosen with Start held is in the 2P outfit)
		if (kstart && !key_start_was[p]) press_start();
		key_start_was[p] = kstart;
		const bool kcoin = key(ctl[p].key[C_COIN]);
		if (kcoin && !key_coin_was[p] && !g_pc_mode) st.coin_frames = 6;
		key_coin_was[p] = kcoin;
		// a key set as Menu opens it too (Esc always does)
		const bool kmenu = key(ctl[p].key[C_MENU]);
		if (kmenu && !key_menu_was[p]) hk = hotkey::menu;
		key_menu_was[p] = kmenu;
		if (SDL_Gamepad *gp = pads[p]) {
			auto btn = [&](SDL_GamepadButton x) { return SDL_GetGamepadButton(gp, x); };
			const int lx = SDL_GetGamepadAxis(gp, SDL_GAMEPAD_AXIS_LEFTX), ly = SDL_GetGamepadAxis(gp, SDL_GAMEPAD_AXIS_LEFTY);
			const int dead = 12000;
			const bool back = btn(SDL_GAMEPAD_BUTTON_BACK);
			const bool start = player_controls::pad_down(gp, ctl[p].pad[C_START]);
			// Coin and Menu on View are its tap (below), so View + LB and the like stay hotkeys
			const int coin_code = ctl[p].pad[C_COIN], menu_code = ctl[p].pad[C_MENU];
			const bool pcoin = coin_code != SDL_GAMEPAD_BUTTON_BACK && player_controls::pad_down(gp, coin_code);
			if (pcoin && !pad_coin_was[p] && !g_pc_mode) st.coin_frames = 6;
			pad_coin_was[p] = pcoin;
			const bool pmenu = menu_code != SDL_GAMEPAD_BUTTON_BACK && player_controls::pad_down(gp, menu_code);
			if (pmenu && !st.menu_was && !back) hk = hotkey::menu;   // (View held: LB and RB are its hotkeys)
			st.menu_was = pmenu;
			// L3 + R3 and the touchpad click (a PlayStation pad's) always open the menu as well, so a
			// pad without a View button, or with Menu moved, can't be locked out of it
			const bool sticks = btn(SDL_GAMEPAD_BUTTON_LEFT_STICK) && btn(SDL_GAMEPAD_BUTTON_RIGHT_STICK);
			if (sticks && !st.sticks_was) hk = hotkey::menu;
			st.sticks_was = sticks;
			bool touch_bound = false;
			for (int a = C_GUARD; a < C_COUNT; a++) touch_bound |= ctl[p].pad[a] == SDL_GAMEPAD_BUTTON_TOUCHPAD;
			const bool touch = !touch_bound && btn(SDL_GAMEPAD_BUTTON_TOUCHPAD);
			if (touch && !st.touch_was) hk = hotkey::menu;
			st.touch_was = touch;
			if (btn(SDL_GAMEPAD_BUTTON_DPAD_UP) || ly < -dead) up = true;
			if (btn(SDL_GAMEPAD_BUTTON_DPAD_DOWN) || ly > dead) down = true;
			if (btn(SDL_GAMEPAD_BUTTON_DPAD_LEFT) || lx < -dead) left = true;
			if (btn(SDL_GAMEPAD_BUTTON_DPAD_RIGHT) || lx > dead) right = true;
			if (back) {
				const bool pstart = btn(SDL_GAMEPAD_BUTTON_START);
				const bool lb = btn(SDL_GAMEPAD_BUTTON_LEFT_SHOULDER), rb = btn(SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER);
				if (pstart && !st.start_was) { hk = hotkey::menu; st.back_combo = true; }
				static bool lb_was[2], rb_was[2];
				if (lb && !lb_was[p]) { hk = hotkey::scale; st.back_combo = true; }
				if (rb && !rb_was[p]) { hk = hotkey::aspect; st.back_combo = true; }
				lb_was[p] = lb; rb_was[p] = rb;
			} else {
				for (int a = C_GUARD; a < C_START; a++)
					if (player_controls::pad_down(gp, ctl[p].pad[a])) act(a);
				if (start && !st.start_was) press_start();
				start_held |= start;
			}
			// View tapped on its own: whichever of Menu and Coin it's set as
			if (!back && st.back_was && !st.back_combo) {
				if (menu_code == SDL_GAMEPAD_BUTTON_BACK) hk = hotkey::menu;
				else if (coin_code == SDL_GAMEPAD_BUTTON_BACK && !g_pc_mode) st.coin_frames = 6;
			}
			if (!back) st.back_combo = false;
			st.back_was = back;
			st.start_was = back ? btn(SDL_GAMEPAD_BUTTON_START) : start;
		}
		c.up[p] = up; c.down[p] = down; c.left[p] = left; c.right[p] = right;
		c.guard[p] = g; c.punch[p] = pu; c.kick[p] = k; c.escape[p] = e;
		if (st.coin_frames > 0) { c.coin[p] = true; st.coin_frames--; }
		// Start, held 6 frames
		if (st.start_seq >= 0) {
			const int s = st.start_seq++;
			if (s < 6) c.start[p] = true;
			else st.start_seq = -1;
		}
		if (start_held) c.start[p] = true;
	}
	return hk;
}

// What the console's CONTROLLER page lights: the eight directions and the three buttons as they are held,
// right now. It is a MIRROR and not an input, so it reads and nothing else -- no edges, no coin timer, no
// hotkeys, none of the state read_inputs owns, which is called once a frame and must stay that way. The
// pairs light both of their buttons, as they hand the game both.
uint8_t live_controls(int p, SDL_Gamepad *gp, const player_controls ctl[2])
{
	const bool *ks = SDL_GetKeyboardState(nullptr);
	uint8_t v = 0;
	auto hit = [&](int action, uint8_t bits) {
		const SDL_Scancode sc = ctl[p].key[action];
		if (sc != SDL_SCANCODE_UNKNOWN && ks[sc]) v |= bits;
		if (gp && player_controls::pad_down(gp, ctl[p].pad[action])) v |= bits;
	};
	hit(C_UP, fv2_options::L_UP);
	hit(C_DOWN, fv2_options::L_DOWN);
	hit(C_LEFT, fv2_options::L_LEFT);
	hit(C_RIGHT, fv2_options::L_RIGHT);
	hit(C_GUARD, fv2_options::L_GUARD);
	hit(C_PUNCH, fv2_options::L_PUNCH);
	hit(C_KICK, fv2_options::L_KICK);
	hit(C_GUARD_PUNCH, fv2_options::L_GUARD | fv2_options::L_PUNCH);
	hit(C_PUNCH_KICK, fv2_options::L_PUNCH | fv2_options::L_KICK);
	hit(C_GUARD_KICK, fv2_options::L_GUARD | fv2_options::L_KICK);
	hit(C_ALL_THREE, fv2_options::L_GUARD | fv2_options::L_PUNCH | fv2_options::L_KICK);
	// The d-pad and the left stick always move, whatever the bindings say -- the same rule read_inputs follows.
	if (gp) {
		auto btn = [&](SDL_GamepadButton x) { return SDL_GetGamepadButton(gp, x); };
		if (btn(SDL_GAMEPAD_BUTTON_DPAD_UP)) v |= fv2_options::L_UP;
		if (btn(SDL_GAMEPAD_BUTTON_DPAD_DOWN)) v |= fv2_options::L_DOWN;
		if (btn(SDL_GAMEPAD_BUTTON_DPAD_LEFT)) v |= fv2_options::L_LEFT;
		if (btn(SDL_GAMEPAD_BUTTON_DPAD_RIGHT)) v |= fv2_options::L_RIGHT;
		const int lx = SDL_GetGamepadAxis(gp, SDL_GAMEPAD_AXIS_LEFTX);
		const int ly = SDL_GetGamepadAxis(gp, SDL_GAMEPAD_AXIS_LEFTY);
		const int dead = 12000;
		if (lx < -dead) v |= fv2_options::L_LEFT;
		if (lx > dead) v |= fv2_options::L_RIGHT;
		if (ly < -dead) v |= fv2_options::L_UP;
		if (ly > dead) v |= fv2_options::L_DOWN;
	}
	return v;
}

// A save is used only if it's the right size.
bool load_blob(const std::string &path, void *dst, size_t n)
{
	FILE *f = fopen(path.c_str(), "rb");
	if (!f) return false;
	std::vector<uint8_t> buf(n + 1);
	const size_t got = fread(buf.data(), 1, n + 1, f);
	fclose(f);
	if (got != n) return false;
	memcpy(dst, buf.data(), n);
	return true;
}

bool flush_to_disk(FILE *f)
{
	if (fflush(f) != 0) return false;
#ifdef _WIN32
	return _commit(_fileno(f)) == 0;
#else
	return fsync(fileno(f)) == 0;
#endif
}

bool replace_file(const std::string &path, const std::string &tmp)
{
	std::error_code ec;
	std::filesystem::rename(tmp, path, ec);
	if (ec) { std::filesystem::remove(tmp, ec); return false; }
	return true;
}

// Written beside the old file, flushed to the disk, then swapped in: a crash mid-save leaves the previous one whole.
bool save_blob(const std::string &path, const void *src, size_t n)
{
	const std::string tmp = path + ".tmp";
	FILE *f = fopen(tmp.c_str(), "wb");
	if (!f) return false;
	const bool ok = fwrite(src, 1, n, f) == n && flush_to_disk(f);
	if (fclose(f) != 0 || !ok) { std::error_code ec; std::filesystem::remove(tmp, ec); return false; }
	return replace_file(path, tmp);
}

// Saves written off the game's thread (as VF1's and VF2's): each flushed to the disk took tens of milliseconds on a
// hard disk, a hitch every few seconds while the arcade's bookkeeping kept changing the backup RAM in play. put copies
// the bytes and returns; the writer thread writes the newest copy of each file; finish writes what's left and stops
// (a put after it is dropped: the caller writes the final copies itself).
struct bg_saver
{
	std::mutex m;
	std::condition_variable cv;
	std::map<std::string, std::vector<uint8_t>> pending;
	bool stop = false;
	std::thread writer{ [this] { run(); } };   // (last: started once the rest is made)

	void put(const std::string &path, const void *src, size_t n)
	{
		std::vector<uint8_t> bytes(static_cast<const uint8_t *>(src), static_cast<const uint8_t *>(src) + n);
		{
			std::lock_guard<std::mutex> lk(m);
			if (stop) return;
			pending[path] = std::move(bytes);
		}
		cv.notify_one();
	}
	void run()
	{
		for (;;) {
			std::map<std::string, std::vector<uint8_t>> now;
			{
				std::unique_lock<std::mutex> lk(m);
				cv.wait(lk, [&] { return stop || !pending.empty(); });
				if (pending.empty() && stop) return;
				now.swap(pending);
			}
			for (auto &[path, bytes] : now)
				if (!save_blob(path, bytes.data(), bytes.size())) fprintf(stderr, "couldn't save %s\n", path.c_str());
		}
	}
	void finish()
	{
		{
			std::lock_guard<std::mutex> lk(m);
			stop = true;
		}
		cv.notify_one();
		if (writer.joinable()) writer.join();
	}
	~bg_saver() { finish(); }
};

uint64_t fnv(const void *p, size_t n, uint64_t h = 1469598103934665603ull)
{
	for (size_t i = 0; i < n; i++) h = (h ^ static_cast<const uint8_t *>(p)[i]) * 1099511628211ull;
	return h;
}

// The menu's choices and the controls, kept in settings.ini in the data folder ("key = value").
// scale 0: none chosen yet (the one that fits the window).
struct settings
{
	static constexpr int VERSION = 3;
	int version = VERSION;
	bool upgraded = false;   // (an upgrade made as it was read: saved at once)
	int scale = 0, aspect = 0, volume = 100, renderer3d = 2;   // renderer3d: 0 the CPU, 1 the GPU, 2 Auto
	int music = 100, effects = 100;   // within the volume
	int arcade_coin = -2;   // the arcade way's coin setting, while the PC way's FREE PLAY replaces it (-1 none to give back, -2 none kept yet)
	std::string roms_folder;
	std::string gpu3d_failed;   // SDL_GPU's drivers whose 3D failed here ("direct3d12 vulkan"): the next start tries the next
	bool fullscreen = false, arcade = false, show_fps = false, smooth = true;
	bool timings = false;    // F9: the timings in the title bar
	bool full_roster = false;   // Fighting Vipers 2's hidden fighters, selectable (Model3::unlock_roster)
	bool honey_costume = false; // Honey's other costume, without the thirty wins (Model3::honey_costume)
	// Which ending the staff roll shows: the arcade's replays of the matches just won, or the Dreamcast's
	// illustrations in their place (src/host/fv2_fin.h). The arcade's is what the board does, so it is what
	// this starts at.
	bool ending_dc = false;
	// The console way: the PC way's boot and free play, plus a console's own menu over the game (Start in the
	// attract loop). A third play style rather than a fourth flag on the other two, because that is how the
	// player thinks of it; `arcade` keeps meaning exactly what it did, so nothing that reads it changes.
	bool console = false;
	// Fighting Vipers 2's select-screen portraits. Empty means the folder this port keeps for them beside its own
	// settings and NVRAM (data/portraits-fvipers2); a folder named with --portraits overrides that. portraits_off
	// is the player saying "Sega's, whatever is in the folder".
	std::string portraits;
	bool portraits_off = false;
	bool hd_3d = false;         // the 3D texture pack, from texture_remplacement in the data folder (src/video/tex3d_hd.h)
	// HD 2D (src/video/tilehd.h, docs/fv2-hd-2d.md): the tile plane drawn tile_scale times its size each way,
	// with tile_pack's images in the place of the tiles and layers whose names they carry. 1 and empty are the
	// hardware's own, and then none of it runs at all.
	int tile_scale = 1;
	std::string tile_pack;
	bool smooth_bg = true;   // the 2D backgrounds (the layers below the 3D) scaled smoothly, not as blocks
	bool sharp_textures = true;   // the 3D's textures' mipmap levels for the picture's size (false: the arcade's, video::Raster::arcade_lod)
	int filter = 0;          // the screen filter: 0 off, 1 scanlines, 2 CRT
	int crt_colours = 0;     // an arcade monitor's colours: 0 off, 1-5 as Supermodel's (CRT_COLOURS)
	int graphics = 0;        // Windows: the GPU renderer's driver, 0 Auto (Vulkan first, Direct3D 12 if it fails), 1 Direct3D 12 first
	int aa = 1;              // antialiasing: the 3D drawn aa times larger each way and averaged (1 off)
	player_controls ctl[2] = { player_controls::defaults(0), player_controls::defaults(1) };

	void load(const std::string &path)
	{
		FILE *f = fopen(path.c_str(), "r");
		if (!f) return;
		std::map<std::string, std::string> kv;
		char line[1100], key[64], val[1024];
		while (fgets(line, sizeof line, f))
			if (sscanf(line, " %63[a-z0-9_] = %1023[^\r\n]", key, val) == 2) kv[key] = val;
		fclose(f);
		auto num = [&](const char *k, int &v, int lo, int hi) {
			auto it = kv.find(k);
			if (it != kv.end()) v = std::clamp(atoi(it->second.c_str()), lo, hi);
		};
		auto flag = [&](const char *k, bool &v) { int i = v; num(k, i, 0, 1); v = i; };
		num("scale", scale, 0, MAX_SCALE);
		num("picture_shape", aspect, 0, N_ASPECTS - 1);
		num("volume", volume, 0, 100);
		num("music_volume", music, 0, 100);
		num("effects_volume", effects, 0, 100);
		num("arcade_coins", arcade_coin, -2, m3::Model3::COIN_FREE_PLAY - 1);
		num("renderer_3d", renderer3d, 0, 2);
		if (auto it = kv.find("roms_folder"); it != kv.end()) roms_folder = it->second;
		if (auto it = kv.find("gpu_3d_failed"); it != kv.end()) gpu3d_failed = it->second;
		flag("fullscreen", fullscreen);
		flag("arcade_mode", arcade);
		flag("console_mode", console);
		if (arcade && console) console = false;   // (a file edited by hand: the arcade way wins, as the older key)
		flag("frame_timings", timings);
		flag("show_fps", show_fps);
		flag("smooth_motion", smooth);
		flag("full_roster", full_roster);
		flag("honey_costume", honey_costume);
		flag("ending_dreamcast", ending_dc);
		if (auto it = kv.find("portraits_folder"); it != kv.end()) portraits = it->second;
		flag("portraits_off", portraits_off);
		flag("hd_3d", hd_3d);
		num("tile_scale", tile_scale, 1, 8);
		if (auto it = kv.find("tile_pack"); it != kv.end()) tile_pack = it->second;
		flag("smooth_backgrounds", smooth_bg);
		flag("background_filter", smooth_bg);   // (VF1's and VF2's name)
		flag("sharp_textures", sharp_textures);
		num("screen_filter", filter, 0, 2);
		num("crt_colours", crt_colours, 0, 5);
		num("graphics", graphics, 0, 1);
		num("antialiasing", aa, 0, 3);
		if (aa == 0) aa = 1;   // (VF2's Low, edges smoothed after drawing: not in this game's renderer)
		for (int p = 0; p < 2; p++) ctl[p].load(kv, p);
		// Upgrades, by the version the settings were written with (saved at once when one is made)
		version = 0;
		num("settings_version", version, 0, 1000);
		// 1, before the Menu button: Arcade mode's View tapped was a coin. It stays one (as Coin, so the menu is
		// L3 + R3, the touchpad, View + Start or Esc), so a cabinet's coin button keeps working after the update
		if (arcade && !kv.count("p1_pad_menu"))
			for (auto &c : ctl)
				if (c.pad[C_COIN] == PAD_NONE && c.pad[C_MENU] == SDL_GAMEPAD_BUTTON_BACK) {
					c.pad[C_COIN] = SDL_GAMEPAD_BUTTON_BACK;
					c.pad[C_MENU] = PAD_NONE;
					upgraded = true;
					fprintf(stderr, "settings: View tapped stays the arcade way's coin (the menu: L3 + R3, the touchpad, View + Start or Esc)\n");
				}
		// 3, after the release review (as VF2's): a GPU failure remembered by an older build forgotten once, so the
		// next start tries the GPU again (the failures it remembers now are only its start-up's)
		if (version < 3 && !gpu3d_failed.empty()) {
			fprintf(stderr, "settings: the GPU's failures remembered by an older build forgotten (%s): the GPU tried again\n", gpu3d_failed.c_str());
			gpu3d_failed.clear();
			upgraded = true;
		}
		if (version < VERSION && !kv.empty()) upgraded = true;
		version = VERSION;
	}

	// false if it couldn't be written (said once in the log; the caller tries again later)
	bool save(const std::string &path) const
	{
		const std::string tmp = path + ".tmp";
		std::error_code dec;
		std::filesystem::create_directories(std::filesystem::path(path).parent_path(), dec);   // (not made yet: a zip on the command line)
		FILE *f = fopen(tmp.c_str(), "w");
		static bool said = false;
		if (!f) {
			if (!said) fprintf(stderr, "couldn't save the settings in %s\n", path.c_str());
			said = true;
			return false;
		}
		fprintf(f, "# Virtua Fighter 3 settings (the in-game menu writes these)\n");
		fprintf(f, "settings_version = %d\n", VERSION);
		fprintf(f, "scale = %d\npicture_shape = %d\nfullscreen = %d\nvolume = %d\narcade_mode = %d\nshow_fps = %d\n"
				"smooth_motion = %d\nrenderer_3d = %d\nbackground_filter = %d\n", scale, aspect, int(fullscreen), volume, int(arcade), int(show_fps),
				int(smooth), renderer3d, int(smooth_bg));
		if (!roms_folder.empty()) fprintf(f, "roms_folder = %s\n", roms_folder.c_str());
		if (!gpu3d_failed.empty()) fprintf(f, "gpu_3d_failed = %s\n", gpu3d_failed.c_str());
		fprintf(f, "crt_colours = %d\nsharp_textures = %d\n", crt_colours, int(sharp_textures));
		if (graphics) fprintf(f, "graphics = %d\n", graphics);
		fprintf(f, "screen_filter = %d\nantialiasing = %d\nmusic_volume = %d\neffects_volume = %d\narcade_coins = %d\nframe_timings = %d\n", filter, aa, music, effects, arcade_coin, int(timings));
		if (full_roster) fprintf(f, "full_roster = %d\n", int(full_roster));   // (one game's: written only once asked for)
		if (honey_costume) fprintf(f, "honey_costume = %d\n", int(honey_costume));   // (likewise)
		if (ending_dc) fprintf(f, "ending_dreamcast = %d\n", int(ending_dc));   // (one game's, likewise)
		if (console) fprintf(f, "console_mode = %d\n", int(console));         // (likewise: an older build reads the file as the PC way)
		if (!portraits.empty()) fprintf(f, "portraits_folder = %s\n", portraits.c_str());   // (one game's, likewise)
		if (portraits_off) fprintf(f, "portraits_off = %d\n", int(portraits_off));
		if (hd_3d) fprintf(f, "hd_3d = %d\n", int(hd_3d));   // (a texture pack is one game's too: written only once asked for)
		if (tile_scale > 1) fprintf(f, "tile_scale = %d\n", tile_scale);      // (as full_roster: written only once asked for)
		if (!tile_pack.empty()) fprintf(f, "tile_pack = %s\n", tile_pack.c_str());
		for (int p = 0; p < 2; p++) ctl[p].save(f, p);
		const bool flushed = flush_to_disk(f);
		if (fclose(f) == 0 && flushed && replace_file(path, tmp)) return true;
		std::error_code ec;
		std::filesystem::remove(tmp, ec);
		if (!said) fprintf(stderr, "couldn't save the settings in %s\n", path.c_str());
		said = true;
		return false;
	}
};

// A file copied whole: to a temporary name, then renamed (as VF1's: std::filesystem::copy_file left an empty file
// in Valve's container on a shared folder, and a cut-short copy mustn't look like a set)
bool copy_whole(const std::filesystem::path &from, const std::filesystem::path &to, std::string &why)
{
	FILE *in = fopen(from.string().c_str(), "rb");
	if (!in) { why = "can't read " + from.string(); return false; }
	const std::filesystem::path tmp = to.string() + ".tmp";
	FILE *out = fopen(tmp.string().c_str(), "wb");
	if (!out) { fclose(in); why = "can't write " + tmp.string(); return false; }
	std::vector<char> buf(1 << 20);
	bool ok = true;
	for (size_t n; (n = fread(buf.data(), 1, buf.size(), in)) > 0;)
		if (fwrite(buf.data(), 1, n, out) != n) { ok = false; break; }
	ok = ok && !ferror(in);
	fclose(in);
	ok = (fclose(out) == 0) && ok;
	if (ok) ok = replace_file(to.string(), tmp.string());
	else { std::error_code ec; std::filesystem::remove(tmp, ec); }
	if (!ok) why = "couldn't write " + to.string();
	return ok;
}

// The player's own game, the first time: no complete set in their data folder's roms/ yet, so ask for MAME's
// vf3.zip with a file picker, check it, and copy it in. The player's ROM folder (`folder`) is looked in first and
// only read. `keep`: the player's data folder when the data folder is the program's own (the Steam Deck package):
// a copy of the zip is kept there too, so an update unpacked into a new folder doesn't ask for it again (as VF1's).
// The zip's path in `out`; false if they cancel.
bool first_launch(const std::string &data, const std::string &keep, const std::string &folder, std::string &out)
{
	namespace fs = std::filesystem;
	const fs::path own = fs::path(data) / "roms";
	std::error_code ec;
	// The sets this build knows, tried in the order romset.h gives them, Virtua Fighter 3's first: a folder with
	// vf3.zip in it is found exactly as it always was, and a package for the other game finds its set too. A
	// folder with none of them gives the first name back, so every message below still has something to say.
	const std::vector<std::string> set_names = romset::known_zip_names();
	auto first_set_in = [&](const fs::path &dir) {
		for (const std::string &n : set_names)
			if (fs::is_regular_file(dir / n, ec)) return dir / n;
		return dir / set_names.front();
	};
	const fs::path roms = folder.empty() ? own : fs::path(folder), zip = first_set_in(roms);
	const fs::path kept = !folder.empty() || keep.empty() ? own : fs::path(keep) / "roms";
	auto keep_copy = [&] {   // (when it isn't there already)
		if (kept == roms || !folder.empty() || !fs::is_regular_file(zip, ec)) return;
		const fs::path keep_as = kept / zip.filename();
		if (fs::is_regular_file(keep_as, ec) && fs::file_size(keep_as, ec) == fs::file_size(zip, ec)) return;
		fs::create_directories(kept, ec);
		std::string why;
		if (copy_whole(zip, keep_as, why)) fprintf(stderr, "a copy of %s kept in %s (for the next update)\n", zip.filename().string().c_str(), kept.string().c_str());
		else fprintf(stderr, "couldn't keep a copy of %s: %s\n", zip.filename().string().c_str(), why.c_str());
	};
	std::string problem;
	auto check = [&](const fs::path &z) -> bool {
		romset::result r = romset::load(z.string());
		if (r.ok) { out = z.string(); return true; }
		problem = r.error + "\n\n";
		return false;
	};
	if (!folder.empty()) {
		if (fs::is_regular_file(zip, ec)) fprintf(stderr, "the game from the ROM folder %s\n", roms.string().c_str());
		else {
			fprintf(stderr, "ROM folder: no set this build knows in %s%s: looking in %s next\n", roms.string().c_str(),
					fs::is_directory(roms, ec) ? "" : " (no such folder)", own.string().c_str());
			const fs::path in_own = first_set_in(own);
			if (fs::is_regular_file(in_own, ec) && check(in_own)) {
				fprintf(stderr, "the game from %s\n", own.string().c_str());
				return true;
			}
		}
	}
	if (fs::is_regular_file(zip, ec) && check(zip)) {
		keep_copy();
		return true;
	}
	// an update in a new folder: the set kept from before
	if (const fs::path in_kept = first_set_in(kept); kept != roms && fs::is_regular_file(in_kept, ec) && check(in_kept)) {
		fprintf(stderr, "the game from %s\n", kept.string().c_str());
		return true;
	}
	if (getenv("VF3_HIDDEN")) {
		fprintf(stderr, "%sno set this build knows in %s\n", problem.c_str(), roms.string().c_str());
		return false;
	}
	SDL_Init(SDL_INIT_VIDEO);
	const char *title = "Virtua Fighter 3";
	// (the player's ROM folder holds a vf3.zip that isn't the set: the one chosen goes in the data folder's roms,
	// not over the player's own file)
	const fs::path into = !folder.empty() && fs::exists(zip, ec) ? own : roms;
	const std::string where = "It's copied into " + into.string() + ", so you only do this once.";
	const std::string which = "Choose your copy of one of the games this build runs: MAME's \"vf3\" set (vf3.zip,\n"
	                          "Virtua Fighter 3, Revision D) or \"fvipers2\" (fvipers2.zip, Fighting Vipers 2).";
	std::string note = problem + which + "\n\n" + where;
	for (;;) {
		if (!message_box(SDL_MESSAGEBOX_INFORMATION, title, note)) return false;
		// (the callback may run on another thread: what it writes is read only after done, set last)
		struct pick { std::atomic<bool> done{ false }; bool failed = false; std::vector<std::string> files; } chosen;
		static const SDL_DialogFileFilter filters[] = { { "MAME ROM sets (zip)", "zip" } };
		SDL_ShowOpenFileDialog([](void *ud, const char *const *list, int) {
			auto *c = static_cast<pick *>(ud);
			if (!list) c->failed = true;
			else for (; *list; list++) c->files.push_back(*list);
			c->done.store(true, std::memory_order_release);
		}, &chosen, nullptr, filters, 1, nullptr, false);
		while (!chosen.done.load(std::memory_order_acquire)) { SDL_Event e; while (SDL_PollEvent(&e)) {} SDL_Delay(10); }
		if (chosen.failed) {
			fs::create_directories(roms, ec);
			message_box(SDL_MESSAGEBOX_INFORMATION, title,
				"There's no file picker here. Put your vf3.zip or fvipers2.zip in this folder, then start the\ngame again:\n\n" + roms.string());
			return false;
		}
		if (chosen.files.empty()) return false;
		const std::string f = chosen.files[0];
		romset::result r = romset::load(f);
		if (!r.ok) {
			note = fs::path(f).filename().string() + ": " + r.error + "\n\n" + which + "\n\n" + where;
			continue;
		}
		fs::create_directories(into, ec);
		std::string why;
		// kept under its set's own name, whichever game was picked
		const fs::path into_zip = into / (r.set + ".zip");
		if (!fs::equivalent(f, into_zip, ec) && !copy_whole(f, into_zip, why)) {
			note = "Couldn't copy it into " + into.string() + ": " + why + "\n\n" + which;
			continue;
		}
		if (into == roms) keep_copy();
		out = into_zip.string();
		return true;
	}
}

// The board and what makes its pictures and sound
// The game's, the sound's and the pictures' threads: macOS runs a thread that doesn't say on whichever cores it likes,
// the efficiency cores among them (a frame that took 7 ms there took more: late for the picture that waits on it)
static void fast_thread()
{
#if defined(__APPLE__)
	pthread_set_qos_class_self_np(QOS_CLASS_USER_INTERACTIVE, 0);
#endif
}

struct Game
{
	m3::Model3 board;
	ppcrec::Rec rec;
	bool recompiled = false;
	SoundBoard sound;
	video::TileGen tiles;
	video::SceneBuilder builder;
	// the last two frames' scenes (smooth motion goes between them) and their 2D layers
	video::Scene scene[2];
	int newest = 0;
	bool have_prev = false;
	std::vector<uint32_t> below = std::vector<uint32_t>(video::W * video::H), above = std::vector<uint32_t>(video::W * video::H);
	std::vector<uint32_t> below_wide = std::vector<uint32_t>(video::WW * video::H);   // (widescreen's: tilegen.h)
	// the same three at the HD size, empty unless tiles.hd() (src/video/tilehd.h)
	std::vector<uint32_t> below_hd, above_hd, below_wide_hd;
	bool full_3d = false;
	video::TileGen::Scroll prev_scroll;   // the frame before's (for bg_dx, bg_dy)
	std::vector<uint32_t> prev_below;
	bool bg_moved = false;                // the layer below the 3D scrolled this frame by bg_dx, bg_dy
	float bg_dx = 0, bg_dy = 0;   // the newest frame's 3D drawn full width (a stage)
	video::SkyLead sky;           // how far the stage's sky is behind the 3D camera (sky_lead.h)
	bool sky_lead_known = true;   // whether that was measured for the loaded game (Virtua Fighter 3's was)
	// How many rows may carry 2D at the 4:3 edges and still be a stage: fewer than video::H / this. The tight 20
	// is Virtua Fighter 3's, whose HUD never reaches the edges, and it keeps that game's close-ups and its
	// full-screen pictures out. Fighting Vipers 2 puts a player's icon hard against each edge above the health
	// bars: 26 such rows, measured right through a round, against the 19 that limit allows. What really tells a
	// stage from a picture is the other two tests -- the 3D drawn full width, and the 2D covering under half.
	int stage_edge_divisor = 20;
	std::vector<int16_t> audio;
	uint64_t frame = 0;
	bool tiles_on = true;   // (off for the boot run unseen: the 2D layers drawn a line at a time for nothing)

	bool load(const std::string &zip, std::string &error, bool interp)
	{
		if (!board.load(zip, error)) return false;
		sky_lead_known = board.set == "vf3";   // (sky_lead.h was measured on that game's stages)
		stage_edge_divisor = board.set == "vf3" ? 20 : 8;
		if (!interp) {
			ppcrec::Stats st;
			built_image = board.program_image();
			seed_map = ppcrec::map_for(board.set, seed_count);
			recompiled = rec.build(board.cpu, built_image, &st, seed_map, seed_count);
			if (recompiled) {
				board.cpu.rec = &rec;
				fprintf(stderr, "recompiled %zu instructions (%zu native) to %zu KB of native code in %.0f ms\n",
						st.instructions, st.native, st.code_bytes / 1024, st.ms);
			} else fprintf(stderr, "couldn't recompile the game (no executable memory?): running it on the slower interpreter instead\n");
		}
		sound.attach(&board.sound_rom, &board.samples);
		// the music/effects split reads Virtua Fighter 3's sound driver (soundboard.h)
		sound.driver_known = board.set == "vf3";
		sound.madrs_mirror = board.set == "vf3";   // (soundboard.h: right for VF3, wrong for Fighting Vipers 2)
		board.on_line = [this](unsigned y) {
			if (!tiles_on) return;
			if (y == 0) tiles.clear();
			tiles.line(board, y);
		};
		return true;
	}
	// The program changed since its native code was made (the PC way's FREE PLAY notices: an instruction in the
	// CROM the boot copies from): made again, or the copied program wouldn't match it and the interpreter would run
	// everything
	std::vector<uint32_t> built_image;
	// the loaded game's address map (src/rec), kept for a rebuild
	const uint32_t *seed_map = nullptr;
	size_t seed_count = 0;
	void program_changed()
	{
		if (!recompiled) return;
		std::vector<uint32_t> img = board.program_image();
		if (img == built_image) return;
		built_image = std::move(img);
		ppcrec::Stats st;
		if (rec.build(board.cpu, built_image, &st, seed_map, seed_count))
			fprintf(stderr, "recompiled again (the program changed): %zu instructions in %.0f ms\n", st.instructions, st.ms);
		else fprintf(stderr, "couldn't recompile the changed program: the interpreter runs it\n");
	}
	// Once the boot has run: a game that patched its own program is now disagreeing with what the native code
	// was made from, and one word of that turns the whole recompiler off (model3.h, ram_program_image). Built
	// again from RAM as it stands, it agrees and runs natively. A game that patched nothing has nothing to do
	// here -- Virtua Fighter 3 never comes past the first line.
	bool rebuilt_after_boot = false;   // (the check below is made once, when the boot is over)
	void rebuild_from_ram_if_stale()
	{
		if (!recompiled || rec.mismatch_count() == 0) return;
		const size_t n = rec.mismatch_count();
		std::vector<uint32_t> img = board.ram_program_image(built_image.size());
		ppcrec::Stats st;
		if (rec.build(board.cpu, img, &st, seed_map, seed_count) && rec.mismatch_count() == 0) {
			built_image = std::move(img);
			fprintf(stderr, "recompiled from RAM after the boot (%zu words the game had patched): %zu instructions in %.0f ms\n",
			        n, st.instructions, st.ms);
		} else {
			fprintf(stderr, "the game patched %zu words of its own program and the rebuild didn't settle it: "
			        "the interpreter runs everything\n", n);
		}
	}
	// Fighting Vipers 2's hidden fighters, if the player asked for them (settings: full_roster). The patch is in
	// RAM, so it waits for the boot to unpack the program; Model3::unlock_roster reads before it writes and says
	// whether it wrote, so this can be called as often as one likes. The first write is the one just before the
	// rebuild after the boot, which is what makes the native code from the patched program; a later one would
	// mean the game had reloaded that part of itself, which nothing measured here does, so it says so and builds
	// again rather than dropping silently to the interpreter.
	bool full_roster = false;
	uint64_t roster_written = 0;
	void hold_roster()
	{
		if (!full_roster || board.unlock_roster() <= 0) return;
		roster_written++;
		// Before the boot's own rebuild, there is nothing to do: that rebuild is what makes the native code from
		// the patched program. After it, any write at all -- the game having reloaded that stretch of its code,
		// or the player having just turned this on from the menu -- leaves one word of RAM disagreeing with the
		// instruction its native code was made from, and one word turns the whole recompiler off. So it is built
		// again from RAM, where it agrees. (Getting this wrong is what made the game run at a sixth of its speed
		// once before: docs/step2-fvipers2.md, section 19.)
		if (!rebuilt_after_boot) return;
		fprintf(stderr, "the hidden fighters: the patch went in after the boot (%llu), the native code made again\n",
		        (unsigned long long)roster_written);
		rebuild_from_ram_if_stale();
	}
	// Honey's four costumes.
	//
	// The game already cycles two of them, and it is a cycle rather than a held direction -- measured on the
	// select screen: one press of up gives colour 1, a second press gives colour 0 again, and holding up is the
	// same as pressing it once. So this adds two more positions to a wheel the game already turns.
	//
	// The colour needs no help at all, which is the neat part. The game's own toggle advances on every press,
	// so positions 2 and 3 of a four-long cycle already reach the game as colours 0 and 1. All this adds is the
	// skirt, decided when the select screen is left -- the models are set up just after, and Honey's cloth
	// handler runs exactly then (docs/fv2-honey.md).
	//
	// The two instructions go in and come back out, because the choice is per match: positions 0 and 1 must get
	// Sega's own words back. The native code is made again only when something actually changed, so a player who
	// keeps the same costume pays nothing between matches.
	bool honey_costume = false;   // the setting: offer four instead of the game's two
	int honey_cycle = 0;          // 0-3 while the select screen is up
	bool honey_on_select = false, honey_up_was = false;
	void honey_tick(bool up_now)
	{
		if (!honey_costume) return;
		const bool on_select = board.fv2_task_state() == 5;
		if (on_select && !honey_on_select) {   // the screen just opened: back to the first costume
			honey_cycle = 0;
			honey_up_was = false;
			if (board.honey_costume(false) > 0 && rebuilt_after_boot) rebuild_from_ram_if_stale();
		}
		if (on_select) {
			if (up_now && !honey_up_was) {
				honey_cycle = (honey_cycle + 1) & 3;
				// Two things have to happen, in this order. The choice goes in first, because the preview is
				// built from the program as it stands. Then the preview has to be ASKED for: nothing in this
				// game watches the cursor, and the preview is built once, when the cursor lands -- so a press
				// of Up on its own would change nothing on screen until the fight started
				// (Model3::fv2_redo_fighter_model, docs/fv2-honey.md).
				if (board.honey_costume(honey_cycle >= 2) > 0 && rebuilt_after_boot) rebuild_from_ram_if_stale();
				for (int p = 0; p < 2; p++) board.fv2_redo_fighter_model(p);
				fprintf(stderr, "Honey: costume %d of 4%s\n", honey_cycle + 1, honey_cycle >= 2 ? ", no skirt" : "");
			}
			honey_up_was = up_now;
		}
		honey_on_select = on_select;
	}
	// The sound board runs on a thread of its own, a frame behind the game (its ~3 ms a frame alongside the next
	// frame's work, not in a picture's time): it hears only the game's bytes, and its sound is queued a frame later
	// (the queue kept a frame shorter for it: QUEUED)
	std::thread sound_thread;
	std::mutex sound_m;
	std::condition_variable sound_cv;
	bool sound_busy = false, sound_quit = false, sound_job_on = false;
	std::vector<uint8_t> sound_midi;
	std::vector<int16_t> sound_out;
	void sound_wait()
	{
		std::unique_lock<std::mutex> lk(sound_m);
		sound_cv.wait(lk, [this] { return !sound_busy; });
	}
	void sound_start()
	{
		sound_thread = std::thread([this] {
			fast_thread();
			std::unique_lock<std::mutex> lk(sound_m);
			for (;;) {
				sound_cv.wait(lk, [this] { return sound_busy || sound_quit; });
				if (sound_quit) return;
				lk.unlock();
				for (uint8_t b : sound_midi) sound.midi_in(b);
				sound_out.clear();
				sound.run(1.0 / FRAME_HZ, sound_out);
				if (!sound_job_on) sound_out.clear();
				lk.lock();
				sound_busy = false;
				sound_cv.notify_all();
			}
		});
	}
	~Game()
	{
		if (emu_thread.joinable()) {
			job_wait();
			{ std::lock_guard<std::mutex> lk(emu_m); emu_quit = true; }
			emu_cv.notify_all();
			emu_thread.join();
		}
		if (sound_thread.joinable()) {
			{ std::lock_guard<std::mutex> lk(sound_m); sound_quit = true; }
			sound_cv.notify_all();
			sound_thread.join();
		}
	}
	void reset()
	{
		if (emu_thread.joinable()) job_wait();
		sound_wait();
		sound_out.clear();
		board.reset();
		sound.reset();
		builder.clear_cache();
		have_prev = false;
		scene[0] = scene[1] = video::Scene();
		frame = 0;
	}
	// One frame: the board, the sound board, the frame's scene and 2D layers. In two halves for smooth motion
	// (start_frame, finish_frame): the first half run a picture before the frame is due, so no picture's time holds
	// a whole frame (at 120 a second a picture has 8.3 ms, a frame took 7-10 on an M4 Max: every other picture missed
	// its refresh, about 80 a second)
	void run_frame(bool sound_on, bool picture)
	{
		start_frame();
		finish_frame(sound_on, picture);
		sysinfo::note_frame();   // (on the thread that ran it: the game's own, when it has one)
	}
	void start_frame() { board.run_frame_start(); }
	// ONE SOUND FRAME WITHOUT A GAME FRAME, for the console's own surfaces: they freeze the PowerPC, and a
	// frozen PowerPC runs no sound frames, so a menu that beeps has to turn the board over itself. This is
	// exactly finish_frame's sound half, with nothing touching the board's state.
	//
	// Safe only where the game says nothing to its sound board, which is where these surfaces open: between
	// frames 1247 and 1385 -- the whole title screen -- the game sends not one byte (docs/fv2-console-son.md
	// step 2). It is not an interrupted conversation, it is a prolonged silence. During a FIGHT it would be
	// another matter, 55 blocks in 1700 frames, and that is noted there for the day a pause menu is written.
	void sound_only_frame()
	{
		if (!sound_thread.joinable()) sound_start();
		sound_wait();
		audio.insert(audio.end(), sound_out.begin(), sound_out.end());
		sound_out.clear();
		note_music();
		{
			std::lock_guard<std::mutex> lk(sound_m);
			sound_midi.assign(board.midi.begin(), board.midi.end());
			board.midi.clear();
			sound_job_on = true;
			sound_busy = true;
		}
		sound_cv.notify_all();
	}
	// A note handed to the sound board as if the game had written it: A0 <bank> <index>, the shape established
	// by the owner by sending them and listening. Bank 0x2E is the effects', bank 0x10 the music's.
	void play_effect(uint8_t index) { for (uint8_t b : { uint8_t(0xA0), uint8_t(0x2E), index }) board.midi.push_back(b); }
	void play_music(uint8_t index) { for (uint8_t b : { uint8_t(0xA0), uint8_t(0x10), index }) board.midi.push_back(b); }
	// The game's own silence, measured: it never sends FF except once at boot, and A0 00 01 is what it puts at
	// the head of its own blocks. Both were measured to take the music to a peak of 0.
	void stop_music() { for (uint8_t b : { uint8_t(0xA0), uint8_t(0x00), uint8_t(0x01) }) board.midi.push_back(b); }

	// WHICH MUSIC IS PLAYING, which is the whole of what "put back what was playing" needs. Read off the bytes
	// as they go past, never guessed: a music command sets it, the game's own silence clears it. -1 means
	// nothing is playing, and then there is nothing to stop and nothing to put back -- which is the case on a
	// title screen reached straight from the SEGA logo, where the first measurement of this job went wrong by
	// proving silence where there had been nothing to cut.
	int music_now = -1;
	// Set by the host on the road where the SEGA logo is skipped: its voice is dropped before the board hears
	// it, so the skip is whole rather than half (see where it is set).
	bool mute_voices = false;
	// TRAINING: the two lines the announcer says before a fight, dropped the same way. The states that
	// say them are cut to one frame each (console_play.h), which takes their pictures and not their
	// sound; bank 0x2B is the one whose index changes with the state, 0x0E at READY and 0x0F at FIGHT.
	// Held only across those frames, never during the fight.
	bool mute_fight_voice = false;
	void note_music()
	{
		if (mute_voices)
			for (size_t i = 0; i + 2 < board.midi.size();)
				if (board.midi[i] == 0xA0 && board.midi[i + 1] == 0x30)
					board.midi.erase(board.midi.begin() + long(i), board.midi.begin() + long(i) + 3);
				else
					i++;
		if (mute_fight_voice)
			for (size_t i = 0; i + 2 < board.midi.size();)
				if (board.midi[i] == 0xA0 && board.midi[i + 1] == 0x2B) {
					// IT SAYS WHAT IT DROPPED. A sound that was never going to be heard looks exactly like a
					// sound that was dropped, so the probe has to speak for itself.
					fprintf(stderr, "training: the announcer dropped -- A0 2B %02X\n", board.midi[i + 2]);
					board.midi.erase(board.midi.begin() + long(i), board.midi.begin() + long(i) + 3);
				} else
					i++;
		for (size_t i = 0; i + 2 < board.midi.size(); i++) {
			if (board.midi[i] != 0xA0) continue;
			if (board.midi[i + 1] == 0x10) music_now = board.midi[i + 2];
			else if (board.midi[i + 1] == 0x00 && board.midi[i + 2] == 0x01) music_now = -1;
		}
		for (uint8_t b : board.midi) if (b == 0xFF) music_now = -1;
	}
	void finish_frame(bool sound_on, bool picture)
	{
		board.run_frame_end();
		if (!sound_thread.joinable()) sound_start();
		sound_wait();   // (the frame before's sound, made meanwhile)
		audio.insert(audio.end(), sound_out.begin(), sound_out.end());
		sound_out.clear();
		// (tests: what the GAME itself says to the sound board, frame by frame, with the state it was in when
		// it said it -- VF3_MIDI_LOG. Read-only: nothing here changes a byte or when it goes.)
		if (static const bool say = getenv("VF3_MIDI_LOG") != nullptr; say) {
			// the loudest sample the board made for the frame before, so the log says what a byte did as well
			// as that it went
			int peak = 0;
			for (int16_t s : audio) peak = std::max(peak, std::abs(int(s)));
			if (!board.midi.empty()) {
				std::string hex;
				for (uint8_t b : board.midi) { char t[8]; snprintf(t, sizeof t, "%02X ", b); hex += t; }
				fprintf(stderr, "midi: frame %llu, mode %d sub %d: %-20s (peak before %d)\n",
						(unsigned long long)frame, board.fv2_mode(), board.fv2_submode(), hex.c_str(), peak);
			} else if (static const uint64_t every = std::max(1ull, strtoull(getenv("VF3_MIDI_LOG"), nullptr, 10)); frame % every == 0) {
				fprintf(stderr, "midi: frame %llu, mode %d sub %d: %-20s (peak before %d)\n",
						(unsigned long long)frame, board.fv2_mode(), board.fv2_submode(), "-", peak);
			}
		}
		// VF3_ENERGY_LOG=N: every N frames, WHICH SLOTS made the sound (soundboard.h: energy_on, energy_take),
		// the loudest first as a share of the whole, with the state the game was in. The total says a sound is
		// there; only this says where it comes from -- written for the victory screen's swell, which four
		// measured hypotheses failed to account for (2026-10-24).
		if (static const char *ev = getenv("VF3_ENERGY_LOG"); ev) {
			static const uint64_t every = std::max<uint64_t>(1, strtoull(ev, nullptr, 10));
			static const bool armed = (sound.energy_on(true), true);
			(void)armed;
			if (frame % every == 0) {
				double e[2][32];
				sound.energy_take(e);
				std::vector<std::pair<double, int>> top;
				double all = 0;
				for (int n = 0; n < 2; n++)
					for (int sl = 0; sl < 32; sl++)
						if (e[n][sl] > 0) { top.push_back({ e[n][sl], n * 32 + sl }); all += e[n][sl]; }
				std::sort(top.rbegin(), top.rend());
				fprintf(stderr, "energy: frame %llu, mode %d sub %d, %zu voix, total %.4g:",
				        (unsigned long long)frame, board.fv2_mode(), board.fv2_submode(), top.size(), all);
				for (size_t i = 0; i < top.size() && i < 6; i++)
					fprintf(stderr, " chip%d/slot%d %.0f%%", top[i].second / 32, top[i].second % 32,
					        all > 0 ? 100.0 * top[i].first / all : 0.0);
				fprintf(stderr, "\n");
			}
		}
		note_music();
		{
			std::lock_guard<std::mutex> lk(sound_m);
			sound_midi.assign(board.midi.begin(), board.midi.end());
			board.midi.clear();
			sound_job_on = sound_on;
			sound_busy = true;
		}
		sound_cv.notify_all();
		frame++;
		if (!picture) { have_prev = false; sky.reset(); return; }
		newest ^= 1;
		builder.build(board, scene[newest]);
		have_prev = frame > 1;
		tiles.aligned(below, above);
		tiles.aligned_wide(below_wide);
		// HD 2D: the same two planes at the HD size, beside the native ones. Everything that reads the layers to
		// decide something -- the scroll below, the stage test, the sides' colours -- goes on reading the native
		// ones, so none of those measurements change with the scale.
		if (tiles.hd()) { tiles.aligned_hd(below_hd, above_hd); tiles.aligned_wide_hd(below_wide_hd); }
		bg_moved = video::TileGen::scrolled(prev_scroll, tiles.scroll, prev_below, below, bg_dx, bg_dy) && have_prev;
		prev_scroll = tiles.scroll;
		prev_below = below;
		static const bool sky_lag = getenv("VF3_SKY_LAG") != nullptr;   // (tests: the game's own backdrop, 3 frames behind)
		// The lead is how far Virtua Fighter 3's backdrop trails its camera, measured on that game's stages
		// (sky_lead.h: 3 frames, 1.11 across and 1.08 up). Another game scrolls its own way, so it is left alone
		// until the same thing has been measured for it -- its backdrop then simply moves as the game moves it.
		if (have_prev && !sky_lag && sky_lead_known) sky.frame(prev(), cur(), bg_moved, bg_dx, bg_dy);
		else sky.reset();
		bool full = false;   // (the 3D drawn full width: a stage; a close-up's or a title's 3D isn't)
		for (auto &v : scene[newest].viewports) full |= v.full_screen && !v.models.empty();
		full_3d = full;
	}
	const video::Scene &cur() const { return scene[newest]; }
	const video::Scene &prev() const { return scene[newest ^ 1]; }
	void forget_prev() { have_prev = false; view.have_prev = false; }

	// What the pictures are drawn from: the last two frames' scenes, their 2D layers, the sky's lead and a copy of the
	// texture RAM, taken from the game as each frame ends (publish). The game's frames run on a thread of their own
	// (job_start, job_wait) while the pictures are drawn: the pictures never read what the game is writing.
	struct View {
		video::Scene scene[2];
		int newest = 0;
		bool have_prev = false, full_3d = false, bg_moved = false;
		float bg_dx = 0, bg_dy = 0;
		std::vector<uint32_t> below = std::vector<uint32_t>(video::W * video::H), above = std::vector<uint32_t>(video::W * video::H);
		std::vector<uint32_t> below_wide = std::vector<uint32_t>(video::WW * video::H);
		std::vector<uint32_t> below_hd, above_hd, below_wide_hd;   // (HD 2D: empty unless tiles.hd())
		video::SkyLead sky;
		std::vector<uint16_t> tex;
		std::vector<m3::Model3::TexUpload> uploads;   // (the areas changed since a renderer last took them)
		uint32_t tex_serial = 0;   // (the board's count of uploads to it when this copy was taken)
		const video::Scene &cur() const { return scene[newest]; }
		const video::Scene &prev() const { return scene[newest ^ 1]; }
	} view;
	void publish()
	{
		view.newest ^= 1;
		view.scene[view.newest] = scene[newest];
		view.have_prev = have_prev;
		view.full_3d = full_3d;
		view.bg_moved = bg_moved;
		view.bg_dx = bg_dx;
		view.bg_dy = bg_dy;
		view.below = below;
		view.above = above;
		view.below_wide = below_wide;
		view.below_hd = below_hd;
		view.above_hd = above_hd;
		view.below_wide_hd = below_wide_hd;
		view.sky = sky;
		// the texture RAM: the rows written since the last (all of it the first time, or after many)
		auto &ups = board.tex_uploads;
		if (view.tex.size() != board.tex_ram.size() || ups.size() > 256) {
			view.tex = board.tex_ram;
			view.uploads.assign(1, m3::Model3::TexUpload{ 0, 0, 2048, 2048 });
		} else {
			for (auto &u : ups) {
				const size_t y0 = u.y & 2047, h = std::min<size_t>(u.h, 2048 - y0);
				memcpy(view.tex.data() + y0 * 2048, board.tex_ram.data() + y0 * 2048, h * 2048 * sizeof(uint16_t));
				view.uploads.push_back(u);
			}
			if (view.uploads.size() > 512) view.uploads.assign(1, m3::Model3::TexUpload{ 0, 0, 2048, 2048 });
		}
		view.tex_serial = board.tex_serial;
		ups.clear();
	}
	std::thread emu_thread;
	std::mutex emu_m;
	std::condition_variable emu_cv;
	bool emu_busy = false, emu_quit = false, emu_sound_on = true;
	void job_start(bool sound_on)
	{
		if (!emu_thread.joinable())
			emu_thread = std::thread([this] {
				fast_thread();
				std::unique_lock<std::mutex> lk(emu_m);
				for (;;) {
					emu_cv.wait(lk, [this] { return emu_busy || emu_quit; });
					if (emu_quit) return;
					const bool snd = emu_sound_on;
					lk.unlock();
					run_frame(snd, true);
					lk.lock();
					emu_busy = false;
					emu_cv.notify_all();
				}
			});
		{
			std::lock_guard<std::mutex> lk(emu_m);
			emu_sound_on = sound_on;
			emu_busy = true;
		}
		emu_cv.notify_all();
	}
	void job_wait()
	{
		std::unique_lock<std::mutex> lk(emu_m);
		emu_cv.wait(lk, [this] { return !emu_busy; });
	}
};

} // namespace


int main(int argc, char **argv)
{
	if (getenv("VF3_HIDDEN")) SDL_SetHint(SDL_HINT_MAC_BACKGROUND_APP, "1");
	const bool pace_log = getenv("VF3_PACE_LOG") != nullptr;
	std::string zip;
	bool arg_arcade = false, sound = true, show_boot = false, arg_interp = getenv("VF3_INTERP") != nullptr;
	int arg_full_roster = -1;   // Fighting Vipers 2's hidden fighters (--full-roster / --no-full-roster)
	int arg_honey = -1;         // Honey's other costume (--honey-costume / --no-honey-costume)
	int arg_style = -1;         // the play style asked for on the command line: 0 the PC way, 2 the console way
	                            // (the arcade way is --arcade, which came first and is kept as it was)
	std::string arg_portraits;      // Fighting Vipers 2's select-screen portraits, from a folder of PNGs (--portraits DIR)
	bool arg_no_portraits = false;  // --no-portraits: back to Sega's, and the setting forgotten
	int arg_hd_3d = -1;         // the 3D texture pack (--hd-3d / --no-hd-3d)
	std::string arg_hd_pack, arg_hd_dump;   // folders of their own, instead of the data folder's (--hd-3d-pack / --hd-3d-dump)
	bool arg_hd_dump_on = false;
	int arg_tile_scale = -1;    // HD 2D (--tile-scale / --tile-pack / --no-tile-pack / --tile-dump)
	std::string arg_tile_pack, arg_tile_dump;
	bool arg_no_tile_pack = false, arg_tile_dump_on = false;
	int arg_volume = -1, arg_music = -1, arg_effects = -1, arg_scale = 0, arg_aspect = -1, arg_full = -1;
	std::string arg_roms;
	for (int i = 1; i < argc; i++) {
		const std::string a = argv[i];
		if (a == "--arcade") arg_arcade = true;
		else if (a == "--roms" && i + 1 < argc) arg_roms = argv[++i];
		else if (a.rfind("--roms=", 0) == 0) arg_roms = a.substr(7);
		else if (a == "--interp") arg_interp = true;
		else if (a == "--fullscreen") arg_full = 1;
		else if (a == "--window") arg_full = 0;
		else if (a == "--no-sound") sound = false;
		else if (a == "--volume" && i + 1 < argc) arg_volume = std::clamp(atoi(argv[++i]), 0, 100);
		else if (a == "--music" && i + 1 < argc) arg_music = std::clamp(atoi(argv[++i]), 0, 100);
		else if (a == "--effects" && i + 1 < argc) arg_effects = std::clamp(atoi(argv[++i]), 0, 100);
		else if (a == "--scale" && i + 1 < argc) arg_scale = std::clamp(atoi(argv[++i]), 1, MAX_SCALE);
		else if (a == "--aspect" && i + 1 < argc) {
			const std::string v = argv[++i];
			for (int k = 0; k < N_ASPECTS; k++) if (v == ASPECT_NAMES[k] || (k == 0 && v == "auto")) arg_aspect = k;
			if (arg_aspect < 0) { fprintf(stderr, "--aspect: fit, 4:3, 16:10, 16:9 or 21:9\n"); return 2; }
		}
		else if (a == "--console") arg_style = 2;
		else if (a == "--pc") arg_style = 0;
		else if (a == "--full-roster") arg_full_roster = 1;
		else if (a == "--no-full-roster") arg_full_roster = 0;
		else if (a == "--honey-costume") arg_honey = 1;
		else if (a == "--no-honey-costume") arg_honey = 0;
		else if (a == "--portraits" && i + 1 < argc) arg_portraits = argv[++i];
		else if (a.rfind("--portraits=", 0) == 0) arg_portraits = a.substr(12);
		else if (a == "--no-portraits") arg_no_portraits = true;
		else if (a == "--hd-3d") arg_hd_3d = 1;
		else if (a == "--no-hd-3d") arg_hd_3d = 0;
		else if (a == "--hd-3d-pack" && i + 1 < argc) { arg_hd_pack = argv[++i]; arg_hd_3d = 1; }
		else if (a == "--hd-3d-dump") {
			arg_hd_dump_on = true;
			if (i + 1 < argc && std::string(argv[i + 1]).rfind("--", 0) != 0) arg_hd_dump = argv[++i];
		}
		else if (a == "--tile-scale" && i + 1 < argc) arg_tile_scale = std::clamp(atoi(argv[++i]), 1, 8);
		else if (a == "--tile-pack" && i + 1 < argc) arg_tile_pack = argv[++i];
		else if (a == "--no-tile-pack") arg_no_tile_pack = true;
		else if (a == "--tile-dump") {
			arg_tile_dump_on = true;
			if (i + 1 < argc && std::string(argv[i + 1]).rfind("--", 0) != 0) arg_tile_dump = argv[++i];
		}
		else if (a == "--show-boot") show_boot = true;
		else if (a.rfind("--", 0) == 0) { fprintf(stderr, "unknown option %s\n", a.c_str()); return 2; }
		else if (a.rfind("-psn_", 0) == 0) {}
		else {
			std::error_code ec;
			if (!std::filesystem::is_regular_file(a, ec)) { fprintf(stderr, "%s: no such file (options start with --)\n", a.c_str()); return 2; }
			zip = a;
		}
	}
	const bool hidden = getenv("VF3_HIDDEN") != nullptr;
	const int64_t quit_after = getenv("VF3_QUIT_AFTER") ? atoll(getenv("VF3_QUIT_AFTER")) : -1;
	// A limit in SECONDS of real time, not in the game's frames, and it exists for one reason: anything that
	// freezes the game stops its frames, so VF3_QUIT_AFTER never comes. A run that opened the console's list of
	// modes sat for nine minutes that way once and had to be killed from outside. The clock keeps running while
	// the game is frozen, so this always arrives -- and what is frozen does not change, so a picture taken at
	// the limit is the same picture whenever the limit falls. Use it with VF3_QUIT_AFTER (a number out of
	// reach), which is what marks a run as a test run everywhere else.
	const double quit_seconds = getenv("VF3_QUIT_AFTER_S") ? atof(getenv("VF3_QUIT_AFTER_S")) : -1;
	InputScript script;
	bool scripted = false;
	if (const char *sp = getenv("VF3_SCRIPT")) {
		std::string err;
		if (!script.load(sp, err)) { fprintf(stderr, "%s\n", err.c_str()); return 2; }
		scripted = true;
	}
	auto fail = [](const std::string &what) {
		fprintf(stderr, "%s\n", what.c_str());
		if (!getenv("VF3_HIDDEN")) message_box(SDL_MESSAGEBOX_ERROR, "Virtua Fighter 3", what);
		return 1;
	};

	// The game and the data folder (see the top)
	std::error_code ec;
	std::string data = getenv("VF3_DATA") ? getenv("VF3_DATA") : "";
	bool players = false;
	std::string keep_data;   // (the Steam Deck package's: where its zip is kept across updates)
	const std::string home = program_folder();
	const char *portable = portable_marker(home);
	if (zip.empty()) {
		const char *base = SDL_GetBasePath();
		const std::string here = base ? base : "";
		if (portable) {
			if (data.empty()) data = home;
			players = true;
		} else if (!here.empty() && std::filesystem::is_directory(here + "roms", ec)) {
			if (data.empty()) {
				data = here;
				// the zip kept in the player's data folder too, for the next update (not while testing: VF3_DATA)
				char *pref = SDL_GetPrefPath(nullptr, "Virtua Fighter 3 Recomp");
				if (pref) keep_data = pref;
				SDL_free(pref);
				while (!keep_data.empty() && keep_data.back() == '/') keep_data.pop_back();
			}
			players = true;
		} else if (arg_roms.empty() && !getenv("VF3_ROMS") && std::filesystem::is_regular_file("roms/vf3.zip", ec)) zip = "roms/vf3.zip";
		else {
			if (data.empty()) {
				char *pref = SDL_GetPrefPath(nullptr, "Virtua Fighter 3 Recomp");
				data = pref ? pref : ".";
				SDL_free(pref);
			}
			players = true;
		}
		while (data.size() > 1 && (data.back() == '/' || data.back() == '\\') && data[data.size() - 2] != ':') data.pop_back();
	}
	if (data.empty()) data = (std::filesystem::path(zip).parent_path() / "vf3_data").string();
	if (data.empty()) data = "vf3_data";
	if (!isatty(fileno(stderr))) {
		std::filesystem::create_directories(data, ec);
		if (freopen((data + "/log.txt").c_str(), "w", stderr)) setvbuf(stderr, nullptr, _IONBF, 0);
	}
	sysinfo::tune();
	fast_thread();
	const std::string system = sysinfo::describe();
	fprintf(stderr, "Virtua Fighter 3 Recomp %s (%s) on %s\n", VF3_VERSION, VF3_DATE, system.c_str());
	// a Steam Deck, with Steam (its SteamDeck variable) or not (its APU: the LCD model's 0405, the OLED's 0932)
	const bool deck = getenv("SteamDeck") || system.find("Custom APU 0405") != std::string::npos || system.find("Custom APU 0932") != std::string::npos;
	if (deck && !getenv("SteamDeck")) fprintf(stderr, "a Steam Deck (its APU), started without Steam\n");
	if (portable) fprintf(stderr, "portable (%s): everything in %s\n", portable, data.c_str());
	else if (home.find("/AppTranslocation/") != std::string::npos)   // (macOS runs a downloaded app from a hidden copy until it's moved)
		fprintf(stderr, "run from macOS's hidden copy of the app (App Translocation): a portable.txt beside it isn't seen until the app is moved with the Finder\n");
	const std::string settings_path = data + "/settings.ini";
	settings cfg;
	bool cfg_dirty = false;
	if (deck) { cfg.fullscreen = true; cfg.aspect = 2; cfg.scale = 2; }
	cfg.load(settings_path);
	if (cfg.upgraded && !getenv("VF3_QUIT_AFTER")) cfg.save(settings_path);
	if (arg_scale) cfg.scale = arg_scale;
	if (arg_aspect >= 0) cfg.aspect = arg_aspect;
	if (arg_full >= 0) cfg.fullscreen = arg_full;
	if (arg_arcade) cfg.arcade = true;
	if (arg_volume >= 0) cfg.volume = arg_volume;
	if (arg_music >= 0) cfg.music = arg_music;
	if (arg_effects >= 0) cfg.effects = arg_effects;
	if (arg_full_roster >= 0) cfg.full_roster = arg_full_roster;
	if (arg_honey >= 0) cfg.honey_costume = arg_honey;
	if (arg_style >= 0) { cfg.console = arg_style == 2; cfg.arcade = false; }
	// --portraits names a folder other than the one this port keeps for them, and is remembered, so it is named
	// once. --no-portraits goes back to Sega's and is remembered too -- otherwise a folder still full of PNGs
	// would quietly bring them back on the next launch.
	if (!arg_portraits.empty() && (cfg.portraits != arg_portraits || cfg.portraits_off)) {
		cfg.portraits = arg_portraits;
		cfg.portraits_off = false;
		cfg_dirty = true;
	}
	if (arg_no_portraits && !cfg.portraits_off) { cfg.portraits_off = true; cfg_dirty = true; }
	// The console way takes the PC way's boot and free play: it only adds a menu of its own over the game, and
	// it never touches the credit -- choosing a mode there hands the game a real Start, and the game decides
	// what that costs (docs/fv2-console.md).
	if (arg_hd_3d >= 0) cfg.hd_3d = arg_hd_3d;
	if (arg_tile_scale >= 0) cfg.tile_scale = arg_tile_scale;
	if (!arg_tile_pack.empty()) cfg.tile_pack = arg_tile_pack;
	if (arg_no_tile_pack) cfg.tile_pack.clear();   // (--no-tile-pack: the settings' pack ignored, for comparing)
	g_pc_mode = !cfg.arcade;
	int aspect_i = cfg.aspect;

	if (players) {
		std::string roms_folder, from;
		if (!arg_roms.empty()) roms_folder = player_folder(arg_roms), from = "--roms";
		else if (const char *e = getenv("VF3_ROMS"); e && *e) roms_folder = player_folder(e), from = "VF3_ROMS";
		else if (!cfg.roms_folder.empty()) roms_folder = player_folder(cfg.roms_folder, data), from = "settings.ini";
		if (!roms_folder.empty()) fprintf(stderr, "ROM folder (%s): %s\n", from.c_str(), roms_folder.c_str());
		else fprintf(stderr, "ROM folder: %s\n", (std::filesystem::path(data) / "roms").string().c_str());
		if (!first_launch(data, keep_data, roms_folder, zip)) return hidden ? 1 : 0;
	}
	static Game game;   // (large: not on the stack)
	{
		std::string err;
		if (!game.load(zip, err, arg_interp)) return fail(err);
	}
	m3::Model3 &board = game.board;
	// HD 2D (docs/fv2-hd-2d.md). The pack is read once the scale is known, because every image in it has to be
	// at that scale: one drawn for another is refused by name rather than shown at the wrong size. With no pack
	// and a scale of 1 -- the default -- hd() stays false and the rasteriser is the one it has always been.
	static video::TilePack tile_pack;
	static video::TileDump tile_dump;
	// Where the 2D's folders are, when the player names none: beside the 3D's, laid out the same way --
	// texture_remplacement_2d for the images that replace tiles and layers, and DUMP under it for what the dump
	// writes. A folder of its own, not the 3D's, because the two name their files differently: a 3D replacement
	// is <hash>_<w>x<h>_f<format>.png and a tile is <hash>_p<palette>_<bpp>bpp.png, and the 3D loader reads the
	// root of its own folder, so one heap would have each refusing the other's files by the dozen.
	{
		const std::string root = data + "/texture_remplacement_2d";
		if (arg_tile_dump_on || !arg_tile_dump.empty()) {
			const std::string into = arg_tile_dump.empty() ? root + "/DUMP" : player_folder(arg_tile_dump, data);
			std::string why;
			std::error_code tec;
			std::filesystem::create_directories(into, tec);
			if (!tile_dump.open(into, why))
				fprintf(stderr, "the tile dump can't be written in %s (%s)\n", into.c_str(), why.c_str());
			else fprintf(stderr, "tile dump: %s, with tiles/ and layers/ -- everything the game draws, once each\n", into.c_str());
		}
		const std::string pack = !cfg.tile_pack.empty() ? player_folder(cfg.tile_pack, data) : root;
		// An empty folder says nothing: a player who never puts anything there should not read a report about it
		// at every launch. TilePack::load reads <folder>/tiles and <folder>/layers, so those are what is looked
		// in -- not the root, which holds only those two and would always look empty.
		bool any = false;
		std::error_code pec;
		for (const char *sub : { "/tiles", "/layers" })
			for (std::filesystem::directory_iterator it(pack + sub, pec), end; it != end && !any; it.increment(pec))
				if (it->path().extension() == ".png") any = true;
		if (any) {
			std::string report;
			tile_pack.load(pack, cfg.tile_scale, report);
			fprintf(stderr, "%s\n", report.c_str());
		}
	}
	game.tiles.hd_set(cfg.tile_scale, &tile_pack, &tile_dump);
	if (game.tiles.hd())
		fprintf(stderr, "HD 2D: the tile plane drawn at %d x %d (%d times the hardware's)\n",
		        video::W * game.tiles.hd_scale(), video::H * game.tiles.hd_scale(), game.tiles.hd_scale());
	game.full_roster = cfg.full_roster;
	game.honey_costume = cfg.honey_costume;
	if (cfg.full_roster && board.unlock_roster() < 0)
		fprintf(stderr, "the hidden fighters: that was measured on Fighting Vipers 2 (Japan, Revision A), not on this game\n");
	// Per-game names for what this port keeps of its own: Virtua Fighter 3 keeps the names it has always had, so
	// saves made before this port took a second game still load, and another game gets its own -- otherwise
	// Fighting Vipers 2 would read Virtua Fighter 3's settings at the start and write over them at the end. The
	// NVRAM below and the portraits folder just under are both named this way.
	//
	// The 3D texture pack (src/video/tex3d_hd.h) is not, and on purpose: it sits in one folder,
	// texture_remplacement, because a replacement is recognised by the content of the texture it stands in for,
	// so one game's images cannot match another's and there is nothing to keep apart. texture_remplacement/DUMP
	// is where the dump writes. Both take a folder of their own on the command line. Both are set up before the
	// first picture, and asking for neither leaves both rasterisers' texture paths exactly as they were -- which
	// is why Virtua Fighter 3 is untouched by any of it.
	const std::string hd_pack_dir = arg_hd_pack.empty() ? data + "/texture_remplacement" : player_folder(arg_hd_pack, data);
	{
		const std::string &pack = hd_pack_dir;
		if (cfg.hd_3d) {
			std::string report;
			if (!video::hd3d().load(pack, report))
				fprintf(stderr, "the 3D texture pack: nothing loaded from %s: the game's own textures are drawn\n", pack.c_str());
			else
				fprintf(stderr, "%s\n", report.c_str());
		}
		if (arg_hd_dump_on) {
			const std::string into = arg_hd_dump.empty() ? data + "/texture_remplacement/DUMP" : player_folder(arg_hd_dump, data);
			std::string why;
			if (!video::hd3d().dump_to(into, why)) fprintf(stderr, "%s\n", why.c_str());
			else fprintf(stderr, "the 3D texture dump: every texture a polygon samples, into %s\n", into.c_str());
		}
	}
	const std::string nv_suffix = board.set == "vf3" ? std::string() : "-" + board.set;
	// The select-screen portraits, read from a folder of PNGs. Unlike the roster patch above, nothing here has
	// to wait: the sheets are in the VROM, which is a ROM the game never writes, so they are replaced once in
	// the image this process holds and that is the end of it. Nothing is written to the player's zip. See
	// src/machine/fv2_portraits.cpp and docs/fv2-portraits.md.
	//
	// Where that folder is: beside this port's own settings and NVRAM, named the way they are --
	// data/portraits-fvipers2. The first version of this had no folder of its own and asked the player to point
	// the option at one in another project's tree, which is how two projects quietly start sharing files and
	// then diverge. --portraits still names another folder for anyone who wants one.
	//
	// It is made if it is not there, so there is somewhere plain to drop the PNGs, and an empty one says
	// nothing: a player who never touches it should not read two lines about it at every launch.
	const std::string portraits_dir = cfg.portraits.empty() ? data + "/portraits" + nv_suffix
	                                                        : player_folder(cfg.portraits, data);
	if (board.set == "fvipers2" && !cfg.portraits_off) {
		std::error_code pec;
		std::filesystem::create_directories(portraits_dir, pec);
		bool any = false;
		for (std::filesystem::directory_iterator it(portraits_dir, pec), end; it != end && !any; it.increment(pec))
			any = true;
		if (any) fv2::replace_portraits_and_report(board.set, board.vrom, portraits_dir, stderr);
		else fprintf(stderr, "portraits: none to read in %s (drop light/00.png and the rest there)\n", portraits_dir.c_str());
	}
	const std::string backup_path = data + "/backup" + nv_suffix, eeprom_path = data + "/eeprom" + nv_suffix;
	load_blob(backup_path, board.backup.data(), board.backup.size());
	load_blob(eeprom_path, board.eeprom.regs, sizeof(board.eeprom.regs));
	auto nvram_hash = [&] { return fnv(board.eeprom.regs, sizeof(board.eeprom.regs), fnv(board.backup.data(), board.backup.size())); };
	uint64_t saved_hash = nvram_hash();
	bg_saver saver;
	// (in play: copied for the writer thread, bg_saver; at the quit: written here, the writer finished first)
	auto save_nvram = [&](bool now = false) {
		if (game.emu_thread.joinable()) game.job_wait();   // (not while a frame runs alongside)
		if (quit_after >= 0) return;   // test runs leave the saves alone
		const uint64_t h = nvram_hash();
		if (h == saved_hash) return;
		std::filesystem::create_directories(data, ec);
		if (!now) {
			saver.put(backup_path, board.backup.data(), board.backup.size());
			saver.put(eeprom_path, board.eeprom.regs, sizeof(board.eeprom.regs));
			saved_hash = h;
		} else if (save_blob(backup_path, board.backup.data(), board.backup.size()) &&
				   save_blob(eeprom_path, board.eeprom.regs, sizeof(board.eeprom.regs)))
			saved_hash = h;
		else fprintf(stderr, "couldn't save the NVRAM in %s\n", data.c_str());
	};

	// Boot: the PC way runs it unseen up to the SEGA logo
	auto skip_boot = [&] { return g_pc_mode && !show_boot && !scripted; };
	uint64_t game_frame = 0;   // frames since the boot (the script's frame numbers)
	// The PC way: the FREE PLAY notices hidden (always free play: nothing to say); the arcade way: shown. Both
	// the notices and the coin settings are at addresses found in Virtua Fighter 3's own test menu, so for
	// another game there is nothing to hide and nothing to say about it (model3.h).
	// Whether this port knows where the game keeps its coin settings (Model3::coin_offset). Both games now, so
	// the PC way -- and the console way, which is the PC way plus a menu -- comes up on FREE PLAY for either.
	const bool play_style_known = board.coin_offset() >= 0;
	// The console way is one game's: its menu is built on addresses measured in Fighting Vipers 2 (Japan,
	// Revision A) and means nothing anywhere else, so the menu doesn't offer it there (docs/fv2-console.md).
	const bool console_known = board.set == "fvipers2";
	// the console way's own menu and its injections, mixed in with the script's and the player's (frame_inputs)
	console_play cplay;
	cplay.on = cfg.console && console_known;
	m3::Controls console_ctl;
	auto apply_play_style = [&] {
		if (!play_style_known) return;
		if (game.emu_thread.joinable()) game.job_wait();   // (not while a frame runs alongside)
		if (!g_pc_mode) board.show_free_play_text();
		// Where the notices are is Virtua Fighter 3's own, found in its CROM. For another game they simply stay,
		// which is a cosmetic matter and not worth a line of complaint at every boot: Fighting Vipers 2 shows
		// FREE PLAY in the corner, which is true.
		else if (board.hide_free_play_text() != 2 && board.set == "vf3")
			fprintf(stderr, "not the data known: the FREE PLAY notices stay\n");
		game.program_changed();
	};
	auto boot = [&] {
		apply_play_style();
		game.reset();
		// The PC way plays free: the arcade's coin setting is kept (settings.ini's arcade_coins) before FREE PLAY
		// replaces it, and the arcade way gives it back (as VF1's and VF2's). A board that has never run writes its
		// settings in its first frames, so those run first. FREE PLAY the player chose in the test menu, with none
		// kept, stays.
		// (a game whose coin settings this port doesn't know has nothing to look for here, and its board is left
		// where it is rather than run 300 frames for nothing)
		if (play_style_known)
			for (int i = 0; i < 300 && board.coin_setting() < 0; i++) board.run_frame();
		if (const int coin = board.coin_setting(); coin >= 0) {
			int keep = cfg.arcade_coin;
			if (g_pc_mode) {
				if (coin != m3::Model3::COIN_FREE_PLAY) keep = coin;
				else if (keep == -2) keep = -1;
				board.set_coin_setting(m3::Model3::COIN_FREE_PLAY);
			} else {
				if (coin == m3::Model3::COIN_FREE_PLAY && keep >= 0) board.set_coin_setting(keep);
				keep = -1;
			}
			if (keep != cfg.arcade_coin) { cfg.arcade_coin = keep; cfg_dirty = true; }
			if (g_pc_mode && !board.free_play()) fprintf(stderr, "the PC way: couldn't set FREE PLAY in the game's settings (coin setting %d)\n", board.coin_setting());
			game.reset();
		} else if (!play_style_known && g_pc_mode) {
			static bool said = false;
			if (!said) { said = true; fprintf(stderr, "the PC way: where this game keeps its coin settings isn't known, so it takes coins as the arcade does\n"); }
		}
		game_frame = 0;
		if (!skip_boot()) return;
		const auto t0 = std::chrono::steady_clock::now();
		game.tiles_on = false;
		while (game_frame < BOOT_FRAMES) {
			game.run_frame(false, false);
			game.audio.clear();
			game_frame++;
		}
		game.tiles_on = true;
		game.rebuilt_after_boot = true;
		game.hold_roster();   // (before the rebuild: the native code is then made from the program as it will run)
		game.rebuild_from_ram_if_stale();
		fprintf(stderr, "boot: %llu frames in %.2f s, unseen\n", (unsigned long long)game_frame,
				std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count());
	};
	boot();

#ifdef _WIN32
	SDL_SetHint(SDL_HINT_FORCE_RAISEWINDOW, "1");
#endif
	if (!SDL_Init(SDL_INIT_VIDEO)) return fail(std::string("Couldn't start the video: ") + SDL_GetError());
	if (!SDL_InitSubSystem(SDL_INIT_GAMEPAD)) fprintf(stderr, "no gamepads: %s\n", SDL_GetError());
	if (sound && !SDL_InitSubSystem(SDL_INIT_AUDIO)) fprintf(stderr, "no sound: %s\n", SDL_GetError());
	int win_w = 1366, win_h = 768;
	const bool fixed_window = getenv("VF3_WINDOW") && sscanf(getenv("VF3_WINDOW"), "%dx%d", &win_w, &win_h) == 2;
	if (!fixed_window) {
		SDL_Rect usable;
		if (SDL_GetDisplayUsableBounds(SDL_GetPrimaryDisplay(), &usable) && usable.w > 0 && usable.h > 0) {
			const float k = std::min({ 1.0f, usable.w * 0.9f / win_w, usable.h * 0.9f / win_h });
			win_w = int(win_w * k); win_h = int(win_h * k);
		}
	}
	// the window's name is the game's (board.title), so a second game doesn't say it is the first
	const std::string game_name = board.title.empty() ? std::string("Virtua Fighter 3") : board.title;
	SDL_Window *win = SDL_CreateWindow(game_name.c_str(), win_w, win_h,
		SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY | (cfg.fullscreen ? SDL_WINDOW_FULLSCREEN : 0) | SDL_WINDOW_HIDDEN);
	if (!win) return fail(std::string("Couldn't open the window: ") + SDL_GetError());
	SDL_HideCursor();
	// the window's icon (the Mac app and the Windows exe carry their own)
	if (SDL_Surface *icon = SDL_LoadPNG((find_assets() + "/icon/icon.png").c_str())) {
		SDL_SetWindowIcon(win, icon);
		SDL_DestroySurface(icon);
	}
	// The 3D on the GPU (Display > 3D renderer: Auto or GPU, or VF3_3D=cpu|gpu|auto): SDL's "gpu" renderer
	// (Vulkan, Direct3D 12 or Metal), whose device the 3D shares, its pictures the renderer's textures. Else, or if
	// it can't start here, SDL's usual one and the CPU's 3D.
	enum { R3D_CPU, R3D_GPU, R3D_AUTO };
	static const char *R3D_NAMES[] = { "CPU", "GPU", "Auto" };
	int r3d_mode = cfg.renderer3d;
	if (const char *e = getenv("VF3_3D")) r3d_mode = !strcmp(e, "gpu") ? R3D_GPU : !strcmp(e, "auto") ? R3D_AUTO : R3D_CPU;
	SDL_Renderer *ren = nullptr;
	auto failed_before = [&](const char *d) { return (" " + cfg.gpu3d_failed + " ").find(std::string(" ") + d + " ") != std::string::npos; };
	if (r3d_mode != R3D_CPU && !getenv("SDL_RENDER_DRIVER")) {
		// (as VF2's: a marker left while the renderer starts, there next time if it crashed then, so SDL's usual
		// renderer and the CPU's 3D, said so; deleting gpu-start.crashed tries again. CRT Emudriver, the modified
		// AMD driver arcade cabinets use, crashes there: not tried at all.)
		const std::string marker = data + "/gpu-start.crashed";
		const bool known_bad = sysinfo::gpu_names().find("CRT Emudriver") != std::string::npos;
		const bool crashed = known_bad || SDL_GetPathInfo(marker.c_str(), nullptr);
		if (known_bad) fprintf(stderr, "SDL's gpu renderer: not tried with CRT Emudriver (its start crashes there): SDL's usual renderer, the CPU's 3D\n");
		else if (crashed) fprintf(stderr, "SDL's gpu renderer: the last start closed while starting it (%s): SDL's usual renderer, the CPU's 3D\n", marker.c_str());
		if (!crashed) {
			if (SDL_IOStream *io = SDL_IOFromFile(marker.c_str(), "w")) SDL_CloseIO(io);
			// (a driver whose 3D failed its check here before is passed over: Direct3D 12's for Vulkan, SDL's other on
			// Windows: some older cards draw nothing on one and are fine on the other. SDL_GPU_DRIVER chooses over this.)
			// (as VF1's and VF2's: Vulkan first on Windows for every card, Direct3D 12 if Vulkan won't start or draws no
			// 3D, remembered. A Radeon's first (vf2-recomp #19: an RX 9070 on Direct3D 12 took ~29 ms a picture, 30 a
			// second at any scale), then NVIDIA's too: vf2-recomp #21 and #22 (an RTX 3060: ~100 ms a 3D picture on
			// Direct3D 12, 57 a second on Vulkan), vf1-recomp #28 (17 ms a 2D layer's upload), #29 and #16. The player's
			// Display > Graphics driver: Direct3D 12 first instead.)
#ifdef _WIN32
			const std::string cards = sysinfo::gpu_names();   // (the hardware's: software adapters left out)
			const bool any_card = !cards.empty() && cards.find("Microsoft Basic Render") == std::string::npos;
#else
			const bool any_card = false;
#endif
			const bool vk_first = any_card && cfg.graphics == 0 && !failed_before("vulkan") && !getenv("SDL_GPU_DRIVER");
			const bool try_vulkan = (failed_before("direct3d12") || vk_first) && !failed_before("vulkan") && !getenv("SDL_GPU_DRIVER");
			if (try_vulkan) {
				fprintf(stderr, "SDL's gpu renderer: Vulkan (%s)\n", failed_before("direct3d12") ? "the 3D failed on Direct3D 12 here before" : "Windows, first");
				SDL_SetHint(SDL_HINT_GPU_DRIVER, "vulkan");
			}
			ren = SDL_CreateRenderer(win, "gpu");
			if (!ren && try_vulkan) {   // (no Vulkan here: Direct3D 12 again)
				fprintf(stderr, "SDL's gpu renderer: Vulkan won't start (%s): Direct3D 12\n", SDL_GetError());
				cfg.gpu3d_failed += " vulkan";
				cfg_dirty = true;
				SDL_ResetHint(SDL_HINT_GPU_DRIVER);
				ren = SDL_CreateRenderer(win, "gpu");
			}
			// A first look, before anything is drawn (as VF2's: vf2-recomp #3, a GTX 970 drew no 3D on Direct3D 12 and
			// was fine on Vulkan): a made-up scene, one white quad over the screen, on the renderer's device and on the
			// CPU; the GPU's empty where the CPU's is drawn, the renderer made again on Vulkan at once, not from the next
			// start. Only that case: the full check, pixel for pixel, comes later on the game's own scene. 1 drawn, 0
			// empty, -1 not looked at. (VF3_GPU3D_NO_PROBE: not done; VF3_GPU3D_PROBE_ANY: on any driver, for tests.)
			auto draws_3d = [&](SDL_Renderer *r) -> int {
				SDL_GPUDevice *gd = r ? SDL_GetGPURendererDevice(r) : nullptr;
				if (!gd) return -1;
				R3dGpu dev;
				if (!dev.init(gd)) return -1;
				video::Scene sc;
				video::Viewport v;
				v.full_screen = true;
				v.left = -1; v.right = 1; v.bottom = -0.75f; v.top = 0.75f; v.cota = 1;
				video::Mesh m;
				const float q[6][2] = { { -20, -20 }, { 20, -20 }, { 20, 20 }, { -20, -20 }, { 20, 20 }, { -20, 20 } };
				for (auto &p : q) {
					video::Vertex x = {};
					x.pos[0] = p[0]; x.pos[1] = p[1]; x.pos[2] = -10;
					x.normal[2] = x.face_normal[2] = 1;
					x.colour[0] = x.colour[1] = x.colour[2] = x.colour[3] = 255;
					m.verts.push_back(x);
				}
				video::Model md;
				md.meshes = std::make_shared<std::vector<video::Mesh>>(1, m);
				md.matrix = video::Mat4::identity();
				v.models.push_back(md);
				sc.viewports.push_back(v);
				video::Raster cpu;
				cpu.resize(video::W, video::H);
				cpu.prepare(sc);
				if (!dev.render(sc, cpu, board)) return 0;
				std::vector<uint32_t> gp;
				if (!dev.read_output(gp)) return -1;
				cpu.render(sc, board.tex_ram.data());
				std::vector<uint32_t> cp(size_t(cpu.width()) * cpu.height());
				cpu.to_premultiplied(reinterpret_cast<uint8_t *>(cp.data()), size_t(cpu.width()) * 4);
				if (gp.size() != cp.size()) return -1;
				long long cpu_drawn = 0, gpu_drawn = 0;
				for (size_t i = 0; i < cp.size(); i++) { cpu_drawn += (cp[i] & 0xFFFFFF) != 0; gpu_drawn += (gp[i] & 0xFFFFFF) != 0; }
				const long long px = (long long)cp.size();
				if (getenv("VF3_GPU3D_PROBE_ANY")) fprintf(stderr, "SDL's gpu renderer: the probe: %lld of %lld pixels drawn on the CPU, %lld on the GPU\n", cpu_drawn, px, gpu_drawn);
				return cpu_drawn > px / 2 && gpu_drawn < px / 100 ? 0 : 1;
			};
			if (ren && !getenv("SDL_GPU_DRIVER") && !getenv("VF3_GPU3D_NO_PROBE") && !(failed_before("direct3d12") && failed_before("vulkan"))) {
				SDL_GPUDevice *gd = SDL_GetGPURendererDevice(ren);
				const char *drv = gd ? SDL_GetGPUDeviceDriver(gd) : nullptr;
				if (drv && !strcmp(drv, "vulkan") && vk_first && !failed_before("direct3d12") && draws_3d(ren) == 0) {
					// (Vulkan chosen first draws nothing: Direct3D 12, as before)
					fprintf(stderr, "SDL's gpu renderer: Vulkan draws no 3D here: Direct3D 12 instead\n");
					cfg.gpu3d_failed += (cfg.gpu3d_failed.empty() ? "" : " ") + std::string("vulkan");
					cfg_dirty = true;
					SDL_DestroyRenderer(ren);
					SDL_ResetHint(SDL_HINT_GPU_DRIVER);
					ren = SDL_CreateRenderer(win, "gpu");
				} else if (drv && (!strcmp(drv, "direct3d12") || getenv("VF3_GPU3D_PROBE_ANY")) && draws_3d(ren) == 0 && !strcmp(drv, "direct3d12")) {
					fprintf(stderr, "SDL's gpu renderer: Direct3D 12 draws no 3D here: Vulkan instead\n");
					if (!failed_before("direct3d12")) { cfg.gpu3d_failed += (cfg.gpu3d_failed.empty() ? "" : " ") + std::string("direct3d12"); cfg_dirty = true; }
					SDL_DestroyRenderer(ren);
					SDL_SetHint(SDL_HINT_GPU_DRIVER, "vulkan");
					ren = SDL_CreateRenderer(win, "gpu");
					const int vk = ren ? draws_3d(ren) : -1;
					if (!ren || vk == 0) {   // (no Vulkan, or no 3D on it either: Direct3D 12 for the 2D, the 3D on the CPU)
						fprintf(stderr, "SDL's gpu renderer: Vulkan %s: Direct3D 12, the 3D on the CPU\n", ren ? "draws no 3D either" : "won't start");
						if (ren) SDL_DestroyRenderer(ren);
						if (!failed_before("vulkan")) { cfg.gpu3d_failed += " vulkan"; cfg_dirty = true; }
						SDL_ResetHint(SDL_HINT_GPU_DRIVER);
						ren = SDL_CreateRenderer(win, "gpu");
					} else fprintf(stderr, "SDL's gpu renderer: Vulkan draws the 3D here\n");
				}
			}
			SDL_RemovePath(marker.c_str());
			if (!ren) fprintf(stderr, "SDL's gpu renderer: %s (SDL's usual renderer, the CPU's 3D)\n", SDL_GetError());
		}
	}
	if (!ren) ren = SDL_CreateRenderer(win, nullptr);
	if (!ren) return fail(std::string("Couldn't start the graphics: ") + SDL_GetError());
	SDL_SetRenderVSync(ren, 1);
	fprintf(stderr, "renderer: %s\n", SDL_GetRendererName(ren));

	// ---- the picture ----
	// The picture's shape: the chosen one, or the window's (fit: between 4:3 and 48:9, three screens side by side)
	auto window_aspect = [&] {
		int w = 0, h = 0;
		SDL_GetWindowSizeInPixels(win, &w, &h);
		return h > 0 ? std::clamp(float(w) / float(h), 4.0f / 3.0f, 48.0f / 9.0f) : 16.0f / 9.0f;
	};
	auto picture_aspect = [&] { return ASPECTS[aspect_i] > 0 ? ASPECTS[aspect_i] : window_aspect(); };
	int scale = cfg.scale;       // in use (the governor may lower it for this run)
	if (!scale) {
		int pw = 0, ph = 0;
		SDL_GetWindowSizeInPixels(win, &pw, &ph);
		if (cfg.fullscreen)
			if (const SDL_DisplayMode *dm = SDL_GetDesktopDisplayMode(SDL_GetPrimaryDisplay())) {
				pw = int(dm->w * dm->pixel_density); ph = int(dm->h * dm->pixel_density);
			}
		const int cores = int(std::thread::hardware_concurrency());
		const int most = cores >= 8 ? 4 : cores >= 6 ? 3 : 2;   // (the first start's: 5x and 6x only when chosen)
		scale = std::clamp(int(std::ceil(ph / 384.0 - 0.1)), 2, most);   // (at least 2x, as VF1's and VF2's)
	}
	const int auto_scale = scale;   // the player's scale while settings.ini has none (0: worked out above)
	auto chosen_scale = [&] { return cfg.scale ? cfg.scale : auto_scale; };
	static video::Raster raster;
	// ---- the 3D on the GPU ----
	static R3dGpu gpu3d;
	struct { bool on = false, failed = false, checked = false; SDL_Texture *tex = nullptr; int w = 0, h = 0, k = 1; double cpu_ms = 0, gpu_ms = 0; } g3;
	if (r3d_mode != R3D_CPU) {
		const char *rn = SDL_GetRendererName(ren);
		SDL_GPUDevice *gd = rn && !strcmp(rn, "gpu") ? SDL_GetGPURendererDevice(ren) : nullptr;
		if (gd && failed_before(SDL_GetGPUDeviceDriver(gd))) {
			g3.failed = true;
			fprintf(stderr, "the 3D on the GPU: failed on %s here before (the CPU's 3D; choosing a 3D renderer in the menu tries again)\n", SDL_GetGPUDeviceDriver(gd));
		} else if (gd) {
			const char *name = SDL_GetStringProperty(SDL_GetGPUDeviceProperties(gd), SDL_PROP_GPU_DEVICE_NAME_STRING, nullptr);
			fprintf(stderr, "SDL_GPU: %s, %s\n", SDL_GetGPUDeviceDriver(gd), name ? name : "?");
			// (as VF1's: a "GPU" that is itself drawn by the CPU, a machine with no graphics driver, draws the 3D about 5
			// times slower than the CPU's own renderer, so Auto keeps the CPU there; the GPU chosen by hand still uses it)
			std::string lower = name ? name : "";
			for (char &c : lower) c = char(std::tolower((unsigned char)c));
			bool software = false;
			for (const char *k : { "llvmpipe", "lavapipe", "softpipe", "swiftshader", "software" }) software |= lower.find(k) != std::string::npos;
			const auto t0 = std::chrono::steady_clock::now();
			if (software && r3d_mode == R3D_AUTO)
				fprintf(stderr, "the 3D on the GPU: %s draws on the CPU, so Auto uses the CPU's 3D (faster here)\n", name);
			else if (gpu3d.init(gd)) {
				g3.on = true;
				fprintf(stderr, "the 3D on the GPU: started in %.0f ms (checked against the CPU's picture before it's trusted)\n",
						std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count());
			} else {
				g3.failed = true;
				fprintf(stderr, "the 3D on the GPU: %s (the CPU's 3D)\n", gpu3d.error().c_str());
			}
		} else fprintf(stderr, "the 3D on the GPU: no GPU renderer here (the CPU's 3D)\n");
	}
	fprintf(stderr, "the 3D: %s%s\n", R3D_NAMES[r3d_mode], getenv("VF3_3D") ? " (VF3_3D)" : "");
	// remember: the driver kept as failed for the next start (its start-up check, its textures); a picture failing
	// in play (a command buffer refused around a sleep or a GPU reset, once) only for this run: kept, a Steam Deck's
	// one bad moment put the 3D on the CPU at every start after, until the player changed it back
	auto g3_fail = [&](const std::string &why, bool remember = true) {
		fprintf(stderr, "the 3D on the GPU: %s: on the CPU from now on%s\n", why.c_str(), remember ? "" : " (this run)");
		g3.on = false;
		g3.failed = true;
		if (!remember) return;
		// the driver it failed on, kept: the next start tries SDL's other (Direct3D 12, then Vulkan)
		const char *rn = SDL_GetRendererName(ren);
		if (SDL_GPUDevice *gd = rn && !strcmp(rn, "gpu") ? SDL_GetGPURendererDevice(ren) : nullptr)
			if (const char *drv = SDL_GetGPUDeviceDriver(gd); drv && !failed_before(drv)) {
				cfg.gpu3d_failed += (cfg.gpu3d_failed.empty() ? "" : " ") + std::string(drv);
				cfg_dirty = true;
				if (!strcmp(drv, "direct3d12") && !failed_before("vulkan")) fprintf(stderr, "the 3D on the GPU: the next start tries Vulkan\n");
			}
	};
	static video::Scene between;
	SDL_Texture *tex3d = nullptr, *tex_below = nullptr, *tex_above = nullptr;
	bool g3_shown = false;   // the picture on show is the GPU's
	// ---- THE HUD PUSHED OUT TO THE SCREEN'S EDGES IN WIDESCREEN ----
	//
	// Frédéric asked for it the 2026-10-25, with a picture he had cut and pasted by hand: in widescreen the
	// life bars, the names, the armour icons, the round marks and the texts along the foot should sit at the
	// EDGES of the window, where they sit at the edges of a 4:3 screen -- and the timer should stay where it
	// is, in the middle. Symmetrically.
	//
	// AND THE CUTS ARE MEASURED IN THE LAYER, not written down as rectangles. A rectangle is right for one
	// game, one screen and one revision; what is true of all of them is that the HUD HANGS OFF THE EDGES --
	// this port's own note says so, "Fighting Vipers 2 puts a player's icon hard against each edge above the
	// health" -- and that something centred (the timer, ROUND 1, SUPER K.O.) does not. So each band is read:
	// the block that reaches the left edge ends at the first gap after it, and that gap is where the picture
	// is cut; the same on the right. Nothing reaching an edge means nothing moves, which is what happens on
	// the select screen, a fade, or a replay whose only text is centred.
	//
	// Measured on a versus, frame 2000 (VF3_HUD_AUDIT): the top band reads 0..28, 35..216, 222..269,
	// 275..456, 463..491 -- the two player icons hard against the edges, the two bars, and the timer at
	// 222..269, whose middle is 245.5 against the picture's 248. The gaps at 217..221 and 270..274 are the
	// two cuts, and they are 29 and 26 from the middle: symmetric to within a pixel and a half.
	// The two cuts, in the layer's own columns (0 and 0 means "do not move"), and what each of the three
	// pieces ACTUALLY covers -- its first and last column with something on it. The cut is a gap and the ink
	// is not in the middle of it, so a block placed by its slice would not be placed by what is seen.
	struct HudCut {
		int left = 0, right = 0;
		int l0 = -1, l1 = -1, c0 = -1, c1 = -1, r0 = -1, r1 = -1;
	};
	HudCut hud_top, hud_foot;
	bool hud_hide_mid = false;   // training: the stopped clock is left undrawn (see where it is set)
	// HOW DEEP THE TOP BAND GOES, and it is measured, not rounded. At 100 it cut a drawing in half:
	// under the right-hand bar there is a mark that comes and goes during a fight -- Frederic calls it
	// the joystick's animation -- and the audit of a training fight has it at rows 93..108, columns
	// 423..452. Its top half was spread out to the window's edge with the band and its bottom half stayed
	// where 4:3 had it. 120 takes the whole of it and still stops well short of the ROUND X announcement,
	// which the same audit puts at rows 125..254.
	static constexpr int HUD_TOP_ROWS = 120;      // the band the bars, the names and that mark live in
	static constexpr int HUD_FOOT_ROWS = 60;      // the band the stage and the time live in
	// How near an edge a block has to begin to count as hanging off it. Twenty-four columns, about a
	// twentieth of the picture, and the number is measured: the player icons begin at column 0, but the
	// foot's STAGE 1 begins at 16 and its time ends at 476, so at eight the foot never moved at all.
	static constexpr int HUD_EDGE = 24;
	// THE TIMER'S FIELD, measured and FIXED, and that is the whole correction of the 2026-10-25. The cut
	// used to be a gap looked for in the picture frame by frame, and the timer's own two digits have a gap
	// between them exactly as wide as the gaps that frame the field -- five columns. At "0 1" the cut fell
	// BETWEEN THE DIGITS: the 0 went out to the left and the 1 to the right, and the clock read as two
	// numbers on opposite sides of the screen. Frederic photographed it.
	//
	// Geometry cannot tell those two gaps apart, because they are the same width. What tells them apart is
	// that the field does not move: over ninety frames of a fight the band reads 0..28, 35..216, 222..269,
	// 275..456, 463..491 and the middle run never budges, whatever the clock says. So the field is written
	// down once -- the two gaps 217..221 and 270..274 are the cuts -- and what the timer draws inside it is
	// no longer anyone's business.
	static constexpr int HUD_MID_L = 217, HUD_MID_R = 275;
	// The Dreamcast's ending, its pictures standing in for the roll's replays (fv2_fin.h). Declared here
	// with the renderer's own state because the picture is composed where the 3D and the 2D are.
	fv2_fin fin;
	uint32_t side_colour[2] = { 0xFF000000u, 0xFF000000u };   // widescreen's bars beside the 2D (left, right)
	bool sky_wide = false;
	float bg_shift_x = 0, bg_shift_y = 0;   // the layers below the 3D moved back towards the frame before (draw_below)
	// the layers below the 3D across the picture's width (VF3_WIDE_SKY=0: never)
	const bool wide_sky = !getenv("VF3_WIDE_SKY") || atoi(getenv("VF3_WIDE_SKY")) != 0;
	// Widescreen's sides where the 2D over the 3D is opaque at the 4:3 edge (a close-up's frame, the title): that
	// layer's edge carried on across the side, over the 3D (the game draws its stage behind such screens, hidden in
	// 4:3); runs of rows of one colour
	struct SideRun { int y0, y1; uint32_t c; };
	std::vector<SideRun> side_cover[2];
	// what the pictures cost, for the log: a line every 10 s (2 s with VF3_PACE_LOG)
	struct { uint64_t t0 = 0; int pictures = 0, frames = 0; double draw_ms = 0, emu_ms = 0, upload_ms = 0, gpu_ms = 0, stopped_s = 0; int slow[4] = {}; double slow_work_ms = 0, prep_ms = 0, g3_wall_ms = 0, interp_ms = 0; } pace;   // (slow: passes over 1.5 refreshes, by what they ran: nothing but a picture, a frame's first half, its second, a whole frame)
	uint64_t between_n = 0, cut_n = 0;   // smooth motion's pictures between frames, and those shown as the frame (a cut)
	int tex_w = 0, tex_h = 0;
	// HD 2D: the tile plane's two textures are the scale times the hardware's size, and the layers sent into
	// them are the HD ones. Everything else about them -- blending, filtering, where they are drawn -- is as
	// before, and at scale 1 these are the old sizes to the pixel.
	const int tile_s = game.tiles.hd() ? game.tiles.hd_scale() : 1;
	auto make_2d = [&](SDL_Texture *&t, int w) {
		t = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING, w * tile_s, video::H * tile_s);
		SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
		SDL_SetTextureScaleMode(t, SDL_SCALEMODE_NEAREST);
	};
	make_2d(tex_below, video::WW);
	SDL_SetTextureScaleMode(tex_below, cfg.smooth_bg ? SDL_SCALEMODE_LINEAR : SDL_SCALEMODE_NEAREST);
	video::Raster::arcade_lod = !cfg.sharp_textures;
	make_2d(tex_above, video::W);
	// A picture into a streaming texture: on Direct3D 11 into the texture itself, mapped (d3d11_map.h: SDL's
	// lock and update would make a new staging texture each time), else locked; write(pixels, pitch)
	auto send = [&](SDL_Texture *t, auto &&write) {
		void *p = nullptr;
		int pitch = 0;
		if (d3d11_map(t, &p, &pitch)) { write(static_cast<uint8_t *>(p), size_t(pitch)); d3d11_unmap(t); }
		else if (SDL_LockTexture(t, nullptr, &p, &pitch)) { write(static_cast<uint8_t *>(p), size_t(pitch)); SDL_UnlockTexture(t); }
	};
	auto send_2d = [&](SDL_Texture *t, const std::vector<uint32_t> &src, int w) {
		const size_t rw = size_t(w) * tile_s, rows = size_t(video::H) * tile_s;
		if (src.size() < rw * rows) return;   // (a frame before the game's first: the layers not made yet)
		send(t, [&](uint8_t *p, size_t pitch) {
			for (size_t y = 0; y < rows; y++) memcpy(p + y * pitch, src.data() + y * rw, rw * 4);
		});
	};
	// the 3D's size: 384 lines a scale, as wide as the picture's shape
	// antialiasing in use: the player's, less where the picture drawn would be over 8 times the arcade's each way
	int gov_aa_cut = 0;   // the governor's: steps below the player's antialiasing (cut before the scale)
	auto aa_now = [&] {
		int k = std::clamp(cfg.aa - gov_aa_cut, 1, 3);
		while (k > 1 && scale * k > 8) k--;
		return k;
	};
	// the picture's size as shown (the 3D's drawn aa_now() times that each way)
	auto size_3d = [&](int &w, int &h, int &gw) {
		h = 384 * scale;
		w = int(std::lround(h * picture_aspect())) & ~1;
		gw = int(std::lround(h * 4.0 / 3.0));
		w = std::max(w, gw);
	};
	auto ensure_3d = [&]() -> bool {
		int w, h, gw;
		size_3d(w, h, gw);
		raster.resize(w * aa_now(), h * aa_now(), gw * aa_now());
		if (tex3d && tex_w == w && tex_h == h) return true;
		if (tex3d) SDL_DestroyTexture(tex3d);
		for (;;) {
			size_3d(w, h, gw);
			tex3d = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING, w, h);
			if (tex3d || scale <= 1) break;
			fprintf(stderr, "a %d x %d picture is too large here (%s): trying %dx\n", w, h, SDL_GetError(), scale - 1);
			scale--;
		}
		if (!tex3d) return false;
		raster.resize(w * aa_now(), h * aa_now(), gw * aa_now());
		SDL_SetTextureBlendMode(tex3d, SDL_BLENDMODE_BLEND_PREMULTIPLIED);
		SDL_SetTextureScaleMode(tex3d, SDL_SCALEMODE_LINEAR);
		tex_w = w; tex_h = h;
		return true;
	};
	if (!ensure_3d()) { gpu3d.shutdown(); return fail("Couldn't make the picture's texture"); }   // (the GPU's objects let go before the renderer)
	// a picture drawn: the 3D (smooth motion: between the last two frames, t from the older) and the 2D layers
	double test_load_ms = 0, test_load_s = 0;
	if (const char *tl = getenv("VF3_TEST_LOAD")) sscanf(tl, "%lf:%lf", &test_load_ms, &test_load_s);
	// the GPU's output, as a texture of the renderer's (made again at a new size)
	auto g3_texture = [&]() -> bool {
		const int k = aa_now(), ow = raster.width() / k, oh = raster.height() / k;
		if (g3.tex && g3.w == ow && g3.h == oh && g3.k == k) return true;
		if (g3.tex) { SDL_DestroyTexture(g3.tex); g3.tex = nullptr; }
		SDL_GPUTexture *gt = gpu3d.output(raster.width(), raster.height(), k);
		if (!gt) { g3_fail(gpu3d.error()); return false; }
		SDL_PropertiesID pr = SDL_CreateProperties();
		SDL_SetNumberProperty(pr, SDL_PROP_TEXTURE_CREATE_FORMAT_NUMBER, SDL_PIXELFORMAT_ABGR8888);
		SDL_SetNumberProperty(pr, SDL_PROP_TEXTURE_CREATE_ACCESS_NUMBER, SDL_TEXTUREACCESS_STATIC);
		SDL_SetNumberProperty(pr, SDL_PROP_TEXTURE_CREATE_WIDTH_NUMBER, ow);
		SDL_SetNumberProperty(pr, SDL_PROP_TEXTURE_CREATE_HEIGHT_NUMBER, oh);
		SDL_SetPointerProperty(pr, SDL_PROP_TEXTURE_CREATE_GPU_TEXTURE_POINTER, gt);
		g3.tex = SDL_CreateTextureWithProperties(ren, pr);
		SDL_DestroyProperties(pr);
		if (!g3.tex) { g3_fail(std::string("the renderer won't show its pictures: ") + SDL_GetError()); return false; }
		SDL_SetTextureBlendMode(g3.tex, SDL_BLENDMODE_BLEND_PREMULTIPLIED);
		SDL_SetTextureScaleMode(g3.tex, SDL_SCALEMODE_LINEAR);
		g3.w = ow; g3.h = oh; g3.k = k;
		return true;
	};
	// The start's check: the GPU's picture of the game's own scene against the CPU's. Judged by how wrong, not how
	// many (as VF2's): colours off by a little, or edges a hair apart, anywhere, are a driver's rounding (NVIDIA's on
	// Direct3D 12 puts pixels a hair off); only a picture plainly wrong goes to the CPU: over 1% of it with colours off
	// by more than GPU_ROUNDING_COLOUR, or over 5% drawn on one and not the other (a GPU drawing nothing). When the
	// player chose the GPU (not Auto): 10% and 25%.
	constexpr long long GPU_ROUNDING_PX = 64;
	constexpr int GPU_ROUNDING_COLOUR = 8;
	constexpr double GPU_WRONG_COLOUR = 0.01, GPU_WRONG_COVER = 0.05, GPU_WRONG_COLOUR_CHOSEN = 0.10, GPU_WRONG_COVER_CHOSEN = 0.25;
	auto g3_check = [&](const video::Scene &s) -> bool {
		std::vector<uint32_t> gp;
		if (!gpu3d.read_output(gp)) { g3_fail("its picture couldn't be read back"); return false; }
		raster.render(s, game.view.tex.data());
		const int k = aa_now(), cw = raster.width() / k, ch = raster.height() / k;
		std::vector<uint32_t> cp(size_t(cw) * ch);
		raster.to_premultiplied(reinterpret_cast<uint8_t *>(cp.data()), size_t(cw) * 4, k);
		long long n = 0, drawn = 0, cover = 0, big = 0;
		int most = 0;
		for (size_t i = 0; i < cp.size(); i++) {
			if (cp[i] >> 24) drawn++;
			if (cp[i] == gp[i]) continue;
			n++;
			if (((cp[i] >> 24) == 0) != ((gp[i] >> 24) == 0)) { cover++; continue; }
			int d = 0;
			for (int sh = 0; sh < 32; sh += 8) d = std::max(d, std::abs(int((cp[i] >> sh) & 255) - int((gp[i] >> sh) & 255)));
			if (d > GPU_ROUNDING_COLOUR) big++;
			most = std::max(most, d);
		}
		const long long px = (long long)cp.size();
		const bool chosen = r3d_mode == R3D_GPU;
		const long long ok = std::max(GPU_ROUNDING_PX, (long long)(px * (chosen ? GPU_WRONG_COLOUR_CHOSEN : GPU_WRONG_COLOUR)));
		const long long ok_cover = std::max(GPU_ROUNDING_PX, (long long)(px * (chosen ? GPU_WRONG_COVER_CHOSEN : GPU_WRONG_COVER)));
		const bool keep = big <= ok && cover <= ok_cover;
		fprintf(stderr, "the 3D on the GPU: its picture (%d x %d, %lld pixels of 3D) against the CPU's: %lld pixels differ (%lld drawn on one only, %lld by more than %d, colours by %d at most; up to %lld and %lld kept): %s\n",
				cw, ch, drawn, n, cover, big, GPU_ROUNDING_COLOUR, most, ok_cover, ok, keep ? "the GPU's" : "the CPU's");
		if (getenv("VF3_GPU_CHECK_DIR")) {
			const std::string d = getenv("VF3_GPU_CHECK_DIR");
			write_png((d + "/gpu.png").c_str(), gp.data(), cw, ch);
			write_png((d + "/cpu.png").c_str(), cp.data(), cw, ch);
		}
		if (!keep) { g3_fail("its pictures aren't the CPU's"); return false; }
		return true;
	};
	double picture_fixed_ms = 0;   // the last picture's in-between and triangles prepared (the governor's)
	auto draw_picture = [&](double t, bool new_2d) {
		ensure_3d();
		const double fixed_before = pace.interp_ms + pace.prep_ms;
		picture_fixed_ms = 0;
		// the stage's sky where the 3D camera is now, not where it was 3 frames before (sky_lead.h)
		bg_shift_x = -game.view.sky.at_x(1);
		bg_shift_y = -game.view.sky.at_y(1);
		const video::Scene *s = &game.view.cur();
		// (at t = 1 too with smooth motion: the frame itself, but for a flipbook's step still fading in)
		if ((t < 1.0 || cfg.smooth) && game.view.have_prev) {
			const auto ti = std::chrono::steady_clock::now();
			const bool blended = video::interpolate(game.view.prev(), game.view.cur(), float(t), between);
			pace.interp_ms += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - ti).count();
			picture_fixed_ms = pace.interp_ms - fixed_before;
			if (blended) {
				s = &between;
				between_n++;
				if (game.view.bg_moved) { bg_shift_x = float(1 - t) * game.view.bg_dx; bg_shift_y = float(1 - t) * game.view.bg_dy; }
				else bg_shift_x = bg_shift_y = 0;
				bg_shift_x -= game.view.sky.at_x(float(t));
				bg_shift_y -= game.view.sky.at_y(float(t));
			}
			else cut_n++;
		}
		bool on_gpu = false;
		// (not before the first frame's copy: with no sound to wait on, the first picture can come before any frame,
		// and the GPU's first upload is the whole texture memory, read from that copy)
		if (g3.on && g3_texture() && !game.view.tex.empty()) {
			const auto tg = std::chrono::steady_clock::now();
			raster.prepare(*s);
			// the texture pack's place for each polygon, which the GPU renderer reads out of `raster` as the CPU
			// rasteriser does (tex3d_hd.h); nothing at all when no pack and no dump were asked for
			raster.tex_version = 1 + game.view.tex_serial;
			raster.resolve_hd(game.view.tex.data());
			const auto tp = std::chrono::steady_clock::now();
			pace.prep_ms += std::chrono::duration<double, std::milli>(tp - tg).count();
			picture_fixed_ms = pace.interp_ms + pace.prep_ms - fixed_before;
			const bool rendered = gpu3d.render(*s, raster, game.view.tex, game.view.uploads);
			pace.g3_wall_ms += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - tp).count();
			if (!rendered) g3_fail(gpu3d.error(), false);
			else {
				on_gpu = true;
				g3.gpu_ms += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - tg).count();
				// checked once, on a scene with enough in it
				size_t polys = raster.tris.size();
				if (!g3.checked && polys > 500) { g3.checked = true; on_gpu = g3_check(*s); }
				// (checked: the CPU's drawing buffers let go, made again if it ever draws: at 2x with 3x antialiasing on
				// a Steam Deck they were near 500 MB of its shared memory, kept for nothing)
				if (on_gpu && g3.checked) raster.drop_buffers();
			}
		}
		if (!on_gpu) {
			game.view.uploads.clear();
			raster.tex_version = 1 + game.view.tex_serial;
			raster.render(*s, game.view.tex.data());
			const uint64_t tu = SDL_GetTicksNS();
			send(tex3d, [&](uint8_t *p, size_t pitch) { raster.to_premultiplied(p, pitch, aa_now()); });
			pace.upload_ms += double(SDL_GetTicksNS() - tu) / 1e6;
		} else pace.gpu_ms += gpu3d.last_cpu_ms;
		g3_shown = on_gpu;
		// (tests: VF3_VIEW_LOG=frame,frame,... prints every viewport of those pictures with the light the game
		// put on it -- its ambient, its sun and how strong, and how many models are in it. For telling a scene
		// that is lit differently from one that is drawn differently, which a count of models cannot.)
		if (const char *vl = getenv("VF3_VIEW_LOG");
			vl && strstr((std::string(",") + vl + ",").c_str(), ("," + std::to_string(game_frame) + ",").c_str())) {
			fprintf(stderr, "views at frame %llu: %zu, mode %d sub %d\n", (unsigned long long)game_frame,
					s->viewports.size(), board.fv2_mode(), board.fv2_submode());
			for (size_t i = 0; i < s->viewports.size(); i++) {
				const auto &v = s->viewports[i];
				// The models' addresses as well, because a count cannot say WHAT is in a viewport and the
				// fighters and the scenery do not come from the same place. Four is enough to tell a family.
				std::string addrs;
				for (size_t k = 0; k < v.models.size() && k < 4; k++) {
					char t[16];
					snprintf(t, sizeof t, " %06X", v.models[k].addr);
					addrs += t;
				}
				fprintf(stderr, "  view %2zu  node %06X  ambient %.3f  sun %.3f (%.2f %.2f %.2f)  models %zu%s\n",
						i, v.node, v.ambient, v.sun_intensity, v.sun[0], v.sun[1], v.sun[2],
						v.models.size(), addrs.c_str());
			}
		}
		if (static const bool pic_log = getenv("VF3_PIC_LOG") != nullptr; pic_log) {   // (tests: each picture's makings)
			size_t models = 0;
			for (auto &v : s->viewports) models += v.models.size();
			fprintf(stderr, "pic f %llu t %.2f blended %d models %zu vps %zu full %d sky %d side %08x blank %d mode %d sub %d\n",
			        (unsigned long long)game_frame, t,
			        int(s == &between), models, s->viewports.size(), int(game.view.full_3d), int(sky_wide), side_colour[0], int(s->blank),
			        board.fv2_mode(), board.fv2_submode());
		}
		// (tests: VF3_TIMER_LOG=1 follows the match's own clock -- the cooked word the round starts from
		// (0x00581418 for one player, 0x0058141C for two) and what is left of it (0x00581404) -- so that a
		// ROUND TIME set on the OPTIONS page can be seen arriving in a fight rather than taken on trust.
		// It reads the board and writes nothing; docs/fv2-console-options.md keeps the measurement.)
		if (static const bool timer_log = getenv("VF3_TIMER_LOG") != nullptr; timer_log) {
			static int was = -1;
			const int left = board.fv2_option(0x00581404, 4);
			if (left != was) {
				fprintf(stderr, "timer f %llu: %d left (%.1f s), the round starts from %d (1P) / %d (2P)\n",
						(unsigned long long)game_frame, left, double(left) / 64.0, board.fv2_option(0x00581418, 4) / 64,
						board.fv2_option(0x0058141C, 4) / 64);
				was = left;
			}
		}
		if (new_2d) {
			// A stage: the 3D full width and the 2D over it clear of the 4:3 edges (a fight's HUD never reaches them)
			// covering under half of the rest (a fight's HUD and a replay's under 10%, the ranking's a third; the
			// close-ups' and the next match's pictures, 2D over a hidden stage, all of it). Rows opaque all the way
			// across are a replay's letterbox bands: they don't count.
			int edge_rows = 0, rows = 0;
			long covered = 0;
			bool band[video::H];
			for (int y = 0; y < video::H; y++) {
				const uint32_t *row = &game.view.above[size_t(y) * video::W];
				int c = 0;
				for (int x = 0; x < video::W; x++) c += (row[x] >> 24) != 0;
				band[y] = c == video::W;
				if (band[y]) continue;
				rows++;
				covered += c;
				edge_rows += (row[0] >> 24) != 0 || (row[video::W - 1] >> 24) != 0;
			}
			bool stage = game.view.full_3d && rows > 0 && edge_rows * game.stage_edge_divisor < video::H && covered * 100 < long(rows) * video::W * 50;
			// The two cuts, read in the layer, band by band (see HudCut above for what they are for).
			{
				auto cuts = [&](int y0, int y1) {
					HudCut c;
					bool on[video::W];
					for (int x = 0; x < video::W; x++) {
						bool any = false;
						for (int y = y0; y < y1 && !any; y++) any = (game.view.above[size_t(y) * video::W + x] >> 24) != 0;
						on[x] = any;
					}
					// Something has to hang off each edge, or there is nothing to push out: that is what
					// leaves the select screen, a fade and the STAGE / VS banner exactly where they are.
					bool at_edge = false, at_far = false;
					for (int x = 0; x < HUD_EDGE; x++) at_edge = at_edge || on[x];
					for (int x = video::W - HUD_EDGE; x < video::W; x++) at_far = at_far || on[x];
					if (!at_edge || !at_far) return HudCut();
					// AND THE TWO CUTS HAVE TO BE EMPTY. Anything crossing them is one drawing spanning the
					// field, and cutting a drawing in three is exactly the fault this is here to prevent.
					for (int x = HUD_MID_L; x < HUD_MID_L + 5; x++) if (on[x]) return HudCut();
					for (int x = HUD_MID_R - 5; x < HUD_MID_R; x++) if (on[x]) return HudCut();
					c.left = HUD_MID_L;
					c.right = HUD_MID_R;
					// What each piece covers, which is what gets placed
					auto ink = [&](int x0, int x1, int &a, int &b) {
						a = b = -1;
						for (int x = x0; x < x1; x++) if (on[x]) { if (a < 0) a = x; b = x; }
					};
					ink(0, c.left, c.l0, c.l1);
					ink(c.left, c.right, c.c0, c.c1);
					ink(c.right, video::W, c.r0, c.r1);
					if (c.l0 < 0 || c.r0 < 0) return HudCut();   // nothing to move on one side
					return c;
				};
				// FIGHTING VIPERS 2 ONLY, for now, and not because the rule is particular to it: nothing
				// above names a game, and Virtua Fighter 3 hangs its HUD off the edges too. It is that VF3
				// is this port's reference and its picture is not changed unasked. Frédéric asked for this
				// one; the other has only to be asked for.
				// AND ONLY DURING A FIGHT. The select screen reaches both edges too, and its top band is
				// empty exactly where the timer's field is, so it was being cut and spread as well -- its
				// row of portraits came out with a piece of itself repeated against the right edge,
				// photographed the 2026-10-25. The HUD belongs to GAME, Sega's modes 6 and 7, and to no
				// other screen.
				const int gmode = board.fv2_mode();
				const bool mine = board.set == "fvipers2" && (gmode == 6 || gmode == 7);
				hud_top = mine ? cuts(0, HUD_TOP_ROWS) : HudCut();
				hud_foot = mine ? cuts(video::H - HUD_FOOT_ROWS, video::H) : HudCut();
				// THE CLOCK IS NOT SHOWN IN TRAINING. It is stopped there, so what it shows is a number that
				// never changes; Frederic asked for it gone the 2026-10-25. Nothing of the game is touched --
				// the middle slice of the top band, which is the timer's own field and nothing else, is simply
				// not drawn. Only during a fight, or the select screen would lose a strip of itself.
				hud_hide_mid = mine && cplay.training_on;
			}
			// VF3_HUD_AUDIT=1: WHERE THE 2D ACTUALLY IS, row band by row band and column run by column run.
			// Written to spread Fighting Vipers 2's HUD in widescreen -- the life bars and what hangs off them
			// out to the window's edges, the timer left where it is -- because the rectangles that move have
			// to be measured on the picture and not guessed from a screenshot's pixels.
			if (static const char *ha = getenv("VF3_HUD_AUDIT"); ha && game_frame % std::max<uint64_t>(1, strtoull(ha, nullptr, 10)) == 0) {
				fprintf(stderr, "hud frame %llu (mode %d sub %d)\n", (unsigned long long)game_frame,
						board.fv2_mode(), board.fv2_submode());
				auto runs = [&](int y0, int y1, const char *what) {
					std::string s;
					int from = -1;
					for (int x = 0; x <= video::W; x++) {
						bool any = false;
						for (int y = y0; y < y1 && !any; y++) any = (game.view.above[size_t(y) * video::W + x % video::W] >> 24) != 0;
						if (x == video::W) any = false;
						if (any && from < 0) from = x;
						else if (!any && from >= 0) { s += " " + std::to_string(from) + ".." + std::to_string(x - 1); from = -1; }
					}
					fprintf(stderr, "hud %s rows %d..%d:%s\n", what, y0, y1 - 1, s.empty() ? " (nothing)" : s.c_str());
				};
				// the row bands that carry any 2D at all, so nothing is assumed about where the HUD sits
				{
					std::string b;
					int from = -1;
					for (int y = 0; y <= video::H; y++) {
						int c = 0;
						if (y < video::H)
							for (int x = 0; x < video::W; x++) c += (game.view.above[size_t(y) * video::W + x] >> 24) != 0;
						if (c && from < 0) from = y;
						else if (!c && from >= 0) { b += " " + std::to_string(from) + ".." + std::to_string(y - 1); from = -1; }
					}
					fprintf(stderr, "hud bands:%s\n", b.empty() ? " (nothing)" : b.c_str());
				}
				runs(0, 100, "top");
				// and what lies BETWEEN the two bands, which is where ROUND X is announced: rows 125..254, well
				// clear of the HUD. Added the 2026-10-25 to answer where that text sits across the picture.
				runs(100, video::H - 60, "middle");
				runs(video::H - 60, video::H, "bottom");
			}
			// VF3_STAGE_AUDIT=1: what the test above saw, for fitting it to a game whose HUD sits elsewhere
			if (getenv("VF3_STAGE_AUDIT"))
				fprintf(stderr, "stage %d: full_3d %d rows %d edge_rows %d (limit %d) covered %.1f%% (limit 50%%)\n",
				        int(stage), int(game.view.full_3d), rows, edge_rows, video::H / game.stage_edge_divisor,
				        rows ? 100.0 * double(covered) / (double(rows) * video::W) : 0.0);
			// a stage stays one through a round's opening wipe and the like (2D over part of it for under 20 frames);
			// a picture over all of it (the next match's, a close-up) or the 3D gone ends it at once
			static int stage_hold = 0;
			if (stage) stage_hold = 20;
			else if (stage_hold > 0 && game.view.full_3d && covered * 100 < long(std::max(rows, 1)) * video::W * 50) { stage_hold--; stage = true; }
			else stage_hold = 0;
			// On a stage the sky is stretched across the picture and the letterbox bands carried across the sides;
			// elsewhere the picture is 4:3 with bars the full height over whatever is beside it: the 2D's edge colour
			// where that is one colour all the way down (the SEGA logo's white, a fade), else black
			sky_wide = stage && wide_sky;
			for (int s = 0; s < 2; s++) {
				const int x = s ? video::W - 1 : 0;
				auto at = [&](int y) {
					const uint32_t a = game.view.above[size_t(y) * video::W + x], b = game.view.below[size_t(y) * video::W + x];
					return (a >> 24) ? a : (b >> 24) ? b : 0xFF000000u;
				};
				side_colour[s] = 0xFF000000u;
				side_cover[s].clear();
				if (stage) {
					for (int y = 0; y < video::H; y++) {
						if (!band[y]) continue;
						const uint32_t c = game.view.above[size_t(y) * video::W + x];
						if (!side_cover[s].empty() && side_cover[s].back().y1 == y && side_cover[s].back().c == c) side_cover[s].back().y1++;
						else side_cover[s].push_back({ y, y + 1, c });
					}
				} else {
					const uint32_t c0 = at(0);
					bool one = true;
					for (int y = 1; y < video::H && one; y++) one = at(y) == c0;
					side_colour[s] = one ? c0 : 0xFF000000u;
					side_cover[s].push_back({ 0, video::H, side_colour[s] });
				}
			}
			const uint64_t tu = SDL_GetTicksNS();
			send_2d(tex_below, game.view.below_wide_hd.empty() ? game.view.below_wide : game.view.below_wide_hd, video::WW);
			send_2d(tex_above, game.view.above_hd.empty() ? game.view.above : game.view.above_hd, video::W);
			pace.upload_ms += double(SDL_GetTicksNS() - tu) / 1e6;
		}
	};
	// Where the picture goes in the window: as large as fits, its shape kept
	auto picture_rect = [&](SDL_FRect &all, SDL_FRect &mid) {
		int pw = 0, ph = 0;
		SDL_GetRenderOutputSize(ren, &pw, &ph);
		const float a = picture_aspect();
		float w = float(pw), h = float(ph);
		if (w / h > a) w = h * a; else h = w / a;
		all = { (pw - w) / 2, (ph - h) / 2, w, h };
		const float mw = h * 4.0f / 3.0f;
		mid = { all.x + (w - mw) / 2, all.y, mw, h };
	};
	// The layers below the 3D (the sky, the city). Between frames (smooth motion) moved on from the frame before
	// as far as the picture is between them, to the fraction of a pixel, as the 3D is (bg_shift: in its pixels).
	// Widescreen, on a stage: across the whole picture, 1:1 (turning with the 3D), the panorama carried on past the
	// 4:3 screen's edges (tilegen.h); stretched only past the widened layer's own edges (wider than about 2.6:1).
	// Else 4:3.
	auto draw_below = [&](const SDL_FRect &all, const SDL_FRect &mid) {
		const float k = mid.w / video::W;
		// the picture's sides, in the layer's pixels from the 4:3 screen's left
		const float l = sky_wide ? (all.x - mid.x) / k : 0.0f, r = sky_wide ? (all.x + all.w - mid.x) / k : float(video::W);
		const float u0 = std::max(video::EXT + l - bg_shift_x, 0.0f), u1 = std::min(video::EXT + r - bg_shift_x, float(video::WW));
		const float x0 = mid.x + l * k, x1 = mid.x + r * k;
		// Moved down by bg_shift_y (or up): the band it leaves at the top (or bottom) its edge row's, carried on (the
		// sky's: a camera tilting fast, a jump attack's, moved it further than the 12 rows that were stretched to cover
		// it, and left a bar of black)
		const float sy = std::clamp(bg_shift_y, -float(video::H) + 1, float(video::H) - 1);
		auto band = [&](float v0, float v1, float d0, float d1) {
			if (v1 - v0 < 0.01f || d1 - d0 < 0.01f) return;
			const float ts = float(tile_s);   // (the source is in the texture's own pixels: the scale times the layer's)
			const SDL_FRect src = { u0 * ts, v0 * ts, (u1 - u0) * ts, (v1 - v0) * ts };
			const SDL_FRect dst = { x0, mid.y + d0 * k, x1 - x0, (d1 - d0) * k };
			SDL_RenderTexture(ren, tex_below, &src, &dst);
		};
		if (sy > 0) {
			band(0, 1, 0, sy);
			band(0, float(video::H) - sy, sy, float(video::H));
		} else {
			band(-sy, float(video::H), 0, float(video::H) + sy);
			band(float(video::H) - 1, float(video::H), float(video::H) + sy, float(video::H));
		}
	};
	// Screen filters (as VF2's): a picture of the monitor's glass multiplied over the game's, at the window's own
	// pixels (made again when the picture's size changes). Scanlines darkens between the arcade's 384 lines; CRT adds
	// an aperture grille (red, green and blue stripes) and slightly darker corners.
	static const char *FILTER_NAMES[] = { "Off", "Scanlines", "CRT" };
	SDL_Texture *glass = nullptr;
	int glass_w = 0, glass_h = 0, glass_kind = 0;
	auto draw_filter = [&](const SDL_FRect &r) {
		if (!cfg.filter) return;
		const int w = int(std::lround(r.w)), h = int(std::lround(r.h));
		if (w < 1 || h < 1) return;
		if (!glass || w != glass_w || h != glass_h || cfg.filter != glass_kind) {
			if (glass) SDL_DestroyTexture(glass);
			glass = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, w, h);
			if (!glass) return;
			glass_w = w; glass_h = h; glass_kind = cfg.filter;
			const bool crt = cfg.filter == 2;
			// the lines want a couple of pixels each to show: they fade in from 1.5 to 3 per line
			const float per_line = h / 384.0f;
			const float depth = (crt ? 0.45f : 0.6f) * std::clamp((per_line - 1.5f) / 1.5f, 0.0f, 1.0f);
			// a whole number of screen pixels a line (the picture's lines to the nearest), so every line looks the
			// same: a fraction of a line apart (the Steam Deck's 800 lines: 2.08 a line) beats into uneven bands
			const int period = std::max(1, int(std::lround(per_line)));
			std::vector<float> row(static_cast<size_t>(h));
			for (int y = 0; y < h; y++) {
				const float f = period == 2 ? float(y % 2) * 0.5f : (float(y % period) + 0.5f) / float(period);
				row[size_t(y)] = 1 - depth * (1 - std::sin(f * 3.14159265f));   // 1 mid-line, darker at its edges
			}
			auto glass_at = [&](int x, int y, float rgb[3]) {
				rgb[0] = rgb[1] = rgb[2] = row[size_t(y)];
				if (!crt) return;
				for (int c = 0; c < 3; c++) if (c != x % 3) rgb[c] *= 0.78f;   // the grille: one colour per pixel column
				const float dx = (x + 0.5f) / w * 2 - 1, dy = (y + 0.5f) / h * 2 - 1;
				const float d = std::min(1.0f, (dx * dx + dy * dy) / 2);
				for (int c = 0; c < 3; c++) rgb[c] *= 1 - 0.22f * d * d;      // the corners
			};
			// Multiplying only darkens: drawn as twice the product (dst * glass * 2), the glass also brightens the
			// middle of each line, so the picture keeps about its brightness
			const SDL_BlendMode times2 = SDL_ComposeCustomBlendMode(SDL_BLENDFACTOR_DST_COLOR, SDL_BLENDFACTOR_SRC_COLOR,
				SDL_BLENDOPERATION_ADD, SDL_BLENDFACTOR_ZERO, SDL_BLENDFACTOR_ONE, SDL_BLENDOPERATION_ADD);
			const bool doubled = SDL_SetTextureBlendMode(glass, times2);
			if (!doubled) SDL_SetTextureBlendMode(glass, SDL_BLENDMODE_MOD);   // a renderer without it: darker
			float gain = 1;
			if (doubled) {
				double sum = 0;
				int n = 0;
				for (int y = 0; y < h; y += 3)
					for (int x = 0; x < w; x += 7) { float v[3]; glass_at(x, y, v); sum += v[0] + v[1] + v[2]; n += 3; }
				gain = std::min(1.4f, float(n / sum));
			}
			const float k = 255 * gain / (doubled ? 2 : 1);
			std::vector<uint32_t> px(size_t(w) * h);
			parallel_rows(h, [&](int y0, int y1) {
				for (int y = y0; y < y1; y++)
					for (int x = 0; x < w; x++) {
						float v[3];
						glass_at(x, y, v);
						uint32_t c[3];
						for (int i = 0; i < 3; i++) c[i] = uint32_t(std::min(255.0f, v[i] * k + 0.5f));
						px[size_t(y) * w + x] = 0xff000000u | c[0] << 16 | c[1] << 8 | c[2];
					}
			}, 4);
			SDL_UpdateTexture(glass, nullptr, px.data(), w * 4);
			SDL_SetTextureScaleMode(glass, SDL_SCALEMODE_NEAREST);
		}
		SDL_RenderTexture(ren, glass, nullptr, &r);   // over the picture, under the menu
	};
	// CRT colours (as Supermodel's CRT colours, issue #2): the picture as an arcade monitor showed its colours, its
	// phosphors' and its white's (a Japanese cabinet's 9300 K: bluer whites, deeper reds) on today's sRGB screen. The
	// picture is put together in a texture, then drawn into the window through r3d.hlsl's FS_COLOUR: the monitor's
	// gamma (or sRGB's curve) to linear light, the matrix (Dan Mons' colour_matrix_adaptations, as Supermodel's), and
	// sRGB's curve out. On SDL's GPU renderer only (its custom render state): not on SDL's other renderers.
	struct crt_colours { const char *name; float gamma; float m[9]; };   // (gamma 0: sRGB's curve)
	static const crt_colours CRT_COLOURS[] = {
		{ "Off", 0, {} },
		{ "Sega arcade (ARI, 9300 K)", 2.5f, { 1.3272128f, -0.3802109f, -0.1003608f, -0.0241849f, 0.9544506f, 0.0807359f, -0.0239586f, -0.0409736f, 1.4076564f } },
		{ "Sony PVM-20M2U (9300 K)", 2.25f, { 0.9241392f, -0.0690701f, -0.0084279f, 0.0231962f, 0.9729222f, 0.0148832f, -0.0054876f, 0.0098221f, 1.3383897f } },
		{ "BT.601 NTSC (9300 K, Japan)", 0, { 0.7822491f, 0.0506169f, 0.0137752f, 0.0147969f, 0.9741745f, 0.0220302f, -0.0013501f, -0.0044077f, 1.3484820f } },
		{ "BT.601 NTSC (6500 K, US)", 0, { 0.9395421f, 0.0501814f, 0.0102766f, 0.0177722f, 0.9657929f, 0.0164349f, -0.0016216f, -0.0043697f, 1.0059913f } },
		{ "BT.601 PAL (6500 K, Europe, Australia)", 0, { 1.0440432f, -0.0440432f, 0, 0, 1, 0, 0, 0.0117934f, 0.9882066f } },
	};
	const int N_CRT_COLOURS = int(sizeof CRT_COLOURS / sizeof CRT_COLOURS[0]);
	SDL_GPURenderState *crt_state = nullptr;
	SDL_GPUShader *crt_shader = nullptr;
	SDL_Texture *crt_tex = nullptr;
	int crt_tex_w = 0, crt_tex_h = 0, crt_set = -1;
	bool crt_failed = false;
	// the texture to put the picture together in, the CRT colours on (null: straight into the window)
	auto crt_target = [&]() -> SDL_Texture * {
		if (!cfg.crt_colours || crt_failed) return nullptr;
		if (!crt_state) {
			SDL_GPUDevice *gd = SDL_GetGPURendererDevice(ren);
			if (gd) crt_shader = colour_shader(gd);
			SDL_GPURenderStateCreateInfo ci = {};
			ci.fragment_shader = crt_shader;
			if (crt_shader) crt_state = SDL_CreateGPURenderState(ren, &ci);
			if (!crt_state) {
				fprintf(stderr, "CRT colours: not on this renderer (%s)\n", gd ? SDL_GetError() : "SDL's GPU renderer only");
				crt_failed = true;
				return nullptr;
			}
		}
		if (crt_set != cfg.crt_colours) {
			const crt_colours &c = CRT_COLOURS[cfg.crt_colours];
			const float u[12] = { c.m[0], c.m[1], c.m[2], c.gamma, c.m[3], c.m[4], c.m[5], 0, c.m[6], c.m[7], c.m[8], 0 };
			SDL_SetGPURenderStateFragmentUniforms(crt_state, 0, u, sizeof u);
			crt_set = cfg.crt_colours;
		}
		int pw = 0, ph = 0;
		SDL_GetRenderOutputSize(ren, &pw, &ph);
		if (pw < 1 || ph < 1) return nullptr;
		if (!crt_tex || pw != crt_tex_w || ph != crt_tex_h) {
			if (crt_tex) SDL_DestroyTexture(crt_tex);
			crt_tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_TARGET, pw, ph);
			if (!crt_tex) return nullptr;
			SDL_SetTextureScaleMode(crt_tex, SDL_SCALEMODE_NEAREST);
			SDL_SetTextureBlendMode(crt_tex, SDL_BLENDMODE_NONE);
			crt_tex_w = pw; crt_tex_h = ph;
		}
		return crt_tex;
	};
	auto compose = [&](bool filter = true) {
		SDL_Texture *crt = crt_target();
		if (crt) SDL_SetRenderTarget(ren, crt);
		SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
		SDL_RenderClear(ren);
		SDL_FRect all, mid;
		picture_rect(all, mid);
		for (int s = 0; s < 2; s++) {
			const uint32_t v = side_colour[s];
			SDL_SetRenderDrawColor(ren, uint8_t(v), uint8_t(v >> 8), uint8_t(v >> 16), 255);
			const SDL_FRect bar = s ? SDL_FRect{ mid.x + mid.w, all.y, all.x + all.w - (mid.x + mid.w), all.h } : SDL_FRect{ all.x, all.y, mid.x - all.x, all.h };
			if (bar.w > 0) SDL_RenderFillRect(ren, &bar);
		}
		draw_below(all, mid);
		SDL_RenderTexture(ren, g3_shown && g3.tex ? g3.tex : tex3d, nullptr, &all);
		const float ky = mid.h / video::H;
		for (int s = 0; s < 2; s++)
			for (const SideRun &r : side_cover[s]) {
				SDL_SetRenderDrawColor(ren, uint8_t(r.c), uint8_t(r.c >> 8), uint8_t(r.c >> 16), 255);
				const float y0 = mid.y + r.y0 * ky, y1 = mid.y + r.y1 * ky;
				const SDL_FRect bar = s ? SDL_FRect{ mid.x + mid.w, y0, all.x + all.w - (mid.x + mid.w), y1 - y0 }
				                        : SDL_FRect{ all.x, y0, mid.x - all.x, y1 - y0 };
				if (bar.w > 0) SDL_RenderFillRect(ren, &bar);
			}
		// THE DREAMCAST'S ENDING stands in for the replay: its picture covers the whole window, and the 2D
		// of that moment -- the replay's own banners -- goes with it (fv2_fin.h). Not an early return: the
		// CRT filter below owns the render target and has to be let finish.
		bool fin_on = false;
		if (fin.want && cplay.fin_art >= 0 && fin.have()) {
			fin.show(ren, cplay.fin_art, stderr);
			if (fin.tex) { fin.render(ren, all); fin_on = true; }
		} else if (fin.at >= 0) {
			fin.drop();
		}
		// THE PRESENTATION IS BACK, and it stays. Frederic asked for a training that starts without the
		// STAGE / VS screen; the four ROUND_MASK sub-states are the ARENA LOADING (console_play.h), so there
		// is nothing to skip, and hiding what is drawn over the load left a blank screen for two seconds,
		// which he liked less than the banner. The banner at least says what is coming.
		if (fin_on) { /* the Dreamcast ending's picture stands in for the whole frame */ } else
		// The 2D over the 3D. In widescreen the HUD's two bands are drawn in three slices each, the outer two
		// pushed out to the window's edges and the middle left exactly where it was (HudCut above). Only
		// when the picture IS wider than 4:3 and only when a cut was found: everything else is one blit, as
		// it always was.
		{
			const float ks = mid.w / video::W, ts = float(tile_s);
			const float dl = mid.x - all.x, dr = (all.x + all.w) - (mid.x + mid.w);
			auto slice = [&](int x0, int x1, int y0, int y1, float dx) {
				if (x1 <= x0 || y1 <= y0) return;
				const SDL_FRect src = { x0 * ts, y0 * ts, (x1 - x0) * ts, (y1 - y0) * ts };
				const SDL_FRect dst = { mid.x + x0 * ks + dx, mid.y + y0 * ks, (x1 - x0) * ks, (y1 - y0) * ks };
				SDL_RenderTexture(ren, tex_above, &src, &dst);
			};
			// EVEN SPACE EITHER SIDE, for the band that has something standing still in the middle.
			// Frederic, the 2026-10-25: the two blocks of the top HUD go "a egale distance entre le bord de
			// l'ecran et le timer". Hard against the window's edge they left the whole of the widening on
			// one side of each block, which reads as two things that fell off. The foot has nothing in its
			// middle to measure against, and he passed it as it was, so it still goes hard against the edges.
			auto band = [&](const HudCut &c, int y0, int y1, bool even, bool no_mid) {
				if (!c.left) {
					if (!no_mid) { slice(0, video::W, y0, y1, 0); return; }
					// no cut to spread by, but the clock's field is a measured place and does not move
					slice(0, HUD_MID_L, y0, y1, 0);
					slice(HUD_MID_R, video::W, y0, y1, 0);
					return;
				}
				float dxl = -dl, dxr = dr;
				if (even && c.c0 >= 0 && c.l0 >= 0 && c.r0 >= 0) {
					const float stand_l = mid.x + c.c0 * ks, stand_r = mid.x + (c.c1 + 1) * ks;
					const float wl = (c.l1 + 1 - c.l0) * ks, wr = (c.r1 + 1 - c.r0) * ks;
					dxl = all.x + (stand_l - all.x - wl) * 0.5f - (mid.x + c.l0 * ks);
					dxr = stand_r + ((all.x + all.w) - stand_r - wr) * 0.5f - (mid.x + c.r0 * ks);
				}
				slice(0, c.left, y0, y1, dxl);
				if (!no_mid) slice(c.left, c.right, y0, y1, 0);
				slice(c.right, video::W, y0, y1, dxr);
			};
			if (dl > 0.5f && (hud_top.left || hud_foot.left)) {
				band(hud_top, 0, HUD_TOP_ROWS, true, hud_hide_mid);
				slice(0, video::W, HUD_TOP_ROWS, video::H - HUD_FOOT_ROWS, 0);
				band(hud_foot, video::H - HUD_FOOT_ROWS, video::H, false, false);
			} else if (hud_hide_mid) {
				// in 4:3 there is nothing to spread, but the clock still goes
				band(HudCut(), 0, HUD_TOP_ROWS, false, true);
				slice(0, video::W, HUD_TOP_ROWS, video::H, 0);
			} else {
				SDL_RenderTexture(ren, tex_above, nullptr, &mid);
			}
		}
		if (crt) {
			SDL_SetRenderTarget(ren, nullptr);
			SDL_SetGPURenderState(ren, crt_state);
			SDL_RenderTexture(ren, crt, nullptr, nullptr);
			SDL_SetGPURenderState(ren, nullptr);
		}
		if (filter) draw_filter(all);
	};
	auto read_window = [&](const std::string &path, const SDL_Rect *area = nullptr) {
		SDL_Surface *s = SDL_RenderReadPixels(ren, area);
		if (!s) return;
		SDL_Surface *c = SDL_ConvertSurface(s, SDL_PIXELFORMAT_ABGR8888);
		SDL_DestroySurface(s);
		if (!c) return;
		std::vector<uint32_t> px(size_t(c->w) * c->h);
		for (int y = 0; y < c->h; y++) memcpy(&px[size_t(y) * c->w], static_cast<uint8_t *>(c->pixels) + size_t(y) * c->pitch, size_t(c->w) * 4);
		write_png(path.c_str(), px.data(), c->w, c->h);
		SDL_DestroySurface(c);
	};
	// the frame as the game made it (1x, 4:3): the 2D with the 3D between, for shots and tests
	auto write_frame = [&](const std::string &path) {
		static video::Raster one;
		one.resize(video::W, video::H);
		one.render(game.cur(), board.tex_ram.data());
		std::vector<uint32_t> pic(video::W * video::H);
		for (int i = 0; i < video::W * video::H; i++) pic[i] = (game.below[i] >> 24) ? game.below[i] : 0xFF000000u;
		one.compose(pic.data(), pic.data());
		if (game.tiles.hd() && !game.below_hd.empty()) {
			// HD 2D: the HD layers with the 3D between them, as tools/vf3run writes its own (video::compose_hd).
			// The 3D keeps the hardware's size and is repeated, which is what the renderer does on screen.
			for (int i = 0; i < video::W * video::H; i++)
				if (!(one.base[i].a > 0 || one.t1[i].a > 0 || one.t2[i].a > 0)) pic[i] = 0;
			const int S = game.tiles.hd_scale();
			std::vector<uint32_t> big(size_t(video::W) * S * size_t(video::H) * S);
			video::compose_hd(S, game.below_hd, game.above_hd, pic.data(), big.data());
			write_png(path.c_str(), big.data(), video::W * S, video::H * S);
			return;
		}
		for (int i = 0; i < video::W * video::H; i++) if (game.above[i] >> 24) pic[i] = game.above[i];
		write_png(path.c_str(), pic.data(), video::W, video::H);
	};

	// ---- sound ----
	SDL_AudioStream *audio = nullptr;
	if (sound) {
		SDL_AudioSpec spec{ SDL_AUDIO_S16, 2, SOUND_RATE };
		audio = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr);
		if (audio) {
			if (hidden) SDL_SetAudioStreamGain(audio, 0.0f);
			SDL_ResumeAudioStreamDevice(audio);
		} else fprintf(stderr, "no audio: %s\n", SDL_GetError());
	}
	game.sound_wait();
	game.sound.gain = LEVEL * cfg.volume / 100.0;
	game.sound.set_volumes(cfg.music / 100.0f, cfg.effects / 100.0f);
	const int frame_bytes = int(SOUND_RATE / FRAME_HZ) * 4;
	constexpr double QUEUED = 3.0;   // (3.5 when the sound was made in the frame's own time: it's a frame later now; 2.5 kept it near
	                                 // the frame and a half that runs an extra frame, two frames in a pass)

	// ---- pads ----
	SDL_Gamepad *pads[2] = { nullptr, nullptr };
	bool pads_swapped = false;
	auto guid_of = [](SDL_JoystickID id) {
		char buf[64];
		SDL_GUIDToString(SDL_GetGamepadGUIDForID(id), buf, sizeof buf);
		return std::string(buf);
	};
	auto open_pads = [&] {
		for (auto &p : pads) if (p) { SDL_CloseGamepad(p); p = nullptr; }
		int n = 0;
		SDL_JoystickID *ids = SDL_GetGamepads(&n);
		std::vector<bool> taken(size_t(std::max(n, 0)), false);
		for (int p = 0; p < 2; p++) {
			const std::string &c = cfg.ctl[p].pad_choice;
			if (c == "auto" || c == "none") continue;
			for (int i = 0; i < n; i++)
				if (!taken[i] && guid_of(ids[i]) == c) { pads[p] = SDL_OpenGamepad(ids[i]); taken[i] = true; break; }
		}
		// then "auto" players take the free ones in order, then a player whose chosen pad isn't there (or shows up
		// differently: a pad in another mode has another GUID) takes the next free one too (as VF1's and VF2's)
		for (int pass = 0; pass < 2; pass++)
			for (int p = 0; p < 2; p++) {
				const std::string &c = cfg.ctl[p].pad_choice;
				if (pads[p] || c == "none" || (pass == 0 && c != "auto")) continue;
				for (int i = 0; i < n; i++)
					if (!taken[i]) { pads[p] = SDL_OpenGamepad(ids[i]); taken[i] = true; break; }
			}
		SDL_free(ids);
		if (pads_swapped && pads[0] && pads[1]) std::swap(pads[0], pads[1]);
		for (int p = 0; p < 2; p++)
			if (pads[p]) fprintf(stderr, "player %d: %s\n", p + 1, SDL_GetGamepadName(pads[p]));
	};
	open_pads();

	// ---- the menu ----
	auto menu_owner = std::make_unique<game_menu>();   // (let go before the renderer it draws with)
	game_menu &menu = *menu_owner;
	if (!menu.init(win, ren, find_assets()))
		fprintf(stderr, "no menu: its files (assets/ui) weren't found\n");
	// ---- the console's own list of modes ----
	// A surface of its own, drawn over the picture the game made on its own title screen: the owner's picture of
	// the seven lines and nothing else (src/host/fv2_mode_list.h). It is NOT a page of the menu above, and that
	// distinction is the whole point -- a page of the menu is a page of this port's settings whatever is in it.
	fv2_mode_list modes;
	if (cplay.on) modes.load(ren, find_assets(), stderr);
	// And the OPTIONS page behind that list's last line, which is a surface of its own for the same reason.
	// Unlike the six others it EDITS something: the arcade's GAME ASSIGNMENTS, in the backup RAM where they
	// last, so a change restarts the game when the page closes -- as the Arcade settings page does for Virtua
	// Fighter 3 (src/host/fv2_options.h, docs/fv2-console-options.md).
	fv2_texte console_font;
	fv2_options options;
	fv2_pause pause;   // Start during a match, over the fight
	fv2_stage_list stages;   // VERSUS's arena, chosen before the match (fv2_stage_list.h)
	// WHICH OF THE TWO DOORS THE CAROUSEL CAME IN BY. From the select screen the game is left RUNNING and
	// parked in Sega's empty STAGE_SEL; from the pause menu there is no such sub-state to park in -- it is
	// a fight -- so the game is frozen under the page, and the arena changes by restarting the fight.
	bool stage_from_pause = false;
	fv2_training train;      // TRAINING's column of inputs and its damage (fv2_training.h)
	// The console's surfaces, over the picture the game made (and under this port's menu, if that is open as
	// well): mid is the game's own 4:3 screen, which is what their placement was measured against. ONE place
	// draws them, because F12 photographs the window by composing it again -- and a photograph that left the
	// page out would not be a photograph of what is on screen.
	auto draw_console = [&](m3::Model3 &b) {
		SDL_FRect all, mid;
		const bool column = cplay.training_on && cplay.train_fighting;
		if (!modes.shown && !options.shown && !pause.shown && !stages.shown && !column) return;
		picture_rect(all, mid);
		// TRAINING's column goes UNDER the surfaces and over the fight, and only while a round is being
		// fought: a column that stayed up through the judgement and the replay would be reading itself.
		if (column) train.render(ren, all, mid);
		if (modes.shown) modes.render(ren, all, mid);
		// The OPTIONS page's backdrop covers the 4:3 screen and NOTHING IS DIMMED: the Dreamcast does not dim
		// this one, measured (docs/fv2-dreamcast-menus.md section 3bis).
		if (options.shown) options.render(ren, mid, b);
		// The pause menu goes over the fight, dimmed, which is why it is drawn last and takes `all` too.
		if (pause.shown) pause.render(ren, all, mid);
		if (stages.shown) stages.render(ren, all, mid);
	};
	// What was playing under the console's surface, and whether a mode was chosen on the way out (above).
	int music_held = -1;
	bool music_mode_chosen = false;
	// The SOUNDS page's two tests: which effect and which track are under the cursor, and whether a track was
	// started from there (it is stopped on the way out, so a test does not follow the player into the game).
	int se_index = 0x23, bgm_track = 0;
	// which track is actually sounding, or -1: the marker belongs to that one and not to the one under the
	// cursor, or stepping away would leave it claiming a track that is not playing
	int bgm_playing = -1;
	bool bgm_testing = false;
	// The pump's own clock, out here so that going into a surface can prime it (see there).
	double menu_sound_owed = 0;
	uint64_t menu_sound_last = 0;
	// the probe's counters, reset each time a surface opens so its rate means something (see it)
	int menu_sound_frames = 0;
	uint64_t menu_sound_t0 = 0;
	if (cplay.on) {
		console_font.load(ren, find_assets(), stderr);
		options.load(ren, find_assets(), &console_font, stderr);
		pause.load(ren, find_assets(), &console_font, stderr);
		stages.load(ren, find_assets(), &console_font, stderr);
		train.load(ren, find_assets(), &console_font, stderr);
		fin.find(find_assets(), stderr);
		fin.want = cfg.ending_dc;
	}
	menu.set_footer(std::string("Virtua Fighter 3 Recomp ") + VF3_VERSION + (*VF3_DATE ? std::string(" \u00b7 ") + VF3_DATE : std::string()));
	bool running = true, paused = false;
	pad_state pstate[2];

	// ---- the governor (tests only: VF3_GOVERN) ----
	// The pictures' cost: if they leave too little of the screen's refresh (smooth motion) or of the game's
	// frame, the in-between pictures go first, then the scale a step at a time. Each cut lifts again once the
	// pictures have been well within their time for a while; the first seconds after a start or a change don't
	// count (the GPU's and caches' warm-up).
	struct governor {
		bool smooth_off = false;           // in-between pictures held to 60 a second (as VF1's and VF2's: off, a
		                                   // 60 Hz screen's pictures would be uneven, the game's 57.5 a second)
		int scale_cut = 0;                 // steps below the player's scale
		double avg_ms = 0;                 // a picture's time, averaged
		double fixed_ms = 0;               // the part of it no scale changes (the in-between, the triangles prepared), averaged
		// Refreshes missed: a picture each refresh, passes more than a refresh and a half apart, counted over 3 s. A
		// picture's own time leaves out the GPU's (it's submitted, not waited on) and the wait at the present: a GPU
		// that can't keep up misses refreshes with the pictures' time well in budget (VF2's governor judges by the
		// pictures shown, for the same reason). Over 10% of a window's: taken as pictures over their time.
		uint64_t win_t0 = 0;
		int win_passes = 0, win_missed = 0;
		int was_passes = 0, was_missed = 0;   // (the last window's, closed: what gov.missing was judged on, for the log)
		bool missing = false;
		int missed_cuts = 0;   // cuts made for refreshes missed this run: each waits longer to lift (10 s, 20, 40, then
		                       // 60), never kept for the run (as VF2's since its #3). Kept from the third, the character
		                       // select's and the intros' loads (a stage, a fighter) held a fast Mac at 60 for good by the
		                       // second match; kept at once, a first start's warm-up (the new exe scanned, the driver's
		                       // shaders built) held a fast PC at 60 all run
		uint64_t grace_until = 0, calm_since = 0;
		double emu_ms = 3.0;   // a game frame's cost, smoothed over frames (not the pace line's window, which restarts)
		int pictures = 0;
	} gov;
	auto gov_grace = [&](double s) { gov.grace_until = SDL_GetTicksNS() + uint64_t(s * 1e9); gov.calm_since = 0; gov.avg_ms = gov.fixed_ms = 0; gov.pictures = 0;
	                              gov.win_t0 = SDL_GetTicksNS(); gov.win_passes = gov.win_missed = 0; gov.missing = false; };
	gov_grace(10);   // (the start's first seconds don't count: the GPU warming up, the sound queue filling; as VF2's)
	const uint64_t start_ns = SDL_GetTicksNS();

	// A graphics setting changed by the player: every cut the governor made lifted, the pictures judged afresh. (Only
	// the setting's own cut was: smooth motion held to 60 by a slow moment stayed held while the player lowered the
	// scale, the cuts then lifted only after 10 calm seconds by a measure of the old settings, if ever.)
	auto gov_fresh = [&] {
		if (gov.smooth_off || gov.scale_cut || gov_aa_cut) fprintf(stderr, "governor: a setting changed: its cuts lifted\n");
		gov.smooth_off = false;
		gov.missed_cuts = 0;
		gov.scale_cut = 0;
		scale = chosen_scale();   // (not cfg.scale: 0 on a first start, and the picture's size loop never ended)
		gov_aa_cut = 0;
		gov_grace(3);
	};
	auto set_scale = [&](int s) {
		cfg.scale = std::clamp(s, 1, MAX_SCALE);
		cfg_dirty = true;
		gov_fresh();
		fprintf(stderr, "render scale %dx\n", scale);
	};
	auto set_shape = [&](int a) { aspect_i = cfg.aspect = a; cfg_dirty = true; gov_fresh(); };
	auto set_fullscreen = [&](bool on) { SDL_SetWindowFullscreen(win, on); SDL_SyncWindow(win); cfg.fullscreen = on; cfg_dirty = true; gov_fresh(); };
	int test_frames = 0;        // the test button held for these frames (the menu's Service menu; Start in the test menu)
	int service_frames = 0, test_mode_frames = 0;   // (Down in the test menu: the service button)
	bool test_pending = false, in_test = false, test_down_was = false, test_start_was = false;
	bool clock_reset = false;   // (the boot took its time: the frame clock starts again from now)
	// Reset (the menu, F3, leaving the test menu the PC way): the menu closes, a paused game unpauses, and the screen
	// goes black at once while the boot runs unseen, not the last picture held (as VF1's and VF2's)
	// a frame started, its first half run (smooth motion: Game::start_frame), with the inputs it was started with
	bool half = false;
	// (the started frame run on the game's own thread meanwhile, unless VF3_NO_EMU_THREAD: then its first half when
	// started, the rest when due)
	const bool emu_threaded = getenv("VF3_NO_EMU_THREAD") == nullptr;
	hotkey half_hk = hotkey::none;
	InputScript::Actions half_act;
	bool half_left = false;
	uint64_t half_ns = 0;   // (its first half's time, counted with the frame's)
	auto reset_game = [&] {
		half = false;
		menu.close();   // (back to the game, as VF1's and VF2's)
		paused = false;
		SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
		SDL_RenderClear(ren);
		SDL_RenderPresent(ren);
		save_nvram();
		boot();
		test_frames = service_frames = test_mode_frames = 0;
		test_pending = in_test = false;
		fprintf(stderr, "the game restarted\n");
		clock_reset = true;
	};
	auto on_off = [](bool b) { return std::string(b ? "On" : "Off"); };
	auto wrap = [](int v, int n) { return (v % n + n) % n; };
	auto back_item = [&] { return menu_item{ "Back", "Back to the previous page.", {}, {}, [&] { menu.pop(); } }; };
	auto display_page = [&] {
		menu_page p{ "Display", "", {} };
		p.items.push_back({ "Render scale",
			"How sharp the 3D is. 1x is the original arcade resolution; higher is sharper but needs a faster computer. If yours can't keep up, the game lowers it for you. (F5)",
			[&] { return std::to_string(chosen_scale()) + "x" + (gov.scale_cut ? " (lowered to " + std::to_string(scale) + "x)" : ""); },
			[&](int d) { set_scale(chosen_scale() + d); } });
		p.items.push_back({ "Antialiasing",
			"Smooths jagged edges on the 3D: 2x and 3x draw it twice or three times as large each way and average it down. Cleaner, but four or nine times the work.",
			[&] {
				const int k = aa_now();
				if (cfg.aa > 1 && k < cfg.aa) return std::to_string(cfg.aa) + "x (" + (k > 1 ? std::to_string(k) + "x" : std::string("off")) + (gov_aa_cut ? " for now: the computer is busy)" : " at this render scale)");
				return k > 1 ? std::to_string(k) + "x" : std::string("Off");
			},
			[&](int d) { cfg.aa = std::clamp(cfg.aa + d, 1, 3); cfg_dirty = true; gov_fresh(); } });
		p.items.push_back({ "3D renderer",
			"Which part of your computer draws the 3D. Auto uses the GPU when it draws the same picture as the CPU here (it checks as the game starts). A change takes effect at the next start.",
			[&] {
				std::string now = g3.on ? "GPU" : "CPU";
				if (cfg.renderer3d != r3d_mode) return std::string(R3D_NAMES[cfg.renderer3d]) + " (from the next start)";
				if (r3d_mode == R3D_AUTO) return "Auto (" + now + ")";
				if (r3d_mode == R3D_GPU && g3.failed) return std::string("GPU (failed: CPU)");
				return std::string(R3D_NAMES[r3d_mode]);
			},
			[&](int d) {
				static const int ORDER[] = { R3D_AUTO, R3D_CPU, R3D_GPU };
				int i = 0;
				while (i < 2 && ORDER[i] != cfg.renderer3d) i++;
				cfg.renderer3d = ORDER[wrap(i + d, 3)]; cfg_dirty = true;
				cfg.gpu3d_failed.clear();   // (the player's choice: every GPU driver tried again from the next start)
			} });
		p.items.push_back({ "Picture shape",
			"Fit the window fills any screen shape. The other shapes add black bars to keep their shape; 4:3 is the original arcade screen. Wider, the fights see more to the sides. (F6)",
			[&] { return std::string(aspect_i ? ASPECT_NAMES[aspect_i] : "Fit the window"); },
			[&](int d) { set_shape(wrap(aspect_i + d, N_ASPECTS)); } });
		p.items.push_back({ "Background filter",
			"Smooth softens the stage backgrounds so they don't look blocky when enlarged. Off keeps their original pixels. The scores and text stay sharp either way.",
			[&] { return std::string(cfg.smooth_bg ? "Smooth" : "Off"); },
			[&](int) { cfg.smooth_bg = !cfg.smooth_bg; cfg_dirty = true; SDL_SetTextureScaleMode(tex_below, cfg.smooth_bg ? SDL_SCALEMODE_LINEAR : SDL_SCALEMODE_NEAREST); } });
		p.items.push_back({ "Distant textures",
			"Sharp picks the textures' detail for the picture's size, so things in the distance stay as crisp as the render scale allows. Arcade picks it as the arcade did for its own screen: softer in the distance at higher render scales. The same at 1x.",
			[&] { return std::string(cfg.sharp_textures ? "Sharp" : "Arcade"); },
			[&](int) { cfg.sharp_textures = !cfg.sharp_textures; video::Raster::arcade_lod = !cfg.sharp_textures; cfg_dirty = true; } });
		p.items.push_back({ "Smooth motion",
			"On: a picture for each refresh of your screen (up to 120 a second), placed between the arcade's 57.5 frames a second, a frame behind them. Off: only the arcade's own frames, as on the cabinet (uneven on most screens).",
			[&] { return on_off(cfg.smooth) + (cfg.smooth && gov.smooth_off ? " (60 a second for now: the computer is busy)" : ""); },
			[&](int) { cfg.smooth = !cfg.smooth; cfg_dirty = true; gov_fresh(); } });
		p.items.push_back({ "Screen filter",
			"Scanlines adds the dark lines between the arcade monitor's rows. CRT adds those, the monitor's red, green and blue stripes, and slightly darker corners. The lines need a large window or fullscreen to show.",
			[&] { return std::string(FILTER_NAMES[cfg.filter]); },
			[&](int d) { cfg.filter = wrap(cfg.filter + d, 3); cfg_dirty = true; } });
		p.items.push_back({ "CRT colours",
			"The colours as an arcade monitor showed them, as Supermodel's CRT colours. Sega arcade is a Japanese cabinet's: bluer whites and richer colours. The others are a Sony studio monitor and the TV standards of Japan, the US and Europe. Off: the game's colours as they are.",
			[&] { return std::string(CRT_COLOURS[cfg.crt_colours].name) + (cfg.crt_colours && crt_failed ? " (not with this renderer)" : ""); },
			[&](int d) { cfg.crt_colours = wrap(cfg.crt_colours + d, N_CRT_COLOURS); cfg_dirty = true; } });
#ifdef _WIN32
		p.items.push_back({ "Graphics driver",
			"Auto starts on Vulkan, and goes to Direct3D 12 by itself if Vulkan won't start or draws wrongly. Direct3D 12 starts on Direct3D 12 instead. Applies from the next start.",
			[&] { return std::string(cfg.graphics ? "Direct3D 12" : "Auto (Vulkan first)"); },
			[&](int) { cfg.graphics = !cfg.graphics; cfg_dirty = true; } });
#endif
		p.items.push_back({ "Show FPS", "Shows the frame rate in the top right corner.",
			[&] { return on_off(cfg.show_fps); },
			[&](int) { cfg.show_fps = !cfg.show_fps; cfg_dirty = true; if (!cfg.show_fps) menu.set_fps(""); } });
		p.items.push_back({ "Fullscreen", "Play fullscreen or in a window. (F11, F or Alt+Enter)",
			[&] { return on_off(SDL_GetWindowFlags(win) & SDL_WINDOW_FULLSCREEN); },
			[&](int) { set_fullscreen(!(SDL_GetWindowFlags(win) & SDL_WINDOW_FULLSCREEN)); } });
		p.items.push_back(back_item());
		return p;
	};
	auto sound_page = [&] {
		menu_page p{ "Sound", "", {} };
		p.items.push_back({ "Volume", "The overall volume.",
			[&] { return std::to_string(cfg.volume) + "%"; },
			[&](int d) {
				cfg.volume = std::clamp(cfg.volume + 10 * d, 0, 100); cfg_dirty = true;
				game.sound_wait();
				game.sound.gain = LEVEL * cfg.volume / 100.0;
			} });
		p.items.push_back({ "Music", "The music's volume, within the overall volume.",
			[&] { return std::to_string(cfg.music) + "%"; },
			[&](int d) {
				cfg.music = std::clamp(cfg.music + 10 * d, 0, 100); cfg_dirty = true;
				game.sound_wait();
				game.sound.set_volumes(cfg.music / 100.0f, cfg.effects / 100.0f);
			} });
		p.items.push_back({ "Effects and voices", "Hits, voices and the announcer, within the overall volume.",
			[&] { return std::to_string(cfg.effects) + "%"; },
			[&](int d) {
				cfg.effects = std::clamp(cfg.effects + 10 * d, 0, 100); cfg_dirty = true;
				game.sound_wait();
				game.sound.set_volumes(cfg.music / 100.0f, cfg.effects / 100.0f);
			} });
		p.items.push_back(back_item());
		return p;
	};
	auto game_page = [&] {
		menu_page p{ "Game", "", {} };
		p.items.push_back({ "Play style",
			"PC skips the boot screen and is always free play. Arcade has the boot screen, coins and credits, like the real cabinet: Start needs a credit, and Coin adds one (5 and 6, or set it in Controls: a key, or a pad button such as View); its coin settings are kept for it. Console is the PC way with a console's own menu: Start during the attract loop picks a mode. Changing it restarts the game.",
			[&] { return std::string(cfg.arcade ? "Arcade" : cfg.console ? "Console" : "PC"); },
			// Three ways round, and only two where the console way isn't known: it is one game's, and offering
			// it on another would be offering nothing.
			[&](int d) {
				const int n = console_known ? 3 : 2;
				int i = (cfg.arcade ? 1 : cfg.console ? 2 : 0) + (d < 0 ? n - 1 : 1);
				i %= n;
				cfg.arcade = i == 1;
				cfg.console = i == 2;
				cplay = console_play();   // (a style changed mid-session: nothing of the last one is carried over)
				cplay.on = cfg.console && console_known;
				modes.hide();
				options.hide();
				if (cplay.on) {   // (read once: the second call is free)
					modes.load(ren, find_assets(), stderr);
					console_font.load(ren, find_assets(), stderr);
					options.load(ren, find_assets(), &console_font, stderr);
					pause.load(ren, find_assets(), &console_font, stderr);
					stages.load(ren, find_assets(), &console_font, stderr);
					train.load(ren, find_assets(), &console_font, stderr);
				}
				g_pc_mode = !cfg.arcade;
				cfg_dirty = true;
				reset_game();
			} });
		p.items.push_back({ "Reset game", "Restarts the game, like switching the arcade off and on. Rankings and settings are kept. (F3)",
			{}, {}, [&] { reset_game(); } });
		p.items.push_back(back_item());
		return p;
	};
	// The console way's own page: what a console puts in front of an arcade game. Start during the attract loop
	// opens it, and the game is paused behind it as it is behind any page of this menu. Each line hands the game
	// a real Start when it closes -- the credit is the game's business, and this menu never touches it.
	auto console_page = [&] {
		menu_page p{ "Fighting Vipers 2", "Choosing a mode hands the game a real press of Start. What that costs is the game's own business, on credits as on free play.", {} };
		p.items.push_back({ "Arcade", "The game as it comes: the fixed road of opponents.",
			{}, {}, [&] { cplay.choose_arcade(); menu.close(); } });
		p.items.push_back({ "Survival", "One opponent after another, with the life you have left. The flag is set once the game is running, not before -- set during the attract loop it stops the game starting at all -- and put back when the game ends.",
			{}, {}, [&] { cplay.choose_survival(board); menu.close(); } });
		p.items.push_back({ "Hidden fighters", "Del Sol, Kuhn, BM and Dural on the select screen, the cursor wrapping at 14 instead of 10. Past cursor 10 the game's own names and portraits are wrong or missing.",
			[&] { return std::string(cfg.full_roster ? "On" : "Off"); },
			[&](int) { cfg.full_roster = !cfg.full_roster; game.full_roster = cfg.full_roster; cfg_dirty = true;
			           if (cfg.full_roster) game.hold_roster(); } });
		p.items.push_back({ "Honey: four costumes", "The select screen cycles Honey's two costumes with a press of Up. On, it cycles four: Sega's two, then the same two without the skirt -- the costume Sega put behind thirty straight wins in an intruded match. The extra two are chosen the same way, by pressing Up again.",
			[&] { return std::string(cfg.honey_costume ? "On" : "Off"); },
			[&](int) { cfg.honey_costume = !cfg.honey_costume; game.honey_costume = cfg.honey_costume; cfg_dirty = true;
			           game.honey_cycle = 0; } });
		p.items.push_back(back_item());
		return p;
	};
	// The arcade's own settings (its test menu's GAME ASSIGNMENTS), read and written where the game keeps them in the
	// backup RAM: a byte each at 0x1E000 on (found by changing each in the test menu), the energies 32-bit. The game
	// reads them as it starts: a change restarts it when the menu closes (as VF2's), if they differ from when their
	// page opened (a value stepped there and back, or at its limit, doesn't).
	std::vector<uint8_t> arcade_before;   // (empty: the page not opened since the menu last closed)
	auto arcade_bytes = [&] { if (game.emu_thread.joinable()) game.job_wait(); return std::vector<uint8_t>(board.backup.begin() + 0x1E000, board.backup.begin() + 0x1E040); };
	auto arcade_page = [&] {
		menu_page p{ "Arcade settings: a change restarts the game when the menu closes", "", {} };
		auto at = [&](int o) -> uint8_t & { return board.backup[0x1E000 + size_t(o)]; };
		auto choice = [&](const char *label, const char *help, int o, int lo, int hi, std::function<std::string(int)> name) {
			p.items.push_back({ label, help,
				[at, o, lo, hi, name] { const int v = at(o); return v < lo || v > hi ? std::string("-") : name(v); },
				[&, at, o, lo, hi](int d) {
					const int v = at(o);
					if (v < lo || v > hi) return;
					at(o) = uint8_t(std::clamp(v + d, lo, hi));
				} });
		};
		auto energy = [&](const char *label, const char *help, int o, int lo, int hi) {
			auto get = [at, o] { return int(at(o)) << 24 | int(at(o + 1)) << 16 | int(at(o + 2)) << 8 | int(at(o + 3)); };
			p.items.push_back({ label, help,
				[get, lo, hi] { const int v = get(); return v < lo || v > hi ? std::string("-") : std::to_string(v); },
				[&, at, get, o, lo, hi](int d) {
					const int v = get();
					if (v < lo || v > hi) return;
					const int n = std::clamp(v + 10 * d, lo, hi);
					for (int i = 0; i < 4; i++) at(o + i) = uint8_t(n >> (24 - 8 * i));
				} });
		};
		static const char *DIFFICULTY[] = { "Easy", "Normal", "Hard", "Hardest" };
		static const char *COUNTRY[] = { "Japan", "USA", "Export", "Asia" };
		choice("Difficulty", "How hard the computer fights.", 6, 0, 3, [](int v) { return std::string(DIFFICULTY[v]); });
		choice("Rounds to win (vs CPU)", "Rounds needed to win a match against the computer.", 2, 2, 5, [](int v) { return std::to_string(v); });
		choice("Rounds to win (vs player)", "Rounds needed to win a match between two players.", 3, 2, 5, [](int v) { return std::to_string(v); });
		energy("Energy (vs CPU)", "Each fighter's energy in a match against the computer (the arcade's ENERGY MAX).", 8, 160, 360);
		energy("Energy (vs player)", "Each fighter's energy in a match between two players.", 12, 180, 400);
		choice("Demo sound", "Sound during the demo that plays between games.", 7, 0, 1, [&](int v) { return on_off(v == 0); });
		choice("Country", "The arcade's region: Japan, USA, Export or Asia. It changes the game's text and what it shows.", 0x1E, 0, 3,
			[](int v) { return std::string(COUNTRY[v]); });
		p.items.push_back({ "Service menu",
			"The arcade's own test menu: input and sound tests, the rest of its game settings, coin settings and bookkeeping. Down moves, Start selects (or 9 and F2 on a keyboard). Choose EXIT to come back.",
			{}, {}, [&] { menu.close(); paused = false; test_pending = true; } });
		p.items.push_back(back_item());
		return p;
	};
	auto key_name = [](SDL_Scancode k) {
		if (!k) return std::string("-");
		const char *n = SDL_GetScancodeName(k);
		return n && *n ? std::string(n) : "Key " + std::to_string(int(k));
	};
	auto pad_name = [&](int p) {
		const std::string &c = cfg.ctl[p].pad_choice;
		const char *nm = pads[p] ? SDL_GetGamepadName(pads[p]) : nullptr;
		std::string now = pads[p] ? (nm && *nm ? nm : "Gamepad") : "";
		if (c == "none") return std::string("None");
		if (c == "auto") return now.empty() ? std::string("Any pad (none connected)") : "Any pad: " + now;
		return now.empty() ? std::string("Chosen pad (not connected)") : now;
	};
	auto cycle_pad = [&](int p, int d) {
		std::vector<std::string> opts = { "auto" };
		int n = 0;
		SDL_JoystickID *ids = SDL_GetGamepads(&n);
		for (int i = 0; i < n; i++) opts.push_back(guid_of(ids[i]));
		SDL_free(ids);
		opts.push_back("none");
		auto it = std::find(opts.begin(), opts.end(), cfg.ctl[p].pad_choice);
		int i = it == opts.end() ? 0 : int(it - opts.begin());
		cfg.ctl[p].pad_choice = opts[wrap(i + d, int(opts.size()))];
		if (cfg.ctl[p].pad_choice != "auto" && cfg.ctl[p].pad_choice != "none" && cfg.ctl[1 - p].pad_choice == cfg.ctl[p].pad_choice)
			cfg.ctl[1 - p].pad_choice = "auto";
		cfg_dirty = true;
		open_pads();
	};
	auto keys_page = [&](int p) {
		menu_page pg{ p ? "Player 2 keyboard" : "Player 1 keyboard", "", {} };
		for (int a = 0; a < C_COUNT; a++)
			pg.items.push_back({ CONTROL_NAMES[a], std::string(a == C_COIN ? "A coin for this player in Arcade mode (a cabinet's coin slot). "
													: a == C_MENU ? "Opens this menu (as Esc always does): for a cabinet's button. " : "") +
					"Select, then press the key to use. Delete clears it.",
				[&, p, a] { return key_name(cfg.ctl[p].key[a]); }, {},
				[&, p, a] {
					menu.capture(std::string("Press a key for ") + CONTROL_NAMES[a] + (p ? " (player 2)" : " (player 1)") +
								 ". Backspace or Delete clears it, Esc keeps it.",
						[&, p, a](const SDL_Event &e) {
							if (e.type != SDL_EVENT_KEY_DOWN) return false;
							const SDL_Scancode k = e.key.scancode == SDL_SCANCODE_BACKSPACE || e.key.scancode == SDL_SCANCODE_DELETE ? SDL_SCANCODE_UNKNOWN : e.key.scancode;
							static const SDL_Scancode RESERVED[] = { SDL_SCANCODE_P, SDL_SCANCODE_TAB, SDL_SCANCODE_F3,
								SDL_SCANCODE_F5, SDL_SCANCODE_F9, SDL_SCANCODE_F6, SDL_SCANCODE_F11, SDL_SCANCODE_F12 };
							for (SDL_Scancode r : RESERVED) if (k == r) return false;
							for (const auto &sk : SYS_KEYS) if (k == sk.key) return false;
							if (k) for (auto &c : cfg.ctl) for (auto &x : c.key) if (x == k) x = SDL_SCANCODE_UNKNOWN;
							cfg.ctl[p].key[a] = k;
							cfg_dirty = true;
							return true;
						});
				}, false, [&, p, a] { cfg.ctl[p].key[a] = SDL_SCANCODE_UNKNOWN; cfg_dirty = true; } });
		pg.items.push_back(back_item());
		return pg;
	};
	auto pad_buttons_page = [&](int p) {
		menu_page pg{ p ? "Player 2 pad buttons" : "Player 1 pad buttons", "", {} };
		for (int a = C_GUARD; a < C_COUNT; a++)
			pg.items.push_back({ CONTROL_NAMES[a], std::string(a == C_COIN ? "A coin for this player in Arcade mode (a cabinet's coin slot). "
												   : a == C_MENU ? "Opens this menu. L3 + R3 and the touchpad click (when it isn't set to something else) always do too, and Esc on a keyboard. " : "") +
												   "Select, then press the button (or trigger) to use. Y (or Delete) clears it. "
												   "The d-pad and left stick always move.",
				[&, p, a] { return player_controls::pad_label(cfg.ctl[p].pad[a]); }, {},
				[&, p, a] {
					menu.capture(std::string("Press a pad button for ") + CONTROL_NAMES[a] + (p ? " (player 2)" : " (player 1)") +
								 (a >= C_COIN ? ". Esc keeps it; Delete clears it." : ". View or Esc keeps it; Delete clears it."),
						[&, p, a](const SDL_Event &e) {
							int code = PAD_NONE;
							if (e.type == SDL_EVENT_KEY_DOWN) {
								if (e.key.scancode != SDL_SCANCODE_DELETE && e.key.scancode != SDL_SCANCODE_BACKSPACE) return false;
								cfg.ctl[p].pad[a] = PAD_NONE;
								cfg_dirty = true;
								return true;
							}
							if (e.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) {
								// View: keep it (View is for the menu's hotkeys, not moves), unless it's for Coin or Menu
								if (e.gbutton.button == SDL_GAMEPAD_BUTTON_BACK && a < C_COIN) return true;
								code = e.gbutton.button;
							} else if (e.type == SDL_EVENT_GAMEPAD_AXIS_MOTION && e.gaxis.value > 16000 &&
									   (e.gaxis.axis == SDL_GAMEPAD_AXIS_LEFT_TRIGGER || e.gaxis.axis == SDL_GAMEPAD_AXIS_RIGHT_TRIGGER))
								code = e.gaxis.axis == SDL_GAMEPAD_AXIS_LEFT_TRIGGER ? PAD_LT : PAD_RT;
							else return false;
							for (int b = C_GUARD; b < C_COUNT; b++) if (cfg.ctl[p].pad[b] == code) cfg.ctl[p].pad[b] = PAD_NONE;
							cfg.ctl[p].pad[a] = code;
							cfg_dirty = true;
							return true;
						});
				}, false, [&, p, a] { cfg.ctl[p].pad[a] = PAD_NONE; cfg_dirty = true; } });
		pg.items.push_back(back_item());
		return pg;
	};
	auto player_page = [&](int p) {
		menu_page pg{ p ? "Player 2" : "Player 1", "", {} };
		pg.items.push_back({ "Pad", "Which controller this player uses: any free one, a particular one, or none.",
							 [&, p] { return pad_name(p); }, [&, p](int d) { cycle_pad(p, d); } });
		pg.items.push_back({ "Keyboard", "Change this player's keys.", {}, {}, [&, p] { menu.push(keys_page(p)); } });
		pg.items.push_back({ "Pad buttons", "Change which buttons guard, punch, kick, escape, do the combinations, start, add a coin and open the menu.",
							 {}, {}, [&, p] { menu.push(pad_buttons_page(p)); } });
		pg.items.push_back({ "Reset to defaults", "Put this player's keys and buttons back as they were.", {}, {},
							 [&, p] {
								 cfg.ctl[p] = player_controls::defaults(p);
								 for (auto k : cfg.ctl[p].key)
									 if (k) for (auto &x : cfg.ctl[1 - p].key) if (x == k) x = SDL_SCANCODE_UNKNOWN;
								 cfg_dirty = true;
								 open_pads();
							 } });
		pg.items.push_back(back_item());
		return pg;
	};
	auto controls_page = [&] {
		menu_page pg{ "Controls", "", {} };
		pg.items.push_back({ "Player 1", "Player 1's controller, keys and buttons.", [&] { return pad_name(0); }, {},
							 [&] { menu.push(player_page(0)); } });
		pg.items.push_back({ "Player 2", "Player 2's controller, keys and buttons.", [&] { return pad_name(1); }, {},
							 [&] { menu.push(player_page(1)); } });
		pg.items.push_back({ "Swap pads", "Player 1 and player 2 swap controllers.", {}, {},
							 [&] {
								 auto choice = [&](int p) { return pads[p] ? guid_of(SDL_GetGamepadID(pads[p])) : std::string("none"); };
								 std::string a = choice(0), b = choice(1);
								 if (a == b) { pads_swapped = !pads_swapped; std::swap(pads[0], pads[1]); return; }
								 cfg.ctl[0].pad_choice = b;
								 cfg.ctl[1].pad_choice = a;
								 cfg_dirty = true;
								 open_pads();
							 } });
		pg.items.push_back(back_item());
		return pg;
	};
	auto main_page = [&] {
		menu_page p{ "", "", {} };
		p.items.push_back({ "Resume", "Back to the game.", {}, {}, [&] { menu.close(); } });
		p.items.push_back({ "Display", "Sharpness, antialiasing, screen shape, the 3D renderer, smooth motion, background and screen filters, frame rate and fullscreen.", {}, {}, [&] { menu.push(display_page()); } });
		p.items.push_back({ "Sound", "The overall, music and effects volumes.", {}, {}, [&] { menu.push(sound_page()); } });
		p.items.push_back({ "Game", "Play style and reset.", {}, {}, [&] { menu.push(game_page()); } });
		// the console way's own page, where Start during the attract loop also opens it
		if (cfg.console && console_known)
			p.items.push_back({ "Fighting Vipers 2", "The console's menu: which mode to start, and the hidden fighters. Start during the attract loop opens it too.",
								{}, {}, [&] { menu.push(console_page()); } });
		p.items.push_back({ "Arcade settings", "Difficulty, rounds, energy, demo sound, country and the service menu. Changing them restarts the game.", {}, {},
							[&] { if (arcade_before.empty()) arcade_before = arcade_bytes(); menu.push(arcade_page()); } });
		p.items.push_back({ "Controls", "Each player's controller, keys and buttons.", {}, {}, [&] { menu.push(controls_page()); } });
		p.items.push_back({ "Quit", "Saves and closes the game.", {}, {}, [&] { running = false; } });
		return p;
	};
	bool sound_paused = false;
	auto sync_menu_sound = [&] {
		// THE CONSOLE'S SURFACES ARE NOT SILENT ANY MORE, and this rule was written when they were. The Escape
		// menu is this port's own and has always been quiet, so the device still sleeps under it; the list and
		// the OPTIONS tree now turn the sound board themselves and play the game's own three notes.
		//
		// Pausing the device under them was what Frederic heard as "des bruitages en decale". Measured with
		// the probe on a real machine: the queue grew by exactly one sound frame per frame made -- 0, 1, 2, 3,
		// 4, 5, 6, 7 frames of backlog -- because nothing was consuming it. Every beep went in behind that
		// backlog and only came out when the surface closed and the device woke up, which is word for word
		// what he described: late, and only once the game started again.
		const bool quiet_now = menu.is_open();
		if (!audio || quiet_now == sound_paused) return;
		sound_paused = quiet_now;
		if (sound_paused) SDL_PauseAudioStreamDevice(audio); else SDL_ResumeAudioStreamDevice(audio);
	};
	auto open_menu = [&] { if (game.emu_thread.joinable()) game.job_wait(); if (menu.ready() && !menu.is_open()) menu.open(main_page()); sync_menu_sound(); };
	// A line of the console's list chosen. ARCADE and SURVIVAL are wired: their levers were measured in this
	// port and it writes them (docs/fv2-console.md). The other five are drawn and do nothing yet -- RANDOM and
	// V.S. want the select screen's own timing, which has not been measured here, and NETWORK, TRAINING and
	// OPTIONS have no lever in this port at all. A line that does nothing leaves the list up: closing it would
	// say something happened.
	// Leaving the page. NOTHING RESTARTS, and that is deliberate: the page writes both the backup, which makes
	// a setting last, and the working copy the running game reads, which makes it take effect. The first draft
	// wrote only the backup and restarted the game to have it read -- which is what Virtua Fighter 3's Arcade
	// settings page does, and which, on a console's options screen, looks exactly like a crash: the picture
	// goes black and the window sits silent for seconds while the board boots again.
	// ---- the console's DISPLAY and SOUNDS pages ----
	//
	// These two screens are August's, line for line (OptionsEntries / SoundEntries in the 2026-08-20 backup),
	// but what they switch belongs to THIS PORT, so they are registered from here rather than built inside
	// fv2_options.h, which has no business knowing about cfg.
	//
	// A LINE WITH NOTHING TO SWITCH KEEPS ITS PLACE AND IS STEPPED OVER, which is August's own rule and his
	// reason: a page whose lines slide up and down as settings come and go is misread at a glance. That is
	// what the lines this port cannot do yet are -- drawn, dimmed, and passed by -- rather than quietly
	// missing. Each says below why it is one.
	bool ending_asked = false;   // ENDING on the DISPLAY page: the page closes so the roll can be seen
	bool ending_real = false;    // which of the two that line does: the roll now, or the game's own ending
	if (cplay.on) {
		auto fixed = [](const char *v) { return [v] { return std::string(v); }; };
		// A pack to load: one already in, or at least one PNG sitting in the folder. The dump writes into a
		// subfolder of its own, so the files directly in here are the pack's and nothing else.
		bool hd_pack = video::hd3d().replacing();
		if (!hd_pack) {
			std::error_code hec;
			for (const auto &e : std::filesystem::directory_iterator(hd_pack_dir, hec))
				if (e.is_regular_file(hec) && e.path().extension() == ".png") { hd_pack = true; break; }
		}
		options.own[fv2_options::SCR_DISPLAY] = {
			// August's WIDESCREEN was a switch because its fork had two shapes; this port has five, so the
			// line steps through them and shows the one in force. Same line, more of it.
			{ "WIDESCREEN", [&] { return std::string(aspect_i ? ASPECT_NAMES[aspect_i] : "FIT"); },
			  [&](int d) { set_shape(wrap(aspect_i + d, N_ASPECTS)); }, {}, {}, 0, false, 0 },
			// HD ART SWITCHES NOW. The pack used to be read once at start-up with no way to let go of it, so
			// the line could only report. It needs no new lever: load() empties what it holds before it reads,
			// so loading from nowhere is how a pack is put down, and the GPU renderer already checks the page
			// count rather than assuming it -- its own comment foresaw a pack arriving later.
			//
			// NONE when there is no pack to load, and then the line is stepped over like any other that has
			// nothing to switch. Whether there is one is settled here, once, as the rest of the page is: a
			// folder that gains its first PNG while the game runs is not a case worth a check every picture.
			// Turning it on again re-reads the folder, which is the price of having no unload.
			{ "HD ART", [&] { return std::string(video::hd3d().replacing() ? "ON" : hd_pack ? "OFF" : "NONE"); },
			  hd_pack ? std::function<void(int)>([&](int) {
				  std::string report;
				  if (video::hd3d().replacing()) {
					  video::hd3d().load(std::string(), report);   // (empties it: see above)
					  fprintf(stderr, "the console's DISPLAY: the 3D texture pack put down\n");
				  } else if (video::hd3d().load(hd_pack_dir, report)) {
					  fprintf(stderr, "the console's DISPLAY: %s\n", report.c_str());
				  } else {
					  fprintf(stderr, "the console's DISPLAY: %s\n", report.c_str());
					  return;   // (nothing loaded: the setting does not claim otherwise)
				  }
				  cfg.hd_3d = video::hd3d().replacing();
				  cfg_dirty = true;
			  }) : std::function<void(int)>(), {}, {}, 0, false, 0 },
			{ "FPS COUNTER", [&] { return std::string(cfg.show_fps ? "ON" : "OFF"); },
			  [&](int) { cfg.show_fps = !cfg.show_fps; cfg_dirty = true; }, {}, {}, 0, false, 0 },
			// August's ENDING switched the fork's own illustrations over the replays, and nothing of those
			// exists here. But the GAME has an ending -- the staff roll, mode 12 -- which no one had ever
			// seen running, because reaching it means finishing the ladder and the notes have it among the
			// states never reached. So this line is not his switch but a way to watch it: A asks the game
			// for it and the page gets out of the way. It ends by itself and the attract loop takes over,
			// so there is nothing to put back (model3.cpp, fv2_request_ending).
			//
			// HALF OF IT COMES OUT, and the line is kept anyway because the half that comes out is the roll
			// itself. The ending alternates: about 225 frames of credits over the viper, which draw exactly
			// right, and about 225 of a 3D passage -- 279 models over 16 viewports, 383 later -- which draws
			// as a bare white floor with a figure the size of a thumb on it, under 2D banners left from the
			// demonstration ("Old Armstone Town", "v.s. GRACE"). Nothing of the scenery is there.
			//
			// AND THE REASON IS IN SEGA'S OWN NAMES. The ending's seven sub-states, read out of the table of
			// pointers at 0x00024494, are ending_trans_init, ending_trans_wait, ending_replay_init,
			// ending_replay_ctrl, ending_replay_next, ending_final_mask1, ending_final_mask2: THREE OF THE
			// SEVEN ARE REPLAYS. The roll alternates with replays of the matches the player has just won,
			// and the banners are the replay's own, naming its stage and its opponent as every replay does.
			//
			// So from the title screen there is no match to replay and no stage to load. Measured three ways
			// at the same offset from ENDING_INT: asked from the title, a bare floor; asked from inside a
			// fight, the two fighters of that fight, loaded and lit, on a bare floor; entered by the game's
			// own branch -- ADV_INFO_STATUS bit 3, "the ladder is finished" -- the whole replay on its stage,
			// cage and all. This line shows the roll, which is the ending's own content; the replays want a
			// game played first, and that is a lever of its own (docs/fv2-console-options.md).
			// TWO THINGS A AND LEFT/RIGHT CHOOSE BETWEEN, because the ending is two things.
			//
			//   ROLL NOW        the staff roll, here, over the title screen the page is sitting on. The
			//                   credits are whole; the replay passages have no match to replay, so they come
			//                   out on a bare floor. Costs nothing and gives the game back by itself.
			//   AFTER A MATCH   the REAL one. The bit goes in when the game says the match is over, so the
			//                   game's own branch point takes the ending with its replays on their stages --
			//                   324 KB of picture against 16 for the bare one, measured.
			//
			// Armed, the line says so and the page stays up: there is a game to go and play. The arming
			// keeps until a match ends, and nothing holds the bit (console_play.h, model3.cpp).
			// AND THE LINE IS A CHOICE BETWEEN THE TWO ENDINGS, which is what Frederic asked for the
			// 2026-10-25: "le but n'est pas de lancer la fin du jeu mais de choisir entre la fin version
			// arcade ou version console quand on fini le mode arcade". It is August's own line again --
			// his fork's said DREAMCAST or ARCADE -- and the difference is what stands in for the roll's
			// REPLAYS: the arcade shows the matches just won, the Dreamcast its illustrations. Eight of
			// them, chosen and ordered by Frederic out of a rip of the disc (fv2_fin.h).
			//
			// Watching the roll from here has gone with it. It was a way to see a thing nobody had seen
			// running, and it has been seen; a settings line that STARTS something is not a setting.
			{ "ENDING", [&] { return std::string(cfg.ending_dc ? "DREAMCAST" : "ARCADE"); },
			  fin.have() ? std::function<void(int)>([&](int) {
				  cfg.ending_dc = !cfg.ending_dc;
				  fin.want = cfg.ending_dc;
				  cfg_dirty = true;
			  }) : std::function<void(int)>(), {}, {}, 0, false, 0 },
		};
		// The twenty tracks, and the numbering, are August's measured tables (GameFV2.h): on bank 0x10 the
		// sound test's number is the index plus ten, and on bank 0x2E the index plus 353. Confirmed here
		// twice over in the MIDI log -- the attract's A0 10 07 is no. 17 and the replay's A0 10 11 is no. 27.
		static const char *const MUSIC[20] = {
			"SELECT", "BURNING BEAR", "NIGHT CALL", "RAVIN", "L.S.D.", "INSPIRE", "NAME ENTRY", "ATTRACT",
			"INSERT COIN", "GAME OVER", "BRAIN CLEANSER", "DEEPER", "TRIPBACK", "3RD SIGNAL", "UNDER THE SUN",
			"ENDING", "GOOD NOISE", "DEMO", "UFO DINER", "MENU",
		};
		options.own[fv2_options::SCR_SOUNDS] = {
			// THE SOUND TEST WORKS NOW, and it is worth saying why it could not before: it wants a note handed
			// to the sound board while the page is up, and that wants the board turning under a frozen game,
			// which was only built on the 2026-10-22. The three names are the only three anyone has named;
			// the rest show the number the game's own SOUND TEST shows.
			{ "SE", [&] {
				  const char *known = se_index == 0x23 ? "sd_sign_16" : se_index == 0x26 ? "sd_ketsu_00"
								 : se_index == 0x27 ? "sd_sonic00_11" : nullptr;
				  return known ? std::string(known) : "NO. " + std::to_string(353 + se_index);
			  }, [&](int d) { se_index = (se_index + 256 + d) % 256; game.play_effect(uint8_t(se_index)); },
			  {}, {}, 0, false, 0 },
			{ "SE VOLUME", {}, [&](int d) {
				  cfg.effects = std::clamp(cfg.effects + 5 * d, 0, 100); cfg_dirty = true;
				  game.sound.set_volumes(cfg.music / 100.0f, cfg.effects / 100.0f);
			  }, {}, [&] { return (cfg.effects + 4) / 5; }, 20, false, 0 },
			// August drew this one "BGM" and dispatched on "BGM TEST", because two lines called VOLUME could
			// not be told apart. The break before it is his too: a third of a step, read off the original.
			// A MUSIC WAITS TO BE ASKED FOR. Frederic heard the first version, where stepping played at once:
			// that is right for an effect, which is over in half a second, and wrong for a track, which runs
			// until something stops it. So left and right only choose, A starts, and B stops -- and B is kept
			// by this line while something plays, the next one leaving the page as usual.
			{ "BGM", [&] { return std::string(MUSIC[bgm_track]) + (bgm_playing == bgm_track ? "  >" : ""); },
			  [&](int d) { bgm_track = (bgm_track + 20 + d) % 20; },
			  [&] { game.play_music(uint8_t(bgm_track)); bgm_testing = true; bgm_playing = bgm_track; },
			  {}, 0, false, 0.32f,
			  [&] {
				  if (!bgm_testing) return false;
				  game.stop_music();
				  bgm_testing = false;
				  bgm_playing = -1;
				  return true;
			  },
			  true },   // silent: a track starting and a track stopping are heard; the menu need not say so
			// The music packs -- the first Fighting Vipers' tracks in place of these -- are the fork's, and
			// its own notes say the replacement was wired there and never once heard. Nothing of it here.
			{ "BGM SET", fixed("-"), {}, {}, {}, 0, false, 0 },
			{ "BGM VOLUME", {}, [&](int d) {
				  cfg.music = std::clamp(cfg.music + 5 * d, 0, 100); cfg_dirty = true;
				  game.sound.set_volumes(cfg.music / 100.0f, cfg.effects / 100.0f);
			  }, {}, [&] { return (cfg.music + 4) / 5; }, 20, false, 0 },
			{ "INITIALIZE", {}, {}, [&] {
				  cfg.music = cfg.effects = 100; cfg_dirty = true;
				  game.sound.set_volumes(1.0f, 1.0f);
			  }, {}, 0, true, 1.2f },
		};
		options.ctl = cfg.ctl;   // CONTROLLER reads the bindings and writes them
		// And which pad is whose, so a capture on one player's column takes that player's pad and no other
		// (fv2_options.h). Filled again whenever the pads are opened again, below.
		for (int p = 0; p < 2; p++) options.pad_id[p] = pads[p] ? SDL_GetGamepadID(pads[p]) : 0;
		options.build_pages();
	}
	auto close_options = [&] {
		const bool changed = options.changed;
		options.changed = false;
		options.hide();
		cplay.swallow_start = true;
		cplay.start_was = true;
		sync_menu_sound();
		fprintf(stderr, "the console's OPTIONS: closed, %s\n", changed ? "a setting changed" : "nothing changed");
	};
	auto choose_mode = [&](int line) {
		fprintf(stderr, "the console's list: %s chosen\n", fv2_mode_list::name(line));
		if (line == fv2_mode_list::ARCADE) cplay.choose_arcade();
		else if (line == fv2_mode_list::SURVIVAL) cplay.choose_survival(board);
		// RANDOM asks the game for its own flag, RANDOM_CHARA, rather than reproducing the hand manoeuvre
		// that was recorded in August -- that one, Start held across the fighter's validation, was measured
		// here to do nothing at all (console_play.h, model3.cpp).
		else if (line == fv2_mode_list::RANDOM) cplay.choose_random();
		// VERSUS starts as it always did. The stage is NOT asked for here: the summer's ROM put that choice
		// AFTER the fighters are picked, and so does this port -- console_play raises want_stage_page when
		// the game leaves SELECT for DECIDE_INT, and the page opens there (fv2_stage_list.h).
		else if (line == fv2_mode_list::VERSUS) cplay.choose_versus();
		// TRAINING is not a mode this board has -- Sega's twenty-four hold none -- so it is one this port
		// builds, and what exists of it today is its arena: MARBLE ARENA, by one byte of the game's own route
		// table (console_play.h, model3.cpp). Everything else a training mode is -- a timer that does not run
		// out, health that comes back, the dummy's behaviour -- is still to be read, in the owner's draft in
		// the summer's patched ROM and in the Dreamcast version.
		else if (line == fv2_mode_list::TRAINING) cplay.choose_training();
		// OPTIONS starts nothing: the list gives way to a page of its own and the game stays frozen behind it,
		// exactly where it was. Nothing is injected, so the press is swallowed below as for the others.
		else if (line == fv2_mode_list::OPTIONS && options.ready) {
			modes.hide();
			options.show();
			cplay.swallow_start = true;
			cplay.start_was = true;
			sync_menu_sound();
			return;
		}
		else {
			fprintf(stderr, "the console's list: %s has no lever in this port yet -- nothing done\n", fv2_mode_list::name(line));
			return;
		}
		modes.hide();
		music_mode_chosen = true;   // the select screen lays its own music: nothing to put back
		// The press that chose must not reach the game as a player's Start as well: the mode's own injection is
		// what hands the game one, and it goes on last (frame_inputs below).
		cplay.swallow_start = true;
		cplay.start_was = true;
		sync_menu_sound();
	};

	// A stage chosen on the versus page: the scene goes to console_play, which holds the picker once the
	// game has left its attract loop and gives it back when the game ends (console_play.h). Entry 0 holds
	// nothing and the game chooses as it does on the board.
	auto chose_stage = [&](int line) {
		const int scene = line + 1;   // the page counts from 0; the picker takes the scene
		fprintf(stderr, "the console's stage select: stage %d chosen\n", scene);
		stages.hide();
		cplay.stage_chosen(board, scene);
		// FROM THE PAUSE MENU THE FIGHT HAS TO BE RESTARTED, or nothing is seen to happen. Holding the
		// draw's table decides the arena the game LOADS, and a fight already has its own loaded; the game
		// loads one at round 1, so the stage changes by the road REMATCH takes (Model3::fv2_rematch):
		// the same two fighters, round 1, and the arena just chosen.
		if (stage_from_pause) {
			const int said = board.fv2_rematch(cplay.task_live[0], cplay.task_live[1]);
			fprintf(stderr, "the console stage select: from the pause menu -- the fight restarts in it, "
				"the rematch the game does between rounds says %d\n", said);
			stage_from_pause = false;
		}
		cplay.swallow_start = true;
		cplay.start_was = true;
		sync_menu_sound();
	};

	// A line of the pause menu chosen. Two of the four have a lever and the other two are stepped over
	// before they get here (fv2_pause.h says what each is waiting on).
	auto chose_pause = [&](int line) {
		fprintf(stderr, "the console's pause menu: %s chosen\n", fv2_pause::name(line));
		if (line == fv2_pause::MAIN_MENU) {
			// August's own lever, and this port already had it under another name: push the attract
			// sequencer to the end of its list with its timer expired and the game walks off the end
			// itself, which is the one road that installs the title screen the way the game does.
			// FROM A FIGHT THAT LEVER DOES NOTHING, measured twice (fv2_pause.h): the attract sequencer is
			// not running there, so there is no list to walk off. The road out of a fight is the match's own
			// end -- finished off screen in about two seconds, then the end screens cut short -- and the
			// title lever is pulled at the far side of it, where it works (console_play::arrive).
			if (board.fv2_in_attract()) board.fv2_attract_to_title();
			else {
				fprintf(stderr, "the console: leaving the match for the title, the game's own exit says %d\n",
						board.fv2_end_match());
				cplay.leave_match(console_play::L_TO_TITLE);
			}
		} else if (line == fv2_pause::CHANGE_VIPER) {
			if (board.fv2_in_attract()) cplay.choose_versus();
			else {
				fprintf(stderr, "the console: leaving the match for the select screen, the game's own exit says %d\n",
						board.fv2_end_match());
				cplay.leave_match(console_play::L_TO_SELECT);
			}
		} else if (line == fv2_pause::REMATCH) {
			// August's own road, and the one his notes did not expect: the game already restarts a fight
			// without the select screen, because it does it between every round (Model3::fv2_rematch). The
			// two task values are the ones console_play saw while the round was being fought -- the loser's
			// register has been stopped by now, so reading it here would hand back a dead one.
			const int said = board.fv2_rematch(cplay.task_live[0], cplay.task_live[1]);
			fprintf(stderr, "the console's pause menu: REMATCH says %d (1 written, 0 no life block to trust), "
					"the tasks seen fighting were %08X and %08X\n",
					said, cplay.task_live[0], cplay.task_live[1]);
		} else if (line == fv2_pause::CHANGE_STAGE) {
			// THE CAROUSEL, the very page the V.S. line opens, instead of a scene number stepped sideways
			// with no picture to go by. Its answer holds the draw's table (chose_stage above), and from
			// here that means the fight is restarted in what was chosen, because a fight's arena is
			// already loaded and the game only loads one at round 1.
			if (stages.ready) {
				pause.hide();
				stages.show();
				// NOT PARKED HERE, and that was the fault: in GAME the sub-state 2 the park writes is
				// ROUND_MASK_DSP, not Sega's empty STAGE_SEL, so parking a fight broke it -- which is
				// Frederic's "change stage ne fonctionne pas" (model3.cpp refuses it outside SEL now).
				// The page freezes the game instead, exactly as the pause menu it was opened from did.
				stage_from_pause = !board.fv2_in_attract();
				cplay.swallow_start = true;
				cplay.start_was = true;
				sync_menu_sound();
				return;
			}
		} else if (line == fv2_pause::RESUME) {
			// Nothing to do but close, which is what B and Start already did -- the line is there so the
			// way back is written down rather than guessed at.
		}
		pause.hide();
		cplay.swallow_start = true;
		cplay.start_was = true;
		sync_menu_sound();
	};

	// ---- test hooks ----
	std::vector<std::pair<SDL_Scancode, uint64_t>> test_keys;   // VF3_KEYS
	std::vector<uint64_t> window_shots;
	if (const char *k = getenv("VF3_KEYS"))
		for (std::string all = std::string(k) + ","; !all.empty(); all.erase(0, all.find(',') + 1)) {
			const std::string t = all.substr(0, all.find(','));
			const size_t at = t.find('@');
			if (t.empty() || at == std::string::npos) continue;
			test_keys.push_back({ SDL_GetScancodeFromName(t.substr(0, at).c_str()), strtoull(t.c_str() + at + 1, nullptr, 10) });
		}
	// VF3_KEYS_S: keys sent at a real-time mark instead of at a game frame, which is what it takes to drive
	// anything drawn over a frozen game (see the top). Each is sent once.
	struct timed_key { SDL_Scancode key; double at; bool sent; };
	std::vector<timed_key> keys_s;
	if (const char *k = getenv("VF3_KEYS_S"))
		for (std::string all = std::string(k) + ","; !all.empty(); all.erase(0, all.find(',') + 1)) {
			const std::string t = all.substr(0, all.find(','));
			const size_t at = t.find('@');
			if (t.empty() || at == std::string::npos) continue;
			keys_s.push_back({ SDL_GetScancodeFromName(t.substr(0, at).c_str()), atof(t.c_str() + at + 1), false });
		}
	// VF3_MIDI_SEND: bytes handed to the sound board at a given game frame, as if the game had written them.
	// For asking the board a question -- does this one stop the music? -- and reading the answer in the peaks,
	// before anything in the menu rests on the answer.
	std::vector<std::pair<uint64_t, std::vector<uint8_t>>> midi_send;
	if (const char *k = getenv("VF3_MIDI_SEND"))
		for (std::string all = std::string(k) + ","; !all.empty(); all.erase(0, all.find(',') + 1)) {
			const std::string t = all.substr(0, all.find(','));
			const size_t at = t.find('@');
			if (t.empty() || at == std::string::npos || at % 2) continue;
			std::vector<uint8_t> bytes;
			for (size_t i = 0; i + 1 < at; i += 2) bytes.push_back(uint8_t(strtoul(t.substr(i, 2).c_str(), nullptr, 16)));
			if (!bytes.empty()) midi_send.push_back({ strtoull(t.c_str() + at + 1, nullptr, 10), bytes });
		}
	// The stage page answered, and the half second it is held for before the fight starts (fv2_stage_list.h)
	int stage_wanted = -1;
	uint64_t stage_locked_ns = 0;
	// The pause menu answered, and the moment it was: the line blinks while its sound plays (fv2_pause.h)
	int pause_wanted = -1;
	uint64_t pause_locked_ns = 0;
	std::vector<uint64_t> shot_at;   // VF3_SHOT_AT: the window at these passes, to build/host_window_at_N.png
	if (const char *k = getenv("VF3_SHOT_AT"))
		for (const char *q = k; *q;) { shot_at.push_back(strtoull(q, nullptr, 10)); while (*q && *q != ',') q++; if (*q) q++; }
	std::vector<std::string> menu_keys;   // VF3_MENU_KEYS
	if (const char *k = getenv("VF3_MENU_KEYS"))
		for (std::string all = std::string(k) + ","; !all.empty(); all.erase(0, all.find(',') + 1))
			if (all.find(',') > 0) menu_keys.push_back(all.substr(0, all.find(',')));
	auto send_key = [&](SDL_Scancode sc) {
		SDL_Event e{};
		e.type = SDL_EVENT_KEY_DOWN;
		e.key.scancode = sc;
		e.key.key = SDL_GetKeyFromScancode(sc, SDL_KMOD_NONE, false);
		e.key.down = true;
		SDL_PushEvent(&e);
		e.type = SDL_EVENT_KEY_UP;
		e.key.down = false;
		SDL_PushEvent(&e);
	};

	// VF3_RECORD: the recording (see the top)
	struct {
		std::string dir;
		uint64_t from = 0, to = UINT64_MAX, n = 0, base = 0;   // game frames; n: the pictures recorded; base: frames_run then
		bool on = false;
		FILE *video = nullptr, *wav = nullptr;
		uint32_t wav_bytes = 0;
		int w = 0, h = 0;
	} rec;
	if (const char *d = getenv("VF3_RECORD")) {
		rec.dir = d;
		std::filesystem::create_directories(rec.dir, ec);
		if (const char *f = getenv("VF3_RECORD_FROM")) rec.from = strtoull(f, nullptr, 10);
		if (const char *f = getenv("VF3_RECORD_TO")) rec.to = strtoull(f, nullptr, 10);
	}
	auto wav_header = [&] {
		const uint32_t rate = SOUND_RATE, br = rate * 4, sz = 36 + rec.wav_bytes;
		const uint16_t fmt = 1, ch = 2, align = 4, bits = 16;
		const uint32_t sixteen = 16;
		fseek(rec.wav, 0, SEEK_SET);
		fwrite("RIFF", 1, 4, rec.wav); fwrite(&sz, 4, 1, rec.wav); fwrite("WAVEfmt ", 1, 8, rec.wav);
		fwrite(&sixteen, 4, 1, rec.wav); fwrite(&fmt, 2, 1, rec.wav); fwrite(&ch, 2, 1, rec.wav);
		fwrite(&rate, 4, 1, rec.wav); fwrite(&br, 4, 1, rec.wav); fwrite(&align, 2, 1, rec.wav); fwrite(&bits, 2, 1, rec.wav);
		fwrite("data", 1, 4, rec.wav); fwrite(&rec.wav_bytes, 4, 1, rec.wav);
		fseek(rec.wav, 0, SEEK_END);
	};
	auto rec_stop = [&] {
		if (rec.video) pclose(rec.video);
		if (rec.wav) { wav_header(); fclose(rec.wav); }
		if (rec.on) fprintf(stderr, "record: %llu pictures, %.1f s, to %s\n", (unsigned long long)rec.n, rec.n / 60.0, rec.dir.c_str());
		rec.video = rec.wav = nullptr;
		rec.on = false;
	};

	// ---- the loop ----
	// Pacing (as VF2's): the game's frames by the clock, one every 1/57.52 s; the sound card's clock is the one
	// that counts: the frame clock is nudged (1% at most) to keep about QUEUED frames of sound queued, and a queue
	// running low gets its frames at once. A picture for each pass (each refresh of the screen, by vsync): with
	// smooth motion, between the last two frames by the frame clock; else the newest frame's, once.
	double frame_clock = 0, queue_avg = 0;
	double screen_hz = 60;   // (looked at once a second, below)
	bool vsync_on = true;    // (the renderer's vsync as it reports it, looked at with the refresh rate)
	// A picture each refresh with smooth motion: the frame clock moves a refresh's time each pass, pulled a tenth of
	// the way to the real clock (a real hitch, more than 2 refreshes off: taken whole). By the real clock, each
	// picture showed the game where it was when that pass happened to draw, early or late in the refresh as the
	// game's frame took its time (on a Steam Deck 4-8 ms in some passes, none in others), and shown a refresh apart
	// the motion wobbled: 60 pictures a second that didn't feel smooth.
	double even_real = 0, even_shown = 0, pass_avg = 0;   // (pass_avg: the passes' time apart, averaged)
	uint64_t frames_run = 0, clock_t = SDL_GetTicksNS(), pass = 0;
	bool shown = false, first_shown = false;
	bool new_frame = true;
	auto restart_clock_real = [&] { frame_clock = double(frames_run); clock_t = SDL_GetTicksNS(); even_real = even_shown = 0; };
	pace.t0 = SDL_GetTicksNS();
	int shot_n = 0;
	m3::Controls script_ctl;
	// The renderer's device or its targets reset (Direct3D's after a driver update, a display change; Direct3D 9's
	// render targets): every texture let go of and made again, the menu's too, and the newest frame drawn again, as
	// VF1's and VF2's. The device lost for good: the game closes (the log says so).
	auto textures_lost = [&](bool device) {
		fprintf(stderr, "the renderer's %s reset: its textures made again\n", device ? "device" : "targets");
		if (tex3d) { SDL_DestroyTexture(tex3d); tex3d = nullptr; tex_w = tex_h = 0; }
		if (g3.tex) { SDL_DestroyTexture(g3.tex); g3.tex = nullptr; g3.w = g3.h = 0; }
		SDL_DestroyTexture(tex_below);
		SDL_DestroyTexture(tex_above);
		make_2d(tex_below, video::WW);
		SDL_SetTextureScaleMode(tex_below, cfg.smooth_bg ? SDL_SCALEMODE_LINEAR : SDL_SCALEMODE_NEAREST);
		make_2d(tex_above, video::W);
		if (glass) { SDL_DestroyTexture(glass); glass = nullptr; glass_w = glass_h = 0; }
		menu.textures_lost();
		if (cplay.on) {   // (nothing of this exists in the other styles)
			modes.textures_lost(ren, find_assets());
			console_font.textures_lost(ren, find_assets());
			options.textures_lost(ren, find_assets());
			pause.textures_lost(ren, find_assets());
			stages.textures_lost(ren, find_assets());
			train.textures_lost(ren, find_assets());
		}
		new_frame = true;   // (the next picture sends the 2D again)
	};
	// Started from a terminal, the window can open behind it or without the keyboard (the keys go to the terminal):
	// it asks for it over about 5 s, and the log says whether it got it (as VF1's and VF2's)
	int focus_checks = 300;
	// smooth motion's pictures between frames: at most 120 a second (as VF1's and VF2's: a 144 or 240 Hz screen
	// gets a picture at its refreshes up to that), none while the window can't be seen
	double smooth_budget = 0;
	uint64_t smooth_t = SDL_GetTicksNS();
	// the window's title: the scale and the picture's shape; F9, the timings every 2 s instead (and in the log)
	std::string title_now;
	struct { uint64_t t0 = 0; int frames = 0, pictures = 0; double emu_ms = 0, draw_ms = 0; } t9;
	t9.t0 = SDL_GetTicksNS();
	uint64_t save_t = SDL_GetTicksNS();   // the settings saved every 3 s when changed (the NVRAM once a minute: nvram_t)
	while (running) {
		pass++;
		// the test runs' limit in seconds (above): this pass still draws and still writes its pictures
		static const uint64_t run_t0 = SDL_GetTicksNS();
		if (quit_seconds >= 0 && double(SDL_GetTicksNS() - run_t0) / 1e9 >= quit_seconds) running = false;
		const uint64_t pass_t0 = SDL_GetTicksNS();
		int pass_kind = 0;   // (the pace line's: 0 a picture only, 1 a frame's first half, 2 its second, 3 a whole frame)
		uint64_t pass_work_ns = 0;
		const bool menu_was_open = menu.is_open();
		if (!menu_keys.empty() && pass >= 30 && (pass - 30) % 6 == 0 && (pass - 30) / 6 < menu_keys.size()) {
			const std::string &k = menu_keys[(pass - 30) / 6];
			SDL_Scancode sc = k == "esc" ? SDL_SCANCODE_ESCAPE : k == "up" ? SDL_SCANCODE_UP : k == "down" ? SDL_SCANCODE_DOWN
				: k == "left" ? SDL_SCANCODE_LEFT : k == "right" ? SDL_SCANCODE_RIGHT : k == "enter" ? SDL_SCANCODE_RETURN
				: SDL_GetScancodeFromName(k.c_str());
			send_key(sc);
		}
		// VF3_KEYS_S: the keys that go by the clock, so they arrive even with the game frozen
		for (timed_key &k : keys_s)
			if (!k.sent && double(SDL_GetTicksNS() - run_t0) / 1e9 >= k.at) {
				k.sent = true;
				send_key(k.key);
				fprintf(stderr, "%.1f s: key %s sent\n", k.at, SDL_GetScancodeName(k.key));
			}
		SDL_Event e;
		while (SDL_PollEvent(&e)) {
			if (e.type == SDL_EVENT_QUIT) running = false;
			if (e.type == SDL_EVENT_RENDER_DEVICE_RESET || e.type == SDL_EVENT_RENDER_TARGETS_RESET) textures_lost(e.type == SDL_EVENT_RENDER_DEVICE_RESET);
			if (e.type == SDL_EVENT_RENDER_DEVICE_LOST) { fprintf(stderr, "the graphics device was lost: closing\n"); running = false; }
			if (e.type == SDL_EVENT_GAMEPAD_ADDED || e.type == SDL_EVENT_GAMEPAD_REMOVED) { open_pads(); menu.refresh(); }
			if (e.type == SDL_EVENT_WINDOW_ENTER_FULLSCREEN || e.type == SDL_EVENT_WINDOW_LEAVE_FULLSCREEN) {
				menu.refresh();
				if (!hidden && SDL_GetKeyboardFocus() != win) SDL_RaiseWindow(win);   // (the move to a new Space can lose it)
			}
			if (menu.handle_event(e)) continue;
			// Two of this port's own keys reach past the console's surfaces: Escape, so its menu is always
			// available, and F12, so a page can be photographed like anything else on screen. Everything
			// else belongs to the surface while it is up.
			auto ours = [](const SDL_Event &ev) {
				return ev.type == SDL_EVENT_KEY_DOWN
					   && (ev.key.scancode == SDL_SCANCODE_ESCAPE || ev.key.scancode == SDL_SCANCODE_F12);
			};
			// The console's list of modes, while it is up: its own surface takes the keys and the pad, and
			// Escape is left alone so this port's menu can always be reached.
			if (modes.shown && !ours(e)) {
				const int chose = modes.handle_event(e, cfg.ctl);
				if (chose >= 0) choose_mode(chose);
				else if (chose == fv2_mode_list::CANCELLED) {
					// Backed out: the list goes and the game comes back exactly where it was, on its own title
					// screen. Nothing else is touched -- the press stays ours, as when a line is chosen.
					modes.hide();
					cplay.swallow_start = true;
					cplay.start_was = true;
					fprintf(stderr, "the console's list: backed out\n");
				}
				if (e.type == SDL_EVENT_KEY_DOWN || e.type == SDL_EVENT_KEY_UP || e.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN
					|| e.type == SDL_EVENT_GAMEPAD_BUTTON_UP || e.type == SDL_EVENT_GAMEPAD_AXIS_MOTION) continue;
			}
			// The pause menu, the same way. It reads and writes nothing of the game's RAM while a frame runs,
			// so the wait below is for the same reason as the page's: the frame in flight when it opened may
			// not have finished.
			if (pause.shown && !ours(e)) {
				if (game.emu_thread.joinable()) game.job_wait();
				const int chose = pause.handle_event(e, cfg.ctl);
				if (chose == fv2_pause::CANCELLED) {
					pause.hide();
					cplay.swallow_start = true;
					cplay.start_was = true;
					sync_menu_sound();
					fprintf(stderr, "the console's pause menu: back to the fight\n");
				} else if (chose >= 0) {
					// NOT ANSWERED HERE. The line is held while its sound plays, as the carousel's is: the
					// menu used to close on the very frame A was pressed and, for the three and a half
					// seconds a way out of a match takes, nothing visibly happened -- so Frederic pressed
					// Start again, which paused the game, which stopped the very thing he had asked for.
					pause_wanted = chose;
					pause.lock();
					pause_locked_ns = SDL_GetTicksNS();
				}
				if (e.type == SDL_EVENT_KEY_DOWN || e.type == SDL_EVENT_KEY_UP || e.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN
					|| e.type == SDL_EVENT_GAMEPAD_BUTTON_UP || e.type == SDL_EVENT_GAMEPAD_AXIS_MOTION) continue;
			}
			// VERSUS's stage select, the same way. It touches no RAM of the game's at all -- it only hands a
			// scene number to console_play, which holds the patch later -- but it waits for the frame in flight
			// like the others, so a surface never acts half inside a frame.
			if (stages.shown && !ours(e)) {
				if (game.emu_thread.joinable()) game.job_wait();
				const int chose = stages.handle_event(e, cfg.ctl);
				if (chose == fv2_stage_list::CANCELLED) {
					// backed out: nothing is held, and the game picks its own arena as it does on the board.
					// FROM THE PAUSE MENU, BACKING OUT CHANGES NOTHING AT ALL: the fight behind the page has its
					// arena already, and letting go of the hold there would only decide the arena of some later
					// fight nobody asked about. The page closes and the fight carries on where it was.
					stages.hide();
					if (stage_from_pause) {
						stage_from_pause = false;
						fprintf(stderr, "the console stage select: backed out of the pause menu -- the fight is as it was\n");
					} else {
						cplay.stage_chosen(board, 0);
						fprintf(stderr, "the console stage select: backed out -- the game picks its own\n");
					}
					cplay.swallow_start = true;
					cplay.start_was = true;
					sync_menu_sound();
				} else if (chose >= 0) {
					// NOT ANSWERED HERE. The page holds half a second first, so the sound that says "chosen"
					// is heard over the carousel and not under the next screen (fv2_stage_list.h). The arena
					// is handed over when that half second is up, below.
					stage_wanted = chose;
					stages.lock();
					stage_locked_ns = SDL_GetTicksNS();
				}
				if (e.type == SDL_EVENT_KEY_DOWN || e.type == SDL_EVENT_KEY_UP || e.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN
					|| e.type == SDL_EVENT_GAMEPAD_BUTTON_UP || e.type == SDL_EVENT_GAMEPAD_AXIS_MOTION) continue;
			}
			// The OPTIONS page, the same way, and the same two keys left alone for the same reasons.
			if (options.shown && !ours(e)) {
				// This page reads and writes the game's own RAM, so not while a frame runs alongside. The page
				// freezes the game, but the frame that was in flight when it opened may not have finished.
				if (game.emu_thread.joinable()) game.job_wait();
				if (options.handle_event(e, cfg.ctl, board) == fv2_options::CANCELLED) close_options();
				// CONTROLLER rebound something: kept like any other setting of this port.
				if (options.ctl_changed) { options.ctl_changed = false; cfg_dirty = true; }
				// ENDING was asked for: the page has nothing more to say and the roll is behind it.
				if (ending_asked) { ending_asked = false; close_options(); }
				if (e.type == SDL_EVENT_KEY_DOWN || e.type == SDL_EVENT_KEY_UP || e.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN
					|| e.type == SDL_EVENT_GAMEPAD_BUTTON_UP || e.type == SDL_EVENT_GAMEPAD_AXIS_MOTION) continue;
			}
			// Fullscreen also with F and Alt+Enter, as VF1's and VF2's (after the menu's own events: typing in its
			// fields never does it; F not while a player has it as a control, nor with Ctrl, Alt or the command key)
			auto fullscreen_key = [&](const SDL_KeyboardEvent &k) {
				if (k.scancode == SDL_SCANCODE_RETURN && (k.mod & SDL_KMOD_ALT)) return true;
				if (k.scancode != SDL_SCANCODE_F || (k.mod & (SDL_KMOD_CTRL | SDL_KMOD_ALT | SDL_KMOD_GUI))) return false;
				for (const auto &c : cfg.ctl) for (SDL_Scancode sc : c.key) if (sc == SDL_SCANCODE_F) return false;
				return true;
			};
			if (e.type == SDL_EVENT_KEY_DOWN && !e.key.repeat && fullscreen_key(e.key)) {
				set_fullscreen(!(SDL_GetWindowFlags(win) & SDL_WINDOW_FULLSCREEN));
				continue;
			}
			if (e.type == SDL_EVENT_KEY_DOWN && !e.key.repeat) {
				switch (e.key.scancode) {
				case SDL_SCANCODE_ESCAPE: open_menu(); break;
				case SDL_SCANCODE_P: paused = !paused; break;
				case SDL_SCANCODE_F3: reset_game(); break;
				case SDL_SCANCODE_F9:   // the timings in the title (and the log) every 2 s
					cfg.timings = !cfg.timings; cfg_dirty = true; title_now.clear();
					t9 = {}; t9.t0 = SDL_GetTicksNS();
					break;
				case SDL_SCANCODE_F5: set_scale(scale % MAX_SCALE + 1); break;
				case SDL_SCANCODE_F6: set_shape((aspect_i + 1) % N_ASPECTS); break;
				case SDL_SCANCODE_F11: set_fullscreen(!(SDL_GetWindowFlags(win) & SDL_WINDOW_FULLSCREEN)); break;
				case SDL_SCANCODE_F12: {
					// the picture as drawn (the render scale, antialiased, its shape), without the bars around it or
					// the screen filter, into Screenshots in the data folder, numbered, never overwritten (as VF2's)
					const std::string dir = data + "/Screenshots";
					std::filesystem::create_directories(dir, ec);
					char name[32];
					do snprintf(name, sizeof name, "/vf3_%04d.png", ++shot_n);
					while (std::filesystem::exists(dir + name, ec) && shot_n < 9999);
					SDL_FRect all, mid;
					picture_rect(all, mid);
					const SDL_Rect area = { int(std::lround(all.x)), int(std::lround(all.y)), int(std::lround(all.w)), int(std::lround(all.h)) };
					compose(false);
					draw_console(board);
					read_window(dir + name, &area);
					fprintf(stderr, "screenshot %s\n", (dir + name).c_str());
					break;
				}
				default: break;
				}
			}
		}
		sync_menu_sound();
		if (menu_was_open && !menu.is_open()) {
			if (cfg_dirty && quit_after < 0 && cfg.save(settings_path)) cfg_dirty = false;
			if (!arcade_before.empty()) {
				const bool changed = arcade_before != arcade_bytes();
				arcade_before.clear();
				if (changed) reset_game();
			}
			pstate[0] = pstate[1] = pad_state{};
			pstate[0].back_was = pstate[1].back_was = true;
			pstate[0].back_combo = pstate[1].back_combo = true;
			pstate[0].start_was = pstate[1].start_was = true;
			for (pad_state &s : pstate) s.menu_was = s.touch_was = s.sticks_was = true;
			restart_clock_real();
			gov_grace(2);
		}
		if (clock_reset) { clock_reset = false; frames_run = 0; restart_clock_real(); new_frame = true; }
		// the console's list of modes freezes the game behind it as the menu does: the title screen it is drawn
		// on lasts 192 frames of its own (measured), and a list that let the game walk on to the demo would be
		// a list over a demonstration
		if (modes.shown && menu.is_open()) modes.hide();   // (Escape opened this port's menu over it)
		if (options.shown && menu.is_open()) close_options();
		// VF3_CONSOLE_MODE=arcade|versus|survival|random|training, at game frame VF3_CONSOLE_AT
		// (1210 by default, a few frames after the list is up): the console's mode chosen without a
		// hand, the way vf3run's --console-mode does it. Written because the only other way in was
		// VF3_KEYS_S, which goes by the CLOCK while the list goes by frames -- so a fast boot or a
		// busy machine moved the keys off the list and a measurement silently became a run that
		// chose nothing at all. Twice, on the 2026-10-25.
		//
		// AND IT WAITS FOR THE LIST, not for a frame number. The first version fired at a game frame
		// and deadlocked: the road's Start opens the list at frame 1200, the list FREEZES the game, so
		// frame 1210 never came and the run sat on the list for seventy-five seconds. The list being
		// up is the condition a hand would use, and it is the one used here.
		if (static const char *cm = getenv("VF3_CONSOLE_MODE"); cm && cplay.on) {
			static const uint64_t at = getenv("VF3_CONSOLE_AT") ? strtoull(getenv("VF3_CONSOLE_AT"), nullptr, 10) : 0;
			static bool done = false;
			if (!done && modes.shown && game_frame >= at) {
				done = true;
				if (!strcmp(cm, "versus")) cplay.choose_versus();
				else if (!strcmp(cm, "survival")) cplay.choose_survival(board);
				else if (!strcmp(cm, "random")) cplay.choose_random();
				else if (!strcmp(cm, "training")) cplay.choose_training();
				else cplay.choose_arcade();
				modes.hide();
				music_mode_chosen = true;
				cplay.swallow_start = true;
				cplay.start_was = true;
				sync_menu_sound();
				fprintf(stderr, "frame %llu: VF3_CONSOLE_MODE=%s chosen without a hand\n",
						(unsigned long long)game_frame, cm);
			}
		}
		// The pause menu's answer, held while its sound is heard and then acted on
		if (pause.shown && pause.locked) {
			pause.lock_t = double(SDL_GetTicksNS() - pause_locked_ns) / 1e9;
			if (pause.lock_t >= fv2_pause::HOLD_S && pause_wanted >= 0) {
				const int line = pause_wanted;
				pause_wanted = -1;
				chose_pause(line);
			}
		}
		// The half second the answered page is held for: the carousel stays, its chosen picture blinks, the
		// sound is heard over it, and only then is the arena handed to the game. The game is parked and
		// running underneath all the while, so the music never breaks (fv2_stage_list.h).
		if (stages.shown && stages.locked) {
			stages.lock_t = double(SDL_GetTicksNS() - stage_locked_ns) / 1e9;
			if (stages.lock_t >= fv2_stage_list::HOLD_S && stage_wanted >= 0) {
				const int line = stage_wanted;
				stage_wanted = -1;
				chose_stage(line);
			}
		}
		// VF3_CONSOLE_STATS=row: the console's OPTIONS opened straight on STATS with the cursor on that
		// row, and row>=100 opens that character's page (row-100). The fourth of this family, and for the
		// same reason: the page FREEZES the game, so the only other way in is VF3_KEYS_S, which goes by the
		// clock -- six key presses timed against a boot whose length is the machine's.
		if (static const char *cs2 = getenv("VF3_CONSOLE_STATS"); cs2 && cplay.on) {
			static const int want = atoi(cs2);
			static bool done = false;
			if (!done && modes.shown && options.ready) {
				done = true;
				modes.hide();
				options.show();
				options.at = fv2_options::SCR_STATS;
				options.vs_row_sel = (want >= 100 ? want - 100 : want) % 14;   // (the fourteen characters)
				options.vs_chara = want >= 100 ? options.vs_row_sel : -1;
				cplay.swallow_start = true;
				cplay.start_was = true;
				sync_menu_sound();
				fprintf(stderr, "frame %llu: VF3_CONSOLE_STATS=%d -- STATS opened without a hand\n",
						(unsigned long long)game_frame, want);
			}
		}
		// VF3_CONSOLE_ENDING=frame: the game's staff roll asked for at that frame, and VF3_ENDING_DC=1 to
		// watch it the Dreamcast's way. Written to see the thing at all: reaching the ending for real means
		// finishing the ladder, and this port's own notes have it among the states never reached.
		if (static const char *ce = getenv("VF3_CONSOLE_ENDING"); ce && cplay.on) {
			static const uint64_t at = strtoull(ce, nullptr, 10);
			static bool done = false;
			if (!done && game_frame >= at) {
				done = true;
				if (getenv("VF3_ENDING_DC")) fin.want = true;
				cplay.ask_ending(board);
				fprintf(stderr, "frame %llu: VF3_CONSOLE_ENDING -- the staff roll asked for (%s)\n",
						(unsigned long long)game_frame, fin.want ? "DREAMCAST" : "ARCADE");
			}
		}
		// VF3_CONSOLE_PAUSE=n: the pause menu answered without a hand -- 0 TITLE SCREEN, 1 CHANGE YOUR VIPER,
		// 2 REMATCH, 3 CHANGE STAGE -- as soon as it is up, however it was opened. The third of this family,
		// and for the same reason as the first two: the menu FREEZES the game, so VF3_KEYS_S goes by a clock
		// the game no longer shares and a measurement silently becomes a run that chose nothing.
		if (static const char *cp = getenv("VF3_CONSOLE_PAUSE"); cp) {
			static const int want = atoi(cp);
			static bool done = false;
			if (!done && pause.shown) {
				done = true;
				fprintf(stderr, "frame %llu: VF3_CONSOLE_PAUSE=%d (%s) chosen without a hand\n",
						(unsigned long long)game_frame, want, fv2_pause::name(want));
				chose_pause(want);
			}
		}
		// VF3_CONSOLE_STAGE=n (1-14, 0 to back out and let the game choose, "none" so the page never opens
		// at all): the versus stage page answered without a hand, the companion of VF3_CONSOLE_MODE above
		// and waiting on the same condition -- THE PAGE BEING UP, not a frame number. It takes the SAME road
		// a hand takes, the half-second hold included, so what it measures is what a player gets.
		if (static const char *cs = getenv("VF3_CONSOLE_STAGE"); cs) {
			static const int want = atoi(cs);
			static bool done = false;
			if (!done && stages.shown && !stages.locked) {
				done = true;
				if (want <= 0) {
					stages.hide();
					if (stage_from_pause) stage_from_pause = false;
					else cplay.stage_chosen(board, 0);
					cplay.swallow_start = true;
					cplay.start_was = true;
					sync_menu_sound();
					fprintf(stderr, "frame %llu: VF3_CONSOLE_STAGE=0 -- backed out without a hand\n",
							(unsigned long long)game_frame);
				} else {
					stages.sel = (want - 1) % (stages.have > 0 ? stages.have : 1);
					stages.want_sound = fv2_stage_list::SND_VALIDATE;
					stage_wanted = stages.sel;
					stages.lock();
					stage_locked_ns = SDL_GetTicksNS();
					fprintf(stderr, "frame %llu: VF3_CONSOLE_STAGE=%d chosen without a hand\n",
							(unsigned long long)game_frame, want);
				}
			}
		}
		// AND THE STAGE PAGE IS NOT IN THIS LIST, alone of the four surfaces. The other three freeze the
		// PowerPC; this one leaves the game running and parks it in Sega's own empty STAGE_SEL sub-state
		// instead (model3.cpp). Frozen, the select screen it had made drained away -- the two fighters
		// vanished, the music stopped, and nothing could be heard moving through the carousel. Parked, the
		// game goes on drawing that screen and the sound board goes on playing, because the game never
		// stopped. Its controls are silenced below instead, so the player's directions turn the carousel
		// and not the game's own cursor.
		// The carousel is the one surface that does not always freeze: on the select screen the game runs on
		// under it, parked. Opened from the pause menu there is nothing to park in, so there it freezes like
		// the other three.
		const bool stopped = paused || menu.is_open() || modes.shown || options.shown || pause.shown
			|| (stages.shown && stage_from_pause);
		const bool ff = !stopped && SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_TAB];
		int due = 0;
		// ---- the console's surfaces keep the sound board turning ----
		//
		// The list and the OPTIONS page freeze the PowerPC, so no game frame runs and no sound frame with it:
		// a menu that beeps has to turn the board over itself. BY AN ACCUMULATOR, AND THAT IS THE WHOLE POINT.
		// The version undone on the 2026-10-21 ran one sound frame per pass, gated on 17.38 ms having gone by,
		// which on a 60 Hz screen gave thirty a second where the board wants 57.5 -- the queue starved and it
		// rasped. Time is measured and the frames owed are made, however many passes it takes.
		//
		// This runs only under the console's own surfaces, never under this port's Escape menu or a pause:
		// those two are its, and have always been silent.
		// ---- the music under the console's surfaces ----
		//
		// THE WHOLE TREE IS ONE STATE, which is Frederic's own reading and what makes this writable at all.
		// The list and the OPTIONS tree are one surface: the music stops on going in and is decided on coming
		// out, so moving about inside -- a line, a door, a screen backed out of, a value changed -- decides
		// nothing, because nothing has been left. Without that, every move the tree added would have wanted a
		// case of its own in the table of docs/fv2-console-son.md, and the table is held over.
		//
		// What happens on the way out is that table, and each cell was measured there:
		//   a mode chosen      NOTHING. The select screen lays its own (A0 00 01 then A0 10 00, frame 1257),
		//                      so putting the demo's back would be worse than leaving it -- it is what made
		//                      August's select screen inherit the demo's music;
		//   backed out with B  put back what was playing;
		//   Escape over it     put back too. That was hole no. 1 of the version undone on the 2026-10-21;
		//   a line with no lever  nothing happens and the list stays up, so there is no way out to speak of.
		//
		// And nothing is stopped that was not playing. On a title screen reached straight from the SEGA logo
		// there is no music at all, and music_now says so -- which is the trap the first measurement of this
		// job fell into, proving a silence where there had been nothing to cut.
		// A track started by the SOUNDS page's test is stopped when that page is left, by its own silence
		// command. Otherwise a test followed the player back into the game, which is not a test any more.
		{
			static int was_at = fv2_options::SCR_OPTIONS;
			const int now_at = options.shown ? options.at : fv2_options::SCR_OPTIONS;
			if (was_at == fv2_options::SCR_SOUNDS && now_at != fv2_options::SCR_SOUNDS && bgm_testing) {
				game.stop_music();
				bgm_testing = false;
				bgm_playing = -1;
			}
			was_at = now_at;
		}
		{
			static bool was_up = false;
			const bool up = modes.shown || options.shown;
			if (up && !was_up) {
				// THE QUEUE IS EMPTIED ON THE WAY IN, and that is what makes the menu's sounds prompt.
				// Frederic heard them "en decale" -- late, and only once the game started again. The board
				// was making them on time all along, measured: what was late was the SDL queue, which the
				// game had run ahead into before it froze. A beep put behind a second of backlog comes out a
				// second later. Nothing drains that backlog while the game is stopped, since the pump below
				// makes exactly as much as real time asks for.
				if (audio) SDL_ClearAudioStream(audio);
				game.audio.clear();
				// Emptied, then PRIMED. Left at nothing the device is starved, which is the other way to make
				// it rasp: the pump only makes what real time asks for, so it never gets ahead on its own.
				// Three frames is what the game itself keeps ahead (QUEUED).
				menu_sound_owed = QUEUED;
				menu_sound_last = 0;
				menu_sound_frames = 0;
				menu_sound_t0 = 0;
				music_held = game.music_now;
				if (music_held >= 0) {
					game.stop_music();
					fprintf(stderr, "the console's music: A0 10 %02X was playing, stopped\n", music_held);
				}
			} else if (!up && was_up) {
				if (music_held >= 0 && !music_mode_chosen) {
					game.play_music(uint8_t(music_held));
					fprintf(stderr, "the console's music: A0 10 %02X put back\n", music_held);
				} else if (music_held >= 0)
					fprintf(stderr, "the console's music: a mode was chosen, the select screen lays its own\n");
				music_held = -1;
				music_mode_chosen = false;
			}
			was_up = up;
		}
		// The three menu sounds, which are the game's own. The owner found them by sending notes and listening,
		// and the numbers are his: sd_ketsu_00 no. 391 moves, sd_sign_16 no. 388 decides (and is also the one
		// that opens), sd_sonic00_11 no. 392 backs out. Each surface says WHAT happened and this says what that
		// sounds like, so neither has to know the other's job.
		//
		// Drained here, once a pass, rather than where the events are dispatched: those blocks `continue` on
		// every key and pad event, so a note handed over inside them would not leave until the event after.
		//
		// This is step 4 of docs/fv2-console-son.md and nothing more -- the EFFECTS, with the music left
		// exactly alone. The step before it, the table of what the music does on each of the four ways out,
		// is deliberately skipped for now and will be filled in later.
		{
			auto heard = [&](int want) {
				switch (want) {
				case fv2_options::SND_MOVE: game.play_effect(0x26); break;
				case fv2_options::SND_VALIDATE: game.play_effect(0x23); break;
				case fv2_options::SND_CANCEL: game.play_effect(0x27); break;
				default: break;
				}
			};
			if (modes.want_sound) { heard(modes.want_sound); modes.want_sound = fv2_mode_list::SND_NONE; }
			if (options.want_sound) { heard(options.want_sound); options.want_sound = fv2_options::SND_NONE; }
			if (pause.want_sound) { heard(pause.want_sound); pause.want_sound = fv2_pause::SND_NONE; }
			if (stages.want_sound) { heard(stages.want_sound); stages.want_sound = fv2_stage_list::SND_NONE; }
		}
		if (stopped && cplay.on && (modes.shown || options.shown)) {
			const uint64_t now = SDL_GetTicksNS();
			if (menu_sound_last) menu_sound_owed += double(now - menu_sound_last) / 1e9 * FRAME_HZ;
			menu_sound_last = now;
			menu_sound_owed = std::min(menu_sound_owed, 4.0);   // (a window dragged: catch up a little, never a flood)
			int made = 0;
			while (menu_sound_owed >= 1.0) { menu_sound_owed -= 1.0; game.sound_only_frame(); made++; }
			// (tests: VF3_MENU_SOUND=1 reports what the board made under the surface, which is the measure the
			// plan asks for -- the peak rises on a move where it was at zero -- and the rate, which is what
			// the undone version got wrong: 30 a second where the board wants 57.5.)
			// (tests: the rate and the counters start again at each opening. Counted across them they said
			// 43/s where the board makes 57.5, because the wall clock went on running while the surface was
			// shut and the frames did not -- a figure that reads like a fault and is one of the instrument.)
			if (static const bool say = getenv("VF3_MENU_SOUND") != nullptr; say && made) {
				int peak = 0;
				for (int16_t s : game.audio) peak = std::max(peak, std::abs(int(s)));
				menu_sound_frames += made;
				if (!menu_sound_t0) menu_sound_t0 = now;
				const int frames = menu_sound_frames;
				const double since = double(now - menu_sound_t0) / 1e9;
				// The queue is in the line too, and it is the only thing left that can make a sound late: the
				// board was measured making them on the key's own frame, so a beep heard late is a beep sitting
				// behind something. frame_bytes is one sound frame's worth, so the figure reads as frames of
				// delay.
				const int q = audio ? SDL_GetAudioStreamQueued(audio) : -1;
				if (peak > 0 || frames % 60 == 0)
					fprintf(stderr, "menu sound: %.2f s, %d sound frames (%.1f/s), peak %d, queued %d bytes (%.1f frames)\n",
							since, frames, since > 0 ? frames / since : 0.0, peak, q,
							frame_bytes > 0 ? double(q) / frame_bytes : 0.0);
			}
			if (audio && !game.audio.empty() && rec.dir.empty())
				SDL_PutAudioStreamData(audio, game.audio.data(), int(game.audio.size() * sizeof(int16_t)));
			game.audio.clear();
		}
		if (stopped) {
			pace.stopped_s += double(SDL_GetTicksNS() - clock_t) / 1e9;   // (the menu's time isn't the game's)
			restart_clock_real();
		}
		else if (!rec.dir.empty()) {
			// the recording: before it starts, frames as fast as they go; then 60 pictures a second by the game's clock
			if (!rec.on && game_frame >= rec.from) {
				rec.on = true;
				rec.from = game_frame;
				rec.base = frames_run;
				int ww = 0, wh = 0;
				SDL_GetCurrentRenderOutputSize(ren, &ww, &wh);
				rec.w = ww & ~1; rec.h = wh & ~1;
				const std::string cmd = "ffmpeg -loglevel error -y -f rawvideo -pix_fmt rgba -s " + std::to_string(rec.w) + "x" + std::to_string(rec.h) +
				                        " -r 60 -i - -c:v libx264 -preset slow -crf 12 -pix_fmt yuv420p '" + rec.dir + "/video.mp4'";
				rec.video = popen(cmd.c_str(), "w");
				rec.wav = fopen((rec.dir + "/sound.wav").c_str(), "wb");
				if (rec.wav) wav_header();
				fprintf(stderr, "record: from frame %llu, %dx%d\n", (unsigned long long)rec.from, rec.w, rec.h);
			}
			if (!rec.on) { due = 8; frame_clock = double(frames_run + due); }
			else {
				frame_clock = double(rec.base) + double(rec.n + 1) * FRAME_HZ / 60.0;
				due = std::max(0, int(int64_t(std::floor(frame_clock)) - int64_t(frames_run)));
			}
			frames_run += due;
		}
		else {
			const uint64_t now_ns = SDL_GetTicksNS();
			double dt = double(now_ns - clock_t) / 1e9;
			clock_t = now_ns;
			// (only while the passes keep to the screen's refreshes, a whole number of them apart on average: not with
			// vsync off or not honoured, a minimised or covered window, or a refresh rate that isn't the screen's; each
			// pass moves the clock by the whole refreshes it took, so a picture every other refresh steps evenly too,
			// and never back)
			static const bool no_even = getenv("VF3_NO_EVEN_PACE") != nullptr;   // (tests: by the real clock, as before)
			pass_avg += (dt - pass_avg) * 0.05;
			const double per = pass_avg * screen_hz, whole = std::round(per);
			const bool unseen_now = !hidden && (SDL_GetWindowFlags(win) & (SDL_WINDOW_MINIMIZED | SDL_WINDOW_OCCLUDED));
			if (!no_even && cfg.smooth && !hidden && !unseen_now && screen_hz <= (gov.smooth_off ? 60 : 120) * 1.05 &&
				whole >= 1 && std::fabs(per - whole) < 0.12) {
				const double unit = 1.0 / screen_hz, step = std::max(1.0, std::round(dt * screen_hz)) * unit;
				even_real += dt;
				const double behind = even_real - even_shown;
				dt = std::fabs(behind - step) > 2 * unit ? behind : step + (behind - step) * 0.1;
				dt = std::max(0.0, dt);
				even_shown += dt;
				if (even_shown > 1e6) { even_real -= even_shown; even_shown = 0; }   // (kept small)
			} else even_real = even_shown = 0;
			double rate = FRAME_HZ;
			int low = 0;
			bool sound_full = false;
			if (audio) {
				const int q = SDL_GetAudioStreamQueued(audio);
				if (q > frame_bytes * int(FRAME_HZ * 0.3)) sound_full = true;
				queue_avg += (q - queue_avg) * std::min(1.0, dt / 0.5);
				rate *= 1 - std::clamp((queue_avg / frame_bytes - QUEUED) * 0.005, -0.01, 0.01);
				if (q < frame_bytes) low = int(std::ceil(QUEUED - double(q) / frame_bytes));   // (under a frame's: extra frames)
			}
			frame_clock += dt * rate;
			due = int(int64_t(std::floor(frame_clock)) - int64_t(frames_run));
			if (due > 8) due = 1, frame_clock = double(frames_run + 1);
			due = std::max(due, low);
			if (due > 0) frame_clock = std::max(frame_clock, double(frames_run + due));
			due = std::max(due, 0);
			if (!ff && due > 2 && due > low) { frame_clock -= due - std::max(2, low); due = std::max(2, low); }
			if (ff) { due = 8; frame_clock = double(frames_run + due); }
			else if (sound_full) { due = 0; frame_clock = std::min(frame_clock, double(frames_run) + 1); }
			frames_run += due;
		}
		// the inputs the game reads during a frame, set before it starts (or its first half)
		auto frame_inputs = [&](hotkey &hk, InputScript::Actions &act, bool &left_test) {
				// the inputs the game reads during the next frame: the script's, then the player's
				m3::Controls live;
				hk = read_inputs(live, pads, pstate, cfg.ctl);
				act = InputScript::Actions();
				if (scripted) act = script.step(script_ctl, game_frame);
				// The console way's Start: one button, two jobs, told apart by where the game is
				// (console_play.h). On the game's own title screen it opens the console's list of modes over
				// that screen; anywhere else it asks the game for its title screen. The press itself never
				// reaches the game either way, and nothing here touches the credit -- the board is on FREE
				// PLAY in this style, and a mode chosen from the list hands the game a Start of its own.
				//
				// In the PC way and the arcade way Start goes to the game untouched, as it always did.
				//
				// A script's Start counts as a player's here, deliberately: it arrives through script_ctl
				// rather than the keyboard, and reading only the player's own would leave this whole path
				// impossible to exercise without hands on a pad -- which is exactly how it went unverified
				// the first time round.
				if (cplay.on) {
					const bool st = live.start[0] || live.start[1] || script_ctl.start[0] || script_ctl.start[1];
					// AND START IS DEAD WHILE A WAY OUT OF A MATCH IS RUNNING. Choosing TITLE SCREEN or
					// CHANGE YOUR VIPER finishes the match off screen over three and a half seconds; a
					// Start pressed in that interval used to pause the game, which froze the PowerPC and
					// stopped the exit, so the menu looked as if it had done nothing. Measured in
					// Frederic's own log of the 2026-10-25: the pause opened at frames 2200, 2236 and
					// 2272 and TITLE SCREEN was chosen three times over.
					bool busy = cplay.leaving != console_play::L_OFF;
					if (busy) cplay.swallow_start = true;
					const console_play::start_for what = busy
						? console_play::start_for::none
						: cplay.take_start(board, st, modes.shown || pause.shown || stages.shown);
					switch (what) {
					case console_play::start_for::list:
						modes.show();
						sync_menu_sound();
						fprintf(stderr, "frame %llu: Start on the title screen: the console's list of modes is up\n",
								(unsigned long long)game_frame);
						break;
					case console_play::start_for::title:
						fprintf(stderr, "frame %llu: Start off the title screen (mode %d, sub-state %d): the game asked for its title screen\n",
								(unsigned long long)game_frame, board.fv2_mode(), board.fv2_submode());
						break;
					case console_play::start_for::pause: {
						// The panel stands on the side of the player who paused.
						// WHICH LINES THIS MODE SHOWS (fv2_pause.h). REMATCH is out of TRAINING, where the
						// round restarts of itself and the lives are held. CHANGE STAGE is out of the
						// modes that follow the arcade's fixed road -- the arena is drawn once a match,
						// so there it would change the NEXT match's and not this one's.
						uint32_t on = fv2_pause::ALL;
						if (cplay.training_on) on &= ~(1u << fv2_pause::REMATCH);
						else if (!cplay.is_versus) on &= ~(1u << fv2_pause::CHANGE_STAGE);
						pause.on = on;
						pause.show(!(live.start[0] || script_ctl.start[0]) && (live.start[1] || script_ctl.start[1]) ? 1 : 0);
						sync_menu_sound();
						fprintf(stderr, "frame %llu: Start in a game (mode %d, sub-state %d): the console's pause menu is up\n",
								(unsigned long long)game_frame, board.fv2_mode(), board.fv2_submode());
						break;
					}
					case console_play::start_for::cut:
						fprintf(stderr, "frame %llu: Start in the presentation or the replay (mode %d, sub-state %d) -> the state is cut short\n",
								(unsigned long long)game_frame, board.fv2_mode(), board.fv2_submode());
						break;
					case console_play::start_for::none: break;
					}
					if (cplay.swallow_start) {
						live.start[0] = live.start[1] = false;
						script_ctl.start[0] = script_ctl.start[1] = false;
					}
				}
				// THE LOGO'S VOICE GOES WITH THE LOGO. Start off the title screen restarts the attract loop and
				// asks for the title at the SEGA logo, so the logo is never seen -- but the game says "SEGA"
				// (A0 30 00) on the very frame it would have drawn it, and that voice was arriving over the
				// title screen. Frederic heard it before this was measured. Skipping the picture and keeping
				// its voice is half a skip; `skip_logo` is exactly the road where the picture is skipped, so
				// the voice is dropped on the same road and nowhere else.
				game.mute_voices = cplay.skip_logo > 0;
				game.mute_fight_voice = cplay.training_on && cplay.train_hush > 0;
				cplay.step(board, console_ctl);
				// THE CHARACTERS HAVE BEEN CHOSEN: the stage page opens here, between SELECT and DECIDE_INT,
				// where the summer's ROM put it and where Sega's own STAGE_SEL states sit empty. The game is
				// NOT frozen under it -- console_play parks it in that empty sub-state and it goes on running
				// (model3.cpp) -- and the arena it holds is read ten frames later, at ROUND_MASK_INT.
				//
				// VF3_CONSOLE_STAGE=none: the page never opens and the versus starts as it did before there
				// was one. It is here for ONE measurement and it is worth its keep -- it is the only way to
				// put a fight that went through the page beside a fight that did not, on the same build, the
				// same script and the same machine.
				static const bool no_stage_page = [] {
					const char *s = getenv("VF3_CONSOLE_STAGE");
					return s && !strcmp(s, "none");
				}();
				// MAIN MENU has got the game back to its title screen: the console's list goes up over it, which
				// is where that line said it was going.
				// AND IT WAITS FOR THE TITLE TO BE THERE. The list freezes the PowerPC, so opened a moment too
				// early it froze the game on the SEGA logo with the menu in front of it -- what Frederic saw the
				// 2026-10-25. fv2_on_title is the game saying it itself.
				if (cplay.want_list && board.fv2_on_title()) {
					cplay.want_list = false;
					if (modes.ready && !modes.shown && !options.shown) {
						modes.show();
						music_mode_chosen = false;
						sync_menu_sound();
						fprintf(stderr, "frame %llu: MAIN MENU -- the list of modes is up\n",
							(unsigned long long)game_frame);
					}
				}
				if (cplay.want_stage_page) {
					cplay.want_stage_page = false;
					if (stages.ready && !stages.shown && !no_stage_page) {
						stages.show();
						cplay.swallow_start = true;
						cplay.start_was = true;
						sync_menu_sound();
						fprintf(stderr, "frame %llu: the fighters are chosen -- the console's stage select is up\n",
								(unsigned long long)game_frame);
					} else {
						// console_play parked the game on the tick it asked for the page; no page, so it is
						// let go again at once and the game walks into DECIDE_INT as it always did.
						board.fv2_leave_stage_select();
					}
				}
				// TRAINING'S COLUMN, fed with the PLAYER'S own controls and not with the game's memory --
				// which is the point of it: it has to show what was done, including what the game ignored.
				// Player 1's, and the damage it reads is what player 2 has just had taken off him.
				if (cplay.training_on && cplay.train_fighting && train.ready) {
					const bool st = live.start[0] || script_ctl.start[0];
					(void)st;
					train.feed(live.up[0] || script_ctl.up[0], live.down[0] || script_ctl.down[0],
							   live.left[0] || script_ctl.left[0], live.right[0] || script_ctl.right[0],
							   live.punch[0] || script_ctl.punch[0], live.kick[0] || script_ctl.kick[0],
							   live.guard[0] || script_ctl.guard[0]);
					if (cplay.train_took[1] > 0) train.damage(cplay.train_took[1]);
					cplay.train_took[0] = cplay.train_took[1] = 0;
				} else if (!cplay.training_on && !train.lines.empty()) {
					train.clear();
				}
				// TRAINING: ONE PLAYER CHOOSES BOTH FIGHTERS. On the select screen player 1's controls go to
				// his own cursor until he has pressed punch, and to PLAYER 2'S from then on, so the same hands
				// pick the opponent. Nothing of the game is touched: the controls are composed here, which is
				// the one place this port composes them.
				//
				// AND IT COMES AFTER THE COLUMN, not before. Routed first, it had already emptied player 1's
				// controls by the time the column read them, so the column showed nothing of what was being
				// pressed -- Frederic saw the buttons stop matching. The column reads the hands; this moves
				// what the hands did to the other player; the order between them is not free.
				if (cplay.training_on && (board.fv2_mode() == 4 || board.fv2_mode() == 5)) {
					const bool punch = live.punch[0] || script_ctl.punch[0];
					auto move = [](bool *a) { a[1] = a[1] || a[0]; a[0] = false; };
					if (!cplay.train_pick_arm) {
						// THE BUTTON HAS TO BE LET GO FIRST: the press that chose TRAINING in the console's list is
						// still down when the select screen arrives (console_play.h), and taken for a choice it made
						// player 1's pad pick player 2's fighter first -- the fault Frederic reported.
						if (!punch) cplay.train_pick_arm = true;
						// and it reaches the game no more than it counts here, or it would validate player 1's
						// fighter on the frame the screen appeared, wherever the cursor happened to stand
						else { live.punch[0] = false; script_ctl.punch[0] = false; }
					} else if (!cplay.train_p1_chose) {
						// (this press is his own choice: it stays his, and the routing begins after it)
						if (punch) { cplay.train_p1_chose = true; cplay.train_punch_held = true; }
					} else {
						move(live.up); move(live.down); move(live.left); move(live.right);
						move(live.kick); move(live.guard);
						move(script_ctl.up); move(script_ctl.down); move(script_ctl.left); move(script_ctl.right);
						// the punch only once the one that chose him has been let go (console_play.h)
						if (cplay.train_punch_held && !punch) cplay.train_punch_held = false;
						if (cplay.train_punch_held) { live.punch[0] = false; script_ctl.punch[0] = false; }
						else { move(live.punch); move(script_ctl.punch); }
					}
				}
				// THE PAGE IS UP AND THE GAME IS STILL RUNNING: it is held in Sega's empty STAGE_SEL every
				// frame, because a request is only read once and the point here is that it is never walked
				// out of. Nothing of the player reaches the game meanwhile -- the carousel has the keys and
				// the pad, as it does under the three surfaces that freeze instead.
				if (stages.shown) {
					board.fv2_park_stage_select();
					live = m3::Controls();
					script_ctl = m3::Controls();
				}
				m3::Controls &c = board.controls;
				auto mix = [](bool *a, const bool *b, const bool *d) { a[0] = b[0] || d[0]; a[1] = b[1] || d[1]; };
				mix(c.coin, script_ctl.coin, live.coin); mix(c.start, script_ctl.start, live.start);
				mix(c.test, script_ctl.test, live.test); mix(c.service, script_ctl.service, live.service);
				mix(c.up, script_ctl.up, live.up); mix(c.down, script_ctl.down, live.down);
				mix(c.left, script_ctl.left, live.left); mix(c.right, script_ctl.right, live.right);
				mix(c.punch, script_ctl.punch, live.punch); mix(c.kick, script_ctl.kick, live.kick);
				mix(c.guard, script_ctl.guard, live.guard); mix(c.escape, script_ctl.escape, live.escape);
				// The console way's injections go on last, so they get through even while the menu is holding
				// the player's own Start back: choosing a mode has to hand the game a Start on that very frame.
				for (int q = 0; q < 2; q++) c.start[q] = c.start[q] || console_ctl.start[q];
				// Honey's costume wheel, turned by the same presses of up the game turns its own two with
				game.honey_tick(c.up[0] || c.up[1]);
				// The arcade's test menu (RAM 0x102002 is 4 while it's up; 0 at the attract, a fight, the boot): Down
				// moves (the service button) and Start selects (the test button), any pad or key, as VF1's and VF2's;
				// the menu's Service menu presses test once the game is past its boot; leaving it, the PC way restarts
				// at once past the boot (the game reboots through its own, about 7 s)
				const bool test_mode = board.ram.size() > 0x102002 && board.ram[0x102002 ^ 3] == 4;
				test_mode_frames = test_mode ? test_mode_frames + 1 : 0;
				if (!in_test && test_mode_frames >= 30) in_test = true;
				left_test = false;
				if (in_test && !test_mode) { in_test = false; left_test = true; }
				if (in_test) {
					const bool down = c.down[0] || c.down[1], start = c.start[0] || c.start[1];
					if (down && !test_down_was) service_frames = 6;
					if (start && !test_start_was) test_frames = 6;
					test_down_was = down;
					test_start_was = start;
				}
				if (service_frames > 0) { c.service[0] = true; service_frames--; }
				if (test_pending && game_frame >= BOOT_FRAMES) { test_pending = false; test_frames = 10; }
				if (test_frames > 0) { c.test[0] = true; test_frames--; }
				game.builder.widen = std::max(1.0f, float(picture_aspect()) * 0.75f) * 1.05f;   // (a little more: smooth motion)
		};
		for (int i = 0; i < due && running; i++) {
			// A frame more than this pass's first (a slow pass let two fall due): started on the game's own thread, not
			// run here, and taken next pass (run here it made this pass slow too, and the next, one after another)
			if (emu_threaded && i > 0 && !half && !ff && cfg.smooth) {
				frames_run -= uint64_t(due - i);
				frame_clock = std::min(frame_clock, double(frames_run) + 1.0);
				const uint64_t th = SDL_GetTicksNS();
				frame_inputs(half_hk, half_act, half_left);
				game.job_start(!ff);
				half = true;
				half_ns = SDL_GetTicksNS() - th;
				break;
			}
			uint64_t te = SDL_GetTicksNS();
			if (half) te -= half_ns;
			hotkey hk = hotkey::none;
			InputScript::Actions act;
			bool left_test = false;
			if (half) { hk = half_hk; act = half_act; left_test = half_left; half_hk = hotkey::none; half_act = InputScript::Actions(); }
			else frame_inputs(hk, act, left_test);
			if (half) {
				if (emu_threaded) game.job_wait();
				else game.finish_frame(!ff, true);
				half = false;
				pass_kind = std::max(pass_kind, 2);
			} else { game.run_frame(!ff, true); pass_kind = 3; }
			game.publish();
			game_frame++;
			game.hold_roster();   // (nothing to do unless the player asked for the hidden fighters)
			// a boot the player watches: the same check, once it is over
			if (!game.rebuilt_after_boot && game_frame >= BOOT_FRAMES) { game.rebuilt_after_boot = true; game.rebuild_from_ram_if_stale(); }
			pace.frames++;
			pace.emu_ms += double(SDL_GetTicksNS() - te) / 1e6;
			t9.frames++;
			t9.emu_ms += double(SDL_GetTicksNS() - te) / 1e6;
			gov.emu_ms += (std::min(double(SDL_GetTicksNS() - te) / 1e6, 4 * gov.emu_ms + 1) - gov.emu_ms) * 0.02;   // (a lone slow frame barely counts)
			new_frame = true;
			for (auto &s : act.shots) write_frame("build/host_" + s + ".png");
			if (act.quit) running = false;
			if (rec.wav && game_frame > rec.from) {
				fwrite(game.audio.data(), sizeof(int16_t), game.audio.size(), rec.wav);
				rec.wav_bytes += uint32_t(game.audio.size() * sizeof(int16_t));
			}
			if (audio && !game.audio.empty() && !ff && rec.dir.empty())   // (recording: to its file only, not queued for hours)
				SDL_PutAudioStreamData(audio, game.audio.data(), int(game.audio.size() * sizeof(int16_t)));
			game.audio.clear();
			for (auto &k : test_keys) if (k.second == game_frame) { send_key(k.first); window_shots.push_back(game_frame + 3); }
			// VF3_MIDI_SEND: in with the game's own bytes, so the board hears it exactly as it hears the game
			for (auto &m : midi_send)
				if (m.first == game_frame) {
					board.midi.insert(board.midi.end(), m.second.begin(), m.second.end());
					fprintf(stderr, "midi sent by hand at frame %llu\n", (unsigned long long)game_frame);
				}
			if (quit_after >= 0 && int64_t(game_frame) >= quit_after) running = false;
			switch (hk) {
			case hotkey::menu: open_menu(); break;
			case hotkey::scale: set_scale(scale % MAX_SCALE + 1); break;
			case hotkey::aspect: set_shape((aspect_i + 1) % N_ASPECTS); break;
			case hotkey::none: break;
			}
			if (left_test && g_pc_mode) { reset_game(); break; }
			if (menu.is_open()) break;
		}
		// The next frame's first half, when it's due within half a frame: smooth motion's pictures then each hold half
		// a frame's work at most (the pictures go between the last two frames finished: this one isn't yet)
		static const bool no_halves = getenv("VF3_NO_HALVES") != nullptr;   // (tests: whole frames, as before)
		if (!half && !no_halves && rec.dir.empty() && running && !stopped && !ff && cfg.smooth && (due == 0 || emu_threaded) && !menu.is_open() && first_shown &&
			frame_clock - double(frames_run) >= 0.5) {
			const uint64_t th = SDL_GetTicksNS();
			frame_inputs(half_hk, half_act, half_left);
			if (emu_threaded) game.job_start(!ff);
			else game.start_frame();
			half = true;
			pass_kind = std::max(pass_kind, 1);
			half_ns = SDL_GetTicksNS() - th;
		}
		// The picture: smooth motion's, between the last two frames, at every pass; else the newest frame's
		const bool smooth = cfg.smooth && !ff;
		const uint64_t bt = SDL_GetTicksNS();
		smooth_budget = std::min(2.0, smooth_budget + double(bt - smooth_t) * (gov.smooth_off ? 60 : 120) / 1e9);
		smooth_t = bt;
		const bool unseen = !hidden && (SDL_GetWindowFlags(win) & (SDL_WINDOW_MINIMIZED | SDL_WINDOW_OCCLUDED));
		// (a screen no faster than the pictures' limit: a picture each refresh, its vsync the pace. By the budget alone a
		// refresh a hair early found it a hair short of one, and that refresh's picture was lost: 108-118 a second on a
		// 120 Hz screen. The budget only holds a faster screen to the limit.)
		static uint64_t screen_hz_t = 0;
		if (bt - screen_hz_t > 1000000000ull) {
			const bool first_look = !screen_hz_t;
			screen_hz_t = bt;
			const SDL_DisplayMode *dm = SDL_GetCurrentDisplayMode(SDL_GetDisplayForWindow(win));
			const double hz_now = dm && dm->refresh_rate > 0 ? dm->refresh_rate : 60.0;
			if (hz_now != screen_hz || first_look) fprintf(stderr, "screen: %.2f Hz%s\n", hz_now, !hidden && hz_now <= 126 ? " (a picture each refresh)" : "");
			screen_hz = hz_now;
			int vs = 0;
			const bool vs_now = SDL_GetRenderVSync(ren, &vs) && vs != 0;
			if (vs_now != vsync_on || first_look) fprintf(stderr, "vsync: %s\n", vs_now ? "on" : "off (pictures by the budget, not a picture each refresh)");
			vsync_on = vs_now;
		}
		// (hidden, or vsync off: nothing paces the passes to the screen, a picture each pass was ~500 a second; as VF1's)
		const bool every_refresh = !hidden && vsync_on && screen_hz <= (gov.smooth_off ? 60 : 120) * 1.05;
		const bool draw = rec.on || new_frame || (smooth && !stopped && !unseen && (smooth_budget >= 1 || every_refresh)) || !first_shown;
		if (draw) {
			smooth_budget = std::max(0.0, smooth_budget - 1);
			t9.pictures++;
			const uint64_t td = SDL_GetTicksNS();
			double t = smooth && !stopped ? std::clamp(frame_clock - double(frames_run), 0.0, 1.0) : 1.0;
			if (stopped) t = 1.0;
			draw_picture(t, new_frame);
			if (test_load_ms > 0 && double(SDL_GetTicksNS() - start_ns) / 1e9 < test_load_s + 5)
				SDL_DelayNS(uint64_t(test_load_ms * scale * scale * 1e6));
			const double ms = double(SDL_GetTicksNS() - td) / 1e6;
			pace.draw_ms += ms;
			t9.draw_ms += ms;
			pace.pictures++;
			new_frame = false;
			// the governor
			const uint64_t now = SDL_GetTicksNS();
			// (off for players, as VF1's and VF2's since its v0.1.10: the scale, the antialiasing and smooth motion
			// always the player's; its cuts held a fast Mac at 60 through the character select's loads. VF3_GOVERN:
			// on, for tests)
			static const bool governs = getenv("VF3_GOVERN") != nullptr;
			if (governs && now > gov.grace_until && !stopped && !ff && !unseen) {
				gov.fixed_ms = gov.pictures ? gov.fixed_ms + (std::min(picture_fixed_ms, ms) - gov.fixed_ms) * 0.05 : std::min(picture_fixed_ms, ms);
				gov.avg_ms = gov.pictures++ ? gov.avg_ms + (ms - gov.avg_ms) * 0.05 : ms;
				const SDL_DisplayMode *dm = SDL_GetCurrentDisplayMode(SDL_GetDisplayForWindow(win));
				const double screen_hz = dm && dm->refresh_rate > 0 ? std::min(240.0f, dm->refresh_rate) : 60.0;
				const double hz = std::min(screen_hz, gov.smooth_off ? 60.0 : 120.0);   // (the pictures a second at most)
				const double frame_ms = 1000.0 / FRAME_HZ, emu = gov.emu_ms;
				// a picture each refresh (smooth) or each frame, with the game's frame: at 120 a second a game frame
				// comes with about every other picture, so each picture's share of it (it was taken whole from each,
				// and a 10 ms frame on an M4 Max left 120 a second no time at all: held to 60)
				const double budget = smooth ? 1000.0 / hz - emu * std::min(1.0, FRAME_HZ / hz) : frame_ms - emu;
				if (gov.pictures > 30) {
					if (gov.avg_ms > 0.9 * budget || gov.missing) {
						if (gov.missing) gov.missed_cuts++;
						if (gov.missing) fprintf(stderr, "governor: %d of %d refreshes missed (the GPU or the present, not the pictures' %.1f ms)\n", gov.was_missed, gov.was_passes, gov.avg_ms);
						if (smooth && !gov.smooth_off && hz > 65) {
							gov.smooth_off = true;
							fprintf(stderr, "governor: pictures take %.1f ms, over %.0f a second's time: smooth motion held to 60 pictures a second for now\n", gov.avg_ms, hz);
						} else if (!gov.missing && gov.avg_ms - gov.fixed_ms < 0.35 * budget) {
							// Most of a picture is the in-between and the triangles prepared, on the CPU at any scale: a lower
							// scale or less antialiasing would only make it look worse (a Steam Deck in Wolf's desert: 13 ms
							// pictures, the scale cut from 2x to 1x and still 13 ms). Said once.
							static bool said = false;
							if (!said) fprintf(stderr, "governor: pictures take %.1f ms, %.1f of it on the CPU at any scale: the scale and antialiasing kept\n", gov.avg_ms, gov.fixed_ms);
							said = true;
						} else if (aa_now() > 1) {
							gov_aa_cut = cfg.aa - (aa_now() - 1);
							fprintf(stderr, "governor: pictures take %.1f ms, over a frame's time: antialiasing lowered to %s for now\n", gov.avg_ms,
									aa_now() > 1 ? (std::to_string(aa_now()) + "x").c_str() : "off");
						} else if (scale > 1) {
							scale--;
							gov.scale_cut++;
							fprintf(stderr, "governor: pictures take %.1f ms, over a frame's time: the render scale lowered to %dx for now\n", gov.avg_ms, scale);
						}
						gov_grace(2);
						gov.calm_since = 0;
					} else if (gov.smooth_off || gov.scale_cut || gov_aa_cut) {
						// calm: the cut lifted after 10 s (longer for cuts again for refreshes missed: missed_cuts) well within the time the pictures would take then (the scale's
						// first, then the antialiasing's, then smooth motion)
						const int k = aa_now();
						const double want = gov.scale_cut ? gov.avg_ms * double((scale + 1) * (scale + 1)) / (scale * scale)
						                  : gov_aa_cut ? gov.avg_ms * double((k + 1) * (k + 1)) / (k * k) : gov.avg_ms;
						const double want_budget = gov.scale_cut || gov_aa_cut ? budget : 1000.0 / std::min(screen_hz, 120.0) - emu * std::min(1.0, FRAME_HZ / std::min(screen_hz, 120.0));
						if (want < 0.6 * want_budget) {
							if (!gov.calm_since) gov.calm_since = now;
							else if (now - gov.calm_since > (gov.missed_cuts > 3 ? 60 : 10ull << std::max(0, gov.missed_cuts - 1)) * 1000000000ull) {
								if (gov.scale_cut) { scale++; gov.scale_cut--; fprintf(stderr, "governor: calm again: the render scale back up to %dx\n", scale); }
								else if (gov_aa_cut) { gov_aa_cut--; fprintf(stderr, "governor: calm again: antialiasing back up to %dx\n", aa_now()); }
								else if (cfg.smooth) { gov.smooth_off = false; fprintf(stderr, "governor: calm again: smooth motion back to 120 pictures a second\n"); }
								gov_grace(2);
							}
						} else gov.calm_since = 0;
					}
				}
			}
		}
		// The CONTROLLER page's mirror, filled while that screen is up and not otherwise: it reads the pads
		// every pass, and there is no reason to do it behind a page that does not show it.
		if (options.shown && options.at == fv2_options::SCR_CONTROLLER) {
			for (int p = 0; p < 2; p++) options.live[p] = live_controls(p, pads[p], cfg.ctl);
			// read again every pass rather than once at the start: a pad can be plugged in while the page is up
			for (int p = 0; p < 2; p++) options.pad_id[p] = pads[p] ? SDL_GetGamepadID(pads[p]) : 0;
			// (tests: VF3_LIVE=n lights that set of bits on the player shown. A pushed key event does not
			// change the keyboard state SDL reports, so the test hooks cannot reach this page any other way.)
			// (tests: VF3_LIVE=n[,m] lights those bits on 1P and 2P.)
			if (const char *lv = getenv("VF3_LIVE")) {
				options.live[0] = uint8_t(atoi(lv));
				const char *c = strchr(lv, 44);
				if (c) options.live[1] = uint8_t(atoi(c + 1));
			}
		}
		compose();
		draw_console(board);
		// the frame rate: the pictures shown a second, over the game (not the menu or a pause, where no new
		// pictures are made: hidden there, as VF1's and VF2's)
		const bool fps_shown = cfg.show_fps && !stopped;
		if (fps_shown) {
			static uint64_t fps_t = 0;
			static int fps_n = 0;
			if (draw) fps_n++;
			if (SDL_GetTicksNS() - fps_t > 500000000ull) {
				char b[32];
				snprintf(b, sizeof b, "%.0f FPS", fps_n * 1e9 / double(SDL_GetTicksNS() - fps_t));
				menu.set_fps(b);
				fps_t = SDL_GetTicksNS();
				fps_n = 0;
			}
		}
		if (menu.is_open() || fps_shown) menu.render(fps_shown);
		for (auto it = window_shots.begin(); it != window_shots.end();)
			if (*it <= game_frame) { read_window("build/host_window_" + std::to_string(*it - 3) + ".png"); it = window_shots.erase(it); }
			else ++it;
		for (uint64_t s : shot_at) if (s == pass) read_window("build/host_window_at_" + std::to_string(pass) + ".png");
		if (!running && quit_after >= 0) {
			if (game.emu_thread.joinable()) game.job_wait();   // (a frame's half may still be running)
			write_frame("build/host_last.png");
			read_window("build/host_window.png");
		}
		if (rec.on && rec.video) {
			if (SDL_Surface *s = SDL_RenderReadPixels(ren, nullptr)) {
				if (SDL_Surface *c = SDL_ConvertSurface(s, SDL_PIXELFORMAT_ABGR8888)) {
					for (int y = 0; y < rec.h && y < c->h; y++)
						fwrite(static_cast<uint8_t *>(c->pixels) + size_t(y) * c->pitch, 4, size_t(rec.w), rec.video);
					SDL_DestroySurface(c);
				}
				SDL_DestroySurface(s);
			}
			rec.n++;
			if (rec.n % 600 == 0) fprintf(stderr, "record: %.0f s\n", rec.n / 60.0);
			if (game_frame >= rec.to) running = false;
		}
		pass_work_ns = SDL_GetTicksNS() - pass_t0;
		SDL_RenderPresent(ren);
		{   // the governor's count of refreshes missed (above)
			static uint64_t last_present = 0;
			const uint64_t now_p = SDL_GetTicksNS();
			// (only passes whose own work fit in a refresh: one that ran long on the CPU before its present, the game's
			// frame loading a stage or a fighter (the character select, the intros: 11 ms before the present), is no
			// sign of the GPU or the present falling behind, and fewer pictures wouldn't help it. Counted, those
			// loads alone held smooth motion at 60 on an M4 Max at 4x.)
			if (last_present && quit_after < 0 && rec.dir.empty() && every_refresh && draw && smooth && !stopped && !unseen && now_p > gov.grace_until &&
				double(pass_work_ns) < 1e9 / screen_hz) {
				gov.win_passes++;
				if (double(now_p - last_present) > 1.5e9 / screen_hz) gov.win_missed++;
			}
			last_present = now_p;
			if (now_p - gov.win_t0 > 3000000000ull) {
				gov.missing = gov.win_passes > 100 && gov.win_missed * 10 > gov.win_passes;
				gov.was_passes = gov.win_passes; gov.was_missed = gov.win_missed;
				gov.win_t0 = now_p;
				gov.win_passes = gov.win_missed = 0;
			}
		}
		if (!first_shown) {
			first_shown = true;
			if (!hidden) { SDL_ShowWindow(win); SDL_RaiseWindow(win); }
			fprintf(stderr, "first picture %.2f s after the start\n", double(SDL_GetTicksNS() - start_ns) / 1e9);
		}
		shown = true;
		if (focus_checks > 0 && !hidden && first_shown) {
			const bool ours = SDL_GetKeyboardFocus() == win;
			if (--focus_checks % 30 == 0 && !ours) SDL_RaiseWindow(win);
			if (focus_checks == 0 || ours) {
				fprintf(stderr, "keyboard: %s\n", ours ? "ours" : "not ours (click the window)");
				focus_checks = 0;
			}
		}
		if (hidden) SDL_DelayNS(1000000);   // (no vsync to wait for when hidden on some systems)
		else if (unseen) SDL_DelayNS(2000000);   // (a minimised window's present may not wait: not a core spun)
		{   // at most a pass every 2 ms, as VF1's and VF2's: with vsync off (forced so, or not taken) or a minimised
			// window, the present returns at once, and the passes ran flat out (a core and the GPU at full tilt)
			static uint64_t last_pass = SDL_GetTicksNS();
			const uint64_t tn = SDL_GetTicksNS();
			if (!hidden && tn - last_pass < 2000000) SDL_DelayNS(2000000 - (tn - last_pass));
			last_pass = SDL_GetTicksNS();
		}
		// the title: the scale and shape, or (F9) the timings
		if (cfg.timings && SDL_GetTicksNS() - t9.t0 >= 2000000000ull) {
			const double secs = double(SDL_GetTicksNS() - t9.t0) / 1e9;
			char t[200];
			snprintf(t, sizeof t, "%s  %dx  %.1f frames/s  %.1f pictures/s  game %.2f ms/frame  picture %.2f ms", game_name.c_str(), scale, t9.frames / secs,
					 t9.pictures / secs, t9.frames ? t9.emu_ms / t9.frames : 0.0, t9.pictures ? t9.draw_ms / t9.pictures : 0.0);
			SDL_SetWindowTitle(win, t);
			fprintf(stderr, "%s\n", t);
			t9 = {};
			t9.t0 = SDL_GetTicksNS();
		} else if (!cfg.timings) {
			char t[96];
			if (cfg.aspect) snprintf(t, sizeof t, "%s  %dx  %s", game_name.c_str(), scale, ASPECT_NAMES[cfg.aspect]);
			else snprintf(t, sizeof t, "%s  %dx  fit (%.2f:1)", game_name.c_str(), scale, picture_aspect());
			if (title_now != t) { title_now = t; SDL_SetWindowTitle(win, t); }
		}
		if (SDL_GetTicksNS() - pace.t0 > (pace_log ? 2000000000ull : 10000000000ull)) {
			const double all_s = double(SDL_GetTicksNS() - pace.t0) / 1e9, s = std::max(0.001, all_s - pace.stopped_s);
			const double pics = std::max(1, pace.pictures);
			char stopped_note[64] = "";
			if (pace.stopped_s >= 0.05) snprintf(stopped_note, sizeof(stopped_note), " (of %.1f s: %.1f s in the menu or paused)", all_s, pace.stopped_s);
			fprintf(stderr, "pace: %.1f frames/s, %.1f pictures/s%s (%llu between frames, %llu at a cut); a frame %.2f ms; a picture %.2f ms (%s %.2f, uploads %.2f), %dx (%d x %d)%s; sound queued %d bytes\n",
					pace.frames / s, pace.pictures / all_s, stopped_note, (unsigned long long)between_n, (unsigned long long)cut_n, pace.frames ? pace.emu_ms / pace.frames : 0.0,
					pace.draw_ms / pics, g3_shown ? "the GPU's set-up" : "the CPU's 3D", g3_shown ? pace.gpu_ms / pics : (pace.draw_ms - pace.upload_ms) / pics,
					pace.upload_ms / pics, scale, tex_w, tex_h, gov.smooth_off ? ", smooth motion held to 60 a second" : "", audio ? SDL_GetAudioStreamQueued(audio) : -1);
			if (const std::string pl = sysinfo::placement(); !pl.empty()) fprintf(stderr, "  %s\n", pl.c_str());
			if (g3_shown && pics > 0)
				fprintf(stderr, "  a picture's 3D: the in-between %.2f ms, the triangles prepared %.2f ms, the GPU's renderer %.2f ms (its set-up %.2f, the rest waiting on the GPU)\n",
						pace.interp_ms / pics, pace.prep_ms / pics, pace.g3_wall_ms / pics, pace.gpu_ms / pics);
			if (const int ns = pace.slow[0] + pace.slow[1] + pace.slow[2] + pace.slow[3])
				fprintf(stderr, "  passes over a refresh and a half: %d (pictures only %d, first halves %d, second halves %d, whole frames %d), their work before the present %.2f ms\n",
						ns, pace.slow[0], pace.slow[1], pace.slow[2], pace.slow[3], pace.slow_work_ms / ns);
			pace = {};
			pace.t0 = SDL_GetTicksNS();
			between_n = cut_n = 0;
		}
		// the settings (a hotkey's change: Steam can end the game without a quit) saved every 3 s when changed, and the
		// NVRAM (the game writes its rankings and settings as it goes: the arcade's bookkeeping changes it all through
		// play) once a minute, as VF1's and VF2's; at the quit, whatever's changed
		static uint64_t nvram_t = SDL_GetTicksNS();
		if (SDL_GetTicksNS() - nvram_t > 60000000000ull) {
			nvram_t = SDL_GetTicksNS();
			save_nvram();
		}
		if (SDL_GetTicksNS() - save_t > 3000000000ull) {
			save_t = SDL_GetTicksNS();
			if (cfg_dirty && !menu.is_open() && quit_after < 0 && cfg.save(settings_path)) cfg_dirty = false;
		}
		// (a refresh and a half of this screen's: at 60 Hz every pass waiting on vsync was over the 120 Hz one's)
		if (!stopped && double(SDL_GetTicksNS() - pass_t0) > 1.5e9 / std::max(60.0, screen_hz)) { pace.slow[pass_kind]++; pace.slow_work_ms += double(pass_work_ns) / 1e6; }
		(void)shown;
	}
	saver.finish();
	saved_hash = ~nvram_hash();   // (the last copies written here whatever the writer did)
	save_nvram(true);
	rec_stop();
	if (cfg_dirty && quit_after < 0) cfg.save(settings_path);
	if (audio) SDL_DestroyAudioStream(audio);
	if (tile_dump.on()) {
		// (tiles.csv and layers/index.csv are written here: the dump keeps its inventory until the end)
		fprintf(stderr, "tile dump: %zu distinct tiles and %zu layer layouts in %s\n",
		        tile_dump.tiles(), tile_dump.layouts(), arg_tile_dump.c_str());
		tile_dump.close();
	}
	for (auto &p : pads) if (p) SDL_CloseGamepad(p);
	menu_owner.reset();
	if (g3.tex) SDL_DestroyTexture(g3.tex);
	gpu3d.shutdown();
	SDL_DestroyTexture(tex3d);
	if (glass) SDL_DestroyTexture(glass);
	if (crt_tex) SDL_DestroyTexture(crt_tex);
	if (crt_state) SDL_DestroyGPURenderState(crt_state);
	if (crt_shader) SDL_ReleaseGPUShader(SDL_GetGPURendererDevice(ren), crt_shader);
	SDL_DestroyTexture(tex_below);
	SDL_DestroyTexture(tex_above);
	SDL_DestroyRenderer(ren);
	SDL_DestroyWindow(win);
	SDL_Quit();
	fprintf(stderr, "quit after %llu frames\n", (unsigned long long)game_frame);
	return 0;
}
