#include <Arduino.h>
#include <FastLED.h>
#include <string>

#include "Display.h"
#include "Blinky.h"
#include "Animations.h"
#include "core/App.h"
#include "core/Protocol.h"

Display display;
core::App app;

namespace {

constexpr uint32_t IDLE_TIMEOUT_MS = 3000;

std::string serialBuf;
uint32_t lastInputMillis = 0;

// Buffers Serial bytes until a full line is seen, then parses it. Returns
// true only when a valid event was parsed this call (both "no full line
// yet" and "line failed to parse" return false; parse errors are reported
// over Serial directly).
bool pollSerial(proto::InputEvent& out) {
    while (Serial.available()) {
        char c = static_cast<char>(Serial.read());
        if (c == '\n') {
            std::string line = serialBuf;
            serialBuf.clear();
            if (!line.empty() && line.back() == '\r') line.pop_back();

            std::string err;
            if (proto::parseLine(line, out, err)) return true;
            Serial.print("ERR ");
            Serial.println(err.c_str());
            return false;
        }
        serialBuf += c;
    }
    return false;
}

void applyEvent(const proto::InputEvent& ev) {
    switch (ev.type) {
        case proto::InputEvent::Type::Button:
            app.onButton(ev.buttonPressed);
            break;
        case proto::InputEvent::Type::Enc1X:
            app.onEncoder(core::Player::P1, core::App::Axis::X, ev.encoderDelta);
            break;
        case proto::InputEvent::Type::Enc1Y:
            app.onEncoder(core::Player::P1, core::App::Axis::Y, ev.encoderDelta);
            break;
        case proto::InputEvent::Type::Enc2X:
            app.onEncoder(core::Player::P2, core::App::Axis::X, ev.encoderDelta);
            break;
        case proto::InputEvent::Type::Enc2Y:
            app.onEncoder(core::Player::P2, core::App::Axis::Y, ev.encoderDelta);
            break;
        default:
            break;
    }
}

// The physical display/encoders may or may not be attached -- this just
// blits whatever core::App computed; harmless either way.
void render() {
    const core::DisplayState& d = app.display();
    for (int y = 0; y < core::BOARD_H; y++) {
        for (int x = 0; x < core::BOARD_W; x++) {
            display.player(P1)(x, y) = d.p1[y][x];
            display.player(P2)(x, y) = d.p2[y][x];
        }
    }
    for (int x = 0; x < core::BOARD_W; x++) display.bar()(x) = d.bar[x];
    display.show();
}

// animRadar() paints straight into Display, bypassing core::App -- this
// reads the real buffers back out so the serial mirror can reflect it too.
core::DisplayState snapshotDisplay() {
    core::DisplayState d;
    for (int y = 0; y < core::BOARD_H; y++) {
        for (int x = 0; x < core::BOARD_W; x++) {
            d.p1[y][x] = display.player(P1)(x, y);
            d.p2[y][x] = display.player(P2)(x, y);
        }
    }
    for (int x = 0; x < core::BOARD_W; x++) d.bar[x] = display.bar()(x);
    return d;
}

}  // namespace

void setup() {
    Serial.begin(115200);
    blinkySetup();
    display.begin();
    render();
    Serial.println(proto::encodeState(app.display()).c_str());
    lastInputMillis = millis();
}

void loop() {
    blinkyUpdate();

    proto::InputEvent ev;
    if (pollSerial(ev)) {
        applyEvent(ev);
        lastInputMillis = millis();
        render();
        Serial.println(proto::encodeState(app.display()).c_str());
        return;
    }

    // Idle: no input for a while -- run the radar sweep instead of sitting
    // on a static cursor. Any new input immediately takes back over above.
    if (millis() - lastInputMillis >= IDLE_TIMEOUT_MS) {
        animRadar();   // draws into Display and calls display.show() itself
        Serial.println(proto::encodeState(snapshotDisplay()).c_str());
    }
}
