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

// Timing for the SHOOTING-state shot reveal animation (see
// Game::processDisplayUpdates): the offense's crosshair fades out, regrows
// from the screen edges in red as the shot is charging, then -- once the
// shot has actually resolved (cap popped or the safety timeout hit, see
// Game::resolveShot()) -- snaps fully shut, holds, fades to black, and
// fades back in on the result. kTotalRevealMs (from the moment of
// resolution) is what Game::isShotRevealComplete() waits out before the
// caller is allowed to leave SHOOTING.
namespace shotReveal {
    constexpr uint32_t kCrosshairFadeOutMs = 300;   // initial fade of the aiming crosshair
    // Ease-out time constant for the red regrowth -- kept at 1/5 of
    // Board's cfg::POP_TIMEOUT_MS (see Board.h) so the close-up nears the
    // target at roughly the same pace the timeout is scaled to. The
    // target cell itself is never lit by this growth -- see
    // Game::processDisplayUpdates -- so it stays a surprise until resolved.
    constexpr float     kGrowthTauMs       = 1600;
    constexpr uint32_t kFadeToBlackMs      = 250;   // once resolved: fade everything out
    constexpr uint32_t kFadeInTargetMs     = 1000;  // then fade in just the target pixel's result
    constexpr uint32_t kFadeInRestMs       = 1600;  // then fade in everything else around it
    constexpr uint32_t kTotalRevealMs      = kFadeToBlackMs + kFadeInTargetMs + kFadeInRestMs;
}

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
        m_shotResolved = false;
    }

    // Commits the shot stashed by beginShooting() into the shooter's
    // ShotGrid, making the hit/miss marker visible on-screen, and stamps
    // when that happened so the reveal animation in processDisplayUpdates
    // can time itself off the real event (cap popped or the safety
    // timeout hit) instead of a fixed countdown. Idempotent -- only the
    // first call after beginShooting() has any effect -- so the caller can
    // call it every loop() once the board reports done, rather than having
    // to track "did I already call this" itself.
    void resolveShot() {
        if (m_shotResolved) return;
        m_shotResolved = true;
        m_shotResolvedMs = millis();
        ShotGrid& grid = (m_pendingShooter == Player::P1) ? m_p1Shots : m_p2Shots;
        grid(m_shotX, m_shotY) = m_pendingHit ? ShotResult::HIT : ShotResult::MISS;
    }

    // True once the shot has resolved AND the post-resolution reveal
    // animation (snap shut, hold, fade to black, fade back in) has played
    // out on the offense's screen -- i.e. it's safe to leave SHOOTING.
    bool isShotRevealComplete() const {
        if (!m_shotResolved) return false;
        return millis() - m_shotResolvedMs >= shotReveal::kTotalRevealMs;
    }

    // Whose turn it is during SELECTING.
    Player getCurrentPlayer() const { return m_currentPlayer; }
    void setCurrentPlayer(Player p) { m_currentPlayer = p; }

    // Who sank the other fleet. Set alongside the transition into ENDSCREEN
    // and read by processDisplayUpdates to decide which panel gets the W and
    // which gets the L. Recorded explicitly rather than inferred from
    // m_currentPlayer: that happens to still hold the winner today (nothing
    // swaps it on the way into ENDSCREEN), but relying on it would silently
    // put the W on the wrong screen the moment that changes.
    Player getWinner() const { return m_winner; }
    void setWinner(Player p) { m_winner = p; }

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
    Player m_winner = Player::P1;          // only meaningful in ENDSCREEN; see setWinner()
    ShotGrid m_p1Shots, m_p2Shots;   // each player's own shot history against their opponent
    int32_t m_shotX = 0, m_shotY = 0;      // set by beginShooting(); the SHOOTING animation's target cell
    uint32_t m_shootingStartMs = 0;        // set by beginShooting(); paces the SHOOTING animations
    Player m_pendingShooter = Player::P1;  // set by beginShooting(); committed to a ShotGrid by resolveShot()
    bool m_pendingHit = false;             // set by beginShooting(); committed to a ShotGrid by resolveShot()
    bool m_shotResolved = false;           // set by resolveShot(); gates the SHOOTING reveal animation's phases
    uint32_t m_shotResolvedMs = 0;         // set by resolveShot(); paces the reveal animation's phases
};

}
