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
        for (PlayerInput& in : m_players) {
            in.everPressed = false;
            // Also clear justPressed -- otherwise the button hold used to
            // start the game reads back as a fresh, un-aimed shot the moment
            // SELECTING begins.
            in.justPressed = false;
        }
    }

    // True if the given player's button was freshly pressed since the last
    // call for that player; clears the flag on read.
    bool consumePress(Player p);

    GameState getState() const { return m_state; }
    void setState(GameState state) { m_state = state; }

    // Whose turn it is during SELECTING/SHOOTING.
    Player getCurrentPlayer() const { return m_currentPlayer; }
    void setCurrentPlayer(Player p) { m_currentPlayer = p; }

    // Hands the turn to the other player. Called when SHOOTING is done
    // resolving a shot and control passes back to SELECTING.
    void endTurn() { m_currentPlayer = otherPlayer(m_currentPlayer); }

    const PlayerInput& getPlayerInput(Player p) const { return playerInput(p); }

    // Records the outcome of a shot the given player just fired against
    // their opponent's board, for later display on that player's own screen.
    // Also flags the bar to flash red for the next processDisplayUpdates.
    void recordShot(Player shooter, int x, int y, bool hit) {
        shotGrid(shooter)(x, y) = hit ? ShotResult::HIT : ShotResult::MISS;
        m_justFired = true;
    }

    const ShotGrid& getShots(Player shooter) const { return shotGrid(shooter); }

    // (Re)draws the current cursor/button/ship state onto the display. Ships
    // are the base layer, so state-driven overlays (concealment, crosshair,
    // border) composite on top of them correctly.
    void processDisplayUpdates(Ship& p1Ship, Ship& p2Ship);

private:
    // Per-player state lives in 2-element arrays indexed by Player (P1 = 0,
    // P2 = 1), so "this player's slot" vs. "the other player's slot" is a
    // single indexed access instead of a P1/P2 ternary repeated at every use.
    PlayerInput&       playerInput(Player p)       { return m_players[static_cast<size_t>(p)]; }
    const PlayerInput& playerInput(Player p) const { return m_players[static_cast<size_t>(p)]; }
    ShotGrid&       shotGrid(Player p)       { return m_shots[static_cast<size_t>(p)]; }
    const ShotGrid& shotGrid(Player p) const { return m_shots[static_cast<size_t>(p)]; }

    Display& m_display;
    PlayerInput m_players[2];
    GameState m_state = GameState::IDLE;
    Player m_currentPlayer = Player::P1;
    ShotGrid m_shots[2];       // each player's own shot history against their opponent
    bool m_justFired = false; // set by recordShot(), consumed by the next processDisplayUpdates
};

}
