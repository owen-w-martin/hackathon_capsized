#include "HardwareIO.h"

#include <Arduino.h>
#include <Encoder.h>

namespace {

// TODO: confirm real wiring -- placeholder pins only.
// J7 is P1_ENC_L
// J8 is P1_ENC_R
// J9 is P2_ENC_L
// J10 is P2_ENC_R

constexpr uint8_t P2_ENC_R_B = 5;
constexpr uint8_t P1_ENC_R_B = 6;

constexpr uint8_t P2_ENC_R_A = 7;
constexpr uint8_t P1_ENC_R_A = 8;

constexpr uint8_t P2_ENC_L_B = 9;
constexpr uint8_t P1_ENC_L_B = 10;

constexpr uint8_t P2_ENC_L_A = 11;
constexpr uint8_t P1_ENC_L_A = 12;

constexpr uint8_t P2_FIRE = 26;
constexpr uint8_t P1_FIRE = 24;

constexpr uint32_t DEBOUNCE_US = 20000;   // 20ms

// PEC11R-style detent encoders click once per full electrical (quadrature)
// cycle, and the Encoder library counts all 4 edges of that cycle -- so 4
// raw counts per detent. If yours turns out to detent every half-cycle
// instead (2 raw counts per click), change this to 2.
constexpr int32_t COUNTS_PER_DETENT = 4;

// X = L encoder, Y = R encoder.
Encoder g_p1EncX(P1_ENC_L_A, P1_ENC_L_B);
Encoder g_p1EncY(P1_ENC_R_A, P1_ENC_R_B);
Encoder g_p2EncX(P2_ENC_L_A, P2_ENC_L_B);
Encoder g_p2EncY(P2_ENC_R_A, P2_ENC_R_B);

// Last raw count consumed into a whole detent, per encoder -- any leftover
// partial-cycle remainder is kept for next time rather than dropped.
int32_t g_p1EncXBase = 0;
int32_t g_p1EncYBase = 0;
int32_t g_p2EncXBase = 0;
int32_t g_p2EncYBase = 0;

volatile bool     g_p1ButtonPressed    = false;
volatile bool     g_p1ButtonChanged    = false;
volatile uint32_t g_p1ButtonLastMicros = 0;

volatile bool     g_p2ButtonPressed    = false;
volatile bool     g_p2ButtonChanged    = false;
volatile uint32_t g_p2ButtonLastMicros = 0;

// Debounced in the ISR itself so callers never see a bounce as a real change.
void p1ButtonISR() {
    uint32_t now = micros();
    if (now - g_p1ButtonLastMicros < DEBOUNCE_US) return;
    g_p1ButtonLastMicros = now;

    g_p1ButtonPressed = (digitalRead(P1_FIRE) == LOW);   // active-low, INPUT_PULLUP
    g_p1ButtonChanged = true;
}

void p2ButtonISR() {
    uint32_t now = micros();
    if (now - g_p2ButtonLastMicros < DEBOUNCE_US) return;
    g_p2ButtonLastMicros = now;

    g_p2ButtonPressed = (digitalRead(P2_FIRE) == LOW);   // active-low, INPUT_PULLUP
    g_p2ButtonChanged = true;
}

// Returns whole detents moved since last call; keeps any partial-cycle
// remainder in base so it isn't lost across polls.
int32_t consumeDetents(Encoder& enc, int32_t& base) {
    int32_t now = enc.read();
    int32_t detents = (now - base) / COUNTS_PER_DETENT;
    base += detents * COUNTS_PER_DETENT;
    return detents;
}

}  // namespace

void hardwareIOBegin() {
    pinMode(P1_FIRE, INPUT_PULLUP);
    pinMode(P2_FIRE, INPUT_PULLUP);

    attachInterrupt(digitalPinToInterrupt(P1_FIRE), p1ButtonISR, CHANGE);
    attachInterrupt(digitalPinToInterrupt(P2_FIRE), p2ButtonISR, CHANGE);

    // Encoder objects configure their own pins and interrupts internally.
}

// check if we have received an interrupt from any of the hardware (button or encoder) since the last frame
// if so, update the game state using the callbacks
bool pollAndApplyHardware(core::Game& game) {
    int32_t p1x = -1 * consumeDetents(g_p1EncX, g_p1EncXBase);
    int32_t p1y = consumeDetents(g_p1EncY, g_p1EncYBase);
    int32_t p2x = -1 * consumeDetents(g_p2EncX, g_p2EncXBase);
    int32_t p2y = consumeDetents(g_p2EncY, g_p2EncYBase);

    bool p1BtnChanged, p1BtnPressed, p2BtnChanged, p2BtnPressed;
    noInterrupts();
    p1BtnChanged = g_p1ButtonChanged; g_p1ButtonChanged = false; p1BtnPressed = g_p1ButtonPressed;
    p2BtnChanged = g_p2ButtonChanged; g_p2ButtonChanged = false; p2BtnPressed = g_p2ButtonPressed;
    interrupts();

    bool applied = false;
    if (p1x != 0) { game.onEncoderInput(core::Player::P1, core::Game::Axis::X, p1x); applied = true; }
    if (p1y != 0) { game.onEncoderInput(core::Player::P1, core::Game::Axis::Y, p1y); applied = true; }
    if (p2x != 0) { game.onEncoderInput(core::Player::P2, core::Game::Axis::X, p2x); applied = true; }
    if (p2y != 0) { game.onEncoderInput(core::Player::P2, core::Game::Axis::Y, p2y); applied = true; }
    if (p1BtnChanged) { game.onButtonInput(core::Player::P1, p1BtnPressed); applied = true; }
    if (p2BtnChanged) { game.onButtonInput(core::Player::P2, p2BtnPressed); applied = true; }
    return applied;
}
