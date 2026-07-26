#pragma once

#include <cstdint>

namespace core {

enum class Player : uint8_t { P1 = 0, P2 = 1 };

constexpr Player otherPlayer(Player p) {
    return (p == Player::P1) ? Player::P2 : Player::P1;
}

}
