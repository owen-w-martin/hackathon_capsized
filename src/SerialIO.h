#pragma once

#include "core/Game.h"
#include "core/Types.h"

// Reads and applies at most one full line from Serial to game. Returns true
// only when a valid event was parsed and applied this call.
bool pollAndApplySerial(core::Game& game);

// Encodes and prints a display-state dump over Serial.
void printToSerial(const core::DisplayState& d);
