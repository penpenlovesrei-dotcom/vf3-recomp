// rejouer : rejoue une trace d'acces au SCSP dans un ou deux coeurs, et dit la premiere divergence.
//
// Point 2 de l'etape 2 de docs/scsp_migration_plan.md, le dernier prealable : "la meme sequence d'acces
// rejouee dans les deux [...] et la comparaison des PCM, des etats observables, des interruptions et des
// timers -- pas seulement du WAV."
//
//   tools/son/rejouer <trace>                 un coeur, et ce qu'il a rendu
//   tools/son/rejouer <trace> <coeur> <coeur> deux coeurs, et leur premiere divergence
//   tools/son/rejouer <trace> --pcm <fichier> ecrit le PCM brut (s32 entrelace) pour comparaison externe
//   tools/son/rejouer <trace> --contre <pcm>  contre la reference PCM gardee a l'enregistrement
//
// LE PREMIER CONTROLE A PASSER EST "mame contre mame", et il n'est pas une formalite : il prouve que le
// rejeu est DETERMINISTE, c'est-a-dire que la trace porte vraiment tout ce dont la puce depend. Si deux
// rejeux du meme coeur sur la meme trace divergent, c'est qu'un etat entre par une porte que la trace ne
// note pas -- et il faut le trouver avant de comparer quoi que ce soit d'autre.
#include "../../src/sound/scsp_select.h"
#include "../../src/sound/scsp_trace.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {

constexpr std::uint32_t RAM_OCTETS = 0x100000;   // 1 Mo, comme une puce de la carte

// Un rejeu complet : sa propre RAM, son propre coeur, sa propre sortie.
bool un_rejeu(const char *trace, const char *nom, scsp::sortie &s, std::string &erreur)
{
	std::vector<std::uint16_t> ram(RAM_OCTETS / 2, 0);
	scsp::puits_notant puits(s);
	std::unique_ptr<scsp::core> c = scsp::make(nom, 45158400 / 2, ram.data(), RAM_OCTETS, puits);
	if (!c) { erreur = std::string("coeur inconnu : ") + nom; return false; }
	puits.suit(c.get());
	c->reset();
	return scsp::rejoue(trace, *c, ram, s, &erreur);
}

void resume(const char *nom, const scsp::sortie &s)
{
	printf("  %-6s %zu echantillons, %zu interruptions, %zu armements, %zu lectures, %zu octets MIDI\n",
	       nom, s.pcm.size() / 2, s.irqs.size(), s.armes.size(), s.lues.size(), s.midis.size());
}

} // namespace

