#include <Arduino.h>

#include "Blinky.h"
#include "Display.h"
#include "HardwareIO.h"
#include "Game.h"
#include "serial/SerialIO.h"
#include "Ship.h"
#include "Board.h"

#define FRAMERATE 100
// How long SHOOTINGFINISHED holds the resolved (unanimated) board on
// screen before handing the turn over and returning to SELECTING.
constexpr uint32_t SHOOTING_FINISHED_MS = 4000;

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
    static uint32_t shootingFinishedStartMs = 0;
    uint32_t nowMs = millis();
    if (nowMs - lastFrameMs < 1000 / FRAMERATE) return;
    lastFrameMs = nowMs;
    blinkyUpdate();

    board.update();

    // -- get inputs --
    // debug: get inputs from serial, aka laptop
    pollAndApplySerial(game, p1Ship, p2Ship, board);
    // get inputs from the physical button + encoder
    pollAndApplyHardware(game);

    switch (game.getState()) {
        case core::GameState::IDLE:

            // if both players have pressed their button, kick off the (non-blocking) board scan
            if ((game.hasPressedButton(core::Player::P1) && game.hasPressedButton(core::Player::P2))) {
                game.resetButtonPresses();
                // board.beginScan(P1);
                game.setState(core::GameState::SHIPSELECT);
            }

           break;
        case core::GameState::SHIPSELECT: {
            // Gate state for the non-blocking board scan kicked off above --
            // board.update() (called at the top of loop(), every
            // iteration) is what actually advances it a step at a time.
            // Scans P1 first, then P2, then builds both ships and starts
            // the game.
            static bool scanningP2 = false;
            if (board.getStatus() == BoardStatus::IDLE) {
                if (!scanningP2) {
                    // p1Ship = Ship(board.getCapPositions(P1));
                    // board.beginScan(P2);
                    scanningP2 = true;
                } else {
                    // p2Ship = Ship(board.getCapPositions(P2));
                    scanningP2 = false;

                    p1Ship.addCell({0, 0});
                    p1Ship.addCell({1, 0});
                    p1Ship.addCell({2, 0});

                    p2Ship.addCell({0, 0});
                    p2Ship.addCell({0, 1});
                    p2Ship.addCell({0, 2});
                    p2Ship.addCell({1, 3});

                    game.setCurrentPlayer(core::Player::P1);
                    game.setState(core::GameState::SELECTING);
                }
            }
            break;
        }
        case core::GameState::SELECTING: {
            core::Player current = game.getCurrentPlayer();
            core::Player other = core::otherPlayer(current);

            // Rescan the defending player's board every loop, since the
            // defense is free to add/rearrange capacitors before the
            // offense fires.
            // board.beginScan(toDisplayPlayer(other));
            // Ship& defendingShip = (other == core::Player::P1) ? p1Ship : p2Ship;
            // defendingShip = Ship(board.getCapPositions(toDisplayPlayer(other)));

            // The defense's button does nothing this turn, but a press
            // still sets justPressed -- left unconsumed, it would stay
            // latched and cause an instant, un-aimed auto-fire the moment
            // this player becomes offense. Drain and discard it here.
            game.consumePress(other);

            if (game.consumePress(current)) {
                core::Player offense = current;
                core::Player defense = other;
                int32_t x = game.getPlayerInput(offense).x;
                int32_t y = game.getPlayerInput(offense).y;

                Serial.print("Targeting x: ");
                Serial.print(x);
                Serial.print(", y: ");
                Serial.println(y);

                Ship& defenseShip = (defense == core::Player::P1) ? p1Ship : p2Ship;
                bool hit = defenseShip.checkHit({x, y});

                board.popCap(toDisplayPlayer(defense), x, y);
                // The shot isn't recorded into the ShotGrid (and so isn't
                // shown on any screen) until resolveShot(), below, once it
                // has actually resolved -- otherwise the defense's screen
                // would reveal the hit the instant the button was pressed,
                // before the pop even starts. Current player also swaps
                // only once SHOOTING ends -- see Game::beginShooting().
                game.beginShooting(offense, x, y, hit);
            }

            // check if victory

            break;
        }
        case core::GameState::SHOOTING:
            // Stay here for exactly as long as the board select pin is on
            // (i.e. Board's pop-charge cycle is still running); once it's
            // back to IDLE the shot has fully resolved, so reveal it and
            // move to SHOOTINGFINISHED to hold that result on screen
            // briefly before handing off the turn.
            if (board.getStatus() != BoardStatus::CHARGING) {
                game.resolveShot();
                shootingFinishedStartMs = millis();
                game.setState(core::GameState::SHOOTINGFINISHED);
            }
            break;
        case core::GameState::SHOOTINGFINISHED:
            // Nothing but a delay -- Game::processDisplayUpdates already
            // draws this state as a static, unanimated SELECTING-style
            // board. Once the hold's up, hand the turn to whoever just
            // got shot at and resume SELECTING.
            if (millis() - shootingFinishedStartMs >= SHOOTING_FINISHED_MS) {
                core::Player justFired = game.getCurrentPlayer();
                game.setCurrentPlayer(core::otherPlayer(justFired));
                game.setState(core::GameState::SELECTING);
            }
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
    printToSerial(display);
}

