// FV2SonSautsIndirects: resolve the five `jmp (An)` that were called unresolvable.
//
// WHY. FV2SonTablesDeSaut.java enumerated eleven computed jumps, opened four tables, and wrote that
// the remaining seven `jmp (An)` "need the register's value and nothing static resolves them". That
// sentence was wrong on all seven. Measured the 2026-10-25: EVERY ONE of them is preceded, four
// bytes above, by a `lea <base>(pc,Dn.w),An` that names the destination outright. The mistake was
// reading "jmp (An)" as the whole instruction when the instruction BEFORE it carried the answer.
//
// Two were handled by FV2SonDonnees.java (0x601F3A, 0x6023E0). These are the other five.
//
// THREE ARE BRANCH-SLOT TABLES, the same shape as table 1 at 0x60088E -- and the index mask gives
// the slot count directly, which is better than the self-sizing rule because it is written in the
// code:
//
//   0x600308  lea 0x60030A(pc,D1.w),A1   D1 = <val> & 0x0E        8 slots of 2  (bra.b)
//   0x603494  lea 0x603496(pc,D0.w),A0   D0 = (D1>>2) & 0x1C      8 slots of 4  (bra.w)
//   0x60376A  lea 0x60376C(pc,D0.w),A4   D0 = (D0 & 7) << 2       8 slots of 4  (bra.w)
//
// And the dumps hold two surprises worth recording, because both would have been mis-sized by a
// rule that only counted branches:
//
//   At 0x60030A only SEVEN slots are `bra.b`. The eighth, 0x600318, is already ordinary code
//   (`movea.l (0xC10).w,A2`) -- the last case's handler sits right there instead of behind a branch.
//   At 0x60376C only SEVEN are `bra.w`. The eighth, 0x603788, is `move.w #0x2000,SR ; rts` in line,
//   and slot 6 branches to it. Table 1 does the same thing with the rts its slots share.
//
// So: eight destinations each, seven branches and one in-line handler. The script checks each slot's
// opcode and SAYS which ones are not branches rather than assuming.
//
// TWO ARE UNROLLED CLEAR BLOCKS, Duff's devices like the copies FV2SonDonnees handles -- but they
// CLEAR, they do not copy:
//
//   0x602042  lea 0x602044(pc,D5.w),A0   128 x `clr.l (A2)+`
//   0x6024E8  lea 0x6024EA(pc,D5.w),A0   128 x `clr.l (A2)+`
//
// Measured: 128 instructions each, 0x100 bytes of block, 512 bytes processed at most. And the
// arithmetic of the pair closes. For the copy at 0x601F36, D0 = (0x200 - D5) >> 1, so the entry skips
// (0x200-D5)/4 instructions and 128 - (0x200-D5)/4 = D5/4 of them run, copying D5 BYTES. For the
// clear, the index is D5>>1, so D5/4 instructions are skipped and 128 - D5/4 run, clearing
// 512 - D5 BYTES. One 512-byte destination: D5 bytes from (A1), zeros for the rest. The constant
// 0x200 in `move.w #$200,D0` is that buffer, and it appears twice.
//
// WHAT IT DOES AND DOES NOT DO. It adds the missing COMPUTED_JUMP references and disassembles, so
// the decompiler stops seeing a jump to nowhere. It does NOT create functions at the slot targets:
// coverage of this program is already 100 % (FV2SonDonnees), those targets are inside existing
// bodies, and cutting a function in two at a branch target would make the listing worse, not better.
//
//@category FV2
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.symbol.RefType;
import ghidra.program.model.symbol.SourceType;

public class FV2SonSautsIndirects extends GhidraScript {

	// le jmp, la base, le nombre de creneaux, le pas, et ce que le masque de l'index vaut
	private static final Object[][] TABLES_DE_BRANCHEMENT = {
		{ 0x600308L, 0x60030AL, 8, 2, "D1 & 0x0E, donc huit creneaux de deux octets" },
		{ 0x603494L, 0x603496L, 8, 4, "(D1 >> 2) & 0x1C, donc huit creneaux de quatre octets" },
		{ 0x60376AL, 0x60376CL, 8, 4, "(D0 & 7) << 2, donc huit creneaux de quatre octets" },
	};

