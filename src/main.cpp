#include <Arduino.h>

#include "Blinky.h"
#include "Display.h"
#include "HardwareIO.h"
#include "Game.h"
#include "serial/SerialIO.h"
#include "Ship.h"
#include "Board.h"

#define FRAMERATE 60
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
    analogReadResolution(12);
    printToSerial(display);
}

void loop() {
    static uint32_t lastFrameMs = 0;
    static uint32_t shootingFinishedStartMs = 0;
    // Tracks the board's scan status as of the end of the previous loop()
    // call, so we can tell exactly when a scan transitions SCANNING -> IDLE
    // (i.e. just finished) this iteration -- see scanJustFinished below.
    static BoardStatus prevBoardStatus = BoardStatus::IDLE;
    uint32_t nowMs = millis();
    if (nowMs - lastFrameMs < 1000 / FRAMERATE) return;
    lastFrameMs = nowMs;
    blinkyUpdate();

    board.update();
    BoardStatus boardStatus = board.getStatus();
    bool scanJustFinished = (prevBoardStatus == BoardStatus::SCANNING && boardStatus == BoardStatus::IDLE);
    prevBoardStatus = boardStatus;

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
                board.beginScan(P1);
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
                    p1Ship = Ship(board.getCapPositions(P1));
                    board.beginScan(P2);
                    scanningP2 = true;
                } else {
                    p2Ship = Ship(board.getCapPositions(P2));
                    scanningP2 = false;

                    // p1Ship.addCell({0, 0});
                    // p1Ship.addCell({1, 0});
                    // p1Ship.addCell({2, 0});

                    // p2Ship.addCell({0, 0});
                    // p2Ship.addCell({0, 1});
                    // p2Ship.addCell({0, 2});
                    // p2Ship.addCell({1, 3});

                    // A scan coming back empty means no capacitors were
                    // detected at all -- none placed, or a wiring/threshold
                    // problem. An empty fleet can't be hit, so the game would
                    // otherwise run forever on guaranteed misses; drop in a
                    // single cell near the middle of that player's own board
                    // so the rig stays playable and winnable. Announced on
                    // serial so a stand-in fleet is never mistaken for a real
                    // one.
                    constexpr int kFallbackX = cfg::PLAYING_AREA_W / 2;
                    constexpr int kFallbackY = cfg::PLAYING_AREA_H / 2;
                    if (p1Ship.cellCount() == 0) {
                        Serial.println("WARNING: P1 scan found no capacitors -- substituting a fake 1-cell ship");
                        p1Ship.addCell({kFallbackX, kFallbackY});
                    }
                    if (p2Ship.cellCount() == 0) {
                        Serial.println("WARNING: P2 scan found no capacitors -- substituting a fake 1-cell ship");
                        p2Ship.addCell({kFallbackX, kFallbackY});
                    }

                    game.setCurrentPlayer(core::Player::P1);
                    game.setState(core::GameState::SELECTING);
                }
            }
            break;
        }
        case core::GameState::SELECTING: {
            core::Player current = game.getCurrentPlayer();
            core::Player other = core::otherPlayer(current);

            // Once a scan actually finishes (scanJustFinished), rebuild the
            // displayed Ship from its results -- this must happen BEFORE
            // beginScan() below, since beginScan() clears the capacitor
            // list to start the next scan, and would wipe out the very
            // results we're about to read. Only rebuilding on
            // scanJustFinished (rather than every loop) means the board
            // shown on screen holds its last complete state instead of
            // flickering through the partial, mid-scan contents of the
            // capacitor list.
            // if (scanJustFinished) {
            //     Ship& defendingShip = (other == core::Player::P1) ? p1Ship : p2Ship;
            //     defendingShip = Ship(board.getCapPositions(toDisplayPlayer(other)));
            // }

            // Kick off a rescan of the defending player's board every loop
            // (beginScan() no-ops while one's already in progress), since
            // the defense is free to add/rearrange capacitors before the
            // offense fires.
            // board.beginScan(toDisplayPlayer(other));

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

            // Victory deliberately isn't checked here -- the shot fired above
            // hasn't resolved yet. It's checked at the end of
            // SHOOTINGFINISHED, once the pop is done and the result is on
            // screen.

            break;
        }
        case core::GameState::SHOOTING:
            // Once the board select pin drops (i.e. Board's pop-charge
            // cycle is done -- cap popped or the safety timeout hit),
            // commit the shot (resolveShot() is idempotent, so calling it
            // every loop here is fine) and let Game::processDisplayUpdates
            // play out the reveal animation (snap shut, fade to black,
            // fade back in on the result) before handing off the turn --
            // see Game::isShotRevealComplete().
            if (board.getStatus() != BoardStatus::CHARGING) {
                game.resolveShot();
                if (game.isShotRevealComplete()) {
                    shootingFinishedStartMs = millis();
                    game.setState(core::GameState::SHOOTINGFINISHED);
                }
            }
            break;
        case core::GameState::SHOOTINGFINISHED:
            // Nothing but a delay -- Game::processDisplayUpdates already
            // draws this state as a static, unanimated SELECTING-style
            // board. Once the hold's up, hand the turn to whoever just
            // got shot at and resume SELECTING.
            if (millis() - shootingFinishedStartMs >= SHOOTING_FINISHED_MS) {
                core::Player justFired = game.getCurrentPlayer();
                core::Player defense = core::otherPlayer(justFired);
                Ship& defenseShip = (defense == core::Player::P1) ? p1Ship : p2Ship;
                // No guard against an empty fleet here on purpose: SHIPSELECT
                // substitutes a fake cell for any player whose scan came back
                // empty, so by this point every fleet has at least one cell.
                // Should one ever slip through anyway,
                // checkIfNoCellsRemaining() is vacuously true for an empty
                // ship and this just ends the game -- which beats running an
                // unwinnable one.
                //
                // NOTE: this assumes ships are built once (at SHIPSELECT) and
                // damage lives in Ship::health, which is how it works today.
                // If the per-turn rescan in SELECTING above is ever switched
                // back on, ships get rebuilt from surviving capacitors and the
                // SHIPSELECT fallback would start resurrecting sunk fleets
                // mid-game -- both would need rethinking together.
                bool won = defenseShip.checkIfNoCellsRemaining();
                if (won) {
                    game.setWinner(justFired);
                    game.setState(core::GameState::ENDSCREEN);
                    break;
                } else {
                    game.setCurrentPlayer(defense);
                    game.setState(core::GameState::SELECTING);
                }
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

