#include "Hardware.h"

Hardware::Hardware() {
    for (uint8_t pin : cfg::ROW_PINS) pinMode(pin, OUTPUT);
    for (uint8_t pin : cfg::COL_PINS) pinMode(pin, OUTPUT);
    pinMode(cfg::BOARD_SEL_P1, OUTPUT);
    pinMode(cfg::BOARD_SEL_P2, OUTPUT);
    pinMode(cfg::CURRENT_LEVEL_PIN, OUTPUT);
    pinMode(cfg::CURRENT_SENSE_PIN, INPUT);

    digitalWrite(cfg::BOARD_SEL_P1, LOW);
    digitalWrite(cfg::BOARD_SEL_P2, LOW);
    setCurrentLevel(CurrentLevel::Detect);
}

void Hardware::selectCell(uint8_t row, uint8_t col) const {
    // Both players' physical boards are mounted rotated 90 degrees
    // counterclockwise relative to the row/col address bus, so compensate
    // here rather than have every caller reason about the rotation. Both
    // boards share this bus and are rotated the same way, so one transform
    // covers both.
    uint8_t physRow = col;
    uint8_t physCol = (cfg::BOARD_SIZE - 1) - row;

    digitalWrite(cfg::ROW_PINS[0], (physRow >> 0) & 1);
    digitalWrite(cfg::COL_PINS[0], (physCol >> 0) & 1);
    digitalWrite(cfg::ROW_PINS[1], (physRow >> 1) & 1);
    digitalWrite(cfg::COL_PINS[1], (physCol >> 1) & 1);
    digitalWrite(cfg::ROW_PINS[2], (physRow >> 2) & 1);
    digitalWrite(cfg::COL_PINS[2], (physCol >> 2) & 1);
    digitalWrite(cfg::ROW_PINS[3], (physRow >> 3) & 1);
    digitalWrite(cfg::COL_PINS[3], (physCol >> 3) & 1);
}

void Hardware::excite(Player p, bool on) const {
    digitalWrite(cfg::BOARD_SEL_P1, (p == P1 && on) ? HIGH : LOW);
    digitalWrite(cfg::BOARD_SEL_P2, (p == P2 && on) ? HIGH : LOW);
}

void Hardware::setCurrentLevel(CurrentLevel level) const {
    digitalWrite(cfg::CURRENT_LEVEL_PIN, static_cast<uint8_t>(level));
}

float Hardware::readCurrentMa() const {
    // int raw = analogRead(cfg::CURRENT_SENSE_PIN);
    // float volts = (raw / static_cast<float>(cfg::ADC_MAX_COUNT)) * cfg::ADC_REF_VOLTAGE;
    // return volts * cfg::CURRENT_SENSE_MA_PER_VOLT;

    return static_cast<float>((analogRead(cfg::CURRENT_SENSE_PIN)) * .917f);

}
