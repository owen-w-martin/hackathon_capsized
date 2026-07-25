#include <Arduino.h>
#include <FastLED.h>
#include <math.h>
#include <string.h>
#include "Display.h"
#include "Animations.h"

extern Display display;

void animPlasma() {
  static uint8_t  hueOffset = 0;
  static uint16_t frame     = 0;

  // Swirling plasma: two overlapping sine waves per pixel, phase-shifted over
  // time, mapped through the color wheel. P2 gets a mirrored copy so the two
  // screens breathe in and out of sync with each other.
  for (int y = 0; y < cfg::SCREEN_H; y++) {
    for (int x = 0; x < cfg::SCREEN_W; x++) {
      uint8_t v    = sin8(x * 18 + frame) / 2 + sin8(y * 18 - frame) / 2;
      CRGB    c    = CHSV(v + hueOffset, 255, 255);
      display.player(P1)(x, y)                     = c;
      display.player(P2)(cfg::SCREEN_W - 1 - x, y) = c;
    }
  }

  // Rainbow chase across the light bar.
  for (int i = 0; i < cfg::SCREEN_W; i++) {
    display.bar()(i) = CHSV(hueOffset + i * (256 / cfg::SCREEN_W), 255, 255);
  }

  display.show();

  hueOffset += 2;
  frame     += 6;
  delay(20);
}

// Dinner: a plate gets eaten bite by bite (a Pac-Man-style wedge, growing
// from angle 0), with a fork sparkle at each bite, steam rising off the food,
// and a fresh randomly-colored dish served once the plate is clean. P2 sits
// across the table -- a mirrored copy, like a dinner companion.
void animDinner() {
  static float    eatenAngle    = 0.0f;
  static uint16_t pauseCounter  = 0;
  static float    nextBiteAngle = 0.35f;
  static uint8_t  forkFlashTimer = 0;
  static float    forkFlashAngle = 0.0f;
  static CRGB     foodColor      = CRGB(200, 80, 20);
  static float    steamPhase     = 0.0f;

  const float cx        = (cfg::SCREEN_W - 1) / 2.0f;
  const float plateCy   = 8.0f;
  const float plateR    = 3.6f;
  const float aaWidth   = 0.12f;
  const float biteStep  = 0.35f;
  const CRGB  plateColor = CRGB(45, 40, 36);

  bool eating = (pauseCounter == 0);
  if (eating && eatenAngle >= 2 * PI) {
    pauseCounter = 50;   // meal finished -> brief pause before a new dish is served
  }

  float steamIntensity = eating ? fmaxf(0.0f, 1.0f - eatenAngle / (2 * PI)) : 0.0f;

  for (int y = 0; y < cfg::SCREEN_H; y++) {
    for (int x = 0; x < cfg::SCREEN_W; x++) {
      float dx = x - cx;
      float dy = y - plateCy;
      float r  = sqrtf(dx * dx + dy * dy);
      CRGB  pixel = CRGB::Black;

      if (r <= plateR) {
        float phi = atan2f(dy, dx);
        if (phi < 0) phi += 2 * PI;

        // A soft-edged wedge sweeps out from angle 0, like a bite being taken.
        float edge = phi - eatenAngle;
        if (edge < -aaWidth) {
          pixel = plateColor;                              // already eaten -> bare plate
        } else if (edge < aaWidth) {
          float t = (edge + aaWidth) / (2 * aaWidth);
          pixel   = blend(plateColor, foodColor, (uint8_t)(t * 255));
        } else {
          pixel = foodColor;                               // untouched food
        }
      }

      // Fork sparkle right where the last bite was taken.
      if (forkFlashTimer > 0) {
        float fx = cx + (plateR + 0.8f) * cosf(forkFlashAngle);
        float fy = plateCy + (plateR + 0.8f) * sinf(forkFlashAngle);
        float fd = sqrtf((x - fx) * (x - fx) + (y - fy) * (y - fy));
        if (fd < 0.9f) {
          uint8_t b = (uint8_t)((1.0f - fd) * (forkFlashTimer / 6.0f) * 255);
          pixel     = blend(pixel, CRGB(220, 220, 230), b);
        }
      }

      // Rising steam over the food, fading out as the plate empties.
      if (steamIntensity > 0.0f && y < 5) {
        for (int w = -1; w <= 1; w++) {
          float wx = cx + w * 1.6f + sinf(y * 0.9f + steamPhase + w * 2.0f) * 0.9f;
          if (fabsf(x - wx) < 0.5f) {
            uint8_t b = (uint8_t)(steamIntensity * (5 - y) * 20);
            pixel     = blend(pixel, CRGB(200, 220, 230), b);
          }
        }
      }

      display.player(P1)(x, y)                     = pixel;
      display.player(P2)(cfg::SCREEN_W - 1 - x, y) = pixel;   // dinner companion across the table
    }
  }

  // Bar: how much food is left, draining as the plate is eaten.
  float remaining = eating ? fmaxf(0.0f, 1.0f - eatenAngle / (2 * PI)) : 0.0f;
  float litF      = remaining * cfg::SCREEN_W;
  int   fullLit   = (int)litF;
  float frac      = litF - fullLit;

  display.bar().clear();
  for (int i = 0; i < fullLit && i < cfg::SCREEN_W; i++) display.bar()(i) = foodColor;
  if (fullLit < cfg::SCREEN_W) {
    CRGB partial = foodColor;
    partial.nscale8((uint8_t)(frac * 255));
    display.bar()(fullLit) = partial;
  }

  display.show();

  steamPhase += 0.15f;

  if (eating) {
    eatenAngle += 0.02f;
    if (eatenAngle >= nextBiteAngle && nextBiteAngle < 2 * PI) {
      forkFlashTimer = 6;
      forkFlashAngle = nextBiteAngle;
      nextBiteAngle += biteStep;
    }
  } else {
    pauseCounter--;
    if (pauseCounter == 0) {
      eatenAngle    = 0.0f;
      nextBiteAngle = biteStep;
      foodColor     = CHSV((uint8_t)random(256), 200, 220);   // a new dish each meal
    }
  }

  if (forkFlashTimer > 0) forkFlashTimer--;

  delay(30);
}

