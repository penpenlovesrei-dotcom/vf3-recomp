// FV2SoundData: account for the bytes Ghidra leaves outside every function body.
//
// WHY. After the four jump tables were opened (683f1bf) Ghidra held 168 functions covering 14484
// bytes, and "4 % of the program is still not decompiled" was the way that got stated. THE 4 % WAS
// A DENOMINATOR ERROR. Measured the 2026-10-25, the nine holes are 654 bytes, and 650 of them were
// never code:
//
//   600000-6000FF  256  la table des vecteurs du 68000 -- 64 longs, des donnees par construction
//   600100-60011F   32  UNE SIGNATURE : "Ver3.5.9|98  1/13|model3|by Hiro". Trente-deux octets qui
//                       finissent exactement ou le vecteur de reset envoie le processeur (0x600120).
//                       Le pilote porte sa version, sa date et son auteur.
//   6002D4-6002E7   20  dix mots, copies d'affilee dans les registres communs 0x418..0x42A du SCSP
//                       par la boucle de 0x600288 : lea 0x100418,A0 / lea 0x6002D4,A1 / moveq #9,D7
//                       / move.w (A1)+,(A0)+ / dbra. Ce sont les timers et les interruptions.
//   600396-6003B5   32  seize mots, lus par move.w (A1,D0.w) apres lea 0x600396,A1 en 0x60037E,
//                       index par D0 double, et ecrits dans un tampon (A0)+
//   60103C-60107B   64  trente-deux mots DECROISSANTS (0x1A00 -> 0x0001, l'entree 0 valant 1), lus
//                       en deux endroits -- 0x600CF6 et 0x6038DA -- toujours par
//                       move.w (table,Dn.w), 0x0E(A4), l'index etant un compteur incremente et
//                       range en 0x0A(A5). Une courbe de 32 pas indexee par un compteur.
//   60141C-6014BD  162  la table de saut 3 : 81 mots. 162/2 = 81, le compte que la regle
//                       d'auto-dimensionnement avait donne. Les deux mesures sont independantes.
//   602730-60274F   32  la table de saut 2 : 16 mots -- le compte fait a la main
//   602EE0-602F13   52  la table de saut 4 : 26 mots
//
// Les 4 octets qui restent sont du CODE, et le seul que Ghidra manquait vraiment :
//
//   601F3C, 6023E2   2 octets chacun : le PREMIER move.l (A1)+,(A2)+ d'un bloc deroule de 128,
//                    entre a un decalage calcule -- lea 0x601F3C(pc,D0.w),A0 puis jmp (A0), avec
//                    D0 = (0x200 - D5) >> 1. Un dispositif de Duff. Les deux etaient comptes parmi
//                    les "sept jmp (An) que rien de statique ne resout" : c'etait faux, un lea les
//                    nomme quatre octets plus haut. Il en reste cinq.
//
// ET LA FIN DU CODE N'ETAIT PAS LA BONNE : 0x603B20 dans mes constantes, alors que le rts de cette
// adresse occupe 0x603B20-0x603B21. Le code va donc a 0x603B22, ou commence une table d'octets
// croissants (80 81 .. 8b 90 .. 9b a0 ..). Avec la borne corrigee, couvert + trous = 15138 et
// l'arithmetique ferme ; avec l'ancienne elle donnait 15138 pour 15136.
//
// Donc : code reel 15138 - 650 = 14488 octets, dont Ghidra couvrait 14484. Quatre octets.
//
// Le script declare les donnees, rattache les quatre octets de code, puis RECOMPTE lui-meme ce qui
// reste hors de tout corps de fonction ET hors de toute donnee declaree, et l'imprime. Il ne se
// contente pas d'annoncer un pourcentage : il nomme ce qui manque encore, s'il manque quelque chose.
//
//@category FV2
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.address.AddressSet;
import ghidra.program.model.data.ArrayDataType;
import ghidra.program.model.data.PointerDataType;
import ghidra.program.model.data.WordDataType;
import ghidra.program.model.listing.Data;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionIterator;
import ghidra.program.model.symbol.RefType;
import ghidra.program.model.symbol.SourceType;

public class FV2SoundData extends GhidraScript {

	// La fin du code, le dernier rts compris. Pas 0x603B20.
	private static final long CODE_FIN = 0x603B22L;

