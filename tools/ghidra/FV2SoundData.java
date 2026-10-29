// FV2SoundData: account for the bytes Ghidra leaves outside every function body.
//
// WHY. After the four jump tables were opened (683f1bf) Ghidra held 168 functions covering 14484
// bytes, and "4 % of the program is still not decompiled" was the way that got stated. THE 4 % WAS
// A DENOMINATOR ERROR. Measured the 2026-10-25, the nine holes are 654 bytes, and 650 of them were
// never code:
//
//   600000-6000FF  256  the 68000's vector table -- 64 longs, data by construction
//   600100-60011F   32  A SIGNATURE: "Ver3.5.9|98  1/13|model3|by Hiro". Thirty-two bytes that end
//                       exactly where the reset vector sends the processor (0x600120). The driver
//                       carries its version, its date and its author.
//   6002D4-6002E7   20  ten words, copied straight into the SCSP's common registers 0x418..0x42A by
//                       the loop at 0x600288: lea 0x100418,A0 / lea 0x6002D4,A1 / moveq #9,D7
//                       / move.w (A1)+,(A0)+ / dbra. They are the timers and the interrupts.
//   600396-6003B5   32  sixteen words, read by move.w (A1,D0.w) after lea 0x600396,A1 at 0x60037E,
//                       indexed by a doubled D0, and written into a buffer (A0)+
//   60103C-60107B   64  thirty-two DESCENDING words (0x1A00 -> 0x0001, entry 0 being 1), read in two
//                       places -- 0x600CF6 and 0x6038DA -- always by
//                       move.w (table,Dn.w), 0x0E(A4), the index being a counter that is incremented
//                       and stored at 0x0A(A5). A 32-step curve indexed by a counter.
//   60141C-6014BD  162  jump table 3: 81 words. 162/2 = 81, the count the self-sizing rule had
//                       given. The two measurements are independent.
//   602730-60274F   32  jump table 2: 16 words -- the count made by hand
//   602EE0-602F13   52  jump table 4: 26 words
//
// The 4 bytes that remain are CODE, and the only ones Ghidra really missed:
//
//   601F3C, 6023E2   2 bytes each: the FIRST move.l (A1)+,(A2)+ of an unrolled block of 128,
//                    entered at a computed offset -- lea 0x601F3C(pc,D0.w),A0 then jmp (A0), with
//                    D0 = (0x200 - D5) >> 1. A Duff's device. Both were counted among the
//                    "seven jmp (An) that nothing static resolves": that was false, a lea names them
//                    four bytes above. Five remain.
//
// AND THE END OF THE CODE WAS NOT THE RIGHT ONE: 0x603B20 in my constants, when the rts at that
// address occupies 0x603B20-0x603B21. So the code runs to 0x603B22, where a table of ascending bytes
// begins (80 81 .. 8b 90 .. 9b a0 ..). With the bound corrected, covered + holes = 15138 and the
// arithmetic closes; with the old one it gave 15138 against 15136.
//
// So: real code 15138 - 650 = 14488 bytes, of which Ghidra covered 14484. Four bytes.
//
// The script declares the data, attaches the four bytes of code, then COUNTS AGAIN itself what is
// left outside every function body AND outside every declared datum, and prints it. It does not
// settle for announcing a percentage: it names what is still missing, if anything is.
//
//@category FV2
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.address.AddressSet;
import ghidra.program.model.data.ArrayDataType;
import ghidra.program.model.data.PointerDataType;
import ghidra.program.model.data.WordDataType;
import ghidra.program.model.listing.Data;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionIterator;
import ghidra.program.model.symbol.RefType;
import ghidra.program.model.symbol.SourceType;

public class FV2SoundData extends GhidraScript {

	// The end of the code, the last rts included. Not 0x603B20.
	private static final long CODE_END = 0x603B22L;

