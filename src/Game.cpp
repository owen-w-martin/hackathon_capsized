#include "Game.h"

#include <math.h>

#include "Ship.h"

namespace core {

namespace {

int32_t clampToAxis(int32_t v, int size) {
    if (v < 0) return 0;
    if (v > size - 1) return size - 1;
    return v;
}

// Ambient color shown on the top bar for the current game state.
CRGB colorForState(GameState state) {
    switch (state) {
        case GameState::IDLE:       return CRGB::Black;
        case GameState::SHIPSELECT: return CRGB::Black;
        case GameState::SELECTING:  return CRGB::Green;
        case GameState::SHOOTING:   return CRGB::Black;
        case GameState::SHOOTINGFINISHED: return CRGB::Black;
        case GameState::ENDSCREEN:  return CRGB::Pink;
    }
    return CRGB::Black;
}

::Player toDisplayPlayer(Player p) {
    return (p == Player::P1) ? ::P1 : ::P2;
}

// Radar-sweep brightness (0-255) for one cell: peaks right at the sweep's
// leading edge (angle == sweepAngle) and fades to 0 over trailRad radians
// behind it. Shared by the SELECTING/SHOOTING offense screen and the
// SHIPSELECT scanning screen.
uint8_t sweepBrightness(int x, int y, float centerX, float centerY, float sweepAngle, float trailRad) {
    float cellAngle = atan2f(y - centerY, x - centerX);
    if (cellAngle < 0) cellAngle += TWO_PI;

    float trail = sweepAngle - cellAngle;
    if (trail < 0) trail += TWO_PI;

    if (trail >= trailRad) return 0;
    return (uint8_t)(255.0f * (1.0f - trail / trailRad));
}

}  // namespace

void Game::onEncoderInput(Player p, Axis axis, int32_t delta) {
    PlayerInput& in = (p == Player::P1) ? m_p1 : m_p2;
    // Both axes are inverted from the raw encoder reading: y=0 is the
    // bottom of the screen (see Display::show()), so moving "up" must
    // increase y; and the offense's own screen is drawn mirrored in x (see
    // processDisplayUpdates), so moving the crosshair visually right must
    // decrease x.
    int32_t step = (delta > 0) ? -1 : 1;
    int32_t& coord = (axis == Axis::X) ? in.x : in.y;
    int size = (axis == Axis::X) ? cfg::PLAYING_AREA_W : cfg::PLAYING_AREA_H;
    coord = clampToAxis(coord + step, size);
}

void Game::onButtonInput(Player p, bool pressed) {
    PlayerInput& in = (p == Player::P1) ? m_p1 : m_p2;
    bool wasPressed = in.button;
    in.button = pressed;
    if (pressed) in.everPressed = true;
    if (pressed && !wasPressed) in.justPressed = true;
}

bool Game::hasPressedButton(Player p) const {
    const PlayerInput& in = (p == Player::P1) ? m_p1 : m_p2;
    return in.everPressed;
}

bool Game::consumePress(Player p) {
    PlayerInput& in = (p == Player::P1) ? m_p1 : m_p2;
    bool result = in.justPressed;
    in.justPressed = false;
    return result;
}

void Game::processDisplayUpdates(Ship& p1Ship, Ship& p2Ship) {
    m_display.clear();

    if (m_state == GameState::SELECTING || m_state == GameState::SHOOTING ||
        m_state == GameState::SHOOTINGFINISHED) {
        // The defender's ship is only relevant for the defense's own screen
        // now -- the offense no longer gets to see it directly, only their
        // own shot history against it.
        Player defense = otherPlayer(m_currentPlayer);
        Ship& defenseShip = (defense == Player::P1) ? p1Ship : p2Ship;
        const PlayerInput& offenseIn = (m_currentPlayer == Player::P1) ? m_p1 : m_p2;

        // Target cell within the 10x10 grid -- already clamped to range by
        // onEncoderInput, so this is a direct board coordinate.
        int cx = static_cast<int>(offenseIn.x);
        int cy = static_cast<int>(offenseIn.y);


        // Offense's screen is three stacked layers, back to front: the blue
        // background, the crosshair on top of it, then their own shot
        // history (white miss / red hit) on top of that -- so a shot marker
        // always reads clearly even on the row/column currently being aimed
        // at. The actual ship layout stays hidden.
        //
        // The offense's own screen is mirrored in x relative to the real
        // target coordinate (cx/cy, used everywhere else -- hit checks, the
        // defense's ship draw, the shot grid, the bar) so that aiming right
        // on the offense's own panel lines up with the same visual side on
        // the defense's panel, compensating for the two players' screens
        // being physically mirror images of each other.
        auto mirrorX = [](int x) { return cfg::PLAYING_AREA_W - 1 - x; };

        PlayerScreen& offenseScreen = m_display.player(toDisplayPlayer(m_currentPlayer));
        const ShotGrid& offenseShots = getShots(m_currentPlayer);

        // Layer 1: a sonar sweep across the WHOLE screen (border included).
        // In the 1-pixel border ring it's an unsaturated black-to-gray
        // gradient, a dim light gray right at its leading edge (not full
        // white -- too bright against everything else here). Within the
        // playing area it stays in flavors of green instead -- from the
        // plain dark phosphor green at rest up to a brighter green at the
        // sweep's peak, but that peak is still dimmer than the crosshair
        // color (see kCrosshairColor below) so the crosshair always reads
        // as the brightest thing on screen.
        const CRGB kOffenseGreen = CRGB(5, 10, 2);
        const CRGB kSweepGreenPeak = CRGB(17, 40, 3);   // dimmer than kCrosshairColor (30, 70, 5)
        const CRGB kSweepBorderPeak = CRGB(50, 50, 50); // dim light gray, not full white
        constexpr uint32_t kSweepPeriodMs = 2500;
        constexpr float kSweepTrailRad = PI * 0.4f;   // trailing wedge angle -- was PI/2 (90 deg), now ~72 deg
        constexpr float kCenterX = (cfg::SCREEN_W - 1) / 2.0f;
        constexpr float kCenterY = (cfg::SCREEN_H - 1) / 2.0f;

        float sweepAngle = (millis() % kSweepPeriodMs) / (float)kSweepPeriodMs * TWO_PI;

        for (int y = 0; y < cfg::SCREEN_H; y++) {
            for (int x = 0; x < cfg::SCREEN_W; x++) {
                uint8_t brightness = sweepBrightness(x, y, kCenterX, kCenterY, sweepAngle, kSweepTrailRad);

                bool inPlayingArea = x >= cfg::PLAYING_AREA_X0 && x < cfg::PLAYING_AREA_X0 + cfg::PLAYING_AREA_W &&
                                      y >= cfg::PLAYING_AREA_Y0 && y < cfg::PLAYING_AREA_Y0 + cfg::PLAYING_AREA_H;
                CRGB color = inPlayingArea
                    ? CRGB::blend(kOffenseGreen, kSweepGreenPeak, brightness)
                    : CRGB::blend(CRGB::Black, kSweepBorderPeak, brightness);
                offenseScreen(x, y) = color;
            }
        }

        // Layer 2, SELECTING only: the crosshair, drawn solid, arms running
        // the full width/height of the screen so they reach the border.
        // Once a shot's been fired (SHOOTING/SHOOTINGFINISHED) it's gone --
        // the offense is no longer choosing where to aim, so a crosshair
        // sitting on the just-fired cell would just be stale clutter over
        // the converging brackets and the resolved shot marker.
        if (m_state == GameState::SELECTING) {
            const CRGB kCrosshairColor = CRGB(30, 70, 5); // desaturated neon green, less yellow
            for (int x = 0; x < cfg::SCREEN_W; x++)
                offenseScreen(x, cfg::PLAYING_AREA_Y0 + cy) = kCrosshairColor;
            for (int y = 0; y < cfg::SCREEN_H; y++)
                offenseScreen(cfg::PLAYING_AREA_X0 + mirrorX(cx), y) = kCrosshairColor;
        }

        // Layer 3: shot history, drawn on top of the crosshair.
        const CRGB kMissWhite = CRGB(125, 125, 125);
        const CRGB kHitRed = CRGB(125, 0, 0);
        for (int y = 0; y < cfg::PLAYING_AREA_H; y++) {
            for (int x = 0; x < cfg::PLAYING_AREA_W; x++) {
                switch (offenseShots(x, y)) {
                    case ShotResult::MISS:
                        offenseScreen(cfg::PLAYING_AREA_X0 + mirrorX(x), cfg::PLAYING_AREA_Y0 + y) = kMissWhite;
                        break;
                    case ShotResult::HIT:
                        offenseScreen(cfg::PLAYING_AREA_X0 + mirrorX(x), cfg::PLAYING_AREA_Y0 + y) = kHitRed;
                        break;
                    case ShotResult::NONE:
                        break;
                }
            }
        }

        // Layer 4, SHOOTING only: four red L-shaped brackets travel in from
        // the screen's corners and converge on the cell just fired at, like
        // a shrinking targeting reticle. All four share the same t, so they
        // always move in lockstep; each arm always reaches all the way out
        // to its own edge of the screen (not a fixed short stub), so at any
        // moment the four brackets' arms trace a complete (shrinking)
        // rectangle around the target. The starting vertex is inset one
        // pixel from the true corner -- starting exactly on the corner
        // would make the very first frame's "L" a single pixel with
        // zero-length arms, invisible as an L at all.
        if (m_state == GameState::SHOOTING) {
            constexpr uint32_t kConvergeDurationMs = 7000;
            const CRGB kConvergeColor = CRGB(180, 0, 0);

            float t = (millis() - m_shootingStartMs) / (float)kConvergeDurationMs;
            if (t > 1.0f) t = 1.0f;

            int targetX = cfg::PLAYING_AREA_X0 + mirrorX(m_shotX);
            int targetY = cfg::PLAYING_AREA_Y0 + m_shotY;

            // {startX, startY, edgeX, edgeY} per corner -- start is inset
            // one pixel from the true edge so the initial L is visible;
            // edge is where each arm always extends out to.
            const int brackets[4][4] = {
                {1, 1, 0, 0},
                {cfg::SCREEN_W - 2, 1, cfg::SCREEN_W - 1, 0},
                {1, cfg::SCREEN_H - 2, 0, cfg::SCREEN_H - 1},
                {cfg::SCREEN_W - 2, cfg::SCREEN_H - 2, cfg::SCREEN_W - 1, cfg::SCREEN_H - 1},
            };
            for (const auto& b : brackets) {
                int startX = b[0], startY = b[1], edgeX = b[2], edgeY = b[3];
                int vertexX = startX + (int)roundf((targetX - startX) * t);
                int vertexY = startY + (int)roundf((targetY - startY) * t);

                int xLo = (edgeX < vertexX) ? edgeX : vertexX;
                int xHi = (edgeX < vertexX) ? vertexX : edgeX;
                for (int x = xLo; x <= xHi; x++) offenseScreen.setPixel(x, vertexY, kConvergeColor);

                int yLo = (edgeY < vertexY) ? edgeY : vertexY;
                int yHi = (edgeY < vertexY) ? vertexY : edgeY;
                for (int y = yLo; y <= yHi; y++) offenseScreen.setPixel(vertexX, y, kConvergeColor);
            }
        }

        // Defense's playing area gets a solid blue backdrop, same footprint,
        // with their own fleet's positions shown in gray, then the offense's
        // shots against them overlaid on top -- white for a miss, red for a
        // hit on one of their own ship cells -- so defense can see both
        // their fleet and how it's being hit at the same time. Same in
        // SHOOTING as in SELECTING, just with a subtle white pulse over the
        // top (below) while the shot resolves.
        PlayerScreen& defenseScreen = m_display.player(toDisplayPlayer(defense));
        const CRGB kDefenseBackground = CRGB(0, 0, 15);
        for (int y = 0; y < cfg::PLAYING_AREA_H; y++)
            for (int x = 0; x < cfg::PLAYING_AREA_W; x++)
                defenseScreen(cfg::PLAYING_AREA_X0 + x, cfg::PLAYING_AREA_Y0 + y) = kDefenseBackground;
        // CRGB::Gray (128,128,128) reads as near-white on the LEDs -- dim it
        // down so it's clearly distinct from the white miss marker below.
        const CRGB kShipGray = CRGB(25, 25, 25);
        defenseShip.draw(defenseScreen, kShipGray);
        for (int y = 0; y < cfg::PLAYING_AREA_H; y++) {
            for (int x = 0; x < cfg::PLAYING_AREA_W; x++) {
                switch (offenseShots(x, y)) {
                    case ShotResult::MISS:
                        defenseScreen(cfg::PLAYING_AREA_X0 + x, cfg::PLAYING_AREA_Y0 + y) = CRGB::White;
                        break;
                    case ShotResult::HIT:
                        defenseScreen(cfg::PLAYING_AREA_X0 + x, cfg::PLAYING_AREA_Y0 + y) = CRGB::Red;
                        break;
                    case ShotResult::NONE:
                        break;
                }
            }
        }

        // SHOOTING only: a slow, subtle white pulse over the whole defense
        // playing area, blended over whatever's currently drawn there
        // (background/ship/shot markers) -- reads as "under fire,
        // resolving" without washing everything out to solid white. Also
        // drives the bar's pulse below, on the same phase (there at full
        // strength -- the bar's not showing ship/shot detail, so it can
        // afford to be less subtle).
        uint8_t shootingPulse = 0;
        if (m_state == GameState::SHOOTING) {
            constexpr uint32_t kShootingPulsePeriodMs = 1600;
            float phase = (millis() - m_shootingStartMs) / (float)kShootingPulsePeriodMs * TWO_PI;
            shootingPulse = (uint8_t)((sinf(phase) * 0.5f + 0.5f) * 255);

            constexpr uint8_t kDefenseFlashMaxWeight = 130;   // subtle, but a clearly visible swing
            uint8_t flashWeight = (uint8_t)(((uint16_t)shootingPulse * kDefenseFlashMaxWeight) / 255);
            for (int y = 0; y < cfg::PLAYING_AREA_H; y++) {
                for (int x = 0; x < cfg::PLAYING_AREA_W; x++) {
                    int sx = cfg::PLAYING_AREA_X0 + x, sy = cfg::PLAYING_AREA_Y0 + y;
                    defenseScreen(sx, sy) = CRGB::blend(defenseScreen(sx, sy), CRGB::White, flashWeight);
                }
            }
        }

        // Top bar: during SHOOTING, pulse just the column that was fired at
        // (shootingPulse, computed above; m_shotX rather than cx, since the
        // offense's cursor isn't frozen and could have drifted since firing).
        // Otherwise, highlight the column the offense is currently aiming
        // at in red, black everywhere else -- lines up with the
        // crosshair's vertical arm since it shares the same x coordinate.
        //
        // The bar's columns map straight to physical columns (see
        // Display::show()), but P1's screen is physically mirrored
        // end-to-end there. It should match the defense's side, so flip the
        // bar column whenever the defense is P1 (i.e. the offense is P2);
        // when the defense is P2, whose screen isn't mirrored, use it as-is.
        if (m_state == GameState::SHOOTING) {
            int shotBarCol = cfg::PLAYING_AREA_X0 + m_shotX;
            if (defense == Player::P1) {
                shotBarCol = cfg::SCREEN_W - 1 - shotBarCol;
            }
            m_display.bar().clear();
            m_display.bar()(shotBarCol) = CRGB(shootingPulse, 0, 0);
        } else {
            int barCol = cfg::PLAYING_AREA_X0 + cx;
            if (defense == Player::P1) {
                barCol = cfg::SCREEN_W - 1 - barCol;
            }
            m_display.bar().clear();
            m_display.bar()(barCol) = CRGB::Red;
        }
    } else {
        // Outside SELECTING/SHOOTING (e.g. IDLE), the player screens stay
        // solid black -- except during SHIPSELECT (boards being scanned,
        // neither player is offense/defense yet), where both screens get
        // the same neutral gray radar sweep (the one the border ring uses
        // during SELECTING), across the whole screen rather than just the
        // border.
        if (m_state == GameState::SHIPSELECT) {
            constexpr uint32_t kSweepPeriodMs = 2500;
            constexpr float kSweepTrailRad = PI * 0.4f;
            constexpr float kCenterX = (cfg::SCREEN_W - 1) / 2.0f;
            constexpr float kCenterY = (cfg::SCREEN_H - 1) / 2.0f;
            const CRGB kSweepPeak = CRGB(50, 50, 50);

            float sweepAngle = (millis() % kSweepPeriodMs) / (float)kSweepPeriodMs * TWO_PI;

            for (Player p : {Player::P1, Player::P2}) {
                PlayerScreen& screen = m_display.player(toDisplayPlayer(p));
                for (int y = 0; y < cfg::SCREEN_H; y++) {
                    for (int x = 0; x < cfg::SCREEN_W; x++) {
                        uint8_t brightness = sweepBrightness(x, y, kCenterX, kCenterY, sweepAngle, kSweepTrailRad);
                        screen(x, y) = CRGB::blend(CRGB::Black, kSweepPeak, brightness);
                    }
                }
            }
        }

        // Bar is split in half: left reflects P1's readiness, right
        // reflects P2's -- turns red the moment that player presses their
        // button and stays red (not just while held) so each player can
        // see the other is ready while waiting on their own press.
        // Otherwise it shows the ambient color for the current game state.
        CRGB stateColor = colorForState(m_state);
        CRGB p1BarColor = hasPressedButton(Player::P1) ? CRGB(255, 0, 0) : stateColor;
        CRGB p2BarColor = hasPressedButton(Player::P2) ? CRGB(255, 0, 0) : stateColor;
        int half = cfg::SCREEN_W / 2;
        for (int i = 0; i < half; i++)             m_display.bar()(i) = p1BarColor;
        for (int i = half; i < cfg::SCREEN_W; i++) m_display.bar()(i) = p2BarColor;
    }
}

}
