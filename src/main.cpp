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
    game.processDisplayUpdates();
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

    display.player(P1)(1, 1) = CRGB::Red;

    switch (game.getState()) {
        case core::GameState::IDLE:

            if (!(game.hasPressedButton(core::Player::P1) && game.hasPressedButton(core::Player::P2))) {
                break;
            }
            game.resetButtonPresses();
            game.setCurrentPlayer(core::Player::P1);

            // board.scanBoard(P1);
            // board.scanBoard(P2);

            // p1Ship = Ship(board.getCapPositions(P1));
            // p2Ship = Ship(board.getCapPositions(P2));

            p1Ship.addCell({0, 0});
            p1Ship.addCell({1, 0});
            p1Ship.addCell({2, 0});

            p2Ship.addCell({0, 0});
            p2Ship.addCell({0, 1});
            p2Ship.addCell({0, 2});

            p2Ship.addCell({1, 3});


            game.setState(core::GameState::SELECTING);
            break;
        case core::GameState::SHIPSELECT:
            // not used
            break;
        case core::GameState::SELECTING:
            if (game.consumePress(game.getCurrentPlayer())) {
                game.setCurrentPlayer(core::otherPlayer(game.getCurrentPlayer()));
                int32_t x = game.getPlayerInput(game.getCurrentPlayer()).x;
                int32_t y = game.getPlayerInput(game.getCurrentPlayer()).y;
                board.popCap(toDisplayPlayer(core::otherPlayer(game.getCurrentPlayer())), x, y);
            }

            
            // shooting

            // check if victory

            break;
        case core::GameState::SHOOTING:
            break;
        case core::GameState::ENDSCREEN:
            break;
    }

    // -- update what should be on the display --
    display.clear();
    game.processDisplayUpdates();
    display.player(P1)(1, 1) = CRGB::Red;
    display.player(P2)(1, 1) = CRGB::Red;

    p1Ship.draw(display.player(P1));
    p2Ship.draw(display.player(P2));

    // -- actually write to the display --
    display.show();

    // debug: push to serial as well so we can view on laptop
    printToSerial(display);
}

