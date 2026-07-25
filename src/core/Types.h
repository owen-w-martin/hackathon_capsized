#pragma once

#include "../Display.h"

namespace core {

enum class Player : uint8_t { P1 = 0, P2 = 1 };

// Raw display buffer: two grids (sized to match the physical screens,
// cfg::SCREEN_W/H) + a bar.
struct DisplayState {
    CRGB p1[cfg::SCREEN_H][cfg::SCREEN_W] = {};
    CRGB p2[cfg::SCREEN_H][cfg::SCREEN_W] = {};
    CRGB bar[cfg::SCREEN_W] = {};
};

}