// Tetris, auto-played as two independent 12x12 games (one per screen). Pieces
// spawn already in a random orientation (no live rotation modeled) and drift
// diagonally toward a randomly chosen target column while falling -- a
// deliberate simplification that still reads as "a game of Tetris happening."
struct TetShape { CRGB color; int8_t cell[4][2]; };

static const TetShape TET_SHAPES[13] = {
  { CRGB(0, 220, 220),  { {0,0}, {1,0}, {2,0}, {3,0} } },   // I horizontal
  { CRGB(0, 220, 220),  { {0,0}, {0,1}, {0,2}, {0,3} } },   // I vertical
  { CRGB(220, 200, 0),  { {0,0}, {1,0}, {0,1}, {1,1} } },   // O
  { CRGB(170, 0, 220),  { {0,0}, {1,0}, {2,0}, {1,1} } },   // T down
  { CRGB(170, 0, 220),  { {1,0}, {0,1}, {1,1}, {2,1} } },   // T up
  { CRGB(0, 200, 0),    { {1,0}, {2,0}, {0,1}, {1,1} } },   // S
  { CRGB(0, 200, 0),    { {0,0}, {0,1}, {1,1}, {1,2} } },   // S vertical
  { CRGB(220, 0, 0),    { {0,0}, {1,0}, {1,1}, {2,1} } },   // Z
  { CRGB(220, 0, 0),    { {1,0}, {0,1}, {1,1}, {0,2} } },   // Z vertical
  { CRGB(0, 80, 220),   { {0,0}, {0,1}, {1,1}, {2,1} } },   // J
  { CRGB(0, 80, 220),   { {1,0}, {2,0}, {1,1}, {1,2} } },   // J vertical
  { CRGB(230, 120, 0),  { {2,0}, {0,1}, {1,1}, {2,1} } },   // L
  { CRGB(230, 120, 0),  { {1,0}, {1,1}, {1,2}, {2,2} } },   // L vertical
};

struct FallingPiece { int8_t shapeIdx, px, py, targetPx; };

static bool tetPieceFits(uint8_t (&board)[cfg::SCREEN_H][cfg::SCREEN_W], int8_t shapeIdx, int px, int py) {
  const TetShape &s = TET_SHAPES[shapeIdx];
  for (int i = 0; i < 4; i++) {
    int x = px + s.cell[i][0];
    int y = py + s.cell[i][1];
    if (x < 0 || x >= cfg::SCREEN_W || y < 0 || y >= cfg::SCREEN_H) return false;
    if (board[y][x] != 0) return false;
  }
  return true;
}

