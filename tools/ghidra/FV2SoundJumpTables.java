// FV2SoundJumpTables: open the sound driver's PC-relative jump tables to Ghidra.
//
// WHY. Ghidra follows calls and fall-through but does not recover these tables, so everything
// reachable only through them is invisible to it. Measured 2026-10-25: 41 functions and 45 % of the
// code covered before the first two were opened, 90 and 77 % after, and the holes that remained
// turned out to be the targets of two more.
//
// EVERY COMPUTED JUMP IN THE PROGRAM, enumerated rather than guessed (11 of them):
//
//     0x60088C  jmp (A1)            table 1, via lea (0x60088E,PC,D0.w) -- the MIDI status dispatch
//                                   (read 2026-10-25: more precisely, the dispatch of 0x600852, the
//                                   consumer that pulls messages from the 0x800 ring -- not of MIDI
//                                   arriving from the main board. Its slot 5 (0xDn) and slot 7 (0xFn)
//                                   both land on one shared rts, so those two are ignored.)
//     0x60272C  jmp (d8,PC,D2.w)    table 2 at 0x602730 -- control commands, banks 0x00-0x0F
//     0x601418  jmp (d8,PC,D2.w)    table 3 at 0x60141C -- the Bn control-change sub-dispatch
//     0x602EDC  jmp (d8,PC,D0.w)    table 4 at 0x602EE0 -- under control bank 0x00
//     0x600308, 0x601F3A, 0x602042, 0x6023E0, 0x6024E8, 0x603494, 0x60376A   jmp (An)
//
// The seven `jmp (An)` need the register's value and are NOT handled here: nothing static resolves
// them, and guessing would be worse than leaving them.
//
// CORRECTION, 2026-10-25: TWO OF THE SEVEN DO RESOLVE. 0x601F3A and 0x6023E0 are each preceded by
// `lea 0x<entry>(pc,D0.w),A0` four bytes above, which names the destination block outright -- an
// unrolled copy of 128 `move.l (A1)+,(A2)+` entered at a computed offset, a Duff's device. They are
// handled by FV2SoundData.java. Five remain: 0x600308, 0x602042, 0x6024E8, 0x603494, 0x60376A.
// The lesson: "jmp (An)" was read as the whole instruction, when the instruction BEFORE it was the
// one carrying the answer.
//
// THE TWO FORMS, read before a line of this was written:
//
//   Table 1 is EIGHT SLOTS OF FOUR BYTES, each holding a bra.w (the 0xFn slot holds the rts they
//   share). The slots are the jmp's destinations.
//
//   Tables 2, 3 and 4 are WORDS, each an offset relative to the table's own base. Their length is
//   not written anywhere -- but each table ENDS where its lowest target begins, so the count is
//   (lowest target - base) / 2, found by reading words while the read position stays below the
//   lowest target seen. That rule is checked against table 2, whose 16 entries were counted by hand
//   first: if it ever stops giving 16, this script fails loudly instead of quietly sizing the others
//   wrong.
//
// It prints functions AND covered bytes on both sides, because the count can rise while coverage
// does not. Ghidra's undo covers the whole change.
//
//@category FV2
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionIterator;
import ghidra.program.model.symbol.RefType;
import ghidra.program.model.symbol.SourceType;

import java.util.LinkedHashSet;

public class FV2SoundJumpTables extends GhidraScript {

	private static final long CODE_END = 0x603B20L;
	private static final double CODE_BYTES = 15136.0;

	// the bra.w table: the jmp, the base, the slot count
	private static final long T1_JMP = 0x60088CL, T1_BASE = 0x60088EL;
	private static final int T1_SLOTS = 8;

	// the word-offset tables: the jmp, the base, and the count expected when it is already known
	private static final long[][] WORD_TABLES = {
		{ 0x60272CL, 0x602730L, 16 },   // known by hand: 16. The check.
		{ 0x601418L, 0x60141CL, -1 },
		{ 0x602EDCL, 0x602EE0L, -1 },
	};

	private long covered()
	{
		long n = 0;
		final FunctionIterator it = currentProgram.getFunctionManager().getFunctions(true);
		while (it.hasNext()) n += it.next().getBody().getNumAddresses();
		return n;
	}

	private void report(String when)
	{
		println(String.format("%-8s %d functions, %d bytes covered (%.0f %% of the %.0f of code)",
		                      when, currentProgram.getFunctionManager().getFunctionCount(),
		                      covered(), 100.0 * covered() / CODE_BYTES, CODE_BYTES));
	}

	@Override
	public void run() throws Exception
	{
		report("before:");
		final LinkedHashSet<Long> targets = new LinkedHashSet<>();

		// --- table 1: the slots themselves are the destinations; disassembling them lets Ghidra see
		// each bra.w and follow it.
		for (int k = 0; k < T1_SLOTS; k++) {
			final Address slot = toAddr(T1_BASE + 4L * k);
			disassemble(slot);
			currentProgram.getReferenceManager().addMemoryReference(
				toAddr(T1_JMP), slot, RefType.COMPUTED_JUMP, SourceType.USER_DEFINED, 0);
			targets.add(slot.getOffset());
		}
		println(String.format("table 1 %08X : %d bra.w slots, referenced from the jmp at %08X",
		                      T1_BASE, T1_SLOTS, T1_JMP));

		// --- the word-offset tables, each sized by its own lowest target
		for (long[] t : WORD_TABLES) {
			final long jmp = t[0], base = t[1];
			final long expected = t[2];
			long lowest = Long.MAX_VALUE;
			int n = 0;
			while (n <= 256) {
				final long pos = base + 2L * n;
				if (pos >= lowest)
					break;
				final long dest = base + (getShort(toAddr(pos)) & 0xFFFFL);
				if (dest < 0x600000L || dest >= CODE_END)
					break;
				lowest = Math.min(lowest, dest);
				n++;
			}
			if (expected > 0 && n != expected) {
				println(String.format("*** table %08X : the rule gives %d entries, %d were counted by hand."
				                      + " The script stops rather than size the others blind.",
				                      base, n, expected));
				return;
			}
			final LinkedHashSet<Long> mine = new LinkedHashSet<>();
			for (int k = 0; k < n; k++) {
				final long dest = base + (getShort(toAddr(base + 2L * k)) & 0xFFFFL);
				mine.add(dest);
			}
			for (long dest : mine) {
				final Address at = toAddr(dest);
				disassemble(at);
				currentProgram.getReferenceManager().addMemoryReference(
					toAddr(jmp), at, RefType.COMPUTED_JUMP, SourceType.USER_DEFINED, 0);
			}
			targets.addAll(mine);
			println(String.format("table   %08X : %3d entries%s, %d distinct targets, from the jmp at %08X",
			                      base, n, expected > 0 ? " (rule confirmed against a count made by hand)" : "",
			                      mine.size(), jmp));
		}

		int made = 0, already = 0;
		for (long t : targets) {
			final Address at = toAddr(t);
			if (getFunctionAt(at) != null) { already++; continue; }
			if (createFunction(at, null) != null) made++;
		}
		println(targets.size() + " distinct targets in all: " + made + " functions created, " + already + " already there");

		println("analysis run again...");
		analyzeAll(currentProgram);
		report("after:");
		println("Five jmp (An) are left with no static resolution: 0x600308, 0x602042, 0x6024E8,");
		println("0x603494, 0x60376A. The other two -- 0x601F3A and 0x6023E0 -- are unrolled-block");
		println("entries that a lea names, and FV2SoundData.java handles them.");
	}
}
