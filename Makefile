# Virtua Fighter 3 (Model 3) recomp: headless tools for now (build/bin/vf3run).
# ROM-derived data lives in build/ and outside the repo, and is never committed.
CXX ?= clang++
CXXFLAGS ?= -std=c++20 -O2 -g -Wall -Wno-unused-function
CXXFLAGS += -DSLJIT_CONFIG_AUTO=1 -Isrc/core/shim_include $(EXTRA_CFLAGS)
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
	src/sound/scsp_trace.cpp src/sound/scsp_select.cpp src/sound/scsp_maison.cpp src/sound/dsp_maison.cpp src/core/mame_shim.cpp \
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

# The effects DSP's two bodies against a reference that decodes MPRO in the loop (tools/son/test_scspdsp.cpp).
# No ROM, no window, fixed seeds. Not in `all`: `make test` builds and runs it.
$(BIN)/test_scspdsp: $(call objs,src/sound/scspdsp.cpp src/core/mame_shim.cpp tools/son/test_scspdsp.cpp)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $^ -o $@

# src/sound/scsp_contract.h against the two things it describes: the field positions in scsp.cpp and the
# driver's constants in the sound ROM (tools/son/test_scsp_contract.cpp). Part 1 needs nothing but the
# sources; part 2 says so and steps aside when build/fv2-son-68000.bin is not there.
$(BIN)/test_scsp_contract: $(call objs,tools/son/test_scsp_contract.cpp)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $^ -o $@

# MAME's SCSP driven ONLY through the new interface (tools/son/test_scsp_core.cpp): step 3 of the migration
# plan, the one that decides whether the contract holds. Writes its registers through scsp_contract.h's own
# field table, so a wrong field there stops the sound here instead of in a game.
$(BIN)/test_scsp_core: $(call objs,src/sound/scsp.cpp src/sound/scspdsp.cpp src/sound/scsp_mame.cpp src/core/mame_shim.cpp tools/son/test_scsp_core.cpp)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $^ -o $@

# What a register write reads back (tools/son/test_scsp_regs.cpp): block 1 of the architecture's order,
# and the plan wants these written BEFORE the blocks they test. No ROM, no sound: write, read back.
$(BIN)/test_scsp_regs: $(call objs,src/sound/scsp.cpp src/sound/scspdsp.cpp src/sound/scsp_mame.cpp src/sound/scsp_select.cpp src/sound/scsp_maison.cpp src/sound/dsp_maison.cpp src/core/mame_shim.cpp tools/son/test_scsp_regs.cpp)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $^ -o $@

# L'oscillateur, l'adressage PCM et les quatre bouclages (tools/son/test_scsp_fentes.cpp) : bloc 2 de
# l'architecture. Mesure la suite exacte des indices d'echantillon, SDIR mettant l'enveloppe de cote.
$(BIN)/test_scsp_fentes: $(call objs,src/sound/scsp.cpp src/sound/scspdsp.cpp src/sound/scsp_mame.cpp src/sound/scsp_select.cpp src/sound/scsp_maison.cpp src/sound/dsp_maison.cpp src/core/mame_shim.cpp tools/son/test_scsp_fentes.cpp)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $^ -o $@

# L'enveloppe ADSR (tools/son/test_scsp_adsr.cpp) : bloc 3 de l'architecture. Une onde continue fait que la
# sortie EST l'enveloppe, et les durees d'attaque en echantillons encodent les tables de temps de MAME.
$(BIN)/test_scsp_adsr: $(call objs,src/sound/scsp.cpp src/sound/scspdsp.cpp src/sound/scsp_mame.cpp src/sound/scsp_select.cpp src/sound/scsp_maison.cpp src/sound/dsp_maison.cpp src/core/mame_shim.cpp tools/son/test_scsp_adsr.cpp)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $^ -o $@

# Le pas, les deux LFO et la modulation par anneau (tools/son/test_scsp_lfo.cpp) : bloc 4 de l'architecture.
# Lit l'accumulateur de phase par une rampe, et fixe les deux divergences deliberees du portage (PLFOS, MDL).
$(BIN)/test_scsp_lfo: $(call objs,src/sound/scsp.cpp src/sound/scspdsp.cpp src/sound/scsp_mame.cpp src/sound/scsp_select.cpp src/sound/scsp_maison.cpp src/sound/dsp_maison.cpp src/core/mame_shim.cpp tools/son/test_scsp_lfo.cpp)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $^ -o $@

