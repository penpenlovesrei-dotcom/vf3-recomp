// scsp_select.h -- pick an SCSP implementation by name, and reach what scsp::core does not carry.
// Point 2 of step 2 of docs/scsp_migration_plan.md: "both cores in the same binary".
//
// WHY A SELECTOR, and the reason still holds now that the second core exists: the replay needs it,
// because comparing two cores means naming them, and comparing one TO ITSELF is the first check to pass
// -- the one that proves the machinery is not measuring its own noise. And it is here, and nowhere else,
// that the question "what does a core need in order to be built" gets a single answer: the clock, the
// RAM, the sink. Exactly what a standalone core will have to be given.
//
// WHAT THIS FILE NO LONGER KNOWS, and that is the change of 2026-10-28: no concrete class. It includes
// neither scsp_mame.h nor scsp_own.h, and names neither. EACH CORE REGISTERS ITSELF IN ITS OWN .cpp,
// through the `inscrit` below, and the selector only holds the list of those that did. The consequence is
// the one we were after: **what is LINKED decides what exists.** A binary that links only scsp_own.cpp
// and dsp_own.cpp has a one-entry selector, which says so, and does not drag in the 2 110 lines of
// MAME's core -- which is the last prerequisite for a separate repository (docs/scsp_migration_plan.md,
// and docs/sound.md §7 for the "why not yet").
//
// Before that, the selector named both classes, so everything that linked it linked MAME: the eight block
// tests did so through tools/sound/scsp_bench.h, for no substantive reason, their references being frozen
// numbers. The `make test-without-mame` target is the MEASUREMENT that this is over: the same tests, the same
// code, linked without a line of MAME.
#pragma once

#include "scsp_core.h"

#include <memory>

namespace scsp {

// The accepted names, for an error message and for a tool's help text. It is what is registered, hence
// what is linked: the string is not the same in every binary of the repository, and that is deliberate.
const char *known_names();

// Builds the named core, or nullptr if the name is unknown (the message goes to stderr).
//
// `ram` and `puits` must outlive the core returned.
std::unique_ptr<core> make(const char *name, u32 clock, u16 *ram, u32 ram_bytes, sink &snk);

// -----------------------------------------------------------------------------------------------------
// WHAT scsp::core DOES NOT CARRY, AND YET HAS TO BE REACHED.
//
// scsp_core.h has an "AND WHAT IS DELIBERATELY ABSENT" section: the port's additions are not in it,
// because a standalone core must not have to implement them to be usable. Two of them still have to be
// reachable from outside, and here they are -- in ONE place, no longer in three.
//
// THE MADRS MIRROR, and it cost dearly. It is the only one of the additions THAT CHANGES THE SOUND, and
// it changes per game: true for Virtua Fighter 3, FALSE for Fighting Vipers 2 (soundboard.h,
// docs/scsp_audit.md §2). It used to be set by a `dynamic_cast` written into SoundBoard::reset, and two
// further copies of the same switch lived in tools/sound/scsp_bench.h and tools/sound/test_scsp_regs.cpp. The
// board's copy was set AFTER the trace recorder was created, so the trace could not see it: a replay left
// the mirror at its `true` default and diverged from the board at sample 772 067, with no way to say
// whether it was the core or the trace. Measured and closed on 2026-10-28 (docs/scsp_validation.md §3 c).
//
// THE EXPOSED GAIN, that is to say MVOL: the chip never applies it itself, it only exposes it, and it is
// the board that multiplies (measured on 2026-10-26, scsp_contract.h §13 point 7). Block 7 checks it.
//
// These functions guess nothing silently: they return false, or -1, when the core does not carry the
// thing. And THE DAY THEY DISAPPEAR, step 3's debt is paid.
// -----------------------------------------------------------------------------------------------------

bool set_madrs_mirror_on(core &c, bool on);
bool madrs_mirror_of(const core &c, bool *found = nullptr);
float exposed_gain(const core &c);   // -1 if the core does not expose it

// -----------------------------------------------------------------------------------------------------
// REGISTERING A CORE, to be written in the core's .cpp and nowhere else.
//
// The last three functions take a `core&` and return FALSE when it is not theirs: the `dynamic_cast`
// thus lives in the only file that knows the type, and the selector does not have to know it. That is the
// whole trick, and it costs no more than walking a two-entry list on a rare call.
// -----------------------------------------------------------------------------------------------------
struct registration {
	const char *name;
	std::unique_ptr<core> (*factory)(u32 clock, u16 *ram, u32 ram_bytes, sink &snk);
	bool (*set_mirror)(core &c, bool on);          // false if `c` is not this core
	bool (*read_mirror)(const core &c, bool &out);   // likewise
	bool (*read_gain)(const core &c, float &out);    // likewise
};

// To be built as a file static: `const scsp::inscrit s_inscrit{ ... };`. Initialisation order across
// translation units does not matter -- the list is a function static, created on first use.
struct registered {
	explicit registered(const registration &i);
};

} // namespace scsp
