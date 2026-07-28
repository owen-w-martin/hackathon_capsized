#include "Game.h"

#include "Ship.h"

namespace core {

namespace {

int32_t clampToAxis(int32_t v, int size) {
    if (v < 0) return 0;
    if (v > size - 1) return size - 1;
    return v;
}

// Ambient color shown on the top bar for the current game state.
CRGB colorForState(GameState state) {
    switch (state) {
        case GameState::IDLE:       return CRGB::Blue;
        case GameState::SHIPSELECT: return CRGB::Black;
        case GameState::SELECTING:  return CRGB::Green;
        case GameState::SHOOTING:   return CRGB::Black;
        case GameState::ENDSCREEN:  return CRGB::Pink;
    }
    return CRGB::Black;
}

::Player toDisplayPlayer(Player p) {
    return (p == Player::P1) ? ::P1 : ::P2;
}

}  // namespace

void Game::onEncoderInput(Player p, Axis axis, int32_t delta) {
    PlayerInput& in = (p == Player::P1) ? m_p1 : m_p2;
    int32_t step = (delta > 0) ? 1 : -1;   // any rotation this way moves exactly one block
    // y=0 is the bottom of the screen (see Display::show()), so moving "up"
    // must increase y -- the opposite of the raw encoder step.
    if (axis == Axis::Y) step = -step;
    int32_t& coord = (axis == Axis::X) ? in.x : in.y;
    int size = (axis == Axis::X) ? cfg::PLAYING_AREA_W : cfg::PLAYING_AREA_H;
    coord = clampToAxis(coord + step, size);
}

void Game::onButtonInput(Player p, bool pressed) {
    PlayerInput& in = (p == Player::P1) ? m_p1 : m_p2;
    bool wasPressed = in.button;
    in.button = pressed;
    if (pressed) in.everPressed = true;
    if (pressed && !wasPressed) in.justPressed = true;
}

bool Game::hasPressedButton(Player p) const {
    const PlayerInput& in = (p == Player::P1) ? m_p1 : m_p2;
    return in.everPressed;
}

bool Game::consumePress(Player p) {
    PlayerInput& in = (p == Player::P1) ? m_p1 : m_p2;
    bool result = in.justPressed;
    in.justPressed = false;
    return result;
}

void Game::processDisplayUpdates(Ship& p1Ship, Ship& p2Ship) {
    m_display.clear();

    if (m_state == GameState::SELECTING) {
        // The defender's ship is only relevant for the defense's own screen
        // now -- the offense no longer gets to see it directly, only their
        // own shot history against it.
        Player defense = otherPlayer(m_currentPlayer);
        Ship& defenseShip = (defense == Player::P1) ? p1Ship : p2Ship;
        const PlayerInput& offenseIn = (m_currentPlayer == Player::P1) ? m_p1 : m_p2;

        // Target cell within the 10x10 grid -- already clamped to range by
        // onEncoderInput, so this is a direct board coordinate.
        int cx = static_cast<int>(offenseIn.x);
        int cy = static_cast<int>(offenseIn.y);

        const CRGB kOffenseBlue = CRGB(0, 0, 10);

        // Offense aims with a crosshair over their own shot history: cells
        // they haven't fired at yet keep the faint blue tint; cells they
        // have are white (miss) or red (hit). The actual ship layout stays
        // hidden -- this is the only thing drawn on the offense's screen.
        PlayerScreen& offenseScreen = m_display.player(toDisplayPlayer(m_currentPlayer));
        const ShotGrid& offenseShots = getShots(m_currentPlayer);
        for (int y = 0; y < cfg::PLAYING_AREA_H; y++) {
            for (int x = 0; x < cfg::PLAYING_AREA_W; x++) {
                CRGB c = kOffenseBlue;
                switch (offenseShots(x, y)) {
                    case ShotResult::MISS: c = CRGB::White; break;
                    case ShotResult::HIT:  c = CRGB::Red;   break;
                    case ShotResult::NONE: break;
                }
                offenseScreen(cfg::PLAYING_AREA_X0 + x, cfg::PLAYING_AREA_Y0 + y) = c;
            }
        }

        // The crosshair is centered on the targeted cell, but its arms run
        // the full width/height of the screen so they reach the border. It's
        // blended (rather than drawn solid) so it reads as a translucent
        // overlay: a previous shot marker it crosses -- white for a miss,
        // red for a hit -- still shows through underneath instead of being
        // clobbered.
        const CRGB kCrosshairColor = CRGB::Red;
        const uint8_t kCrosshairBlend = 30; // 0..255 -- amount of kCrosshairColor mixed in

        for (int x = 0; x < cfg::SCREEN_W; x++) {
            CRGB& px = offenseScreen(x, cfg::PLAYING_AREA_Y0 + cy);
            px = blend(px, kCrosshairColor, kCrosshairBlend);
        }
        for (int y = 0; y < cfg::SCREEN_H; y++) {
            CRGB& px = offenseScreen(cfg::PLAYING_AREA_X0 + cx, y);
            px = blend(px, kCrosshairColor, kCrosshairBlend);
        }

        // Defense's playing area gets a solid blue backdrop, same footprint,
        // with their own fleet's positions shown in gray, then the offense's
        // shots against them overlaid on top -- white for a miss, red for a
        // hit on one of their own ship cells -- so defense can see both
        // their fleet and how it's being hit at the same time.
        PlayerScreen& defenseScreen = m_display.player(toDisplayPlayer(defense));
        const CRGB kDefenseBlue = CRGB(0, 0, 40);
        for (int y = 0; y < cfg::PLAYING_AREA_H; y++)
            for (int x = 0; x < cfg::PLAYING_AREA_W; x++)
                defenseScreen(cfg::PLAYING_AREA_X0 + x, cfg::PLAYING_AREA_Y0 + y) = kDefenseBlue;
        // CRGB::Gray (128,128,128) reads as near-white on the LEDs -- dim it
        // down so it's clearly distinct from the white miss marker below.
        const CRGB kShipGray = CRGB(40, 40, 40);
        defenseShip.draw(defenseScreen, kShipGray);
        for (int y = 0; y < cfg::PLAYING_AREA_H; y++) {
            for (int x = 0; x < cfg::PLAYING_AREA_W; x++) {
                switch (offenseShots(x, y)) {
                    case ShotResult::MISS:
                        defenseScreen(cfg::PLAYING_AREA_X0 + x, cfg::PLAYING_AREA_Y0 + y) = CRGB::White;
                        break;
                    case ShotResult::HIT:
                        defenseScreen(cfg::PLAYING_AREA_X0 + x, cfg::PLAYING_AREA_Y0 + y) = CRGB::Red;
                        break;
                    case ShotResult::NONE:
                        break;
                }
            }
        }

        // Top bar: highlights the column the offense is currently aiming at
        // in red, black everywhere else -- lines up with the crosshair's
        // vertical arm since it shares the same x coordinate. On the frame a
        // shot was just fired, the whole bar flashes red instead.
        //
        // The bar's columns map straight to physical columns (see
        // Display::show()), but P1's screen is physically mirrored
        // end-to-end there. So on P1's turn, flip the bar column the same
        // way to keep it lined up with P1's screen; on P2's turn, whose
        // screen isn't mirrored, use it as-is.
        int barCol = cfg::PLAYING_AREA_X0 + cx;
        if (m_currentPlayer == Player::P1) {
            barCol = cfg::SCREEN_W - 1 - barCol;
        }

        if (m_justFired) {
            m_display.bar().fill(CRGB::Red);
            m_justFired = false;
        } else {
            m_display.bar().clear();
            m_display.bar()(barCol) = CRGB::Red;
        }
    } else {
        // Outside SELECTING (e.g. IDLE), the player screens stay solid
        // black -- no cursor.

        // Bar is split in half: left reflects P1's button, right reflects
        // P2's. Otherwise it shows the ambient color for the current game
        // state.
        CRGB stateColor = colorForState(m_state);
        CRGB p1BarColor = m_p1.button ? CRGB(255, 0, 0) : stateColor;
        CRGB p2BarColor = m_p2.button ? CRGB(255, 0, 0) : stateColor;
        int half = cfg::SCREEN_W / 2;
        for (int i = 0; i < half; i++)             m_display.bar()(i) = p1BarColor;
        for (int i = half; i < cfg::SCREEN_W; i++) m_display.bar()(i) = p2BarColor;
    }
}

}
