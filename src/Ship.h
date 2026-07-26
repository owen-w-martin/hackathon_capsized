#pragma once

#include <Arduino.h>
#include <FastLED.h>
#include <algorithm>
#include <vector>
#include <utility>

#include "Display.h"

class Ship {
public:
    Ship() = default;
    Ship(const std::vector<std::pair<int, int>> coords);

    bool checkHit(std::pair<int, int> shot);
    void draw(PlayerScreen& screen);

    bool checkIfNoCellsRemaining() const { return std::all_of(health.begin(), health.end(), [](bool h) { return !h; }); }

    // Adds a healthy cell at pos; for testing ship layouts without real
    // capacitor hardware.
    void addCell(std::pair<int, int> pos);

private:
    std::vector<std::pair<int, int>> coords; // Vector of coordinates representing the ship's position
    std::vector<bool> health; // Vector of health values for each coordinate
};
