#pragma once

#include <cstdint>

#include "../Display.h"
#include "Types.h"

// Bare I/O bring-up game: each player has two encoders (left/right,
// up/down) driving a lit cursor pixel, and a button that lights their half
// of the bar red while held. Writes straight into the Display it's given --
// no intermediate state. No game rules yet -- this exists purely to prove
// the input -> display pipeline works.
namespace core {

class Game {
public:
    explicit Game(Display& display) : m_display(display) {}

    enum class Axis { X, Y };

    void onEncoderInput(Player p, Axis axis, int32_t delta);
    void onButtonInput(Player p, bool pressed);

    // (Re)draws the current cursor/button state onto the display.
    void processDisplayUpdates();

    // Current (live, not edge-triggered) pressed state of a player's button.
    bool buttonPressed(Player p) const { return (p == Player::P1) ? m_p1.button : m_p2.button; }

private:
    struct PlayerInput {
        int32_t x = 0, y = 0;   // driven by that player's two encoders
        bool button = false;
    };

    Display& m_display;
    PlayerInput m_p1, m_p2;
};

}
