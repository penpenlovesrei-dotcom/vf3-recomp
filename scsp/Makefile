# Le SCSP du projet, seul. AUCUN fichier de MAME dans cette chaine.
#
#     make test     les sept tests de bloc, sur le coeur du projet
#     make mesurer  le socle, par poste
#
# Les chemins sont ceux de fv2-recomp -- src/sound, tools/son -- pour que pas une ligne des sources ne
# change entre les deux depots.
CXX ?= g++
CXXFLAGS ?= -std=c++20 -O2 -g -Wall -Wno-unused-function
OBJ ?= build/obj
BIN ?= build/bin

COEUR = src/sound/scsp_select.cpp src/sound/scsp_maison.cpp src/sound/dsp_maison.cpp
objs = $(patsubst %.cpp,$(OBJ)/%.o,$(1))

BLOCS = test_scsp_regs test_scsp_fentes test_scsp_adsr test_scsp_lfo test_scsp_irq test_scsp_mix test_scsp_dsp

all: test

$(OBJ)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

$(BIN)/%: $(call objs,$(COEUR)) $(OBJ)/tools/son/%.o
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $^ -o $@

$(BIN)/mesurer-coeur: $(call objs,$(COEUR)) $(OBJ)/tools/son/mesurer-coeur.o
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $^ -o $@

test: $(addprefix $(BIN)/,$(BLOCS))
	@for t in $(BLOCS); do $(BIN)/$$t maison || exit 1; done

mesurer: $(BIN)/mesurer-coeur
	$(BIN)/mesurer-coeur --socle

clean:
	rm -rf build

.PHONY: all test mesurer clean
-include $(shell find $(OBJ) -name '*.d' 2>/dev/null)
