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

    // TODO: tune to how long it actually takes to pop a capacitor at the Pop
    // current level; this is a safety cutoff for when the current never
    // drops (e.g. a wiring fault or a capacitor that won't pop).
    constexpr uint32_t POP_TIMEOUT_MS = 15000;
}

enum class BoardStatus {
    IDLE,
    CHARGING,
};

class Capacitor {
public:
    Capacitor(std::pair<int, int> pos);

    std::pair<int, int> getPosition() const;
    bool                isPopped() const;
    void                setPopped();
    void                setFaulty();

private:
    std::pair<int, int> position;
    bool                popped;
};

class Board {
public:
    Board();

    void update();

    void scanBoard(Player player);

    std::vector<std::pair<int, int>> getCapPositions(Player p) const;

    void popCap(Player player, int x, int y);

    bool isCapPopped(Player player, std::pair<int, int> pos);

    void scanSquare(Player player, int x, int y);
    BoardStatus getStatus() const { return status; }

private:
    Hardware hardware;
    std::vector<Capacitor> P1_caps;
    std::vector<Capacitor> P2_caps;
    BoardStatus status;
    std::pair<int, int> targetCoord;
    Player targetPlayer;
    uint32_t chargeStartMs;
};
