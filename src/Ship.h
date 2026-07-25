#pragma once

#include <Arduino.h>
#include <FastLED.h>
#include <vector>
#include <utility>

#include "Display.h"

class Ship {
public:
    Ship(const std::vector<std::pair<int, int>> coords);

    bool checkHit(std::pair<int, int> shot);
    void draw(PlayerScreen& screen);

private:
    std::vector<std::pair<int, int>> coords; // Vector of coordinates representing the ship's position
    std::vector<bool> health; // Vector of health values for each coordinate
};
