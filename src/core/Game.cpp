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
    PlayerInput& in = (p == Player::P1) ? m_p1 : m_p2;
    int32_t step = (delta > 0) ? 1 : -1;   // any rotation this way moves exactly one block
    (axis == Axis::X ? in.x : in.y) += step;
    processDisplayUpdates();
}

void Game::onButtonInput(Player p, bool pressed) {
    PlayerInput& in = (p == Player::P1) ? m_p1 : m_p2;
    in.button = pressed;
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

    // Bar is split in half: left reflects P1's button, right reflects P2's.
    CRGB p1BarColor = m_p1.button ? CRGB(255, 0, 0) : CRGB::Black;
    CRGB p2BarColor = m_p2.button ? CRGB(255, 0, 0) : CRGB::Black;
    int half = cfg::SCREEN_W / 2;
    for (int i = 0; i < half; i++)             m_display.bar()(i) = p1BarColor;
    for (int i = half; i < cfg::SCREEN_W; i++) m_display.bar()(i) = p2BarColor;
}

}
