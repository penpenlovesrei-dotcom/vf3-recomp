// FV2SonTablesDeSaut: open the sound driver's two PC-relative jump tables to Ghidra.
//
// WHY. Measured the 2026-10-25 (dc541b6): Ghidra covers 6806 of the 15136 code bytes, 45 %, and the
// largest hole is 7278 bytes from 0x600BC6 to 0x602833 -- nearly half the code in one block. All six
// dispatch targets are ignored. Ghidra follows calls and fall-through, but it does not recover these
// two tables, so everything reachable only through them is invisible to it.
//
// THE TWO TABLES, both read and checked before this script was written:
//
//   0x60088E, the MIDI status dispatch. snd_repartir_commande_midi does
//       lsr.b #2 ; andi.w #$1C          on the status byte -> 0, 4, 8 ... 0x1C
//       lea (0x60088E,PC,D0.w), A1
//       jmp (A1)                        at 0x60088C
//   so the table is EIGHT SLOTS OF FOUR BYTES, each holding a bra.w (the 0xFn slot holds the rts
//   those branches share). Seven distinct targets, all inside the code.
//
//   0x602730, the control-command dispatch for banks 0x00-0x0F. 0x602722 does
//       lsl.w #$1, d2
//       move.w $602730(pc,d2.w), d2     a WORD OFFSET relative to the table's own base
//       jmp $602730(pc,d2.w)            at 0x60272C
//   so it is SIXTEEN WORDS, and each target is 0x602730 + that word. Twelve distinct targets, all
//   inside the code (0x602750 appears four times, the unused-bank handler).
//
// WHAT THIS DOES. Disassembles each slot, adds a COMPUTED_JUMP reference from each jmp to each of
// its destinations so Ghidra's flow can follow them, creates a function at each distinct target,
// then re-runs analysis. It prints the function count AND the covered-bytes total before and after,
// because the count alone can rise while coverage does not.
//
// Ghidra's undo covers all of it. Nothing here asserts a fact: the before/after lines are the claim.
//
//@category FV2
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionIterator;
import ghidra.program.model.symbol.RefType;
import ghidra.program.model.symbol.SourceType;

import java.util.LinkedHashSet;

public class FV2SonTablesDeSaut extends GhidraScript {

	private static final long TABLE1 = 0x60088EL, JMP1 = 0x60088CL;   // 8 slots of 4 bytes, each a bra.w
	private static final long TABLE2 = 0x602730L, JMP2 = 0x60272CL;   // 16 words, offsets from TABLE2

	private long covered()
	{
		long n = 0;
		final FunctionIterator it = currentProgram.getFunctionManager().getFunctions(true);
		while (it.hasNext()) n += it.next().getBody().getNumAddresses();
		return n;
	}

	private void report(String when)
	{
		println(String.format("%-7s %d functions, %d bytes covered (%.0f %% of the 15136 of code)",
		                      when, currentProgram.getFunctionManager().getFunctionCount(), covered(),
		                      100.0 * covered() / 15136.0));
	}

	@Override
	public void run() throws Exception
	{
		report("before:");

		final LinkedHashSet<Long> targets = new LinkedHashSet<>();

		// --- table 1: eight slots of four bytes, each a bra.w. The slots themselves are the jmp's
		// destinations; disassembling them lets Ghidra see the branch and follow it.
		for (int k = 0; k < 8; k++) {
			final Address slot = toAddr(TABLE1 + 4L * k);
			disassemble(slot);
			currentProgram.getReferenceManager().addMemoryReference(
				toAddr(JMP1), slot, RefType.COMPUTED_JUMP, SourceType.USER_DEFINED, 0);
			targets.add(slot.getOffset());
		}
		println("table 1 at " + toAddr(TABLE1) + ": 8 slots referenced from the jmp at " + toAddr(JMP1));

		// --- table 2: sixteen words, each an offset from the table's own base.
		for (int k = 0; k < 16; k++) {
			final int off = getShort(toAddr(TABLE2 + 2L * k)) & 0xFFFF;
			final Address dest = toAddr(TABLE2 + off);
			disassemble(dest);
			currentProgram.getReferenceManager().addMemoryReference(
				toAddr(JMP2), dest, RefType.COMPUTED_JUMP, SourceType.USER_DEFINED, 0);
			targets.add(dest.getOffset());
		}
		println("table 2 at " + toAddr(TABLE2) + ": 16 words referenced from the jmp at " + toAddr(JMP2));

		int made = 0, already = 0;
		for (long t : targets) {
			final Address at = toAddr(t);
			if (getFunctionAt(at) != null) { already++; continue; }
			if (createFunction(at, null) != null) made++;
		}
		println(targets.size() + " distinct targets: " + made + " functions created, " + already + " already there");

		println("re-running analysis...");
		analyzeAll(currentProgram);
		report("after: ");
	}
}
