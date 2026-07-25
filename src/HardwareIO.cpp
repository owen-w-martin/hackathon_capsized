#include "HardwareIO.h"

#include <Arduino.h>

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

constexpr uint32_t BUTTON_DEBOUNCE_US  = 20000;   // 20ms
constexpr uint32_t ENCODER_DEBOUNCE_US = 10000;    // 2ms -- bump up if still noisy

// Brief settle time before sampling the OTHER channel's level. Without this,
// a read that lands right as the other channel is mid-transition is a
// coin flip -- which shows up as noise/bounce in whichever rotation
// direction happens to place that race close together (the other direction
// naturally samples mid-stable-level and reads clean either way).
constexpr uint32_t ENCODER_SETTLE_US = 200;

// One tick per detent: triggered on A's rising edge, B's level gives direction.
// X = L encoder, Y = R encoder.
volatile int32_t g_p1EncXDelta = 0;
volatile int32_t g_p1EncYDelta = 0;
volatile int32_t g_p2EncXDelta = 0;
volatile int32_t g_p2EncYDelta = 0;

volatile uint32_t g_p1EncXALastMicros = 0;
volatile uint32_t g_p1EncYALastMicros = 0;
volatile uint32_t g_p2EncXALastMicros = 0;
volatile uint32_t g_p2EncYALastMicros = 0;
volatile uint32_t g_p1EncXBLastMicros = 0;
volatile uint32_t g_p1EncYBLastMicros = 0;
volatile uint32_t g_p2EncXBLastMicros = 0;
volatile uint32_t g_p2EncYBLastMicros = 0;

volatile bool     g_p1ButtonPressed    = false;
volatile bool     g_p1ButtonChanged    = false;
volatile uint32_t g_p1ButtonLastMicros = 0;

volatile bool     g_p2ButtonPressed    = false;
volatile bool     g_p2ButtonChanged    = false;
volatile uint32_t g_p2ButtonLastMicros = 0;

volatile bool g_p1ALowFirst = false;
volatile bool g_p1BLowFirst = false;
void p1EncXISRA() {
    uint32_t now = micros();
    if (now - g_p1EncXALastMicros < ENCODER_DEBOUNCE_US) return;
    g_p1EncXALastMicros = now;
    delayMicroseconds(ENCODER_SETTLE_US);
    if (g_p1BLowFirst) {
        g_p1EncXDelta ++;
        g_p1BLowFirst = false;
    } else {
        g_p1ALowFirst = true;
    }
}

void p1EncXISRB() {
    uint32_t now = micros();
    if (now - g_p1EncXBLastMicros < ENCODER_DEBOUNCE_US) return;
    g_p1EncXBLastMicros = now;
    delayMicroseconds(ENCODER_SETTLE_US);
    if (g_p1ALowFirst) {
        g_p1EncXDelta --;
        g_p1ALowFirst = false;
    } else {
        g_p1BLowFirst = true;
    }
}

void p1EncYISRA() {
    uint32_t now = micros();
    if (now - g_p1EncYALastMicros < ENCODER_DEBOUNCE_US) return;
    g_p1EncYALastMicros = now;
    delayMicroseconds(ENCODER_SETTLE_US);
    g_p1EncYDelta += (digitalRead(P1_ENC_R_B) == HIGH) ? 1 : -1;
}

void p2EncXISRA() {
    uint32_t now = micros();
    if (now - g_p2EncXALastMicros < ENCODER_DEBOUNCE_US) return;
    g_p2EncXALastMicros = now;
    delayMicroseconds(ENCODER_SETTLE_US);
    g_p2EncXDelta += (digitalRead(P2_ENC_L_B) == HIGH) ? 1 : -1;
}

void p2EncYISRA() {
    uint32_t now = micros();
    if (now - g_p2EncYALastMicros < ENCODER_DEBOUNCE_US) return;
    g_p2EncYALastMicros = now;
    delayMicroseconds(ENCODER_SETTLE_US);
    g_p2EncYDelta += (digitalRead(P2_ENC_R_B) == HIGH) ? 1 : -1;
}


void p1EncYISRB() {
    uint32_t now = micros();
    if (now - g_p1EncYBLastMicros < ENCODER_DEBOUNCE_US) return;
    g_p1EncYBLastMicros = now;
    delayMicroseconds(ENCODER_SETTLE_US);
    g_p1EncYDelta += (digitalRead(P1_ENC_R_A) == HIGH) ? 1 : -1;
}

