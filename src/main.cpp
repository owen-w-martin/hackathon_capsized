#include <Arduino.h>
#include <FastLED.h>
#include "Display.h"
#include "Animations.h"   // animPlasma / animDinner / animTetris / animFace / animRadar / animBadApple / animConwayLife

#define FRAMERATE 30

Display display;

void setup() {
  display.begin();
}

void loop() {
  static uint32_t lastFrameMs = 0;
  uint32_t nowMs = millis();
  if (nowMs - lastFrameMs < 1000 / FRAMERATE) return;
  lastFrameMs = nowMs;

  display.clear();
  





  display.show();
}

