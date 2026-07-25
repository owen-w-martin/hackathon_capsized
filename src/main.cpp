#include <Arduino.h>

#include "Blinky.h"
#include "Display.h"
#include "SerialIO.h"
#include "core/Game.h"

#define FRAMERATE 30

Display display;
core::Game game;

namespace {

void refreshDisplays() {
    display.draw(game);
    printToSerial(game.display());
}

}  // namespace

void setup() {
    Serial.begin(115200);
    blinkySetup();
    display.begin();
    refreshDisplays();
}

void loop() {
    static uint32_t lastFrameMs = 0;
    uint32_t nowMs = millis();
    if (nowMs - lastFrameMs < 1000 / FRAMERATE) return;
    lastFrameMs = nowMs;

    blinkyUpdate();
    pollAndApplySerial(game);
    refreshDisplays();
}
