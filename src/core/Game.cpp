#include "Game.h"

namespace core {

namespace {

int wrapToAxis(int32_t v, int size) {
    int m = static_cast<int>(v % size);
    if (m < 0) m += size;
    return m;
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

Player otherPlayer(Player p) {
    return (p == Player::P1) ? Player::P2 : Player::P1;
}

}  // namespace

void Game::onEncoderInput(Player p, Axis axis, int32_t delta) {
    PlayerInput& in = (p == Player::P1) ? m_p1 : m_p2;
    int32_t step = (delta > 0) ? 1 : -1;   // any rotation this way moves exactly one block
    (axis == Axis::X ? in.x : in.y) += step;
    processDisplayUpdates();
}

void Game::onButtonInput(Player p, bool pressed) {
    PlayerInput& in = (p == Player::P1) ? m_p1 : m_p2;
    bool wasPressed = in.button;
    in.button = pressed;
    if (pressed) in.everPressed = true;

    // A fresh press from the player on offense ends their turn.
    if (m_state == GameState::SELECTING && pressed && !wasPressed && p == m_currentPlayer) {
        m_currentPlayer = otherPlayer(m_currentPlayer);
    }

    processDisplayUpdates();
}

bool Game::hasPressedButton(Player p) const {
    const PlayerInput& in = (p == Player::P1) ? m_p1 : m_p2;
    return in.everPressed;
}

void Game::transitionState() {
    switch (m_state) {
        case GameState::IDLE:
            if (hasPressedButton(Player::P1) && hasPressedButton(Player::P2)) {
                resetButtonPresses();
                m_currentPlayer = Player::P1;
                m_state = GameState::SELECTING;
            }
            break;
        case GameState::SHIPSELECT:
            // not used
            break;
        case GameState::SELECTING:

            // selecting

            // shooting

            // check if victory
            break;
        case GameState::SHOOTING:
            break;
        case GameState::ENDSCREEN:
            break;
    }
}

void Game::processDisplayUpdates() {
    m_display.clear();

    if (m_state == GameState::SELECTING) {
        // Offense (whoever's turn it is) aims with a crosshair; defense's
        // board is hidden behind a solid blue screen.
        Player defense = otherPlayer(m_currentPlayer);
        PlayerInput& offenseIn = (m_currentPlayer == Player::P1) ? m_p1 : m_p2;
        int cx = wrapToAxis(offenseIn.x, cfg::SCREEN_W);
        int cy = wrapToAxis(offenseIn.y, cfg::SCREEN_H);

        PlayerScreen& offenseScreen = m_display.player(toDisplayPlayer(m_currentPlayer));
        for (int x = 0; x < cfg::SCREEN_W; x++) offenseScreen(x, cy) = CRGB::Red;
        for (int y = 0; y < cfg::SCREEN_H; y++) offenseScreen(cx, y) = CRGB::Red;

        m_display.player(toDisplayPlayer(defense)).fill(CRGB::Blue);
    } else {
        int p1x = wrapToAxis(m_p1.x, cfg::SCREEN_W);
        int p1y = wrapToAxis(m_p1.y, cfg::SCREEN_H);
        m_display.player(P1)(p1x, p1y) = CRGB(0, 255, 255);    // P1: cyan

        int p2x = wrapToAxis(m_p2.x, cfg::SCREEN_W);
        int p2y = wrapToAxis(m_p2.y, cfg::SCREEN_H);
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