// Standard 4-factor board heuristic (aggregate height, complete lines, holes,
// bumpiness) used to score a hypothetical placement -- higher is better.
static float tetScoreBoard(uint8_t (&board)[cfg::SCREEN_H][cfg::SCREEN_W]) {
  int heights[cfg::SCREEN_W];
  int holes = 0;

  for (int x = 0; x < cfg::SCREEN_W; x++) {
    int top = cfg::SCREEN_H;
    for (int y = 0; y < cfg::SCREEN_H; y++) {
      if (board[y][x] != 0) { top = y; break; }
    }
    heights[x] = cfg::SCREEN_H - top;
    for (int y = top + 1; y < cfg::SCREEN_H; y++) if (board[y][x] == 0) holes++;
  }

  int aggHeight = 0, bumpiness = 0;
  for (int x = 0; x < cfg::SCREEN_W; x++) {
    aggHeight += heights[x];
    if (x > 0) bumpiness += abs(heights[x] - heights[x - 1]);
  }

  int lines = 0;
  for (int y = 0; y < cfg::SCREEN_H; y++) {
    bool full = true;
    for (int x = 0; x < cfg::SCREEN_W; x++) if (board[y][x] == 0) { full = false; break; }
    if (full) lines++;
  }

  return -0.51f * aggHeight + 0.76f * lines - 0.36f * holes - 0.18f * bumpiness;
}

// Tries every valid column for this (fixed-orientation) piece, simulates
// dropping it straight down, and returns whichever column scores best.
static int tetBestColumn(uint8_t (&board)[cfg::SCREEN_H][cfg::SCREEN_W], int8_t shapeIdx, int lo, int hi, int spawnPy) {
  const TetShape &s = TET_SHAPES[shapeIdx];
  int   bestPx    = lo;
  float bestScore = -1e9f;

  for (int px = lo; px <= hi; px++) {
    int py = spawnPy;
    while (tetPieceFits(board, shapeIdx, px, py + 1)) py++;
    if (!tetPieceFits(board, shapeIdx, px, py)) continue;

    uint8_t trial[cfg::SCREEN_H][cfg::SCREEN_W];
    memcpy(trial, board, sizeof(trial));
    for (int i = 0; i < 4; i++) trial[py + s.cell[i][1]][px + s.cell[i][0]] = shapeIdx + 1;

    float score = tetScoreBoard(trial);
    if (score > bestScore) { bestScore = score; bestPx = px; }
  }
  return bestPx;
}

static void tetSpawn(FallingPiece &f, uint8_t (&board)[cfg::SCREEN_H][cfg::SCREEN_W]) {
  f.shapeIdx = random(13);
  const TetShape &s = TET_SHAPES[f.shapeIdx];
  int minX = 3, maxX = 0, minY = 3, maxY = 0;
  for (int i = 0; i < 4; i++) {
    minX = min(minX, (int)s.cell[i][0]);
    maxX = max(maxX, (int)s.cell[i][0]);
    minY = min(minY, (int)s.cell[i][1]);
    maxY = max(maxY, (int)s.cell[i][1]);
  }
  int lo = -minX;
  int hi = (cfg::SCREEN_W - 1) - maxX;
  f.py       = -minY;
  f.px       = lo + random(hi - lo + 1);           // random entry column, just for the visual slide-in
  f.targetPx = tetBestColumn(board, f.shapeIdx, lo, hi, f.py);
}

