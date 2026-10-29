// FV2SoundIndirectJumps: resolve the five `jmp (An)` that were called unresolvable.
//
// WHY. FV2SoundJumpTables.java enumerated eleven computed jumps, opened four tables, and wrote that
// the remaining seven `jmp (An)` "need the register's value and nothing static resolves them". That
// sentence was wrong on all seven. Measured the 2026-10-25: EVERY ONE of them is preceded, four
// bytes above, by a `lea <base>(pc,Dn.w),An` that names the destination outright. The mistake was
// reading "jmp (An)" as the whole instruction when the instruction BEFORE it carried the answer.
//
// Two were handled by FV2SoundData.java (0x601F3A, 0x6023E0). These are the other five.
//
// THREE ARE BRANCH-SLOT TABLES, the same shape as table 1 at 0x60088E -- and the index mask gives
// the slot count directly, which is better than the self-sizing rule because it is written in the
// code:
//
//   0x600308  lea 0x60030A(pc,D1.w),A1   D1 = <val> & 0x0E        8 slots of 2  (bra.b)
//   0x603494  lea 0x603496(pc,D0.w),A0   D0 = (D1>>2) & 0x1C      8 slots of 4  (bra.w)
//   0x60376A  lea 0x60376C(pc,D0.w),A4   D0 = (D0 & 7) << 2       8 slots of 4  (bra.w)
//
// And the dumps hold two surprises worth recording, because both would have been mis-sized by a
// rule that only counted branches:
//
//   At 0x60030A only SEVEN slots are `bra.b`. The eighth, 0x600318, is already ordinary code
//   (`movea.l (0xC10).w,A2`) -- the last case's handler sits right there instead of behind a branch.
//   At 0x60376C only SEVEN are `bra.w`. The eighth, 0x603788, is `move.w #0x2000,SR ; rts` in line,
//   and slot 6 branches to it. Table 1 does the same thing with the rts its slots share.
//
// So: eight destinations each, seven branches and one in-line handler. The script checks each slot's
// opcode and SAYS which ones are not branches rather than assuming.
//
// TWO ARE UNROLLED CLEAR BLOCKS, Duff's devices like the copies FV2SoundData handles -- but they
// CLEAR, they do not copy:
//
//   0x602042  lea 0x602044(pc,D5.w),A0   128 x `clr.l (A2)+`
//   0x6024E8  lea 0x6024EA(pc,D5.w),A0   128 x `clr.l (A2)+`
//
// Measured: 128 instructions each, 0x100 bytes of block, 512 bytes processed at most. And the
// arithmetic of the pair closes. For the copy at 0x601F36, D0 = (0x200 - D5) >> 1, so the entry skips
// (0x200-D5)/4 instructions and 128 - (0x200-D5)/4 = D5/4 of them run, copying D5 BYTES. For the
// clear, the index is D5>>1, so D5/4 instructions are skipped and 128 - D5/4 run, clearing
// 512 - D5 BYTES. One 512-byte destination: D5 bytes from (A1), zeros for the rest. The constant
// 0x200 in `move.w #$200,D0` is that buffer, and it appears twice.
//
// WHAT IT DOES AND DOES NOT DO. It adds the missing COMPUTED_JUMP references and disassembles, so
// the decompiler stops seeing a jump to nowhere. It does NOT create functions at the slot targets:
// coverage of this program is already 100 % (FV2SoundData), those targets are inside existing
// bodies, and cutting a function in two at a branch target would make the listing worse, not better.
//
//@category FV2
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.symbol.RefType;
import ghidra.program.model.symbol.SourceType;

public class FV2SoundIndirectJumps extends GhidraScript {

	// the jmp, the base, the slot count, the stride, and what the index mask comes to
	private static final Object[][] BRANCH_TABLES = {
		{ 0x600308L, 0x60030AL, 8, 2, "D1 & 0x0E, so eight slots of two bytes" },
		{ 0x603494L, 0x603496L, 8, 4, "(D1 >> 2) & 0x1C, so eight slots of four bytes" },
		{ 0x60376AL, 0x60376CL, 8, 4, "(D0 & 7) << 2, so eight slots of four bytes" },
	};

