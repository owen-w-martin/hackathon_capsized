#pragma once

#include <cstdint>

#include "Display.h"

namespace core {

enum class Player : uint8_t { P1 = 0, P2 = 1 };

constexpr Player otherPlayer(Player p) {
    return (p == Player::P1) ? Player::P2 : Player::P1;
}

// Maps this project's Player (used throughout Game/input logic) to the
// Display/Board's own Player enum (used to index screens and hardware).
constexpr ::Player toDisplayPlayer(Player p) {
    return (p == Player::P1) ? ::P1 : ::P2;
}

}