	// adresse, nombre de mots, nom, commentaire
	private static final Object[][] TABLES_DE_MOTS = {
		{ 0x6002D4L, 10, "scsp_init_418_42A",
		  "Dix mots copies tels quels dans les registres communs 0x418 a 0x42A du SCSP 0, par la\n"
		  + "boucle de 0x600288 (lea 0x100418,A0 / lea 0x6002D4,A1 / moveq #9,D7 / move.w (A1)+,(A0)+).\n"
		  + "Dix mots, dix registres, dans l'ordre :\n\n"
		  + "  0x418 TIMA/TACTL = 0x00CE     0x424 SCILV0 = 0x0088\n"
		  + "  0x41A TIMB/TBCTL = 0x03C6     0x426 SCILV1 = 0x0048\n"
		  + "  0x41C TIMC/TCCTL = 0x0100     0x428 SCILV2 = 0x0000\n"
		  + "  0x41E SCIEB      = 0x00C8     0x42A MCIEB  = 0x0020\n"
		  + "  0x420 SCIPD      = 0x0000\n"
		  + "  0x422 SCIRE      = 0x00C8\n\n"
		  + "Ce que SCIEB = 0x00C8 dit, bits 3, 6 et 7 : entree MIDI, timer A et timer B armes. Le\n"
		  + "timer C ne l'est PAS. Les trois SCILV donnent le niveau par source, un bit chacun :\n"
		  + "MIDI (bit 3) prend 0b011 = 3, timer A (bit 6) prend 0b010 = 2, timer B (bit 7) prend\n"
		  + "0b001 = 1. MCIEB = 0x0020, bit 5, arme une source vers l'autre cote.\n\n"
		  + "CORRECTION du 2026-10-25 : la premiere version de ce commentaire nommait ces bits\n"
		  + "timer B et timer C, et disait le timer A non arme. C'etait l'inverse. Ce qui l'a\n"
		  + "revele : snd_irq2 (0x6032D2) recharge 0x418 -- TIMA -- et acquitte SCIRE bit 6, ce qui\n"
		  + "n'aurait aucun sens si le timer A n'etait pas arme. Les bits viennent desormais de\n"
		  + "l'implementation (src/sound/scsp.cpp : timerA_cb pose 0x40, timerB_cb 0x80, timerC_cb\n"
		  + "0x100, l'entree MIDI 8) et non d'un souvenir de table. Les NIVEAUX 3, 2, 1 etaient\n"
		  + "justes des le depart : seuls les noms des sources etaient permutes.\n\n"
		  + "Et la valeur se recoupe trois fois : TIMA = 0xCE ici, [0x60806A] = 0x00CE que snd_irq2\n"
		  + "recharge a chaque tic, et le commentaire du portage qui avait deja mesure 50 echantillons\n"
		  + "et 882 tics par seconde. C'est le tic de la musique.\n\n"
		  + "C'est une lecture, pas une mesure sur machine : elle vient de ces dix mots et de la\n"
		  + "disposition des registres du SCSP. Ce qu'elle predit et qui reste a verifier sur le\n"
		  + "portage : trois sources d'interruption et trois seulement, a ces trois niveaux." },

		{ 0x600396L, 16, "table_mots_600396",
		  "Seize mots lus par move.w (A1,D0.w) apres lea 0x600396,A1 en 0x60037E, l'index D0 etant\n"
		  + "double juste avant, et le mot ecrit dans un tampon (A0)+ avant un bra.w 0x6002EC.\n"
		  + "0x6002EC est un distributeur sur un flux d'octets : les branches voisines y font\n"
		  + "move.b (A2)+,(A0)+ une ou deux fois avant de revenir au meme endroit. Donc : seize\n"
		  + "valeurs de deux octets qu'un code a un chiffre fait emettre dans le flux de sortie.\n\n"
		  + "Les valeurs : 0010 0020 0400 0700 0A00 0E00 2022 4000 4002 7700 7800 7900 7C00 A000\n"
		  + "A040 A0A3. CE QU'ELLES SONT N'EST PAS ETABLI. Les nommer demanderait de lire le\n"
		  + "distributeur de 0x6002EC en entier, ce qui n'est pas fait." },

		{ 0x60103CL, 32, "courbe_32_pas_60103C",
		  "Trente-deux mots, et ils DECROISSENT : 0x1A00 0x1100 0x0C90 0x09A0 0x06B0 0x0450 0x02C0\n"
		  + "0x0200 0x0160 0x0100 0x00A8 0x0078 0x0050 0x0038 0x002C 0x0026 0x0023 0x0020 0x001D\n"
		  + "0x001A 0x0017 0x0014 0x0011 0x000E 0x000B 0x0008 0x0006 0x0004 0x0003 0x0002 0x0001,\n"
		  + "l'entree 0 valant 0x0001 a part.\n\n"
		  + "Lue en deux endroits, de la meme facon :\n"
		  + "  0x600CF6  lea 0x60103C,A2 ; move.w (A2,D5.w), 0x0E(A4)\n"
		  + "  0x6038DA  lea 0x60103C,A3 ; move.w (A3,D3.w), 0x0E(A4)\n"
		  + "et en 0x6038D2 l'index est un compteur : addq.b #1,D3 ; move.w D3,0x0A(A5) ; lsl.w #1,D3.\n"
		  + "Un compteur range en 0x0A(A5) avance d'un pas, et le mot correspondant va en 0x0E(A4).\n\n"
		  + "Trente-deux entrees pour un index de cinq bits, et une courbe qui tombe : cela RESSEMBLE\n"
		  + "a une duree par pas d'enveloppe ou de fondu logiciel. CE N'EST PAS ETABLI. Ce qui l'est :\n"
		  + "trente-deux mots decroissants, deux lecteurs, un compteur comme index, et 0x0E(A4) comme\n"
		  + "destination. Ce que vaut 0x0E dans la fiche pointee par A4 n'est pas identifie ici." },

		{ 0x60141CL, 81, "table_de_saut_3_mots",
		  "La table de saut 3, celle du jmp (d8,PC,D2.w) de 0x601418 : le sous-distributeur des\n"
		  + "changements de controle MIDI (Bn), par numero de controleur.\n\n"
		  + "81 mots, et ce compte est confirme DEUX FOIS, independamment : la regle\n"
		  + "d'auto-dimensionnement de FV2SoundJumpTables (la table finit ou sa cible la plus basse\n"
		  + "commence) en avait donne 81, et le trou que Ghidra laisse ici fait 162 octets, soit\n"
		  + "exactement 81 mots. Les deux mesures ne partagent rien." },

		{ 0x602730L, 16, "table_de_saut_2_mots",
		  "La table de saut 2, celle du jmp (d8,PC,D2.w) de 0x60272C : les commandes de controle,\n"
		  + "banques 0x00 a 0x0F. Seize mots -- le compte avait ete fait a la main, et le trou de\n"
		  + "32 octets le redonne." },

		{ 0x602EE0L, 26, "table_de_saut_4_mots",
		  "La table de saut 4, celle du jmp (d8,PC,D0.w) de 0x602EDC, sous la banque de controle 0x00.\n"
		  + "26 mots : 52 octets de trou, et la regle d'auto-dimensionnement disait 26." },
	};

