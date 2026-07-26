#include "SerialIO.h"

#include <Arduino.h>
#include <string>

#include "Protocol.h"

namespace {

std::string buf;

void apply(core::Game& game, Ship& p1Ship, Ship& p2Ship, const protocol::InputEvent& ev) {
    switch (ev.type) {
        case protocol::InputEvent::Type::Button1:
            game.onButtonInput(core::Player::P1, ev.buttonPressed);
            break;
        case protocol::InputEvent::Type::Button2:
            game.onButtonInput(core::Player::P2, ev.buttonPressed);
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
        case protocol::InputEvent::Type::ShipCell1:
            p1Ship.addCell({ev.x, ev.y});
            break;
        case protocol::InputEvent::Type::ShipCell2:
            p2Ship.addCell({ev.x, ev.y});
            break;
        default:
            break;
    }
}

}  // namespace

bool pollAndApplySerial(core::Game& game, Ship& p1Ship, Ship& p2Ship) {
    while (Serial.available()) {
        char c = static_cast<char>(Serial.read());
        if (c == '\n') {
            std::string line = buf;
            buf.clear();
            if (!line.empty() && line.back() == '\r') line.pop_back();

            protocol::InputEvent ev;
            std::string err;
            if (protocol::parseLine(line, ev, err)) {
                apply(game, p1Ship, p2Ship, ev);
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
