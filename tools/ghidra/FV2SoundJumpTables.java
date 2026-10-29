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

	private static final long CODE_FIN = 0x603B20L;
	private static final double CODE_OCTETS = 15136.0;

	// the bra.w table: the jmp, the base, the slot count
	private static final long T1_JMP = 0x60088CL, T1_BASE = 0x60088EL;
	private static final int T1_SLOTS = 8;

	// the word-offset tables: the jmp, the base, and the count expected when it is already known
	private static final long[][] WORD_TABLES = {
		{ 0x60272CL, 0x602730L, 16 },   // known by hand: 16. The check.
		{ 0x601418L, 0x60141CL, -1 },
		{ 0x602EDCL, 0x602EE0L, -1 },
	};

	private long couvert()
	{
		long n = 0;
		final FunctionIterator it = currentProgram.getFunctionManager().getFunctions(true);
		while (it.hasNext()) n += it.next().getBody().getNumAddresses();
		return n;
	}

	private void etat(String quand)
	{
		println(String.format("%-8s %d fonctions, %d octets couverts (%.0f %% des %.0f de code)",
		                      quand, currentProgram.getFunctionManager().getFunctionCount(),
		                      couvert(), 100.0 * couvert() / CODE_OCTETS, CODE_OCTETS));
	}

	@Override
	public void run() throws Exception
	{
		etat("avant :");
		final LinkedHashSet<Long> cibles = new LinkedHashSet<>();

		// --- table 1: the slots themselves are the destinations; disassembling them lets Ghidra see
		// each bra.w and follow it.
		for (int k = 0; k < T1_SLOTS; k++) {
			final Address slot = toAddr(T1_BASE + 4L * k);
			disassemble(slot);
			currentProgram.getReferenceManager().addMemoryReference(
				toAddr(T1_JMP), slot, RefType.COMPUTED_JUMP, SourceType.USER_DEFINED, 0);
			cibles.add(slot.getOffset());
		}
		println(String.format("table 1 %08X : %d creneaux de bra.w, references depuis le jmp de %08X",
		                      T1_BASE, T1_SLOTS, T1_JMP));

		// --- the word-offset tables, each sized by its own lowest target
		for (long[] t : WORD_TABLES) {
			final long jmp = t[0], base = t[1];
			final long attendu = t[2];
			long plusBasse = Long.MAX_VALUE;
			int n = 0;
			while (n <= 256) {
				final long pos = base + 2L * n;
				if (pos >= plusBasse)
					break;
				final long dest = base + (getShort(toAddr(pos)) & 0xFFFFL);
				if (dest < 0x600000L || dest >= CODE_FIN)
					break;
				plusBasse = Math.min(plusBasse, dest);
				n++;
			}
			if (attendu > 0 && n != attendu) {
				println(String.format("*** table %08X : la regle donne %d entrees, %d comptees a la main."
				                      + " Le script s'arrete plutot que de dimensionner les autres a l'aveugle.",
				                      base, n, attendu));
				return;
			}
			final LinkedHashSet<Long> miennes = new LinkedHashSet<>();
			for (int k = 0; k < n; k++) {
				final long dest = base + (getShort(toAddr(base + 2L * k)) & 0xFFFFL);
				miennes.add(dest);
			}
			for (long dest : miennes) {
				final Address at = toAddr(dest);
				disassemble(at);
				currentProgram.getReferenceManager().addMemoryReference(
					toAddr(jmp), at, RefType.COMPUTED_JUMP, SourceType.USER_DEFINED, 0);
			}
			cibles.addAll(miennes);
			println(String.format("table   %08X : %3d entrees%s, %d cibles distinctes, depuis le jmp de %08X",
			                      base, n, attendu > 0 ? " (regle confirmee sur un compte fait a la main)" : "",
			                      miennes.size(), jmp));
		}

		int faites = 0, deja = 0;
		for (long t : cibles) {
			final Address at = toAddr(t);
			if (getFunctionAt(at) != null) { deja++; continue; }
			if (createFunction(at, null) != null) faites++;
		}
		println(cibles.size() + " cibles distinctes en tout : " + faites + " fonctions creees, " + deja + " deja la");

		println("analyse relancee...");
		analyzeAll(currentProgram);
		etat("apres :");
		println("Restent cinq jmp (An) sans resolution statique : 0x600308, 0x602042, 0x6024E8,");
		println("0x603494, 0x60376A. Les deux autres -- 0x601F3A et 0x6023E0 -- sont des entrees de");
		println("bloc deroule qu'un lea nomme, et FV2SoundData.java les traite.");
	}
}
