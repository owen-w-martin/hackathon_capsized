#include "SerialIO.h"

#include <Arduino.h>
#include <string>

#include "core/Protocol.h"

namespace {

std::string buf;

void apply(core::Game& game, const protocol::InputEvent& ev) {
    switch (ev.type) {
        case protocol::InputEvent::Type::Button:
            game.onButtonInput(ev.buttonPressed);
            break;
        case protocol::InputEvent::Type::Enc1X:
            game.onEncoderInput(core::Player::P1, core::Game::Axis::X, ev.encoderDelta);
            break;
        case protocol::InputEvent::Type::Enc1Y:
            game.onEncoderInput(core::Player::P1, core::Game::Axis::Y, ev.encoderDelta);
            break;
        case protocol::InputEvent::Type::Enc2X:
            game.onEncoderInput(core::Player::P2, core::Game::Axis::X, ev.encoderDelta);
            break;
        case protocol::InputEvent::Type::Enc2Y:
            game.onEncoderInput(core::Player::P2, core::Game::Axis::Y, ev.encoderDelta);
            break;
        default:
            break;
    }
}

}  // namespace

bool pollAndApplySerial(core::Game& game) {
    while (Serial.available()) {
        char c = static_cast<char>(Serial.read());
        if (c == '\n') {
            std::string line = buf;
            buf.clear();
            if (!line.empty() && line.back() == '\r') line.pop_back();

            protocol::InputEvent ev;
            std::string err;
            if (protocol::parseLine(line, ev, err)) {
                apply(game, ev);
                return true;
            }
            Serial.print("ERR ");
            Serial.println(err.c_str());
            return false;
        }
        buf += c;
    }
    return false;
}

void printToSerial(const Display& display) {
    Serial.println(protocol::encodeState(display).c_str());
}
