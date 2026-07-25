#pragma once

#include <cstdint>

#include "Types.h"

// Bare I/O bring-up game: each player's two encoders (left/right, up/down)
// drive a lit cursor pixel around their grid, the button flashes the bar.
// No game rules yet -- this exists purely to prove the input -> display
// pipeline works.
namespace core {

class Game {
public:
    enum class Axis { X, Y };

    void onEncoderInput(Player p, Axis axis, int32_t delta);
    void onButtonInput(bool pressed);

    const DisplayState& display() const { return m_displayState; }

private:
    struct Cursor { int32_t x = 0, y = 0; };

    void render();

    bool m_button = false;
    Cursor m_p1, m_p2;
    DisplayState m_displayState;
};

}
