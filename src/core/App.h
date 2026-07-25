#pragma once

#include <cstdint>

#include "Types.h"

// Bare I/O bring-up app: each player's two encoders (left/right, up/down)
// drive a lit cursor pixel around their grid, the button flashes the bar.
// No game rules -- this exists purely to prove the input -> display
// pipeline works.
namespace core {

class App {
public:
    enum class Axis { X, Y };

    void onEncoder(Player p, Axis axis, int32_t delta);
    void onButton(bool pressed);

    const DisplayState& display() const { return display_; }

private:
    struct Cursor { int32_t x = 0, y = 0; };

    void render();

    bool button_ = false;
    Cursor p1_, p2_;
    DisplayState display_;
};

}
