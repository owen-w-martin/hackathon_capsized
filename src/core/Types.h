#pragma once

#include <FastLED.h>

namespace core {

enum class Player : uint8_t { P1 = 0, P2 = 1 };

constexpr int BOARD_W = 12;
constexpr int BOARD_H = 12;

// Raw display buffer: two 12x12 player grids + a 12-pixel bar.
struct DisplayState {
    CRGB p1[BOARD_H][BOARD_W] = {};
    CRGB p2[BOARD_H][BOARD_W] = {};
    CRGB bar[BOARD_W] = {};
};

}
