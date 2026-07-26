#pragma once

#include <Arduino.h>
#include <FastLED.h>

// ============================================================================
//  CONFIG  —  set these to match your physical build.
// ============================================================================
namespace cfg {
    constexpr uint8_t DATA_PIN = 25;   // TODO: your Teensy data pin

    constexpr int SCREEN_W = 12;       // TODO: per-player screen width  (pixels)
    constexpr int SCREEN_H = 12;       // TODO: per-player screen height (pixels)

    // The 10x10 board sits centered inside the 12x12 screen, leaving a
    // 1-pixel decorative border ring all the way around.
    constexpr int PLAYING_AREA_W  = 10;
    constexpr int PLAYING_AREA_H  = 10;
    constexpr int PLAYING_AREA_X0 = (SCREEN_W - PLAYING_AREA_W) / 2;
    constexpr int PLAYING_AREA_Y0 = (SCREEN_H - PLAYING_AREA_H) / 2;

    // Region offsets within the ONE physical strip.
    // Assumes the data line runs  P1 -> P2 -> bar; reorder if yours differs.
    //constexpr int P1_OFFSET  = 0;
    //constexpr int P2_OFFSET  = P1_OFFSET + SCREEN_W * SCREEN_H;
    //constexpr int BAR_OFFSET = P2_OFFSET + SCREEN_W * SCREEN_H;
    constexpr int NUM_LEDS   = SCREEN_W * SCREEN_H * 2 + SCREEN_W;

    constexpr uint8_t  BRIGHTNESS    = 50;
    constexpr uint8_t  VOLTS         = 5;
    constexpr uint32_t MAX_MILLIAMPS = 2000;   // TODO: your PSU's safe budget
}

enum Player : uint8_t { P1 = 0, P2 = 1 };

// ============================================================================
//  Screen  —  a rectangular region with its own (x, y) buffer.
//  Templated on size, so it lives in the header.
//
//  The ACCESS INTERFACE below is fully implemented: it works purely in logical
//  row-major coordinates and never touches the physical wiring. The physical
//  mapping (mapLocal) is the only stub left for you.
// ============================================================================
template <int W, int H>
class Screen {
public:
    // --- array-like access. All return real references, so every CRGB
    //     operator (+=, -=, %=, .fadeToBlackBy(), ...) works directly. ---
    CRGB&       operator()(int x, int y)       { return _buf[y * W + x]; }  // screen(x, y)
    const CRGB& operator()(int x, int y) const { return _buf[y * W + x]; }
    CRGB*       operator[](int y)              { return &_buf[y * W]; }      // screen[y][x]
    const CRGB* operator[](int y) const        { return &_buf[y * W]; }

    // --- bounds-checked door for untrusted coordinates (sprite pos, input, ...) ---
    void setPixel(int x, int y, CRGB c) {
        if ((unsigned)x < (unsigned)W && (unsigned)y < (unsigned)H)
            _buf[y * W + x] = c;
    }
    CRGB getPixel(int x, int y) const {
        if ((unsigned)x < (unsigned)W && (unsigned)y < (unsigned)H)
            return _buf[y * W + x];
        return CRGB::Black;
    }

    // --- bulk helpers ---
    void        fill(CRGB c) { for (auto& p : _buf) p = c; }
    void        clear()      { fill(CRGB::Black); }
    CRGB*       raw()        { return _buf; }          // hand to any FastLED function
    const CRGB* raw() const  { return _buf; }

    static constexpr int width()  { return W; }
    static constexpr int height() { return H; }
    static constexpr int count()  { return W * H; }

private:
    CRGB _buf[W * H] = {};   // logical, row-major
};

// ============================================================================
//  LightBar  —  the 1D diffused strip across the top.
// ============================================================================
class LightBar {
public:
    CRGB&       operator[](int i)       { return _buf[i]; }
    const CRGB& operator[](int i) const { return _buf[i]; }
    CRGB&       operator()(int i)       { return _buf[i]; }
    const CRGB& operator()(int i) const { return _buf[i]; }

    void        set(int i, CRGB c) { if ((unsigned)i < (unsigned)cfg::SCREEN_W) _buf[i] = c; }
    void        fill(CRGB c)       { for (auto& p : _buf) p = c; }
    void        clear()            { fill(CRGB::Black); }
    CRGB*       raw()              { return _buf; }
    static constexpr int count()   { return cfg::SCREEN_W; }

private:
    CRGB _buf[cfg::SCREEN_W] = {};
};

using PlayerScreen = Screen<cfg::SCREEN_W, cfg::SCREEN_H>;

// ============================================================================
//  Display  —  owns the single physical strip and composes all regions onto it.
//  The accessors (player / operator[] / bar) are implemented; the physical
//  composition (begin / blitScreen / show) is left for you.
// ============================================================================
class Display {
public:
    void begin();      // call once in setup()  -- TODO
    void clear();      // blank all regions
    void show();       // map every region into the strip, then output  -- TODO

    // Parameterized, array-like access: pick a player, then index the screen.
    //   display.player(P1)(x, y) = CRGB::Red;
    //   display[P1][y][x]       += CRGB(5, 5, 5);
    PlayerScreen&       player(Player p)       { return (p == P1) ? _p1 : _p2; }
    const PlayerScreen& player(Player p) const { return (p == P1) ? _p1 : _p2; }
    PlayerScreen& operator[](Player p) { return player(p); }
    LightBar&       bar()       { return _bar; }
    const LightBar& bar() const { return _bar; }

    void setBrightness(uint8_t b)     { FastLED.setBrightness(b); }
    void setMaxMilliamps(uint32_t ma) { FastLED.setMaxPowerInVoltsAndMilliamps(cfg::VOLTS, ma); }

private:
    CRGB         _leds[cfg::NUM_LEDS];   // the ONE physical strip FastLED reads
    PlayerScreen _p1, _p2;
    LightBar     _bar;

    //void blitScreen(PlayerScreen& s, int offset);   // copy one screen into its block  -- TODO
};