	// Les entrees de bloc deroule : le jmp qui y va, l'entree, et les deux octets a rattacher.
	private static final long[][] DUFF = {
		{ 0x601F3AL, 0x601F3CL },
		{ 0x6023E0L, 0x6023E2L },
	};

	@Override
	public void run() throws Exception
	{
		// --- la table des vecteurs : 64 longs
		clearListing(toAddr(0x600000L), toAddr(0x6000FFL));
		createData(toAddr(0x600000L), new ArrayDataType(PointerDataType.dataType, 64, 4));
		setPlateComment(toAddr(0x600000L),
			"La table des vecteurs d'exception du 68000 : 64 longs, 256 octets. Des donnees par\n"
			+ "construction, et c'est d'elle que FV2SoundSetup.java tire ses points d'entree.");
		println("vecteurs 600000-6000FF : 64 pointeurs declares");

		// --- la signature
		clearListing(toAddr(0x600100L), toAddr(0x60011FL));
		createAsciiString(toAddr(0x600100L), 32);
		setPlateComment(toAddr(0x600100L),
			"LA SIGNATURE DU PILOTE : \"Ver3.5.9|98  1/13|model3|by Hiro\".\n\n"
			+ "Trente-deux octets, qui finissent exactement ou le vecteur de reset envoie le\n"
			+ "processeur : 0x600120. Le pilote porte sa version (3.5.9), sa date (98 1/13), sa cible\n"
			+ "(model3) et son auteur. Le nom est coupe par la fin du champ, pas par la lecture.");
		println("signature 600100-60011F : \"" + getDataAt(toAddr(0x600100L)).getValue() + "\"");

		// --- les six tables de mots
		for (Object[] t : TABLES_DE_MOTS) {
			final long a = (Long) t[0];
			final int n = (Integer) t[1];
			final String nom = (String) t[2];
			final Address at = toAddr(a);
			clearListing(at, toAddr(a + 2L * n - 1));
			final Data d = createData(at, new ArrayDataType(WordDataType.dataType, n, 2));
			createLabel(at, nom, true);
			setPlateComment(at, (String) t[3]);
			println(String.format("%08X : %d mots (%d octets) -> %s", a, n, d.getLength(), nom));
			if (d.getLength() != 2 * n)
				println("*** longueur inattendue en " + at + " : " + d.getLength());
		}

		// --- les deux entrees de bloc deroule
		for (long[] p : DUFF) {
			final Address jmp = toAddr(p[0]), entree = toAddr(p[1]);
			currentProgram.getReferenceManager().addMemoryReference(
				jmp, entree, RefType.COMPUTED_JUMP, SourceType.USER_DEFINED, 0);
			disassemble(entree);
			final Function f = getFunctionContaining(entree.add(2));
			if (f == null) {
				println("*** rien ne contient " + entree.add(2) + " : les deux octets restent dehors");
				continue;
			}
			final AddressSet corps = new AddressSet(f.getBody());
			corps.addRange(entree, entree.add(1));
			f.setBody(corps);
			setPreComment(entree,
				"Entree d'un bloc deroule de 128 move.l (A1)+,(A2)+ : lea " + entree + "(pc,D0.w),A0\n"
				+ "puis jmp (A0), avec D0 = (0x200 - D5) >> 1. Un dispositif de Duff -- la copie\n"
				+ "commence d'autant plus loin dans le bloc qu'il y a moins a copier. Ces deux octets\n"
				+ "etaient les seuls du programme que Ghidra ne tenait pas, et le jmp etait compte a\n"
				+ "tort parmi les sauts indirects que rien ne resout.");
			println(String.format("%08X rattache a %s (%d octets de corps)",
			                      p[1], f.getName(), f.getBody().getNumAddresses()));
		}

		// --- le recompte, fait par le script lui-meme
		final AddressSet rendu = new AddressSet();
		final FunctionIterator it = currentProgram.getFunctionManager().getFunctions(true);
		int nf = 0;
		while (it.hasNext()) { rendu.add(it.next().getBody()); nf++; }
		long octetsCode = rendu.getNumAddresses();

		final AddressSet donnees = new AddressSet();
		Data d = getDataAt(toAddr(0x600000L));
		for (Address a = toAddr(0x600000L); a.getOffset() < CODE_FIN; ) {
			d = getDataContaining(a);
			if (d != null && d.isDefined()) {
				donnees.addRange(d.getMinAddress(), d.getMaxAddress());
				a = d.getMaxAddress().add(1);
			} else {
				a = a.add(1);
			}
		}

		final AddressSet rendre = new AddressSet(toAddr(0x600000L), toAddr(CODE_FIN - 1));
		rendre.delete(rendu);
		rendre.delete(donnees);

		final long total = CODE_FIN - 0x600000L;
		println("");
		println(String.format("%d fonctions, %d octets dans un corps, %d octets de donnees declarees,"
		                      + " sur %d de 600000-%06X", nf, octetsCode, donnees.getNumAddresses(),
		                      total, CODE_FIN));
		println(String.format("code reel = %d - %d = %d octets, dont %d dans un corps = %.2f %%",
		                      total, donnees.getNumAddresses(), total - donnees.getNumAddresses(),
		                      octetsCode, 100.0 * octetsCode / (total - donnees.getNumAddresses())));
		println(String.format("RESTE HORS DE TOUT : %d octets", rendre.getNumAddresses()));
		for (ghidra.program.model.address.AddressRange r : rendre)
			println(String.format("  %08X-%08X  %d", r.getMinAddress().getOffset(),
			                      r.getMaxAddress().getOffset(), r.getLength()));
		if (rendre.isEmpty())
			println("  (rien : chaque octet de 600000 a " + Long.toHexString(CODE_FIN)
			        + " est soit dans une fonction, soit une donnee nommee)");
		println("");
		println("Restent cinq jmp (An) sans resolution statique, non pas sept : 0x600308, 0x602042,");
		println("0x6024E8, 0x603494, 0x60376A. Les deux autres (0x601F3A, 0x6023E0) sont traites ici.");
	}
}
