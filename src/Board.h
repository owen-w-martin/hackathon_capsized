#pragma once

#include "Display.h"

// Placeholder for whatever a single player's board actually holds.
class PlayerBoard {
public:
    PlayerBoard();
};

class Board {
public:
    Board();

    PlayerBoard& operator[](Player p) { return (p == P1) ? _p1 : _p2; }

private:
    PlayerBoard _p1, _p2;
};
