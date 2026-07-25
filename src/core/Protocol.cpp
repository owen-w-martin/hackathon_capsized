#include "Protocol.h"

#include <cstdio>
#include <sstream>

namespace protocol {

static std::string hex(const CRGB& c) {
    char buf[7];
    std::snprintf(buf, sizeof(buf), "%02X%02X%02X", c.r, c.g, c.b);
    return std::string(buf, 6);
}

std::string encodeState(const core::DisplayState& d) {
    std::string out = "S ";
    for (int y = 0; y < cfg::SCREEN_H; y++)
        for (int x = 0; x < cfg::SCREEN_W; x++)
            out += hex(d.p1[y][x]);
    out += " ";
    for (int y = 0; y < cfg::SCREEN_H; y++)
        for (int x = 0; x < cfg::SCREEN_W; x++)
            out += hex(d.p2[y][x]);
    out += " ";
    for (int x = 0; x < cfg::SCREEN_W; x++)
        out += hex(d.bar[x]);

    return out;
}

bool parseLine(const std::string& line, InputEvent& out, std::string& err) {
    std::istringstream ss(line);
    std::string cmd;
    if (!(ss >> cmd)) { err = "EMPTY"; return false; }

    if (cmd == "BTN") {
        int v;
        if (!(ss >> v)) { err = "BAD_ARG"; return false; }
        out.type = InputEvent::Type::Button;
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

    err = "UNKNOWN_CMD";
    return false;
}

}
