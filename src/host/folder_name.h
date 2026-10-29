// WHICH OF TWO NAMES A FOLDER OR A FILE GOES BY, when the repository has renamed it and the player's own
// copy has not.
//
// The repository's language is English, so the folders this port reads were renamed: texture_remplacement
// became texture_replacement, assets/menu-console/fin became ending, decors became stages. But those
// folders are NOT in the repository -- they sit in the player's data and assets folders, holding pictures
// the player made or ripped, and a rename in the source cannot reach them. A straight rename would simply
// stop finding them, which is a port that lost its texture pack because a comment changed language.
//
// So the new name is PREFERRED and the old one is still ACCEPTED, exactly as assets/menu-console/font.png
// is preferred over police.png (src/host/fv2_text.h). A folder somebody already has keeps working, a folder
// made from now on can take the English name, and nothing has to be moved on anybody's disk.
//
// It answers with the new name when NEITHER is there, so that what gets created, and what a message names,
// is the name the repository uses.
#pragma once
#include <filesystem>
#include <string>

// Both given as full paths: the first of the two that exists, and the preferred one when neither does.
inline std::string either_name(const std::string &preferred, const std::string &accepted)
{
	std::error_code ec;
	if (std::filesystem::exists(preferred, ec)) return preferred;
	ec.clear();
	if (std::filesystem::exists(accepted, ec)) return accepted;
	return preferred;
}
