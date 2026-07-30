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
    // Brief static hold after a shot resolves: same board as SELECTING
    // (ship/shots/crosshair, updated with the just-resolved shot) but with
    // none of SHOOTING's animations, so the result is readable for a beat
    // before the turn changes hands. See main.cpp's SHOOTINGFINISHED case.
    SHOOTINGFINISHED,
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

    // Stashes the shot's outcome and target cell, transitions into
    // SHOOTING, and stamps when it began so processDisplayUpdates can
    // pace the SHOOTING animations off of elapsed time. Deliberately
    // leaves m_currentPlayer as the player who just fired -- the caller
    // swaps it (and calls setState(SELECTING)) once it observes the shot
    // has fully resolved (board select pin off), so offense/defense here
    // keep meaning "who fired" / "who got shot at" for the whole SHOOTING
    // phase.
    //
    // The shot is NOT recorded into the shooter's ShotGrid yet -- and so
    // doesn't show up on any screen yet -- until resolveShot() is called;
    // see there for why.
    void beginShooting(Player shooter, int x, int y, bool hit) {
        m_state = GameState::SHOOTING;
        m_shootingStartMs = millis();
        m_pendingShooter = shooter;
        m_shotX = x;
        m_shotY = y;
        m_pendingHit = hit;
    }

    // Commits the shot stashed by beginShooting() into the shooter's
    // ShotGrid, making the hit/miss marker visible on-screen. Call once
    // the shot has fully resolved (board select pin off) -- not at fire
    // time -- so the defense's screen can't reveal a hit before the pop
    // has actually happened.
    void resolveShot() {
        ShotGrid& grid = (m_pendingShooter == Player::P1) ? m_p1Shots : m_p2Shots;
        grid(m_shotX, m_shotY) = m_pendingHit ? ShotResult::HIT : ShotResult::MISS;
    }

    // Whose turn it is during SELECTING.
    Player getCurrentPlayer() const { return m_currentPlayer; }
    void setCurrentPlayer(Player p) { m_currentPlayer = p; }

    const PlayerInput& getPlayerInput(Player p) const { return (p == Player::P1) ? m_p1 : m_p2; }

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
    int32_t m_shotX = 0, m_shotY = 0;      // set by beginShooting(); the SHOOTING animation's target cell
    uint32_t m_shootingStartMs = 0;        // set by beginShooting(); paces the SHOOTING animations
    Player m_pendingShooter = Player::P1;  // set by beginShooting(); committed to a ShotGrid by resolveShot()
    bool m_pendingHit = false;             // set by beginShooting(); committed to a ShotGrid by resolveShot()
};

}
