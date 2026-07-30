#pragma once

#include <cstdint>

#include "Display.h"
#include "Types.h"

class Ship;

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

// Per-cell record of a player's own shots against their opponent's board:
// nothing yet, a miss, or a hit.
enum class ShotResult { NONE, MISS, HIT };

// One 10x10 grid of ShotResult per shooting player, indexed the same way as
// the playing area itself.
class ShotGrid {
public:
    ShotResult&       operator()(int x, int y)       { return _cells[y * cfg::PLAYING_AREA_W + x]; }
    const ShotResult& operator()(int x, int y) const { return _cells[y * cfg::PLAYING_AREA_W + x]; }

private:
    ShotResult _cells[cfg::PLAYING_AREA_W * cfg::PLAYING_AREA_H] = {};
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
    void resetButtonPresses() {
        m_p1.everPressed = m_p2.everPressed = false;
        // Also clear justPressed -- otherwise the button hold used to
        // start the game reads back as a fresh, un-aimed shot the moment
        // SELECTING begins.
        m_p1.justPressed = m_p2.justPressed = false;
    }

    // True if the given player's button was freshly pressed since the last
    // call for that player; clears the flag on read.
    bool consumePress(Player p);

    GameState getState() const { return m_state; }
    void setState(GameState state) { m_state = state; }

    // Transitions into SHOOTING and stamps when it began, so
    // processDisplayUpdates can pace the shooting-phase animations off of
    // elapsed time. Deliberately leaves m_currentPlayer as the player who
    // just fired -- the caller swaps it (and calls setState(SELECTING))
    // once it observes the shot has fully resolved (board select pin
    // off), so offense/defense here keep meaning "who fired" / "who got
    // shot at" for the whole SHOOTING phase.
    void beginShooting() {
        m_state = GameState::SHOOTING;
        m_shootingStartMs = millis();
    }

    // Whose turn it is during SELECTING.
    Player getCurrentPlayer() const { return m_currentPlayer; }
    void setCurrentPlayer(Player p) { m_currentPlayer = p; }

    const PlayerInput& getPlayerInput(Player p) const { return (p == Player::P1) ? m_p1 : m_p2; }

    // Records the outcome of a shot the given player just fired against
    // their opponent's board, for later display on that player's own
    // screen. Also stashes (x, y) as the shooting-phase animation's target
    // cell (see beginShooting()).
    void recordShot(Player shooter, int x, int y, bool hit) {
        ShotGrid& grid = (shooter == Player::P1) ? m_p1Shots : m_p2Shots;
        grid(x, y) = hit ? ShotResult::HIT : ShotResult::MISS;
        m_shotX = x;
        m_shotY = y;
    }

    const ShotGrid& getShots(Player shooter) const { return (shooter == Player::P1) ? m_p1Shots : m_p2Shots; }

    // (Re)draws the current cursor/button/ship state onto the display. Ships
    // are the base layer, so state-driven overlays (concealment, crosshair,
    // border) composite on top of them correctly.
    void processDisplayUpdates(Ship& p1Ship, Ship& p2Ship);

private:
    Display& m_display;
    PlayerInput m_p1, m_p2;
    GameState m_state = GameState::IDLE;
    Player m_currentPlayer = Player::P1;
    ShotGrid m_p1Shots, m_p2Shots;   // each player's own shot history against their opponent
    int32_t m_shotX = 0, m_shotY = 0;      // set by recordShot(); the SHOOTING animation's target cell
    uint32_t m_shootingStartMs = 0;        // set by beginShooting(); paces the SHOOTING animations
};

}
