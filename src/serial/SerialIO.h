#pragma once

#include "../Game.h"

// Reads and applies at most one full line from Serial to game. Returns true
// only when a valid event was parsed and applied this call.
bool pollAndApplySerial(core::Game& game);

// Encodes and prints display's current pixel state over Serial.
void printToSerial(const Display& display);