	// le jmp, l'entree du bloc, le nombre d'instructions attendu, le motif de deux octets
	private static final Object[][] BLOCS_DEROULES = {
		{ 0x602042L, 0x602044L, 128, 0x429A, "clr.l (A2)+" },
		{ 0x6024E8L, 0x6024EAL, 128, 0x429A, "clr.l (A2)+" },
	};

	private boolean estBranchement(long a) throws Exception
	{
		final int mot = getShort(toAddr(a)) & 0xFFFF;
		if ((mot & 0xFF00) != 0x6000) return false;         // pas un bra
		return true;                                        // bra.b ou bra.w, les deux conviennent
	}

	@Override
	public void run() throws Exception
	{
		int refs = 0;

		for (Object[] t : TABLES_DE_BRANCHEMENT) {
			final long jmp = (Long) t[0], base = (Long) t[1];
			final int n = (Integer) t[2], pas = (Integer) t[3];
			int branchements = 0;
			final StringBuilder enLigne = new StringBuilder();
			for (int k = 0; k < n; k++) {
				final long a = base + (long) pas * k;
				final Address at = toAddr(a);
				disassemble(at);
				currentProgram.getReferenceManager().addMemoryReference(
					toAddr(jmp), at, RefType.COMPUTED_JUMP, SourceType.USER_DEFINED, 0);
				refs++;
				if (estBranchement(a)) branchements++;
				else enLigne.append(String.format(" %08X", a));
			}
			println(String.format("%08X -> %08X : %d creneaux de %d octets, %d bra%s",
			                      jmp, base, n, pas, branchements,
			                      enLigne.length() == 0 ? ""
			                      : ", et en ligne (pas un bra) :" + enLigne));
			setPlateComment(toAddr(base), String.format(
				"Table de branchement du jmp (An) de %08X, %d creneaux de %d octets.%n%n"
				+ "L'index est %s -- le masque donne le compte, il est ecrit dans le code.%n%n"
				+ "Ce jmp etait compte parmi les sept \"que rien de statique ne resout\" : faux. Le lea%n"
				+ "quatre octets au-dessus nomme cette base. Les sept l'avaient.",
				jmp, n, pas, (String) t[4]));
		}

		for (Object[] d : BLOCS_DEROULES) {
			final long jmp = (Long) d[0], entree = (Long) d[1];
			final int attendu = (Integer) d[2], motif = (Integer) d[3];
			int n = 0;
			while ((getShort(toAddr(entree + 2L * n)) & 0xFFFF) == motif) n++;
			disassemble(toAddr(entree));
			currentProgram.getReferenceManager().addMemoryReference(
				toAddr(jmp), toAddr(entree), RefType.COMPUTED_JUMP, SourceType.USER_DEFINED, 0);
			refs++;
			println(String.format("%08X -> %08X : %d x %s%s", jmp, entree, n, (String) d[4],
			                      n == attendu ? "" : "   *** " + attendu + " attendus"));
			setPlateComment(toAddr(entree), String.format(
				"Bloc deroule de %d x %s, entre a un decalage calcule : lea %08X(pc,D5.w),A0 puis%n"
				+ "jmp (A0), avec D5 deja divise par deux. %d instructions, 0x%X octets de bloc,%n"
				+ "512 octets traites au plus.%n%n"
				+ "Il EFFACE, il ne copie pas -- et il va par paire avec une copie deroulee. Pour la%n"
				+ "copie, D0 = (0x200 - D5) >> 1 : D5/4 instructions tournent, donc D5 OCTETS copies.%n"
				+ "Pour cet effacement, l'index est D5>>1 : 128 - D5/4 tournent, donc 512 - D5 OCTETS%n"
				+ "effaces. Une destination de 512 octets : D5 octets depuis (A1), des zeros pour le%n"
				+ "reste. Le 0x200 de move.w #$200,D0 est ce tampon, et il apparait deux fois.%n%n"
				+ "Ce que les 512 octets SONT n'est pas etabli ici.",
				n, (String) d[4], entree, n, 2 * n));
		}

		println("");
		println(refs + " references de saut calcule ajoutees. Aucune fonction creee : la couverture");
		println("est deja de 100 % et ces cibles sont dans des corps existants.");
		println("Il ne reste AUCUN jmp (An) non resolu dans ce programme.");
	}
}
