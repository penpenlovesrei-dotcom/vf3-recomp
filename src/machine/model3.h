// The Model 3 Step 1.0 board as Virtua Fighter 3 uses it: the PowerPC's memory map, the IRQ controller and system
// registers, the inputs and EEPROM, the JTAG chain, the MPC105 PCI bridge, the 53C810 (whose SCRIPTS processor
// carries display lists to the Real3D), the Real3D's and tile generator's registers and memories, and the frame
// timing.
//
// Written for this project. Every value the PowerPC can see follows the reference build (Supermodel), so a run
// here can be compared with it frame by frame; the comments say where the reference's behaviour is followed
// rather than known hardware behaviour.
#pragma once
#include "../../third_party/mame/315-5881/crypt_315_5881.h"
#include "../ppc/ppc.h"
#include <cstdint>
#include <deque>
#include <functional>
#include <string>
#include <vector>

namespace m3 {

// The switches, as the game reads them (true = pressed)
struct Controls
{
	bool coin[2] = {}, start[2] = {}, test[2] = {}, service[2] = {};
	bool up[2] = {}, down[2] = {}, left[2] = {}, right[2] = {};
	bool punch[2] = {}, kick[2] = {}, guard[2] = {}, escape[2] = {};
};

// The IRQ controller: eight maskable lines, and lines above bit 7 that aren't masked (the 53C810's)
class Irq
{
public:
	ppc::Cpu *cpu = nullptr;
	unsigned enable = 0, state = 0;
	void assert_(unsigned bits);
	void deassert(unsigned bits);
	void reset() { enable = 0; state = 0; }
};

// The 93C46 serial EEPROM (64 words) behind input register 0
class Eeprom
{
public:
	uint16_t regs[64];
	Eeprom();
	void write(unsigned cs, unsigned clk, unsigned di);
	unsigned read();
	void reset();
private:
	unsigned CS = 0, CLK = 0, DI = 0, DO = 0;
	uint32_t out = 0, in = 0;
	int bits_out = 0;
	bool receiving = true;
	unsigned addr = 0;
	int busy = 0;
	bool locked = true;
};

// The JTAG chain to the Real3D's ASICs and 3D-RAMs (the game reads the ID codes). Step 1.x has ten devices on
// the chain, Step 2.x seventeen (a second Earth, two more Mars, four more 3D-RAMs), with their own ID codes.
class Jtag
{
public:
	void write(bool tck, bool tms, bool tdi, bool trst);
	bool read() const { return tdo; }
	void reset();
	void configure(int stepping);   // which chain: 0x10 Step 1.0, 0x15 Step 1.5, 0x20 Step 2.0, 0x21 Step 2.1
	uint32_t modeword[5] = {};  // per ASIC kind, for the renderer
private:
	struct Device
	{
		bool asic = false;
		int kind = -1;          // which ASIC (0 Mercury .. 4 Jupiter), -1 for a dummy or a 3D-RAM
		uint32_t id = 0;
		uint64_t shift = 0;
		int size = 0;
		uint8_t ir = 0;
		uint32_t mode = 0;
	};
	Device dev[17];
	int n_dev = 10;             // how many of them are on this board's chain
	int state = 0;
	bool last_tck = false, tdo = false;
	void dev_reset(Device &d);
	void capture_dr(Device &d);
	void capture_ir(Device &d);
	void update_dr(Device &d);
public:
	Jtag();
};

class Model3;

// The NCR/LSI 53C810: the registers and the SCRIPTS memory-move and interrupt instructions
class Scsi
{
public:
	Model3 *board = nullptr;
	uint8_t regs[0x60];
	uint32_t temp = 0, dsp = 0, dsps = 0, dbc = 0;
	uint8_t dcmd = 0, dcntl = 0, dmode = 0, dstat = 0, dien = 0, istat = 0;
	bool halt = false;
	uint8_t base = 0;
	void reset();
	uint8_t read(unsigned reg);
	void write(unsigned reg, uint8_t data);
private:
	void run(bool single);
	bool exec();
	uint32_t fetch(uint32_t off);
};

class Model3 : public ppc::Bus
{
public:
	ppc::Cpu cpu;
	Irq irq;
	Eeprom eeprom;
	Jtag jtag;
	Scsi scsi;
	Controls controls;

