#pragma once

#include <cstdint>

#include "Display.h"
#include "Types.h"

// Bare I/O bring-up game: each player has two encoders (left/right,
// up/down) driving a lit cursor pixel, and a button that lights their half
// of the bar red while held. Writes straight into the Display it's given --
// no intermediate state. No game rules yet -- this exists purely to prove
// the input -> display pipeline works.
namespace core {

enum class GameState {
    IDLE,
    SHIPSELECT,
    SELECTING,
    SHOOTING,
    ENDSCREEN
};

class Game {
public:
    struct PlayerInput {
        int32_t x = 0, y = 0;   // driven by that player's two encoders
        bool button = false;
        bool everPressed = false;
        bool justPressed = false;
    };

    explicit Game(Display& display) : m_display(display) {}

    enum class Axis { X, Y };

    void onEncoderInput(Player p, Axis axis, int32_t delta);
    void onButtonInput(Player p, bool pressed);

    // True once the given player's button has been pressed at least once.
    bool hasPressedButton(Player p) const;
    void resetButtonPresses() { m_p1.everPressed = m_p2.everPressed = false; }

    // True if the given player's button was freshly pressed since the last
    // call for that player; clears the flag on read.
    bool consumePress(Player p);

    GameState getState() const { return m_state; }
    void setState(GameState state) { m_state = state; }

    // Whose turn it is during SELECTING.
    Player getCurrentPlayer() const { return m_currentPlayer; }
    void setCurrentPlayer(Player p) { m_currentPlayer = p; }

    const PlayerInput& getPlayerInput(Player p) const { return (p == Player::P1) ? m_p1 : m_p2; }

    // (Re)draws the current cursor/button state onto the display.
    void processDisplayUpdates();

private:
    Display& m_display;
    PlayerInput m_p1, m_p2;
    GameState m_state = GameState::IDLE;
    Player m_currentPlayer = Player::P1;
};

}
