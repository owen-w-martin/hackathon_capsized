#include <Arduino.h>

#include "Blinky.h"
#include "Display.h"
#include "HardwareIO.h"
#include "SerialIO.h"
#include "core/Game.h"

#define FRAMERATE 30

Display display;
core::Game game(display);

void setup() {
    Serial.begin(115200);
    blinkySetup();
    display.begin();
    hardwareIOBegin();
    game.processDisplayUpdates();
    printToSerial(display);
}

void loop() {
    static uint32_t lastFrameMs = 0;
    uint32_t nowMs = millis();
    if (nowMs - lastFrameMs < 1000 / FRAMERATE) return;
    lastFrameMs = nowMs;
    blinkyUpdate();

    // -- get inputs --
    // debug: get inputs from serial, aka laptop
    pollAndApplySerial(game);
    // get inputs from the physical button + encoder
    pollAndApplyHardware(game);

    game.transitionState();

    // -- update what should be on the display --
    display.clear();
    game.processDisplayUpdates();

    // -- actually write to the display --
    display.show();

    // debug: push to serial as well so we can view on laptop
    printToSerial(display);
}

