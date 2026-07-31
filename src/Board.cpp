#include "Board.h"

#include <Arduino.h>

Capacitor::Capacitor(std::pair<int, int> pos)
    : position(pos), popped(false) {}

std::pair<int, int> Capacitor::getPosition() const {
    return position;
}

bool Capacitor::isPopped() const {
    return popped;
}

void Capacitor::setPopped() {
    popped = true;
}

void Capacitor::setFaulty() {
    popped = true;
}

Board::Board() {
    status = BoardStatus::IDLE;
}

namespace {
    Capacitor* findCap(std::vector<Capacitor>& caps, std::pair<int, int> pos) {
        for (Capacitor& cap : caps) {
            if (cap.getPosition() == pos) return &cap;
        }
        return nullptr;
    }

    // Prefixes every debug line with the Teensy's own elapsed-since-boot
    // clock (there's no wall-clock RTC in use here, just millis()) so
    // tooling reading the serial stream can place events in time without
    // relying on USB/OS receipt timing, which is jittery under load.
    // "T<millis> " rather than a bare number so it can't be confused with
    // any line that happens to start with a digit for other reasons.
    void printTimestamp() {
        Serial.print('T');
        Serial.print(millis());
        Serial.print(' ');
    }
}

void Board::popCap(Player player, int x, int y){
    if (status == BoardStatus::CHARGING) return;

    targetCoord = std::make_pair(x, y);
    targetPlayer = player;
    chargeStartMs = millis();
    status = BoardStatus::CHARGING;

    // Explicit markers (rather than leaving callers to infer charging from
    // the presence/absence of "currentMa: " lines) so tooling can mark
    // exactly when a charge starts and ends.
    printTimestamp();
    Serial.print("chargeBegin: ");
    Serial.print(player == P1 ? "P1" : "P2");
    Serial.print(" (");
    Serial.print(x);
    Serial.print(",");
    Serial.print(y);
    Serial.println(")");
}

void Board::update() {
    if (status == BoardStatus::SCANNING) {
        stepScan();
        return;
    }

    if (status == BoardStatus::HOLDING) {
        int rawAdc;
        float currentMa = hardware.readCurrentMa(&rawAdc);
        printTimestamp();
        Serial.print("holdCell: ");
        Serial.print(holdPlayer == P1 ? "P1" : "P2");
        Serial.print(" rawAdc=");
        Serial.print(rawAdc);
        Serial.print(" currentMa=");
        Serial.println(currentMa);
        return;
    }

    if (status != BoardStatus::CHARGING) {
        hardware.enableBoardSelect(targetPlayer, false);
        hardware.setCurrentLevel(Hardware::CurrentLevel::Detect);
        return;
    }

    hardware.setCurrentLevel(Hardware::CurrentLevel::Pop);
    hardware.selectCell(targetCoord.first, targetCoord.second);
    hardware.enableBoardSelect(targetPlayer, true);

    int rawAdc;
    float currentMa = hardware.readCurrentMa(&rawAdc);
    printTimestamp();
    Serial.print("rawAdc: ");
    Serial.print(rawAdc);
    Serial.print(" currentMa: ");
    Serial.println(currentMa);

    // Serial.print("Board::update status=");
    // Serial.print(status == BoardStatus::CHARGING ? "CHARGING" : "IDLE");
    // Serial.print(" currentLevelPin=");
    // Serial.print(digitalRead(cfg::CURRENT_LEVEL_PIN));
    // Serial.print(" p1ExcitePin=");
    // Serial.print(digitalRead(cfg::BOARD_SEL_P1));
    // Serial.print(" p2ExcitePin=");
    // Serial.print(digitalRead(cfg::BOARD_SEL_P2));
    // Serial.print(" targetPlayer=");
    // Serial.print(targetPlayer == P1 ? "P1" : "P2");
    // Serial.print(" targetCoord=(");
    // Serial.print(targetCoord.first);
    // Serial.print(",");
    // Serial.print(targetCoord.second);
    // Serial.println(")");

    bool poppedDetected = false; //currentMa < cfg::POPPED_THRESHOLD_MA;
    bool timedOut = millis() - chargeStartMs >= cfg::POP_TIMEOUT_MS;

    // Serial.println("timedOut: " + String(timedOut ? "true" : "false"));
    printTimestamp();
    Serial.println("time elapsed in shooting " + String(millis() - chargeStartMs));

    if (poppedDetected || timedOut) {
        hardware.enableBoardSelect(targetPlayer, false);
        hardware.setCurrentLevel(Hardware::CurrentLevel::Detect);
        status = BoardStatus::IDLE;

        printTimestamp();
        Serial.println("chargeEnd");

        if (poppedDetected) {
            auto& caps = (targetPlayer == P1) ? P1_caps : P2_caps;
            Capacitor* cap = findCap(caps, targetCoord);
            if (cap) cap->setPopped();
        }
    }
}

