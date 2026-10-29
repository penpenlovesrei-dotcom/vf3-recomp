# The project's SCSP, on its own. NO MAME file anywhere in this chain.
#
#     make test     the seven block tests, on the project's core
#     make measure  the baseline, item by item
#
# The paths are fv2-recomp's -- src/sound, tools/sound -- so that not one line of the sources changes
# between the two repositories.
CXX ?= g++
CXXFLAGS ?= -std=c++20 -O2 -g -Wall -Wno-unused-function
OBJ ?= build/obj
BIN ?= build/bin

CORE = src/sound/scsp_select.cpp src/sound/scsp_own.cpp src/sound/dsp_own.cpp
objs = $(patsubst %.cpp,$(OBJ)/%.o,$(1))

BLOCKS = test_scsp_regs test_scsp_fentes test_scsp_adsr test_scsp_lfo test_scsp_irq test_scsp_mix test_scsp_dsp

all: test

$(OBJ)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

$(BIN)/%: $(call objs,$(CORE)) $(OBJ)/tools/sound/%.o
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $^ -o $@

$(BIN)/measure-core: $(call objs,$(CORE)) $(OBJ)/tools/sound/measure-core.o
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $^ -o $@

test: $(addprefix $(BIN)/,$(BLOCKS))
	@for t in $(BLOCKS); do $(BIN)/$$t own || exit 1; done

measure: $(BIN)/measure-core
	$(BIN)/measure-core --baseline

clean:
	rm -rf build

.PHONY: all test measure clean
-include $(shell find $(OBJ) -name '*.d' 2>/dev/null)
