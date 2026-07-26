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

// Decorative 1-pixel border ring around the 10x10 play area -- always on,
// in every state, just a slow, dim breathing black rather than flat off.
void drawAnimatedBorder(PlayerScreen& screen) {
    uint8_t level = beatsin8(6, 0, 6);
    CRGB borderColor = CRGB(level, level, level);

    for (int x = 0; x < cfg::SCREEN_W; x++) {
        screen(x, 0) = borderColor;
        screen(x, cfg::SCREEN_H - 1) = borderColor;
    }
    for (int y = 0; y < cfg::SCREEN_H; y++) {
        screen(0, y) = borderColor;
        screen(cfg::SCREEN_W - 1, y) = borderColor;
    }
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

    // Base layer, every state: the decorative border ring around the 10x10
    // board. Anything drawn below can paint over it (e.g. the crosshair's
    // arms reach all the way through it to the true screen edge).
    drawAnimatedBorder(m_display.player(P1));
    drawAnimatedBorder(m_display.player(P2));

    if (m_state == GameState::SELECTING) {
        // Only the defender's ships are relevant this turn: shown to the
        // offense (revealed, so they can aim) and to the defense (their own
        // board). Same Ship, drawn on both screens, on top of the fills
        // below so it stays visible against them.
        Player defense = otherPlayer(m_currentPlayer);
        Ship& defenseShip = (defense == Player::P1) ? p1Ship : p2Ship;
        const PlayerInput& offenseIn = (m_currentPlayer == Player::P1) ? m_p1 : m_p2;

        // Target cell within the 10x10 grid -- already clamped to range by
        // onEncoderInput, so this is a direct board coordinate.
        int cx = static_cast<int>(offenseIn.x);
        int cy = static_cast<int>(offenseIn.y);

        const CRGB kSlightBlueTint = CRGB(0, 0, 40);

        // Offense aims with a crosshair over a slight blue tint marking the
        // 10x10 playing area.
        PlayerScreen& offenseScreen = m_display.player(toDisplayPlayer(m_currentPlayer));
        for (int y = 0; y < cfg::PLAYING_AREA_H; y++)
            for (int x = 0; x < cfg::PLAYING_AREA_W; x++)
                offenseScreen(cfg::PLAYING_AREA_X0 + x, cfg::PLAYING_AREA_Y0 + y) = kSlightBlueTint;
        defenseShip.draw(offenseScreen);

        // The crosshair is centered on the targeted cell, but its arms run
        // the full width/height of the screen so they reach the border.
        for (int x = 0; x < cfg::SCREEN_W; x++) offenseScreen(x, cfg::PLAYING_AREA_Y0 + cy) = CRGB::Red;
        for (int y = 0; y < cfg::SCREEN_H; y++) offenseScreen(cfg::PLAYING_AREA_X0 + cx, y) = CRGB::Red;

        // Defense's playing area gets a solid blue backdrop, same footprint.
        PlayerScreen& defenseScreen = m_display.player(toDisplayPlayer(defense));
        for (int y = 0; y < cfg::PLAYING_AREA_H; y++)
            for (int x = 0; x < cfg::PLAYING_AREA_W; x++)
                defenseScreen(cfg::PLAYING_AREA_X0 + x, cfg::PLAYING_AREA_Y0 + y) = CRGB::Blue;
        defenseShip.draw(defenseScreen);
    } else {
        int p1x = cfg::PLAYING_AREA_X0 + static_cast<int>(m_p1.x);
        int p1y = cfg::PLAYING_AREA_Y0 + static_cast<int>(m_p1.y);
        m_display.player(P1)(p1x, p1y) = CRGB(0, 255, 255);    // P1: cyan

        int p2x = cfg::PLAYING_AREA_X0 + static_cast<int>(m_p2.x);
        int p2y = cfg::PLAYING_AREA_Y0 + static_cast<int>(m_p2.y);
        m_display.player(P2)(p2x, p2y) = CRGB(255, 140, 0);   // P2: orange
    }

    // Bar is split in half: left reflects P1's button, right reflects P2's.
    // Otherwise it shows the ambient color for the current game state.
    CRGB stateColor = colorForState(m_state);
    CRGB p1BarColor = m_p1.button ? CRGB(255, 0, 0) : stateColor;
    CRGB p2BarColor = m_p2.button ? CRGB(255, 0, 0) : stateColor;
    int half = cfg::SCREEN_W / 2;
    for (int i = 0; i < half; i++)             m_display.bar()(i) = p1BarColor;
    for (int i = half; i < cfg::SCREEN_W; i++) m_display.bar()(i) = p2BarColor;
}

}
