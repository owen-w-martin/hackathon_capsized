#include "Board.h"

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
    for (int y = 0; y < cfg::BOARD_H; y++) {
        for (int x = 0; x < cfg::BOARD_W; x++) {
            P1_caps.emplace_back(std::make_pair(x, y));
            P2_caps.emplace_back(std::make_pair(x, y));
        }
    }

    status = BoardStatus::IDLE;
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

    hardware.selectCell(targetCoord.second, targetCoord.first);
    hardware.setCurrentLevel(Hardware::CurrentLevel::Pop);
    hardware.excite(targetPlayer, true);

    float currentMa = hardware.readCurrentMa();
    bool poppedDetected = currentMa < cfg::POPPED_THRESHOLD_MA;
    bool timedOut = millis() - chargeStartMs >= cfg::POP_TIMEOUT_MS;

    if (poppedDetected || timedOut) {
        hardware.excite(targetPlayer, false);
        status = BoardStatus::IDLE;

        if (poppedDetected) {
            auto& caps = (targetPlayer == P1) ? P1_caps : P2_caps;
            caps[targetCoord.second * cfg::BOARD_W + targetCoord.first].setPopped();
        }
    }
}

void Board::scanBoard(Player player) {
    hardware.excite(player, true);
    hardware.setCurrentLevel(Hardware::CurrentLevel::Detect);

    auto& caps = (player == P1) ? P1_caps : P2_caps;
    for (int x = 0; x < cfg::BOARD_W; x++) {
        for (int y = 0; y < cfg::BOARD_H; y++) {
            hardware.selectCell(y, x);
            float currentMa = hardware.readCurrentMa();

            if (currentMa < cfg::POPPED_THRESHOLD_MA) {
                caps[y * cfg::BOARD_W + x].setPopped();
            }
        }
    }
}

std::vector<std::pair<int, int>> Board::getCapPositions(Player p) const {
    const auto& caps = (p == P1) ? P1_caps : P2_caps;
    std::vector<std::pair<int, int>> positions;
    for (const Capacitor& cap : caps) {
        if (!cap.isPopped()) positions.push_back(cap.getPosition());
    }
    return positions;
}