	// Which board and which game the loaded set is. Step 1.0 (Virtua Fighter 3) and Step 2.0 (Fighting Vipers 2)
	// differ in the CPU's clock, the PCI bridge (MPC105 or MPC106, at different addresses), the SCSI controller
	// (Step 1.x only) and the Real3D's DMA port and JTAG chain; everything the two share is one piece of code.
	int stepping = 0x10;             // 0x10 Step 1.0, 0x20 Step 2.0
	std::string set, title;          // MAME's name for the set, and the game's title
	uint32_t crypto_key = 0;         // the 315-5881 security chip's key (0: the board has no security board)
	bool step2() const { return stepping >= 0x20; }
	// The 315-5881 on a keyed board (Fighting Vipers 2), and how often the game has asked it for a word and
	// programmed it. A set with no key never builds one and never comes here.
	mame::Crypt5881 crypt;
	uint64_t security_reads = 0, security_writes = 0;

	std::vector<uint8_t> ram;        // 8 MB, host-order words
	std::vector<uint8_t> crom;       // fixed CROM, 8 MB, host-order words
	std::vector<uint8_t> banked;     // banked CROM, 128 MB, host-order words
	std::vector<uint8_t> vrom;       // 64 MB
	std::vector<uint8_t> sound_rom, samples;   // the sound board's: 68000 program, samples (16-bit words, host order)
	std::vector<uint8_t> backup;     // 128 KB backup RAM, host-order words

