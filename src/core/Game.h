#pragma once

#include <cstdint>

#include "../Display.h"
#include "Types.h"

// Bare I/O bring-up game: each player's two encoders (left/right, up/down)
// drive a lit cursor pixel around their grid, the button flashes the bar.
// Writes straight into the Display it's given -- no intermediate state.
// No game rules yet -- this exists purely to prove the input -> display
// pipeline works.
namespace core {

class Game {
public:
    explicit Game(Display& display) : m_display(display) {}

    enum class Axis { X, Y };

    void onEncoderInput(Player p, Axis axis, int32_t delta);
    void onButtonInput(bool pressed);

    // (Re)draws the current cursor/button state onto the display.
    void processDisplayUpdates();

private:
    struct Cursor { int32_t x = 0, y = 0; };

    Display& m_display;
    bool m_button = false;
    Cursor m_p1, m_p2;
};

}
