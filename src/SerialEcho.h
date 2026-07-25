#pragma once

// Call once from setup().
void serialEchoSetup();
// Call every loop() iteration; echoes back any bytes received over USB serial.
void serialEchoUpdate();