void p2EncXISRB() {
    uint32_t now = micros();
    if (now - g_p2EncXBLastMicros < ENCODER_DEBOUNCE_US) return;
    g_p2EncXBLastMicros = now;
    delayMicroseconds(ENCODER_SETTLE_US);
    g_p2EncXDelta += (digitalRead(P2_ENC_L_A) == HIGH) ? 1 : -1;
}

void p2EncYISRB() {
    uint32_t now = micros();
    if (now - g_p2EncYBLastMicros < ENCODER_DEBOUNCE_US) return;
    g_p2EncYBLastMicros = now;
    delayMicroseconds(ENCODER_SETTLE_US);
    g_p2EncYDelta += (digitalRead(P2_ENC_R_A) == HIGH) ? 1 : -1;
}
// Debounced in the ISR itself so callers never see a bounce as a real change.
void p1ButtonISR() {
    uint32_t now = micros();
    if (now - g_p1ButtonLastMicros < BUTTON_DEBOUNCE_US) return;
    g_p1ButtonLastMicros = now;

    g_p1ButtonPressed = (digitalRead(P1_FIRE) == LOW);   // active-low, INPUT_PULLUP
    g_p1ButtonChanged = true;
}

void p2ButtonISR() {
    uint32_t now = micros();
    if (now - g_p2ButtonLastMicros < BUTTON_DEBOUNCE_US) return;
    g_p2ButtonLastMicros = now;

    g_p2ButtonPressed = (digitalRead(P2_FIRE) == LOW);   // active-low, INPUT_PULLUP
    g_p2ButtonChanged = true;
}

}  // namespace

void hardwareIOBegin() {
    pinMode(P1_FIRE, INPUT_PULLUP);
    pinMode(P1_ENC_R_A, INPUT_PULLUP);
    pinMode(P1_ENC_R_B, INPUT_PULLUP);
    pinMode(P1_ENC_L_A, INPUT_PULLUP);
    pinMode(P1_ENC_L_B, INPUT_PULLUP);

    pinMode(P2_FIRE, INPUT_PULLUP);
    pinMode(P2_ENC_R_A, INPUT_PULLUP);
    pinMode(P2_ENC_R_B, INPUT_PULLUP);
    pinMode(P2_ENC_L_A, INPUT_PULLUP);
    pinMode(P2_ENC_L_B, INPUT_PULLUP);

    attachInterrupt(digitalPinToInterrupt(P1_FIRE), p1ButtonISR, CHANGE);
    attachInterrupt(digitalPinToInterrupt(P1_ENC_L_A), p1EncXISRA, FALLING);
    attachInterrupt(digitalPinToInterrupt(P1_ENC_R_A), p1EncYISRA, FALLING);
    attachInterrupt(digitalPinToInterrupt(P1_ENC_L_B), p1EncXISRB, FALLING);
    attachInterrupt(digitalPinToInterrupt(P1_ENC_R_B), p1EncYISRB, FALLING);

    attachInterrupt(digitalPinToInterrupt(P2_FIRE), p2ButtonISR, CHANGE);
    attachInterrupt(digitalPinToInterrupt(P2_ENC_L_A), p2EncXISRA, FALLING);
    attachInterrupt(digitalPinToInterrupt(P2_ENC_R_A), p2EncYISRA, FALLING);
    attachInterrupt(digitalPinToInterrupt(P2_ENC_L_B), p2EncXISRB, FALLING);
    attachInterrupt(digitalPinToInterrupt(P2_ENC_R_B), p2EncYISRB, FALLING);
}

bool pollAndApplyHardware(core::Game& game) {
    int32_t p1x, p1y, p2x, p2y;
    bool p1BtnChanged, p1BtnPressed, p2BtnChanged, p2BtnPressed;

    noInterrupts();
    p1x = g_p1EncXDelta; g_p1EncXDelta = 0;
    p1y = g_p1EncYDelta; g_p1EncYDelta = 0;
    p2x = g_p2EncXDelta; g_p2EncXDelta = 0;
    p2y = g_p2EncYDelta; g_p2EncYDelta = 0;
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
