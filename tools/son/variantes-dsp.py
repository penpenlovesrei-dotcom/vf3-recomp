# -*- coding: utf-8 -*-
# LES VARIANTES A JUGER AU COMPTEUR D'INSTRUCTIONS.
#
# Chacune est une transformation de dsp_maison.cpp qui doit etre EQUIVALENTE au comportement pres -- c'est
# verifie par les dix tests avant qu'elle soit retenue. Chacune attaque le NOMBRE D'INSTRUCTIONS du chemin
# inevitable d'un pas, qui est ou va l'ecart (+19,6 % dans la boucle, docs/scsp_audit.md §4 nonies).
#
# Ce fichier ne mesure rien : il construit un exe par variante. Le jugement demande une fenetre
# administrateur, et c'est mesurer-variantes.cmd qui le fait, une capture par exe.

# --- les morceaux de source, tels qu'ils sont aujourd'hui
ENTREE = """		// --- l'entree
		s32 entree;
		if (I.ira <= 0x1F)      entree = MEMS[I.ira];
		else if (I.ira <= 0x2F) entree = MIXS[I.ira - 0x20] << 4;    // MIXS fait 20 bits
		else if (I.ira <= 0x31) entree = EXTS[I.ira - 0x30] << 8;    // EXTS en fait 16
		else return;                                                 // au-dela, le pas abandonne tout
		entree = sext(entree, 24);
"""

YSEL = """		s32 y = 0;
		switch (I.ysel) {
		case 0: y = m_frc_reg; break;
		case 1: y = COEF[I.coef] >> 3; break;       // COEF fait 16 bits, Y en veut 13
		case 2: y = (m_y_reg >> 11) & 0x1FFF; break;
		case 3: y = (m_y_reg >> 4) & 0x0FFF; break;
		}"""

SORTIES = """		if ((f & I_TWT)) TEMP[(I.twa + dec) & 0x7F] = shifted;"""

