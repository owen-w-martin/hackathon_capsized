#include "Game.h"

namespace core {

namespace {

int wrapToAxis(int32_t v, int size) {
    int m = static_cast<int>(v % size);
    if (m < 0) m += size;
    return m;
}

}  // namespace

void Game::onEncoderInput(Player p, Axis axis, int32_t delta) {
    Cursor& c = (p == Player::P1) ? m_p1 : m_p2;
    (axis == Axis::X ? c.x : c.y) += delta;
    render();
}

void Game::onButtonInput(bool pressed) {
    m_button = pressed;
    render();
}

void Game::render() {
    m_displayState = DisplayState{};
    m_displayState.p1[wrapToAxis(m_p1.y, cfg::SCREEN_H)][wrapToAxis(m_p1.x, cfg::SCREEN_W)] = CRGB(0, 255, 255);    // P1: cyan
    m_displayState.p2[wrapToAxis(m_p2.y, cfg::SCREEN_H)][wrapToAxis(m_p2.x, cfg::SCREEN_W)] = CRGB(255, 140, 0);   // P2: orange

    CRGB barColor = m_button ? CRGB(255, 0, 0) : CRGB::Black;
    for (auto& c : m_displayState.bar) c = barColor;
}

}