	// The test menu's COIN ASSIGNMENTS > COIN/CREDIT SETTING, in backup RAM (found by setting each in the test menu,
	// leaving it, and comparing): the setting's number (1-27) at 0x1E033; for #1-#26 the same at 0x1E037 and 0 at
	// 0x1E03B; #27, FREE PLAY, sets 0x1E03B to 1 and leaves 0x1E037. No checksum. A board that has never run has 0
	// there until the game writes its settings (by frame 120): -1, not written yet.
	// (the addresses are Virtua Fighter 3's own, found in its test menu, so the coin settings and the FREE PLAY
	// notices below are only touched for that set: another game keeps its settings where this port doesn't know)
	static constexpr int COIN_FACTORY = 1, COIN_FREE_PLAY = 27;
	// Where a game keeps its test menu's COIN ASSIGNMENTS > COIN/CREDIT SETTING in the backup RAM, or -1 for one
	// this port has not been shown. Both games number that setting the same way, 1 to 27, 27 being FREE PLAY.
	//
	// Virtua Fighter 3: 0x1E033, with the number repeated at 0x1E037 and a flag at 0x1E03B.
	//
	// Fighting Vipers 2: 0x17, near the start of the backup RAM, and ONE byte is the whole of it. Found the same
	// way as Virtua Fighter 3's -- by setting FREE PLAY in the game's own test menu and comparing the backup RAM
	// before and after. That moved seven bytes: four of them four apart (0x0B, 0x0F, 0x13, 0x17, which are the
	// four lines of that page) and three more that look like bookkeeping. The one that matters was then picked
	// out by putting the other six back and leaving only 0x17 at 27: the game still came up in FREE PLAY, and
	// its picture was byte-identical to the run with all seven changed. So there is no checksum to keep and no
	// neighbour to agree with.
	int coin_offset() const { return set == "vf3" ? 0x1E033 : set == "fvipers2" ? 0x17 : -1; }
	int coin_setting() const
	{
		const int at = coin_offset();
		if (at < 0 || backup.size() <= size_t(at) + 8) return -1;
		const int v = backup[at];
		return v >= 1 && v <= COIN_FREE_PLAY ? v : -1;
	}
	bool set_coin_setting(int v)   // (true if it changed)
	{
		const int at = coin_offset();
		if (coin_setting() < 0 || v < 1 || v > COIN_FREE_PLAY) return false;
		if (at != 0x1E033) {   // Fighting Vipers 2: the one byte, and that is all of it
			const uint8_t was = backup[at];
			backup[at] = uint8_t(v);
			return was != backup[at];
		}
		const uint8_t was[3] = { backup[0x1E033], backup[0x1E037], backup[0x1E03B] };
		backup[0x1E033] = uint8_t(v);
		if (v == COIN_FREE_PLAY) backup[0x1E03B] = 1;
		else backup[0x1E037] = uint8_t(v), backup[0x1E03B] = 0;
		return was[0] != backup[0x1E033] || was[1] != backup[0x1E037] || was[2] != backup[0x1E03B];
	}
	bool free_play() const
	{
		if (coin_setting() != COIN_FREE_PLAY) return false;
		return coin_offset() != 0x1E033 || backup[0x1E03B] == 1;
	}
	// Fighting Vipers 2's GAME ASSIGNMENTS -- its operator settings -- WHERE THEY LAST, which is the backup RAM.
	//
	// The block the game reads them from at 0x00120520 is only a working copy; it is loaded from the backup when
	// the game starts, so writing it does not survive a restart. Watching who writes it (vf3run --watch 120540)
	// gives a memcpy, and its two callers give the whole picture: 0x00020D80 loads the block from the backup at
	// 0xFE0C0000, 0x00020F38 loads it from a factory table at 0x00120D24, and 0x00020F98 chooses between them on
	// three fields at the head of a first block -- "SEGA", 0x8221, 0x0312. Four blocks in all, contiguous in RAM
	// from 0x0011F024, on four separate pages of the backup. docs/fv2-console-options.md has the reading.
	//
	// The loader reads one 32-bit word of backup per 16-bit halfword of settings (lwbrx then sthu), so each value
	// sits in two of every four bytes. For the live byte at 0x00120520 + k that works out as below -- and the
	// formula gives back this port's own coin byte, 0x17, which had been found by hand: k = 11, which is
	// 0x0012052B, in the COIN ASSIGNMENTS where it belongs.
	//
	// Measured end to end, not just read: a run with the backup poked and a control beside it changed five
	// settings through the backup and the game loaded all five.
	static int fv2_option_index(int k) { return k % 2 == 0 ? 2 * k + 2 : 2 * k + 1; }
	static constexpr int FV2_OPTIONS_LIVE = 0x00120520;
	int fv2_option(int live, int bytes) const
	{
		if (set != "fvipers2") return -1;
		int v = 0;
		for (int i = 0; i < bytes; i++) {
			const size_t at = size_t(fv2_option_index(live - FV2_OPTIONS_LIVE + i));
			if (at >= backup.size()) return -1;
			v = v << 8 | backup[at];
		}
		return v;
	}
	// True if it wrote anything -- which is what tells the caller to restart the game, since the game reads
	// these only as it starts.
	bool fv2_set_option(int live, int bytes, int v)
	{
		if (set != "fvipers2") return false;
		bool changed = false;
		for (int i = 0; i < bytes; i++) {
			const size_t at = size_t(fv2_option_index(live - FV2_OPTIONS_LIVE + i));
			if (at >= backup.size()) return changed;
			const uint8_t was = backup[at], now = uint8_t(v >> (8 * (bytes - 1 - i)));
			backup[at] = now;
			changed = changed || was != now;
		}
		return changed;
	}
	// Blanks the FREE PLAY notices (the PC way: always free play, nothing to say), in the fixed CROM the boot copies
	// the program from (so from the next boot). Checked against the known data around them first; returns how many
	// it changed (2; 0: not the data known). show_free_play_text puts them back.
	int hide_free_play_text();
	void show_free_play_text();
	// Fighting Vipers 2's hidden fighters, made selectable: the one instruction that gates the select screen's
	// wider cursor, turned always-on (see the comment on it in model3.cpp, and docs/fv2-roster.md). Unlike the
	// notices above, the address is in RAM, so there is nothing to do until the boot has unpacked the program --
	// which is why this reads before it writes and the caller keeps calling. Returns 1 if it wrote the patch just
	// now, 0 if there was nothing to do (already there, or the program isn't unpacked yet), -1 for a game this
	// isn't known for.
	int unlock_roster();
	// TRAINING's arena: 0x0007111C is the function that draws the stage, and made to return `scene` the game
	// genuinely chooses it -- forcing the scene index instead was measured twice and gives one arena's
	// geometry under another's textures. Scene 3 is MARBLE ARENA. Read before written and the caller keeps
	// calling, as the roster patch does. model3.cpp carries the numbering and the two instructions.
	int fv2_hold_stage(int scene);
	int fv2_release_stage();
	// And the half that actually commands a game's FIRST arena, which the picker above does not: one byte of
	// the game's own route table, 0x000B4100 -- eight bytes a match, four (arena, opponent) pairs, the pair
	// chosen by 0x0058145D. model3.cpp carries the instructions it was read from.
	int fv2_hold_training_stage(int scene);
	int fv2_release_training_stage();
	uint8_t fv2_route_was = 0;
	bool fv2_route_held = false;
	// Honey's other costume, the one Sega gated behind thirty straight wins (docs/fv2-honey.md). Two
	// instructions in her own cloth handler, turned always-true, in the same shape as the roster patch above:
	// read before every write, and the caller keeps calling. 1 if it wrote just now, 0 if there was nothing to
	// do, -1 for a game this isn't known for.
	int honey_costume(bool on);
	// Makes the game build a fighter's model again, now, without the cursor having moved. What that is for: the
	// select screen's preview is the same model, built by the same task, and it is built once when the cursor
	// lands -- so a costume chosen afterwards would not show until the fight. See docs/fv2-honey.md.
	void fv2_redo_fighter_model(int player);

