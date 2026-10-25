// FV2SonListe: print every function Ghidra has in the sound program, one address a line.
//
// Written to settle a comparison rather than repeat it: "Ghidra finds 41 where a hand-seeded
// walker reaches 95" was stated as a gap needing an explanation, but the two numbers may not count
// the same thing. The walker (tools/son/desas_son.py) calls every bsr/jsr target a function, and a
// bsr into the middle of a real function would be counted there and not here. This dumps Ghidra's
// side so the two sets can be compared instead of their sizes.
//
// Read-only: it changes nothing.
//
//@category FV2
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.AddressRange;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionIterator;

public class FV2SonListe extends GhidraScript {

	@Override
	public void run() throws Exception
	{
		int n = 0;
		long ranges = 0;
		final StringBuilder sb = new StringBuilder();
		final FunctionIterator it = currentProgram.getFunctionManager().getFunctions(true);
		while (it.hasNext()) {
			final Function f = it.next();
			sb.append(String.format("FONCTION %08X %d %s%n",
			                        f.getEntryPoint().getOffset(), f.getBody().getNumAddresses(), f.getName()));
			// A body is an address SET and may be FRAGMENTED, so the ranges are printed one a line.
			// Treating a body as [entry, entry + count) is wrong for a fragmented one, and the hole
			// list computed that way is wrong with it.
			for (AddressRange r : f.getBody()) {
				sb.append(String.format("PLAGE %08X %08X%n",
				                        r.getMinAddress().getOffset(), r.getMaxAddress().getOffset()));
				ranges++;
			}
			n++;
		}
		println("total " + n + " fonctions, " + ranges + " plages");
		// println, not print: print carries no "<script>.java>" prefix, and a filter looking for one
		// swallows the whole dump while the total still arrives. That happened on 2026-10-25.
		for (String line : sb.toString().split("\\R"))
			println(line);
	}
}
