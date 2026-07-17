#pragma once

#include "core/game_state.h"

namespace platform {

inline constexpr const char* kSaveDir = "sdmc:/3ds/myfarm";
inline constexpr const char* kSavePath = "sdmc:/3ds/myfarm/save.dat";

// Writes state to sdmc:/3ds/myfarm/save.dat, creating the parent
// directories first if needed (plain fopen() won't create them). Returns
// false on any I/O failure.
bool saveToDisk(const core::GameState& state);

// Reads and parses sdmc:/3ds/myfarm/save.dat into outState. Returns false
// if the file doesn't exist, can't be read, or fails to parse (bad
// magic/version/truncated) - callers treat all of these the same as "no
// save found" and start a fresh game.
bool loadFromDisk(core::GameState* outState);

} // namespace platform