	// ---- Fighting Vipers 2's console mode (docs/fv2-console.md) ----
	// What the host has to read and write to put a console's own menu over this game. Every address below was
	// measured in this port before it was used, not carried over.
	//
	// The state of the task Sega's tables call entry1 -- the select screen's -- as a word. Measured through
	// tools/scripts/fv2-fight.txt: 0 for the whole attract loop, 5 on the select screen, 0x2A in a fight. What
	// is used is only "0 or not", which answers "is a game running". It is NOT a screen detector and must not
	// be made into one: which attract screen is up does not matter, because the menu covers the screen itself.
	// -1 for a game this isn't known for.
	int fv2_task_state() const;
	bool fv2_in_attract() const { return fv2_task_state() == 0; }
	// Survival: a flag the game reads, and the operator setting that has to agree with it. Measured as 0 and 0
	// on a stock board. The flag must NOT be set during the attract loop -- set there it stops the game
	// starting at all, the injected Start is ignored and no credit is ever taken -- so the host arms it and
	// only writes once a game is running, then holds it (docs/fv2-console.md).
	int fv2_survival() const;            // 1 on, 0 off, -1 not this game
	void fv2_set_survival(bool on);
	// RANDOM: the game's own flag for it, which Sega's debug tables name RANDOM_CHARA (0x0058145E, beside the
	// Survival flag). Set, the select screen says RANDOM MODE. It does not need holding; model3.cpp says what
	// it replaces and why.
	int fv2_random_chara() const;        // 1 on, 0 off, -1 not this game
	void fv2_set_random_chara(bool on);
	// The region byte. Measured 0 on this Japanese revision; 0 Japan, 1 U.S.A., 2 Export, 3 Asia. The game does
	// not write it back, so one write holds.
	int fv2_region() const;              // -1: not this game
	void fv2_set_region(int r);

