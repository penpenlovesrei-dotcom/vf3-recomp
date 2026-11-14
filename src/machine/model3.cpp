// The Model 3 Step 1.0 board. See model3.h.
#include "model3.h"
#include "romset.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <random>

namespace m3 {

namespace {

inline uint32_t bswap32(uint32_t v) { return __builtin_bswap32(v); }
inline uint16_t bswap16(uint16_t v) { return __builtin_bswap16(v); }
inline uint32_t &word(std::vector<uint8_t> &v, size_t at) { return *reinterpret_cast<uint32_t *>(&v[at]); }
inline uint16_t &half(std::vector<uint8_t> &v, size_t at) { return *reinterpret_cast<uint16_t *>(&v[at]); }

const uint32_t IRQ_SCSI = 0x100;     // the 53C810's interrupt, not maskable (as the reference wires it)
const uint32_t IRQ_R3D_DMA = 0x100;
const uint32_t PCI_ID_REAL3D = 0x16C311DB;        // Mercury, in Step 1.x config space and at the DMA port always
const uint32_t PCI_ID_REAL3D_STEP2 = 0x178611DB;  // the DMA device, in Step 2.x config space
const uint32_t PCI_ID_53C810 = 0x00011000;

} // namespace

// ---- IRQ controller ----

void Irq::assert_(unsigned bits)
{
	state |= bits;
	if (state & enable)
		cpu->set_irq_line(true);
	if (state & ~0xFFu)
		cpu->set_irq_line(true);
}

void Irq::deassert(unsigned bits)
{
	state &= ~bits;
	if (!(state & enable) && !(state & ~0xFFu))
		cpu->set_irq_line(false);
}

// ---- 93C46 EEPROM ----

static uint16_t reverse16(uint16_t v)
{
	uint16_t r = 0;
	for (int i = 0; i < 16; i++)
		if (v & (1 << i))
			r |= uint16_t(1 << (15 - i));
	return r;
}

Eeprom::Eeprom() { memset(regs, 0xFF, sizeof(regs)); }

void Eeprom::reset()
{
	receiving = true;
	locked = true;
	in = 0;
	out = 0;
	addr = 0;
	busy = 0;
	CS = 0;
}

void Eeprom::write(unsigned cs, unsigned clk, unsigned di)
{
	unsigned prev = CLK;
	CS = !!cs;
	CLK = !!clk;
	DI = !!di;
	if (!CS) {
		in = 0;
		receiving = true;
		busy = 5;
		return;
	}
	if (prev || !CLK)
		return;
	if (receiving) {
		in = (in << 1) | DI;
		if ((in & 0xFFFFFFC0) == 0x180) {             // READ
			addr = in & 0x3F;
			out = reverse16(regs[addr]);
			bits_out = 0;
			receiving = false;
		} else if (in == 0x13) {                      // write enable
			locked = false;
		} else if (in == 0x10) {                      // write disable
			locked = true;
		} else if ((in & 0xFFC00000) == 0x01400000) { // WRITE
			if (!locked) regs[(in >> 16) & 0x3F] = uint16_t(in);
			DO = 1;
		} else if ((in & 0xFFF00000) == 0x01100000) { // WRITE ALL
			if (!locked) for (auto &r : regs) r = uint16_t(in);
			DO = 1;
		} else if ((in & 0xFFFFFFC0) == 0x1C0) {      // ERASE
			if (!locked) regs[in & 0x3F] = 0xFFFF;
			DO = 1;
		} else if ((in & 0xFFFFFFF0) == 0x120) {      // ERASE ALL
			if (!locked) for (auto &r : regs) r = 0xFFFF;
			DO = 1;
		}
	} else {
		DO = out & 1;
		out >>= 1;
		if (++bits_out == 16) {
			addr = (addr + 1) & 0x3F;
			out = reverse16(regs[addr]);
			bits_out = 0;
		}
	}
}

unsigned Eeprom::read()
{
	if (receiving) {
		if (busy > 0) {
			--busy;
			return 0;
		}
		return 1;
	}
	return DO;
}

// ---- JTAG ----

namespace {
enum { TLR, RTI, SEL_DR, CAP_DR, SH_DR, EX1_DR, PAUSE_DR, EX2_DR, UPD_DR, SEL_IR, CAP_IR, SH_IR, EX1_IR, PAUSE_IR, EX2_IR, UPD_IR };
const int FSM[16][2] = {
	{ RTI, TLR }, { RTI, SEL_DR }, { CAP_DR, SEL_IR }, { SH_DR, EX1_DR }, { SH_DR, EX1_DR }, { PAUSE_DR, UPD_DR },
	{ PAUSE_DR, EX2_DR }, { SH_DR, UPD_DR }, { RTI, SEL_DR }, { CAP_IR, TLR }, { SH_IR, EX1_IR }, { SH_IR, EX1_IR },
	{ PAUSE_IR, UPD_IR }, { PAUSE_IR, EX2_IR }, { SH_IR, UPD_IR }, { RTI, SEL_DR },
};
}

Jtag::Jtag() { configure(0x10); }

// The chain, in the order the ASICs and 3D-RAMs sit on it, with the ID codes of the board's stepping (the
// reference build's; only the first ASIC of each kind gives the renderer its modeword, so the second Earth and
// the three further Mars are on the chain as ASICs with no kind of their own).
void Jtag::configure(int stepping)
{
	struct Device_spec { bool asic; int kind; uint32_t id; };
	const Device_spec step1[10] = {
		// Jupiter, Mercury, Venus, Earth, a 3D-RAM, two Mars, three 3D-RAMs
		{ true, 4, 0x116c7057 }, { true, 0, 0x216c3057 }, { true, 1, 0x116c4057 }, { true, 2, 0x216c5057 },
		{ false, -1, 0 }, { true, 3, 0x116c6057 }, { true, -1, 0x116c6057 }, { false, -1, 0 }, { false, -1, 0 },
		{ false, -1, 0 },
	};
	const Device_spec step2[17] = {
		// as Step 1.x to the fourth 3D-RAM, then the second Earth, a 3D-RAM, two more Mars and three 3D-RAMs
		{ true, 4, 0x416c7057 }, { true, 0, 0x416c3057 }, { true, 1, 0x316c4057 }, { true, 2, 0x416c5057 },
		{ false, -1, 0 }, { true, 3, 0x316c6057 }, { true, -1, 0x316c6057 }, { false, -1, 0 }, { false, -1, 0 },
		{ false, -1, 0 }, { true, -1, 0x416c5057 }, { false, -1, 0 }, { true, -1, 0x316c6057 },
		{ true, -1, 0x316c6057 }, { false, -1, 0 }, { false, -1, 0 }, { false, -1, 0 },
	};
	const Device_spec *chain = stepping >= 0x20 ? step2 : step1;
	n_dev = stepping >= 0x20 ? 17 : 10;
	for (int i = 0; i < n_dev; i++) {
		dev[i].asic = chain[i].asic;
		dev[i].kind = chain[i].kind;
		dev[i].id = chain[i].id;
		dev_reset(dev[i]);
	}
	state = TLR;
}

void Jtag::dev_reset(Device &d)
{
	if (d.asic) {
		d.size = 32;
		d.shift = d.id;
		d.ir = 3;   // IDCODE
	} else {
		d.size = 1;
		d.shift = 0;
		d.ir = 15;  // BYPASS
	}
}

void Jtag::capture_dr(Device &d)
{
	if (d.asic && d.ir == 3) { d.size = 32; d.shift = d.id; }
	else if (d.asic && d.ir == 8) { d.size = 32; d.shift = d.mode; }
	else { d.size = 1; d.shift = 0; }
}

void Jtag::capture_ir(Device &d)
{
	if (d.asic) { d.size = 5; d.shift = 1; }
	else { d.size = 4; d.shift = 0b1001; }
}

void Jtag::update_dr(Device &d)
{
	if (d.asic && d.ir == 8) {
		d.mode = uint32_t(d.shift);
		if (d.kind >= 0)
			modeword[d.kind] = d.mode;
	}
}

void Jtag::reset()
{
	state = TLR;
	for (int i = 0; i < n_dev; i++) dev_reset(dev[i]);
}

void Jtag::write(bool tck, bool tms, bool tdi, bool trst)
{
	if (!trst) {
		for (int i = 0; i < n_dev; i++) dev_reset(dev[i]);
		return;
	}
	if (last_tck == tck)
		return;
	last_tck = tck;
	if (tck) {
		switch (state) {
		case TLR: for (int i = 0; i < n_dev; i++) dev_reset(dev[i]); break;
		case CAP_DR: for (int i = 0; i < n_dev; i++) capture_dr(dev[i]); break;
		case CAP_IR: for (int i = 0; i < n_dev; i++) capture_ir(dev[i]); break;
		case SH_DR: case SH_IR:
			for (int i = 0; i < n_dev; i++) {
				Device &d = dev[i];
				bool out = d.shift & 1;
				d.shift >>= 1;
				if (tdi) d.shift |= uint64_t(1) << (d.size - 1);
				else d.shift &= ~(uint64_t(1) << (d.size - 1));
				tdi = out;
			}
			break;
		default: break;
		}
		state = FSM[state][tms];
	} else {
		switch (state) {
		case SH_DR: case SH_IR: tdo = dev[n_dev - 1].shift & 1; break;
		case UPD_DR: for (int i = 0; i < n_dev; i++) update_dr(dev[i]); break;
		case UPD_IR: for (int i = 0; i < n_dev; i++) dev[i].ir = uint8_t(dev[i].shift); break;
		default: break;
		}
	}
}

// ---- 53C810 ----

void Scsi::reset()
{
	memset(regs, 0, sizeof(regs));
	regs[0x00] = 0xC0;
	regs[0x0C] = 0x80;
	regs[0x0F] = 0x02;
	regs[0x18] = 0xFF;
	regs[0x19] = 0xF0;
	regs[0x1A] = 0x01;
	regs[0x46] = 0x60;
	regs[0x47] = 0x0F;
	regs[0x4C] = 0x03;
	temp = dsp = dsps = dbc = 0;
	dcmd = dcntl = dmode = dien = istat = 0;
	dstat = 0x80;
	halt = false;
}

uint32_t Scsi::fetch(uint32_t off)
{
	return bswap32(board->read32(dsp + off));
}

// One SCRIPTS instruction: memory moves and INT; anything else stops the processor
bool Scsi::exec()
{
	uint32_t op = fetch(0);
	dbc = op & 0x00FFFFFF;
	dcmd = uint8_t(op >> 24);
	dsps = fetch(4);
	if ((dcmd & 0xE0) == 0xC0) {        // memory move
		uint32_t src = dsps, dst = temp = fetch(8);
		uint32_t n = dbc;
		for (uint32_t i = 0; i < n / 4; i++, src += 4, dst += 4)
			board->write32(dst, board->read32(src));
		for (n &= 3; n; n--)
			board->write8(dst++, board->read8(src++));
		dbc = 0;
		dsps = src;
		temp = dst;
		dsp += 12;
		return true;
	}
	if ((dcmd & 0xF8) == 0x98) {        // INT / INTFLY
		halt = true;
		istat |= 1;
		dstat |= 4;
		if (dien & 4)
			board->irq.assert_(IRQ_SCSI);
		return !(dbc & 0x100000);       // INTFLY isn't emulated (nor by the reference)
	}
	return false;
}

void Scsi::run(bool single)
{
	// (a memory move into the 53C810's own registers started it again inside itself, deeper each time till the stack
	// ran out: a SCRIPTS program in a snapshot's RAM can. The game's never does)
	if (running) return;
	running = true;
	run_once(single);
	running = false;
}

void Scsi::run_once(bool single)
{
	if (single) {
		exec();
		istat |= 1;
		dstat |= 8;
		if (dien & 8)
			board->irq.assert_(IRQ_SCSI);
		return;
	}
	for (int i = 0; i < 100 && !halt; i++)
		if (!exec())
			break;
}

void Scsi::write(unsigned reg, uint8_t data)
{
	if (reg >= 0x60)
		return;
	regs[reg] = data;
	auto set_byte = [](uint32_t &r, int n, uint8_t v) { r = (r & ~(0xFFu << (8 * n))) | (uint32_t(v) << (8 * n)); };
	switch (reg) {
	case 0x14: istat = data; break;
	case 0x1C: case 0x1D: case 0x1E: case 0x1F: set_byte(temp, reg - 0x1C, data); break;
	case 0x24: case 0x25: case 0x26: set_byte(dbc, reg - 0x24, data); break;
	case 0x27: dcmd = data; break;
	case 0x2C: case 0x2D: case 0x2E: set_byte(dsp, reg - 0x2C, data); break;
	case 0x2F:
		set_byte(dsp, 3, data);
		halt = false;
		if (!(dmode & 1))
			run(false);
		break;
	case 0x30: case 0x31: case 0x32: case 0x33: set_byte(dsps, reg - 0x30, data); break;
	case 0x38: dmode = data; break;
	case 0x39: dien = data; break;
	case 0x3B:
		dcntl = data;
		if ((dcntl & 0x14) == 0x14)
			run(true);
		else if (dcntl & 0x04)
			run(false);
		break;
	default: break;
	}
}

uint8_t Scsi::read(unsigned reg)
{
	if (reg >= 0x60)
		return 0;
	switch (reg) {
	case 0x0C: {
		uint8_t r = dstat;
		istat &= 0xFE;
		dstat &= 0xF7;
		board->irq.deassert(IRQ_SCSI);
		return r;
	}
	case 0x14: return istat;
	case 0x1C: case 0x1D: case 0x1E: case 0x1F: return uint8_t(temp >> (8 * (reg - 0x1C)));
	case 0x24: case 0x25: case 0x26: return uint8_t(dbc >> (8 * (reg - 0x24)));
	case 0x27: return dcmd;
	case 0x2C: case 0x2D: case 0x2E: case 0x2F: return uint8_t(dsp >> (8 * (reg - 0x2C)));
	case 0x30: case 0x31: case 0x32: case 0x33: return uint8_t(dsps >> (8 * (reg - 0x30)));
	case 0x38: return dmode;
	case 0x39: return dien;
	case 0x3B: return dcntl;
	default: return regs[reg];
	}
}

// ---- the board ----

bool Model3::load(const std::string &zip, std::string &error)
{
	romset::result r = romset::load(zip, *profile);
	if (!r.ok) {
		error = r.error;
		return false;
	}
	crom = std::move(r.crom);
	banked = std::move(r.banked);
	vrom = std::move(r.vrom);
	sound_rom = std::move(r.sound);
	samples = std::move(r.samples);
	unpacked_program = std::move(r.program);
	set = r.set;
	title = r.title;
	stepping = r.stepping;
	crypto_key = r.crypto_key;
	pci_model = step2() ? 0x106 : 0x105;
	jtag.configure(stepping);
	// the security chip, on a board that has one: what it decrypts is that board's own RAM
	if (crypto_key)
		crypt.init(crypto_key, [this](uint32_t at) { return read_security_ram(at); });
	ram.assign(0x800000, 0);
	backup.assign(0x20000, 0);
	security.assign(0x20000, 0);
	tile_ram.assign(0x120000, 0);
	// (each with 64K words of zeros past its end: the scene builder reads a node's words, a matrix (up to 0xFFF * 12
	// words on), a LOD entry or a pointer list from wherever the game points, checking only the start)
	cull_lo.assign(0x400000 / 4 + CULL_PAD, 0);
	cull_hi.assign(0x100000 / 4 + CULL_PAD, 0);
	poly_ram.assign(0x400000 / 4, 0);
	tex_fifo.reserve(0x100000 / 4);
	tex_ram.assign(2048 * 2048, 0);
	setup_pages();
	cpu.attach(this, ram.data(), crom.data());
	// The CPU, as the reference build configures it for each stepping. Both are a 603R (the same PVR); what
	// differs is the bus multiplier, and from it the core's clock and the PLL configuration HID1 reports:
	//  - Step 1.0: multiplier 1.0 on a 66 MHz bus, so a 66 MHz core. The 603R's PLL table (MPC603E7TEC page 23,
	//    as the reference reads it) has no entry for 1.0 at 66 MHz, and the reference puts -1 in HID1's top
	//    nibble for that: 0xF0000000.
	//  - Step 2.0: multiplier 2.5 on the same bus, so a 165 MHz core, with PLL configuration 0x6 for 2.5:1 at
	//    66 MHz: 0x60000000. The frame is paced at 166 MHz (ppc_hz) as the reference paces it, which is where
	//    the decrementer's ratio of 10 rather than 10.06 comes from.
	// The multiplier is passed doubled, as the reference keeps it (1.0 -> 2, 2.5 -> 5).
	if (step2()) {
		cpu.configure(0x00071202, 5, 165000000, 0x60000000);
		ppc_hz = 166000000;
	} else {
		cpu.configure(0x00071202, 2, 66000000, 0xF0000000);
		ppc_hz = 66000000;
	}
	irq.cpu = &cpu;
	scsi.board = this;
	set_crom_bank(0xFF);
	return true;
}

void Model3::reset()
{
	std::fill(ram.begin(), ram.end(), 0);
	set_crom_bank(0xFF);
	input_bank = serial1 = serial2 = adc_channel = 0;
	midi_ctrl = 0;
	cpu.reset();
	if (cpu.rec)
		cpu.rec->recount();
	irq.reset();
	pci_reset();
	scsi.reset();
	eeprom.reset();
	security_first_read = true;
	security_reads = security_writes = 0;
	if (crypto_key)
		crypt.reset();
	std::fill(tile_ram.begin(), tile_ram.end(), 0);
	memset(tile_regs, 0, sizeof(tile_regs));
	std::fill(cull_lo.begin(), cull_lo.end(), 0);
	std::fill(cull_hi.begin(), cull_hi.end(), 0);
	std::fill(poly_ram.begin(), poly_ram.end(), 0);
	tex_fifo.clear();
	std::fill(tex_ram.begin(), tex_ram.end(), 0);
	snap_epoch++;   // (the memories filled: states saved before copy whole)
	tex_serial++;
	tex_uploads.clear();
	tex_uploads.push_back({ 0, 0, 2048, 2048 });
	command_written = tilegen_draw = false;
	pending_hi.clear();
	pending_poly.clear();
	memset(r3d_config, 0, sizeof(r3d_config));
	vrom_fifo_n = 0;
	ping_pong = ping_pong_copy = false;
	dma_src = dma_dst = dma_len = dma_data = 0;
	dma_status = dma_config = 0;
	jtag.reset();
	gpus_ready = false;
	frame = 0;
}

void Model3::power_on()
{
	// the PowerPC's registers (a reset leaves them: a board that had run kept its own), but what configure set
	const uint32_t pvr = cpu.s.pvr, hid1 = cpu.s.hid1;
	const int timer_ratio = cpu.s.timer_ratio;
	cpu.s = ppc::State{};
	cpu.s.pvr = pvr;
	cpu.s.hid1 = hid1;
	cpu.s.timer_ratio = timer_ratio;
	// the devices as made (the EEPROM keeping what it holds; the 53C810 its board)
	uint16_t words[64];
	memcpy(words, eeprom.regs, sizeof words);
	eeprom = Eeprom();
	memcpy(eeprom.regs, words, sizeof words);
	jtag = Jtag();
	scsi = Scsi{};
	scsi.board = this;
	std::fill(security.begin(), security.end(), 0);
	flushes = 0;
	vrom_fifo[0] = vrom_fifo[1] = 0;
	gun_reg = 0;
	outputs[0] = outputs[1] = 0;
	flip_line_ = 0;
	midi.clear();
	controls = Controls{};
	reset();
}

void Model3::set_crom_bank(uint8_t v)
{
	crom_bank_reg = v;
	crom_bank = banked.data() + size_t((~v) & 0xF) * 0x800000;
}

// One video frame, timed as the reference times it: 57.524160 Hz, 424 lines of which 40 are blanking; the
// vertical blank first (waiting for the game to acknowledge IRQ 2, then MIDI interrupts while it sends), then
// the 384 active lines, the ping-pong flip at the line the tile generator says and IRQ 2 at the start of the last.
void Model3::run_frame()
{
	run_frame_start();
	run_frame_end();
}

void Model3::run_lines(unsigned from, unsigned to)
{
	const unsigned cycles = unsigned(ppc_hz);
	const unsigned line_cycles = unsigned(float(cycles) / 57.524160f) / 424;
	const uint32_t flip_line = flip_line_;
	for (unsigned i = from; i < to; i++) {
		if (i == flip_line) {
			ping_pong = !ping_pong;
			tilegen_draw = command_written = false;
		}
		if (i == 383)
			irq.assert_(0x02);
		if (on_line) on_line(i);
		cpu.execute(int(line_cycles));
		if (after_slice) after_slice();
	}
}

void Model3::run_frame_start()
{
	if (debug_mode && profile->debug_flags)   // (Arcade settings > Debug mode: the secret code's bit, before each frame)
		write32(profile->debug_flags, read32(profile->debug_flags) | 0x200000);
	auto exec = [this](int n) {
		cpu.execute(n);
		if (after_slice) after_slice();
	};
	const unsigned cycles = unsigned(ppc_hz);
	const unsigned frame_cycles = unsigned(float(cycles) / 57.524160f);
	const unsigned line_cycles = frame_cycles / 424;
	unsigned vblank_cycles = line_cycles * 40;
	cpu.set_timer_ratio(int(unsigned(cpu.bus_freq_multiplier() * 2) * cycles / unsigned(cpu.cycles_per_second())));

	if (gpus_ready) {
		ping_pong_copy = ping_pong;
		for (auto &w : pending_hi) { cull_hi[w.first] = w.second; touch(2, size_t(w.first) * 4); }
		for (auto &w : pending_poly) { poly_ram[w.first] = w.second; touch(3, size_t(w.first) * 4); }
		pending_hi.clear();
		pending_poly.clear();
		if (command_written)
			r3d_flush_textures();
		while ((irq.enable & 2) && (irq.state & 2) && vblank_cycles > 1000) {
			exec(1000);
			vblank_cycles -= 1000;
		}
		int n = 0;
		while (midi_ctrl & 0x20) {
			if (!(irq.enable & 0x40))
				break;
			irq.assert_(0x40);
			exec(1000);
			vblank_cycles -= 1000;
			if (++n > 128)
				break;
		}
		exec(int(vblank_cycles));
		irq.assert_(0x0D);
	}
	flip_line_ = tile_regs[0x08 / 4];
	run_lines(0, 192);
}

void Model3::run_frame_end()
{
	run_lines(192, 384);
	gpus_ready = true;
	frame++;
}

// ---- save states ----

namespace {

// (writes waiting for a memory of `most` words: a state from elsewhere (a watched match's snapshot) with one past its
// end isn't taken)
template <class IO> void pairs(IO &io, std::vector<std::pair<uint32_t, uint32_t>> &v, size_t most)
{
	uint64_t n = v.size();
	io.pod(n);
	if constexpr (IO::loading) {
		if (!io.ok || n > 0x1000000) { io.ok = false; return; }
		v.resize(size_t(n));
	}
	for (auto &w : v) { io.pod(w.first); io.pod(w.second); }
	if constexpr (IO::loading)
		for (auto &w : v)
			if (w.first >= most) { io.ok = false; v.clear(); return; }
}

} // namespace

// The state's small parts, after the memories: the CPU, the board's registers, the Real3D's and the tile
// generator's, the writes and bytes waiting
template <class IO> void Model3::state(IO &io)
{
	io.section("cpu");
	cpu.s.state(io);
	io.section("board");
	irq.save_state(io);
	eeprom.save_state(io);
	jtag.save_state(io);
	scsi.save_state(io);
	for (int p = 0; p < 2; p++) {
		Controls &c = controls;
		io.pod(c.coin[p]); io.pod(c.start[p]); io.pod(c.test[p]); io.pod(c.service[p]);
		io.pod(c.up[p]); io.pod(c.down[p]); io.pod(c.left[p]); io.pod(c.right[p]);
		io.pod(c.punch[p]); io.pod(c.kick[p]); io.pod(c.guard[p]); io.pod(c.escape[p]);
	}
	io.pod(crom_bank_reg); io.pod(input_bank); io.pod(serial1); io.pod(serial2); io.pod(adc_channel); io.pod(gun_reg);
	io.pod(outputs); io.pod(gpus_ready);
	io.pod(pci_regs); io.pod(pci_bus); io.pod(pci_device); io.pod(pci_function); io.pod(pci_reg);
	io.pod(frame); io.pod(flip_line_);
	io.deq(midi);
	io.pod(midi_ctrl);
	io.section("tile generator");
	io.pod(tile_regs);
	io.pod(tilegen_draw);
	io.section("real3d");
	io.pod(r3d_config); io.pod(vrom_fifo); io.pod(vrom_fifo_n);
	io.pod(ping_pong); io.pod(ping_pong_copy); io.pod(flushes); io.pod(command_written);
	io.pod(dma_src); io.pod(dma_dst); io.pod(dma_len); io.pod(dma_data); io.pod(dma_status); io.pod(dma_config);
	pairs(io, pending_hi, cull_hi.size());
	pairs(io, pending_poly, poly_ram.size());
	io.vec(tex_fifo);
	io.section("end");
	if constexpr (IO::loading) io.check(state_sane());
}

// A state from another process (a watcher's snapshot, from any match's host) may hold anything: what's used as an
// index, a divisor, an address to fetch from or a count is as the board makes it, else the state's refused. The
// board's own states always are (a few comparisons a load)
bool Model3::state_sane() const
{
	const ppc::State &c = cpu.s;
	for (const uint8_t f : c.cr) if (f > 15) return false;
	auto fetchable = [](uint32_t pc) { return !(pc & 3) && (pc <= 0x007FFFFC || pc >= 0xFF800000); };
	if (!fetchable(c.pc) || !fetchable(c.npc) || c.timer_ratio < 1 || (c.reserved != 0 && c.reserved != 1)) return false;
	// (the texture FIFO and the sound bytes: no more than the board keeps; the frame's)
	return tex_fifo.size() <= 0x100000 / 4 && midi.size() <= 0x10000;
}

namespace {

struct StateHeader { uint32_t magic, version, id, epoch, gen, zero; };
static_assert(sizeof(StateHeader) == Model3::STATE_HEADER);

const char *const PAGED_NAMES[] = { "tile ram", "culling ram", "culling ram (high)", "polygon ram", "texture ram" };

} // namespace

void Model3::setup_pages()
{
	uint8_t *base[PAGED] = { tile_ram.data(), reinterpret_cast<uint8_t *>(cull_lo.data()), reinterpret_cast<uint8_t *>(cull_hi.data()),
		reinterpret_cast<uint8_t *>(poly_ram.data()), reinterpret_cast<uint8_t *>(tex_ram.data()) };
	const size_t bytes[PAGED] = { tile_ram.size(), cull_lo.size() * 4, cull_hi.size() * 4, poly_ram.size() * 4, tex_ram.size() * 2 };
	for (int r = 0; r < PAGED; r++) {
		paged[r].base = base[r];
		paged[r].bytes = bytes[r];
		paged[r].gen.assign((bytes[r] + PAGE - 1) / PAGE, 0);
	}
	std::random_device rd;
	snap_id = rd() ^ uint32_t(uintptr_t(this));
	snap_epoch++;
}

size_t Model3::fixed_state_size() const
{
	size_t n = STATE_HEADER + ram.size() + backup.size() + security.size();
	for (const Paged &r : paged) n += r.bytes;
	return n;
}

// The state: the header (for this board's saves only: which board, since which reset, which save), the memories at
// fixed places, then the rest. A save into a buffer this board saved before, since its last reset, copies only the
// device memories' pages written since (the RAM, which the recompiled code writes directly, always whole).
void Model3::save_state(std::vector<uint8_t> &out, std::vector<StateSection> *sections)
{
	if (sections) sections->clear();
	const size_t fixed = fixed_state_size();
	StateHeader h{};
	if (out.size() >= fixed) memcpy(&h, out.data(), sizeof h);
	const bool since = out.size() >= fixed && h.magic == STATE_MAGIC && h.version == STATE_VERSION && h.id == snap_id && h.epoch == snap_epoch;
	const uint32_t prev = since ? h.gen : 0;
	out.resize(fixed);   // (no allocation once the buffer has had a state)
	h = { STATE_MAGIC, STATE_VERSION, snap_id, snap_epoch, snap_gen, 0 };
	memcpy(out.data(), &h, sizeof h);
	size_t at = STATE_HEADER;
	auto whole = [&](const char *name, const void *src, size_t n) {
		if (sections) sections->push_back({ name, at });
		memcpy(out.data() + at, src, n);
		at += n;
	};
	whole("ram", ram.data(), ram.size());
	whole("backup", backup.data(), backup.size());
	whole("security", security.data(), security.size());
	for (int r = 0; r < PAGED; r++) {
		const Paged &g = paged[r];
		if (sections) sections->push_back({ PAGED_NAMES[r], at });
		if (!since) memcpy(out.data() + at, g.base, g.bytes);
		else
			for (size_t q = 0; q < g.gen.size(); q++)
				if (g.gen[q] > prev) memcpy(out.data() + at + q * PAGE, g.base + q * PAGE, std::min(PAGE, g.bytes - q * PAGE));
		at += g.bytes;
	}
	StateWriter w(out, sections, true);
	state(w);
	snap_gen++;   // (writes from now on are after this save)
}

bool Model3::load_state(const uint8_t *p, size_t n)
{
	const size_t fixed = fixed_state_size();
	StateHeader h{};
	if (n < fixed) return false;
	memcpy(&h, p, sizeof h);
	if (h.magic != STATE_MAGIC || h.version != STATE_VERSION) return false;
	// a state this board saved since its last reset: only the pages written since it was saved differ
	const bool since = h.id == snap_id && h.epoch == snap_epoch;
	size_t at = STATE_HEADER;
	auto whole = [&](void *dst, size_t m) { memcpy(dst, p + at, m); at += m; };
	whole(ram.data(), ram.size());
	whole(backup.data(), backup.size());
	whole(security.data(), security.size());
	textures_loaded = false;
	for (int r = 0; r < PAGED; r++) {
		Paged &g = paged[r];
		for (size_t q = 0; q < g.gen.size(); q++) {
			const size_t len = std::min(PAGE, g.bytes - q * PAGE);
			uint8_t *dst = g.base + q * PAGE;
			const uint8_t *src = p + at + q * PAGE;
			if (since ? g.gen[q] <= h.gen : !memcmp(dst, src, len)) continue;
			memcpy(dst, src, len);
			g.gen[q] = snap_gen;   // (changed now: the next save into any buffer copies it)
			if (r == PAGED - 1) {   // the texture RAM: a page a row, given to the renderers as changed
				const unsigned y = unsigned(q);
				if (!tex_uploads.empty() && tex_uploads.back().x == 0 && tex_uploads.back().w == 2048 && tex_uploads.back().y + tex_uploads.back().h == y)
					tex_uploads.back().h++;   // (joined to the row above)
				else tex_uploads.push_back({ 0, y, 2048, 1 });
				textures_loaded = true;
			}
		}
		at += g.bytes;
	}
	StateReader rd(p + fixed, n - fixed);
	state(rd);
	// what isn't kept but follows from what is
	set_crom_bank(crom_bank_reg);
	if (cpu.rec)
		cpu.rec->recount();
	if (textures_loaded) tex_serial++;   // (the renderers: what changed is in tex_uploads)
	return rd.done();
}

std::vector<uint32_t> Model3::program_image() const
{
	// Where the set carries its program compressed, the image is what the loader unpacked (Fighting Vipers 2: a
	// gzip at the fixed CROM's offset 0, 2,701,982 bytes of program, which the game's own boot stub unpacks into
	// RAM at 0 with zlib's inflate; see romset.cpp). Nothing here is needed for the board to run -- the stub does
	// the work -- only for the recompiler to know what code to build.
	if (!unpacked_program.empty())
		return unpacked_program;
	// Otherwise the boot copies the program from the fixed CROM to RAM 0 (Revision D's: 0xA0000 bytes from
	// 0x710000, docs/step1-measure.md)
	std::vector<uint32_t> img(profile->program_size / 4);
	memcpy(img.data(), &crom[profile->program_at], profile->program_size);
	return img;
}

std::vector<uint32_t> Model3::ram_program_image(size_t words) const
{
	words = std::min(words, ram.size() / 4);
	std::vector<uint32_t> img(words);
	memcpy(img.data(), ram.data(), words * 4);
	return img;
}

uint64_t Model3::ram_hash() const
{
	uint64_t h = 1469598103934665603ull;
	const uint32_t *w = reinterpret_cast<const uint32_t *>(ram.data());
	for (size_t i = 0; i < ram.size() / 4; i++)
		h = (h ^ w[i]) * 1099511628211ull;
	return h;
}

// ---- PCI bridge (MPC105 on Step 1.x, MPC106 on Step 2.x) ----

void Model3::pci_reset()
{
	memset(pci_regs, 0, sizeof(pci_regs));
	// The reference stores these as little-endian words and reads them back big-endian; kept as it does
	auto le = [this](unsigned at, uint32_t v) { for (int i = 0; i < 4; i++) pci_regs[at + i] = uint8_t(v >> (8 * i)); };
	pci_regs[0] = 0x57; pci_regs[1] = 0x10;  // vendor 0x1057 (Motorola)
	// device 0x0001 (MPC105) or 0x0002 (MPC106): this is how the game's boot tells the two bridges apart
	pci_regs[2] = uint8_t(pci_model == 0x106 ? 0x02 : 0x01); pci_regs[3] = 0x00;
	le(0x04, 0x00800006);   // PCI command and status
	le(0x08, 0x00060000);   // class code and revision
	if (pci_model == 0x106) {
		le(0x0C, 0x00000800);   // cache line size
		le(0x70, 0x00CD0000);   // output driver control
	}
	le(0xA8, 0x0010FF00);   // processor interface configuration 1
	le(0xAC, 0x060C000C);   // processor interface configuration 2
	le(0xB8, 0x04000000);
	if (pci_model == 0x106) {
		le(0xC0, 0x00000100);   // error enabling 1
		le(0xE0, 0x00420FFF);   // emulation support configuration 1
		le(0xE8, 0x00200000);   // emulation support configuration 2
	}
	le(0xF0, 0x0000FF02);   // memory control configuration 1
	le(0xF4, 0x00030000);   // memory control configuration 2
	le(0xFC, 0x00000010);   // memory control configuration 4
	pci_bus = pci_device = pci_function = pci_reg = 0;
}

void Model3::pci_config_address(uint32_t data)
{
	uint32_t d = bswap32(data);
	pci_bus = (d >> 16) & 0xFF;
	pci_device = (d >> 11) & 0x1F;
	pci_function = (d >> 8) & 7;
	pci_reg = d & 0xFF;
}

uint32_t Model3::pci_config_read(unsigned bits, unsigned offset)
{
	if (pci_device == 0) {
		const uint8_t *r = &pci_regs[(pci_reg + offset) & 0xFF];
		switch (bits) {
		case 8: return r[0];
		case 16: return uint32_t(r[0] << 8 | r[1]);
		default: return uint32_t(r[0]) << 24 | uint32_t(r[1]) << 16 | uint32_t(r[2]) << 8 | r[3];
		}
	}
	unsigned reg = (pci_reg >> 2) & 0x3C;
	if (bits == 8)
		return 0;
	uint32_t id;
	switch (pci_device) {
	// The Real3D: Mercury's ID on Step 1.x, the DMA device's on Step 2.x. (The DMA port itself answers with
	// Mercury's on every stepping, which is what r3d_dma_write32 sends; the reference notes that games ask it
	// there and expect the Step 1.x value even on a Step 2.x board.)
	case 13: id = step2() ? PCI_ID_REAL3D_STEP2 : PCI_ID_REAL3D; break;
	case 14: id = PCI_ID_53C810; break;
	case 16: return bits == 32 && reg == 0 ? 0x182711DB : 0;
	default: return 0;
	}
	if (reg != 0)
		return 0;
	uint32_t d = bswap32(id);
	if (bits == 16)
		d = (d >> ((2 - offset) * 8)) & 0xFFFF;
	return d;
}

void Model3::pci_config_write(unsigned bits, unsigned offset, uint32_t data)
{
	if (pci_device == 0) {
		uint8_t *r = &pci_regs[(pci_reg + offset) & 0xFF];
		switch (bits) {
		case 8: r[0] = uint8_t(data); break;
		case 16: r[0] = uint8_t(data >> 8); r[1] = uint8_t(data); break;
		default: r[0] = uint8_t(data >> 24); r[1] = uint8_t(data >> 16); r[2] = uint8_t(data >> 8); r[3] = uint8_t(data); break;
		}
		return;
	}
	unsigned reg = (pci_reg >> 2) & 0x3C;
	if (pci_device == 14 && reg == 4)
		scsi.base = uint8_t(data);  // where the 53C810's registers appear (0xC0 for 0xC0000000)
}

// ---- inputs, system registers, RTC, security ----

uint8_t Model3::read_inputs(unsigned reg)
{
	const Controls &c = controls;
	uint8_t d;
	switch (reg & 0x3F) {
	case 0x00: return input_bank;
	case 0x04:
		d = 0xFF;
		if (!(input_bank & 1)) {
			d &= ~(c.coin[0] << 0); d &= ~(c.coin[1] << 1); d &= ~(c.test[0] << 2);
			d &= ~(c.service[0] << 3); d &= ~(c.start[0] << 4); d &= ~(c.start[1] << 5);
		} else {
			d &= ~(c.service[1] << 6); d &= ~(c.test[1] << 7);
			d = uint8_t((d & 0xDF) | (eeprom.read() << 5));
		}
		return d;
	case 0x08: case 0x0C: {
		int p = (reg & 0x3F) == 0x0C;
		d = 0xFF;
		d &= ~(c.up[p] << 5); d &= ~(c.down[p] << 4); d &= ~(c.left[p] << 7); d &= ~(c.right[p] << 6);
		d &= ~(c.escape[p] << 3); d &= ~(c.guard[p] << 2); d &= ~(c.kick[p] << 1); d &= ~(c.punch[p] << 0);
		return d;
	}
	case 0x10: return outputs[0];
	case 0x14: return outputs[1];
	case 0x18: return debug_mode ? uint8_t(0x7F & ~DEBUG_DIPS) : 0x7F;   // (Arcade settings > Debug mode: its switch on)
	case 0x2C: return serial1;
	case 0x30: return serial2;
	case 0x34: return 0;
	case 0x3C: adc_channel++; return 0;  // no analogue inputs on VF3
	default: return 0xFF;
	}
}

void Model3::write_inputs(unsigned reg, uint8_t data)
{
	switch (reg & 0x3F) {
	case 0x00:
		eeprom.write((data >> 6) & 1, (data >> 7) & 1, (data >> 5) & 1);
		input_bank = data;
		break;
	case 0x10: outputs[0] = data; break;  // the drive board (VF3's billboard)
	case 0x14: outputs[1] = data; break;  // lamps
	case 0x24:                            // serial FIFO commands (light guns; none on VF3)
		if (data == 0x00) gun_reg = serial2;
		else if (data == 0x87) serial1 = serial2 = 0;
		break;
	case 0x28: serial2 = data; break;
	case 0x3C: adc_channel = data & 7; break;
	default: break;
	}
}

uint8_t Model3::read_sysreg(unsigned reg)
{
	switch (reg & 0x3F) {
	case 0x08: return crom_bank_reg;
	case 0x14: return uint8_t(irq.enable);
	case 0x18: return uint8_t(irq.state);
	case 0x1C: return 0xFF;
	case 0x10: return uint8_t(jtag.read() << 5);
	default: return 0xFF;
	}
}

void Model3::write_sysreg(unsigned reg, uint8_t data)
{
	switch (reg & 0x3F) {
	case 0x08: set_crom_bank(data); break;
	case 0x14: irq.enable = data; break;
	case 0x18: irq.deassert(data); break;
	case 0x0C: jtag.write((data >> 6) & 1, (data >> 2) & 1, (data >> 5) & 1, (data >> 7) & 1); break;
	default: break;
	}
}

uint8_t Model3::read_rtc(unsigned reg)
{
	// The host's local time, as the reference gives it (VF3 reads only registers 13 and 15, which read 0)
	time_t now = rtc_time >= 0 ? time_t(rtc_time) : time(nullptr);
	struct tm t;
#ifdef _WIN32
	if (rtc_time >= 0) gmtime_s(&t, &now);
	else localtime_s(&t, &now);
#else
	if (rtc_time >= 0) gmtime_r(&now, &t);
	else localtime_r(&now, &t);
#endif
	switch (reg & 0xF) {
	case 0: return (t.tm_sec % 10) & 0xF;
	case 1: return (t.tm_sec / 10) & 0x7;
	case 2: return (t.tm_min % 10) & 0xF;
	case 3: return (t.tm_min / 10) & 0x7;
	case 4: return (t.tm_hour % 10) & 0xF;
	case 5: return (t.tm_hour / 10) & 0x7;
	case 6: return (t.tm_mday % 10) & 0xF;
	case 7: return (t.tm_mday / 10) & 0x3;
	case 8: return ((t.tm_mon + 1) % 10) & 0xF;
	case 9: return ((t.tm_mon + 1) / 10) & 0x1;
	case 10: return (t.tm_year % 10) & 0xF;
	case 11: return ((t.tm_year % 100) / 10) & 0xF;
	case 12: return t.tm_wday & 0x7;
	default: return 0;
	}
}

// What the 315-5881 decrypts is the security board's RAM, which the game fills itself: its 32-bit words as the
// PowerPC wrote them, the chip taking the top half of the word at each counter value.
uint16_t Model3::read_security_ram(uint32_t addr) const
{
	if (addr >= 0x8000)
		return 0;
	return uint16_t(*reinterpret_cast<const uint32_t *>(&security[addr * 4]) >> 16);
}

uint32_t Model3::read_security(unsigned reg)
{
	// The security board's registers. Virtua Fighter 3's board has no security chip, and the reference's answers
	// for an unkeyed one are what this gives: status 0, and 0xFFFF0000 for the data register.
	//
	// Fighting Vipers 2's board carries a 315-5881 keyed 0x29260E96. The first data read after an address write
	// answers with a dummy word -- the RAM-based scheme expects one before the stream starts, and the reference
	// gives 0xFFFF0000 there -- and every read after it takes the next word from the chip.
	switch (reg) {
	case 0x00: return 0;   // status: ready
	case 0x1C:
		if (security_first_read) {
			security_first_read = false;
			return 0xFFFF0000;
		}
		security_reads++;
		if (!crypto_key)
			return 0xFFFF0000;
		{
			const uint16_t w = crypt.decrypt();
			// VF3_SECURITY_LOG=1: the stream the chip gives back, to check it against what the game does with it
			if (getenv("VF3_SECURITY_LOG"))
				fprintf(stderr, "security: word %3llu = %04X\n", (unsigned long long)security_reads - 1, w);
			return uint32_t(w) << 16;
		}
	default: return 0xFFFFFFFF;
	}
}

void Model3::write_security(unsigned reg, uint32_t data)
{
	security_writes++;
	if (!crypto_key)
		return;
	switch (reg) {
	case 0x10: case 0x14:
		// where in the security board's RAM to decrypt from. The Model 3 only ever points the chip at 0, as the
		// reference has it: the game writes its block to the start of that RAM and reads the whole stream back.
		// The high half goes last, because setting it is what restarts the chip's bit counters.
		crypt.set_addr_low(0);
		crypt.set_addr_high(0);
		security_first_read = true;
		break;
	case 0x18:
		// the sub-key, byte-swapped: the PowerPC is big-endian and the chip takes it the other way round
		crypt.set_subkey(uint16_t(bswap16(uint16_t(data >> 16))));
		break;
	default: break;
	}
	if (getenv("VF3_SECURITY_LOG"))
		fprintf(stderr, "security: register %02X = %08X%s\n", reg, data,
		        reg == 0x18 ? " (sub-key)" : reg == 0x10 || reg == 0x14 ? " (restart)" : "");
}

// The 53C810 answers at 0xC0000000 only where the board has one and its base register points there: Step 2.x
// has no SCSI controller at all, and leaves 0xC0000000 to the net board (which this port doesn't carry). Its
// other two windows, 0xF9000000 and 0xC1000000, stay as the reference leaves them on every stepping.
bool Model3::scsi_mapped(uint32_t) const { return stepping <= 0x15 && scsi.base == 0xC0; }

// ---- Real3D and tile generator ----

uint32_t Model3::r3d_register(unsigned reg) const
{
	if (reg == 0)
		return 0xFDFFFFFF | (ping_pong ? 0x02000000 : 0);
	if (reg >= 20 && reg <= 32)
		return 0;  // the line-of-sight values, from the renderer (0 until there is one)
	return 0xFFFFFFFF;
}

void Model3::r3d_flush()
{
	flushes++;
	command_written = true;
	if (ping_pong == ping_pong_copy)
		r3d_flush_textures();
}

// The texture FIFO: entries of a length word (in 16-bit units, less 2... as the reference reads it), a header and
// the texels
void Model3::r3d_flush_textures()
{
	size_t n = tex_fifo.size();
	if (n > 2) {
		for (size_t i = 0; i < n - 2;) {
			uint32_t size = (2 + tex_fifo[i] / 2) / 4;
			if (!size)
				break;
			const uint16_t *data = reinterpret_cast<const uint16_t *>(&tex_fifo[i + 2]);
			r3d_upload(tex_fifo[i + 1], data, (n - i - 2) * 2);
			i += size;
		}
	}
	tex_fifo.clear();
}

namespace {
// The texel order within an 8x8, 8x4 and 8x2 tile as the data arrives
const uint8_t decode8x8[64] = { 1, 0, 5, 4, 9, 8, 13, 12, 3, 2, 7, 6, 11, 10, 15, 14, 17, 16, 21, 20, 25, 24, 29, 28,
	19, 18, 23, 22, 27, 26, 31, 30, 33, 32, 37, 36, 41, 40, 45, 44, 35, 34, 39, 38, 43, 42, 47, 46, 49, 48, 53, 52,
	57, 56, 61, 60, 51, 50, 55, 54, 59, 58, 63, 62 };
const uint8_t decode8x4[32] = { 1, 0, 5, 4, 3, 2, 7, 6, 9, 8, 13, 12, 11, 10, 15, 14, 17, 16, 21, 20, 19, 18, 23, 22,
	25, 24, 29, 28, 27, 26, 31, 30 };
const uint8_t decode8x2[16] = { 1, 0, 3, 2, 5, 4, 7, 6, 9, 8, 11, 10, 13, 12, 15, 14 };
const int mip_x[] = { 0, 1024, 1536, 1792, 1920, 1984, 2016, 2032, 2040, 2044, 2046, 2047 };
const int mip_y[] = { 0, 512, 768, 896, 960, 992, 1008, 1016, 1020, 1022, 1023 };
} // namespace

void Model3::r3d_store(unsigned x0, unsigned y0, unsigned w, unsigned h, const uint16_t *&data, const uint16_t *end,
	bool sixteen, bool lsb, bool msb)
{
	const unsigned tx = std::min(8u, w), ty = std::min(8u, h);
	const uint8_t *dec = tx == 8 ? decode8x8 : tx == 4 ? decode8x4 : decode8x2;
	auto at = [&](size_t i) -> uint16_t { return data + i < end ? data[i] : 0; };
	if (tx < 2) return;
	if (sixteen) {
		for (unsigned y = y0; y < y0 + h; y += ty)
			for (unsigned x = x0; x < x0 + w; x += tx) {
				for (unsigned yy = 0; yy < ty; yy++)
					for (unsigned xx = 0; xx < tx; xx++)
						tex_ram[((y + yy) & 2047) * 2048 + ((x + xx) & 2047)] = at(dec[yy * tx + xx]);
				data += tx * ty;
			}
	} else {
		// 8-bit texels go to the low or high byte of the 16-bit texel (the rows of the order swapped)
		static const uint16_t keep[4] = { 0xFFFF, 0xFF00, 0x00FF, 0x0000 };
		const unsigned sel = unsigned(lsb) | (unsigned(msb) << 1);
		const unsigned step = std::max(1u, tx * ty / 2);
		for (unsigned y = y0; y < y0 + h; y += ty)
			for (unsigned x = x0; x < x0 + w; x += tx) {
				if (sel)
					for (unsigned yy = 0; yy < ty; yy++)
						for (unsigned xx = 0; xx < tx; xx++) {
							uint16_t &t = tex_ram[((y + yy) & 2047) * 2048 + ((x + xx) & 2047)];
							unsigned shift = 8 * ((xx & 1) ^ 1);
							unsigned idx = (yy ^ 1) * tx + (xx ^ 1) - (tx & 1);
							uint16_t v = (at(dec[idx] / 2) >> shift) & 0xFF;
							v |= v << 8;
							t = (t & keep[sel]) | (v & (keep[sel] ^ 0xFFFF));
						}
				data += step;
			}
	}
	for (unsigned y = y0; y < y0 + h + ty; y++) touch(4, size_t(y & 2047) * 4096);   // (its rows, a page each)
	tex_uploads.push_back({ x0, y0, w, h });
	tex_serial++;
}

// Texture header: X 32*(h&0x3F), Y 32*((h>>7)&0x1F), width 32<<((h>>14)&7), height 32<<((h>>17)&7), sheet bit 20,
// 8-bit data to the low byte (21) or high byte (22), 16-bit (23), type (h>>24): 0 with mipmaps, 1 without, 2 only
// mipmaps (0x80: not a texture)
void Model3::r3d_upload(uint32_t header, const uint16_t *data, size_t avail)
{
	const uint16_t *end = data + avail;
	unsigned x = 32 * (header & 0x3F), y = 32 * ((header >> 7) & 0x1F);
	unsigned page = (header >> 20) & 1;
	unsigned w = 32u << ((header >> 14) & 7), h = 32u << ((header >> 17) & 7);
	unsigned type = header >> 24;
	bool sixteen = (header >> 23) & 1, msb = (header >> 22) & 1, lsb = (header >> 21) & 1;
	if (type > 2)
		return;
	if (type != 2)
		r3d_store(x, y + page * 1024, w, h, data, end, sixteen, lsb, msb);
	if (type == 1)
		return;
	for (int i = 1; w >= 4 && h >= 4 && i < int(std::size(mip_y)); i++) {   // (a 4096 x 4096 header, a snapshot's texture FIFO's, ran past the levels' table)
		unsigned mx = mip_x[i] + (x >> i), my = mip_y[i] + (y >> i);
		w /= 2;
		h /= 2;
		r3d_store(mx, my + page * 1024, w, h, data, end, sixteen, lsb, msb);
	}
}

// The VROM texture port, 0x90000000-0x9000000B. The two steppings use it in entirely different ways.
void Model3::r3d_texture_port(unsigned, uint32_t data)
{
	const uint32_t *v = reinterpret_cast<const uint32_t *>(vrom.data());
	if (!step2()) {
		// Step 1.0: the data is a VROM word address, and the length is the first word there; the words go into
		// the texture FIFO, which is unpacked at the frame's start
		uint32_t addr = data & 0xFFFFFF;
		uint32_t n = (2 + v[addr] / 2) / 4;
		for (uint32_t i = 0; i < n && tex_fifo.size() < 0x100000 / 4; i++)   // (the same words; a count of millions, a VROM word a program in a snapshot points at, no longer run through once the FIFO's full)
			tex_fifo.push_back(v[(addr + i) & 0xFFFFFF]);
		return;
	}
	// Step 1.5 and later: the three words of the port are an address, then the texture's header, then a write
	// that sends it (whatever that one carries). Nothing goes through the FIFO: the Real3D fetches the texture
	// itself, from polygon RAM for the first 4 MB of the space and from the VROM above it. Addresses are in
	// 32-bit words. (The reference build does the same, and the first 4 MB mapping to polygon RAM is its note.)
	if (vrom_fifo_n < 2) {
		vrom_fifo[vrom_fifo_n++] = data;
		return;
	}
	const uint32_t addr = vrom_fifo[0], header = vrom_fifo[1];
	vrom_fifo_n = 0;
	if (addr < 0x100000) {
		const size_t at = addr & 0xFFFFFF;
		if (at < poly_ram.size())
			r3d_upload(header, reinterpret_cast<const uint16_t *>(&poly_ram[at]), (poly_ram.size() - at) * 2);
	} else {
		const size_t words = vrom.size() / 4, at = addr & 0xFFFFFF;
		if (at < words)
			r3d_upload(header, reinterpret_cast<const uint16_t *>(&v[at]), (words - at) * 2);
	}
}

uint32_t Model3::r3d_dma_read32(unsigned reg) const { return reg == 0x14 ? dma_data : 0; }

void Model3::r3d_dma_write32(unsigned reg, uint32_t data)
{
	switch (reg) {
	case 0x00: dma_src = data; break;
	case 0x04: dma_dst = data; break;
	case 0x08:
		// (a DMA into this register started another inside it, deeper each time till the stack ran out; and a length of
		// billions ran for minutes: a program in a snapshot can ask either. The game's: short, never into the DMA's
		// own registers; done as ever)
		if (dma_running) break;
		dma_len = data;
		dma_running = true;
		for (uint32_t guard = 0; dma_len && guard < 0x400000; guard++) {
			uint32_t v = read32(dma_src);
			write32(dma_dst, (dma_config & 0x80) ? bswap32(v) : v);
			dma_src += 4;
			dma_dst += 4;
			--dma_len;
		}
		dma_running = false;
		if (dma_config & 1) {
			dma_status |= 1;
			irq.assert_(IRQ_R3D_DMA);
		}
		break;
	case 0x10:
		if (data & 0x20000000) dma_data = PCI_ID_REAL3D;
		else if (data & 0x80000000) dma_data = r3d_register(data & 0x3F);
		break;
	case 0x14: dma_data = 0xFFFFFFFF; break;
	default: break;
	}
}

uint16_t Model3::peek16(uint32_t a) const
{
	if (a < 0x00800000) return *reinterpret_cast<const uint16_t *>(&ram[(a & 0x7FFFFE) ^ 2]);
	if ((a >> 24) == 0xFF) return *reinterpret_cast<const uint16_t *>(&(a < 0xFF800000 ? crom_bank : crom.data())[(a & 0x7FFFFE) ^ 2]);
	return 0;
}

uint32_t Model3::peek32(uint32_t a) const
{
	return uint32_t(peek16(a)) << 16 | peek16(a + 2);
}

void Model3::tile_ram_write32(unsigned addr, uint32_t data)
{
	word(tile_ram, addr) = data;   // the palette (addresses from 0x100000) is decoded by the renderer
	touch(0, addr);
}

void Model3::tile_reg_write(unsigned reg, uint32_t data)
{
	reg &= 0xFF;
	if (reg == 0x10)
		irq.deassert(data & 0xFF);
	tile_regs[reg / 4] = data;
}

// ---- the PowerPC's address space ----

uint8_t Model3::read8(uint32_t a)
{
	if (a < 0x00800000)
		return ram[a ^ 3];
	switch (a >> 24) {
	case 0xFF: return (a < 0xFF800000 ? crom_bank : crom.data())[(a & 0x7FFFFF) ^ 3];
	case 0xC2:
		switch (a & 0xFF) {
		case 0xC: return dma_status;
		case 0xE: return dma_config;
		default: return 0;
		}
	case 0xF0: case 0xFE:
		switch ((a >> 16) & 0xFF) {
		case 0x04: return read_inputs(a & 0x3F);
		case 0x08: return (a & 0xF) == 4 ? 0x83 : 0;   // the MIDI ports read as the reference returns them
		case 0x10: return read_sysreg(a & 0x3F);
		case 0x14:
			if ((a & 3) == 1) return 0x03;               // battery voltage
			if ((a & 3) == 0) return read_rtc((a >> 2) & 0xF);
			return 0;
		default: break;
		}
		break;
	case 0xF1:
		if (a < 0xF1120000) return tile_ram[a & 0x1FFFFF];
		break;
	case 0xC0:
		if (!scsi_mapped(a)) break;
		[[fallthrough]];
	case 0xF9: case 0xC1:
		return scsi.read(a & 0xFF);
	default: break;
	}
	return 0xFF;
}

uint16_t Model3::read16(uint32_t a)
{
	if (a & 1)
		return uint16_t(read8(a) << 8 | read8(a + 1));
	if (a < 0x00800000)
		return half(ram, a ^ 2);
	switch (a >> 24) {
	case 0xFF: return *reinterpret_cast<const uint16_t *>(&(a < 0xFF800000 ? crom_bank : crom.data())[(a & 0x7FFFFF) ^ 2]);
	case 0xF0: case 0xFE:
		switch ((a >> 16) & 0xFF) {
		case 0x0C: case 0x0D: return half(backup, (a & 0x1FFFF) ^ 2);
		case 0xC0:   // 0xF0C00CF8: the MPC105's CONFIG_DATA, on Step 1.x
			if (pci_model == 0x105) return uint16_t(pci_config_read(16, a & 3));
			break;
		// 0xFEE00000-0xFEFFFFFF: the MPC106's CONFIG_DATA, on Step 2.x
		case 0xE0: case 0xE1: case 0xE2: case 0xE3: case 0xE4: case 0xE5: case 0xE6: case 0xE7:
		case 0xE8: case 0xE9: case 0xEA: case 0xEB: case 0xEC: case 0xED: case 0xEE: case 0xEF:
			if (pci_model == 0x106) return uint16_t(pci_config_read(16, a & 3));
			break;
		default: break;
		}
		break;
	case 0xF1:
		if (a < 0xF1120000) return bswap16(half(tile_ram, a & 0x1FFFFF));
		break;
	default: break;  // 0xC0: the net board's RAM on later steppings (not on VF3's)
	}
	return 0xFFFF;
}

uint32_t Model3::read32(uint32_t a)
{
	if (a & 3)
		return uint32_t(read16(a)) << 16 | read16(a + 2);
	if (a < 0x00800000)
		return word(ram, a);
	switch (a >> 24) {
	case 0xFF: return *reinterpret_cast<const uint32_t *>(&(a < 0xFF800000 ? crom_bank : crom.data())[a & 0x7FFFFF]);
	case 0x84: return bswap32(r3d_register(a & 0x3F));
	case 0xC2: return bswap32(r3d_dma_read32(a & 0xFF));
	case 0xF0: case 0xFE:
		switch ((a >> 16) & 0xFF) {
		case 0x04:
			return uint32_t(read_inputs((a & 0x3F) + 0)) << 24 | uint32_t(read_inputs((a & 0x3F) + 1)) << 16 |
			       uint32_t(read_inputs((a & 0x3F) + 2)) << 8 | read_inputs((a & 0x3F) + 3);
		case 0x0C: case 0x0D: return word(backup, a & 0x1FFFF);
		case 0x10:
			return uint32_t(read_sysreg((a & 0x3F) + 0)) << 24 | uint32_t(read_sysreg((a & 0x3F) + 1)) << 16 |
			       uint32_t(read_sysreg((a & 0x3F) + 2)) << 8 | read_sysreg((a & 0x3F) + 3);
		case 0xC0:   // 0xF0C00CF8: the MPC105's CONFIG_DATA, on Step 1.x
			if (pci_model == 0x105) return pci_config_read(32, 0);
			break;
		// 0xFEE00000-0xFEFFFFFF: the MPC106's CONFIG_DATA, on Step 2.x
		case 0xE0: case 0xE1: case 0xE2: case 0xE3: case 0xE4: case 0xE5: case 0xE6: case 0xE7:
		case 0xE8: case 0xE9: case 0xEA: case 0xEB: case 0xEC: case 0xED: case 0xEE: case 0xEF:
			if (pci_model == 0x106) return pci_config_read(32, 0);
			break;
		case 0x14: return uint32_t(read_rtc((a >> 2) & 0xF)) << 24 | 0x00030000;
		case 0x18: case 0x19: return word(security, a & 0x1FFFF);
		case 0x1A: return read_security(a & 0x3F);
		default: break;
		}
		break;
	case 0xF1:
		if (a < 0xF1120000) return bswap32(word(tile_ram, a & 0x1FFFFF));
		if (a >= 0xF1180000 && a < 0xF1180100) return bswap32(tile_regs[(a & 0xFF) / 4]);
		break;
	case 0xC0:
		if (!scsi_mapped(a)) break;
		[[fallthrough]];
	case 0xF9: case 0xC1:
		return uint32_t(scsi.read((a + 0) & 0xFF)) << 24 | uint32_t(scsi.read((a + 1) & 0xFF)) << 16 |
		       uint32_t(scsi.read((a + 2) & 0xFF)) << 8 | scsi.read((a + 3) & 0xFF);
	default: break;
	}
	return 0xFFFFFFFF;
}

uint64_t Model3::read64(uint32_t a)
{
	uint64_t hi = read32(a);
	return hi << 32 | read32(a + 4);
}

void Model3::write8(uint32_t a, uint8_t d)
{
	if (a < 0x00800000) {
		ram[a ^ 3] = d;
		cpu.ram_written(a & ~3u);
		return;
	}
	switch (a >> 24) {
	case 0xC2:
		if ((a & 0xFF) == 0xD && (d & 1)) {
			dma_status &= ~1;
			irq.deassert(IRQ_R3D_DMA);
		} else if ((a & 0xFF) == 0xE)
			dma_config = d;
		break;
	case 0xF0: case 0xFE:
		switch ((a >> 16) & 0xFF) {
		case 0x04: write_inputs(a & 0x3F, d); break;
		case 0x08:
			if ((a & 0xF) == 0) {         // MIDI data
				midi.push_back(d);
				irq.deassert(0x40);
			} else if ((a & 0xF) == 4) {  // MIDI control
				midi_ctrl = d;
				if (!(d & 0x20))
					irq.deassert(0x40);
			}
			break;
		case 0x0C: case 0x0D: backup[(a & 0x1FFFF) ^ 3] = d; break;
		case 0x10: write_sysreg(a & 0x3F, d); break;
		default: break;
		}
		break;
	case 0xF1:
		if (a < 0xF1120000) {
			uint32_t at = a & 0x1FFFFF;
			uint32_t w = word(tile_ram, at & ~3u);
			uint32_t sh = (at & 3) * 8;
			tile_ram_write32(at & ~3u, (w & ~(0xFFu << sh)) | (uint32_t(d) << sh));
		}
		break;
	// 0xF8FFF000-0xF8FFF0FF: the bridge's own window. The reference build lets the MPC105 be written here and
	// leaves the MPC106 alone, on every access width; kept as it does.
	case 0xF8: if (pci_model == 0x105) pci_regs[a & 0xFF] = d; break;
	case 0xC0:
		if (!scsi_mapped(a)) break;
		[[fallthrough]];
	case 0xF9: case 0xC1:
		scsi.write(a & 0xFF, d);
		break;
	default: break;
	}
}

void Model3::write16(uint32_t a, uint16_t d)
{
	if (a & 1) {
		write8(a, uint8_t(d >> 8));
		write8(a + 1, uint8_t(d));
		return;
	}
	if (a < 0x00800000) {
		half(ram, a ^ 2) = d;
		cpu.ram_written(a & ~3u);
		return;
	}
	switch (a >> 24) {
	case 0xF0: case 0xFE:
		switch ((a >> 16) & 0xFF) {
		case 0x0C: case 0x0D: half(backup, (a & 0x1FFFF) ^ 2) = d; break;
		case 0xC0: pci_config_write(16, a & 2, d); break;
		default: break;
		}
		break;
	case 0xF1:
		if (a < 0xF1120000) {
			uint32_t at = a & 0x1FFFFF;
			tile_ram[at] = uint8_t(d >> 8);
			tile_ram[at + 1] = uint8_t(d);
			touch(0, at);
		}
		break;
	case 0xF8:
		if (pci_model == 0x105) {
			pci_regs[a & 0xFF] = uint8_t(d >> 8);
			pci_regs[(a + 1) & 0xFF] = uint8_t(d);
		}
		break;
	default: break;
	}
}

void Model3::write32(uint32_t a, uint32_t d)
{
	if (a & 3) {
		write16(a, uint16_t(d >> 16));
		write16(a + 2, uint16_t(d));
		return;
	}
	if (a < 0x00800000) {
		word(ram, a) = d;
		cpu.ram_written(a);
		return;
	}
	switch (a >> 24) {
	case 0x88: r3d_flush(); break;
	case 0x8C: cull_lo[(a & 0x3FFFFF) / 4] = bswap32(d); touch(1, a & 0x3FFFFC); break;
	case 0x8E:
		if (ping_pong != ping_pong_copy) pending_hi.push_back({ (a & 0xFFFFF) / 4, bswap32(d) });
		else { cull_hi[(a & 0xFFFFF) / 4] = bswap32(d); touch(2, a & 0xFFFFC); }
		break;
	case 0x90: r3d_texture_port(a & 0xFF, bswap32(d)); break;
	case 0x94: if (tex_fifo.size() < 0x100000 / 4) tex_fifo.push_back(bswap32(d)); break;
	case 0x98:
		if (ping_pong != ping_pong_copy) pending_poly.push_back({ (a & 0x3FFFFF) / 4, bswap32(d) });
		else { poly_ram[(a & 0x3FFFFF) / 4] = bswap32(d); touch(3, a & 0x3FFFFC); }
		break;
	case 0x9C: r3d_config[(a & 0xF) / 4] = bswap32(d); break;
	case 0xC2: r3d_dma_write32(a & 0xFF, bswap32(d)); break;
	case 0xF0: case 0xFE: {
		unsigned sub = (a >> 16) & 0xFF;
		switch (sub) {
		case 0x04:
			for (int i = 0; i < 4; i++) write_inputs((a & 0x3F) + i, uint8_t(d >> (24 - 8 * i)));
			break;
		case 0x0C: case 0x0D: word(backup, a & 0x1FFFF) = d; break;
		case 0x80:   // 0xF0800CF8: the MPC105's CONFIG_ADDR, on Step 1.x
			if (pci_model == 0x105) pci_config_address(d);
			break;
		case 0x10:
			for (int i = 0; i < 4; i++) write_sysreg((a & 0x3F) + i, uint8_t(d >> (24 - 8 * i)));
			break;
		case 0x18: case 0x19: word(security, a & 0x1FFFF) = d; break;
		case 0x1A: write_security(a & 0x3F, d); break;
		default:
			// The PCI bridge's config ports: on Step 1.x the MPC105's CONFIG_DATA at 0xF0C00CF8, on Step 2.x the
			// MPC106's CONFIG_ADDR at 0xFEC00000-0xFEDFFFFF and its CONFIG_DATA at 0xFEE00000-0xFEEFFFFF
			if (sub >= 0xC0 && sub <= 0xEF) {
				if (pci_model == 0x105 && a >= 0xF0C00CF8 && a < 0xF0C00D00)
					pci_config_write(32, 0, d);
				else if (pci_model == 0x106 && a >= 0xFEC00000 && a < 0xFEE00000)
					pci_config_address(d);
				else if (pci_model == 0x106 && a >= 0xFEE00000 && a < 0xFEF00000)
					pci_config_write(32, 0, d);
			}
			break;
		}
		break;
	}
	case 0xF1:
		if (a < 0xF1120000)
			tile_ram_write32(a & 0x1FFFFF, bswap32(d));
		else if (a >= 0xF1180000 && a < 0xF1180100)
			tile_reg_write(a & 0xFF, bswap32(d));
		break;
	case 0xF8:
		if (pci_model == 0x105)
			for (int i = 0; i < 4; i++) pci_regs[((a & 0xFF) + i) & 0xFF] = uint8_t(d >> (24 - 8 * i));
		break;
	case 0xC0:
		if (!scsi_mapped(a)) break;
		[[fallthrough]];
	case 0xF9: case 0xC1:
		for (int i = 0; i < 4; i++) scsi.write(((a & 0xFF) + i) & 0xFF, uint8_t(d >> (24 - 8 * i)));
		break;
	default: break;
	}
}

void Model3::write64(uint32_t a, uint64_t d)
{
	write32(a, uint32_t(d >> 32));
	write32(a + 4, uint32_t(d));
}

// The FREE PLAY notices, in the fixed CROM the boot copies the program from (the program image's address + the
// profile's program_at; the CROM holds the PowerPC's 32-bit words, so byte a of the image is crom[(a + at) ^ 3]).
// Revision D's (the profile's free_play_text):
// - the fight's top line (alternating with PRESS START BUTTON): "     FREE PLAY       " (21 characters) at the
//   image's 0x7CAE0, between "PRESS START BUTTON   " and "CREDIT": spaces;
// - the attract's, bottom left: picture 0x4018 (its number from a table at 0x7CB50), drawn by the call at 0x4E9C0
//   (bl 0x33300, the routine that draws a picture into a text window): a no-op instead.
int Model3::hide_free_play_text()
{
	if (set != "vf3") return 0;   // the addresses below are Virtua Fighter 3's own
	if (!free_play_text.empty()) return int(free_play_text.size());   // (hidden already)
	const size_t CROM_AT = profile->program_at;
	const Profile::FreePlayText &t = profile->free_play_text;
	if (!t.free_play) return 0;   // (not known for this program)
	auto at = [&](size_t a) -> uint8_t & { return crom[(a + CROM_AT) ^ 3]; };
	auto is = [&](size_t a, const char *t) {
		for (size_t k = 0; t[k]; k++)
			if (a + CROM_AT + k >= crom.size() || at(a + k) != uint8_t(t[k])) return false;
		return true;
	};
	auto word = [&](size_t a) { return uint32_t(at(a)) << 24 | uint32_t(at(a + 1)) << 16 | uint32_t(at(a + 2)) << 8 | at(a + 3); };
	auto patch = [&](size_t a, const uint8_t *to, size_t n) {
		std::vector<uint8_t> was;
		for (size_t k = 0; k < n; k++) { was.push_back(at(a + k)); at(a + k) = to[k]; }
		free_play_text.push_back({ a, was });
	};
	if (!is(t.press_start, "PRESS START BUTTON   ") || !is(t.free_play, "     FREE PLAY       ") || !is(t.credit, "CREDIT") ||
		word(t.picture_table) != t.picture || word(t.draw_call) != t.draw_word)
		return 0;
	const uint8_t spaces[21] = { ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ' };
	const uint8_t nop[4] = { 0x60, 0x00, 0x00, 0x00 };
	patch(t.free_play, spaces, sizeof spaces);
	patch(t.draw_call, nop, sizeof nop);
	return int(free_play_text.size());
}

int Model3::set_dural(bool on, bool offline, bool legacy)
{
	const size_t CROM_AT = profile->program_at;
	auto at = [&](size_t a) -> uint8_t & { return crom[(a + CROM_AT) ^ 3]; };
	auto word = [&](size_t a) { return uint32_t(at(a)) << 24 | uint32_t(at(a + 1)) << 16 | uint32_t(at(a + 2)) << 8 | at(a + 3); };
	auto put = [&](size_t a, uint32_t v) { for (int k = 0; k < 4; k++) at(a + k) = uint8_t(v >> (24 - 8 * k)); };
	const DuralPatch *d = profile->dural;
	if (!on) {
		for (auto it = dural_words.rbegin(); it != dural_words.rend(); ++it) put(it->first, it->second);
		if (!dural_words.empty() && d && ram.size() >= d->ram_to) std::fill(ram.begin() + d->ram_from, ram.begin() + d->ram_to, 0);   // (her heads' copies: as the game leaves that RAM)
		dural_words.clear();
		dural_offline = dural_legacy = false;
		return 0;
	}
	if (!dural_words.empty() && (dural_offline != offline || dural_legacy != legacy)) set_dural(false);   // (the other set: from the start)
	if (!dural_words.empty()) return int(dural_words.size());   // (on already)
	if (!d || crom.size() < CROM_AT + profile->program_size) return 0;
	for (size_t i = 0; i < d->known_count; i++)
		if (word(d->known[i][0]) != d->known[i][1]) return 0;
	const size_t count = legacy ? d->legacy_count : d->patch_count;
	for (size_t i = 0; i < count; i++)
		if (word(d->patch[i][0]) != d->patch[i][1]) return 0;
	for (size_t i = 0; i < count; i++) {
		dural_words.push_back({ d->patch[i][0], d->patch[i][1] });
		put(d->patch[i][0], d->patch[i][2]);
	}
	dural_legacy = legacy;
	if (offline) {
		bool known = true;
		for (size_t i = 0; i < d->offline_count; i++) known = known && word(d->offline[i][0]) == d->offline[i][1];
		for (size_t i = 0; known && i < d->offline_count; i++) {
			dural_words.push_back({ d->offline[i][0], d->offline[i][1] });
			put(d->offline[i][0], d->offline[i][2]);
		}
		dural_offline = true;
	}
	return int(dural_words.size());
}

void Model3::show_free_play_text()
{
	for (auto &[a, bytes] : free_play_text)
		for (size_t k = 0; k < bytes.size(); k++) crom[(a + profile->program_at + k) ^ 3] = bytes[k];
	free_play_text.clear();
}

// Fighting Vipers 2's hidden fighters on the character select: Del Sol, Kuhn and B.M.
//
// Measured on the Japan Revision A set. The other Fighting Vipers 2 set is a different build and its own
// address was never looked for, so it is left alone rather than patched at an address that means nothing
// there.
//
// THE GATE, and the two BOUNDS of the row it opens.
//
// The gate alone gives a row of fifteen cells, 0 to 14, and the last of them is a SECOND KUHN: cursor 12 is
// id 13 (KUHN) and cursor 14 is id 12 (DURAL), the same character under his old development name, with three
// times the armour. On the select screen he wears BAHN's name plate, the drawing routine stopping before his
// cell -- photographed here at cursors 12, 13 and 14: KUHN, nothing, BAHN. That cell goes.
//
// It is not the gate that says how wide the row is. The gate's 192 is the operator input port, both buttons
// held at once; the width is two numbers in the handler the gate opens, found by watching who writes the
// cursor (vf3run --watch 279668 names PC 0005F484) and reading around it:
//
//   0005F444   li r0,14            left from cell 0 lands on 14
//   0005F4B8   cmpwi cr1,r0,14     and at 0005FA28, ble, else the cursor goes back to 0
//
// Both carry 14, both become 13, and the row is then 0 to 13: the game's ten, Del Sol, Kuhn and B.M.
int Model3::unlock_roster(bool on)
{
	if (set != "fvipers2") return -1;
	struct Site { uint32_t at, sega, ours; };
	static const Site SITES[] = {
		{ 0x0005F3BC, 0x2C8000C0, 0x2C800000 },   // cmpwi r0,192  -> cmpwi r0,0     the gate
		{ 0x0005F444, 0x3800000E, 0x3800000D },   // li r0,14      -> li r0,13       the left wrap
		{ 0x0005F4B8, 0x2C80000E, 0x2C80000D },   // cmpwi r0,14   -> cmpwi r0,13    the right bound
	};
	// Read before each write: this game's program is a gzip the boot stub inflates into RAM, so anything but the
	// instruction that belongs there means this is not the moment, and nothing is written. Each site stands on its
	// own -- the gate may already be in from an earlier call while a bound is not. Off reads and writes the same
	// three sites the other way round, so a patch that is not in is not taken out twice.
	int wrote = 0;
	for (const Site &s : SITES) {
		const uint32_t there = on ? s.sega : s.ours, put = on ? s.ours : s.sega;
		if (ram.size() <= s.at + 4 || word(ram, s.at) != there) continue;
		word(ram, s.at) = put;
		cpu.ram_written(s.at);   // (the recompiler's native code was made from the instruction that was there)
		wrote = 1;
	}
	return wrote;
}

// ---- Fighting Vipers 2's SURVIVAL and RANDOM ----
//
// The cabinet keeps both behind the operator's buttons, and neither is a manoeuvre to reproduce: each is a
// byte the game itself reads. The addresses are Revision A's, measured for this port's console way; what is
// above each one in model3.h is what was measured, not what was assumed.
//
// A byte of guest address a is ram[a ^ 3]: RAM here keeps the PowerPC's 32-bit words in the host's order.
namespace {
constexpr uint32_t FV2_TASK_ENTRY1 = 0x00700000;      // 0 in the attract loop, 5 on the select, 0x2A in a fight
constexpr uint32_t FV2_SURVIVAL_FLAG = 0x0058145F;
constexpr uint32_t FV2_SURVIVAL_SETTING = 0x00120550;   // (the operator setting beside it: the flag alone does nothing)
constexpr uint32_t FV2_RANDOM_CHARA = 0x0058145E;
constexpr uint32_t FV2_BONUS_STAGE = 0x00581461;     // MODE_INFO's BONUS_STG_FLAG, beside the two above
constexpr uint32_t FV2_MODE_MAIN = 0x0058FD98;       // the mode, and the sub-state beside it (Sega's MODE_MAIN / MODE_SUB)
constexpr uint32_t FV2_MODE_SUB = 0x0058FD99;
}

// A player's *1P COLOR on the select screen: byte 185 of his fighter record (docs/fv2-colours.md). The record
// is there from the moment the screen opens, and the byte is what UP and DOWN toggle -- both of them, the
// screen having nothing above or below the row to move to.
int Model3::fv2_colour(int player) const
{
	if (set != "fvipers2" || player < 0 || player > 1) return -1;
	constexpr uint32_t REC[2] = { 0x00700500, 0x00707500 };
	const uint32_t at = REC[player] + 185;
	if (ram.size() <= at) return -1;
	return ram[at ^ 3];
}

// A player's cursor on the select row (docs/fv2-roster.md): the two bytes the roster patch's own bound
// checks are written against, player 1's first.
int Model3::fv2_cursor(int player) const
{
	if (set != "fvipers2" || player < 0 || player > 1) return -1;
	constexpr uint32_t CURSOR = 0x00279668;
	if (ram.size() <= CURSOR + 1) return -1;
	return ram[(CURSOR + uint32_t(player)) ^ 3];
}

int Model3::fv2_task_state() const
{
	if (set != "fvipers2" || ram.size() <= FV2_TASK_ENTRY1 + 3) return -1;
	return int(*reinterpret_cast<const uint32_t *>(&ram[FV2_TASK_ENTRY1]));   // (word() wants it writable)
}

// Honey's other costume: the one players have passed around for twenty years, and which turns out to be in
// Sega's code, in her own cloth handler and nobody else's (docs/fv2-honey.md).
//
// The cloth task switches on the fighter's id through a table of twelve handlers. Honey's is the only one of
// the twelve that reads a counter, and it zeroes three halfwords of her cloth model's working copy when two
// things hold at the instant her model is set up:
//
//   - her LOWER armour has actually been knocked off -- armor_flag & 8, which is the piece gone, not merely
//     damaged past its limit;
//   - the word at 0x00581438 is greater than 29. That word is the current winner's run of straight wins, and
//     it only advances while 0x0058145A is 3 -- a byte built as 1 for player 1 having entered plus 2 for
//     player 2, so three means both players are in the machine. Which is 乱入対戦, the intruded match.
//
// Two instructions decide it, and they are what this turns always-true:
//
//   00057A24   beq +0x40        the armour test   ->  nop
//   00057A30   cmpwi r0,29      the win counter   ->  cmpwi r0,-1
//
// Both were read in this port's own RAM before being used: 0x41820040 and 0x2C80001D, exactly as measured.
// Patching only the first does nothing at all -- that was measured too, 0 pixels different over a whole run,
// because the counter test then fails -- so the pair go in together or not at all.
//
// In RAM, like the roster patch, and for the same reason: this game's program is a gzip the boot stub unpacks,
// so there is nothing to patch until it has. Hence the read before each write.
// It goes both ways, because the costume is chosen per match: the two extra costumes are the cycle's positions
// 2 and 3, and positions 0 and 1 must get Sega's own instructions back.
int Model3::honey_costume(bool on)
{
	if (set != "fvipers2") return -1;
	constexpr uint32_t ARMOUR_TEST = 0x00057A24, WIN_TEST = 0x00057A30;
	constexpr uint32_t AS_SEGA_WROTE_IT[2] = { 0x41820040, 0x2C80001D };   // beq +0x40 ; cmpwi r0,29
	constexpr uint32_t ALWAYS[2] = { 0x60000000, 0x2C80FFFF };             // nop       ; cmpwi r0,-1
	const uint32_t *from = on ? AS_SEGA_WROTE_IT : ALWAYS;
	const uint32_t *to = on ? ALWAYS : AS_SEGA_WROTE_IT;
	// Both words, or neither: the pair is one lever. And the read first, as everywhere here -- anything but
	// what is expected means the program is not unpacked yet, or is not what this thinks it is.
	if (word(ram, ARMOUR_TEST) != from[0] || word(ram, WIN_TEST) != from[1]) return 0;
	word(ram, ARMOUR_TEST) = to[0];
	word(ram, WIN_TEST) = to[1];
	cpu.ram_written(ARMOUR_TEST);   // (the recompiler's native code was made from what was there)
	cpu.ram_written(WIN_TEST);
	return 1;
}

// The two words the select screen's own code writes when a cursor lands on a fighter: the block's entry
// point and the bit that says there is work to do. Measured on the Japan Revision A program, one block a
// player (docs/fv2-honey.md).
void Model3::fv2_redo_fighter_model(int player)
{
	if (set != "fvipers2" || player < 0 || player > 1) return;
	constexpr uint32_t BLOCK[2] = { 0x0070EC00, 0x0070ED00 };
	constexpr uint32_t ENTRY[2] = { 0x0005756C, 0x000575A0 };
	if (ram.size() <= BLOCK[player] + 8) return;
	word(ram, BLOCK[player] + 4) = ENTRY[player];
	word(ram, BLOCK[player]) |= 1;   // (already set while the select screen is up; set anyway, for anywhere else)
}

int Model3::fv2_survival()
{
	if (set != "fvipers2" || ram.size() <= FV2_SURVIVAL_FLAG) return -1;
	return ram[FV2_SURVIVAL_FLAG ^ 3] ? 1 : 0;
}

void Model3::fv2_set_survival(bool on)
{
	if (set != "fvipers2" || ram.size() <= FV2_SURVIVAL_FLAG) return;
	ram[FV2_SURVIVAL_FLAG ^ 3] = on ? 1 : 0;
	ram[FV2_SURVIVAL_SETTING ^ 3] = on ? 1 : 0;
}

int Model3::fv2_random_chara()
{
	if (set != "fvipers2" || ram.size() <= FV2_RANDOM_CHARA) return -1;
	return ram[FV2_RANDOM_CHARA ^ 3] ? 1 : 0;
}

void Model3::fv2_set_random_chara(bool on)
{
	if (set != "fvipers2" || ram.size() <= FV2_RANDOM_CHARA) return;
	ram[FV2_RANDOM_CHARA ^ 3] = on ? 1 : 0;
}

int Model3::fv2_mode() const
{
	if (set != "fvipers2" || ram.size() <= FV2_MODE_SUB) return -1;
	return ram[FV2_MODE_MAIN ^ 3];
}

int Model3::fv2_submode() const
{
	if (set != "fvipers2" || ram.size() <= FV2_MODE_SUB) return -1;
	return ram[FV2_MODE_SUB ^ 3];
}

int Model3::fv2_bonus_stage()
{
	if (set != "fvipers2" || ram.size() <= FV2_BONUS_STAGE) return -1;
	return ram[FV2_BONUS_STAGE ^ 3] ? 1 : 0;
}

// ---- WHAT A TWO-PLAYER VERSUS ASKS OF THE GAME ----
//
// Every address and every rule below was measured on the console build of this port and is carried over
// word for word, with the measurement that established it. Nothing here is new work on the game: the
// versus is a mode the BOARD already has, and this is the handful of its own bytes that have to be read
// or asked for to reach it from a menu (src/host/fv2_versus.h).
constexpr uint32_t FV2_MODE_REQUEST = 0x0058FD9A;      // what the dispatcher at 0x0000ABC4 copies into the mode
constexpr uint32_t FV2_SUBMODE_REQUEST = 0x0058FD9B;   // and the sub-state beside it, copied at 0x0000BA74
constexpr uint8_t FV2_MODE_ADV_DSP = 3, FV2_MODE_GAME_INT = 6, FV2_MODE_OVER_INT = 18;
constexpr uint8_t FV2_ADV_TITLE_INT = 4, FV2_ADV_TITLE_DSP = 5;
constexpr uint8_t FV2_SEL_STAGE_SEL_INT = 2, FV2_SEL_DECIDE_INT = 4;
constexpr uint32_t FV2_ADV_INFO_STATUS = 0x005813F0;   // the match's own word
constexpr uint32_t FV2_MATCH_OVER = 0x40000000;        // set when the match, not merely the round, is decided
constexpr uint32_t FV2_ROUNDS_LOST_1P = 0x00581455;
constexpr uint32_t FV2_ROUNDS_LOST_2P = 0x0058FDBA;
constexpr uint32_t FV2_ROUND_NUMBER = 0x00581456;
constexpr uint32_t FV2_GAME_TIMER = 0x00581404;        // Sega's *GAME TIMER: what FIGHT_DSP counts down
constexpr uint32_t FV2_SCREEN_TIMER = 0x0058FD56;      // what the screen now up has left, SIGNED
constexpr uint16_t FV2_SCREEN_TIMER_EDGE = 8;          // an eighth of a second, and firmly positive
constexpr uint32_t FV2_SCENE_INDEX = 0x0058FD9F;
constexpr uint32_t FV2_TASK_ENTRY2 = 0x00700100;       // player 2's task register, NOT 0x00707000
constexpr uint32_t FV2_PLAYERS = 0x0058145A;           // 1 one player, 3 two: the branch point's first question
constexpr uint32_t FV2_VS_FINISH = 0x00120558;         // the operator setting the end of a versus is read against
// AND TWO MORE OF THE SAME BLOCK, which the Versus page offers before the match starts (fv2_versus.h's caller).
// GAME ASSIGNMENTS live TWICE: a copy in backup RAM at 0x1E000, which the game reads as it boots and which this
// port's Arcade settings page edits -- a change there restarts the game -- and the LIVE copy at 0x00120540 on,
// which is where the game reads them while it runs. These two are the live copy, so a change takes in the match
// about to start and nothing restarts; it also means they go back to the backup RAM's values at the next boot,
// which is the console build's own behaviour and not a shortcoming of these two writes.
//
// Both are the test menu's own fields, with its own names, ranges and steps, carried over from the table this
// port's console build measured for its OPTIONS page (src/host/fv2_options.h on that branch):
//   MATCH COUNT VS   0x0012054D   one byte, 2 to 5, factory 3    -- rounds needed to WIN a match, which is the
//                                                                  field Virtua Fighter 3's test menu spells out
//                                                                  as "Rounds to win"
//   ROUND TIME VS    0x00120556   one byte, 20 to 60 by 5, factory 45   -- the round's length, in seconds
constexpr uint32_t FV2_MATCH_COUNT_VS = 0x0012054D;
constexpr uint32_t FV2_ROUND_TIME_VS = 0x00120556;
constexpr uint32_t FV2_FIGHTER_PTR[2] = { 0x0007511C, 0x00075120 };
constexpr uint32_t FV2_LIFE_IN_BLOCK = 0x66, FV2_LIFE_MAX_IN_BLOCK = 0x68;
constexpr uint16_t FV2_LIFE_PLAUSIBLE = 2000;          // a starting life above this is not a fighter's block

// What the game is ASKING FOR, as opposed to where it is (fv2_submode). The two differ for exactly one tick,
// and that tick is what the stage page and the end-of-match panel are both opened on: SELECT writes
// DECIDE_INT into the request and Sega's dispatcher acts on it the NEXT tick, so reading the request catches
// the game before DECIDE_INT has run even once. Reading MODE_SUB instead catches it a tick late, and letting
// the sub-state run and then pushing the game back runs it twice.
int Model3::fv2_submode_request() const
{
	if (set != "fvipers2" || ram.size() <= FV2_SUBMODE_REQUEST) return -1;
	return ram[FV2_SUBMODE_REQUEST ^ 3];
}

// The sub-state the game is in asked for AGAIN, which is how it is held where it stands: the dispatcher
// copies the request into the sub-state every tick, so the same value every tick is the same sub-state every
// tick. The end-of-match panel holds VIC_DSP this way, so that NEXT_PROG_SEL -- which is what sends a decided
// versus to GAME OVER, or hands the winner a ladder -- is not left waiting to run the instant the panel goes.
void Model3::fv2_hold_submode()
{
	if (set != "fvipers2" || ram.size() <= FV2_SUBMODE_REQUEST) return;
	const int sub = fv2_submode();
	if (sub < 0 || sub > 255) return;
	ram[FV2_SUBMODE_REQUEST ^ 3] = uint8_t(sub);
}

// The sub-state cut short: the next one asked for, and the mode left alone.
void Model3::fv2_cut_submode()
{
	if (set != "fvipers2" || ram.size() <= FV2_SUBMODE_REQUEST) return;
	const int sub = fv2_submode();
	if (sub < 0 || sub >= 255) return;
	ram[FV2_SUBMODE_REQUEST ^ 3] = uint8_t(sub + 1);
}

// The game's own title screen is up: the mode is ADV and the sub-state one of its two title states. This is
// NOT the question fv2_in_attract answers -- "is a game running" is true of the warning, the SEGA logo, the
// title, the presentations, the demos and the rankings alike.
bool Model3::fv2_on_title() const
{
	const int mode = fv2_mode();
	if (mode != 2 && mode != 3) return false;
	const int sub = fv2_submode();
	return sub == FV2_ADV_TITLE_INT || sub == FV2_ADV_TITLE_DSP;
}

// THE GAME PARKED IN ITS OWN EMPTY STAGE_SEL, which is the opposite of a freeze.
//
// SEL's six sub-states are SEL_START, SELECT, STAGE_SEL_INT, STAGE_SEL_DSP, DECIDE_INT, DECIDE_DSP, and the
// arcade board jumps straight from SELECT to DECIDE_INT: its two STAGE_SEL states are an EMPTY STUB, one `b`
// to the dispatcher's exit (0x0000BBB4 -> 0x0000BAD8). Sega kept the name and emptied the body, so there is a
// sub-state that runs the game's whole frame -- its drawing tasks, its music, its sound board -- and advances
// nothing. The stage page is drawn over the game parked there, so the select screen the game made goes on
// being drawn and the board goes on playing: frozen instead, that screen drained away, the two fighters
// vanished and nothing could be heard moving through the page.
//
// HELD, the request renewed every frame, which is the one case where holding a request is right: the point is
// that the game never walks on.
//
// AND IT REFUSES ANYWHERE BUT THE SELECT SCREEN. The sub-state is read against the MODE it belongs to: 2
// means Sega's empty STAGE_SEL_INT in SEL, and ROUND_MASK_DSP in GAME. Parking a fight would write
// ROUND_MASK_DSP into it, every frame.
bool Model3::fv2_park_stage_select()
{
	if (set != "fvipers2" || ram.size() <= FV2_SUBMODE_REQUEST) return false;
	const int mode = fv2_mode();
	if (mode != 4 && mode != 5) return false;   // SEL_INT, SEL_DSP: nowhere else does 2 mean STAGE_SEL
	ram[FV2_SUBMODE_REQUEST ^ 3] = FV2_SEL_STAGE_SEL_INT;
	return true;
}

// And let go: DECIDE_INT asked for, once, which is the sub-state SELECT would have gone to on its own.
bool Model3::fv2_leave_stage_select()
{
	if (set != "fvipers2" || ram.size() <= FV2_SUBMODE_REQUEST) return false;
	const int mode = fv2_mode();
	if (mode != 4 && mode != 5) return false;
	ram[FV2_SUBMODE_REQUEST ^ 3] = FV2_SEL_DECIDE_INT;
	return true;
}

// WHICH ARENA THE FIGHT HAPPENS IN, and it is the DRAW'S OWN TABLE, not a patch on its code.
//
// 0x0007111C is the stage draw, twenty instructions long: a random number, modulo 27, then the first FREE
// entry of a 27-byte table at 0x004836FF from there on, the entry taken being marked used. 0x00071098 builds
// that table and decides which arenas may come up at all -- read back out of three RAM dumps, it is
// 01 00 00 00 01 00 00 00 00 00 00 00 00 then fourteen 01: index 0 locked, 1 to 3 free, 4 conditional, 5 to
// 12 free, 13 to 26 locked.
//
// SO THE LEVER IS THE TABLE: leave one index free and the draw returns it, whatever the random number was. A
// write to DATA, which matters here more than it does in an emulator -- this is a RECOMPILATION, and one word
// of CODE disagreeing with what the native code was made from turns the recompiler off and drops the PowerPC
// to the interpreter (measured: 48.8 frames a second against 57.5). The first version of this patched the
// draw's first two instructions to `li r3,<scene> ; blr` and paid exactly that.
//
// HELD, because the draw marks its own choice used as it takes it. Returns 1 when it had to write, 0 when it
// did not, -1 for a game this isn't known for or a scene the draw cannot return.
int Model3::fv2_hold_stage(int scene)
{
	if (set != "fvipers2" || scene < 1 || scene > 26) return -1;
	if (ram.size() <= FV2_STAGE_DRAW + FV2_STAGE_DRAW_N) return -1;
	if (!fv2_draw_held) {
		for (int i = 0; i < FV2_STAGE_DRAW_N; i++) fv2_draw_was[i] = ram[(FV2_STAGE_DRAW + uint32_t(i)) ^ 3];
		fv2_draw_held = true;
	}
	int wrote = 0;
	for (int i = 0; i < FV2_STAGE_DRAW_N; i++) {
		uint8_t &b = ram[(FV2_STAGE_DRAW + uint32_t(i)) ^ 3];
		const uint8_t want = uint8_t(i == scene ? 0 : 1);
		if (b != want) { b = want; wrote = 1; }
	}
	return wrote;
}

// The draw given back exactly as it was found. Not rebuilt from the rule above: index 4 carries a condition
// this port does not evaluate, and a table written from a rule we only half know is a table that is wrong the
// day the rule matters.
int Model3::fv2_release_stage()
{
	if (set != "fvipers2" || !fv2_draw_held) return 0;
	if (ram.size() <= FV2_STAGE_DRAW + FV2_STAGE_DRAW_N) return -1;
	for (int i = 0; i < FV2_STAGE_DRAW_N; i++)
		ram[(FV2_STAGE_DRAW + uint32_t(i)) ^ 3] = fv2_draw_was[i];
	fv2_draw_held = false;
	return 1;
}

// How many are playing: the game's own byte, 1 for one player and 3 for two. It is the first thing the
// match's branch point asks (0x0000DB60), and 0x0000C2EC picks the round's length on it. Measured 3 through a
// versus and through a training.
int Model3::fv2_players() const
{
	if (set != "fvipers2" || ram.size() <= FV2_PLAYERS) return -1;
	return ram[FV2_PLAYERS ^ 3];
}

// VS FINISH, the operator setting a decided two-player match is read against, in the WORKING copy -- the one
// the running game reads. What the player chose in the cabinet's own settings is kept in the backup and is not
// touched here, so what is lent for the length of a way out is given back without changing anything of theirs.
int Model3::fv2_vs_finish() const
{
	if (set != "fvipers2" || ram.size() <= FV2_VS_FINISH) return -1;
	return ram[FV2_VS_FINISH ^ 3];
}

bool Model3::fv2_set_vs_finish(int v)
{
	if (set != "fvipers2" || ram.size() <= FV2_VS_FINISH) return false;
	ram[FV2_VS_FINISH ^ 3] = uint8_t(v);
	return true;
}

// Its two neighbours in the same block (the constants above carry what was measured about each). Read first and
// written as they are: a byte the game reads when the match begins, nothing more. -1 where there is no such
// byte to read, which is every program but this one.
int Model3::fv2_rounds_vs() const
{
	if (set != "fvipers2" || ram.size() <= FV2_MATCH_COUNT_VS) return -1;
	return ram[FV2_MATCH_COUNT_VS ^ 3];
}

bool Model3::fv2_set_rounds_vs(int v)
{
	if (set != "fvipers2" || ram.size() <= FV2_MATCH_COUNT_VS) return false;
	ram[FV2_MATCH_COUNT_VS ^ 3] = uint8_t(v);
	return true;
}

int Model3::fv2_round_time_vs() const
{
	if (set != "fvipers2" || ram.size() <= FV2_ROUND_TIME_VS) return -1;
	return ram[FV2_ROUND_TIME_VS ^ 3];
}

bool Model3::fv2_set_round_time_vs(int v)
{
	if (set != "fvipers2" || ram.size() <= FV2_ROUND_TIME_VS) return false;
	ram[FV2_ROUND_TIME_VS ^ 3] = uint8_t(v);
	return true;
}

// THE MATCH, AND NOT MERELY THE ROUND, IS DECIDED: one bit of the match's own word.
//
// fv2_game_NEXT_PROG_SEL (0x0000DB48, game sub-state 19) is the branch point of the whole match flow:
//     if (!(ADV_INFO_STATUS & 0x40000000)) { ++ROUND_NO; S-MODE_REQUEST = 6; return; }   /* another round */
// so that bit is the difference between another round of this match and this match being over.
bool Model3::fv2_match_over() const
{
	if (set != "fvipers2" || ram.size() <= FV2_ADV_INFO_STATUS + 4) return false;
	uint32_t v = 0;
	for (int i = 0; i < 4; i++) v = v << 8 | ram[(FV2_ADV_INFO_STATUS + uint32_t(i)) ^ 3];
	return (v & FV2_MATCH_OVER) != 0;
}

// The same bit put back, every frame, until the game leaves GAME. The bit does not keep: between the round's
// end and the branch point the game writes ADV_INFO_STATUS itself, and what it writes there is its own count
// of the rounds won, which says the match is not decided. Measured on the console build: the match was asked
// to end at frame 3000 and the game fought another round at 3010 and four more after it.
void Model3::fv2_hold_match_over()
{
	if (set != "fvipers2" || ram.size() <= FV2_ADV_INFO_STATUS + 4) return;
	uint8_t &high = ram[FV2_ADV_INFO_STATUS ^ 3];
	high = uint8_t(high | (FV2_MATCH_OVER >> 24));
}

// ---- A MATCH ENDED BY THE GAME'S OWN BRANCH POINTS, not by making a player lose ----
//
// Two words, and both are conditions the game's own handlers test every frame:
//
//   fv2_game_FIGHT_DSP (0x0000C5C4, sub-state 12) runs the round, and its exit is one line --
//       if (... || --GAME_TIMER > 0) { return; }   ADV_INFO_STATUS |= 0x80000000;   S-MODE_REQUEST++;
//   so GAME_TIMER at 1 means the next frame takes it to 0, the handler marks the time-out itself and asks for
//   JUDGE. Measured: 0x00581404 reads 5 4 3 2 1 and on the frame it reaches 0 the sub-state becomes JUDGE and
//   ADV_INFO_STATUS becomes 0x80000000, all three together.
//
//   and NEXT_PROG_SEL's bit above is what makes that the end of the MATCH rather than of the round.
//
// Nobody's life is touched, and every value written is one the game reads on purpose. -1 for a moment that is
// not a round being fought: the two words mean nothing anywhere else, which is why the end-of-match panel's
// own way out does not call this at all.
int Model3::fv2_end_match()
{
	if (set != "fvipers2" || ram.size() <= FV2_ADV_INFO_STATUS + 4) return -1;
	const int mode = fv2_mode(), sub = fv2_submode();
	if ((mode != 6 && mode != 7) || (sub != 11 && sub != 12)) return -1;
	uint8_t &high = ram[FV2_ADV_INFO_STATUS ^ 3];        // the bit is in the top byte; the rest is left alone
	high = uint8_t(high | (FV2_MATCH_OVER >> 24));
	for (int i = 0; i < 4; i++) ram[(FV2_GAME_TIMER + uint32_t(i)) ^ 3] = i == 3 ? 1 : 0;
	return 1;
}

// GAME OVER asked for with the game's own write. NEXT_PROG_SEL's two-player road ends in three instructions --
// li r0,18 ; stb r0,[0x0058FD9A] -- and that road is UNREACHABLE once the winner's pose is over: the byte it
// branches on, 0x0058145A, falls from 3 to 1 in the very frame the branch runs, the board's INTRUDE rule
// handing the winner the game. Lending the 3 back was tried and does not take, the handover being in the same
// tick and after the lend. So this writes what that road writes, where it writes it -- Sega's own three
// instructions, not a lever invented here.
void Model3::fv2_request_game_over()
{
	if (set != "fvipers2" || ram.size() <= FV2_MODE_REQUEST) return;
	ram[FV2_MODE_REQUEST ^ 3] = FV2_MODE_OVER_INT;
}

// ---- THE SAME TWO FIGHTERS, THE SAME ARENA, ANOTHER MATCH ----
//
// And no loading at all, because nothing is unloaded: the game already knows how to start a fight again
// without the select screen, since it does it between every round. Its NEXT_PROG_SEL branch asks for one of
// two things -- another round, or the match is over -- so writing the first where the second was, and putting
// the counters back, gives a round 1 with the same characters and the same stage.
//
// Five writes, and only the first commands anything; the other four undo the match just fought. They all go in
// BEFORE the game runs again, the dispatcher copying the request into the mode on the next frame.
//
// AND THE LOSER'S TASK IS HANDED BACK. The versus is not a mode of its own on this board: it is player 2
// stepping into player 1's arcade game, which the game calls INTRUDE, and its rule is that the WINNER INHERITS
// THE GAME -- so the loser's task register falls to 0 and that player's pad goes dead. Whichever of the two
// reads 0 is the one that was stopped; the live one is left exactly as it is, and a task whose fighting value
// was never seen is left alone rather than guessed at. Which is why the caller has to have read them WHILE THE
// ROUND WAS BEING FOUGHT: read here, the loser's is already the stopped one.
//
// Returns 1 when it wrote, 0 when there was nothing it could trust (no plausible life block: not a fight, or
// not the block we think), -1 for a game this isn't known for.
int Model3::fv2_rematch(uint32_t entry1_live, uint32_t entry2_live)
{
	if (set != "fvipers2" || ram.size() <= FV2_TASK_ENTRY2 + 3) return -1;
	auto refill = [&](int who) {
		const int m = fv2_life_max(who);
		return m > 0 && fv2_set_life(who, m);
	};
	const bool one = refill(1), two = refill(2);
	if (!one && !two) return 0;
	ram[FV2_MODE_REQUEST ^ 3] = FV2_MODE_GAME_INT;   // the only one that commands
	ram[FV2_ROUNDS_LOST_1P ^ 3] = 0;
	ram[FV2_ROUNDS_LOST_2P ^ 3] = 0;
	ram[FV2_ROUND_NUMBER ^ 3] = 1;                   // round 1, not the sixth
	// Nothing is written for the characters, the stage or the sub-state: between two rounds the game sets its
	// own sub-state, and imposing one would be a second intervention where one is being tried.
	auto give_back = [&](uint32_t at, uint32_t live) {
		if (!live) return;                                            // never seen fighting: leave it alone
		if (*reinterpret_cast<const uint32_t *>(&ram[at])) return;    // still alive: the winner keeps his
		*reinterpret_cast<uint32_t *>(&ram[at]) = live;
	};
	give_back(FV2_TASK_ENTRY1, entry1_live);
	give_back(FV2_TASK_ENTRY2, entry2_live);
	return 1;
}

// Player 2's task register, the companion of fv2_task_state: watched while a fight runs, so that what has to
// be put back is known before it is stopped (fv2_rematch above).
int Model3::fv2_task2_state() const
{
	if (set != "fvipers2" || ram.size() <= FV2_TASK_ENTRY2 + 3) return -1;
	return int(*reinterpret_cast<const uint32_t *>(&ram[FV2_TASK_ENTRY2]));
}

// How long the screen now up has left, and it is SIGNED: an already expired screen reads negative, and read
// unsigned once it reported 65535 and lengthened a screen that was meant to be shortened.
int Model3::fv2_screen_timer() const
{
	if (set != "fvipers2" || ram.size() <= FV2_SCREEN_TIMER + 1) return 0;
	return int16_t(ram[FV2_SCREEN_TIMER ^ 3] << 8 | ram[(FV2_SCREEN_TIMER + 1) ^ 3]);
}

// The screen now up cut short -- to an eighth of a second, not to zero. Eight is still firmly positive, so the
// game crosses zero itself by the road it takes anyway; at zero it would not. CONTINUE and GAME OVER are MODES
// of their own, not sub-states, so the sub-state request does not reach them and their own counter is the only
// lever there is.
void Model3::fv2_expire_screen()
{
	if (set != "fvipers2" || ram.size() <= FV2_SCREEN_TIMER + 1) return;
	ram[FV2_SCREEN_TIMER ^ 3] = uint8_t(FV2_SCREEN_TIMER_EDGE >> 8);
	ram[(FV2_SCREEN_TIMER + 1) ^ 3] = uint8_t(FV2_SCREEN_TIMER_EDGE);
}

// The arena now loaded. A change in it is what says the end-of-game screens have gone past: entry1's task has
// read zero since the K.O. and does not tell CONTINUE and GAME OVER apart.
int Model3::fv2_scene_index() const
{
	if (set != "fvipers2" || ram.size() <= FV2_SCENE_INDEX) return -1;
	return ram[FV2_SCENE_INDEX ^ 3];
}

// The attract sequencer's own road to the title screen: its list pushed to its last real entry and its timer
// set long expired, written ONCE, and the game then walks off the end of the list itself -- which is the only
// way it takes the branch that installs the title screen off-list. Writing 15 does not work: the sequencer
// tests the entry after the current one against the list's terminator and runs away.
//
// WHY THIS AND NOT THE MODE REQUEST. Asked for the title screen mid-demonstration, the mode machine shows it
// at once -- but the 3D the demonstration was drawing is still there. A title screen the game reached by itself
// carries no 3D at all; one reached by the request carried 397 models over 10 viewports with the full-screen 3D
// still on, and the demonstrations after it then drew from a scene nobody tidied. The request changes what is
// displayed; it does not stop what was drawing.
//
// It works from the attract loop and only there: the sequencer does not run during a game.
void Model3::fv2_attract_to_title()
{
	constexpr uint32_t FV2_ADV_LIST_AT = 0x00580DFC;     // where the attract sequencer is in its list
	constexpr uint32_t FV2_ADV_TIMER = 0x00580DD2;       // how long the entry it is on still has
	constexpr uint8_t FV2_ADV_LAST_ENTRY = 14;
	if (set != "fvipers2" || ram.size() <= FV2_ADV_LIST_AT + 1) return;
	ram[FV2_ADV_LIST_AT ^ 3] = FV2_ADV_LAST_ENTRY;
	ram[FV2_ADV_TIMER ^ 3] = 0x7F;                       // (a halfword, big-endian: 0x7FFF)
	ram[(FV2_ADV_TIMER + 1) ^ 3] = 0xFF;
}

// ---- THE TWO FIGHTERS' LIVES, which is all a rematch needs of them ----
//
// The block the game is using for a fighter, by its own pointer. 0 when there is none -- outside a fight the
// pointer holds whatever it held, so every reader below checks the starting life as well, and that is the
// guard that actually catches a block which is not a fighter's.
uint32_t Model3::fv2_fighter(int player) const
{
	if (set != "fvipers2") return 0;
	const uint32_t at = FV2_FIGHTER_PTR[player == 2 ? 1 : 0];
	if (ram.size() <= at + 3) return 0;
	const uint32_t block = *reinterpret_cast<const uint32_t *>(&ram[at]);
	return block + FV2_LIFE_MAX_IN_BLOCK + 1 < ram.size() ? block : 0;
}

int Model3::fv2_life_max(int player) const
{
	const uint32_t b = fv2_fighter(player);
	if (!b) return -1;
	const uint32_t at = b + FV2_LIFE_MAX_IN_BLOCK;
	const int m = ram[at ^ 3] << 8 | ram[(at + 1) ^ 3];
	return m > 0 && m <= FV2_LIFE_PLAUSIBLE ? m : -1;   // not a fight, or not the block we think
}

int Model3::fv2_life(int player) const
{
	const uint32_t b = fv2_fighter(player);
	if (!b || fv2_life_max(player) < 0) return -1;
	const uint32_t at = b + FV2_LIFE_IN_BLOCK;
	return ram[at ^ 3] << 8 | ram[(at + 1) ^ 3];
}

bool Model3::fv2_set_life(int player, int v)
{
	const int m = fv2_life_max(player);
	if (m < 0) return false;
	const uint32_t at = fv2_fighter(player) + FV2_LIFE_IN_BLOCK;
	const int w = v < 0 ? 0 : v > m ? m : v;
	ram[at ^ 3] = uint8_t(w >> 8);
	ram[(at + 1) ^ 3] = uint8_t(w);
	return true;
}

} // namespace m3
