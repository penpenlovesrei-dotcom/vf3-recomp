// Bound each state handler's function body to exactly its own code, from an
//     address,end
// file (end exclusive).
//
// A handler is a switch case: it ends with an unconditional branch to the shared
// epilogue, not with a return, so Ghidra's own body-walk runs straight on into the
// next case and the one after that -- a 160-byte handler decompiles as 4455 lines
// of the whole chain. With the body set, each handler decompiles as itself and the
// branch out shows as the jump it is.
//
// @category FV2
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.address.AddressSet;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.SourceType;

import java.io.BufferedReader;
import java.io.FileReader;

public class FV2Body extends GhidraScript {

	@Override
	public void run() throws Exception {
		String[] args = getScriptArgs();
		if (args.length < 1) {
			println("usage: FV2Body.java <address,end file>");
			return;
		}
		int set = 0, missing = 0, clashed = 0;
		try (BufferedReader r = new BufferedReader(new FileReader(args[0]))) {
			String line;
			while ((line = r.readLine()) != null) {
				line = line.trim();
				if (line.isEmpty() || line.startsWith("#"))
					continue;
				String[] p = line.split(",");
				if (p.length < 2)
					continue;
				Address a = toAddr(Long.parseLong(p[0].trim(), 16));
				Address end = toAddr(Long.parseLong(p[1].trim(), 16) - 1);
				Function f = getFunctionAt(a);
				if (f == null) {
					missing++;
					continue;
				}
				try {
					f.setBody(new AddressSet(a, end));
					set++;
				} catch (Exception e) {
					// Another function already holds part of that range: say which and
					// carry on, so one bad bound does not cost all the others.
					println("FV2Body: " + a + " kept its own body: " + e.getMessage());
					clashed++;
				}
			}
		}
		println("FV2Body: " + set + " bodies set, " + missing + " with no function, " + clashed + " clashing");
	}
}