void animTetris() {
  static uint8_t      board[2][cfg::SCREEN_H][cfg::SCREEN_W];
  static FallingPiece falling[2];
  static uint8_t      topOutFlash[2] = { 0, 0 };
  static uint8_t      barFlash       = 0;
  static uint16_t     tick           = 0;
  static bool         initialized    = false;

  if (!initialized) {
    randomSeed(micros());
    tetSpawn(falling[0], board[0]);
    tetSpawn(falling[1], board[1]);
    initialized = true;
  }

  bool stepNow = (++tick % 4 == 0);

  for (int p = 0; p < 2; p++) {
    if (!stepNow) continue;
    FallingPiece &f = falling[p];

    // Slide one column toward the target and fall one row, same tick -> a diagonal drift.
    if (f.px < f.targetPx && tetPieceFits(board[p], f.shapeIdx, f.px + 1, f.py)) f.px++;
    else if (f.px > f.targetPx && tetPieceFits(board[p], f.shapeIdx, f.px - 1, f.py)) f.px--;

    if (tetPieceFits(board[p], f.shapeIdx, f.px, f.py + 1)) {
      f.py++;
      continue;
    }

    // Can't fall further -- lock it into the stack.
    const TetShape &s = TET_SHAPES[f.shapeIdx];
    for (int i = 0; i < 4; i++) {
      board[p][f.py + s.cell[i][1]][f.px + s.cell[i][0]] = f.shapeIdx + 1;
    }

    // Compact the stack, dropping any completed rows.
    int writeY = cfg::SCREEN_H - 1;
    for (int y = cfg::SCREEN_H - 1; y >= 0; y--) {
      bool full = true;
      for (int x = 0; x < cfg::SCREEN_W; x++) if (board[p][y][x] == 0) { full = false; break; }
      if (full) {
        barFlash = 255;
      } else {
        if (writeY != y) for (int x = 0; x < cfg::SCREEN_W; x++) board[p][writeY][x] = board[p][y][x];
        writeY--;
      }
    }
    for (int y = writeY; y >= 0; y--) for (int x = 0; x < cfg::SCREEN_W; x++) board[p][y][x] = 0;

    tetSpawn(f, board[p]);
    if (!tetPieceFits(board[p], f.shapeIdx, f.px, f.py)) {
      // No room for the next piece -- topped out, start a fresh game.
      memset(board[p], 0, sizeof(board[p]));
      topOutFlash[p] = 10;
      tetSpawn(f, board[p]);
    }
  }

  for (int p = 0; p < 2; p++) {
    Player player = (p == 0) ? P1 : P2;

    for (int y = 0; y < cfg::SCREEN_H; y++) {
      for (int x = 0; x < cfg::SCREEN_W; x++) {
        uint8_t cell  = board[p][y][x];
        CRGB    pixel = cell ? TET_SHAPES[cell - 1].color : CRGB::Black;
        if (topOutFlash[p] > 0) pixel = blend(pixel, CRGB::Red, 160);
        display.player(player)(x, y) = pixel;
      }
    }

    const FallingPiece &f = falling[p];
    const TetShape &s = TET_SHAPES[f.shapeIdx];
    for (int i = 0; i < 4; i++) {
      display.player(player)(f.px + s.cell[i][0], f.py + s.cell[i][1]) = s.color;
    }

    if (topOutFlash[p] > 0) topOutFlash[p]--;
  }

  // Bar: a bright "line clear!" flash, decaying quickly back to black.
  display.bar().fill(CRGB(barFlash, barFlash, barFlash));
  barFlash = (barFlash > 40) ? barFlash - 40 : 0;

  display.show();
  delay(80);
}

void animFace() {
  static float angle = 0.0f;

  const float cx = (cfg::SCREEN_W - 1) / 2.0f;
  const float cy = (cfg::SCREEN_H - 1) / 2.0f;

  const float faceR      = min(cfg::SCREEN_W, cfg::SCREEN_H) / 2.0f - 0.5f;
  const float eyeR       = 0.9f;
  const float eyeOffX    = faceR * 0.45f;
  const float eyeOffY    = faceR * 0.35f;
  const float mouthHalfW = faceR * 0.6f;
  const float mouthDepth = faceR * 0.45f;
  const float mouthY     = faceR * 0.15f;
  const float mouthThick = 0.9f;

  // Sample every pixel through the inverse rotation so the whole face spins
  // as one rigid shape instead of the eyes/mouth sliding around inside it.
  const float s = sinf(-angle);
  const float c = cosf(-angle);

  for (int y = 0; y < cfg::SCREEN_H; y++) {
    for (int x = 0; x < cfg::SCREEN_W; x++) {
      float dx = x - cx;
      float dy = y - cy;

      float sx = dx * c - dy * s;
      float sy = dx * s + dy * c;

      CRGB  pixel = CRGB::Black;
      float r     = sqrtf(sx * sx + sy * sy);

      if (r <= faceR) {
        pixel = CRGB(255, 200, 0);

        float leftEye  = sqrtf((sx + eyeOffX) * (sx + eyeOffX) + (sy + eyeOffY) * (sy + eyeOffY));
        float rightEye = sqrtf((sx - eyeOffX) * (sx - eyeOffX) + (sy + eyeOffY) * (sy + eyeOffY));
        if (leftEye < eyeR || rightEye < eyeR) pixel = CRGB::Black;

        if (fabsf(sx) < mouthHalfW) {
          float curve = mouthY + mouthDepth * (1.0f - (sx * sx) / (mouthHalfW * mouthHalfW));
          if (fabsf(sy - curve) < mouthThick) pixel = CRGB::Black;
        }
      }

      display.player(P1)(x, y) = pixel;
      display.player(P2)(x, y) = pixel;
    }
  }

  // Warm glow on the bar, pulsing twice per rotation.
  uint8_t glow = (uint8_t)((sinf(angle * 2.0f) * 0.5f + 0.5f) * 255);
  display.bar().fill(CRGB(glow, glow / 2, 0));

  display.show();

  angle += 0.15f;
  if (angle > TWO_PI) angle -= TWO_PI;
  delay(30);
}

