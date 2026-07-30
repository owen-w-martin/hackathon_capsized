#include <Arduino.h>

#include "Blinky.h"
#include "Display.h"
#include "HardwareIO.h"
#include "Game.h"
#include "serial/SerialIO.h"
#include "Ship.h"
#include "Board.h"

#define FRAMERATE 30

Display display;
Board board;
core::Game game(display);
Ship p1Ship;
Ship p2Ship;

void setup() {
    Serial.begin(115200);
    blinkySetup();
    display.begin();
    hardwareIOBegin();
    game.processDisplayUpdates(p1Ship, p2Ship);
    printToSerial(display);
}

void loop() {
    static uint32_t lastFrameMs = 0;
    uint32_t nowMs = millis();
    if (nowMs - lastFrameMs < 1000 / FRAMERATE) return;
    lastFrameMs = nowMs;
    blinkyUpdate();

    board.update();

    // -- get inputs --
    // debug: get inputs from serial, aka laptop
    pollAndApplySerial(game, p1Ship, p2Ship);
    // get inputs from the physical button + encoder
    pollAndApplyHardware(game);

    // Snapshot of the shot fired in SELECTING. Encoders keep moving during
    // SHOOTING, so the target position and hit/miss result have to be
    // captured at fire time rather than re-read from live input state once
    // SHOOTING resolves and records it.
    static int32_t  s_shotX = 0;
    static int32_t  s_shotY = 0;
    static bool     s_shotHit = false;
    static uint32_t s_shotStartMs = 0;

    switch (game.getState()) {
        case core::GameState::IDLE:

            // if both players have pressed their button, transition to the SELECTING state
            if ((game.hasPressedButton(core::Player::P1) && game.hasPressedButton(core::Player::P2))) {
                game.resetButtonPresses();
                game.setCurrentPlayer(core::Player::P1);

                board.scanBoard(P1);
                board.scanBoard(P2);

                p1Ship = Ship(board.getCapPositions(P1));
                p2Ship = Ship(board.getCapPositions(P2));

                /*
                p1Ship.addCell({0, 0});
                p1Ship.addCell({1, 0});
                p1Ship.addCell({2, 0});

                p2Ship.addCell({0, 0});
                p2Ship.addCell({0, 1});
                p2Ship.addCell({0, 2});
                p2Ship.addCell({1, 3});

                */

                game.setState(core::GameState::SELECTING);
             }

           break;
        case core::GameState::SHIPSELECT:
            // not used
            break;
        case core::GameState::SELECTING: {
            core::Player offense = game.getCurrentPlayer();
            core::Player defense = core::otherPlayer(offense);

            // CODE TO SCAN BOARD EVERY LOOP
            // board.scanBoard(P2);
            // Ship& currentShip = p2Ship;
            // currentShip = Ship(board.getCapPositions(P2));

            // The defense's button does nothing this turn, but a press
            // still sets justPressed -- left unconsumed, it would stay
            // latched and cause an instant, un-aimed auto-fire the moment
            // this player becomes offense. Drain and discard it here.
            game.consumePress(defense);

            if (game.consumePress(offense)) {
                int32_t x = game.getPlayerInput(offense).x;
                int32_t y = game.getPlayerInput(offense).y;

                Serial.print("Targeting x: ");
                Serial.print(x);
                Serial.print(", y: ");
                Serial.println(y);

                Ship& defenseShip = (defense == core::Player::P1) ? p1Ship : p2Ship;
                bool hit = defenseShip.checkHit({x, y});

                board.popCap(core::toDisplayPlayer(defense), x, y);

                // The shot is locked in, but offense/defense don't swap yet,
                // and it isn't recorded onto the display yet either -- both
                // happen once SHOOTING resolves it below.
                s_shotX = x;
                s_shotY = y;
                s_shotHit = hit;
                s_shotStartMs = millis();
                game.setState(core::GameState::SHOOTING);
            }

            // check if victory

            break;
        }
        case core::GameState::SHOOTING: {
            core::Player offense = game.getCurrentPlayer();
            core::Player defense = core::otherPlayer(offense);

            // A hit only resolves once the physical capacitor actually
            // pops; a miss has nothing to pop, so it just holds for a fixed
            // beat instead.
            constexpr uint32_t kMissDelayMs = 5000;
            bool doneShooting = s_shotHit
                ? board.isCapPopped(core::toDisplayPlayer(defense), {s_shotX, s_shotY})
                : (millis() - s_shotStartMs >= kMissDelayMs);

            if (doneShooting) {
                game.recordShot(offense, s_shotX, s_shotY, s_shotHit);
                game.endTurn();
                game.setState(core::GameState::SELECTING);
            }

            break;
        }
        case core::GameState::ENDSCREEN:
            break;
    }

    // -- update what should be on the display --
    display.clear();
    game.processDisplayUpdates(p1Ship, p2Ship);

    // -- actually write to the display --
    display.show();

    // debug: push to serial as well so we can view on laptop
    printToSerial(display);
}