int main(int argc, char **argv)
{
	if (argc < 2) {
		printf("rejouer : rejoue une trace d'acces au SCSP (src/sound/scsp_trace.h pour le format)\n\n"
		       "  rejouer <trace>                      un coeur, et ce qu'il a rendu\n"
		       "  rejouer <trace> <coeur> <coeur>      deux coeurs, et leur premiere divergence\n"
		       "  rejouer <trace> --pcm <fichier>      ecrit le PCM brut (s32 entrelace)\n"
		       "  rejouer <trace> --contre <pcm>       contre la reference gardee a l'enregistrement\n\n"
		       "coeurs connus : %s\n\n"
		       "Une trace s'obtient avec VF3_SCSP_TRACE=f (src/sound/soundboard.cpp) : elle commence a la\n"
		       "remise a zero de la carte, et porte les ecritures de RAM sans lesquelles un rejeu n'a aucun\n"
		       "echantillon a jouer.\n", scsp::noms_connus());
		return 2;
	}

	const char *trace = argv[1];

	// --- un rejeu, avec ou sans PCM ecrit
	if (argc == 2 || (argc == 4 && !strcmp(argv[2], "--pcm"))) {
		scsp::sortie s;
		std::string erreur;
		if (!un_rejeu(trace, "mame", s, erreur)) {
			fprintf(stderr, "rejouer: %s\n", erreur.c_str());
			return 1;
		}
		printf("rejeu de %s par le coeur mame :\n", trace);
		resume("mame", s);
		if (argc == 4) {
			FILE *f = fopen(argv[3], "wb");
			if (!f) { fprintf(stderr, "rejouer: impossible d'ecrire %s\n", argv[3]); return 1; }
			fwrite(s.pcm.data(), sizeof(std::int32_t), s.pcm.size(), f);
			fclose(f);
			printf("  PCM ecrit dans %s (%zu octets)\n", argv[3], s.pcm.size() * sizeof(std::int32_t));
		}
		return 0;
	}

	// --- un rejeu contre la reference PCM que l'enregistrement a gardee
	// C'EST LE CONTROLE QUI VALIDE LA TRACE ELLE-MEME, et il doit passer avant qu'une comparaison entre
	// deux coeurs veuille dire quoi que ce soit : si le rejeu ne retrouve pas le PCM que la carte a produit,
	// c'est que la trace ne porte pas tout ce dont la puce depend, et une divergence entre deux coeurs
	// deviendrait impossible a attribuer.
	if (argc == 4 && !strcmp(argv[2], "--contre")) {
		scsp::sortie s;
		std::string erreur;
		if (!un_rejeu(trace, "mame", s, erreur)) { fprintf(stderr, "rejouer: %s\n", erreur.c_str()); return 1; }
		FILE *f = fopen(argv[3], "rb");
		if (!f) { fprintf(stderr, "rejouer: %s ne s'ouvre pas\n", argv[3]); return 1; }
		std::vector<std::int32_t> ref;
		std::int32_t tampon[4096];
		for (std::size_t lu; (lu = fread(tampon, sizeof(std::int32_t), 4096, f)) > 0; )
			ref.insert(ref.end(), tampon, tampon + lu);
		fclose(f);
		printf("rejeu de %s contre la reference %s :\n", trace, argv[3]);
		resume("rejeu", s);
		printf("  %-6s %zu echantillons\n", "ref", ref.size() / 2);
		if (ref.size() != s.pcm.size()) {
			printf("\nDIVERGENCE -- %zu echantillons rejoues contre %zu enregistres\n",
			       s.pcm.size() / 2, ref.size() / 2);
			return 1;
		}
		for (std::size_t i = 0; i < ref.size(); i++)
			if (ref[i] != s.pcm[i]) {
				printf("\nDIVERGENCE -- echantillon %zu %s : rejeu %d, enregistre %d\n",
				       i / 2, (i & 1) ? "droite" : "gauche", s.pcm[i], ref[i]);
				return 1;
			}
		printf("\nIDENTIQUE A L'OCTET sur %zu echantillons : la trace porte tout ce dont la puce depend.\n",
		       ref.size() / 2);
		return 0;
	}

	// --- deux rejeux, et la comparaison
	if (argc == 4) {
		scsp::sortie a, b;
		std::string erreur;
		if (!un_rejeu(trace, argv[2], a, erreur)) { fprintf(stderr, "rejouer: %s\n", erreur.c_str()); return 1; }
		if (!un_rejeu(trace, argv[3], b, erreur)) { fprintf(stderr, "rejouer: %s\n", erreur.c_str()); return 1; }
		printf("rejeu de %s dans deux coeurs :\n", trace);
		resume(argv[2], a);
		resume(argv[3], b);
		const std::string d = scsp::compare(a, b);
		if (d.empty()) {
			printf("\nIDENTIQUES : le PCM, les interruptions, les armements de timer, les lectures de\n"
			       "registre et le MIDI sortant concordent, echantillon par echantillon.\n");
			return 0;
		}
		printf("\nDIVERGENCE -- %s\n", d.c_str());
		return 1;
	}

	fprintf(stderr, "rejouer: arguments inattendus ; lancer sans argument pour l'aide\n");
	return 2;
}
