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
}

void Board::popCap(Player player, int x, int y){
    if (status == BoardStatus::CHARGING) return;

    targetCoord = std::make_pair(x, y);
    targetPlayer = player;
    chargeStartMs = millis();
    status = BoardStatus::CHARGING;
}

void Board::update() {
    if (status != BoardStatus::CHARGING) {
        hardware.excite(P1, false);
        hardware.setCurrentLevel(Hardware::CurrentLevel::Detect);
        return;
    }

    hardware.setCurrentLevel(Hardware::CurrentLevel::Pop);
    hardware.selectCell(targetCoord.second, targetCoord.first);
    hardware.excite(targetPlayer, true);

    float currentMa = hardware.readCurrentMa();
    Serial.print("currentMa: ");
    Serial.println(currentMa);

    Serial.print("Board::update status=");
    Serial.print(status == BoardStatus::CHARGING ? "CHARGING" : "IDLE");
    Serial.print(" currentLevelPin=");
    Serial.print(digitalRead(cfg::CURRENT_LEVEL_PIN));
    Serial.print(" p1ExcitePin=");
    Serial.print(digitalRead(cfg::BOARD_SEL_P1));
    Serial.print(" p2ExcitePin=");
    Serial.print(digitalRead(cfg::BOARD_SEL_P2));
    Serial.print(" targetPlayer=");
    Serial.print(targetPlayer == P1 ? "P1" : "P2");
    Serial.print(" targetCoord=(");
    Serial.print(targetCoord.first);
    Serial.print(",");
    Serial.print(targetCoord.second);
    Serial.println(")");

    bool poppedDetected = false; //currentMa < cfg::POPPED_THRESHOLD_MA;
    bool timedOut = millis() - chargeStartMs >= cfg::POP_TIMEOUT_MS;

    Serial.println("timedOut: " + String(timedOut ? "true" : "false"));
    Serial.println("millis() - chargeStartMs: " + String(millis() - chargeStartMs));

    if (poppedDetected || timedOut) {
        hardware.excite(targetPlayer, false);
        hardware.setCurrentLevel(Hardware::CurrentLevel::Detect);
        status = BoardStatus::IDLE;

        if (poppedDetected) {
            auto& caps = (targetPlayer == P1) ? P1_caps : P2_caps;
            Capacitor* cap = findCap(caps, targetCoord);
            if (cap) cap->setPopped();
        }
    }
}

void Board::scanSquare(Player player, int x, int y) {
    Serial.print("scanSquare: scanning ");
    Serial.print(player == P1 ? "P1" : "P2");
    Serial.print(" (");
    Serial.print(x);
    Serial.print(",");
    Serial.print(y);
    Serial.println(")");

    hardware.excite(player, true);
    hardware.selectCell(y, x);
    delay(10);
    float currentMa = hardware.readCurrentMa();
    bool capPresent = currentMa >= cfg::POPPED_THRESHOLD_MA;

    if (capPresent) {
        auto& caps = (player == P1) ? P1_caps : P2_caps;
        caps.emplace_back(std::make_pair(x, y));
    }

    Serial.print("scanSquare: done, ");
    Serial.print(currentMa);
    Serial.print("mA -> ");
    Serial.println(capPresent ? "present" : "none");
}

void Board::scanBoard(Player player) {
    Serial.print("scanBoard: scanning ");
    Serial.println(player == P1 ? "P1" : "P2");

    hardware.excite(player, true);
    
    hardware.setCurrentLevel(Hardware::CurrentLevel::Detect);

    auto& caps = (player == P1) ? P1_caps : P2_caps;
    caps.clear();
    for (int x = 0; x < cfg::BOARD_W; x++) {
        for (int y = 0; y < cfg::BOARD_H; y++) {
            hardware.selectCell(y, x);
            delay(100);
            float currentMa = hardware.readCurrentMa();
            bool capPresent = currentMa >= cfg::POPPED_THRESHOLD_MA;
            // if (currentMa > 10.0f) {
            //     Serial.print("  (");
            //     Serial.print(x);
            //     Serial.print(",");
            //     Serial.print(y);
            //     Serial.print(") ");
            //     Serial.print(currentMa);
            //     Serial.print("mA -> ");
            //     Serial.println(capPresent ? "present" : "none");
            // }

            if (capPresent) {
                caps.emplace_back(std::make_pair(x, y));
            }
        }
    }

    Serial.print("scanBoard: done, ");
    Serial.print(caps.size());
    Serial.print("/");
    Serial.print(cfg::BOARD_W * cfg::BOARD_H);
    Serial.println(" caps found");
}

std::vector<std::pair<int, int>> Board::getCapPositions(Player p) const {
    const auto& caps = (p == P1) ? P1_caps : P2_caps;
    std::vector<std::pair<int, int>> positions;
    for (const Capacitor& cap : caps) {
        if (!cap.isPopped()) positions.push_back(cap.getPosition());
    }
    return positions;
}
