#include "SerialIO.h"

#include <Arduino.h>
#include <string>

#include "Protocol.h"

namespace {

std::string buf;

void apply(core::Game& game, Ship& p1Ship, Ship& p2Ship, Board& board, const protocol::InputEvent& ev) {
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
        case protocol::InputEvent::Type::ScanCell1:
            board.scanCell(P1, ev.x, ev.y);
            break;
        case protocol::InputEvent::Type::ScanCell2:
            board.scanCell(P2, ev.x, ev.y);
            break;
        case protocol::InputEvent::Type::SelectRow1:
            board.debugSelectRow(P1, ev.x);
            break;
        case protocol::InputEvent::Type::SelectRow2:
            board.debugSelectRow(P2, ev.x);
            break;
        case protocol::InputEvent::Type::SelectCol1:
            board.debugSelectCol(P1, ev.x);
            break;
        case protocol::InputEvent::Type::SelectCol2:
            board.debugSelectCol(P2, ev.x);
            break;
        case protocol::InputEvent::Type::SelectCell1:
            board.debugSelectCell(P1, ev.x, ev.y);
            break;
        case protocol::InputEvent::Type::SelectCell2:
            board.debugSelectCell(P2, ev.x, ev.y);
            break;
        case protocol::InputEvent::Type::SelectRowXY1:
            board.debugSelectRowXY(P1, ev.x, ev.y);
            break;
        case protocol::InputEvent::Type::SelectRowXY2:
            board.debugSelectRowXY(P2, ev.x, ev.y);
            break;
        case protocol::InputEvent::Type::SelectColXY1:
            board.debugSelectColXY(P1, ev.x, ev.y);
            break;
        case protocol::InputEvent::Type::SelectColXY2:
            board.debugSelectColXY(P2, ev.x, ev.y);
            break;
        case protocol::InputEvent::Type::SelectCellXY1:
            board.debugSelectCellXY(P1, ev.x, ev.y);
            break;
        case protocol::InputEvent::Type::SelectCellXY2:
            board.debugSelectCellXY(P2, ev.x, ev.y);
            break;
        case protocol::InputEvent::Type::HoldCell1:
            board.beginHold(P1, ev.x, ev.y);
            break;
        case protocol::InputEvent::Type::HoldCell2:
            board.beginHold(P2, ev.x, ev.y);
            break;
        case protocol::InputEvent::Type::UnholdCell1:
        case protocol::InputEvent::Type::UnholdCell2:
            board.endHold();
            break;
        default:
            break;
    }
}

}  // namespace

bool pollAndApplySerial(core::Game& game, Ship& p1Ship, Ship& p2Ship, Board& board) {
    while (Serial.available()) {
        char c = static_cast<char>(Serial.read());
        if (c == '\n') {
            std::string line = buf;
            buf.clear();
            if (!line.empty() && line.back() == '\r') line.pop_back();

            protocol::InputEvent ev;
            std::string err;
            if (protocol::parseLine(line, ev, err)) {
                apply(game, p1Ship, p2Ship, board, ev);
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
