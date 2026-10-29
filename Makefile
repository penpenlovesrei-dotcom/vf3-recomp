# Virtua Fighter 3 (Model 3) recomp: headless tools for now (build/bin/vf3run).
# ROM-derived data lives in build/ and outside the repo, and is never committed.
CXX ?= clang++
CXXFLAGS ?= -std=c++20 -O2 -g -Wall -Wno-unused-function
# -Iscsp/src/sound: the project's SCSP has lived under scsp/ since 2026-10-29, so that `git subtree`
# can extract it. The files that stay here still say `#include "scsp_select.h"` with no path, and it
# is this -I that makes them land right -- nothing to rewrite in soundboard, scsp_mame or scspdsp.
CXXFLAGS += -DSLJIT_CONFIG_AUTO=1 -Isrc/core/shim_include -Iscsp/src/sound $(EXTRA_CFLAGS)
# The PowerPC's floating point must round each operation on its own, on every platform (no fused multiply-adds)
PPC_FLAGS = -ffp-contract=off
LDLIBS ?= -lz

OBJ ?= build/obj
BIN ?= build/bin
objs = $(patsubst %.cpp,$(OBJ)/%.o,$(1))

all: $(BIN)/vf3run $(BIN)/vf3lock $(BIN)/vf3

SDL_CFLAGS ?= $(shell pkg-config --cflags sdl3 2>/dev/null || echo -I/opt/homebrew/include)
SDL_LIBS ?= $(shell pkg-config --libs sdl3 2>/dev/null || echo -L/opt/homebrew/lib -lSDL3)
FT_CFLAGS ?= $(shell pkg-config --cflags freetype2 2>/dev/null || echo -I/opt/homebrew/include/freetype2)
FT_LIBS ?= $(shell pkg-config --libs freetype2 2>/dev/null || echo -L/opt/homebrew/lib -lfreetype)

# the version and date shown in the menu: the git tag (or tag-commits-hash between releases, or the commit) and
# the date of the last commit. Builds without git are handed them in the environment.
VF3_VERSION ?= $(shell git describe --tags --always 2>/dev/null || echo dev)
VF3_DATE ?= $(shell git log -1 --format=%cd --date=format:'%b %-d %Y' 2>/dev/null)
$(OBJ)/version.h: FORCE
	@mkdir -p $(OBJ)
	@printf '#define VF3_VERSION "%s"\n#define VF3_DATE "%s"\n' "$(VF3_VERSION)" "$(VF3_DATE)" > $@.tmp
	@cmp -s $@.tmp $@ && rm -f $@.tmp || mv $@.tmp $@
FORCE:

# RmlUi (MIT, third_party/rmlui): the in-game menu. Its core and the SDL backend, built with FreeType.
RML_SRC = $(wildcard third_party/rmlui/Source/Core/*.cpp third_party/rmlui/Source/Core/*/*.cpp) \
	third_party/rmlui/Backends/RmlUi_Platform_SDL.cpp third_party/rmlui/Backends/RmlUi_Renderer_SDL.cpp
RML_CFLAGS = -DRMLUI_STATIC_LIB -DRMLUI_FONT_ENGINE_FREETYPE -DRMLUI_SDL_VERSION_MAJOR=3 -Ithird_party/rmlui/Include -Ithird_party/rmlui/Backends
$(OBJ)/third_party/rmlui/%.o: CXXFLAGS += $(RML_CFLAGS) $(FT_CFLAGS) $(SDL_CFLAGS) -w
$(OBJ)/src/host/main.o $(OBJ)/src/host/menu.o $(OBJ)/src/host/sysinfo.o $(OBJ)/src/host/d3d11_map.o $(OBJ)/src/gpu/r3d_gpu.o: CXXFLAGS += $(SDL_CFLAGS) $(RML_CFLAGS)
$(OBJ)/src/host/main.o: $(OBJ)/version.h
$(OBJ)/src/host/main.o: CXXFLAGS += -I$(OBJ)
HOST = src/host/main.cpp src/host/menu.cpp src/host/sysinfo.cpp src/host/d3d11_map.cpp src/gpu/r3d_gpu.cpp