	// ---- The game's own state machine (docs/fv2-etats.md) ----
	// Four contiguous bytes that Sega's own code reads and writes every frame: the mode, that mode's sub-state,
	// and the two that REQUEST a mode and a sub-state. The names are the game's own, left in RAM as eighty
	// strings in one run at 0x00024494, the first twenty-four of them the modes.
	//
	// All of it was measured on the ROM, on Supermodel, between the 14th and the 20th of August 2026 -- by
	// freezing the game one sub-state at a time and looking at the screen, because the names alone put the
	// attract slice two out, and a slicing taken from the names was caught wrong three separate times. A
	// contiguous table does not say where its slices begin.
	int fv2_mode() const;        // 2 ADV_INT 3 ADV_DSP 4 SEL_INT 5 SEL_DSP 6 GAME_INT 7 GAME_DSP ...; -1 not this game
	int fv2_submode() const;     // only GAME, SEL and ADV mean anything by it; every other mode leaves the byte alone
	// The game's own title screen is on screen: the mode is ADV and the sub-state is FV2_TITLE_INT or
	// FV2_TITLE_DSP. This is NOT the question fv2_in_attract answers -- that one is "is a game running", and it
	// is true of the warning, the SEGA logo, the title, the presentations, the demos and the rankings alike.
	bool fv2_on_title() const;
	// Asks the game for its title screen. The mode and sub-state here are a REQUEST: the dispatcher at
	// 0x0000ABC4 copies the request into the mode before it jumps, so the mode is not forced, it is asked for --
	// and it takes two frames. Written ONCE and never held: holding a request is what broke the select screen
	// and its countdown, twice.
	// It commands nothing from inside a fight, and the measurement that established that is worth more than the
	// failure: the bytes were still intact a second later, so nobody had read them.
	void fv2_request_title();
	// The attract sequencer's OWN road to the title screen, which is not the same thing and leaves a very
	// different screen behind: pushed to the end of its list with its timer expired, the game walks off the
	// end itself and installs the title screen the way it does on its own. Works from the attract loop and
	// only there. See model3.cpp for what the picture probe measured against the request above.
	void fv2_attract_to_title();
	// Asks the attract loop to start over (DONT_RECYCLE_INT), which is how the game itself leaves a replay --
	// and the restart is where it clears the tasks that draw. model3.cpp says how that was watched.
	void fv2_restart_attract();
	std::vector<uint8_t> security;   // 128 KB, host-order words
	std::vector<std::pair<size_t, std::vector<uint8_t>>> free_play_text;   // what hide_free_play_text replaced

	// The tile generator: its RAM as the device stores it (little-endian words) and its registers
	std::vector<uint8_t> tile_ram;   // 0x120000
	uint32_t tile_regs[64] = {};

	// The Real3D: what the CPU and the 53C810 write, kept as the device sees it (little-endian words)
	static constexpr size_t CULL_PAD = 0x10000;   // zero words past each culling RAM's end (model3.cpp)
	std::vector<uint32_t> cull_lo, cull_hi, poly_ram, tex_fifo;
	uint32_t r3d_config[4] = {};
	uint32_t vrom_fifo[2] = {};
	int vrom_fifo_n = 0;
	bool ping_pong = false, ping_pong_copy = false;
	unsigned flushes = 0;            // writes to 0x88000000 this frame
	uint32_t dma_src = 0, dma_dst = 0, dma_len = 0, dma_data = 0;
	uint8_t dma_status = 0, dma_config = 0;
	// The texture RAM: two 2048x1024 sheets of 16-bit texels (sheet 1 from row 1024), filled from the texture FIFO
	std::vector<uint16_t> tex_ram;
	uint32_t tex_serial = 0;         // counts texture uploads (renderers keep their copies by it)
	struct TexUpload { unsigned x, y, w, h; };
	std::vector<TexUpload> tex_uploads;   // the areas written since a renderer last took them
	bool command_written = false, tilegen_draw = false;
	// Writes to the high culling RAM and polygon RAM after the ping-pong flip wait for the next vertical blank
	std::vector<std::pair<uint32_t, uint32_t>> pending_hi, pending_poly;

	// The MIDI bytes sent to the sound board (played by the sound board once it exists)
	std::deque<uint8_t> midi;
	uint8_t midi_ctrl = 0;

	uint64_t frame = 0;
	std::function<void()> after_slice;   // checking: called after every slice the CPU runs (vf3lock)
	std::function<void(unsigned)> on_line;  // the video: called at the start of each of the 384 active lines

	bool load(const std::string &zip, std::string &error);
	void reset();
	void run_frame();
	// a frame in two halves (the host's smooth motion: each half in its own picture's time): the vertical blank and
	// lines 0-191, then lines 192-383
	void run_frame_start();
	void run_frame_end();
	void run_lines(unsigned from, unsigned to);
	uint32_t flip_line_ = 0;   // (read at the frame's start, as one loop did)

