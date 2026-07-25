#include <Arduino.h>
#include <FastLED.h>
#include "Display.h"
#include "Animations.h"   // animPlasma / animDinner / animTetris / animFace / animRadar / animBadApple / animConwayLife
#include "Blinky.h"
#include "SerialEcho.h"

Display display;

void setup() {
  display.begin();
  blinkySetup();
  serialEchoSetup();
}

void loop() {
  blinkyUpdate();
  serialEchoUpdate();
  animRadar();
}

