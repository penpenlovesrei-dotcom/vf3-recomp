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
	if (getenv("VF3_BANK_LOG") && v != crom_bank_reg)
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
		hang_sample();
		if (after_slice) after_slice();
	}
}

void Model3::run_frame_start()
{
	stage_note();
	murs_note();
	pmg_note();
	hang_watch();

	auto exec = [this](int n) {
		cpu.execute(n);
		hang_sample();
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
		// VF3_CROM_WATCH=<hex address>: every read of that word in the CROM, with the window it came from
		if (const char *w = getenv("VF3_CROM_WATCH"))
			if (a == uint32_t(strtoul(w, nullptr, 16)))
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

// VF3_STAGE=<n> : le decor que le jeu tire au sort, force.
//
// CE QUI A ETE MESURE. L'octet en 0x0058FD9F porte l'index du decor courant,
// dans la table des 27 enregistrements de 0x70 octets en 0x00075170 : son +0x00
// pointe le nom, et les 27 noms se lisent en clair (TEST, WESTERN, AC_CARRIER,
// ...), TEST en 0. Seize sites ecrivent cet octet, dont deux depuis le tirage
// de 0x0007111C, qui prend un octet du generateur, le reduit modulo 27, puis
// le filtre par un masque de 27 octets en 0x004836FF -- zero autorise.
//
// POURQUOI ON NE REPOSE PAS L'OCTET A CHAQUE IMAGE. C'est ce qui a ete essaye
// d'abord, et ca desynchronise : pendant la demonstration, les modeles d'un
// decor sont deja charges quand l'octet dit l'autre, et on obtient le decor de
// l'un avec le sol de l'autre. Il faut forcer le CHOIX, une fois, pour que les
// modeles, l'arene et la camera suivent ensemble.
//
// On patche donc le tireur lui-meme pour qu'il rende l'index voulu :
//
//     0007111C   stwu r1,-0x20(r1)   ->   li r3,<n>
//     00071120   mflr r0             ->   blr
//
// L'adresse est en RAM : le programme est un gzip que l'amorce decompresse en
// RAM 0, donc il n'y a rien a patcher avant qu'elle soit passee par la. D'ou la
// lecture avant l'ecriture -- autre chose que l'instruction de Sega veut dire
// que ce n'est pas le moment. Meme precaution que unlock_roster.
// CE QUI NE SUFFIT PAS : patcher le seul tireur. Mesure -- avec lui patche, un vrai combat demarre quand meme
// sur DAM. Le decor d'un combat ne vient pas du tirage mais d'une FICHE DE COMBAT : en 0x00072C94 le jeu pose
// `lis r9,0x48 ; addi r9,r9,0x3744`, soit une table en 0x00483744 dont le champ +4 de chaque fiche est le decor.
// Deux autres chemins le lisent en +0x27 d'une autre structure, et deux encore dans des octets voisins du sien.
//
// On remplace donc les CINQ lectures indirectes par la constante. Les ecrivains EN DUR sont laisses tranquilles
// -- l'attract (0x000B3C4 pose 21) et l'ecran de selection (0x0005F050 pose 26) : c'est en les ecrasant qu'on
// obtenait le decor de l'un avec le sol de l'autre.
int Model3::force_stage(int n)
{
	if (set != "fvipers2") return -1;
	if (n < 0 || n > 26) return -1;
	// (adresse, l'instruction que Sega a ecrite, le registre charge)
	struct Site { uint32_t at, was; int reg; };
	static const Site sites[] = {
		{ 0x0007111C, 0x9421FFE0, -1 },   // le tireur : stwu r1,-0x20(r1) -> li r3,n ; blr
		{ 0x00072CD0, 0x880B0004,  0 },   // lbz r0,4(r11)      la fiche de combat
		{ 0x00072FD8, 0x881D0027,  0 },   // lbz r0,0x27(r29)
		{ 0x000732EC, 0x881F0027,  0 },   // lbz r0,0x27(r31)
		{ 0x0000D2A0, 0x8863FDA4,  3 },   // lbz r3,-0x25c(r3)
		{ 0x0000D54C, 0x8863FDA5,  3 },   // lbz r3,-0x25b(r3)
		// Les deux du demarrage d'un combat. Sans eux le decor repassait a 9 (DAM) des que le combat commencait,
		// alors que l'attract, lui, affichait bien le notre : c'est ce que l'ecran montrait.
		{ 0x000716B4, 0x7FC0F378,  0 },   // mr r0,r30
		{ 0x00071748, 0x88091460,  0 },   // lbz r0,0x1460(r9)
	};
	// EMPRUNTER LA LISTE DE COMPORTEMENTS D'UN AUTRE DECOR NE MARCHE PAS -- essaye, et mesure.
	//
	// Le champ +0x20 pointe les comportements que le decor active, par indice dans la table de 0x1C octets
	// de 0x00114FE0. TEST en active zero, les autres neuf, dont sept communs a tous. On a donc fait pointer
	// TEST sur la liste de TOWER (0x001155A2) pour lui donner murs et chute.
	//
	// Resultat a l'ecran : les comportements s'activent bien -- des objets apparaissent et eclatent -- mais
	// ils referencent la geometrie de LEUR decor. Sur TEST, qui ne l'a pas, ca ne donne que des eclats
	// parasites, et ni mur ni chute. Une liste n'est donc pas transposable : il faudra lire ce que les
	// entrees 08, 06, 05 et 04 vont chercher, et leur donner la geometrie du ring.

	// LES DEUX CHAMPS QUE LA BORNE N'A PAS, recopies de la Dreamcast.
	//
	// L'enregistrement d'un decor a la meme disposition des deux cotes. Compares champ par champ, ceux de TEST
	// ne different que par deux valeurs :
	//
	//              +0x30   +0x34    +0x38   +0x3c   +0x40
	//     DC       5.000   99.990   2.800   3.000   0.300
	//     borne    5.000    0.000   2.700   3.000   0.300
	//
	// Et 99,99 est exactement ce que portent DAM et TOWER sur la borne -- les decors a chute longue. Zero chez
	// TEST, donc un ejecte qui ne tombe pas. On ne choisit aucune valeur : on recopie celles de la Dreamcast.
	if (n == 0) {
		constexpr uint32_t REC = 0x0007516C;   // l'enregistrement de TEST
		struct Champ { uint32_t off, etait, devient; const char *quoi; };
		static const Champ champs[] = {
			{ 0x34, 0x00000000, 0x42C7FAE1, "la profondeur de chute (0 -> 99,99)" },
			{ 0x38, 0x402CCCCD, 0x40333333, "le champ +0x38 (2,70 -> 2,80)" },
		};
		for (const Champ &c : champs)
			if (word(ram, REC + c.off) == c.etait) {
				word(ram, REC + c.off) = c.devient;
				fprintf(stderr, "decor TEST : %s, comme sur Dreamcast\n", c.quoi);
			}

		// L'ENCEINTE ELARGIE A 6,0 : UN ECART ASSUME, PAS UN CORRECTIF.
		//
		// Que personne ne reprenne ceci pour une fidelite retrouvee. L'enceinte est le carre de demi-cote
		// `fiche_decor + 0x30`, que 0x0001809C recopie en 0x005813F8 et que 0x00046A60 applique au X
		// (+0x94) et au Z (+0x9C) du combattant. La Dreamcast y met 5,0, comme la borne et comme les dix-neuf
		// decors jouables des deux machines. Verifie quatre fois :
		//
		//     la fiche TEST de la DC, +0x30                              5,000
		//     le global vivant 0x8C1E62F0, sauvegarde Flycast sur TEST   5,000
		//     le combattant DC colle au mur, meme sauvegarde             X = -4,574
		//     le combattant du port au meme mur                          X = -4,645
		//
		// Le 5,0 du port est donc deja celui de la Dreamcast, et il n'y avait rien a corriger.
		//
		// CE QUI MOTIVE L'ECART. Le ring VISIBLE est plus large que l'enceinte : le moteur decode les
		// quatre panneaux de deflecteurs a x et z = +-6,0 et les faisceaux a +-5,698..6,841, pour un sol de
		// rayon 4,781. Le combattant s'arrete donc un metre avant les deflecteurs -- 17 % trop tot a l'oeil.
		// C'est vrai sur Dreamcast aussi, mais Frederic veut aller jusqu'aux deflecteurs. On a essaye 6,0,
		// le plan des panneaux : a l'ecran le combattant y entrait. 5,9 le laisse au contact, et c'est la
		// valeur retenue, choisie A L'OEIL et par lui -- elle ne vient d'aucune donnee.
		//
		// VF3_RING=<valeur> change ce nombre sans recompiler -- pour en essayer d'autres a l'ecran.
		{
			float bord = 5.9f;
			if (const char *s = getenv("VF3_RING")) bord = float(atof(s));
			uint32_t mot;
			memcpy(&mot, &bord, 4);
			// On n'ecrit que sur le 5,0 d'origine : avant que l'amorce ait decompresse le programme,
			// l'enregistrement est encore a zero, et il ne faut rien poser dedans.
			if (word(ram, REC + 0x30) == 0x40A00000 && mot != 0x40A00000) {
				float avant;
				const uint32_t a = word(ram, REC + 0x30);
				memcpy(&avant, &a, 4);
				word(ram, REC + 0x30) = mot;
				fprintf(stderr, "decor TEST : enceinte elargie de %.2f a %.2f -- ECART ASSUME,"
				        " la Dreamcast tient 5,00 (VF3_RING)\n", avant, bord);
			}
		}
	}

	// LE CHAMP +0x24 N'EST PAS UNE ARENE : C'EST LA LISTE D'AFFICHAGE DES OBJETS DU DECOR.
	//
	// Ce bloc repointait le +0x24 de TEST sur celui de TOWER, en croyant lui donner une arene de
	// collision. C'etait faux, et le repointage est retire le 23 octobre 2026. La boucle de
	// 0x00027D2C se lit ainsi :
	//
	//     r3  = [fiche + 0x24]        le bloc d'objets
	//     r26 = [r3 + 0]              la liste : 32 paires de demi-mots
	//     r25 = r3 + 0x18             les matrices, 0x60 octets = douze flottants chacune
	//     lhz r6,0(r26)  -> sth r6,0xE(r16)     l'identifiant de l'objet
	//     lhz r6,2(r26)  -> x 0x60              l'index de sa matrice
	//
	// Les « enregistrements de 120 flottants » sont donc DIX MATRICES 3x4, pas des triplets de plans :
	// 0,050 -0,050 1,183 -1,083 sont des elements de matrice. Et les listes se lisent en clair :
	//
	//     TOWER     068E 068C 068B 068D puis 068B 068D ... quatre objets distincts
	//     WESTERN   0626 x 32
	//     DAM       0680 x 32
	//     TEST      00A0 00A0 00A0 0000 ...  sa propre liste de SONS relue comme des objets
	//
	// Repointer TEST sur TOWER ne corrigeait donc aucune collision : ca lui faisait DESSINER les
	// trente-deux objets de TOWER. L'enceinte, elle, est le carre +-fiche+0x30, et elle etait deja
	// juste -- voir le bloc plus haut.
	//
	// Ou un decor declare sa geometrie reste ouvert. Les identifiants d'objet (0x068B..0x068E, 0x0626,
	// 0x0680) sont un espace de noms distinct des identifiants sonores en 0x00A0xxxx.

	int faits = 0;
	for (const Site &s : sites) {
		if (word(ram, s.at) != s.was) continue;
		if (s.reg < 0) {
			if (word(ram, s.at + 4) != 0x7C0802A6u) continue;   // mflr r0
			word(ram, s.at) = 0x38600000u | uint32_t(n);        // li r3,n
			word(ram, s.at + 4) = 0x4E800020u;                  // blr
			cpu.ram_written(s.at + 4);
		} else {
			word(ram, s.at) = 0x38000000u | (uint32_t(s.reg) << 21) | uint32_t(n);   // li rN,n
		}
		cpu.ram_written(s.at);
		faits++;
	}
	return faits;
}

// LE DETECTEUR DE GEL. Mesure : le jeu s'est fige a l'explosion d'armure du second round, et AUCUNE des six
// assertions nommees n'a parle. Le message de 0x00274A24 ne suffit donc pas -- il ne couvre que les six pieges
// du gestionnaire de polygones, et le programme en compte vingt-sept en tout, dont certains sans message.
//
// Un gel, c'est le PowerPC qui tourne en rond. On echantillonne donc son compteur de programme a chaque image :
// s'il ne bouge plus sur une seconde entiere, on le dit, avec l'instruction qui s'y trouve. L'adresse nomme le
// piege directement, et il n'y a plus rien a deviner.
void Model3::hang_report()
{
	const uint32_t pc = cpu.pc();
	fprintf(stderr, "GEL : le PowerPC tourne en %#010x (instruction %08x)\n", pc, peek32(pc));
	for (int i = -3; i <= 3; i++)
		fprintf(stderr, "   %#010x  %08x%s\n", pc + 4 * i, peek32(pc + 4 * i), i ? "" : "   <--");
	// Le premier gel attrape etait la memcpy octet a octet de 0x00001E20 : lbzu / stbu / addic. r5,-1 / bne.
	// Une longueur nulle y devient 0xFFFFFFFF au premier tour, et la boucle part pour quatre milliards
	// d'iterations. Ce qui manque alors, c'est la longueur et l'appelant -- d'ou ce qui suit.
	fprintf(stderr, "   retour en %#010x ; r3=%#010x r4=%#010x r5=%#010x r6=%#010x\n",
	        cpu.lr(), cpu.gpr(3), cpu.gpr(4), cpu.gpr(5), cpu.gpr(6));
	fprintf(stderr, "   r7=%#010x r8=%#010x r9=%#010x r31=%#010x\n",
	        cpu.gpr(7), cpu.gpr(8), cpu.gpr(9), cpu.gpr(31));
}

// CE QUE LE PREMIER DETECTEUR NE POUVAIT PAS PROUVER. Il comparait le compteur de programme d'une image a la
// suivante. Or le recompilateur n'ecrit `s.pc` qu'aux SORTIES DE BLOC (ppc_rec.cpp, `pcs(s.pc, s.next)`), donc
// un echantillon par image peut etre perime, et la meme adresse soixante fois ne prouve rien a elle seule.
//
// Celui-ci echantillonne apres CHAQUE TRANCHE d'execution -- quelques centaines de fois par image -- et retient
// les adresses DISTINCTES. Un jeu qui tourne normalement en traverse des centaines ; un jeu bloque en traverse
// une poignee. C'est la TAILLE de l'ensemble qui conclut, pas la repetition d'une valeur, et l'ensemble lui-meme
// nomme la region ou il tourne.
void Model3::hang_sample()
{
	if (set != "fvipers2" || hang_said >= 3 || hang_over) return;
	const uint32_t pc = cpu.pc();
	// Le programme du jeu tourne en RAM a la base 0 ; l'amorce, elle, tourne dans le CROM. Et elle y passe plus
	// d'une seconde dans une boucle de cinq adresses : elle efface 5,5 Mio de RAM (0xFFF018D8, r5 = 0x15293A).
	// Sans ce filtre le detecteur criait au gel avant meme le demarrage, et se taisait ensuite.
	if (pc >= 0x00800000) return;
	for (unsigned i = 0; i < hang_n; i++)
		if (hang_pcs[i] == pc) return;
	if (hang_n < sizeof(hang_pcs) / sizeof(hang_pcs[0]))
		hang_pcs[hang_n++] = pc;
	else
		hang_over = true;   // plus de 32 adresses : le jeu bouge, inutile d'en retenir plus
}

void Model3::hang_watch()
{
	if (set != "fvipers2" || hang_said >= 3 || !hang_armed) return;
	// LE SIGNAL DE VIE EST CELUI DU JEU, pas une heuristique sur le code execute. Mesure : le critere
	// precedent -- « moins de huit adresses distinctes en une seconde » -- attrapait les attentes actives de
	// l'amorce, qui sont legitimes et longues : une boucle de decompte de 83 millions en 0x000074C4, et
	// l'effacement de 5,5 Mio de RAM en 0xFFF018D8. Les trois rapports etaient consommes avant l'attract.
	//
	// Le jeu, lui, reconstruit sa liste d'affichage a chaque image : le curseur 0x00580FA4 repart de 0x800000
	// et grandit (voir 0x00004F70 et 0x00005080). S'il ne bouge plus, le jeu ne construit plus rien, et c'est
	// la definition meme du gel qu'on cherche.
	const uint32_t vie = word(ram, 0x00580FA4);
	if (vie != hang_live) {
		hang_live = vie;
		hang_frames = 0;
		hang_n = 0;
		hang_over = false;
		return;
	}
	if (++hang_frames < 120) return;   // deux secondes sans que la liste d'affichage bouge
	const unsigned n = hang_over ? 999 : hang_n;
	hang_frames = 0;
	hang_n = 0;
	hang_over = false;
	if (n == 0) return;
	hang_said++;
	fprintf(stderr, "GEL : en une seconde le PowerPC n'a traverse que %u adresse(s) :\n", n);
	for (unsigned i = 0; i < n; i++)
		fprintf(stderr, "   %#010x  %08x\n", hang_pcs[i], peek32(hang_pcs[i]));
	hang_report();
}

// LE GUETTEUR D'ASSERTION. Le gestionnaire de polygones du jeu verifie ses budgets, et quand l'un saute il ecrit
// un message puis se branche sur lui-meme -- le jeu gele sur place, sans rien dire. Six pieges de cette forme,
// tous non gardes, tous dans 0x00005228..0x0000538C :
//
//     005228  lwz   r3, 0xfa4(r3)      le curseur du miroir de RAM de culling
//     005234  addi  r4, r4, 0x740      le plafond, 0x00800740
//     005238  cmpw  r3, r4
//     00523c  ble   0x5254             si on tient, on continue
//     00524c  stw   r3, 0x4a24(r2)     sinon on pose le message en 0x00274A24
//     005250  b     0x5250             et on gele
//
//     0x00580FA4 <= 0x00800740   "PMG ERROR:TOO MANY VIEWPORT"
//     0x00580FA8 <= 0x00802A80   "PMG ERROR:TOO MANY CULLING NODE"
//     0x00581030 <= 0x00805DA8   "PMG ERROR:TOO MANY MATRIX" / MEMCPY_SCRIPT / SIBTBL
//
// Le mot de 0x00274A24 nomme donc l'assertion qui a saute. On le lit a chaque image et on le dit une fois : le
// gel cesse d'etre muet, et on sait lequel des six budgets deborde sans avoir a deviner.
void Model3::pmg_note()
{
	if (set != "fvipers2" || ram.size() < 0x600000) return;
	static bool dit = false;
	if (dit) return;
	const uint32_t p = word(ram, 0x00274A24);
	if (!p || p + 64 >= ram.size()) return;
	char m[64];
	size_t k = 0;
	for (; k < sizeof(m) - 1; k++) {
		const uint8_t c = ram[(p + k) ^ 3];
		if (c < 32 || c >= 127) break;
		m[k] = char(c);
	}
	m[k] = '\0';
	// L'emplacement n'est jamais vide : au repos il porte "NON ERROR". Ce n'est donc pas la nullite qui
	// signale la panne mais le CHANGEMENT de message -- mesure au premier essai, ou le guetteur a parle des
	// l'amorce avec les trois budgets a zero.
	if (k == 0 || strncmp(m, "NON ERROR", 9) == 0) return;
	dit = true;
	fprintf(stderr, "ASSERTION du jeu : %s  (message en %#010x)\n", m, p);
	hang_report();
	fprintf(stderr, "   budgets : viewport %#x/%#x, noeuds %#x/%#x, matrices %#x/%#x\n",
	        word(ram, 0x00580FA4), 0x00800740u,
	        word(ram, 0x00580FA8), 0x00802A80u,
	        word(ram, 0x00581030), 0x00805DA8u);
}

// Le decor que le jeu croit afficher, lu et non deduit d'une image : l'octet
// de 0x0058FD9F, puis le pointeur de nom du bon enregistrement. Ecrit sur
// stderr quand il change, et seulement alors.
void Model3::stage_note()
{
	static const bool on = getenv("VF3_STAGE_LOG") != nullptr;
	if (!on || set != "fvipers2" || ram.size() < 0x600000) return;
	const unsigned i = ram[0x58FD9F ^ 3];
	static unsigned last = 0xFFFF;
	if (i == last) return;
	last = i;
	char nom[24] = "?";
	if (i < 27) {
		const uint32_t p = word(ram, 0x00075170 + i * 0x70);
		if (p && p + sizeof(nom) < ram.size())
			for (size_t k = 0; k < sizeof(nom) - 1; k++) {
				const uint8_t c = ram[(p + k) ^ 3];
				nom[k] = (c >= 32 && c < 127) ? char(c) : '\0';
				if (!nom[k]) break;
			}
	}
	fprintf(stderr, "decor : index %u (%s)\n", i, nom);
}

// LE MASQUE DES MURS, lu a l'execution parce qu'il n'est pas lisible autrement.
//
// L'arene n'est pas installee au chargement du decor : elle est (re)construite quand un MASQUE change. En
// 0x00027B74 :
//
//     r5 = [0x00075124]            une structure batie en RAM, hors de l'image du programme
//     r3 = [r5 + 0xC0]             le masque courant
//     r20 = r3 XOR l'ancien        ce qui a change
//     bit 1 change -> 0x00027D2C avec r18 = 0        (installer une paroi)
//     bit 2 change -> ... r18 = 0xC000
//     bit 3 change -> ... r18 = 0x8000
//
// Chaque bit est donc une paroi, et c'est sa modification qui la pose ou la retire. Si ce masque reste nul sur
// TEST, aucune paroi n'est jamais installee -- et aucun patch de la donnee source ne peut y changer quoi que
// ce soit, ce qui expliquerait les deux essais sans effet.
void Model3::murs_note()
{
	if (set != "fvipers2" || !getenv("VF3_STAGE_LOG") || ram.size() < 0x800000) return;
	// LE MASQUE DES MURS, LU A LA BONNE ADRESSE CETTE FOIS.
	//
	// La version precedente de cette sonde annoncait dans son commentaire qu'elle lisait
	// [[0x00075124] + 0xC0] -- et lisait en fait 0x0058FD50 et 0x005818D9. La conclusion qu'elle a
	// servie le 23 octobre au matin (<< le masque reste a zero meme sur TOWER >>) ne portait donc sur
	// rien. Ce qui suit lit la vraie source, celle de 0x00027B70 :
	//
	//     r5  = [0x00075124]        la structure de combat
	//     r3  = [r5 + 0xC0]         le masque
	//     r4  = [0x005818B4]        sa copie de l'image precedente
	//     r20 = r3 ^ r4             les bits qui viennent de changer
	//     chaque bit qui bascule rappelle FUN_00027C04 puis FUN_00027D2C, qui RECONSTRUISENT la
	//     liste d'affichage des objets du decor.
	//
	// Et le consommateur, en 0x000284F4, boucle sur trente-deux entrees de 0x10 octets a partir de
	// 0x00581A1C (ou 0x0058589C) :
	//
	//     lbz r3,0xC(r29) ; cmpwi r3,2 ; beq -> pas dessine
	//     lhz r16,0xE(r29)                     l'identifiant de l'objet
	//
	// Un objet detruit est donc un objet dont l'octet +0xC vaut 2. On imprime les deux : le masque, et
	// les trente-deux etats.
	const uint32_t combat = word(ram, 0x00075124);
	uint32_t masque = 0;
	if (combat + 0xC0 + 4 < ram.size()) masque = word(ram, combat + 0xC0);
	std::string etats;
	for (int i = 0; i < 32; i++) {
		const uint32_t b = (0x00581A1C + uint32_t(i) * 0x10 + 0xC) ^ 3;
		etats += char('0' + (b < ram.size() ? ram[b] % 10 : 9));
	}
	static uint32_t am = 0xFFFFFFFF;
	static std::string ae;
	if (masque == am && etats == ae) return;
	am = masque; ae = etats;
	// r12 : la base des entrees de 0x10 octets. 0x00027B18 fait `or r17,r12,r12` et
	// 0x00027B30 `addi r17,r12,0x210` -- deux tampons de 33 entrees. Le jeu s'en sert comme d'un
	// registre global ; on l'imprime pour voir s'il est stable a la frontiere d'image, faute de
	// l'avoir trouve pose dans le code.
	fprintf(stderr, "murs : masque %08x  r12 %08x  parois %u  etats %s\n",
	        masque, cpu.gpr(12), ram[0x005818D9 ^ 3], etats.c_str());
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
constexpr uint32_t FV2_SURVIVAL_SETTING = 0x00120550;
constexpr uint32_t FV2_REGION = 0x00120557;
}

// ---- The game's own state machine (docs/fv2-etats.md) ----
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
// It works from the attract loop and only there -- the sequencer does not run during a game (docs/fv2-etats.md).
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