void animRadar() {
  static float sweepAngle = 0.0f;

  const float cx = (cfg::SCREEN_W - 1) / 2.0f;
  const float cy = (cfg::SCREEN_H - 1) / 2.0f;

  const float maxR          = min(cfg::SCREEN_W, cfg::SCREEN_H) / 2.0f - 0.5f;
  const float trailWidth    = 2.2f;            // radians of fading afterglow behind the sweep
  const float ringSpacing   = maxR / 3.0f;
  const float ringThickness = 0.35f;

  // Three fixed contacts; each flashes as the sweep passes over it.
  static const float blipAngle[3]  = { 0.6f, 2.3f, 4.4f };
  static const float blipRadius[3] = { 3.6f, 5.0f, 2.2f };
  float blipFlare[3];
  for (int i = 0; i < 3; i++) {
    float behind = sweepAngle - blipAngle[i];
    while (behind < 0)      behind += 2 * PI;
    while (behind >= 2 * PI) behind -= 2 * PI;
    blipFlare[i] = (behind < 0.5f) ? (1.0f - behind / 0.5f) : 0.0f;
  }

  for (int y = 0; y < cfg::SCREEN_H; y++) {
    for (int x = 0; x < cfg::SCREEN_W; x++) {
      float dx = x - cx;
      float dy = y - cy;
      float r  = sqrtf(dx * dx + dy * dy);

      float brightness = 0.0f;
      CRGB  pixel       = CRGB::Black;

      if (r <= maxR) {
        float phi = atan2f(dy, dx);
        if (phi < 0) phi += 2 * PI;

        // How far behind the sweep's leading edge this pixel sits (0 = just swept).
        float behind = sweepAngle - phi;
        while (behind < 0)      behind += 2 * PI;
        while (behind >= 2 * PI) behind -= 2 * PI;

        if (behind < trailWidth) {
          float t    = 1.0f - behind / trailWidth;
          brightness = t * t;                  // eased, sub-pixel-smooth falloff -> no hard edge
        }

        // Concentric range rings, anti-aliased by distance-to-ring instead of a hard threshold.
        for (int ring = 1; ring <= 3; ring++) {
          float d = fabsf(r - ring * ringSpacing);
          if (d < ringThickness) {
            brightness = max(brightness, (1.0f - d / ringThickness) * 0.25f);
          }
        }

        uint8_t g = (uint8_t)(brightness * 255);
        pixel     = CRGB(0, g, g / 5);
      }

      // Contact flashes rendered as soft (anti-aliased) dots, overlaid on top.
      for (int i = 0; i < 3; i++) {
        if (blipFlare[i] <= 0.0f) continue;
        float bx = cx + blipRadius[i] * cosf(blipAngle[i]);
        float by = cy + blipRadius[i] * sinf(blipAngle[i]);
        float bd = sqrtf((x - bx) * (x - bx) + (y - by) * (y - by));
        if (bd < 1.0f) {
          uint8_t b = (uint8_t)((1.0f - bd) * blipFlare[i] * 255);
          pixel.r   = max(pixel.r, b);
          pixel.g   = max(pixel.g, b);
        }
      }

      display.player(P1)(x, y) = pixel;
      display.player(P2)(x, y) = pixel;
    }
  }

  // Bar doubles as a bearing readout, split across two LEDs for sub-pixel (anti-aliased) position.
  float bearingF = (sweepAngle / (2 * PI)) * cfg::SCREEN_W;
  int   i0       = ((int)bearingF) % cfg::SCREEN_W;
  int   i1       = (i0 + 1) % cfg::SCREEN_W;
  float frac     = bearingF - floorf(bearingF);
  display.bar().clear();
  display.bar()(i0) = CRGB(0, (uint8_t)(255 * (1.0f - frac)), 60);
  display.bar()(i1) = CRGB(0, (uint8_t)(255 * frac), 60);

  display.show();

  sweepAngle += 0.12f;
  if (sweepAngle >= 2 * PI) sweepAngle -= 2 * PI;
  delay(30);
}

