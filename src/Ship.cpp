#include "Ship.h"

#include <algorithm>

Ship::Ship(const std::vector<std::pair<int, int>> coords) {
    this->coords = coords;
    std::sort(this->coords.begin(), this->coords.end()); // pair's operator< already orders by X then Y
    health.resize(this->coords.size(), true); // Initialize all parts as healthy
}

bool Ship::checkHit(std::pair<int, int> shot) {
    for (size_t i = 0; i < coords.size(); ++i) {
        if (coords[i] == shot) {
            health[i] = false; // Mark this part of the ship as damaged (no-op if already dead)
            return true; // Hit -- still a hit even if this cell was already damaged
        }
    }
    return false; // Miss
}

void Ship::addCell(std::pair<int, int> pos) {
    coords.push_back(pos);
    health.push_back(true);
}

void Ship::draw(PlayerScreen& screen, CRGB color) {
    for (size_t i = 0; i < coords.size(); i++) {
        int x = cfg::PLAYING_AREA_X0 + coords[i].first;
        int y = cfg::PLAYING_AREA_Y0 + coords[i].second;
        screen(x, y) = color;
    }
}
