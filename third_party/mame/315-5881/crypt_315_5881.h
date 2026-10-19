// license:BSD-3-Clause
// copyright-holders:Andreas Naive, Olivier Galibert, David Haywood
// MAME's Sega 315-5881 (mame/sega/315-5881_crypt.cpp), reduced to what the Model 3's security board needs: the
// block cipher, the stream's headers and the compressed-line decoder. The code kept is MAME's; what changed is
// only the frame around it -- MAME's device_t, its save state and its memory maps are gone, the read delegate is
// a std::function, and the chip is set up by init() instead of a device parameter. Same licence, BSD-3-Clause,
// as MAME's 68000 in third_party/mame/m68000; none of this is GPL code.
//
// How the Model 3 uses it (src/machine/model3.cpp): the game writes an encrypted block into the security
// board's RAM at 0xF0180000, writes 0 to the address registers and its sub-key to 0xF01A0018, then reads the
// plaintext a word at a time from 0xF01A001C. So the chip's input is RAM the game itself filled -- there is no
// ROM region behind it.
#pragma once
#include <cstdint>
#include <functional>
#include <memory>

namespace mame {

class Crypt5881
{
public:
	// key: the game's encryption key (Fighting Vipers 2: 0x29260E96). read: hands the chip the encrypted 16-bit
	// word at a counter value, which on the Model 3 is the security board's RAM.
	void init(uint32_t encryption_key, std::function<uint16_t(uint32_t)> read);
	void reset();

	// One plaintext word, carrying the stream on. The caller answers the first read after an address write
	// itself, with a dummy word, the way the reference build does.
	uint16_t decrypt() { uint8_t *base; return do_decrypt(base); }

	void set_addr_low(uint16_t data);
	void set_addr_high(uint16_t data);
	void set_subkey(uint16_t data);

	std::function<uint16_t(uint32_t)> m_read;

private:
	enum {
		BUFFER_SIZE = 2, LINE_SIZE = 512,
		FLAG_COMPRESSED = 0x20000
	};

	uint32_t key = 0;

	std::unique_ptr<uint8_t[]> buffer;
	std::unique_ptr<uint8_t[]> line_buffer;
	std::unique_ptr<uint8_t[]> line_buffer_prev;
	uint32_t prot_cur_address = 0;
	uint16_t subkey = 0, dec_hist = 0;
	uint32_t dec_header = 0;

	bool enc_ready = false;

	int buffer_pos = 0, line_buffer_pos = 0, line_buffer_size = 0, buffer_bit = 0, buffer_bit2 = 0;
	uint8_t buffer2[2]{};
	uint16_t buffer2a = 0;

	int block_size = 0;
	int block_pos = 0;
	int block_numlines = 0;
	int done_compression = 0;

	struct sbox {
		uint8_t table[64];
		int inputs[6];      // positions of the inputs bits, -1 means no input except from key
		int outputs[2];     // positions of the output bits
	};

	static const sbox fn1_sboxes[4][4];
	static const sbox fn2_sboxes[4][4];

	static const int FN1GK = 38;
	static const int FN2GK = 32;
	static const int fn1_game_key_scheduling[FN1GK][2];
	static const int fn2_game_key_scheduling[FN2GK][2];
	static const int fn1_sequence_key_scheduling[20][2];
	static const int fn2_sequence_key_scheduling[16];
	static const int fn2_middle_result_scheduling[16];

	static const uint8_t trees[9][2][32];

	uint16_t do_decrypt(uint8_t *&base);
	int feistel_function(int input, const struct sbox *sboxes, uint32_t subkeys);
	uint16_t block_decrypt(uint32_t game_key, uint16_t sequence_key, uint16_t counter, uint16_t data);

	uint16_t get_decrypted_16();
	int get_compressed_bit();

	void enc_start();
	void enc_fill();
	void line_fill();
};

} // namespace mame
