# Host-native unit tests for the pure-logic core/ modules - no
# <3ds.h>/citro2d/citro3d dependency at all, so they're compiled with the
# system g++ (NOT devkitARM, no Podman) and run directly on the host. See
# ../Makefile for the actual 3DS build, which this does not touch or
# depend on.
#
# Usage: make -C tests            (build + run all tests)
#        make -C tests coverage   (build + run with gcov line coverage)
#        make -C tests clean
CXX      := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -I../source -g

CORE := ../source/core

# name -> its own test file + whichever core/*.cpp it actually exercises.
SRCS_test_tile      := test_tile.cpp $(CORE)/tile.cpp
SRCS_test_crop      := test_crop.cpp $(CORE)/crop.cpp $(CORE)/tile.cpp
SRCS_test_chunk     := test_chunk.cpp $(CORE)/chunk.cpp $(CORE)/tile.cpp
SRCS_test_world     := test_world.cpp $(CORE)/world.cpp $(CORE)/chunk.cpp $(CORE)/tile.cpp
SRCS_test_inventory := test_inventory.cpp $(CORE)/inventory.cpp $(CORE)/item_db.cpp
SRCS_test_skills    := test_skills.cpp $(CORE)/skills.cpp
SRCS_test_daylight  := test_daylight.cpp $(CORE)/daylight.cpp
SRCS_test_music      := test_music.cpp $(CORE)/music.cpp
SRCS_test_chip_synth := test_chip_synth.cpp $(CORE)/chip_synth.cpp
# wall_cases.inc is generated from the wall lab's verified rule by
# tools/wall_lab/gen_cpp_cases.js - regenerate it there, don't hand-edit.
SRCS_test_wall_autotile := test_wall_autotile.cpp $(CORE)/wall_autotile.cpp
SRCS_test_save      := test_save.cpp $(CORE)/save.cpp $(CORE)/game_state.cpp \
                       $(CORE)/world.cpp $(CORE)/chunk.cpp $(CORE)/crop.cpp \
                       $(CORE)/inventory.cpp $(CORE)/item_db.cpp $(CORE)/tile.cpp \
                       $(CORE)/skills.cpp

TESTS := test_tile test_crop test_chunk test_world test_inventory test_skills test_daylight test_save test_wall_autotile test_music test_chip_synth
BUILD := build
BINS  := $(addprefix $(BUILD)/,$(TESTS))

.PHONY: all run js clean coverage coverage-clean

all: run js

$(BUILD):
	@mkdir -p $(BUILD)

$(BUILD)/%: $(SRCS_$*) | $(BUILD)
	$(CXX) $(CXXFLAGS) -o $@ $(SRCS_$*)

run: $(BINS)
	@status=0; \
	for t in $(BINS); do \
		./$$t || status=1; \
	done; \
	exit $$status

# --- Wall autotiler (JS) --------------------------------------------------
# The dungeon wall rule is authored in tools/wall_lab/scenarios_template.html,
# because it only means anything next to the actual tilesheet - so its test runs
# under node instead of g++. js/rule_harness.js evaluates that page's <script>
# in a stubbed DOM rather than keeping a second copy of the rule here.
#
#   test_wall_rule   the gate: 56 spec cells + 22 whole-render approval snapshots
#   mutation_check   proves the gate bites, by breaking the rule on purpose
#
# HEADS UP: the myfarm-builder container has g++ but NO node, so running
# `make -C tests` in Podman SKIPS these (it prints a notice, it does not fail).
# Run them on the Windows host instead, where node lives:
#
#   node tests/js/test_wall_rule.js
#
# Add nodejs to the Containerfile if you'd rather have one command cover both.
JS_TESTS := js/test_wall_rule.js js/test_kit_mirror.js js/mutation_check.js

js:
	@if command -v node >/dev/null 2>&1; then \
		status=0; \
		for t in $(JS_TESTS); do node $$t || status=1; done; \
		exit $$status; \
	else \
		echo "  skip: node not found, wall autotiler tests not run"; \
	fi

clean:
	rm -rf $(BUILD)

# --- Coverage -------------------------------------------------------------
# Separate object dir + -O0 --coverage build (gcov's line counts get
# unreliable under optimization). Each source is compiled to its own .o
# once (core files are shared across several tests, so this avoids
# recompiling e.g. chunk.cpp three times over), then linked per test.
COV_BUILD := build-cov
COVFLAGS  := -std=c++17 -Wall -Wextra -I../source -g -O0 --coverage

$(COV_BUILD):
	@mkdir -p $(COV_BUILD)

$(COV_BUILD)/%.o: %.cpp | $(COV_BUILD)
	$(CXX) $(COVFLAGS) -c -o $@ $<
$(COV_BUILD)/%.o: $(CORE)/%.cpp | $(COV_BUILD)
	$(CXX) $(COVFLAGS) -c -o $@ $<

obj_for = $(addprefix $(COV_BUILD)/,$(notdir $(subst .cpp,.o,$(SRCS_$1))))

$(COV_BUILD)/test_tile:      $(call obj_for,test_tile)      | $(COV_BUILD)
	$(CXX) $(COVFLAGS) -o $@ $^
$(COV_BUILD)/test_crop:      $(call obj_for,test_crop)      | $(COV_BUILD)
	$(CXX) $(COVFLAGS) -o $@ $^
$(COV_BUILD)/test_chunk:     $(call obj_for,test_chunk)     | $(COV_BUILD)
	$(CXX) $(COVFLAGS) -o $@ $^
$(COV_BUILD)/test_world:     $(call obj_for,test_world)     | $(COV_BUILD)
	$(CXX) $(COVFLAGS) -o $@ $^
$(COV_BUILD)/test_inventory: $(call obj_for,test_inventory) | $(COV_BUILD)
	$(CXX) $(COVFLAGS) -o $@ $^
$(COV_BUILD)/test_skills:    $(call obj_for,test_skills)    | $(COV_BUILD)
	$(CXX) $(COVFLAGS) -o $@ $^
$(COV_BUILD)/test_daylight:  $(call obj_for,test_daylight)  | $(COV_BUILD)
	$(CXX) $(COVFLAGS) -o $@ $^
$(COV_BUILD)/test_save:      $(call obj_for,test_save)      | $(COV_BUILD)
	$(CXX) $(COVFLAGS) -o $@ $^
$(COV_BUILD)/test_wall_autotile: $(call obj_for,test_wall_autotile) | $(COV_BUILD)
	$(CXX) $(COVFLAGS) -o $@ $^
$(COV_BUILD)/test_music:      $(call obj_for,test_music)      | $(COV_BUILD)
	$(CXX) $(COVFLAGS) -o $@ $^
$(COV_BUILD)/test_chip_synth: $(call obj_for,test_chip_synth) | $(COV_BUILD)
	$(CXX) $(COVFLAGS) -o $@ $^

COV_BINS := $(addprefix $(COV_BUILD)/,$(TESTS))

coverage: $(COV_BINS)
	@for t in $(COV_BINS); do ./$$t >/dev/null; done
	@gcov -o $(COV_BUILD) $(wildcard $(CORE)/*.cpp) 2>&1 | grep -A1 "File '$(CORE)/.*\.cpp'"
	@mv -f *.gcov $(COV_BUILD)/ 2>/dev/null || true

coverage-clean:
	rm -rf $(COV_BUILD)
