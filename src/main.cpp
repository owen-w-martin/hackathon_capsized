#include <Arduino.h>

#include "Blinky.h"
#include "Display.h"
#include "SerialIO.h"
#include "core/Game.h"

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
    blinkyUpdate();
    if (pollAndApplySerial(game)) refreshDisplays();
}
