#pragma once

#include "core/Game.h"

// Sets up interrupt-driven reads of one physical button and one rotary
// encoder. Call once from setup().
void hardwareIOBegin();

// Applies any button/encoder activity accumulated (via interrupts) since
// the last call to game. Returns true if anything was applied.
bool pollAndApplyHardware(core::Game& game);
