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

// Draws a crosshair -- full-width horizontal arm through targetY, full-
// height vertical arm through targetX -- converging inward from all four
// screen edges toward the target cell, but deliberately stopping one pixel
// short of it: the target cell itself is never lit by this function, no
// matter how close gapScale gets to 0, so it stays dark until the caller
// explicitly reveals it once the shot's actually resolved. gapScale is the
// fraction of each arm's original edge-to-target distance that's still
// unlit: 1 means nothing lit yet (fresh from the edges), ~0 means each arm
// has closed the gap down to that last held-back pixel.
//
// Each arm grows a fractional number of pixels per frame rather than a
// whole one, so the leading pixel is antialiased -- blended with whatever
// was already drawn there (the sweep background) in proportion to how far
// the boundary has crossed into it -- giving smooth sub-pixel motion
// instead of the arm visibly jumping a whole LED at a time.
void drawConvergingCrosshair(PlayerScreen& screen, int targetX, int targetY, float gapScale, CRGB color) {
    if (gapScale < 0.0f) gapScale = 0.0f;

    // Every arm's growth and antialiased leading pixel is clamped to stop
    // at least one pixel short of the target index, so this function can
    // never light the target cell itself even at gapScale == 0.
    float growLeft   = fminf(targetX * (1.0f - gapScale), targetX - 1.0f);
    float growRight  = fminf((cfg::SCREEN_W - 1 - targetX) * (1.0f - gapScale), cfg::SCREEN_W - 2 - targetX);
    float growTop    = fminf(targetY * (1.0f - gapScale), targetY - 1.0f);
    float growBottom = fminf((cfg::SCREEN_H - 1 - targetY) * (1.0f - gapScale), cfg::SCREEN_H - 2 - targetY);

    if (growLeft >= 0.0f) {
        int leftFull = (int)floorf(growLeft);
        for (int x = 0; x <= leftFull; x++) screen.setPixel(x, targetY, color);
        float leftCoverage = growLeft - leftFull;
        if (leftCoverage > 0.0f) {
            int x = leftFull + 1;
            screen.setPixel(x, targetY, CRGB::blend(screen.getPixel(x, targetY), color, (uint8_t)(255 * leftCoverage)));
        }
    }

    if (growRight >= 0.0f) {
        int rightFull = (int)floorf(growRight);
        for (int i = 0; i <= rightFull; i++) screen.setPixel(cfg::SCREEN_W - 1 - i, targetY, color);
        float rightCoverage = growRight - rightFull;
        if (rightCoverage > 0.0f) {
            int x = cfg::SCREEN_W - 1 - (rightFull + 1);
            screen.setPixel(x, targetY, CRGB::blend(screen.getPixel(x, targetY), color, (uint8_t)(255 * rightCoverage)));
        }
    }

    if (growTop >= 0.0f) {
        int topFull = (int)floorf(growTop);
        for (int y = 0; y <= topFull; y++) screen.setPixel(targetX, y, color);
        float topCoverage = growTop - topFull;
        if (topCoverage > 0.0f) {
            int y = topFull + 1;
            screen.setPixel(targetX, y, CRGB::blend(screen.getPixel(targetX, y), color, (uint8_t)(255 * topCoverage)));
        }
    }

    if (growBottom >= 0.0f) {
        int bottomFull = (int)floorf(growBottom);
        for (int i = 0; i <= bottomFull; i++) screen.setPixel(targetX, cfg::SCREEN_H - 1 - i, color);
        float bottomCoverage = growBottom - bottomFull;
        if (bottomCoverage > 0.0f) {
            int y = cfg::SCREEN_H - 1 - (bottomFull + 1);
            screen.setPixel(targetX, y, CRGB::blend(screen.getPixel(targetX, y), color, (uint8_t)(255 * bottomCoverage)));
        }
    }
}

// End-screen letters, one string per row with row 0 at the TOP of the panel.
// Drawn across the full screen rather than just the 10x10 playing area so
// they're as large as the hardware allows -- there's no board to show here
// any more, so the border ring is free real estate.
static_assert(cfg::SCREEN_W == 12 && cfg::SCREEN_H == 12,
              "end-screen letters are hand-drawn for a 12x12 panel");

