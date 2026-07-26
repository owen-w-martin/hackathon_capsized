#pragma once

#include "../Game.h"
#include "../Ship.h"

// Reads and applies at most one full line from Serial to game (or, for
// SHIP1/SHIP2 test commands, to the given ship). Returns true only when a
// valid event was parsed and applied this call.
bool pollAndApplySerial(core::Game& game, Ship& p1Ship, Ship& p2Ship);

// Encodes and prints display's current pixel state over Serial.
void printToSerial(const Display& display);
