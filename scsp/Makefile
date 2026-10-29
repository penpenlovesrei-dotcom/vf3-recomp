# The project's SCSP, on its own. NO MAME file anywhere in this chain.
#
#     make test     the seven block tests, on the project's core
#     make mesurer  the baseline, item by item
#
# The paths are fv2-recomp's -- src/sound, tools/son -- so that not one line of the sources changes
# between the two repositories.
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