// Two vees sharing a centre peak. Deliberately left-right symmetric, so it
// reads as a W no matter which way round a panel ends up wired.
const char* const kGlyphWin[cfg::SCREEN_H] = {
    "110000000011",
    "110000000011",
    "011000000110",
    "011000000110",
    "011001100110",
    "011001100110",
    "001110011100",
    "001110011100",
    "001110011100",
    "001110011100",
    "000110011000",
    "000110011000",
};

// Two-pixel-wide stem down the left, two-pixel foot along the bottom.
const char* const kGlyphLose[cfg::SCREEN_H] = {
    "001100000000",
    "001100000000",
    "001100000000",
    "001100000000",
    "001100000000",
    "001100000000",
    "001100000000",
    "001100000000",
    "001100000000",
    "001100000000",
    "001111111100",
    "001111111100",
};

// Paints one letter onto a panel in a single colour.
//
// y counts up from the bottom of the screen while the bitmaps above read
// top-down, hence the SCREEN_H-1-row flip. No per-player x mirror: both
// panels turned out to use the exact same transform on the real rig, in
// spite of what Display::show()'s arithmetic suggests -- see the panel_pos
// note in tools/display_mirror.py. If a rewired panel ever does come out
// backwards, the W is symmetric and unaffected; reverse kGlyphLose's strings
// to fix the L.
void drawGlyph(PlayerScreen& screen, const char* const rows[], CRGB color) {
    for (int row = 0; row < cfg::SCREEN_H; row++) {
        const char* bits = rows[row];
        int y = cfg::SCREEN_H - 1 - row;
        for (int x = 0; x < cfg::SCREEN_W; x++) {
            if (bits[x] == '1') screen(x, y) = color;
        }
    }
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
        // Once a shot's been fired it plays the reveal animation instead
        // (Layer 4, below) -- fades out, then regrows from the edges.
        const CRGB kCrosshairColor = CRGB(30, 70, 5); // desaturated neon green, less yellow
        if (m_state == GameState::SELECTING) {
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

        // Layer 4, SHOOTING only: the shot reveal animation, gated on the
        // real charge/pop event (see Game::resolveShot()) rather than a
        // fixed countdown -- durations in shotReveal (Game.h). Runs after
        // Layer 3, so once resolveShot() has committed the shot, this
        // layer's fade-to-black/fade-in passes act on the fully composited
        // frame (sweep + shot marker) rather than pre-empting it. The
        // target cell itself is deliberately kept dark through every phase
        // up to the resolve -- it's the one thing this whole animation is
        // building suspense around -- and only lit again on its own,
        // ahead of everything else, once the shot has actually resolved:
        //
        //   1. the SELECTING crosshair that was sitting on the target cell
        //      quickly fades to black (target cell excluded).
        //   2. a red crosshair regrows in from the four screen edges,
        //      easing out (fast, then slowing) toward the target cell --
        //      mathematically never quite reaching it -- for as long as
        //      the shot is still charging.
        //   3. once the shot actually resolves: fade the whole screen to
        //      black (target cell forced black throughout, in case Layer 3
        //      already committed the marker there), then fade in just the
        //      target cell with the real hit/miss result, then fade in
        //      everything else around it.
        if (m_state == GameState::SHOOTING) {
            using namespace shotReveal;
            const CRGB kCrosshairRed = CRGB(180, 0, 0);

            int targetX = cfg::PLAYING_AREA_X0 + mirrorX(m_shotX);
            int targetY = cfg::PLAYING_AREA_Y0 + m_shotY;
            uint32_t sinceShotMs = millis() - m_shootingStartMs;

            if (!m_shotResolved) {
                if (sinceShotMs < kCrosshairFadeOutMs) {
                    float fadeT = sinceShotMs / (float)kCrosshairFadeOutMs;
                    CRGB fadingColor = CRGB::blend(kCrosshairColor, CRGB::Black, (uint8_t)(255 * fadeT));
                    for (int x = 0; x < cfg::SCREEN_W; x++) if (x != targetX) offenseScreen(x, targetY) = fadingColor;
                    for (int y = 0; y < cfg::SCREEN_H; y++) if (y != targetY) offenseScreen(targetX, y) = fadingColor;
                } else {
                    float growthMs = (float)(sinceShotMs - kCrosshairFadeOutMs);
                    float gapScale = expf(-growthMs / kGrowthTauMs);
                    drawConvergingCrosshair(offenseScreen, targetX, targetY, gapScale, kCrosshairRed);
                }
            } else {
                const CRGB resultColor = m_pendingHit ? kHitRed : kMissWhite;
                uint32_t sinceResolvedMs = millis() - m_shotResolvedMs;

                if (sinceResolvedMs < kFadeToBlackMs) {
                    float t = sinceResolvedMs / (float)kFadeToBlackMs;
                    uint8_t fadeBy = (uint8_t)(255 * t);
                    for (int y = 0; y < cfg::SCREEN_H; y++) {
                        for (int x = 0; x < cfg::SCREEN_W; x++) {
                            if (x == targetX && y == targetY) {
                                offenseScreen(x, y) = CRGB::Black;
                            } else {
                                offenseScreen(x, y) = CRGB::blend(offenseScreen(x, y), CRGB::Black, fadeBy);
                            }
                        }
                    }
                } else if (sinceResolvedMs < kFadeToBlackMs + kFadeInTargetMs) {
                    float t = (sinceResolvedMs - kFadeToBlackMs) / (float)kFadeInTargetMs;
                    for (int y = 0; y < cfg::SCREEN_H; y++)
                        for (int x = 0; x < cfg::SCREEN_W; x++)
                            offenseScreen(x, y) = CRGB::Black;
                    offenseScreen(targetX, targetY) = CRGB::blend(CRGB::Black, resultColor, (uint8_t)(255 * t));
                } else if (sinceResolvedMs < kTotalRevealMs) {
                    float t = (sinceResolvedMs - kFadeToBlackMs - kFadeInTargetMs) / (float)kFadeInRestMs;
                    uint8_t keep = (uint8_t)(255 * t);
                    for (int y = 0; y < cfg::SCREEN_H; y++) {
                        for (int x = 0; x < cfg::SCREEN_W; x++) {
                            if (x == targetX && y == targetY) {
                                offenseScreen(x, y) = resultColor;
                            } else {
                                offenseScreen(x, y) = CRGB::blend(CRGB::Black, offenseScreen(x, y), keep);
                            }
                        }
                    }
                }
                // else: reveal's fully played out -- normal board, held as-is
                // until the caller advances past isShotRevealComplete().
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
        // resolving" without washing everything out to solid white. Kept
        // dim (kDefenseFlashMaxWeight below) so it stays in the background
        // relative to the offense's own crosshair/reveal animation.
        uint8_t shootingPulse = 0;
        if (m_state == GameState::SHOOTING) {
            constexpr uint32_t kShootingPulsePeriodMs = 1600;
            float phase = (millis() - m_shootingStartMs) / (float)kShootingPulsePeriodMs * TWO_PI;
            shootingPulse = (uint8_t)((sinf(phase) * 0.5f + 0.5f) * 255);

            constexpr uint8_t kDefenseFlashMaxWeight = 60;   // dim -- just a hint of "under fire"
            uint8_t flashWeight = (uint8_t)(((uint16_t)shootingPulse * kDefenseFlashMaxWeight) / 255);
            for (int y = 0; y < cfg::PLAYING_AREA_H; y++) {
                for (int x = 0; x < cfg::PLAYING_AREA_W; x++) {
                    int sx = cfg::PLAYING_AREA_X0 + x, sy = cfg::PLAYING_AREA_Y0 + y;
                    defenseScreen(sx, sy) = CRGB::blend(defenseScreen(sx, sy), CRGB::White, flashWeight);
                }
            }
        }

        // Top bar: during SHOOTING, mirror the offense's crosshair row
        // (Layer 4, above) onto the bar pixel-for-pixel, so the bar echoes
        // the same fade/regrow sequence, rather than just pulsing a single
        // column. The one exception is the target column itself: on the
        // offense's own screen that pixel still shows the dim green sweep
        // (Layer 4 only withholds the crosshair color there, not the
        // background underneath) -- but the bar has no sweep of its own to
        // explain a dim green pixel sitting there, so it'd just look like
        // an unintended hint. Force it off instead, and only let it show
        // through once the shot's resolved and Layer 4 is actively
        // revealing it (black -> result color -- see Layer 4's Phase 3).
        // Otherwise, highlight the column the offense is currently aiming
        // at in red, black everywhere else -- lines up with the
        // crosshair's vertical arm since it shares the same x coordinate.
        if (m_state == GameState::SHOOTING) {
            int targetX = cfg::PLAYING_AREA_X0 + mirrorX(m_shotX);
            int targetY = cfg::PLAYING_AREA_Y0 + m_shotY;
            for (int x = 0; x < cfg::SCREEN_W; x++) {
                bool withheld = (x == targetX) && !m_shotResolved;
                m_display.bar()(x) = withheld ? CRGB::Black : offenseScreen(x, targetY);
            }
        } else {
            // The bar's columns map straight to physical columns (see
            // Display::show()), but P1's screen is physically mirrored
            // end-to-end there. It should match the defense's side, so
            // flip the bar column whenever the defense is P1 (i.e. the
            // offense is P2); when the defense is P2, whose screen isn't
            // mirrored, use it as-is.
            int barCol = cfg::PLAYING_AREA_X0 + cx;
            if (defense == Player::P1) {
                barCol = cfg::SCREEN_W - 1 - barCol;
            }
            m_display.bar().clear();
            m_display.bar()(barCol) = CRGB::Red;
        }
    } else if (m_state == GameState::ENDSCREEN) {
        // The result, one letter per panel: a big green W for whoever sank
        // the other fleet, a big red L for whoever lost it. The W breathes so
        // the win is the animated, alive-looking one; the L is deliberately
        // flat and dimmer.
        constexpr uint32_t kPulsePeriodMs = 2000;
        float phase = (millis() % kPulsePeriodMs) / (float)kPulsePeriodMs * TWO_PI;
        uint8_t pulse = (uint8_t)((sinf(phase) * 0.5f + 0.5f) * 255);

        const CRGB kWinDim    = CRGB(0, 45, 5);
        const CRGB kWinBright = CRGB(0, 210, 30);
        const CRGB kLoseRed   = CRGB(110, 0, 0);
        CRGB winColor = CRGB::blend(kWinDim, kWinBright, pulse);

        Player loser = otherPlayer(m_winner);
        drawGlyph(m_display.player(toDisplayPlayer(m_winner)), kGlyphWin, winColor);
        drawGlyph(m_display.player(toDisplayPlayer(loser)), kGlyphLose, kLoseRed);

        // Bar echoes the result across the same P1-left / P2-right split the
        // readiness bar uses, the winner's half pulsing in step with their W.
        int half = cfg::SCREEN_W / 2;
        CRGB p1BarColor = (m_winner == Player::P1) ? winColor : kLoseRed;
        CRGB p2BarColor = (m_winner == Player::P2) ? winColor : kLoseRed;
        for (int i = 0; i < half; i++)             m_display.bar()(i) = p1BarColor;
        for (int i = half; i < cfg::SCREEN_W; i++) m_display.bar()(i) = p2BarColor;
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
        // Otherwise it shows the ambient color for the current game state
        // -- except IDLE, whose ambient is a slow, dim white breathing
        // pulse instead of solid black, so the bar reads as "powered on,
        // waiting for both players" rather than looking off. Each half
        // still switches to solid red independently the moment that
        // player confirms, same as any other state.
        CRGB stateColor = colorForState(m_state);
        if (m_state == GameState::IDLE) {
            constexpr uint32_t kIdlePulsePeriodMs = 3000;
            constexpr uint8_t kIdlePulseMaxBrightness = 40; // lightly -- not a full-white flash
            float phase = (millis() % kIdlePulsePeriodMs) / (float)kIdlePulsePeriodMs * TWO_PI;
            uint8_t pulse = (uint8_t)((sinf(phase) * 0.5f + 0.5f) * kIdlePulseMaxBrightness);
            stateColor = CRGB(pulse, pulse, pulse);
        }
        CRGB p1BarColor = hasPressedButton(Player::P1) ? CRGB(255, 0, 0) : stateColor;
        CRGB p2BarColor = hasPressedButton(Player::P2) ? CRGB(255, 0, 0) : stateColor;
        int half = cfg::SCREEN_W / 2;
        for (int i = 0; i < half; i++)             m_display.bar()(i) = p1BarColor;
        for (int i = half; i < cfg::SCREEN_W; i++) m_display.bar()(i) = p2BarColor;
    }
}

}
