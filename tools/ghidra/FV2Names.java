// Apply names and plate comments to the Fighting Vipers 2 program image, from a
//     address,name,comment
// file (one row per line, the comment running to the end of the line).
//
// A state handler is a switch case inside its dispatcher, not a called function,
// so there is nothing there for Ghidra to have cut a function at. This makes one
// anyway: each state then decompiles on its own, which is the point -- the whole
// dispatcher as one function is thousands of lines and tells you nothing.
//
// @category FV2
import ghidra.app.cmd.function.CreateFunctionCmd;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.CodeUnit;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.SourceType;

import java.io.BufferedReader;
import java.io.FileReader;

public class FV2Names extends GhidraScript {

	@Override
	public void run() throws Exception {
		String[] args = getScriptArgs();
		if (args.length < 1) {
			println("usage: FV2Names.java <address,name,comment file>");
			return;
		}
		int renamed = 0, made = 0, commented = 0, failed = 0;
		try (BufferedReader r = new BufferedReader(new FileReader(args[0]))) {
			String line;
			while ((line = r.readLine()) != null) {
				line = line.trim();
				if (line.isEmpty() || line.startsWith("#"))
					continue;
				String[] p = line.split(",", 3);
				if (p.length < 2)
					continue;
				Address a = toAddr(Long.parseLong(p[0].trim(), 16));
				Function f = getFunctionAt(a);
				if (f == null) {
					CreateFunctionCmd cmd = new CreateFunctionCmd(a);
					if (cmd.applyTo(currentProgram, monitor))
						made++;
					f = getFunctionAt(a);
				}
				if (f == null) {
					println("FV2Names: no function could be made at " + a + " (" + p[1] + ")");
					failed++;
					continue;
				}
				f.setName(p[1].trim(), SourceType.USER_DEFINED);
				renamed++;
				if (p.length > 2 && !p[2].trim().isEmpty()) {
					currentProgram.getListing().setComment(a, CodeUnit.PLATE_COMMENT, p[2].trim());
					commented++;
				}
			}
		}
		println("FV2Names: " + renamed + " named (" + made + " functions made), "
				+ commented + " commented, " + failed + " failed");
	}
}
