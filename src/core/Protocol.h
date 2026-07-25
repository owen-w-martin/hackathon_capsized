#pragma once

#include <cstdint>
#include <string>

#include "Types.h"

// Shared wire format for raw hardware I/O -- a shared button and, per
// player, two rotary encoders (left/right, up/down) in; a display-state
// dump out.
namespace proto {

struct InputEvent {
    enum class Type { None, Button, Enc1X, Enc1Y, Enc2X, Enc2Y } type = Type::None;
    bool buttonPressed = false;
    int32_t encoderDelta = 0;
};

// "BTN <0|1>" / "ENC1X <delta>" / "ENC1Y <delta>" / "ENC2X <delta>" / "ENC2Y <delta>"
// X = left/right encoder, Y = up/down encoder.
bool parseLine(const std::string& line, InputEvent& out, std::string& err);

// "S <144x 6-hex P1 pixels> <144x 6-hex P2 pixels> <12x 6-hex bar pixels>"
// Encodes whatever is passed in -- doesn't care whether it came from
// core::App or was read back from the real Display, so both the
// cursor/button harness and any animation running on top of it can be
// mirrored the same way.
std::string encodeState(const core::DisplayState& d);

}
