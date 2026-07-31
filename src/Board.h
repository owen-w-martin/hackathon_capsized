#pragma once

#include <utility>
#include <vector>

#include "Display.h"
#include "Hardware.h"

namespace cfg {
    constexpr int BOARD_W = BOARD_SIZE;       // per-player grid width  (cells)
    constexpr int BOARD_H = BOARD_SIZE;       // per-player grid height (cells)

    // TODO: calibrate against real hardware -- an intact capacitor should
    // read close to the ~200mA detection current; a popped one should read
    // close to 0.
    constexpr float POPPED_THRESHOLD_MA = 30.0f;

    // TODO: tune to how long it actually takes to pop a capacitor at the Pop
    // current level; this is a safety cutoff for when the current never
    // drops (e.g. a wiring fault or a capacitor that won't pop).
    constexpr uint32_t POP_TIMEOUT_MS = 20000;
}

enum class BoardStatus {
    IDLE,
    CHARGING,
    SCANNING,
    HOLDING,
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

    // Advances whatever's in progress (a pop charge or a board scan) by
    // one step; call this every loop() iteration regardless of state.
    void update();

    // Starts a full-board scan of `player`'s capacitors without blocking:
    // update() advances it a cell at a time (one select-and-settle step,
    // then one read step, per call) until getStatus() returns to IDLE, at
    // which point getCapPositions() reflects the finished scan. Does
    // nothing if a charge or scan is already in progress.
    void beginScan(Player player);

    std::vector<std::pair<int, int>> getCapPositions(Player p) const;

    void popCap(Player player, int x, int y);

    bool isCapPopped(std::pair<int, int> pos);

    void scanCell(Player player, int x, int y);
    BoardStatus getStatus() const { return status; }

    // Debug-only: excites `player`'s board, selects (x, y) (rotation-
    // compensated, same as scanCell), and keeps reading/reporting the
    // current every update() call -- for interactively probing one cell
    // live (e.g. holding a key down and watching the reading change) --
    // until endHold() is called. Does nothing if a charge/scan/hold is
    // already in progress.
    void beginHold(Player player, int x, int y);
    void endHold();

    // Debug-only: raw address-bus probes, straight through to
    // Hardware::selectRawRow/selectRawCol -- no rotation compensation, and
    // the player's board select pin is left untouched (never enabled), so
    // these can't excite or pop anything. `player` is only for logging.
    void debugSelectRow(Player player, int row);
    void debugSelectCol(Player player, int col);
    void debugSelectCell(Player player, int row, int col);

    // Debug-only: same idea, but rotation-compensated and in game (x, y)
    // coordinates -- straight through to
    // Hardware::selectRowForCell/selectColForCell/selectCell. Still never
    // touches board excitation.
    void debugSelectRowXY(Player player, int x, int y);
    void debugSelectColXY(Player player, int x, int y);
    void debugSelectCellXY(Player player, int x, int y);

private:
    void stepScan();

    Hardware hardware;
    std::vector<Capacitor> P1_caps;
    std::vector<Capacitor> P2_caps;
    BoardStatus status;
    std::pair<int, int> targetCoord;
    Player targetPlayer;
    uint32_t chargeStartMs;

    // Non-blocking scan state (see beginScan()/stepScan()).
    Player scanPlayer;
    int scanX = 0;
    int scanY = 0;
    bool scanAwaitingRead = false;

    // Hold state (see beginHold()/endHold()).
    Player holdPlayer;
};
