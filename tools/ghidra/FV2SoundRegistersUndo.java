// FV2SoundRegistersUndo: take back what FV2SonRegistres.java set, because its premise was false.
//
// FV2SonRegistres (committed as fd74c38) set A5 = 0x00100000 and A6 = 0x1000 over the whole sound
// program, on the claim that the driver sets them once at reset and never touches them again. THAT
// CLAIM IS WRONG. Measured the 2026-10-25 with a corrected opcode scan: A5 is written 33 times and
// A6 8 times. A5 takes 0x00100000 from `lea` in seven places, but also [0xC10] (the chip offset, 0
// or 0x200000) in three, zero in two, and a register-indirect pointer in some twenty more. They are
// ordinary scratch address registers, not global bases.
//
// The claim came from one `lea` at reset, generalised while the notes of the same session had
// already quoted `movea.l $c10.w, a5` at 0x602B32 -- the counter-example was in hand and not read.
//
// And the intervention bought nothing: 41 functions before, 41 after. So it leaves only a false
// constant in the program database, which the decompiler will apply in the twenty-six places A5 is
// something else. This removes it.
//
//@category FV2
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.lang.Register;
import ghidra.program.model.listing.ProgramContext;

public class FV2SoundRegistersUndo extends GhidraScript {

	@Override
	public void run() throws Exception
	{
		final Register a5 = currentProgram.getRegister("A5");
		final Register a6 = currentProgram.getRegister("A6");
		if (a5 == null || a6 == null) {
			println("A5 or A6 is not a register of " + currentProgram.getLanguageID());
			return;
		}
		final Address first = currentProgram.getMinAddress();
		final Address last = currentProgram.getMaxAddress();
		final ProgramContext ctx = currentProgram.getProgramContext();

		println("before: A5 at 0x600B96 = " + ctx.getValue(a5, toAddr(0x600b96), false)
		        + " ; A6 at 0x601CD8 = " + ctx.getValue(a6, toAddr(0x601cd8), false));

		ctx.remove(first, last, a5);
		ctx.remove(first, last, a6);

		println("after:  A5 at 0x600B96 = " + ctx.getValue(a5, toAddr(0x600b96), false)
		        + " ; A6 at 0x601CD8 = " + ctx.getValue(a6, toAddr(0x601cd8), false)
		        + "   (null is what we want)");
		println("functions: " + currentProgram.getFunctionManager().getFunctionCount()
		        + "   (41 before the whole exercise, 41 after it, +0)");

		// And the plate comment at the reset `lea` said the same false thing. Replace it with what
		// was measured, so the project does not keep carrying the claim.
		final String correction =
			"lea (0x00100000).l, A5 -- AND THAT IS ALL THIS INSTRUCTION SAYS.\n\n"
			+ "A comment laid here on 2026-10-24 claimed that A5 holds the first SCSP's register base for\n"
			+ "the WHOLE program, and that telling Ghidra so would raise its function count. Both halves\n"
			+ "were false, measured on 2026-10-25:\n\n"
			+ "  A5 is written 33 times and A6 8 times (corrected opcode scan: the first had a mask that\n"
			+ "  erased the register's bits and returned zero writes, which looked like a proof).\n"
			+ "  A5 takes 0x00100000 from lea in seven places, but also [0xC10] -- the chip offset, 0 or\n"
			+ "  0x200000 -- in three, zero in two, and a register-indirect pointer in some twenty more. At\n"
			+ "  0x600B9C it takes the register address OF ONE SLOT. These are ordinary address registers,\n"
			+ "  not global bases.\n\n"
			+ "  And laying both values over the whole range and running the analysis again gave: 41\n"
			+ "  functions before, 41 after. +0. The values were removed (FV2SoundRegistersUndo.java).\n\n"
			+ "Why the error held: it comes from THIS instruction alone, generalised, while the same\n"
			+ "session's notes already quoted movea.l $c10.w, a5 at 0x602B32 -- the counter-example was in\n"
			+ "hand and was not read.\n\n"
			+ "What stays open, then, with no explanation: Ghidra finds 41 functions where a walker seeded\n"
			+ "by hand reaches 95, in code that 139 rts cut up. The jump tables (jmp (A1) at 0x60088C,\n"
			+ "jmp $602730(pc,d2.w) at 0x60272C) are a plausible and UNVERIFIED cause. See\n"
			+ "docs/fv2-sound-program.md.";
		setPlateComment(toAddr(0x600142), correction);
		println("plate comment at 0x600142 replaced with the correction");
	}
}
