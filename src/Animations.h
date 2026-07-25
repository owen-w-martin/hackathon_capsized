#pragma once

// Non-radar animations, extracted out of main.cpp. Each is a self-contained
// loop-style function -- call one from main's loop() to switch which
// animation is running.
void animPlasma();
void animDinner();
void animTetris();
void animFace();
void animRadar();
void animBadApple();
void animConwayLife();
