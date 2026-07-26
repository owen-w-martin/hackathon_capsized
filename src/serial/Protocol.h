#pragma once

#include <cstdint>
#include <string>

#include "../Display.h"

// Shared wire format for raw hardware I/O -- per player, two rotary
// encoders (left/right, up/down) and one button in; a display-state dump
// out.
namespace protocol {

struct InputEvent {
    enum class Type { None, Button1, Button2, Enc1X, Enc1Y, Enc2X, Enc2Y, ShipCell1, ShipCell2 } type = Type::None;
    bool buttonPressed = false;
    int32_t encoderDelta = 0;
    int32_t x = 0;
    int32_t y = 0;
};

// "BTN1 <0|1>" / "BTN2 <0|1>" / "ENC1X <delta>" / "ENC1Y <delta>" / "ENC2X <delta>" / "ENC2Y <delta>"
// X = left/right encoder, Y = up/down encoder.
// "SHIP1 <x> <y>" / "SHIP2 <x> <y>" -- marks (x, y) as an occupied ship cell
// for that player; for testing ship layouts without real capacitor hardware.
bool parseLine(const std::string& line, InputEvent& out, std::string& err);

// "S <144x 6-hex P1 pixels> <144x 6-hex P2 pixels> <12x 6-hex bar pixels>"
// Reads whatever is currently on display -- doesn't care what put it there.
std::string encodeState(const Display& display);

}
