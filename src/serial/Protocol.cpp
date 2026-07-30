#include "Protocol.h"

#include <cstdio>
#include <sstream>

namespace protocol {

static std::string hex(const CRGB& c) {
    char buf[7];
    std::snprintf(buf, sizeof(buf), "%02X%02X%02X", c.r, c.g, c.b);
    return std::string(buf, 6);
}

std::string encodeState(const Display& display) {
    std::string out = "S ";
    for (int y = 0; y < cfg::SCREEN_H; y++)
        for (int x = 0; x < cfg::SCREEN_W; x++)
            out += hex(display.player(P1)(x, y));
    out += " ";
    for (int y = 0; y < cfg::SCREEN_H; y++)
        for (int x = 0; x < cfg::SCREEN_W; x++)
            out += hex(display.player(P2)(x, y));
    out += " ";
    for (int x = 0; x < cfg::SCREEN_W; x++)
        out += hex(display.bar()(x));

    return out;
}

bool parseLine(const std::string& line, InputEvent& out, std::string& err) {
    std::istringstream ss(line);
    std::string cmd;
    if (!(ss >> cmd)) { err = "EMPTY"; return false; }

    if (cmd == "BTN1" || cmd == "BTN2") {
        int v;
        if (!(ss >> v)) { err = "BAD_ARG"; return false; }
        out.type = (cmd == "BTN1") ? InputEvent::Type::Button1 : InputEvent::Type::Button2;
        out.buttonPressed = (v != 0);
        return true;
    }

    if (cmd == "ENC1X" || cmd == "ENC1Y" || cmd == "ENC2X" || cmd == "ENC2Y") {
        int32_t d;
        if (!(ss >> d)) { err = "BAD_ARG"; return false; }
        if (cmd == "ENC1X")      out.type = InputEvent::Type::Enc1X;
        else if (cmd == "ENC1Y") out.type = InputEvent::Type::Enc1Y;
        else if (cmd == "ENC2X") out.type = InputEvent::Type::Enc2X;
        else                     out.type = InputEvent::Type::Enc2Y;
        out.encoderDelta = d;
        return true;
    }

    if (cmd == "SHIP1" || cmd == "SHIP2") {
        int32_t x, y;
        if (!(ss >> x >> y)) { err = "BAD_ARG"; return false; }
        out.type = (cmd == "SHIP1") ? InputEvent::Type::ShipCell1 : InputEvent::Type::ShipCell2;
        out.x = x;
        out.y = y;
        return true;
    }

    if (cmd == "SCANCELL1" || cmd == "SCANCELL2") {
        int32_t x, y;
        if (!(ss >> x >> y)) { err = "BAD_ARG"; return false; }
        out.type = (cmd == "SCANCELL1") ? InputEvent::Type::ScanCell1 : InputEvent::Type::ScanCell2;
        out.x = x;
        out.y = y;
        return true;
    }

    if (cmd == "SELROW1" || cmd == "SELROW2") {
        int32_t row;
        if (!(ss >> row)) { err = "BAD_ARG"; return false; }
        out.type = (cmd == "SELROW1") ? InputEvent::Type::SelectRow1 : InputEvent::Type::SelectRow2;
        out.x = row;
        return true;
    }

    if (cmd == "SELCOL1" || cmd == "SELCOL2") {
        int32_t col;
        if (!(ss >> col)) { err = "BAD_ARG"; return false; }
        out.type = (cmd == "SELCOL1") ? InputEvent::Type::SelectCol1 : InputEvent::Type::SelectCol2;
        out.x = col;
        return true;
    }

    if (cmd == "SELCELL1" || cmd == "SELCELL2") {
        int32_t row, col;
        if (!(ss >> row >> col)) { err = "BAD_ARG"; return false; }
        out.type = (cmd == "SELCELL1") ? InputEvent::Type::SelectCell1 : InputEvent::Type::SelectCell2;
        out.x = row;
        out.y = col;
        return true;
    }

    if (cmd == "SELROWXY1" || cmd == "SELROWXY2") {
        int32_t x, y;
        if (!(ss >> x >> y)) { err = "BAD_ARG"; return false; }
        out.type = (cmd == "SELROWXY1") ? InputEvent::Type::SelectRowXY1 : InputEvent::Type::SelectRowXY2;
        out.x = x;
        out.y = y;
        return true;
    }

    if (cmd == "SELCOLXY1" || cmd == "SELCOLXY2") {
        int32_t x, y;
        if (!(ss >> x >> y)) { err = "BAD_ARG"; return false; }
        out.type = (cmd == "SELCOLXY1") ? InputEvent::Type::SelectColXY1 : InputEvent::Type::SelectColXY2;
        out.x = x;
        out.y = y;
        return true;
    }

    if (cmd == "SELCELLXY1" || cmd == "SELCELLXY2") {
        int32_t x, y;
        if (!(ss >> x >> y)) { err = "BAD_ARG"; return false; }
        out.type = (cmd == "SELCELLXY1") ? InputEvent::Type::SelectCellXY1 : InputEvent::Type::SelectCellXY2;
        out.x = x;
        out.y = y;
        return true;
    }

    if (cmd == "HOLDCELL1" || cmd == "HOLDCELL2") {
        int32_t x, y;
        if (!(ss >> x >> y)) { err = "BAD_ARG"; return false; }
        out.type = (cmd == "HOLDCELL1") ? InputEvent::Type::HoldCell1 : InputEvent::Type::HoldCell2;
        out.x = x;
        out.y = y;
        return true;
    }

    if (cmd == "UNHOLDCELL1" || cmd == "UNHOLDCELL2") {
        out.type = (cmd == "UNHOLDCELL1") ? InputEvent::Type::UnholdCell1 : InputEvent::Type::UnholdCell2;
        return true;
    }

    err = "UNKNOWN_CMD";
    return false;
}

}
