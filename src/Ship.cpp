#include "Ship.h"

Ship::Ship(const std::vector<std::pair<int, int>> coords) {
    this->coords = coords;
    health.resize(coords.size(), true); // Initialize all parts as healthy
}

bool Ship::checkHit(std::pair<int, int> shot) {
    for (size_t i = 0; i < coords.size(); ++i) {
        if (coords[i] == shot && health[i]) {
            health[i] = false; // Mark this part of the ship as damaged
            return true; // Hit
        }
    }
    return false; // Miss
}

void Ship::draw(PlayerScreen& screen) {
    for (size_t i = 0; i < coords.size(); i++) {
        if (health[i]) {
            screen(coords[i].first, coords[i].second) = CRGB::White; // Healthy part
        } else {
            screen(coords[i].first, coords[i].second) = CRGB::Red; // Damaged part
        }
    }
}