# Les timers, les cinq lignes d'interruption et le DMA (tools/son/test_scsp_irq.cpp) : bloc 6. Mesure par le
# PUITS et non par le son, et fixe le "+1" de timer_samples, troisieme divergence voulue du portage.
$(BIN)/test_scsp_irq: $(call objs,src/sound/scsp.cpp src/sound/scspdsp.cpp src/sound/scsp_mame.cpp src/sound/scsp_select.cpp src/sound/scsp_maison.cpp src/sound/dsp_maison.cpp src/core/mame_shim.cpp tools/son/test_scsp_irq.cpp)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $^ -o $@

# Le melange et le rendu (tools/son/test_scsp_mix.cpp) : bloc 7, le dernier. Lit les trois tables de
# decibels a la Sega (TL, DIPAN, SDLT) dans leur entier, et les DEUX canaux.
$(BIN)/test_scsp_mix: $(call objs,src/sound/scsp.cpp src/sound/scspdsp.cpp src/sound/scsp_mame.cpp src/sound/scsp_select.cpp src/sound/scsp_maison.cpp src/sound/dsp_maison.cpp src/core/mame_shim.cpp tools/son/test_scsp_mix.cpp)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $^ -o $@

# Le rejoueur (tools/son/rejouer.cpp) : point 2 de l'etape 2 du plan de migration. Rejoue une trace d'acces
# dans un ou deux coeurs et compare le PCM, les interruptions, les armements, les lectures et le MIDI.
$(BIN)/rejouer: $(call objs,src/sound/scsp.cpp src/sound/scspdsp.cpp src/sound/scsp_mame.cpp src/sound/scsp_trace.cpp src/sound/scsp_select.cpp src/sound/scsp_maison.cpp src/sound/dsp_maison.cpp src/core/mame_shim.cpp tools/son/rejouer.cpp)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $^ -o $@

# Le jeu d'instructions du DSP d'effets (tools/son/test_scsp_dsp.cpp) : bloc 5, et c'est une SPECIFICATION
# avant d'etre un test -- ce jeu d'instructions n'existe nulle part ecrit. Observe par la fenetre de
# registres (TEMP, MEMS, MIXS, EFREG) et par la RAM, sans une note de musique.
$(BIN)/test_scsp_dsp: $(call objs,src/sound/scsp.cpp src/sound/scspdsp.cpp src/sound/scsp_mame.cpp src/sound/scsp_select.cpp src/sound/scsp_maison.cpp src/sound/dsp_maison.cpp src/core/mame_shim.cpp tools/son/test_scsp_dsp.cpp)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $^ -o $@

# Ce qu'un coeur de SCSP coute, par partie (tools/son/mesurer-coeur.cpp). Le rejoueur compare a l'octet mais
# son temps est domine par l'analyse de trace ; ce banc ne chronometre que run(), et separe les postes.
$(BIN)/mesurer-coeur: $(call objs,src/sound/scsp.cpp src/sound/scspdsp.cpp src/sound/scsp_mame.cpp src/sound/scsp_select.cpp src/sound/scsp_maison.cpp src/sound/dsp_maison.cpp src/core/mame_shim.cpp tools/son/mesurer-coeur.cpp)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $^ -o $@

test: $(BIN)/test_scsp_dsp $(BIN)/test_scspdsp $(BIN)/test_scsp_contract $(BIN)/test_scsp_core $(BIN)/test_scsp_regs $(BIN)/test_scsp_fentes $(BIN)/test_scsp_adsr $(BIN)/test_scsp_lfo $(BIN)/test_scsp_irq $(BIN)/test_scsp_mix
	$(BIN)/test_scspdsp
	$(BIN)/test_scsp_dsp mame
	$(BIN)/test_scsp_dsp maison
	$(BIN)/test_scsp_contract
	$(BIN)/test_scsp_core
	$(BIN)/test_scsp_regs mame
	$(BIN)/test_scsp_regs maison
	$(BIN)/test_scsp_fentes mame
	$(BIN)/test_scsp_fentes maison
	$(BIN)/test_scsp_adsr mame
	$(BIN)/test_scsp_adsr maison
	$(BIN)/test_scsp_lfo mame
	$(BIN)/test_scsp_lfo maison
	$(BIN)/test_scsp_irq mame
	$(BIN)/test_scsp_irq maison
	$(BIN)/test_scsp_mix mame
	$(BIN)/test_scsp_mix maison

clean:
	rm -rf build

.PHONY: all clean test FORCE
-include $(shell find $(OBJ) -name '*.d' 2>/dev/null)
