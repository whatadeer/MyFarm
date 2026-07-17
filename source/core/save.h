#pragma once

#include <cstdint>
#include <vector>

#include "core/game_state.h"

namespace core {

std::vector<uint8_t> serializeSave(const GameState& state);

// Parses `bytes` into `outState`. Returns false (outState left untouched)
// on a bad magic/version or a truncated buffer - callers treat that the
// same as "no save found" and start a fresh game rather than risk
// misparsing an unrecognized format.
bool deserializeSave(const std::vector<uint8_t>& bytes, GameState* outState);

} // namespace core
