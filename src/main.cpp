#include <Arduino.h>

#include "Blinky.h"
#include "Display.h"
#include "HardwareIO.h"
#include "SerialIO.h"
#include "core/Game.h"

#define FRAMERATE 30

Display display;
core::Game game(display);

enum class GameState {
    IDLE,
    SHIPSELECT,
    PLAYING,
    ENDSCREEN
};
GameState state;

void setup() {
    Serial.begin(115200);
    blinkySetup();
    display.begin();
    hardwareIOBegin();
    game.processDisplayUpdates();
    printToSerial(display);

    state = GameState::IDLE;
}

void loop() {
    static uint32_t lastFrameMs = 0;
    uint32_t nowMs = millis();
    if (nowMs - lastFrameMs < 1000 / FRAMERATE) return;
    lastFrameMs = nowMs;
    blinkyUpdate();
    
    // -- get inputs -- 
    // debug: get inputs from serial, aka laptop
    pollAndApplySerial(game);
    // get inputs from the physical button + encoder
    pollAndApplyHardware(game);
    
    // -- update what should be on the display --
    display.clear();
    game.processDisplayUpdates();

    switch (state) {
        case GameState::IDLE: {
            static bool p1EverPressed = false;
            static bool p2EverPressed = false;
            p1EverPressed |= game.buttonPressed(core::Player::P1);
            p2EverPressed |= game.buttonPressed(core::Player::P2);
            if (p1EverPressed && p2EverPressed) {
                p1EverPressed = false;
                p2EverPressed = false;
                state = GameState::PLAYING;
            }
            break;
        }
        case GameState::SHIPSELECT:
            
            break;
        case GameState::PLAYING:
            
            break;
        case GameState::ENDSCREEN:
            
            break;
    }
    
    // -- actually write to the display --  
    display.show();

    // debug: push to serial as well so we can view on laptop
    printToSerial(display);
}