CORE = src/ppc/ppc.cpp src/machine/model3.cpp src/machine/romset.cpp src/machine/fv2_portraits.cpp src/host/input_script.cpp \
	src/rec/ppc_rec.cpp src/rec/vf3_map.cpp src/rec/fv2_map.cpp src/video/tilegen.cpp src/video/r3d_scene.cpp src/video/r3d_raster.cpp \
	src/video/tex3d_hd.cpp src/video/tilehd.cpp \
	src/sound/soundboard.cpp src/sound/scsp.cpp src/sound/scspdsp.cpp src/sound/scsp_mame.cpp \
	scsp/src/sound/scsp_trace.cpp scsp/src/sound/scsp_select.cpp scsp/src/sound/scsp_own.cpp scsp/src/sound/dsp_own.cpp src/core/mame_shim.cpp \
	third_party/mame/m68000/m68000.cpp third_party/mame/m68000/m68000-decode.cpp \
	third_party/mame/m68000/m68000-sdf.cpp third_party/mame/m68000/m68000-sdp.cpp \
	third_party/mame/315-5881/crypt_315_5881.cpp

# SLJIT (BSD-2): the native code made at start-up. C, built by the C++ compiler's driver (-x c)
SLJIT_OBJ = $(OBJ)/third_party/sljit/sljitLir.o
$(SLJIT_OBJ): third_party/sljit/sljitLir.c
	@mkdir -p $(dir $@)
	$(CXX) -x c -O2 -w -DSLJIT_CONFIG_AUTO=1 -c $< -o $@


$(OBJ)/src/ppc/ppc.o: CXXFLAGS += $(PPC_FLAGS)

$(OBJ)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

$(BIN)/vf3run: $(call objs,$(CORE) tools/vf3run.cpp) $(SLJIT_OBJ)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDLIBS)

$(BIN)/vf3lock: $(call objs,$(CORE) tools/vf3lock.cpp) $(SLJIT_OBJ)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDLIBS)

$(BIN)/vf3: $(call objs,$(CORE) $(HOST) $(RML_SRC)) $(SLJIT_OBJ)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(SDL_LIBS) $(FT_LIBS) $(LDLIBS) -lpthread

# The effects DSP's two bodies against a reference that decodes MPRO in the loop (tools/sound/test_scspdsp.cpp).
# No ROM, no window, fixed seeds. Not in `all`: `make test` builds and runs it.
$(BIN)/test_scspdsp: $(call objs,src/sound/scspdsp.cpp src/core/mame_shim.cpp tools/sound/test_scspdsp.cpp)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $^ -o $@

# scsp/src/sound/scsp_contract.h against the two things it describes: the field positions in scsp.cpp and the
# driver's constants in the sound ROM (tools/sound/test_scsp_contract.cpp). Part 1 needs nothing but the
# sources; part 2 says so and steps aside when build/fv2-sound-68000.bin is not there.
$(BIN)/test_scsp_contract: $(call objs,tools/sound/test_scsp_contract.cpp)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $^ -o $@

# MAME's SCSP driven ONLY through the new interface (tools/sound/test_scsp_core.cpp): step 3 of the migration
# plan, the one that decides whether the contract holds. Writes its registers through scsp_contract.h's own
# field table, so a wrong field there stops the sound here instead of in a game.
# (scsp_select.cpp has been there since each core REGISTERS ITSELF in its own .cpp: linking a core means
# linking the register. This test does not use the selector -- it builds scsp_mame outright, which is its subject.)
$(BIN)/test_scsp_core: $(call objs,src/sound/scsp.cpp src/sound/scspdsp.cpp src/sound/scsp_mame.cpp scsp/src/sound/scsp_select.cpp src/core/mame_shim.cpp tools/sound/test_scsp_core.cpp)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $^ -o $@

# What a register write reads back (scsp/tools/sound/test_scsp_regs.cpp): block 1 of the architecture's order,
# and the plan wants these written BEFORE the blocks they test. No ROM, no sound: write, read back.
$(BIN)/test_scsp_regs: $(call objs,src/sound/scsp.cpp src/sound/scspdsp.cpp src/sound/scsp_mame.cpp scsp/src/sound/scsp_select.cpp scsp/src/sound/scsp_own.cpp scsp/src/sound/dsp_own.cpp src/core/mame_shim.cpp scsp/tools/sound/test_scsp_regs.cpp)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $^ -o $@