// "Bad Apple" homage: a tiny monochrome silhouette flip-book dancer, in the
// spirit of the famous black-and-white shadow-animation demo -- not the
// actual video (no source frames to draw from), just the same aesthetic:
// pure black/white, low-res silhouette, flickering like old film.
void animBadApple() {
  static const uint16_t FRAME_IDLE[12] = {
    0b000001100000, 0b000001100000, 0b000001100000, 0b000011110000,
    0b000101101000, 0b000001100000, 0b000001100000, 0b000001100000,
    0b000001100000, 0b000001100000, 0b000010010000, 0b000010010000,
  };
  static const uint16_t FRAME_PREP[12] = {
    0b000001100000, 0b000001100000, 0b000001100000, 0b000011110000,
    0b001001100100, 0b000001100000, 0b000011110000, 0b000001100000,
    0b000010010000, 0b000100001000, 0b001000000100, 0b001000000100,
  };
  static const uint16_t FRAME_JUMP[12] = {
    0b000001100000, 0b000001100000, 0b000010010000, 0b000100001000,
    0b001001100100, 0b010001100010, 0b000001100000, 0b000001100000,
    0b000010010000, 0b000100001000, 0b001000000100, 0b010000000010,
  };
  static const uint16_t* FRAMES[4] = { FRAME_IDLE, FRAME_PREP, FRAME_JUMP, FRAME_PREP };

  static uint8_t frameIndex = 0;
  static uint8_t holdCount  = 0;
  static uint8_t flicker    = 220;
  static uint8_t barGlow    = 0;

  const uint16_t* rows = FRAMES[frameIndex];

  for (int y = 0; y < cfg::SCREEN_H; y++) {
    for (int x = 0; x < cfg::SCREEN_W; x++) {
      bool on    = (rows[y] >> x) & 1;
      CRGB pixel = on ? CRGB(flicker, flicker, flicker) : CRGB::Black;

      display.player(P1)(x, y)                     = pixel;
      display.player(P2)(cfg::SCREEN_W - 1 - x, y) = pixel;  // mirrored, dancing back
    }
  }

  // The bar strobes on every beat (pose change) and decays through the hold.
  display.bar().fill(CRGB(barGlow, barGlow, barGlow));

  display.show();

  flicker = 200 + (uint8_t)random(56);      // old-film brightness wobble
  barGlow = (barGlow > 40) ? barGlow - 40 : 0;

  if (++holdCount >= 6) {                   // hold each pose ~180ms before advancing
    holdCount  = 0;
    frameIndex = (frameIndex + 1) % 4;
    barGlow    = 255;                       // flash on the beat
  }

  delay(30);
}

// Conway's Game of Life, running as two independent toroidal (wrap-around)
// universes -- one per player screen. Cells hold an "age" instead of a plain
// alive/dead flag so newborns flash white and survivors settle into a
// per-screen color that deepens the longer they live.
static void seedLife(uint8_t (&age)[cfg::SCREEN_H][cfg::SCREEN_W]) {
  for (int y = 0; y < cfg::SCREEN_H; y++)
    for (int x = 0; x < cfg::SCREEN_W; x++)
      age[y][x] = (random(100) < 35) ? 1 : 0;
}

