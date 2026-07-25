#include "App.h"

namespace core {

namespace {

int wrapToAxis(int32_t v, int size) {
    int m = static_cast<int>(v % size);
    if (m < 0) m += size;
    return m;
}

}  // namespace

void App::onEncoder(Player p, Axis axis, int32_t delta) {
    Cursor& c = (p == Player::P1) ? p1_ : p2_;
    (axis == Axis::X ? c.x : c.y) += delta;
    render();
}

void App::onButton(bool pressed) {
    button_ = pressed;
    render();
}

void App::render() {
    display_ = DisplayState{};
    display_.p1[wrapToAxis(p1_.y, BOARD_H)][wrapToAxis(p1_.x, BOARD_W)] = CRGB(0, 255, 255);    // P1: cyan
    display_.p2[wrapToAxis(p2_.y, BOARD_H)][wrapToAxis(p2_.x, BOARD_W)] = CRGB(255, 140, 0);   // P2: orange

    CRGB barColor = button_ ? CRGB(255, 0, 0) : CRGB::Black;
    for (auto& c : display_.bar) c = barColor;
}

}
