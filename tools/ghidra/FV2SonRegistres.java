// FV2SonRegistres: tell Ghidra what A5 and A6 hold, for the whole sound program.
//
// Why this exists. The sound 68000's program keeps two base registers set once at reset and never
// touched again (docs/fv2-son-programme.md section 2):
//
//     00600142  lea  (0x00100000).l, A5    the FIRST SCSP's register base
//     00600274  lea  (0x1000).w, A6        the sample-copy descriptors
//
// Ghidra is not told, so every `(d16,A5)` and `(d16,A6)` stays a register-relative access it cannot
// resolve, and its analysis stops early: 41 functions found in about 15 KB of code that holds 139
// rts. A hand-seeded walker reaches 95 with a far dumber tool, which is the tell.
//
// What this does. Sets A5 and A6 as known values over the whole program, then re-runs analysis so
// the resolved references open the flow Ghidra could not follow. It prints the function count
// before and after, because a script that changes a project without saying by how much is not a
// measurement.
//
// HOW TO RUN IT. Copy to %USERPROFILE%\ghidra_scripts (or add tools/ghidra to the Script Manager's
// paths), then in Ghidra: Window -> Script Manager, find FV2SonRegistres, Run. It needs the sound
// program open (C:\Temp\fv2-ghidra\FV2SON.gpr, fv2-son-68000.bin).
//
// It can be undone: Ghidra's undo covers both the register values and the analysis it triggered.
//
//@category FV2
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.lang.Register;
import ghidra.program.model.listing.ProgramContext;

import java.math.BigInteger;

public class FV2SonRegistres extends GhidraScript {

	// What the program's own reset code puts in them.
	private static final long A5_VALUE = 0x00100000L;   // lea (0x00100000).l, A5  at 0x600142
	private static final long A6_VALUE = 0x00001000L;   // lea (0x1000).w, A6      at 0x600274

	@Override
	public void run() throws Exception
	{
		final Register a5 = currentProgram.getRegister("A5");
		final Register a6 = currentProgram.getRegister("A6");
		if (a5 == null || a6 == null) {
			println("A5 or A6 is not a register of this language (" + currentProgram.getLanguageID()
			        + "). Is the sound program the open one?");
			return;
		}

		final Address first = currentProgram.getMinAddress();
		final Address last = currentProgram.getMaxAddress();
		final int before = currentProgram.getFunctionManager().getFunctionCount();

		final ProgramContext ctx = currentProgram.getProgramContext();
		ctx.setValue(a5, first, last, BigInteger.valueOf(A5_VALUE));
		ctx.setValue(a6, first, last, BigInteger.valueOf(A6_VALUE));

		println(String.format("A5 = %08X and A6 = %08X, set over %s - %s", A5_VALUE, A6_VALUE, first, last));
		println("read back at 0x600B96: A5 = " + ctx.getValue(a5, toAddr(0x600b96), false)
		        + " ; at 0x601CD8: A6 = " + ctx.getValue(a6, toAddr(0x601cd8), false));
		println("functions before: " + before);

		println("re-running analysis...");
		analyzeAll(currentProgram);

		final int after = currentProgram.getFunctionManager().getFunctionCount();
		println("functions after:  " + after + "   (" + (after - before >= 0 ? "+" : "") + (after - before) + ")");
		println("A hand-seeded walker reaches 95 on this program (tools/son/desas_son.py), which is the");
		println("number worth beating -- see docs/fv2-son-programme.md.");
	}
}