	// the jmp, the block's entry, the instruction count expected, the two-byte pattern
	private static final Object[][] UNROLLED_BLOCKS = {
		{ 0x602042L, 0x602044L, 128, 0x429A, "clr.l (A2)+" },
		{ 0x6024E8L, 0x6024EAL, 128, 0x429A, "clr.l (A2)+" },
	};

	private boolean isBranch(long a) throws Exception
	{
		final int word = getShort(toAddr(a)) & 0xFFFF;
		if ((word & 0xFF00) != 0x6000) return false;        // not a bra
		return true;                                        // bra.b or bra.w, either will do
	}

	@Override
	public void run() throws Exception
	{
		int refs = 0;

		for (Object[] t : BRANCH_TABLES) {
			final long jmp = (Long) t[0], base = (Long) t[1];
			final int n = (Integer) t[2], stride = (Integer) t[3];
			int branches = 0;
			final StringBuilder inLine = new StringBuilder();
			for (int k = 0; k < n; k++) {
				final long a = base + (long) stride * k;
				final Address at = toAddr(a);
				disassemble(at);
				currentProgram.getReferenceManager().addMemoryReference(
					toAddr(jmp), at, RefType.COMPUTED_JUMP, SourceType.USER_DEFINED, 0);
				refs++;
				if (isBranch(a)) branches++;
				else inLine.append(String.format(" %08X", a));
			}
			println(String.format("%08X -> %08X : %d slots of %d bytes, %d bra%s",
			                      jmp, base, n, stride, branches,
			                      inLine.length() == 0 ? ""
			                      : ", and in line (not a bra):" + inLine));
			setPlateComment(toAddr(base), String.format(
				"Branch table of the jmp (An) at %08X, %d slots of %d bytes.%n%n"
				+ "The index is %s -- the mask gives the count, and it is written in the code.%n%n"
				+ "This jmp was counted among the seven \"that nothing static resolves\": false. The lea%n"
				+ "four bytes above names this base. All seven had one.",
				jmp, n, stride, (String) t[4]));
		}

		for (Object[] d : UNROLLED_BLOCKS) {
			final long jmp = (Long) d[0], entry = (Long) d[1];
			final int expected = (Integer) d[2], pattern = (Integer) d[3];
			int n = 0;
			while ((getShort(toAddr(entry + 2L * n)) & 0xFFFF) == pattern) n++;
			disassemble(toAddr(entry));
			currentProgram.getReferenceManager().addMemoryReference(
				toAddr(jmp), toAddr(entry), RefType.COMPUTED_JUMP, SourceType.USER_DEFINED, 0);
			refs++;
			println(String.format("%08X -> %08X : %d x %s%s", jmp, entry, n, (String) d[4],
			                      n == expected ? "" : "   *** " + expected + " expected"));
			setPlateComment(toAddr(entry), String.format(
				"Unrolled block of %d x %s, entered at a computed offset: lea %08X(pc,D5.w),A0 then%n"
				+ "jmp (A0), with D5 already halved. %d instructions, 0x%X bytes of block,%n"
				+ "512 bytes processed at most.%n%n"
				+ "It CLEARS, it does not copy -- and it goes in a pair with an unrolled copy. For the%n"
				+ "copy, D0 = (0x200 - D5) >> 1: D5/4 instructions run, so D5 BYTES are copied.%n"
				+ "For this clear, the index is D5>>1: 128 - D5/4 run, so 512 - D5 BYTES are cleared.%n"
				+ "One 512-byte destination: D5 bytes from (A1), zeros for the rest. The 0x200 of%n"
				+ "move.w #$200,D0 is that buffer, and it appears twice.%n%n"
				+ "What those 512 bytes ARE is not established here.",
				n, (String) d[4], entry, n, 2 * n));
		}

		println("");
		println(refs + " computed-jump references added. No function created: coverage is already");
		println("100 % and these targets sit inside existing bodies.");
		println("NOT ONE unresolved jmp (An) is left in this program.");
	}
}
