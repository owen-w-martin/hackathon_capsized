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

constexpr uint32_t DEBOUNCE_US = 5000;   // 5ms

// One tick per detent: triggered on A's rising edge, B's level gives direction.
// X = L encoder, Y = R encoder.
volatile int32_t g_p1EncXDelta = 0;
volatile int32_t g_p1EncYDelta = 0;
volatile int32_t g_p2EncXDelta = 0;
volatile int32_t g_p2EncYDelta = 0;

// DEBUG: one fire-count per physical pin -- including the B channels, which
// the movement logic never used to watch. If a pin that shouldn't be
// involved lights up when you turn a different encoder, that pin/wire is
// picking up something it shouldn't.
volatile uint32_t g_p1EncXACount = 0;   // P1_ENC_L_A
volatile uint32_t g_p1EncXBCount = 0;   // P1_ENC_L_B
volatile uint32_t g_p1EncYACount = 0;   // P1_ENC_R_A
volatile uint32_t g_p1EncYBCount = 0;   // P1_ENC_R_B
volatile uint32_t g_p2EncXACount = 0;   // P2_ENC_L_A
volatile uint32_t g_p2EncXBCount = 0;   // P2_ENC_L_B
volatile uint32_t g_p2EncYACount = 0;   // P2_ENC_R_A
volatile uint32_t g_p2EncYBCount = 0;   // P2_ENC_R_B
volatile uint32_t g_p1FireCount  = 0;   // P1_FIRE
volatile uint32_t g_p2FireCount  = 0;   // P2_FIRE

volatile bool     g_p1ButtonPressed    = false;
volatile bool     g_p1ButtonChanged    = false;
volatile uint32_t g_p1ButtonLastMicros = 0;

volatile bool     g_p2ButtonPressed    = false;
volatile bool     g_p2ButtonChanged    = false;
volatile uint32_t g_p2ButtonLastMicros = 0;

// A-channel ISRs drive real movement (RISING only) and count their own fires.
void p1EncXISR_A() { g_p1EncXACount++; g_p1EncXDelta += (digitalRead(P1_ENC_L_B) == HIGH) ? 1 : -1; }
void p1EncYISR_A() { g_p1EncYACount++; g_p1EncYDelta += (digitalRead(P1_ENC_R_B) == HIGH) ? 1 : -1; }
void p2EncXISR_A() { g_p2EncXACount++; g_p2EncXDelta += (digitalRead(P2_ENC_L_B) == HIGH) ? 1 : -1; }
void p2EncYISR_A() { g_p2EncYACount++; g_p2EncYDelta += (digitalRead(P2_ENC_R_B) == HIGH) ? 1 : -1; }

// B-channel ISRs are diagnostic only -- they don't drive movement, just count.
void p1EncXISR_B() { g_p1EncXBCount++; }
void p1EncYISR_B() { g_p1EncYBCount++; }
void p2EncXISR_B() { g_p2EncXBCount++; }
void p2EncYISR_B() { g_p2EncYBCount++; }

// Debounced in the ISR itself so callers never see a bounce as a real change.
void p1ButtonISR() {
    g_p1FireCount++;
    uint32_t now = micros();
    if (now - g_p1ButtonLastMicros < DEBOUNCE_US) return;
    g_p1ButtonLastMicros = now;

    g_p1ButtonPressed = (digitalRead(P1_FIRE) == LOW);   // active-low, INPUT_PULLUP
    g_p1ButtonChanged = true;
}

void p2ButtonISR() {
    g_p2FireCount++;
    uint32_t now = micros();
    if (now - g_p2ButtonLastMicros < DEBOUNCE_US) return;
    g_p2ButtonLastMicros = now;

    g_p2ButtonPressed = (digitalRead(P2_FIRE) == LOW);   // active-low, INPUT_PULLUP
    g_p2ButtonChanged = true;
}

// DEBUG: every loop, prints each pin's total (cumulative, never-reset) fire
// count, by name, on one line -- without ever calling Serial from inside an
// ISR (unsafe/slow). Turn one encoder and watch exactly which name's count
// keeps climbing.
void debugPrintAllCounts() {
    struct Watch { const char* name; volatile uint32_t* count; };
    static const Watch watches[] = {
        {"P1_ENC_L_A", &g_p1EncXACount},
        {"P1_ENC_L_B", &g_p1EncXBCount},
        {"P1_ENC_R_A", &g_p1EncYACount},
        {"P1_ENC_R_B", &g_p1EncYBCount},
        {"P2_ENC_L_A", &g_p2EncXACount},
        {"P2_ENC_L_B", &g_p2EncXBCount},
        {"P2_ENC_R_A", &g_p2EncYACount},
        {"P2_ENC_R_B", &g_p2EncYBCount},
        {"P1_FIRE",    &g_p1FireCount},
        {"P2_FIRE",    &g_p2FireCount},
    };

    for (const Watch& w : watches) {
        noInterrupts();
        uint32_t current = *w.count;
        interrupts();

        Serial.print(w.name);
        Serial.print("=");
        Serial.print(current);
        Serial.print("  ");
    }
    Serial.println();
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
    attachInterrupt(digitalPinToInterrupt(P1_ENC_L_A), p1EncXISR_A, RISING);
    attachInterrupt(digitalPinToInterrupt(P1_ENC_L_B), p1EncXISR_B, CHANGE);
    attachInterrupt(digitalPinToInterrupt(P1_ENC_R_A), p1EncYISR_A, RISING);
    attachInterrupt(digitalPinToInterrupt(P1_ENC_R_B), p1EncYISR_B, CHANGE);

    attachInterrupt(digitalPinToInterrupt(P2_FIRE), p2ButtonISR, CHANGE);
    attachInterrupt(digitalPinToInterrupt(P2_ENC_L_A), p2EncXISR_A, RISING);
    attachInterrupt(digitalPinToInterrupt(P2_ENC_L_B), p2EncXISR_B, CHANGE);
    attachInterrupt(digitalPinToInterrupt(P2_ENC_R_A), p2EncYISR_A, RISING);
    attachInterrupt(digitalPinToInterrupt(P2_ENC_R_B), p2EncYISR_B, CHANGE);
}

bool pollAndApplyHardware(core::Game& game) {
    debugPrintAllCounts();

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