void Board::scanCell(Player player, int x, int y) {
    printTimestamp();
    Serial.print("scanCell: scanning ");
    Serial.print(player == P1 ? "P1" : "P2");
    Serial.print(" (");
    Serial.print(x);
    Serial.print(",");
    Serial.print(y);
    Serial.println(")");

    hardware.enableBoardSelect(player, true);
    hardware.selectCell(x, y);
    delay(5);
    int rawAdc;
    float currentMa = hardware.readCurrentMa(&rawAdc);
    bool capPresent = currentMa >= cfg::POPPED_THRESHOLD_MA;

    if (capPresent) {
        auto& caps = (player == P1) ? P1_caps : P2_caps;
        caps.emplace_back(std::make_pair(x, y));
    }

    printTimestamp();
    Serial.print("scanCell: done, rawAdc=");
    Serial.print(rawAdc);
    Serial.print(", ");
    Serial.print(currentMa);
    Serial.print("mA -> ");
    Serial.println(capPresent ? "present" : "none");
    hardware.enableBoardSelect(player, false);
}

void Board::beginScan(Player player) {
    if (status != BoardStatus::IDLE) return;

    printTimestamp();
    Serial.print("beginScan: scanning ");
    Serial.println(player == P1 ? "P1" : "P2");

    hardware.enableBoardSelect(player, true);
    hardware.setCurrentLevel(Hardware::CurrentLevel::Detect);

    auto& caps = (player == P1) ? P1_caps : P2_caps;
    caps.clear();

    scanPlayer = player;
    scanX = 0;
    scanY = 0;
    scanAwaitingRead = false;
    status = BoardStatus::SCANNING;
}

// One call = one step: either select the next cell (and wait a frame for
// the sense line to settle) or read the cell selected last call. Two
// update() calls per cell, so it never blocks longer than a single
// loop() iteration.
void Board::stepScan() {
    if (!scanAwaitingRead) {
        hardware.selectCell(scanX, scanY);
        scanAwaitingRead = true;
        return;
    }

    int rawAdc;
    float currentMa = hardware.readCurrentMa(&rawAdc);
    bool capPresent = currentMa >= cfg::POPPED_THRESHOLD_MA;
    if (currentMa > 10.0f) {
        printTimestamp();
        Serial.print("  (");
        Serial.print(scanX);
        Serial.print(",");
        Serial.print(scanY);
        Serial.print(") rawAdc=");
        Serial.print(rawAdc);
        Serial.print(", ");
        Serial.print(currentMa);
        Serial.print("mA -> ");
        Serial.println(capPresent ? "present" : "none");
    }

    auto& caps = (scanPlayer == P1) ? P1_caps : P2_caps;
    if (capPresent) {
        caps.emplace_back(std::make_pair(scanX, scanY));
    }
    scanAwaitingRead = false;

    scanY++;
    if (scanY >= cfg::BOARD_H) {
        scanY = 0;
        scanX++;
    }
    if (scanX >= cfg::BOARD_W) {
        printTimestamp();
        Serial.print("beginScan: done, ");
        Serial.print(caps.size());
        Serial.print("/");
        Serial.print(cfg::BOARD_W * cfg::BOARD_H);
        Serial.println(" caps found");

        hardware.enableBoardSelect(scanPlayer, false);
        hardware.setCurrentLevel(Hardware::CurrentLevel::Detect);
        status = BoardStatus::IDLE;
    }
}

