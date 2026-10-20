// Decompile the requested functions of the Fighting Vipers 2 program image and
// write their C to a file. Ghidra headless analyses but does not decompile on its
// own; a script has to call the decompiler.
//
//     <output file> <address> [<address> ...]
//
// or, to take a whole stretch of the program:
//
//     <output file> range:<from>-<to>
//
// Each address is looked up INSIDE its function (getFunctionContaining), not at a
// function start, because an interesting address is usually an instruction the
// measurement named rather than an entry point.
//
// @category FV2
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionIterator;

import java.io.FileWriter;
import java.io.PrintWriter;
import java.util.ArrayList;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Set;

public class FV2Decomp extends GhidraScript {

	@Override
	public void run() throws Exception {
		String[] args = getScriptArgs();
		if (args.length < 2) {
			println("usage: FV2Decomp.java <output> <address>|range:<from>-<to> [...]");
			return;
		}
		DecompInterface deco = new DecompInterface();
		if (!deco.openProgram(currentProgram)) {
			println("the decompiler refuses the program: " + deco.getLastMessage());
			return;
		}

		Set<Function> want = new LinkedHashSet<>();
		List<String> unresolved = new ArrayList<>();
		for (int i = 1; i < args.length; i++) {
			String s = args[i];
			if (s.startsWith("range:")) {
				String[] p = s.substring(6).split("-");
				Address lo = toAddr(Long.parseLong(p[0], 16));
				Address hi = toAddr(Long.parseLong(p[1], 16));
				FunctionIterator it = currentProgram.getFunctionManager().getFunctions(lo, true);
				while (it.hasNext()) {
					Function f = it.next();
					if (f.getEntryPoint().compareTo(hi) > 0)
						break;
					want.add(f);
				}
				continue;
			}
			Address a = toAddr(Long.parseLong(s, 16));
			Function f = getFunctionContaining(a);
			if (f == null)
				f = createFunction(a, null);
			if (f == null) {
				unresolved.add(s);
				continue;
			}
			want.add(f);
		}

		PrintWriter w = new PrintWriter(new FileWriter(args[0]));
		for (String s : unresolved)
			w.println("// " + s + ": no function there");
		int ok = 0, failed = 0;
		for (Function f : want) {
			if (monitor.isCancelled())
				break;
			w.println("// ===================================================");
			w.println("// " + f.getEntryPoint() + "  " + f.getName()
					+ "  (" + f.getBody().getNumAddresses() + " bytes)");
			w.println("// ===================================================");
			DecompileResults r = deco.decompileFunction(f, 180, monitor);
			if (r.decompileCompleted()) {
				w.println(r.getDecompiledFunction().getC());
				ok++;
			} else {
				w.println("// the decompiler failed: " + r.getErrorMessage());
				failed++;
			}
		}
		w.close();
		println("FV2Decomp: " + ok + " functions written, " + failed + " failed, to " + args[0]);
	}
}
