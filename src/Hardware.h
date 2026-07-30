#pragma once

#include <Arduino.h>
#include <cstdint>

#include "Display.h"

namespace cfg {
    // Cells addressable per axis on the row/column select bus. This is the
    // single source of truth for board size -- Board.h's BOARD_W/BOARD_H
    // reference it rather than redeclaring it.
    constexpr uint8_t BOARD_SIZE = 10;

    constexpr uint8_t ROW_PINS[4]       = {22, 21, 20, 19};
    constexpr uint8_t COL_PINS[4]       = {15, 14, 41, 40};

    // constexpr uint8_t ROW_PINS[4]       = {19, 20, 21, 22};
    // constexpr uint8_t COL_PINS[4]       = {40, 41, 14, 15};
    constexpr uint8_t BOARD_SEL_P2     = 34;
    constexpr uint8_t BOARD_SEL_P1     = 33;
    constexpr uint8_t CURRENT_LEVEL_PIN = 32;
    constexpr uint8_t CURRENT_SENSE_PIN = 27;

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

    // Raw address-bus writes for probing the demux directly: no rotation
    // compensation (see selectCell()) and no board excitation -- these
    // just drive ROW_PINS or COL_PINS to the given 4-bit value, leaving
    // everything else (including the other axis) untouched.
    void selectRawRow(uint8_t row) const;
    void selectRawCol(uint8_t col) const;

    // Same idea, but rotation-compensated like selectCell() -- i.e. these
    // take the same (row, col) a real selectCell() call would, and only
    // write the one bus asked for. Both still take (row, col): the
    // rotation ties the two axes together (see selectCell()), so a single
    // axis's physical value can depend on the other's game coordinate.
    void selectRowForCell(uint8_t row, uint8_t col) const;
    void selectColForCell(uint8_t row, uint8_t col) const;

    // Enables this player's board excitation and disables the other
    // player's, so at most one board is ever live at a time.
    void enableBoardSelect(Player p, bool on) const;

    void setCurrentLevel(CurrentLevel level) const;

    // Reads the sense pin and converts it to milliamps.
    float readCurrentMa() const;
};