void Board::beginHold(Player player, int x, int y) {
    if (status != BoardStatus::IDLE) return;

    printTimestamp();
    Serial.print("beginHold: ");
    Serial.print(player == P1 ? "P1" : "P2");
    Serial.print(" (");
    Serial.print(x);
    Serial.print(",");
    Serial.print(y);
    Serial.println(")");

    hardware.setCurrentLevel(Hardware::CurrentLevel::Detect);
    hardware.selectCell(x, y);
    hardware.enableBoardSelect(player, true);

    holdPlayer = player;
    status = BoardStatus::HOLDING;
}

void Board::endHold() {
    if (status != BoardStatus::HOLDING) return;

    printTimestamp();
    Serial.println("endHold");
    hardware.enableBoardSelect(holdPlayer, false);
    hardware.setCurrentLevel(Hardware::CurrentLevel::Detect);
    status = BoardStatus::IDLE;
}

void Board::debugSelectRow(Player player, int row) {
    printTimestamp();
    Serial.print("debugSelectRow: ");
    Serial.print(player == P1 ? "P1" : "P2");
    Serial.print(" row=");
    Serial.println(row);
    hardware.selectRawRow(static_cast<uint8_t>(row));
}

void Board::debugSelectCol(Player player, int col) {
    printTimestamp();
    Serial.print("debugSelectCol: ");
    Serial.print(player == P1 ? "P1" : "P2");
    Serial.print(" col=");
    Serial.println(col);
    hardware.selectRawCol(static_cast<uint8_t>(col));
}

void Board::debugSelectCell(Player player, int row, int col) {
    printTimestamp();
    Serial.print("debugSelectCell: ");
    Serial.print(player == P1 ? "P1" : "P2");
    Serial.print(" row=");
    Serial.print(row);
    Serial.print(" col=");
    Serial.println(col);
    hardware.selectRawRow(static_cast<uint8_t>(row));
    hardware.selectRawCol(static_cast<uint8_t>(col));
}

void Board::debugSelectRowXY(Player player, int x, int y) {
    printTimestamp();
    Serial.print("debugSelectRowXY: ");
    Serial.print(player == P1 ? "P1" : "P2");
    Serial.print(" (");
    Serial.print(x);
    Serial.print(",");
    Serial.print(y);
    Serial.println(")");
    hardware.selectRowForCell(static_cast<uint8_t>(x), static_cast<uint8_t>(y));
}

void Board::debugSelectColXY(Player player, int x, int y) {
    printTimestamp();
    Serial.print("debugSelectColXY: ");
    Serial.print(player == P1 ? "P1" : "P2");
    Serial.print(" (");
    Serial.print(x);
    Serial.print(",");
    Serial.print(y);
    Serial.println(")");
    hardware.selectColForCell(static_cast<uint8_t>(x), static_cast<uint8_t>(y));
}

void Board::debugSelectCellXY(Player player, int x, int y) {
    printTimestamp();
    Serial.print("debugSelectCellXY: ");
    Serial.print(player == P1 ? "P1" : "P2");
    Serial.print(" (");
    Serial.print(x);
    Serial.print(",");
    Serial.print(y);
    Serial.println(")");
    hardware.selectCell(static_cast<uint8_t>(x), static_cast<uint8_t>(y));
}

std::vector<std::pair<int, int>> Board::getCapPositions(Player p) const {
    const auto& caps = (p == P1) ? P1_caps : P2_caps;
    std::vector<std::pair<int, int>> positions;
    for (const Capacitor& cap : caps) {
        if (!cap.isPopped()) positions.push_back(cap.getPosition());
    }
    return positions;
}
