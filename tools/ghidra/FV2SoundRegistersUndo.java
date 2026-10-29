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
			"lea (0x00100000).l, A5 -- ET C'EST TOUT CE QUE CETTE INSTRUCTION DIT.\n\n"
			+ "Un commentaire pose ici le 2026-10-24 affirmait que A5 tient la base des registres du premier\n"
			+ "SCSP pour TOUT le programme, et que le dire a Ghidra remonterait son compte de fonctions. Les\n"
			+ "deux moities etaient fausses, mesurees le 2026-10-25 :\n\n"
			+ "  A5 est ecrit 33 fois et A6 8 fois (balayage d'opcodes corrige : le premier avait un masque\n"
			+ "  qui effacait les bits du registre et rendait zero ecriture, ce qui ressemblait a une preuve).\n"
			+ "  A5 prend 0x00100000 par lea en sept endroits, mais aussi [0xC10] -- le decalage du chip, 0 ou\n"
			+ "  0x200000 -- en trois, zero en deux, et un pointeur indirect dans une vingtaine d'autres. En\n"
			+ "  0x600B9C il prend l'adresse des registres D'UNE FENTE. Ce sont des registres d'adresse\n"
			+ "  ordinaires, pas des bases globales.\n\n"
			+ "  Et poser les deux valeurs sur toute la plage puis relancer l'analyse a donne : 41 fonctions\n"
			+ "  avant, 41 apres. +0. Les valeurs ont ete retirees (FV2SoundRegistersUndo.java).\n\n"
			+ "Pourquoi l'erreur a tenu : elle vient de CETTE instruction seule, generalisee, alors que les\n"
			+ "notes de la meme session citaient deja movea.l $c10.w, a5 en 0x602B32 -- le contre-exemple etait\n"
			+ "en main et n'a pas ete lu.\n\n"
			+ "Ce qui reste donc ouvert, et sans explication : Ghidra trouve 41 fonctions la ou un marcheur\n"
			+ "seme a la main en atteint 95, dans un code que 139 rts decoupent. Les tables de saut\n"
			+ "(jmp (A1) en 0x60088C, jmp $602730(pc,d2.w) en 0x60272C) sont une cause plausible et NON\n"
			+ "VERIFIEE. Voir docs/fv2-sound-program.md.";
		setPlateComment(toAddr(0x600142), correction);
		println("plate comment at 0x600142 replaced with the correction");
	}
}
