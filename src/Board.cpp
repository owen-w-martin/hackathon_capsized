#include "Board.h"

Capacitor::Capacitor(std::pair<int, int> pos)
    : position(pos), status(CapStatus::INTACT) {}

std::pair<int, int> Capacitor::getPosition() const {
    return position;
}

CapStatus Capacitor::getStatus() const {
    return status;
}

void Capacitor::setPopped() {
    status = CapStatus::POPPED;
}

void Capacitor::setFaulty() {
    status = CapStatus::FAULTY;
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

void Board::update() {

}

void Board::scanBoard() {
    hardware.excite(P1, true);
    hardware.setCurrentLevel(Hardware::CurrentLevel::Detect);

    for (int x = 0; x < cfg::BOARD_W; x++) {
        for (int y = 0; y < cfg::BOARD_H; y++) {
            hardware.selectCell(y, x);
            float currentMa = hardware.readCurrentMa();

            if (currentMa < cfg::POPPED_THRESHOLD_MA) {
                P1_caps[y * cfg::BOARD_W + x].setPopped();
            }
        }
    }
}
