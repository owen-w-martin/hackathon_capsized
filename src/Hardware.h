#pragma once

#include <Arduino.h>
#include <cstdint>

#include "Display.h"

namespace cfg {
    constexpr uint8_t ROW_PINS[4]       = {41, 42, 43, 44};
    constexpr uint8_t COL_PINS[4]       = {32, 33, 36, 27};
    constexpr uint8_t P1_EXCITE_PIN     = 26;
    constexpr uint8_t P2_EXCITE_PIN     = 25;
    constexpr uint8_t CURRENT_LEVEL_PIN = 32;
    constexpr uint8_t CURRENT_SENSE_PIN = 19;

    // TODO: calibrate these against the real current-sense circuit.
    constexpr float ADC_REF_VOLTAGE           = 3.3f;
    constexpr int   ADC_MAX_COUNT             = 1023;   // 10-bit analogRead default
    constexpr float CURRENT_SENSE_MA_PER_VOLT = 1000.0f;
}

// Owns every physical pin for the capacitor popping-board: a 4-bit row
// address bus, a 4-bit column address bus, one excitation line per
// player's board, a current-level select line, and an analog current-sense
// input. This is the only place that should ever call
// pinMode/digitalWrite/analogRead directly -- everything else talks to the
// board through these methods.
class Hardware {
public:
    Hardware();

    enum class CurrentLevel : uint8_t { Detect = LOW, Pop = HIGH };

    // Drives the 4-bit row/column address bus to select one cell.
    void selectCell(uint8_t row, uint8_t col) const;

    // Enables this player's board excitation and disables the other
    // player's, so at most one board is ever live at a time.
    void excite(Player p, bool on) const;

    void setCurrentLevel(CurrentLevel level) const;

    // Reads the sense pin and converts it to milliamps.
    float readCurrentMa() const;
};
