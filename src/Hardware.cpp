#include "Hardware.h"

Hardware::Hardware() {
    for (uint8_t pin : cfg::ROW_PINS) pinMode(pin, OUTPUT);
    for (uint8_t pin : cfg::COL_PINS) pinMode(pin, OUTPUT);
    pinMode(cfg::P1_EXCITE_PIN, OUTPUT);
    pinMode(cfg::P2_EXCITE_PIN, OUTPUT);
    pinMode(cfg::CURRENT_LEVEL_PIN, OUTPUT);
    // pinMode(A13, INPUT);

    digitalWrite(cfg::P1_EXCITE_PIN, LOW);
    digitalWrite(cfg::P2_EXCITE_PIN, LOW);
    setCurrentLevel(CurrentLevel::Detect);
}

void Hardware::selectCell(uint8_t row, uint8_t col) const {
    for (int bit = 0; bit < 4; bit++) {
        digitalWrite(cfg::ROW_PINS[bit], (row >> bit) & 1);
        digitalWrite(cfg::COL_PINS[bit], (col >> bit) & 1);
    }
}

void Hardware::excite(Player p, bool on) const {
    digitalWrite(cfg::P1_EXCITE_PIN, (p == P1 && on) ? HIGH : LOW);
    digitalWrite(cfg::P2_EXCITE_PIN, (p == P2 && on) ? HIGH : LOW);
}

void Hardware::setCurrentLevel(CurrentLevel level) const {
    digitalWrite(cfg::CURRENT_LEVEL_PIN, static_cast<uint8_t>(level));
}

float Hardware::readCurrentMa() const {
    // int raw = analogRead(cfg::CURRENT_SENSE_PIN);
    // float volts = (raw / static_cast<float>(cfg::ADC_MAX_COUNT)) * cfg::ADC_REF_VOLTAGE;
    // return volts * cfg::CURRENT_SENSE_MA_PER_VOLT;

    return static_cast<float>((analogRead(cfg::CURRENT_SENSE_PIN)) * 917.f / 1000.f);

}