	// address, word count, name, comment
	private static final Object[][] WORD_TABLES = {
		{ 0x6002D4L, 10, "scsp_init_418_42A",
		  "Ten words copied as they stand into the common registers 0x418 to 0x42A of SCSP 0, by the\n"
		  + "loop at 0x600288 (lea 0x100418,A0 / lea 0x6002D4,A1 / moveq #9,D7 / move.w (A1)+,(A0)+).\n"
		  + "Ten words, ten registers, in order:\n\n"
		  + "  0x418 TIMA/TACTL = 0x00CE     0x424 SCILV0 = 0x0088\n"
		  + "  0x41A TIMB/TBCTL = 0x03C6     0x426 SCILV1 = 0x0048\n"
		  + "  0x41C TIMC/TCCTL = 0x0100     0x428 SCILV2 = 0x0000\n"
		  + "  0x41E SCIEB      = 0x00C8     0x42A MCIEB  = 0x0020\n"
		  + "  0x420 SCIPD      = 0x0000\n"
		  + "  0x422 SCIRE      = 0x00C8\n\n"
		  + "What SCIEB = 0x00C8 says, bits 3, 6 and 7: MIDI input, timer A and timer B armed. Timer C\n"
		  + "is NOT. The three SCILV give the level per source, one bit each: MIDI (bit 3) takes\n"
		  + "0b011 = 3, timer A (bit 6) takes 0b010 = 2, timer B (bit 7) takes 0b001 = 1.\n"
		  + "MCIEB = 0x0020, bit 5, arms one source towards the other side.\n\n"
		  + "CORRECTION of 2026-10-25: this comment's first version named those bits timer B and\n"
		  + "timer C, and said timer A was not armed. It was the other way round. What revealed it:\n"
		  + "snd_irq2 (0x6032D2) reloads 0x418 -- TIMA -- and acknowledges SCIRE bit 6, which would\n"
		  + "make no sense if timer A were not armed. The bits now come from the implementation\n"
		  + "(src/sound/scsp.cpp: timerA_cb sets 0x40, timerB_cb 0x80, timerC_cb 0x100, MIDI input 8)\n"
		  + "and not from a memory of a table. The LEVELS 3, 2, 1 were right from the start: only the\n"
		  + "sources' names were swapped.\n\n"
		  + "And the value cross-checks three times: TIMA = 0xCE here, [0x60806A] = 0x00CE which\n"
		  + "snd_irq2 reloads at every tick, and the port's comment that had already measured 50\n"
		  + "samples and 882 ticks a second. It is the music's tick.\n\n"
		  + "This is a reading, not a measurement on the machine: it comes from these ten words and\n"
		  + "from the SCSP's register layout. What it predicts and what is still to be checked on the\n"
		  + "port: three interrupt sources and three only, at those three levels." },

		{ 0x600396L, 16, "word_table_600396",
		  "Sixteen words read by move.w (A1,D0.w) after lea 0x600396,A1 at 0x60037E, the index D0\n"
		  + "being doubled just before, and the word written into a buffer (A0)+ ahead of a\n"
		  + "bra.w 0x6002EC. 0x6002EC is a dispatcher over a byte stream: the branches around it do\n"
		  + "move.b (A2)+,(A0)+ once or twice before coming back to the same place. So: sixteen\n"
		  + "two-byte values that a one-digit code has emitted into the output stream.\n\n"
		  + "The values: 0010 0020 0400 0700 0A00 0E00 2022 4000 4002 7700 7800 7900 7C00 A000\n"
		  + "A040 A0A3. WHAT THEY ARE IS NOT ESTABLISHED. Naming them would mean reading the\n"
		  + "dispatcher at 0x6002EC in full, which has not been done." },

		{ 0x60103CL, 32, "curve_32_steps_60103C",
		  "Thirty-two words, and they DESCEND: 0x1A00 0x1100 0x0C90 0x09A0 0x06B0 0x0450 0x02C0\n"
		  + "0x0200 0x0160 0x0100 0x00A8 0x0078 0x0050 0x0038 0x002C 0x0026 0x0023 0x0020 0x001D\n"
		  + "0x001A 0x0017 0x0014 0x0011 0x000E 0x000B 0x0008 0x0006 0x0004 0x0003 0x0002 0x0001,\n"
		  + "entry 0 being 0x0001 apart from the rest.\n\n"
		  + "Read in two places, the same way:\n"
		  + "  0x600CF6  lea 0x60103C,A2 ; move.w (A2,D5.w), 0x0E(A4)\n"
		  + "  0x6038DA  lea 0x60103C,A3 ; move.w (A3,D3.w), 0x0E(A4)\n"
		  + "and at 0x6038D2 the index is a counter: addq.b #1,D3 ; move.w D3,0x0A(A5) ; lsl.w #1,D3.\n"
		  + "A counter stored at 0x0A(A5) advances a step, and the matching word goes to 0x0E(A4).\n\n"
		  + "Thirty-two entries for a five-bit index, and a curve that falls: that LOOKS LIKE a\n"
		  + "duration per envelope step or per software fade. IT IS NOT ESTABLISHED. What is: thirty-two\n"
		  + "descending words, two readers, a counter as the index, and 0x0E(A4) as the destination.\n"
		  + "What 0x0E is worth in the record A4 points at is not identified here." },

		{ 0x60141CL, 81, "jump_table_3_words",
		  "Jump table 3, the one of the jmp (d8,PC,D2.w) at 0x601418: the sub-dispatcher of the MIDI\n"
		  + "control changes (Bn), by controller number.\n\n"
		  + "81 words, and that count is confirmed TWICE, independently: FV2SoundJumpTables's\n"
		  + "self-sizing rule (the table ends where its lowest target begins) had given 81, and the\n"
		  + "hole Ghidra leaves here is 162 bytes, which is exactly 81 words. The two measurements\n"
		  + "share nothing." },

		{ 0x602730L, 16, "jump_table_2_words",
		  "Jump table 2, the one of the jmp (d8,PC,D2.w) at 0x60272C: the control commands, banks\n"
		  + "0x00 to 0x0F. Sixteen words -- the count was made by hand, and the 32-byte hole gives it\n"
		  + "again." },

		{ 0x602EE0L, 26, "jump_table_4_words",
		  "Jump table 4, the one of the jmp (d8,PC,D0.w) at 0x602EDC, under control bank 0x00.\n"
		  + "26 words: 52 bytes of hole, and the self-sizing rule said 26." },
	};

