#pragma once

#include <cstdint>
#include <string>

#include "../Display.h"

// Shared wire format for raw hardware I/O -- a shared button and, per
// player, two rotary encoders (left/right, up/down) in; a display-state
// dump out.
namespace protocol {

struct InputEvent {
    enum class Type { None, Button, Enc1X, Enc1Y, Enc2X, Enc2Y } type = Type::None;
    bool buttonPressed = false;
    int32_t encoderDelta = 0;
};

// "BTN <0|1>" / "ENC1X <delta>" / "ENC1Y <delta>" / "ENC2X <delta>" / "ENC2Y <delta>"
// X = left/right encoder, Y = up/down encoder.
bool parseLine(const std::string& line, InputEvent& out, std::string& err);

// "S <144x 6-hex P1 pixels> <144x 6-hex P2 pixels> <12x 6-hex bar pixels>"
// Reads whatever is currently on display -- doesn't care what put it there.
std::string encodeState(const Display& display);

}
