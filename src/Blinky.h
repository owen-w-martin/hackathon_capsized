#pragma once

// Call once from setup().
void blinkySetup();
// Call every loop() iteration; toggles LED_BUILTIN at 0.5 Hz.
void blinkyUpdate();
