#include <Arduino.h>

#include "Blinky.h"
#include "Display.h"
#include "HardwareIO.h"
#include "Game.h"
#include "serial/SerialIO.h"
#include "Ship.h"
#include "Board.h"

#define FRAMERATE 30

namespace {
::Player toDisplayPlayer(core::Player p) {
    return (p == core::Player::P1) ? P1 : P2;
}
}  // namespace

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

    switch (game.getState()) {
        case core::GameState::IDLE:

            if (!(game.hasPressedButton(core::Player::P1) && game.hasPressedButton(core::Player::P2))) {
                break;
            }
            game.resetButtonPresses();
            game.setCurrentPlayer(core::Player::P1);

            board.scanBoard(P1);
            board.scanBoard(P2);

            p1Ship = Ship(board.getCapPositions(P1));
            p2Ship = Ship(board.getCapPositions(P2));

            // p1Ship.addCell({0, 0});
            // p1Ship.addCell({1, 0});
            // p1Ship.addCell({2, 0});

            // p2Ship.addCell({0, 0});
            // p2Ship.addCell({0, 1});
            // p2Ship.addCell({0, 2});

            // p2Ship.addCell({1, 3});


            game.setState(core::GameState::SELECTING);
            break;
        case core::GameState::SHIPSELECT:
            // not used
            break;
        case core::GameState::SELECTING:
            if (game.consumePress(game.getCurrentPlayer())) {
                core::Player offense = game.getCurrentPlayer();
                core::Player defense = core::otherPlayer(offense);
                int32_t x = game.getPlayerInput(offense).x;
                int32_t y = game.getPlayerInput(offense).y;

                Ship& defenseShip = (defense == core::Player::P1) ? p1Ship : p2Ship;
                defenseShip.checkHit({x, y});

                // board.popCap(toDisplayPlayer(defense), x, y);
                game.setCurrentPlayer(defense);
            }

            // check if victory

            break;
        case core::GameState::SHOOTING:
            break;
        case core::GameState::ENDSCREEN:
            break;
    }

    // -- update what should be on the display --
    display.clear();
    game.processDisplayUpdates(p1Ship, p2Ship);

    // -- actually write to the display --
    display.show();

    // debug: push to serial as well so we can view on laptop
    // printToSerial(display);
}

