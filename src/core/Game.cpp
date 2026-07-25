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
    processDisplayUpdates();
}

void Game::onButtonInput(bool pressed) {
    m_button = pressed;
    processDisplayUpdates();
}

void Game::processDisplayUpdates() {
    m_display.clear();

    int p1x = wrapToAxis(m_p1.x, cfg::SCREEN_W);
    int p1y = wrapToAxis(m_p1.y, cfg::SCREEN_H);
    m_display.player(P1)(p1x, p1y) = CRGB(0, 255, 255);    // P1: cyan

    int p2x = wrapToAxis(m_p2.x, cfg::SCREEN_W);
    int p2y = wrapToAxis(m_p2.y, cfg::SCREEN_H);
    m_display.player(P2)(p2x, p2y) = CRGB(255, 140, 0);   // P2: orange

    CRGB barColor = m_button ? CRGB(255, 0, 0) : CRGB::Black;
    for (int i = 0; i < cfg::SCREEN_W; i++) m_display.bar()(i) = barColor;

}

}
