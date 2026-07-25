#include <Arduino.h>
#include <FastLED.h>
#include "Display.h"
#include "Animations.h"   // animPlasma / animDinner / animTetris / animFace / animRadar / animBadApple / animConwayLife

Display display;

void setup() {
  display.begin();
}

void loop() {
  animRadar();
}

