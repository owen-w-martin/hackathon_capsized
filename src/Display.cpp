#include "Display.h"

#include "core/Game.h"

// clear() is pure interface (delegates to the regions), so it's implemented.
void Display::clear() {
    _p1.clear();
    _p2.clear();
    _bar.clear();
}

void Display::draw(const core::Game& game) {
    const core::DisplayState& d = game.display();
    for (int y = 0; y < cfg::SCREEN_H; y++) {
        for (int x = 0; x < cfg::SCREEN_W; x++) {
            player(P1)(x, y) = d.p1[y][x];
            player(P2)(x, y) = d.p2[y][x];
        }
    }
    for (int x = 0; x < cfg::SCREEN_W; x++) bar()(x) = d.bar[x];
    show();
}

// ----------------------------------------------------------------------------
//  Physical layer — YOUR implementation goes below.
// ----------------------------------------------------------------------------

void Display::begin() {
    // Register LEDs
    FastLED.addLeds<WS2812B, cfg::DATA_PIN, GRB>(_leds, cfg::NUM_LEDS);
 
    // Configure max power
    FastLED.setBrightness(cfg::BRIGHTNESS);
    FastLED.setMaxPowerInVoltsAndMilliamps(cfg::VOLTS, cfg::MAX_MILLIAMPS);
 
    // Blank screen
    clear();
    show();
}

void Display::show() {
    // TODO:
    //  - blitScreen(_p1, cfg::P1_OFFSET);  blitScreen(_p2, cfg::P2_OFFSET);
    //  - map the bar into _leds starting at cfg::BAR_OFFSET
    //  - FastLED.show();

    for(int i = 0; i < cfg::NUM_LEDS; i++) {
        int col = i / (cfg::SCREEN_H*2+1);
        int row = i - col * (cfg::SCREEN_H*2+1);

        // P1 region
        if (row < cfg::SCREEN_H) {
            _leds[i] = _p1(cfg::SCREEN_W - 1 - col, cfg::SCREEN_H - 1 - row);
        }

        // bar region
        else if (row == cfg::SCREEN_H) { 
            _leds[i] = _bar(col);
        }

        // P2 region
        else {
            _leds[i] = _p2(col, row - (cfg::SCREEN_H + 1));
        }

    }
    FastLED.show();
}