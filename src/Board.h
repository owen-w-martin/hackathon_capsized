#pragma once

#include <utility>
#include <vector>

#include "Display.h"
#include "Hardware.h"

namespace cfg {
    constexpr int BOARD_W = 10;       // per-player grid width  (cells)
    constexpr int BOARD_H = 10;       // per-player grid height (cells)

    // TODO: calibrate against real hardware -- an intact capacitor should
    // read close to the ~200mA detection current; a popped one should read
    // close to 0.
    constexpr float POPPED_THRESHOLD_MA = 50.0f;
}

enum class BoardStatus {
    IDLE,
    CHARGING,
};

enum class CapStatus {
    INTACT,
    POPPED,
    FAULTY
};

class Capacitor {
public:
    Capacitor(std::pair<int, int> pos);

    std::pair<int, int> getPosition() const;
    CapStatus           getStatus() const;
    void                setPopped();
    void                setFaulty();

private:
    std::pair<int, int> position;
    CapStatus           status;
};

class Board {
public:
    Board();

    void update();

    void scanBoard();

    std::vector<std::pair<int, int>> getCapPositions(Player p) const;

    void popCap(std::pair<int, int> pos);

    CapStatus getCapStatus(std::pair<int, int> pos);

    BoardStatus getStatus() const { return status; }

private:
    Hardware hardware;
    std::vector<Capacitor> P1_caps;
    std::vector<Capacitor> P2_caps;
    BoardStatus status;
    std::pair<int, int> target(std::pair<int, int> pos) const;
};
