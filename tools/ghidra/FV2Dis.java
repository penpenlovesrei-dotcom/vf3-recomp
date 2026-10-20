// Disassemble a stretch of the Fighting Vipers 2 program image, with whatever
// Ghidra has worked out about each instruction's references printed beside it --
// which is how a `lis rX,0x59` / `lbz rY,-0x268(rX)` pair becomes a readable
// address, and how a switch's destinations show up.
//
//     <output file> <from>-<to> [<from>-<to> ...]
//
// @category FV2
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.CodeUnit;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.InstructionIterator;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.Symbol;

import java.io.FileWriter;
import java.io.PrintWriter;

public class FV2Dis extends GhidraScript {

	@Override
	public void run() throws Exception {
		String[] args = getScriptArgs();
		if (args.length < 2) {
			println("usage: FV2Dis.java <output> <from>-<to> [...]");
			return;
		}
		PrintWriter w = new PrintWriter(new FileWriter(args[0]));
		int n = 0;
		for (int i = 1; i < args.length; i++) {
			String[] p = args[i].split("-");
			Address lo = toAddr(Long.parseLong(p[0], 16));
			Address hi = toAddr(Long.parseLong(p[1], 16));
			w.println("// ---- " + lo + " .. " + hi + " ----");
			InstructionIterator it = currentProgram.getListing().getInstructions(lo, true);
			while (it.hasNext()) {
				Instruction in = it.next();
				if (in.getAddress().compareTo(hi) > 0)
					break;
				Function f = getFunctionAt(in.getAddress());
				if (f != null)
					w.println("\n" + f.getName() + ":");
				Symbol[] labels = currentProgram.getSymbolTable().getSymbols(in.getAddress());
				for (Symbol s : labels)
					if (f == null || !s.getName().equals(f.getName()))
						w.println("  " + s.getName() + ":");

				StringBuilder sb = new StringBuilder();
				sb.append(String.format("  %s  %-10s %-30s", in.getAddress(),
						bytesOf(in), in.toString()));
				// Where it points, as the analysis resolved it.
				StringBuilder refs = new StringBuilder();
				for (Reference r : in.getReferencesFrom()) {
					if (r.getToAddress() == null)
						continue;
					Symbol s = currentProgram.getSymbolTable().getPrimarySymbol(r.getToAddress());
					refs.append(refs.length() == 0 ? "  ; " : ", ");
					refs.append(r.getReferenceType()).append(" ")
							.append(s != null ? s.getName() : r.getToAddress().toString());
				}
				sb.append(refs);
				String c = currentProgram.getListing().getComment(CodeUnit.EOL_COMMENT, in.getAddress());
				if (c != null)
					sb.append("   // ").append(c);
				w.println(sb);
				n++;
			}
			w.println();
		}
		w.close();
		println("FV2Dis: " + n + " instructions written to " + args[0]);
	}

	private String bytesOf(Instruction in) throws Exception {
		StringBuilder s = new StringBuilder();
		for (byte b : in.getBytes())
			s.append(String.format("%02X", b));
		return s.toString();
	}
}