VARIANTES = [

 ("temoin", "le code tel qu'il est, pour que les cinq autres se comparent a quelque chose", []),

 # 1. Un pas du socle ne fait AUCUNE des quatre sorties : TWT, MRD, MWT, EWT sont a zero. Il les teste
 #    pourtant une par une, donc quatre tests pour rien. Un seul test groupe les ecarte toutes.
 ("sortie_groupee",
  "un seul test de masque pour les quatre sorties et les deux rares, au lieu de six",
  [(SORTIES,
    """		// UN SEUL TEST POUR LES QUATRE SORTIES. Un pas qui n'ecrit rien -- et il y en a beaucoup --
		// les testait une par une. Le masque groupe les ecarte d'un coup, et les tests individuels
		// restent a l'interieur pour les pas qui ecrivent.
		// I_FRCL ET I_ADRL SONT DANS LE MASQUE, et il a fallu que les cas 9 a 13 le disent : leurs
		// ecritures vivent ENTRE celle de TWT et celle d'EWT, donc un masque qui n'aurait groupe que
		// les quatre sorties les aurait sautees pour un pas qui ne pose que l'une d'elles. Dans le
		// corps maigre les deux bits sont nuls : le masque y vaut donc exactement les quatre sorties.
		if ((f & (I_TWT | I_MRD | I_MWT | I_EWT | I_FRCL | I_ADRL))) {
		if ((f & I_TWT)) TEMP[(I.twa + dec) & 0x7F] = shifted;"""),
   ("""		// EWT AJOUTE, il n'affecte pas : plusieurs pas peuvent nourrir la meme sortie d'effets.
		if ((f & I_EWT)) EFREG[I.ewa] = s16(EFREG[I.ewa] + s16(shifted >> 8));
	}""",
    """		// EWT AJOUTE, il n'affecte pas : plusieurs pas peuvent nourrir la meme sortie d'effets.
		if ((f & I_EWT)) EFREG[I.ewa] = s16(EFREG[I.ewa] + s16(shifted >> 8));
		}
	}""")]),

 # 2. `entree` est calculee a chaque pas, et dans le socle elle ne sert a RIEN : XSEL est nul, IWT nul, et
 #    les deux lecteurs rares sont hors du corps maigre. Trois comparaisons, une charge et une extension de
 #    signe pour une valeur jetee. L'abandon, lui, ne depend que du programme : Start le trouve une fois.
 ("entree_paresseuse",
  "l'entree calculee seulement par les pas qui la lisent ; l'abandon resolu au Start",
  [(ENTREE,
    """		// --- l'entree, CALCULEE SEULEMENT SI CE PAS LA LIT. Dans le socle elle ne sert a rien : XSEL
		// est nul, IWT nul, et les deux lecteurs rares sont hors du corps maigre. L'abandon (IRA au-dela
		// de 0x31) ne depend que du programme, donc Start le trouve une fois : voir m_abandon.
		s32 entree = 0;
		if ((f & (I_XSEL | I_IWT)) || (RARES && (f & (I_YRL | I_ADRL)))) {
			if (I.ira <= 0x1F)      entree = MEMS[I.ira];
			else if (I.ira <= 0x2F) entree = MIXS[I.ira - 0x20] << 4;    // MIXS fait 20 bits
			else                    entree = EXTS[I.ira - 0x30] << 8;    // EXTS en fait 16
			entree = sext(entree, 24);
		}
""")]),

 # 3. Le `switch` a quatre cas sur SHIFT pose deux questions independantes : doubler d'abord, puis ecreter
 #    ou etendre le signe. Posees comme telles, les quatre cas n'ont plus besoin d'un switch -- et SANS
 #    grossir le pas decode, puisque les deux bits vont dans le mot d'indicateurs.
 ("shift_deux_bits",
  "le switch SHIFT remplace par deux bits du mot d'indicateurs",
  [("""		s32 shifted = 0;
		switch (I.shift) {
		case 0: shifted = std::clamp<s32>(m_acc, -0x00800000, 0x007FFFFF); break;
		case 1: shifted = std::clamp<s32>(m_acc * 2, -0x00800000, 0x007FFFFF); break;
		case 2: shifted = sext(m_acc * 2, 24); break;
		case 3: shifted = sext(m_acc, 24); break;
		}""",
    """		const s32 av = (f & I_SHPRE) ? (m_acc * 2) : m_acc;
		const s32 shifted = (f & I_SHECR) ? std::clamp<s32>(av, -0x00800000, 0x007FFFFF) : sext(av, 24);"""),
   ("""	if ((m[3] >> 1) & 1)  f |= I_ADREB;
	if (m[3] & 1)         f |= I_NXADR;
	p.f = f;""",
    """	if ((m[3] >> 1) & 1)  f |= I_ADREB;
	if (m[3] & 1)         f |= I_NXADR;
	// Les deux questions du decalage, posees une fois a l'ecriture du programme.
	if (p.shift == 1 || p.shift == 2) f |= I_SHPRE;
	if (p.shift <= 1)                 f |= I_SHECR;
	p.f = f;"""),
   ("""		I_FRCL = 1u << 12, I_ADRL = 1u << 13, I_YRL = 1u << 14, I_NEGB = 1u << 15,
		I_RARES = I_FRCL | I_ADRL | I_YRL | I_NEGB,""",
    """		I_FRCL = 1u << 12, I_ADRL = 1u << 13, I_YRL = 1u << 14, I_NEGB = 1u << 15,
		I_RARES = I_FRCL | I_ADRL | I_YRL | I_NEGB,
		I_SHPRE = 1u << 16,          // le decalage double l'accumulateur d'abord (SHIFT 1 et 2)
		I_SHECR = 1u << 17,          // il ECRETE (SHIFT 0 et 1) au lieu d'etendre le signe (2 et 3)""",
    'h')]),

 # 4. MAME teste YSEL == 1 EN PREMIER, et son commentaire dit pourquoi : 87 des 112 pas du jeu le prennent.
 #    Le switch du projet met le cas 0 d'abord, qui est le plus rare dans le jeu. Sans effet sur le socle
 #    (ou YSEL vaut zero partout) -- c'est le programme du jeu que ca viserait.
 ("ysel_coef_premier",
  "COEF teste en premier dans le choix de Y, comme chez MAME",
  [(YSEL,
    """		// COEF EN PREMIER, comme chez MAME et pour sa raison : 87 des 112 pas du jeu le prennent
		// (docs/scsp_audit.md §5), et c'est le cas 1, donc le deuxieme d'un switch ecrit dans l'ordre.
		s32 y;
		if (I.ysel == 1)      y = COEF[I.coef] >> 3;       // COEF fait 16 bits, Y en veut 13
		else if (I.ysel == 0) y = m_frc_reg;
		else if (I.ysel == 2) y = (m_y_reg >> 11) & 0x1FFF;
		else                  y = (m_y_reg >> 4) & 0x0FFF;""")]),

]

# LES QUATRE ENSEMBLE, et il faut la construire APRES la liste et non dedans. Elles touchent des regions
# disjointes du pas -- l'entree, le choix de Y, le decalage, les sorties -- donc rien n'empeche de les
# porter toutes. Mesure du 2026-10-28 : -15,00 %, -6,02 %, -3,03 % et -1,55 % d'instructions retirees
# chacune, contre le temoin.
#
# ECRITE D'ABORD DANS LE LITTERAL, par `VARIANTES[1:]`, elle etait VIDE : le nom n'est pas encore lie a cet
# endroit, et les scripts qui lisent ce fichier par `exec` le pre-definissent a []. La variante passait donc
# les dix tests et mesurait exactement comme le temoin, ce qui ressemblait a un resultat.
VARIANTES.append(("tout", "les quatre gagnantes ensemble",
                  [p for _, _, patches in VARIANTES[1:] for p in patches]))
