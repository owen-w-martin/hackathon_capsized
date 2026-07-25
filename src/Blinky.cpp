#include <Arduino.h>
#include "Blinky.h"

void blinkySetup() {
  pinMode(LED_BUILTIN, OUTPUT);
}

// Toggles every 1000ms, giving a 2s period == 0.5 Hz blink.
void blinkyUpdate() {
  static uint32_t lastToggle = 0;
  static bool ledOn = false;
  uint32_t now = millis();
  if (now - lastToggle >= 500) {
    lastToggle = now;
    ledOn = !ledOn;
    digitalWrite(LED_BUILTIN, ledOn ? HIGH : LOW);
  }
}