# The oscillator, the PCM addressing and the four loop modes (scsp/tools/sound/test_scsp_slots.cpp): block 2
# of the architecture. Measures the exact run of sample indices, SDIR setting the envelope aside.
$(BIN)/test_scsp_slots: $(call objs,src/sound/scsp.cpp src/sound/scspdsp.cpp src/sound/scsp_mame.cpp scsp/src/sound/scsp_select.cpp scsp/src/sound/scsp_own.cpp scsp/src/sound/dsp_own.cpp src/core/mame_shim.cpp scsp/tools/sound/test_scsp_slots.cpp)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $^ -o $@

# The ADSR envelope (scsp/tools/sound/test_scsp_adsr.cpp): block 3 of the architecture. A steady wave makes the
# output BE the envelope, and the attack durations in samples encode MAME's time tables.
$(BIN)/test_scsp_adsr: $(call objs,src/sound/scsp.cpp src/sound/scspdsp.cpp src/sound/scsp_mame.cpp scsp/src/sound/scsp_select.cpp scsp/src/sound/scsp_own.cpp scsp/src/sound/dsp_own.cpp src/core/mame_shim.cpp scsp/tools/sound/test_scsp_adsr.cpp)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $^ -o $@

# The step, the two LFOs and ring modulation (scsp/tools/sound/test_scsp_lfo.cpp): block 4 of the architecture.
# Reads the phase accumulator through a ramp, and pins the port's two deliberate divergences (PLFOS, MDL).
$(BIN)/test_scsp_lfo: $(call objs,src/sound/scsp.cpp src/sound/scspdsp.cpp src/sound/scsp_mame.cpp scsp/src/sound/scsp_select.cpp scsp/src/sound/scsp_own.cpp scsp/src/sound/dsp_own.cpp src/core/mame_shim.cpp scsp/tools/sound/test_scsp_lfo.cpp)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $^ -o $@

# The timers, the five interrupt lines and the DMA (scsp/tools/sound/test_scsp_irq.cpp): block 6. Measures
# through the SINK and not through the sound, and pins timer_samples's "+1", the port's third wanted divergence.
$(BIN)/test_scsp_irq: $(call objs,src/sound/scsp.cpp src/sound/scspdsp.cpp src/sound/scsp_mame.cpp scsp/src/sound/scsp_select.cpp scsp/src/sound/scsp_own.cpp scsp/src/sound/dsp_own.cpp src/core/mame_shim.cpp scsp/tools/sound/test_scsp_irq.cpp)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $^ -o $@

# The mixing and the output (scsp/tools/sound/test_scsp_mix.cpp): block 7, the last one. Reads the three
# Sega-style decibel tables (TL, DIPAN, SDLT) in full, and BOTH channels.
$(BIN)/test_scsp_mix: $(call objs,src/sound/scsp.cpp src/sound/scspdsp.cpp src/sound/scsp_mame.cpp scsp/src/sound/scsp_select.cpp scsp/src/sound/scsp_own.cpp scsp/src/sound/dsp_own.cpp src/core/mame_shim.cpp scsp/tools/sound/test_scsp_mix.cpp)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $^ -o $@

# The replayer (scsp/tools/sound/replay.cpp): point 2 of step 2 of the migration plan. Replays an access trace
# into one or two cores and compares the PCM, the interrupts, the arming, the reads and the MIDI.
$(BIN)/replay: $(call objs,src/sound/scsp.cpp src/sound/scspdsp.cpp src/sound/scsp_mame.cpp scsp/src/sound/scsp_trace.cpp scsp/src/sound/scsp_select.cpp scsp/src/sound/scsp_own.cpp scsp/src/sound/dsp_own.cpp src/core/mame_shim.cpp scsp/tools/sound/replay.cpp)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $^ -o $@

# The effects DSP's instruction set (scsp/tools/sound/test_scsp_dsp.cpp): block 5, and it is a SPECIFICATION
# before it is a test -- this instruction set is written down nowhere. Observed through the register window
# (TEMP, MEMS, MIXS, EFREG) and through the RAM, without one note of music.
$(BIN)/test_scsp_dsp: $(call objs,src/sound/scsp.cpp src/sound/scspdsp.cpp src/sound/scsp_mame.cpp scsp/src/sound/scsp_select.cpp scsp/src/sound/scsp_own.cpp scsp/src/sound/dsp_own.cpp src/core/mame_shim.cpp scsp/tools/sound/test_scsp_dsp.cpp)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $^ -o $@