	// ppc::Bus
	uint8_t read8(uint32_t addr) override;
	uint16_t read16(uint32_t addr) override;
	uint32_t read32(uint32_t addr) override;
	uint64_t read64(uint32_t addr) override;
	void write8(uint32_t addr, uint8_t data) override;
	void write16(uint32_t addr, uint16_t data) override;
	void write32(uint32_t addr, uint32_t data) override;
	void write64(uint32_t addr, uint64_t data) override;

	uint64_t ram_hash() const;
	// RAM or CROM as the CPU reads it, without its side effects (0 elsewhere)
	uint16_t peek16(uint32_t addr) const;
	uint32_t peek32(uint32_t addr) const;
	std::vector<uint32_t> program_image() const;  // the program the boot puts in RAM 0, host-order words
	// The same stretch of RAM as it stands now. The recompiler is built from program_image() before the game
	// runs, and a game that patches its own program afterwards -- Fighting Vipers 2 writes its own handlers into
	// three of the PowerPC's exception vectors once the boot has unpacked it -- no longer agrees with it. One
	// word of disagreement turns the recompiler off entirely (there is no per-word fallback: the native code is
	// a straight-line translation and an instruction can be reached by falling into it), so once the boot is
	// over the recompiler is built again from this.
	std::vector<uint32_t> ram_program_image(size_t words) const;

	// How often the board reckons the CPU runs, for the frame's pacing: 66 MHz on Step 1.0, 166 MHz on Step 2.0.
	// The reference build paces a frame by this while the core counts its cycles at the bus multiplier's exact
	// rate (2.5 x 66 = 165 MHz on Step 2.0), and the decrementer's ratio comes from the two together; kept as it
	// does, so a frame here is the reference's frame.
	int ppc_hz = 66000000;

private:
	// The program image the boot unpacks, for sets that carry it compressed (Fighting Vipers 2: a gzip in the
	// CROM). Empty for a set whose program the boot copies from the CROM as it stands (Virtua Fighter 3).
	std::vector<uint32_t> unpacked_program;
	uint8_t crom_bank_reg = 0xFF;
	const uint8_t *crom_bank = nullptr;
	uint8_t input_bank = 0, serial1 = 0, serial2 = 0, adc_channel = 0, gun_reg = 0;
	uint8_t outputs[2] = {};
	bool gpus_ready = false;

	// The PCI bridge: an MPC105 on Step 1.x, an MPC106 on Step 2.x. Same registers, a different device ID, a
	// different set of reset values, and its CONFIG_ADDR and CONFIG_DATA ports at different addresses.
	int pci_model = 0x105;
	uint8_t pci_regs[256];
	uint32_t pci_bus = 0, pci_device = 0, pci_function = 0, pci_reg = 0;
	bool security_first_read = true;   // the security board answers its first data read with 0xFFFF0000
	void pci_reset();
	void pci_config_address(uint32_t data);
	uint32_t pci_config_read(unsigned bits, unsigned offset);
	void pci_config_write(unsigned bits, unsigned offset, uint32_t data);

	void set_crom_bank(uint8_t v);
	uint8_t read_inputs(unsigned reg);
	void write_inputs(unsigned reg, uint8_t data);
	uint8_t read_sysreg(unsigned reg);
	void write_sysreg(unsigned reg, uint8_t data);
	uint8_t read_rtc(unsigned reg);
	uint32_t read_security(unsigned reg);
	void write_security(unsigned reg, uint32_t data);
	uint16_t read_security_ram(uint32_t addr) const;   // what the 315-5881 reads: the security board's RAM
	bool scsi_mapped(uint32_t addr) const;

	uint32_t r3d_register(unsigned reg) const;
	void r3d_flush();
	void r3d_flush_textures();
	void r3d_upload(uint32_t header, const uint16_t *data, size_t avail);
	void r3d_store(unsigned x, unsigned y, unsigned w, unsigned h, const uint16_t *&data, const uint16_t *end, bool sixteen, bool lsb, bool msb);
	void r3d_texture_port(unsigned reg, uint32_t data);
	void r3d_dma_write32(unsigned reg, uint32_t data);
	uint32_t r3d_dma_read32(unsigned reg) const;
	void tile_ram_write32(unsigned addr, uint32_t data);
	void tile_reg_write(unsigned reg, uint32_t data);
};

} // namespace m3