static uint8_t countLifeNeighbors(uint8_t (&age)[cfg::SCREEN_H][cfg::SCREEN_W], int x, int y) {
  uint8_t n = 0;
  for (int dy = -1; dy <= 1; dy++) {
    for (int dx = -1; dx <= 1; dx++) {
      if (dx == 0 && dy == 0) continue;
      int nx = (x + dx + cfg::SCREEN_W) % cfg::SCREEN_W;
      int ny = (y + dy + cfg::SCREEN_H) % cfg::SCREEN_H;
      if (age[ny][nx] > 0) n++;
    }
  }
  return n;
}

void animConwayLife() {
  static uint8_t  age[2][cfg::SCREEN_H][cfg::SCREEN_W];
  static uint32_t history[2][4] = {};
  static uint8_t  staleCount[2] = { 0, 0 };
  static bool     initialized   = false;

  if (!initialized) {
    randomSeed(micros());
    seedLife(age[0]);
    seedLife(age[1]);
    initialized = true;
  }

  uint16_t population[2] = { 0, 0 };

  for (int p = 0; p < 2; p++) {
    uint8_t nextAlive[cfg::SCREEN_H][cfg::SCREEN_W];

    for (int y = 0; y < cfg::SCREEN_H; y++) {
      for (int x = 0; x < cfg::SCREEN_W; x++) {
        uint8_t n     = countLifeNeighbors(age[p], x, y);
        bool    alive = age[p][y][x] > 0;
        nextAlive[y][x] = (alive && (n == 2 || n == 3)) || (!alive && n == 3);
      }
    }

    // Lightweight rolling hash of the new generation -- used only to notice
    // when a board has settled into a still life / short oscillator.
    uint32_t hash = 2166136261u;
    for (int y = 0; y < cfg::SCREEN_H; y++) {
      for (int x = 0; x < cfg::SCREEN_W; x++) {
        if (nextAlive[y][x]) {
          hash = (hash ^ (uint32_t)(y * cfg::SCREEN_W + x + 1)) * 16777619u;
          population[p]++;
        }
      }
    }

    bool repeated = false;
    for (int i = 0; i < 4; i++) if (history[p][i] == hash) repeated = true;
    for (int i = 3; i > 0; i--) history[p][i] = history[p][i - 1];
    history[p][0] = hash;
    staleCount[p] = repeated ? staleCount[p] + 1 : 0;

    if (population[p] == 0 || staleCount[p] > 12) {
      // Extinct, or stuck cycling through the same handful of states -- restart.
      seedLife(age[p]);
      staleCount[p] = 0;
      for (int i = 0; i < 4; i++) history[p][i] = 0;
    } else {
      for (int y = 0; y < cfg::SCREEN_H; y++) {
        for (int x = 0; x < cfg::SCREEN_W; x++) {
          if (nextAlive[y][x]) {
            age[p][y][x] = (age[p][y][x] > 0) ? min((int)age[p][y][x] + 1, 60) : 1;
          } else {
            age[p][y][x] = 0;
          }
        }
      }
    }
  }

  for (int p = 0; p < 2; p++) {
    Player  player  = (p == 0) ? P1 : P2;
    uint8_t baseHue = (p == 0) ? 96 : 200;   // P1: green/cyan colony, P2: pink/purple colony

    for (int y = 0; y < cfg::SCREEN_H; y++) {
      for (int x = 0; x < cfg::SCREEN_W; x++) {
        uint8_t a = age[p][y][x];
        CRGB    pixel;
        if (a == 0)      pixel = CRGB::Black;
        else if (a == 1) pixel = CRGB::White;                          // birth flash
        else             pixel = CHSV(baseHue + min((int)a, 40), 255, 255);

        display.player(player)(x, y) = pixel;
      }
    }
  }

  // Bar: combined population level, drawn as a meter with an anti-aliased partial LED.
  float litF    = (population[0] + population[1]) / (2.0f * cfg::SCREEN_W * cfg::SCREEN_H) * cfg::SCREEN_W;
  int   fullLit = (int)litF;
  float frac    = litF - fullLit;

  display.bar().clear();
  for (int i = 0; i < fullLit && i < cfg::SCREEN_W; i++) {
    display.bar()(i) = CHSV((uint8_t)(96 - i * 6), 255, 255);          // green -> red as it fills
  }
  if (fullLit < cfg::SCREEN_W) {
    display.bar()(fullLit) = CHSV((uint8_t)(96 - fullLit * 6), 255, (uint8_t)(frac * 255));
  }

  display.show();
  delay(150);
}