	// The unrolled blocks' entries: the jmp that goes there, the entry, and the two bytes to attach.
	private static final long[][] DUFF = {
		{ 0x601F3AL, 0x601F3CL },
		{ 0x6023E0L, 0x6023E2L },
	};

	@Override
	public void run() throws Exception
	{
		// --- the vector table: 64 longs
		clearListing(toAddr(0x600000L), toAddr(0x6000FFL));
		createData(toAddr(0x600000L), new ArrayDataType(PointerDataType.dataType, 64, 4));
		setPlateComment(toAddr(0x600000L),
			"The 68000's exception vector table: 64 longs, 256 bytes. Data by construction, and it is\n"
			+ "from it that FV2SoundSetup.java takes its entry points.");
		println("vectors 600000-6000FF : 64 pointers declared");

		// --- the signature
		clearListing(toAddr(0x600100L), toAddr(0x60011FL));
		createAsciiString(toAddr(0x600100L), 32);
		setPlateComment(toAddr(0x600100L),
			"THE DRIVER'S SIGNATURE: \"Ver3.5.9|98  1/13|model3|by Hiro\".\n\n"
			+ "Thirty-two bytes, which end exactly where the reset vector sends the processor:\n"
			+ "0x600120. The driver carries its version (3.5.9), its date (98 1/13), its target\n"
			+ "(model3) and its author. The name is cut off by the end of the field, not by the read.");
		println("signature 600100-60011F : \"" + getDataAt(toAddr(0x600100L)).getValue() + "\"");

		// --- the six word tables
		for (Object[] t : WORD_TABLES) {
			final long a = (Long) t[0];
			final int n = (Integer) t[1];
			final String name = (String) t[2];
			final Address at = toAddr(a);
			clearListing(at, toAddr(a + 2L * n - 1));
			final Data d = createData(at, new ArrayDataType(WordDataType.dataType, n, 2));
			createLabel(at, name, true);
			setPlateComment(at, (String) t[3]);
			println(String.format("%08X : %d words (%d bytes) -> %s", a, n, d.getLength(), name));
			if (d.getLength() != 2 * n)
				println("*** unexpected length at " + at + " : " + d.getLength());
		}

		// --- the two unrolled-block entries
		for (long[] p : DUFF) {
			final Address jmp = toAddr(p[0]), entry = toAddr(p[1]);
			currentProgram.getReferenceManager().addMemoryReference(
				jmp, entry, RefType.COMPUTED_JUMP, SourceType.USER_DEFINED, 0);
			disassemble(entry);
			final Function f = getFunctionContaining(entry.add(2));
			if (f == null) {
				println("*** nothing contains " + entry.add(2) + " : the two bytes stay outside");
				continue;
			}
			final AddressSet body = new AddressSet(f.getBody());
			body.addRange(entry, entry.add(1));
			f.setBody(body);
			setPreComment(entry,
				"Entry of an unrolled block of 128 move.l (A1)+,(A2)+ : lea " + entry + "(pc,D0.w),A0\n"
				+ "then jmp (A0), with D0 = (0x200 - D5) >> 1. A Duff's device -- the copy starts the\n"
				+ "further into the block the less there is to copy. These two bytes were the only ones\n"
				+ "in the program Ghidra did not hold, and the jmp was wrongly counted among the\n"
				+ "indirect jumps that nothing resolves.");
			println(String.format("%08X attached to %s (%d bytes of body)",
			                      p[1], f.getName(), f.getBody().getNumAddresses()));
		}

		// --- the recount, done by the script itself
		final AddressSet accounted = new AddressSet();
		final FunctionIterator it = currentProgram.getFunctionManager().getFunctions(true);
		int nf = 0;
		while (it.hasNext()) { accounted.add(it.next().getBody()); nf++; }
		long codeBytes = accounted.getNumAddresses();

		final AddressSet data = new AddressSet();
		Data d = getDataAt(toAddr(0x600000L));
		for (Address a = toAddr(0x600000L); a.getOffset() < CODE_END; ) {
			d = getDataContaining(a);
			if (d != null && d.isDefined()) {
				data.addRange(d.getMinAddress(), d.getMaxAddress());
				a = d.getMaxAddress().add(1);
			} else {
				a = a.add(1);
			}
		}

		final AddressSet leftOut = new AddressSet(toAddr(0x600000L), toAddr(CODE_END - 1));
		leftOut.delete(accounted);
		leftOut.delete(data);

		final long total = CODE_END - 0x600000L;
		println("");
		println(String.format("%d functions, %d bytes inside a body, %d bytes of declared data,"
		                      + " out of %d from 600000-%06X", nf, codeBytes, data.getNumAddresses(),
		                      total, CODE_END));
		println(String.format("real code = %d - %d = %d bytes, of which %d inside a body = %.2f %%",
		                      total, data.getNumAddresses(), total - data.getNumAddresses(),
		                      codeBytes, 100.0 * codeBytes / (total - data.getNumAddresses())));
		println(String.format("LEFT OUT OF EVERYTHING: %d bytes", leftOut.getNumAddresses()));
		for (ghidra.program.model.address.AddressRange r : leftOut)
			println(String.format("  %08X-%08X  %d", r.getMinAddress().getOffset(),
			                      r.getMaxAddress().getOffset(), r.getLength()));
		if (leftOut.isEmpty())
			println("  (nothing: every byte from 600000 to " + Long.toHexString(CODE_END)
			        + " is either inside a function or a named datum)");
		println("");
		println("Five jmp (An) are left with no static resolution, not seven: 0x600308, 0x602042,");
		println("0x6024E8, 0x603494, 0x60376A. The other two (0x601F3A, 0x6023E0) are handled here.");
	}
}
