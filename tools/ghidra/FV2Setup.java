// FV2Setup: prepare the Fighting Vipers 2 program image for reading.
//
// The image is the game's own program as the boot stub leaves it in RAM at 0
// (vf3run --ram-at, re-swapped to big-endian). Raw binary has no entry point and
// no symbols, so this script supplies both:
//
//   1. code at every address in the recompiler's map (src/rec/fv2_map.cpp), which is
//      the set of words the program is actually entered at -- measured by this port's
//      own interpreter, not guessed -- so the disassembly covers what really runs;
//   2. a FUNCTION only where the program actually calls, which is not the same set:
//      6331 of the map's 7196 entries are branch targets inside a function, and
//      cutting a function at each of those chops real functions into fragments;
//   3. Sega's own variable names, read out of the game's debug-menu tables.
//
// Three files, as arguments:
//   analyzeHeadless ... -postScript FV2Setup.java <map.txt> <calls.txt> <symbols_dbg.csv>

import ghidra.app.script.GhidraScript;
import ghidra.app.cmd.disassemble.DisassembleCommand;
import ghidra.app.cmd.function.CreateFunctionCmd;
import ghidra.program.model.address.Address;
import ghidra.program.model.address.AddressSet;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.SourceType;

import java.io.BufferedReader;
import java.io.FileReader;
import java.util.ArrayList;
import java.util.List;

public class FV2Setup extends GhidraScript {

	@Override
	public void run() throws Exception {
		String[] args = getScriptArgs();
		if (args.length < 3) {
			println("FV2Setup: need <map.txt> <calls.txt> <symbols.csv>");
			return;
		}

		List<Long> map = readAddresses(args[0]);
		List<Long> calls = readAddresses(args[1]);
		println("FV2Setup: " + map.size() + " entry points from the recompiler's map, "
				+ calls.size() + " of them call targets");

		// Disassemble from everything the map knows, so the coverage is the measured one.
		AddressSet starts = new AddressSet();
		for (long a : map)
			starts.add(toAddr(a));
		for (long a : calls)
			starts.add(toAddr(a));
		monitor.setMessage("disassembling from the map");
		DisassembleCommand dis = new DisassembleCommand(starts, null, true);
		dis.applyTo(currentProgram, monitor);
		println("FV2Setup: disassembled " + currentProgram.getListing().getNumInstructions() + " instructions");

		// Cut functions only where the program calls.
		int made = 0, had = 0;
		for (long a : calls) {
			if (monitor.isCancelled())
				break;
			Address addr = toAddr(a);
			Function f = getFunctionAt(addr);
			if (f != null) {
				had++;
				continue;
			}
			CreateFunctionCmd cmd = new CreateFunctionCmd(addr);
			if (cmd.applyTo(currentProgram, monitor))
				made++;
		}
		println("FV2Setup: " + made + " functions created, " + had + " already there");

		// Sega's names are RAM addresses, and most of them are above the program image:
		// the work areas at 0x580000 and 0x700000, the NVRAM copy at 0x11F000. The image
		// loads as one block covering 0..0x29381E, so the rest of the board's 8 MB of RAM
		// has to exist as an uninitialised block before a label can land in it.
		Address progEnd = currentProgram.getMaxAddress().add(1);
		if (progEnd.getOffset() < 0x800000L) {
			currentProgram.getMemory().createUninitializedBlock(
					"ram", progEnd, 0x800000L - progEnd.getOffset(), false);
			println("FV2Setup: RAM block added, " + progEnd + " to 007FFFFF");
		}

		int named = 0, outside = 0;
		try (BufferedReader r = new BufferedReader(new FileReader(args[2]))) {
			String line;
			while ((line = r.readLine()) != null) {
				line = line.trim();
				if (line.isEmpty() || line.startsWith("#"))
					continue;
				int c = line.indexOf(',');
				if (c < 0)
					continue;
				long a = Long.parseLong(line.substring(0, c), 16);
				String name = line.substring(c + 1).trim();
				Address addr = toAddr(a);
				if (currentProgram.getMemory().contains(addr)) {
					createLabel(addr, name, true, SourceType.IMPORTED);
					named++;
				} else {
					outside++;
				}
			}
		}
		println("FV2Setup: " + named + " names applied, " + outside + " outside the image's memory");
	}

	private List<Long> readAddresses(String path) throws Exception {
		List<Long> out = new ArrayList<>();
		try (BufferedReader r = new BufferedReader(new FileReader(path))) {
			String line;
			while ((line = r.readLine()) != null) {
				line = line.trim();
				if (line.isEmpty() || line.startsWith("#"))
					continue;
				out.add(Long.parseLong(line, 16));
			}
		}
		return out;
	}
}
