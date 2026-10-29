// The Model 3 Step 1.0 board. See model3.h.
#include "model3.h"
#include "romset.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

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
	romset::result r = romset::load(zip);
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

void Model3::set_crom_bank(uint8_t v)
{
	// VF3_BANK_LOG=1: each change of the banked CROM's window, for a game that reads from one this port put
	// nothing in (debugging)
	// (asked once: the game changes the window often enough that this is on a warm path too)
	if (static const bool say = getenv("VF3_BANK_LOG") != nullptr; say && v != crom_bank_reg)
		fprintf(stderr, "crom bank: register %02X -> window %u (frame %llu)\n", v, unsigned((~v) & 0xF),
		        (unsigned long long)frame);
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
		for (auto &w : pending_hi) cull_hi[w.first] = w.second;
		for (auto &w : pending_poly) poly_ram[w.first] = w.second;
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

std::vector<uint32_t> Model3::program_image() const
{
	// Where the set carries its program compressed, the image is what the loader unpacked (Fighting Vipers 2: a
	// gzip at the fixed CROM's offset 0, 2,701,982 bytes of program, which the game's own boot stub unpacks into
	// RAM at 0 with zlib's inflate; see romset.cpp). Nothing here is needed for the board to run -- the stub does
	// the work -- only for the recompiler to know what code to build.
	if (!unpacked_program.empty())
		return unpacked_program;
	// Virtua Fighter 3: the boot copies 0xA0000 bytes from the fixed CROM's offset 0x710000 to RAM 0
	// (docs/step1-measure.md)
	std::vector<uint32_t> img(0xA0000 / 4);
	memcpy(img.data(), &crom[0x710000], 0xA0000);
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
	case 0x18: return 0x7F;
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
	time_t now = time(nullptr);
	struct tm t;
#ifdef _WIN32
	localtime_s(&t, &now);
#else
	localtime_r(&now, &t);
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
	for (int i = 1; w >= 4 && h >= 4; i++) {
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
		for (uint32_t i = 0; i < n; i++)
			if (tex_fifo.size() < 0x100000 / 4)
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
		dma_len = data;
		// (tests: VF3_DMA_LOG=1 says where each block of the display list comes from and goes. The culling RAM
		// is filled by THIS, not by the CPU's stores, which is why --watch on 0x8E... reports nothing: a
		// viewport node's words have to be followed back to the main RAM they were copied from.)
		if (static const bool say = getenv("VF3_DMA_LOG") != nullptr; say && dma_len)
			fprintf(stderr, "r3d dma: frame %llu  %08X -> %08X, %u words\n",
					(unsigned long long)frame, dma_src, dma_dst, dma_len);
		while (dma_len) {
			uint32_t v = read32(dma_src);
			write32(dma_dst, (dma_config & 0x80) ? bswap32(v) : v);
			dma_src += 4;
			dma_dst += 4;
			--dma_len;
		}
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
	case 0xFF: {
		const uint32_t v = *reinterpret_cast<const uint32_t *>(&(a < 0xFF800000 ? crom_bank : crom.data())[a & 0x7FFFFF]);
		// VF3_CROM_WATCH=<hex address>: every read of that word in the CROM, with the window it came from.
		// THE ADDRESS IS READ ONCE, and that is not a tidiness: this is the CROM's read path, taken millions
		// of times a frame, and asking the environment on each of them cost 3.9 ms a frame on Fighting Vipers
		// 2 -- the whole of the slowness Frederic reported on 2026-10-24, measured at 6.68 ms against the bare
		// version's 2.78. A probe in a hot path is cached, as this file's VF3_DMA_LOG already is.
		static const uint32_t watch = [] {
			const char *w = getenv("VF3_CROM_WATCH");
			return w ? uint32_t(strtoul(w, nullptr, 16)) : 0u;
		}();
		if (watch)
			if (a == watch)
				fprintf(stderr, "crom read %08X = %08X (window %u, frame %llu)\n", a, bswap32(v),
				        unsigned((~crom_bank_reg) & 0xF), (unsigned long long)frame);
		return v;
	}
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
				// dropped when the host owns the sound, and the interrupt still answered: the game is told
				// its byte was taken, because as far as it is concerned it was (model3.h)
				if (!mute_game_midi) midi.push_back(d);
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
	case 0x8C: cull_lo[(a & 0x3FFFFF) / 4] = bswap32(d); break;
	case 0x8E:
		if (ping_pong != ping_pong_copy) pending_hi.push_back({ (a & 0xFFFFF) / 4, bswap32(d) });
		else cull_hi[(a & 0xFFFFF) / 4] = bswap32(d);
		break;
	case 0x90: r3d_texture_port(a & 0xFF, bswap32(d)); break;
	case 0x94: if (tex_fifo.size() < 0x100000 / 4) tex_fifo.push_back(bswap32(d)); break;
	case 0x98:
		if (ping_pong != ping_pong_copy) pending_poly.push_back({ (a & 0x3FFFFF) / 4, bswap32(d) });
		else poly_ram[(a & 0x3FFFFF) / 4] = bswap32(d);
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

// The FREE PLAY notices, in the fixed CROM the boot copies the program from (the program image's address + 0x710000;
// the CROM holds the PowerPC's 32-bit words, so byte a of the image is crom[(a + 0x710000) ^ 3]):
// - the fight's top line (alternating with PRESS START BUTTON): "     FREE PLAY       " (21 characters) at the
//   image's 0x7CAE0, between "PRESS START BUTTON   " and "CREDIT": spaces;
// - the attract's, bottom left: picture 0x4018 (its number from a table at 0x7CB50), drawn by the call at 0x4E9C0
//   (bl 0x33300, the routine that draws a picture into a text window): a no-op instead.
int Model3::hide_free_play_text()
{
	if (set != "vf3") return 0;   // the addresses below are Virtua Fighter 3's own
	if (!free_play_text.empty()) return int(free_play_text.size());   // (hidden already)
	constexpr size_t CROM_AT = 0x710000;
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
	if (!is(0x7CACA, "PRESS START BUTTON   ") || !is(0x7CAE0, "     FREE PLAY       ") || !is(0x7CAF6, "CREDIT") ||
		word(0x7CB50) != 0x4018 || word(0x4E9C0) != 0x4BFE4941)
		return 0;
	const uint8_t spaces[21] = { ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ' };
	const uint8_t nop[4] = { 0x60, 0x00, 0x00, 0x00 };
	patch(0x7CAE0, spaces, sizeof spaces);
	patch(0x4E9C0, nop, sizeof nop);
	return int(free_play_text.size());
}

void Model3::show_free_play_text()
{
	for (auto &[a, bytes] : free_play_text)
		for (size_t k = 0; k < bytes.size(); k++) crom[(a + 0x710000 + k) ^ 3] = bytes[k];
	free_play_text.clear();
}

// Fighting Vipers 2 carries a wider select screen than it ever shows. Its cursor handler has a second mode where
// the cursor wraps at 14 instead of 10, which puts Del Sol, Kuhn, BM and Dural on the row with the rest, and it is
// gated on both operator buttons -- Service and Test -- being held at once. No player can do that: Test opens the
// test menu. What is tested is not a progress flag but the input port's own state, refreshed every frame, so
// there is nothing to write there. The comparison is what gives:
//
//     0005F3BC   cmpwi r0,192   ->   cmpwi r0,0
//
// and the gate is then open whenever no operator button is held, which is always.
//
// The address is in RAM, not in a ROM. This game's program is a gzip at the fixed CROM's offset 0 that its own
// boot stub inflates into RAM 0 (romset.cpp), so there is nothing to patch until the boot has got that far, and
// writing there before it has would land in a part of the program that is not decompressed yet -- the PowerPC
// then goes somewhere else entirely, some frames later, with nothing to say why. Hence the read first: anything
// but the instruction Sega wrote means this is not the moment.
//
// How often it has to be done was measured rather than assumed (tools/vf3run --watch 5F3BC): over a whole run --
// boot, attract loop, select screen, a fight -- that word is written exactly once, by the inflate, at a boot PC
// in the CROM. Nothing rewrites it afterwards. One write once the boot is over is therefore enough, and the
// caller still calls every frame, which costs one comparison and covers the case this port has not been through.
//
// Known limit, and it is the game's, not the port's: past cursor 10 the names and portraits are wrong or missing.
// The routine that draws them stops there and its table of positions holds eleven entries.
// WHICH ARENA THE GAME FIGHTS IN, and this is the DRAW'S OWN TABLE, not a patch on its code.
//
// 0x0007111C is the stage draw. Read with tools/fv2/disasm_ppc.py the 2026-10-25, out of a RAM dump of a
// running game, it is twenty instructions long and it does this:
//
//     bl 0x000083BC                  a random number
//     r3 = random % 27               by the usual multiply-high trick on 0x2F684BDB
//     r29 = 0x004836FF               a table of 27 bytes
//     loop:  if (table[r3] == 0) { table[r3] = 1; return r3; }      the first one free, and it is taken
//            r3 = (r3 + 1) mod 27                                   with a branchless wrap
//            at most 27 probes, then 0x00071098 builds the table again
//
// and 0x00071098 is what decides which arenas may come up at all: index 0 locked, 1 to 3 free, 4 free only
// if a setting byte and a word of 0x00580E28 agree, 5 to 12 free, 13 to 26 locked. READ BACK OUT OF THREE
// RAM DUMPS, which say exactly that: 01 00 00 00 01 00 00 00 00 00 00 00 00 then fourteen 01. So the free
// indices are the owner's twelve arenas less The Great B.M., which has a condition of its own.
//
// SO THE LEVER IS THE TABLE: leave one index free and the draw returns it, whatever the random number was.
// A write to DATA, which is what makes it the right lever for this port rather than for August's fork.
//
// WHY NOT THE OLD ONE, WHICH IS WHAT THIS PORT DID UNTIL THE 2026-10-25. The first version patched the
// draw's first two instructions to `li r3,<scene> ; blr`, which is August's lever and the right one for
// Supermodel, where it costs nothing. THIS PORT IS A RECOMPILATION: the native code was made from the
// instructions that were in RAM, and one word of RAM disagreeing with them turns the WHOLE recompiler off
// and drops the PowerPC to the interpreter. Measured: a versus whose arena had been chosen ran at 48.8
// frames a second where the same versus with nothing held ran at 57.5, and Frederic saw 60 against under
// 50 in his own window. It was patched around by making the native code again after every write -- a tenth
// of a second each time, hidden under the stage page -- and that patch is gone with the cause.
//
// The scene index and the dozen tables indexed by it agree of themselves, because the game genuinely drew
// this arena: nothing was forced after the fact. Forcing the scene index itself was tried twice on
// Supermodel and rejected both times -- one arena's geometry under another's textures, because two
// consumers read it at two different moments.
//
// Returns 1 when it wrote, 0 when the scene is not one the draw can return, -1 for a game this isn't known
// for. HELD, like the route byte below, because the draw marks its own choice used as it takes it.
int Model3::fv2_hold_stage(int scene)
{
	if (set != "fvipers2" || scene < 1 || scene > 26) return -1;
	if (ram.size() <= FV2_STAGE_DRAW + FV2_STAGE_DRAW_N) return -1;
	if (!fv2_draw_held) {
		for (int i = 0; i < FV2_STAGE_DRAW_N; i++) fv2_draw_was[i] = ram[(FV2_STAGE_DRAW + uint32_t(i)) ^ 3];
		fv2_draw_held = true;
	}
	// Says whether it had to write, so a caller counting how often the game undoes it counts something: the
	// draw marks its own choice used as it takes it, which is the one moment the table stops saying ours.
	int wrote = 0;
	for (int i = 0; i < FV2_STAGE_DRAW_N; i++) {
		uint8_t &b = ram[(FV2_STAGE_DRAW + uint32_t(i)) ^ 3];
		const uint8_t want = uint8_t(i == scene ? 0 : 1);
		if (b != want) { b = want; wrote = 1; }
	}
	return wrote;
}

// The draw given back exactly as it was found. Not rebuilt from the rule above: index 4 carries a condition
// this port does not evaluate, and a table written from a rule we only half know is a table that is wrong
// the day the rule matters.
int Model3::fv2_release_stage()
{
	if (set != "fvipers2" || !fv2_draw_held) return 0;
	if (ram.size() <= FV2_STAGE_DRAW + FV2_STAGE_DRAW_N) return -1;
	for (int i = 0; i < FV2_STAGE_DRAW_N; i++)
		ram[(FV2_STAGE_DRAW + uint32_t(i)) ^ 3] = fv2_draw_was[i];
	fv2_draw_held = false;
	return 1;
}

// TRAINING's arena, the half that actually commands it: the game's own ROUTE TABLE.
//
// The picker above is not what chooses a game's FIRST arena -- patched and held, the game still picked stage
// 9. Watching who writes the scene index gave the real place (vf3run --watch 58FD9C):
//
//     000716B8  stb r0,-609(r9)        r9 = 0x00590000, so 0x0058FD9F, the scene index
//     000716B4  mr  r0,r30             and r30 comes from, a few instructions earlier:
//
//     000715F0  lbz   r9,5213(r11)     [0x0058145D], which picks the route
//     000715F4  lis   r11,0x000B
//     000715F8  addi  r11,r11,16640    the table at 0x000B4100
//     000715FC  addi  r0,r8,-1         the match number, less one
//     00071600  rlwinm r0,r0,3,0,28    times 8
//     00071604  rlwinm r9,r9,1,0,30    the route, times 2
//     00071608  add   r9,r9,r0
//     0007160C  lbzx  r30,r11,r9       <- the arena
//     00071614  lbz   r3,1(r9)         and the byte after it, the opponent
//
// So the arcade's fixed road is a table of eight bytes a match, four (arena, opponent) pairs, the pair chosen
// by 0x0058145D. Read out of a running game it says 09 0A / 01 0B for the first two matches on route 0, which
// is exactly the "0x09 then 0x01 for the second opponent" the match flow measured from the other side.
//
// Measured: the byte at 0x000B4100 set to 3 puts the first fight in MARBLE ARENA. Only the first match is
// touched -- the rest of the route is left as Sega wrote it -- and the byte is put back when the game ends.
int Model3::fv2_hold_training_stage(int scene)
{
	if (set != "fvipers2" || scene < 1 || scene > 12) return -1;
	constexpr uint32_t ROUTE = 0x000B4100;   // route 0, first match: the arena
	if (ram.size() <= ROUTE + 1) return -1;
	uint8_t &here = ram[ROUTE ^ 3];
	if (here == scene) return 0;
	if (!fv2_route_held) { fv2_route_was = here; fv2_route_held = true; }
	here = uint8_t(scene);
	return 1;
}

// Gives the first match's arena back to the game, and only if we took it.
int Model3::fv2_release_training_stage()
{
	if (set != "fvipers2" || !fv2_route_held) return 0;
	constexpr uint32_t ROUTE = 0x000B4100;
	if (ram.size() <= ROUTE + 1) return -1;
	ram[ROUTE ^ 3] = fv2_route_was;
	fv2_route_held = false;
	return 1;
}

int Model3::unlock_roster()
{
	// Measured on the Japan Revision A set. The other Fighting Vipers 2 set is a different build and its own
	// address was never looked for, so it is left alone rather than patched at an address that means nothing
	// there.
	if (set != "fvipers2") return -1;
	constexpr uint32_t SITE = 0x0005F3BC;
	constexpr uint32_t AS_SEGA_WROTE_IT = 0x2C8000C0;   // cmpwi r0,192
	constexpr uint32_t ALWAYS = 0x2C800000;             // cmpwi r0,0
	if (word(ram, SITE) != AS_SEGA_WROTE_IT) return 0;
	word(ram, SITE) = ALWAYS;
	cpu.ram_written(SITE);   // (the recompiler's native code was made from the instruction that was there)
	return 1;
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

// Nothing in this game watches the cursor. The select screen's preview is built by a chain of tasks, and the
// scheduler reads two words per task every frame -- bit 0 of the block for "this one runs", and the word after
// it for where to enter. Those two words are not a trace a routine leaves behind: they ARE the command, read
// sixty times a second. (Found by the decompilation session; the scheduler is at 0x00039174, its indirect call
// at 0x000391A8, and the two primitives that arm and disarm a task are 0x00067B80 and 0x00067B94.)
//
// So a fighter's model is built again by re-arming the cloth task that builds it. One fighter, one block:
//
//     player 1   block 0x0070EC00   entry 0x0005756C
//     player 2   block 0x0070ED00   entry 0x000575A0
//
// A PULSE, and that word matters. Written once, the task runs once and finishes. Written every frame it is
// restarted before it can finish, and the model comes apart on screen -- which is what happened here the first
// time, and is worth leaving written down.
//
// Measured: on the select screen, with the cursor never moving, one pulse makes the preview pick up a costume
// chosen after the cursor landed -- the picture then being byte-identical to the one from a run where the
// costume was set before the cursor ever arrived.
void Model3::fv2_redo_fighter_model(int player)
{
	if (set != "fvipers2" || player < 0 || player > 1) return;
	constexpr uint32_t BLOCK[2] = { 0x0070EC00, 0x0070ED00 };
	constexpr uint32_t ENTRY[2] = { 0x0005756C, 0x000575A0 };
	if (ram.size() <= BLOCK[player] + 8) return;
	word(ram, BLOCK[player] + 4) = ENTRY[player];
	word(ram, BLOCK[player]) |= 1;   // (already set while the select screen is up; set anyway, for anywhere else)
}

// ---- Fighting Vipers 2's console mode ----
//
// Four addresses, all in RAM, all measured here before they were used. The measurements are in
// docs/fv2-console.md; in short, with tools/vf3run --ram-at on tools/scripts/fv2-fight.txt:
//
//   0x00700000  the entry1 task's state, as a word: 0 all through the attract loop, 5 on the select
//               screen, 0x2A in a fight. Read as "0 or not": is a game running.
//   0x0058145F  Survival's flag, 0 on a stock board.
//   0x00120550  the operator setting Survival needs to agree with it, 0 on a stock board.
//   0x00120557  the region, 0 on this Japanese revision (0 Japan, 1 U.S.A., 2 Export, 3 Asia). Writing 2
//               was tried with VF3_POKE and held: the game does not write it back.
//
// A byte of guest address a is ram[a ^ 3]: RAM here keeps the PowerPC's 32-bit words in the host's order.
//
// Everything is gated on the Japan Revision A set, as the roster patch is: the other build was never
// measured, and an address that means nothing there would be worse than doing nothing.
namespace {
constexpr uint32_t FV2_TASK_ENTRY1 = 0x00700000;
constexpr uint32_t FV2_SURVIVAL_FLAG = 0x0058145F;
constexpr uint32_t FV2_RANDOM_CHARA = 0x0058145E;
constexpr uint32_t FV2_SURVIVAL_SETTING = 0x00120550;
constexpr uint32_t FV2_REGION = 0x00120557;
}

// ---- The game's own state machine (docs/fv2-states.md) ----
//
// Four contiguous bytes, and the twenty-four mode numbers Sega named in RAM. Measured on the ROM, on
// Supermodel, and carried here rather than rediscovered: this port had none of it.
//
//   0x0058FD98  MODE_MAIN         the mode
//   0x0058FD99  MODE_SUB          that mode's sub-state
//   0x0058FD9A  MODE_REQUEST      the mode asked for
//   0x0058FD9B  SUBMODE_REQUEST   the sub-state asked for
//
// The attract slice's sub-states, which is the one that matters here:
//
//   0 DONT_RECYCLE_INT  1 DONT_RECYCLE_DSP  2 ADV_SEGA_INT     3 ADV_SEGA_DSP
//   4 FV2_TITLE_INT     5 FV2_TITLE_DSP     6 ADV_RATING_INT   7 ADV_RATING_DSP
//   8 ADV_REPLAY_WAIT   9 ADV_REPLAY_INT   10 ADV_REPLAY_DSP  11 ADV_DEMO_DSP
//
// Twelve, not ten, and starting at the table's index 50 rather than 52 -- the two names that were missing were
// BEFORE the slice, not after, so nothing below them shifts. Both were settled by freezing the game on one
// sub-state and looking: 3 is the SEGA logo, 5 is the title screen.
namespace {
constexpr uint32_t FV2_MODE_MAIN = 0x0058FD98;
constexpr uint32_t FV2_MODE_SUB = 0x0058FD99;
constexpr uint32_t FV2_MODE_REQUEST = 0x0058FD9A;
constexpr uint32_t FV2_SUBMODE_REQUEST = 0x0058FD9B;
constexpr uint8_t FV2_MODE_ADV_INT = 2, FV2_MODE_ADV_DSP = 3;
constexpr uint8_t FV2_ADV_TITLE_INT = 4, FV2_ADV_TITLE_DSP = 5;
constexpr uint8_t FV2_MODE_ENDING_INT = 12;   // the staff roll: see fv2_request_ending below
// GAME OVER, which is where NEXT_PROG_SEL's two-player road sends a decided versus (fv2_request_game_over)
constexpr uint8_t FV2_MODE_OVER_INT = 18;
// SEL's six, by Sega's own names (docs/fv2-states.md): 0 SEL_START, 1 SELECT, 2 STAGE_SEL_INT,
// 3 STAGE_SEL_DSP, 4 DECIDE_INT, 5 DECIDE_DSP. The two in the middle are the empty stub the console's stage
// page parks the game in (fv2_park_stage_select below).
constexpr uint8_t FV2_SEL_STAGE_SEL_INT = 2, FV2_SEL_DECIDE_INT = 4;
constexpr uint32_t FV2_ADV_INFO_STATUS = 0x005813F0;   // the match's own word (docs/fv2-match-flow.md)
constexpr uint32_t FV2_LADDER_DONE = 0x00000008;       // "the ladder is finished": NEXT_PROG_SEL branches to the ending
constexpr uint32_t FV2_MATCH_OVER = 0x40000000;        // set when the match, not merely the round, is decided
// The match's own counters and the two fighters' life, all of them August's, measured one by one in August
// 2026 and written up in SupermodelSrc\Docs\MenuConsole.md section 6 ter. ROUNDS_LOST_1P came out of five
// dumps taken at the head of each round of a five-round match: of eight megabytes, two bytes alone count
// 0 1 2 3 4 and one alone counts 1 2 3 4 5. ROUNDS_LOST_2 lives in the scene block rather than beside it.
constexpr uint32_t FV2_ROUNDS_LOST_1P = 0x00581455;
constexpr uint32_t FV2_ROUND_NUMBER = 0x00581456;
// THE TWO BYTES THE MATCH'S BRANCH POINT COUNTS WITH, read at 0x0000DB60 out of a RAM dump the
// 2026-10-26 (tools/fv2/disasm_ppc.py). HOW MANY ARE PLAYING is already named elsewhere in this file --
// 0x0000C2EC picks the round's length on it -- and the byte beside it is HOW MANY ARE OUT: the branch
// point asks whether the players out are one fewer than the players, which is "all but one", and sends
// the last one on to the next opponent. Anything else and the game is over for everybody.
constexpr uint32_t FV2_PLAYERS = 0x0058145A;
constexpr uint32_t FV2_PLAYERS_OUT = 0x00581457;
constexpr uint32_t FV2_ROUNDS_LOST_2P = 0x0058FDBA;
// THE TWO FIGHTERS, REACHED THE WAY THE GAME REACHES THEM: through its own two pointers, and not by the
// addresses they happen to hold. fv2_game_FIGHT_DSP itself reads `lwz r3,[0x0007511C] ; lwz r3,0(r3)`, and
// ROUND_MASK_INT writes the round's starting health at block+0x68 through the same pair. In every dump so
// far they read 0x00700500 and 0x00707500, which is where August's fixed LIFE_1P and LIFE_2P came from --
// block+0x66 and block+0x68 -- but a fixed address is a copy of an answer and the pointer is the question.
constexpr uint32_t FV2_FIGHTER_PTR[2] = { 0x0007511C, 0x00075120 };
constexpr uint32_t FV2_LIFE_IN_BLOCK = 0x66, FV2_LIFE_MAX_IN_BLOCK = 0x68;
constexpr uint16_t FV2_LIFE_PLAUSIBLE = 2000;          // a starting life above this is not a fighter's block
// The game's OWN "no round clock" switch, and it is not a value to hold: fv2_game_FIGHT_DSP reads
//     lis r3,0x0012 ; lwz r3,-4020(r3) ; rlwinm. r0,r3,31,0,0 ; bne <skip the whole countdown>
// so bit 0 of the word at 0x0011F04C -- Sega's CONFIG_CB_DISP, in the CONFIG block beside *PLAY KUHN --
// makes the handler not decrement the timer at all. Read out of the code the 2026-10-25 with
// tools/fv2/disasm_ppc.py. Holding the timer's value instead would raise the question August had to leave
// open (does the game read this word or write it?); this one is a condition, so there is no question.
constexpr uint32_t FV2_CB_DISP = 0x0011F04C;
// THE ENDING'S OWN TWO BYTES, both August's, measured in August 2026 (MenuConsole.md section 6 septies).
// The staff roll is a LIST OF SEGMENTS and each segment is two halves: a page of credits, then a replay
// of a match the player won, about 225 frames each. ENDING_SEQ_PTR is where the game is in that list --
// it moves only at a segment boundary, except during the twelve frames of ENDING_INT where it runs down
// its list a value a frame -- and ENDING_REPLAY_ON says which half is on screen.
constexpr uint32_t FV2_ENDING_SEQ_PTR = 0x00580E8D;
constexpr uint32_t FV2_ENDING_REPLAY_ON = 0x0070E603;
// THE ENDING KEEPS ITS OWN SUB-STATE, and not in MODE_SUB. Watched through a whole roll, MODE_SUB never
// leaves 11; the seven states Sega names ending_trans_init ... ending_final_mask2 are dispatched by the
// ending's own routine at 0x0007249C, which reads ONE BYTE and jumps through a table of seven:
//
//     0007249C  lbz   r0,-588(r29)      r29 = 0x00590000, so the byte is 0x0058FDB4
//     000724C0  addi  r9,r9,17484       the table, 0x0002444C
//     000724C8  lwzx  r0,r9,r0          handler = table[state]
//     000724D0  blrl                    and it is called
//
// Two of the seven are the transition (0, 1), three are the replay (2, 3, 4) and two are the final mask
// (5, 6) -- the names read out of Sega's own table (docs/fv2-states.md).
constexpr uint32_t FV2_ENDING_SUB = 0x0058FDB4;
// AND ITS OWN CLOCK. ENDING_INT writes 320 into this word (0x0000E21C, li r0,320) and ENDING_DSP ends the
// whole thing when it has run down: 0x0000E2D0 reads it, and if it is not greater than zero it writes 18 --
// GAME OVER -- into MODE_REQUEST. Hold it above zero and the ending stays; let it go and the game leaves by
// its own road, through its own GAME OVER, with nothing to put back.
constexpr uint32_t FV2_ENDING_TIMER = 0x0058FD54;
constexpr uint32_t FV2_TASK_ENTRY2 = 0x00700100;       // player 2's, and NOT 0x00707000, which August measured wrong first
constexpr uint8_t FV2_MODE_GAME_INT = 6;
// LEAVING A MATCH by the game's own road (fv2_end_match below). GAME_TIMER is the word fv2_game_FIGHT_DSP
// decrements every frame and whose reaching zero IS the end of a round; the screen timer is the only lever
// that reaches CONTINUE and GAME OVER, which are modes of their own.
constexpr uint32_t FV2_GAME_TIMER = 0x00581404;    // Sega's *GAME TIMER, read in MODE_INFO
constexpr uint32_t FV2_SCREEN_TIMER = 0x0058FD56;  // how long the screen now up has left, SIGNED: an expired one is negative
constexpr uint32_t FV2_SCENE_INDEX = 0x0058FD9F;
constexpr uint16_t FV2_SCREEN_TIMER_EDGE = 8;      // an eighth of a second: five shortened screens cost six seconds at 64
}

int Model3::fv2_task_state() const
{
	if (set != "fvipers2" || ram.size() <= FV2_TASK_ENTRY1 + 3) return -1;
	return int(*reinterpret_cast<const uint32_t *>(&ram[FV2_TASK_ENTRY1]));
}

int Model3::fv2_survival() const
{
	if (set != "fvipers2") return -1;
	return ram[FV2_SURVIVAL_FLAG ^ 3] ? 1 : 0;
}

void Model3::fv2_set_survival(bool on)
{
	if (set != "fvipers2") return;
	ram[FV2_SURVIVAL_FLAG ^ 3] = on ? 1 : 0;
	ram[FV2_SURVIVAL_SETTING ^ 3] = on ? 1 : 0;
}

// RANDOM: the game's own flag, and it is Sega's own name for it.
//
// Found by reading rather than by playing: the game's debug tables name 0x0058145E `RANDOM_CHARA` on the
// `MODE_INFO` page, right beside `SURVIVAL FLAG` at 0x0058145F (docs/fv2-match-flow.md, section 5). Set,
// the select screen prints RANDOM MODE beside the fighter -- which is exactly what the owner's recorded
// manoeuvre produced in August by HOLDING Start across the fighter's validation.
//
// And that manoeuvre does not reproduce here: held over its own window, by this port or by an input script,
// the game's whole memory at the frame of the choice is byte-identical to a run that held nothing. The flag
// is the lever; the hold was a way of asking the game to set it, and the game does not listen to it in this
// build. Asking for the flag directly is both shorter and measurable.
//
// Unlike Survival's, it does NOT have to be held: written once at frame 2000, RANDOM MODE is still up a
// hundred frames later. It is still written only once a game is running, for Survival's reason -- a flag of
// this block written during the attract loop stopped the game starting at all -- and put back at the end.
int Model3::fv2_random_chara() const
{
	if (set != "fvipers2" || ram.size() <= FV2_RANDOM_CHARA) return -1;
	return ram[FV2_RANDOM_CHARA ^ 3] ? 1 : 0;
}

void Model3::fv2_set_random_chara(bool on)
{
	if (set != "fvipers2" || ram.size() <= FV2_RANDOM_CHARA) return;
	ram[FV2_RANDOM_CHARA ^ 3] = on ? 1 : 0;
}

int Model3::fv2_region() const
{
	if (set != "fvipers2") return -1;
	return ram[FV2_REGION ^ 3];
}

void Model3::fv2_set_region(int r)
{
	if (set != "fvipers2" || r < 0 || r > 3) return;
	ram[FV2_REGION ^ 3] = uint8_t(r);
}

int Model3::fv2_mode() const
{
	if (set != "fvipers2" || ram.size() <= FV2_SUBMODE_REQUEST) return -1;
	return ram[FV2_MODE_MAIN ^ 3];
}

int Model3::fv2_submode() const
{
	if (set != "fvipers2" || ram.size() <= FV2_SUBMODE_REQUEST) return -1;
	return ram[FV2_MODE_SUB ^ 3];
}

bool Model3::fv2_on_title() const
{
	const int mode = fv2_mode();
	if (mode != FV2_MODE_ADV_INT && mode != FV2_MODE_ADV_DSP) return false;
	const int sub = fv2_submode();
	return sub == FV2_ADV_TITLE_INT || sub == FV2_ADV_TITLE_DSP;
}

// ONCE. Not held -- see the header, and the two times holding a request cost a morning.
void Model3::fv2_request_title()
{
	if (set != "fvipers2" || ram.size() <= FV2_SUBMODE_REQUEST) return;
	ram[FV2_MODE_REQUEST ^ 3] = FV2_MODE_ADV_DSP;
	ram[FV2_SUBMODE_REQUEST ^ 3] = FV2_ADV_TITLE_INT;
}

// The sub-state cut short: the next one asked for, and the mode left alone -- the measurement that
// established this wrote the sub-state request and nothing else (model3.h).
void Model3::fv2_cut_submode()
{
	if (set != "fvipers2" || ram.size() <= FV2_SUBMODE_REQUEST) return;
	const int sub = fv2_submode();
	if (sub < 0 || sub >= 255) return;
	ram[FV2_SUBMODE_REQUEST ^ 3] = uint8_t(sub + 1);
}

// THE GAME PARKED IN ITS OWN EMPTY STAGE_SEL -- August's method, and the opposite of a freeze.
//
// SEL's six sub-states are SEL_START, SELECT, STAGE_SEL_INT, STAGE_SEL_DSP, DECIDE_INT, DECIDE_DSP, and the
// arcade board jumps straight from SELECT to DECIDE_INT: its two STAGE_SEL states are an EMPTY STUB, one `b`
// to the dispatcher's exit (0x0000BBB4 -> 0x0000BAD8). Sega kept the name and emptied the body, so there is a
// sub-state that runs the game's whole frame -- its drawing tasks, its music, its sound board -- and advances
// nothing. Holding the game there is not a pause: it is the game itself, idling. "So the frozen screen is
// not a pause: it is the game itself running slow" (FV2 - Choix du decor.cmd, August 2026).
//
// HELD, and that is NOT the mistake fv2_request_title warns about. A request written once is read once and
// the game walks on; here the point is that it never walks on, so the request is renewed every frame for as
// long as the page is up. Sega's dispatcher at 0x0000BA74 copies SUBMODE_REQUEST into MODE_SUB every tick, so
// a request of 2 every tick is a sub-state of 2 every tick.
//
// Why this and not the host's freeze it replaces: a frozen game draws nothing new, so the select screen it
// had made drained away -- the two fighters vanished, the music stopped and nothing could be heard moving
// through the carousel, all three reported by Frédéric the 2026-10-25. Parked, the game goes on drawing the
// screen it was on and the sound board goes on playing, because the game never stopped.
//
// AND IT REFUSES ANYWHERE BUT THE SELECT SCREEN, which is the fault Frederic found the 2026-10-25 under
// "change stage does not work". The sub-state is read against the MODE it belongs to: 2 means Sega's
// empty STAGE_SEL_INT in SEL, and ROUND_MASK_DSP in GAME. The carousel opened from the pause menu parked
// the game exactly as it does on the select screen -- and so wrote ROUND_MASK_DSP into a fight, every
// frame. Mid-match the page freezes the game instead (main.cpp), as the pause menu it was opened from does.
bool Model3::fv2_park_stage_select()
{
	if (set != "fvipers2" || ram.size() <= FV2_SUBMODE_REQUEST) return false;
	const int mode = fv2_mode();
	if (mode != 4 && mode != 5) return false;   // SEL_INT, SEL_DSP: nowhere else does 2 mean STAGE_SEL
	ram[FV2_SUBMODE_REQUEST ^ 3] = FV2_SEL_STAGE_SEL_INT;
	return true;
}

// And let go: DECIDE_INT asked for, once, which is the sub-state SELECT would have gone to on its own. The
// game carries on from exactly where it was held.
bool Model3::fv2_leave_stage_select()
{
	if (set != "fvipers2" || ram.size() <= FV2_SUBMODE_REQUEST) return false;
	const int mode = fv2_mode();
	if (mode != 4 && mode != 5) return false;   // and 4 is DECIDE_INT only in SEL: see the park above
	ram[FV2_SUBMODE_REQUEST ^ 3] = FV2_SEL_DECIDE_INT;
	return true;
}

// GAME OVER, asked for with the game's own write. NEXT_PROG_SEL's two-player road ends in three
// instructions -- li r0,18 ; stb r0,[0x0058FD9A] -- and that road is UNREACHABLE once the winner's
// pose is over: the byte it branches on, 0x0058145A, falls from 3 to 1 in the very frame the branch
// runs, the board's INTRUDE rule handing the winner the game. Measured the 2026-10-28 with
// VF3_RAM_WATCH, and lending the 3 back was tried and does not take -- the handover is in the same
// tick and after the lend. So the port writes what that road writes, where it writes it. The console's
// end-of-match panel uses it, and nothing of GAME OVER is seen or heard: the port owns the screen and
// the sound while it is walked through (console_state.h, GOING and MENU_IN).
void Model3::fv2_request_game_over()
{
	if (set != "fvipers2" || ram.size() <= FV2_MODE_REQUEST) return;
	ram[FV2_MODE_REQUEST ^ 3] = FV2_MODE_OVER_INT;
}

void Model3::fv2_hold_submode()
{
	if (set != "fvipers2" || ram.size() <= FV2_SUBMODE_REQUEST) return;
	const int sub = fv2_submode();
	if (sub < 0 || sub > 255) return;
	ram[FV2_SUBMODE_REQUEST ^ 3] = uint8_t(sub);
}

// What the game is ASKING FOR, as opposed to where it is (fv2_submode). The two differ for exactly one tick,
// and that tick is the whole of the opening above: SELECT writes DECIDE_INT into the request and the
// dispatcher acts on it next tick, so a page that reads the request catches the game before DECIDE_INT has
// run even once. Reading MODE_SUB instead would catch it one tick late, and letting DECIDE_INT run and then
// pushing the game back would run it twice.
int Model3::fv2_submode_request() const
{
	if (set != "fvipers2" || ram.size() <= FV2_SUBMODE_REQUEST) return -1;
	return ram[FV2_SUBMODE_REQUEST ^ 3];
}

// THE GAME'S OWN ENDING, asked for the same way. ENDING is mode 12, and MODE_SUB means nothing to it -- the
// state machine's own rule for every mode but GAME, SEL and ADV (docs/fv2-states.md) -- so the mode alone is
// the whole request, and the sub-state is left where the title screen had it.
//
// Measured before it was wired, from the title screen, which is the only place the console's page can be
// opened from: poked at frame 1250, the game enters ENDING_INT at 1251 and ENDING_DSP at 1263, rolls its
// credits over the viper -- "Director HIROSHI KATAOKA" at 1900, "Presented by SEGA ENTERPRISES, LTD." at
// 5000 -- and LEAVES ON ITS OWN at about 7370, through the attract loop, back to its title screen at 7566.
// So it needs no way out built for it: it borrows the game's.
void Model3::fv2_request_ending()
{
	if (set != "fvipers2" || ram.size() <= FV2_MODE_REQUEST) return;
	ram[FV2_MODE_REQUEST ^ 3] = FV2_MODE_ENDING_INT;
}

// THE GAME'S OWN ROAD TO ITS ENDING, which is not the same thing as asking for the mode.
//
// fv2_game_NEXT_PROG_SEL (0x0000DB48) is the branch point run at the end of EVERY round: it decides another
// round, another opponent, a continue -- or the ending, if ADV_INFO_STATUS carries bit 3, "the ladder is
// finished". The summer's notes had that bit as read in the code and never seen set. Setting it is enough,
// and the game then does everything its own road does besides the mode: FUN_0000DD70, seven tasks stopped,
// the fog cleared, the screen counter to 0x140. The ending comes out whole, replays and all.
//
// WHEN IT IS SET IS THE WHOLE OF IT, and it was measured a frame at a time against one recorded fight whose
// branch point falls at frame 7178:
//
//   set at 7178            too late: the branch has read it, and the game goes to CONTINUE
//   set at 7177 or 7176    the ending, whole
//   set once at 6922       the ending, whole -- the bit keeps, 256 frames, with no need to hold it
//   set at 3000, mid-fight the ending at 4740 instead, at the END OF THE FIRST ROUND, and BARE: that round
//                          boundary is a NEXT_PROG_SEL too, and there is nothing recorded to replay yet
//   set at 1250, on the title screen, before the match even starts: the same, bare, at 4740
//
// So the bit must go in once the MATCH is over, not the round. The game says when: 0x40000000 in the same
// word, which is what its own "another round?" test reads. The game rewrites the whole word every frame from
// 0x000122C4, keeping its value, so a write placed before a frame stands for that frame's reads.
bool Model3::fv2_match_over() const
{
	if (set != "fvipers2" || ram.size() <= FV2_ADV_INFO_STATUS + 4) return false;
	uint32_t v = 0;
	for (int i = 0; i < 4; i++) v = v << 8 | ram[(FV2_ADV_INFO_STATUS + uint32_t(i)) ^ 3];
	return (v & FV2_MATCH_OVER) != 0;
}

// THE SAME TWO FIGHTERS, THE SAME ARENA, ANOTHER MATCH -- and no loading at all, because nothing is
// unloaded. August's REMATCH, done the 2026-08-19 by a road his own notes had not expected:
//
//   "The game already knows how to start a fight again without going through the select screen: it does it
//    between every round." Its NEXT_PROG_SEL branch asks for one of two things -- another round, or the
//    match is over. Writing the first where the second was, and putting the counters back, gives a round 1
//    with the same characters and the same stage.
//
// Five writes, and only the first commands anything; the other four undo the match that has just been
// fought. They all go in BEFORE the game runs again -- the dispatcher copies the request into the mode on
// the next frame, so thawing first and writing after would be one frame too late.
//
// The loser's task is handed back as well. The versus is not a mode of its own on this board: it is player
// 2 stepping into player 1's arcade game, which the game calls INTRUDE, and its rule is that the winner
// inherits the game -- so the loser's task register falls to 0 and that player's pad goes dead. Whichever
// of the two reads 0 is the one that was stopped; the live one is left exactly as it is, and a task whose
// fighting value was never seen is left alone rather than guessed at.
//
// Returns 1 when it wrote, 0 when there was nothing it could trust (no plausible life block: not a fight,
// or not the block we think), -1 for a game this isn't known for.
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
	// Nothing is written for the characters, the stage, or the sub-state: between two rounds the game sets
	// its own sub-state, and imposing one would be a second intervention where one is being tried.
	auto give_back = [&](uint32_t at, uint32_t live) {
		if (!live) return;                                         // never seen fighting: leave it alone
		if (*reinterpret_cast<const uint32_t *>(&ram[at])) return;  // still alive: the winner keeps his
		*reinterpret_cast<uint32_t *>(&ram[at]) = live;
	};
	give_back(FV2_TASK_ENTRY1, entry1_live);
	give_back(FV2_TASK_ENTRY2, entry2_live);
	return 1;
}

// ---- LEAVING A MATCH, by the game's own branch point ----
//
// NOT by August's road, and the difference is the whole point. His fork had to end a match from outside,
// with no decompilation to lean on: he armed the last round, put the round clock at its edge and held
// player 1's life at zero until the attract loop came back -- a match ended by making a player lose.
// THIS PORT HAS READ THE CODE (docs/fv2-match-flow.md), so it can ask the game instead of pushing it.
//
// Two words, and both of them are conditions the game's own handlers test every frame:
//
//   fv2_game_FIGHT_DSP (0x0000C5C4, game sub mode 12) runs the round, and its exit is one line:
//       if (... || --GAME_TIMER > 0) { return; }   ADV_INFO_STATUS |= 0x80000000;   S-MODE_REQUEST++;
//   so GAME_TIMER at 1 means the next frame decrements it to 0, the handler marks the time-out itself and
//   asks for JUDGE. Measured in the flow run: 0x00581404 reads 5 4 3 2 1 and on the frame it reaches 0 the
//   sub mode becomes JUDGE and ADV_INFO_STATUS becomes 0x80000000, all three together.
//
//   fv2_game_NEXT_PROG_SEL (0x0000DB48, game sub mode 19) is the branch point of the whole match flow:
//       if (!(ADV_INFO_STATUS & 0x40000000)) { ++ROUND_NO; S-MODE_REQUEST = 6; return; }  /* another round */
//   so that bit is the difference between "another round of this match" and "this match is decided". It is
//   the bit this port already names FV2_MATCH_OVER and already READS in fv2_match_over.
//
// So: the round ends because the game's own timer ran out, and the match ends because the game's own
// branch point was told the match was decided. Nobody's life is touched, nothing is held frame after
// frame, and every value written is one the game reads on purpose.
//
// Returns 1 when it wrote, -1 for a game this isn't known for or a moment that is not a round being
// fought -- GAME's FIGHT_INT or FIGHT_DSP, because the two words mean nothing anywhere else.
// AND THE SAME BIT HELD, nothing else. Measured on a hidden training run the 2026-10-25: MAIN MENU was
// chosen at frame 3000, fv2_end_match wrote its two words and said so -- and the game fought ANOTHER
// ROUND at 3010 and four more after it. The bit does not keep: between the round's end and the branch
// point the game writes ADV_INFO_STATUS itself, and what it writes there is its own count of the rounds
// won, which says the match is not decided. So it is put back every frame until the game leaves GAME,
// which is the one place the branch point reads it. The timer is NOT touched here: writing 1 into it
// again would end each new round in turn instead of ending the match.
void Model3::fv2_hold_match_over()
{
	if (set != "fvipers2" || ram.size() <= FV2_ADV_INFO_STATUS + 4) return;
	uint8_t &high = ram[FV2_ADV_INFO_STATUS ^ 3];
	high = uint8_t(high | (FV2_MATCH_OVER >> 24));
}

int Model3::fv2_end_match()
{
	if (set != "fvipers2" || ram.size() <= FV2_ADV_INFO_STATUS + 4) return -1;
	const int mode = fv2_mode(), sub = fv2_submode();
	if ((mode != 6 && mode != 7) || (sub != 11 && sub != 12)) return -1;
	uint8_t &high = ram[FV2_ADV_INFO_STATUS ^ 3];        // the bit is in the top byte; the rest is left alone
	high = uint8_t(high | (FV2_MATCH_OVER >> 24));
	// AND WHAT IT DOES NOT DO, measured the 2026-10-27 after four runs of trying: it does not touch the
	// branch point's own counts. NEXT_PROG_SEL (0x0000DB48) asks whether the players out are one fewer
	// than the players playing and hands the last one the ladder, and writing those two bytes -- the
	// count of players out set to the count of players, a point of life so the time-out has a winner --
	// changed nothing a run could see. The door out of a two-player match is elsewhere and is the game's
	// own: VS FINISH, read at 0x0000DBD0 when the match ends (model3.h, console_play.h).
	for (int i = 0; i < 4; i++) ram[(FV2_GAME_TIMER + uint32_t(i)) ^ 3] = i == 3 ? 1 : 0;
	return 1;
}

// What the two words read, so a run that did nothing can say so rather than look like a normal frame.
int Model3::fv2_ending_seq() const
{
	if (set != "fvipers2" || ram.size() <= FV2_ENDING_SEQ_PTR) return -1;
	return ram[FV2_ENDING_SEQ_PTR ^ 3];
}

// The ending's own clock, in frames, and holding it is how the port keeps the roll on screen for as long as
// its own credits take (fv2_roll.h). -1 when this is not that game.
int Model3::fv2_ending_timer() const
{
	if (set != "fvipers2" || ram.size() <= FV2_ENDING_TIMER + 4) return -1;
	return int(uint32_t(ram[FV2_ENDING_TIMER ^ 3]) << 24 | uint32_t(ram[(FV2_ENDING_TIMER + 1) ^ 3]) << 16
			 | uint32_t(ram[(FV2_ENDING_TIMER + 2) ^ 3]) << 8 | uint32_t(ram[(FV2_ENDING_TIMER + 3) ^ 3]));
}

void Model3::fv2_set_ending_timer(int v)
{
	if (set != "fvipers2" || ram.size() <= FV2_ENDING_TIMER + 4) return;
	const uint32_t w = uint32_t(v);
	ram[FV2_ENDING_TIMER ^ 3] = uint8_t(w >> 24);
	ram[(FV2_ENDING_TIMER + 1) ^ 3] = uint8_t(w >> 16);
	ram[(FV2_ENDING_TIMER + 2) ^ 3] = uint8_t(w >> 8);
	ram[(FV2_ENDING_TIMER + 3) ^ 3] = uint8_t(w);
}

// Which of the ending's seven is running, -1 when this is not that game. See FV2_ENDING_SUB above.
int Model3::fv2_ending_sub() const
{
	if (set != "fvipers2" || ram.size() <= FV2_ENDING_SUB) return -1;
	return ram[FV2_ENDING_SUB ^ 3];
}

// ASKED TO MOVE ON, which is what the training mode does to the round's announcement one layer up: the
// state is stepped and the ending's own dispatcher runs the next handler on its next pass. Returns what it
// wrote, or -1 if it wrote nothing.
int Model3::fv2_cut_ending_sub()
{
	const int su = fv2_ending_sub();
	if (su < 0 || su >= 6) return -1;
	ram[FV2_ENDING_SUB ^ 3] = uint8_t(su + 1);
	return su + 1;
}

int Model3::fv2_ending_replay() const
{
	if (set != "fvipers2" || ram.size() <= FV2_ENDING_REPLAY_ON) return -1;
	return ram[FV2_ENDING_REPLAY_ON ^ 3] ? 1 : 0;
}

int Model3::fv2_game_timer() const
{
	if (set != "fvipers2" || ram.size() <= FV2_GAME_TIMER + 4) return -1;
	int v = 0;
	for (int i = 0; i < 4; i++) v = v << 8 | ram[(FV2_GAME_TIMER + uint32_t(i)) ^ 3];
	return v;
}

int Model3::fv2_scene_index() const
{
	if (set != "fvipers2" || ram.size() <= FV2_SCENE_INDEX) return -1;
	return ram[FV2_SCENE_INDEX ^ 3];
}

// How long the screen now up has left, and it is SIGNED: an already expired screen reads negative, and a
// log that read it unsigned once reported 65535 and made the host lengthen a screen it meant to shorten.
int Model3::fv2_screen_timer() const
{
	if (set != "fvipers2" || ram.size() <= FV2_SCREEN_TIMER + 1) return 0;
	return int16_t(ram[FV2_SCREEN_TIMER ^ 3] << 8 | ram[(FV2_SCREEN_TIMER + 1) ^ 3]);
}

// The screen now up cut short -- to an eighth of a second, not to zero. Eight is still firmly positive, so
// the game crosses zero itself by the road it takes anyway; at zero it would not. This one IS August's, and
// it stays his: CONTINUE and GAME OVER are modes of their own, not sub-states, so the sub-state request
// does not reach them and their own counter is the only lever there is. If ever an end screen skips a
// transition, this is the number to raise and nothing else.
void Model3::fv2_expire_screen()
{
	if (set != "fvipers2" || ram.size() <= FV2_SCREEN_TIMER + 1) return;
	ram[FV2_SCREEN_TIMER ^ 3] = uint8_t(FV2_SCREEN_TIMER_EDGE >> 8);
	ram[(FV2_SCREEN_TIMER + 1) ^ 3] = uint8_t(FV2_SCREEN_TIMER_EDGE);
}

// ---- THE TWO FIGHTERS, AND THE ROUND'S CLOCK: what TRAINING is made of ----
//
// The block the game is using for a fighter, by its own pointer. 0 when there is none -- outside a fight
// the pointer holds whatever it held, so every reader below checks the health as well, which is the guard
// that actually catches a block that is not a fighter's.
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

// The same guard every time: an implausible starting life means this is not the block we think, and then
// nothing is written at all.
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

// The round's clock stopped by the game's own switch rather than by holding its value: bit 0 of
// CONFIG_CB_DISP, which fv2_game_FIGHT_DSP tests before it decrements anything. Held, because the game
// loads that block from its backup and this is a working copy. Returns 1 when it had to write.
int Model3::fv2_freeze_timer(bool on)
{
	if (set != "fvipers2" || ram.size() <= FV2_CB_DISP + 3) return -1;
	uint8_t &low = ram[(FV2_CB_DISP + 3) ^ 3];    // bit 0 of the word is the lowest bit of its last byte
	const uint8_t want = uint8_t(on ? (low | 1) : (low & ~1));
	if (low == want) return 0;
	low = want;
	return 1;
}

// Player 2's task register, the companion of fv2_task_state. The host watches both while a fight runs so it
// knows what to put back if one of them is stopped (fv2_rematch above).
int Model3::fv2_task2_state() const
{
	if (set != "fvipers2" || ram.size() <= FV2_TASK_ENTRY2 + 3) return -1;
	return int(*reinterpret_cast<const uint32_t *>(&ram[FV2_TASK_ENTRY2]));
}

void Model3::fv2_finish_ladder()
{
	if (set != "fvipers2" || ram.size() <= FV2_ADV_INFO_STATUS + 4) return;
	uint8_t &low = ram[(FV2_ADV_INFO_STATUS + 3) ^ 3];   // the bit is in the lowest byte; the rest is left alone
	low = uint8_t(low | FV2_LADDER_DONE);
}

// Asks the attract loop to START OVER -- which is the one road on which the game tidies up after itself.
//
// Found by watching the state machine's own bytes (vf3run --watch 58FD98). The mode and its sub-state are
// rewritten every frame by the mode machine, but SUBMODE_REQUEST, 0x0058FD9B, is written only a handful of
// times, and those few writes ARE the attract loop:
//
//     at WARNING_DSP      = 0, = 1        the loop starts
//     at DONT_RECYCLE_*   = 2, = 3        on to the SEGA logo
//     at ADV_SEGA_DSP     = 4, = 5        on to the title screen
//     at FV2_TITLE_DSP    = B or = 8      on to the demonstration, or to the replay
//     at ADV_REPLAY_DSP   = 0 at 0x0000B7C0, then = 1     <- the replay ends by asking for the loop's start
//
// So the game's own way out of a replay is not to ask for the title screen: it is to ask the attract loop to
// begin again, DONT_RECYCLE_INT. And the restart is exactly where the drawing tasks are cleared, by the write
// at 0x0000B240 between sub-states 1 and 2. One request, and the game does its own housekeeping.
//
// Written ONCE and never held, like every other request here.
void Model3::fv2_restart_attract()
{
	if (set != "fvipers2" || ram.size() <= FV2_SUBMODE_REQUEST) return;
	ram[FV2_MODE_REQUEST ^ 3] = FV2_MODE_ADV_DSP;
	ram[FV2_SUBMODE_REQUEST ^ 3] = 0;   // DONT_RECYCLE_INT
}

// The attract sequencer's own road to the title screen: its list is pushed to its last real entry and its
// timer set long expired, written ONCE, and the game then walks off the end of the list itself -- which is
// the only way it takes the branch that installs the title screen off-list. Measured on the ROM in August
// (MenuConsole.md section 6 bis); writing 15 does not work, the sequencer tests the entry after the current
// one against the list's terminator and runs away.
//
// WHY THIS AND NOT THE MODE REQUEST ABOVE, measured here on the 2026-10-21 with the picture probe: asked for
// the title screen mid-demonstration, the mode machine shows it at once, but the 3D the demonstration was
// drawing is still there. A title screen the game reached by itself carries NO 3D at all -- 0 models, 8
// viewports, full_3d 0 -- where one reached by the request carried 397 models over 10 viewports and the
// full-screen 3D still on. The demonstrations that follow then draw from a scene nobody tidied, and that is
// the missing and the blackened fighters. The request changes what is displayed; it does not stop what was
// drawing. Only the game's own road does that.
//
// It works from the attract loop and only there -- the sequencer does not run during a game (docs/fv2-states.md).
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

} // namespace m3