# What a SCSP core costs, part by part (scsp/tools/sound/measure-core.cpp). The replayer compares to the byte
# but its time is dominated by parsing the trace; this bench times run() alone, and splits the items.
$(BIN)/measure-core: $(call objs,src/sound/scsp.cpp src/sound/scspdsp.cpp src/sound/scsp_mame.cpp scsp/src/sound/scsp_select.cpp scsp/src/sound/scsp_own.cpp scsp/src/sound/dsp_own.cpp src/core/mame_shim.cpp scsp/tools/sound/measure-core.cpp)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $^ -o $@

# THE PROOF THAT THE DEPENDENCE ON MAME'S CORE IS CUT, and it is a target rather than a sentence.
#
# The seven tests below speak of nothing but scsp::core, the contract and the selector; they linked MAME's
# 2 110 lines of core all the same, through scsp/tools/sound/scsp_bench.h's two switches and a third in
# test_scsp_regs.cpp -- for no reason of substance, their references being frozen numbers. Since each core
# registers itself in its own .cpp (scsp/src/sound/scsp_select.h), WHAT IS LINKED DECIDES WHAT EXISTS, and
# these same tests build with the project's core alone. It is the last prerequisite of a separate repository
# (scsp/docs/scsp_migration_plan.md), and `make test` does it again every time so that it cannot rust.
#
# The other three tests are not here, and each for a good reason: test_scsp_core drives MAME through the new
# interface, test_scspdsp measures MAME's DSP's two bodies, test_scsp_contract reads scsp.cpp's macros. Their
# subject IS MAME's core.
OWN_ONLY = scsp/src/sound/scsp_select.cpp scsp/src/sound/scsp_own.cpp scsp/src/sound/dsp_own.cpp
WITHOUT_MAME = test_scsp_regs test_scsp_slots test_scsp_adsr test_scsp_lfo test_scsp_irq test_scsp_mix test_scsp_dsp

# THE PREFIX IS scsp/, AND A WRONG ONE IS SILENT. A pattern rule whose prerequisite cannot be made is
# REJECTED, not reported: make then finds the exe already there, has no other rule for it, and declares it
# up to date. These seven stayed at their 2026-10-28 build for a whole day after the git mv, passing every
# time, testing code that had moved. `make clean` would not have said it either -- only the exe's timestamp
# did. Nothing else in this file names a tools/sound object, which is why this one line was the only victim.
$(BIN)/without-mame/%: $(call objs,$(OWN_ONLY)) $(OBJ)/scsp/tools/sound/%.o
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $^ -o $@

test-without-mame: $(addprefix $(BIN)/without-mame/,$(WITHOUT_MAME))
	@echo "---- the same tests, linked WITHOUT a line of MAME's core ----"
	@for t in $(WITHOUT_MAME); do $(BIN)/without-mame/$$t own || exit 1; done

test: $(BIN)/test_scsp_dsp $(BIN)/test_scspdsp $(BIN)/test_scsp_contract $(BIN)/test_scsp_core $(BIN)/test_scsp_regs $(BIN)/test_scsp_slots $(BIN)/test_scsp_adsr $(BIN)/test_scsp_lfo $(BIN)/test_scsp_irq $(BIN)/test_scsp_mix
	$(BIN)/test_scspdsp
	$(BIN)/test_scsp_dsp mame
	$(BIN)/test_scsp_dsp own
	$(BIN)/test_scsp_contract
	$(BIN)/test_scsp_core
	$(BIN)/test_scsp_regs mame
	$(BIN)/test_scsp_regs own
	$(BIN)/test_scsp_slots mame
	$(BIN)/test_scsp_slots own
	$(BIN)/test_scsp_adsr mame
	$(BIN)/test_scsp_adsr own
	$(BIN)/test_scsp_lfo mame
	$(BIN)/test_scsp_lfo own
	$(BIN)/test_scsp_irq mame
	$(BIN)/test_scsp_irq own
	$(BIN)/test_scsp_mix mame
	$(BIN)/test_scsp_mix own
	@$(MAKE) --no-print-directory test-without-mame

clean:
	rm -rf build

.PHONY: all clean test test-without-mame FORCE
-include $(shell find $(OBJ) -name '*.d' 2>/dev/null)
