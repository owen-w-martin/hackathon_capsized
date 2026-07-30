#pragma once

#include <cstdint>
#include <string>

#include "../Display.h"

// Shared wire format for raw hardware I/O -- per player, two rotary
// encoders (left/right, up/down) and one button in; a display-state dump
// out.
namespace protocol {

struct InputEvent {
    enum class Type {
        None, Button1, Button2, Enc1X, Enc1Y, Enc2X, Enc2Y, ShipCell1, ShipCell2,
        ScanCell1, ScanCell2, SelectRow1, SelectRow2, SelectCol1, SelectCol2, SelectCell1, SelectCell2,
        SelectRowXY1, SelectRowXY2, SelectColXY1, SelectColXY2, SelectCellXY1, SelectCellXY2,
        HoldCell1, HoldCell2, UnholdCell1, UnholdCell2,
    } type = Type::None;
    bool buttonPressed = false;
    int32_t encoderDelta = 0;
    int32_t x = 0;
    int32_t y = 0;
};

// "BTN1 <0|1>" / "BTN2 <0|1>" / "ENC1X <delta>" / "ENC1Y <delta>" / "ENC2X <delta>" / "ENC2Y <delta>"
// X = left/right encoder, Y = up/down encoder.
// "SHIP1 <x> <y>" / "SHIP2 <x> <y>" -- marks (x, y) as an occupied ship cell
// for that player; for testing ship layouts without real capacitor hardware.
//
// Hardware debug commands (see Board::scanCell/debugSelectRow/debugSelectCol/debugSelectCell):
// "SCANCELL1 <x> <y>" / "SCANCELL2 <x> <y>" -- excites that player's board,
// selects (x, y) (through the same rotation compensation used during real
// play), reads the current, and reports present/absent, same as a real scan.
// "SELROW1 <row>" / "SELROW2 <row>" -- drives only the row address bus to
// the raw (non-rotated) demux row value; does not touch board excitation.
// "SELCOL1 <col>" / "SELCOL2 <col>" -- same, for the raw column address bus.
// "SELCELL1 <row> <col>" / "SELCELL2 <row> <col>" -- drives both raw
// address buses; still does not touch board excitation.
// "SELROWXY1 <x> <y>" / "SELROWXY2 <x> <y>" -- same rotation compensation
// as SCANCELL/selectCell, but only writes the row bus; still does not
// touch board excitation. Needs both x and y since the rotation ties the
// two axes together (see Hardware::selectCell).
// "SELCOLXY1 <x> <y>" / "SELCOLXY2 <x> <y>" -- same, but only writes the
// column bus.
// "SELCELLXY1 <x> <y>" / "SELCELLXY2 <x> <y>" -- writes both buses,
// rotation-compensated; still does not touch board excitation.
// "HOLDCELL1 <x> <y>" / "HOLDCELL2 <x> <y>" -- excites that player's
// board and selects (x, y) (rotation-compensated, same as scanCell), then
// keeps reading and reporting the current every loop() until:
// "UNHOLDCELL1" / "UNHOLDCELL2" -- ends the hold and de-excites the board.
// For interactively probing one cell live (e.g. holding a key down and
// watching the reading change).
bool parseLine(const std::string& line, InputEvent& out, std::string& err);

// "S <144x 6-hex P1 pixels> <144x 6-hex P2 pixels> <12x 6-hex bar pixels>"
// Reads whatever is currently on display -- doesn't care what put it there.
std::string encodeState(const Display& display);

}
