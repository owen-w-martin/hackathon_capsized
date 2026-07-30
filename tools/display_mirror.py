#!/usr/bin/env python3
"""Mirrors the Teensy's LED display and simulates each player's two rotary
encoders (left/right, up/down), button, and hardware debug commands over its
real USB serial port -- for when the physical display and/or controls aren't
attached to the board.

Talks the same line protocol the Teensy speaks over Serial:
  in:  BTN1/BTN2 <0|1>, ENC1X/ENC1Y/ENC2X/ENC2Y <delta>,
       SCANCELL1/2, SELROWXY1/2, SELCOLXY1/2, SELCELLXY1/2,
       HOLDCELL1/2 <x> <y>, UNHOLDCELL1/2,
       SELROW1/2, SELCOL1/2, SELCELL1/2 <args...>
  out: S <144x 6-hex P1 pixels> <144x 6-hex P2 pixels> <12x 6-hex bar pixels>

Usage:
  python3 tools/display_mirror.py --port /dev/ttyACM0 --baud 115200

Controls:
  Arrow keys -- P1 encoders (left/right, up/down)    Space      -- P1 button
  WASD       -- P2 encoders (left/right, up/down)    Left Shift -- P2 button

  Debug test coordinate (A, B), clamped to the board size:
    K / ;  -- A -1 / +1        L / O  -- B -1 / +1

  Debug fire keys -- unmodified = P1, held Right Shift = P2 (Left Shift is
  already P2's button, so it's kept out of this to avoid firing both):
    1 SCANCELL   (A, B) = (x, y)      5 HOLDCELL  hold to scan (A, B) live
    2 SELROWXY   (A, B) = (x, y)      6 SELROW    A = row
    3 SELCOLXY   (A, B) = (x, y)      7 SELCOL    A = col
    4 SELCELLXY  (A, B) = (x, y)      8 SELCELL   (A, B) = (row, col)
"""

import argparse
import collections
import colorsys
import queue
import sys
import threading

import pygame

BOARD_W = 12
BOARD_H = 12
CELL = 32
GAP = 24
MARGIN = 20
BAR_H = 40
LOG_W = 360
LOG_MAX_LINES = 200   # scrollback kept; only what fits the panel is drawn

# The addressable board is 10x10 (cfg::BOARD_SIZE in Hardware.h) -- distinct
# from BOARD_W/BOARD_H above, which include the LED screen's 1-pixel border.
DEBUG_BOARD_SIZE = 10
# Playing-area inset within the screen (cfg::PLAYING_AREA_X0/Y0 in Display.h).
PLAYING_AREA_OFFSET = (BOARD_W - DEBUG_BOARD_SIZE) // 2

# Fire-key -> serial command prefix (player suffix 1/2 appended at send time).
DEBUG_FIRE_KEYS = {
    pygame.K_1: "SCANCELL",
    pygame.K_2: "SELROWXY",
    pygame.K_3: "SELCOLXY",
    pygame.K_4: "SELCELLXY",
    # K_5 is HOLDCELL/UNHOLDCELL, handled separately (see main()) since it
    # needs distinct keydown/keyup behavior, not a single fire-and-forget.
    pygame.K_6: "SELROW",
    pygame.K_7: "SELCOL",
    pygame.K_8: "SELCELL",
}
HOLD_KEY = pygame.K_5


def predict_target(cmd, test_a, test_b, n=DEBUG_BOARD_SIZE):
    """Best-effort guess at what a debug command aims at, in game (x, y)
    playing-area terms -- ("cell", x, y), ("row", y), or ("col", x). None
    if the command has no meaningful game-grid position to show.

    SCANCELL/SELCELLXY/SELROWXY/SELCOLXY go through Hardware::selectCell's
    rotation starting from game (x, y), so the game-facing target is just
    (A, B) directly. SELROW/SELCOL/SELCELL intentionally bypass that
    rotation and drive the real schematic/demux row and col directly (see
    Board::debugSelectRow/Col/Cell -> Hardware::selectRawRow/selectRawCol)
    -- there's no sensible game-grid cell to light up for a raw address,
    so these just don't get an overlay.
    """
    if cmd in ("SCANCELL", "SELCELLXY"):
        return ("cell", test_a, test_b)
    if cmd == "SELROWXY":
        return ("row", test_b)
    if cmd == "SELCOLXY":
        return ("col", test_a)
    return None


def fire_debug(source, key, player_suffix, test_a, test_b):
    """Sends the debug command bound to `key` for the given player suffix
    ("1" or "2"), using the current (test_a, test_b) coordinate. Returns
    (player_suffix, predicted_target) on success (see predict_target()),
    or None if `key` wasn't a bound debug fire key."""
    cmd = DEBUG_FIRE_KEYS.get(key)
    if cmd is None:
        return None
    if cmd == "SELROW":
        source.write_line(f"SELROW{player_suffix} {test_a}")
    elif cmd == "SELCOL":
        source.write_line(f"SELCOL{player_suffix} {test_a}")
    else:
        source.write_line(f"{cmd}{player_suffix} {test_a} {test_b}")
    return (player_suffix, predict_target(cmd, test_a, test_b))


class SerialSource:
    def __init__(self, port, baud):
        import serial
        self.ser = serial.Serial(port, baud, timeout=1)

    def readline(self):
        return self.ser.readline().decode(errors="replace")

    def write_line(self, line):
        self.ser.write((line + "\n").encode())

    def close(self):
        self.ser.close()


def parse_state(line):
    # "S <p1hex> <p2hex> <barhex>"
    _, p1_hex, p2_hex, bar_hex = line.split(" ", 3)

    def chunk(hexstr):
        return [
            tuple(int(hexstr[i + j:i + j + 2], 16) for j in (0, 2, 4))
            for i in range(0, len(hexstr), 6)
        ]

    return {"p1": chunk(p1_hex), "p2": chunk(p2_hex), "bar": chunk(bar_hex.strip())}


def reader_thread(source, state_queue, log_queue, stop_event):
    while not stop_event.is_set():
        line = source.readline()
        if not line:
            continue
        line = line.strip()
        if not line:
            continue
        if line.startswith("S "):
            try:
                frame = parse_state(line)
            except ValueError:
                continue
            try:
                state_queue.get_nowait()
            except queue.Empty:
                pass
            state_queue.put(frame)
        else:
            print(line)
            log_queue.put(line)


def blank_frame():
    return {
        "p1": [(0, 0, 0)] * (BOARD_W * BOARD_H),
        "p2": [(0, 0, 0)] * (BOARD_W * BOARD_H),
        "bar": [(0, 0, 0)] * BOARD_W,
    }


def boost_color(rgb):
    """WS2812 LEDs read as bright and saturated even at low PWM duty
    cycles the firmware deliberately dims things to (e.g. CRGB(5, 10, 2)),
    but a monitor renders that same RGB triple as nearly black -- this is
    a simulator, so legibility beats color accuracy. Pure black stays
    black; everything else gets a big, brightness-dependent lift (a
    fractional-power curve pulls dim values up hard while barely
    touching already-bright ones) plus a flat saturation boost."""
    r, g, b = rgb
    if r == 0 and g == 0 and b == 0:
        return (0, 0, 0)
    h, s, v = colorsys.rgb_to_hsv(r / 255.0, g / 255.0, b / 255.0)
    s = min(1.0, s * 1.6)
    v = min(1.0, v ** 0.35)
    r2, g2, b2 = colorsys.hsv_to_rgb(h, s, v)
    return (round(r2 * 255), round(g2 * 255), round(b2 * 255))


def panel_pos(player, x, y):
    """player is "p1" or "p2". Returns (col, row) in the same
    0..BOARD_W-1/0..BOARD_H-1 space draw_grid draws in.

    Display::show()'s bare indexing arithmetic suggested P1 and P2 needed
    different transforms (asymmetric mirroring, as if the two screens
    were mirror images of each other), and seemed to check out against
    the bar-column-flip comment in Game.cpp too -- but that reasoning
    didn't survive contact with actually looking at the thing: on-screen
    testing (first "both axes wrong" on P2, then narrowed down to "X is
    flipped" after fixing the row) converged on P1 and P2 using the exact
    same transform, no cross-player mirroring at all. Trusting that over
    the code-derived theory, since Display::show()'s arithmetic clearly
    isn't literally describing one coherent physical panel the way that
    derivation assumed.
    """
    return x, (BOARD_H - 1) - y


def draw_grid(surface, origin, pixels, player):
    ox, oy = origin
    for y in range(BOARD_H):
        for x in range(BOARD_W):
            color = boost_color(pixels[y * BOARD_W + x])
            col, row = panel_pos(player, x, y)
            rect = (ox + col * CELL, oy + row * CELL, CELL - 1, CELL - 1)
            pygame.draw.rect(surface, color, rect)


def draw_bar(surface, origin, width, pixels):
    """Display::show() maps the bar with the exact same "SCREEN_W-1-col"
    mirror it uses for P2's grid (leds[i] = _bar(SCREEN_W-1-col)), so this
    must mirror it the same way -- not draw buffer index i straight across."""
    ox, oy = origin
    seg_w = width / BOARD_W
    for i, color in enumerate(pixels):
        col = (BOARD_W - 1) - i
        rect = (ox + col * seg_w, oy, seg_w - 1, BAR_H - 1)
        pygame.draw.rect(surface, boost_color(color), rect)


def debug_cell_rect(origin, player, x, y):
    """Pixel rect for playing-area (x, y) (0..DEBUG_BOARD_SIZE-1) on the
    grid drawn at `origin` for `player` -- offset by PLAYING_AREA_OFFSET
    and run through the same panel_pos() transform draw_grid uses, so it
    lines up with the actual grid cell."""
    ox, oy = origin
    col, row = panel_pos(player, x + PLAYING_AREA_OFFSET, y + PLAYING_AREA_OFFSET)
    return (ox + col * CELL, oy + row * CELL, CELL - 1, CELL - 1)


def draw_debug_target(surface, origin, player, target):
    """Draws a best-effort marker for the last-fired debug command's
    target (see predict_target()): a single outlined cell, or a full
    highlighted row/column when the command only pins down one axis."""
    color = (255, 200, 0)
    kind = target[0]
    if kind == "cell":
        _, x, y = target
        pygame.draw.rect(surface, color, debug_cell_rect(origin, player, x, y), 3)
    elif kind == "row":
        _, y = target
        for x in range(DEBUG_BOARD_SIZE):
            pygame.draw.rect(surface, color, debug_cell_rect(origin, player, x, y), 1)
    elif kind == "col":
        _, x = target
        for y in range(DEBUG_BOARD_SIZE):
            pygame.draw.rect(surface, color, debug_cell_rect(origin, player, x, y), 1)


def draw_debug_selector(surface, origin, player, x, y, active):
    """Live outline at the current (test_a, test_b) debug coordinate, on
    every frame regardless of whether anything's been fired yet -- dimmer
    than draw_debug_target's fired-command marker, so it reads as "what
    would be targeted" rather than "what was targeted." `active` (whether
    Right Shift is currently held, i.e. whether a fire key would target
    this player right now) draws brighter and thicker than the other
    player's, so it's clear which one a keypress would actually hit."""
    color = (200, 165, 0) if active else (90, 75, 0)
    width = 3 if active else 2
    pygame.draw.rect(surface, color, debug_cell_rect(origin, player, x, y), width)


def _truncate_to_width(font, text, max_width):
    if font.size(text)[0] <= max_width:
        return text
    ellipsis = "..."
    while text and font.size(text + ellipsis)[0] > max_width:
        text = text[:-1]
    return (text + ellipsis) if text else ellipsis


def draw_log_panel(surface, origin, size, font, lines):
    """Everything the Teensy sends over serial that isn't a state frame
    (scanCell/beginScan/debugSelect*/pop-charge status prints, ERR
    replies, ...) -- most recent at the bottom, like a terminal. Lines
    too wide for the panel are truncated with an ellipsis rather than
    left to overflow past its edge."""
    ox, oy = origin
    w, h = size
    pygame.draw.rect(surface, (25, 25, 25), (ox, oy, w, h))
    pygame.draw.rect(surface, (80, 80, 80), (ox, oy, w, h), 1)

    title = font.render("Debug output", True, (150, 150, 150))
    surface.blit(title, (ox + 4, oy + 2))

    line_h = font.get_linesize()
    content_top = oy + 2 + line_h + 2
    max_lines = max(0, (oy + h - content_top) // line_h)
    for i, line in enumerate(list(lines)[-max_lines:]):
        line = _truncate_to_width(font, line, w - 8)
        text = font.render(line, True, (80, 220, 80))
        surface.blit(text, (ox + 4, content_top + i * line_h))


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--port", required=True, help="serial port of the Teensy (e.g. /dev/ttyACM0)")
    parser.add_argument("--baud", type=int, default=115200)
    args = parser.parse_args()

    source = SerialSource(args.port, args.baud)

    state_queue = queue.Queue(maxsize=1)
    log_queue = queue.Queue()
    stop_event = threading.Event()
    thread = threading.Thread(
        target=reader_thread, args=(source, state_queue, log_queue, stop_event), daemon=True
    )
    thread.start()

    grid_w = BOARD_W * CELL
    grid_h = BOARD_H * CELL
    legend_line_h = 22
    legend_h = legend_line_h * 5
    row_gap = 16
    # Some extra width beyond the two grids and the log panel -- text-width
    # estimates don't always match what actually renders (font
    # substitution, DPI scaling), so leave real headroom rather than
    # sizing exactly to the content.
    width = MARGIN * 3 + grid_w * 2 + GAP + LOG_W + 80
    height = MARGIN + grid_h + row_gap + BAR_H + row_gap + legend_h + MARGIN

    pygame.init()
    pygame.key.set_repeat(250, 60)
    screen = pygame.display.set_mode((width, height))
    pygame.display.set_caption("Battleship rig mirror")
    font = pygame.font.SysFont(None, 22)
    log_font = pygame.font.SysFont("monospace", 14)
    clock = pygame.time.Clock()
    log_lines = collections.deque(maxlen=LOG_MAX_LINES)

    p1_origin = (MARGIN, MARGIN)
    p2_origin = (MARGIN + grid_w + GAP, MARGIN)
    bar_origin = (MARGIN, MARGIN + grid_h + row_gap)
    legend_y = MARGIN + grid_h + row_gap + BAR_H + row_gap
    log_origin = (MARGIN + grid_w + GAP + grid_w + GAP, MARGIN)
    log_size = (LOG_W, grid_h + row_gap + BAR_H)

    frame = blank_frame()
    test_a, test_b = 0, 0
    debug_target = None   # (player_suffix, predict_target() result) of the last-fired debug command
    holding = None         # player_suffix currently HOLDCELL-ing, or None
    running = True
    try:
        while running:
            for event in pygame.event.get():
                if event.type == pygame.QUIT:
                    running = False
                elif event.type == pygame.KEYDOWN:
                    if event.key == pygame.K_ESCAPE:
                        running = False
                    elif event.key == pygame.K_LEFT:
                        source.write_line("ENC1X -1")
                    elif event.key == pygame.K_RIGHT:
                        source.write_line("ENC1X 1")
                    elif event.key == pygame.K_UP:
                        source.write_line("ENC1Y -1")
                    elif event.key == pygame.K_DOWN:
                        source.write_line("ENC1Y 1")
                    elif event.key == pygame.K_a:
                        source.write_line("ENC2X -1")
                    elif event.key == pygame.K_d:
                        source.write_line("ENC2X 1")
                    elif event.key == pygame.K_w:
                        source.write_line("ENC2Y -1")
                    elif event.key == pygame.K_s:
                        source.write_line("ENC2Y 1")
                    elif event.key == pygame.K_SPACE:
                        source.write_line("BTN1 1")
                    elif event.key == pygame.K_LSHIFT:
                        source.write_line("BTN2 1")
                    elif event.key == pygame.K_k:
                        test_a = max(0, test_a - 1)
                    elif event.key == pygame.K_SEMICOLON:
                        test_a = min(DEBUG_BOARD_SIZE - 1, test_a + 1)
                    elif event.key == pygame.K_l:
                        test_b = max(0, test_b - 1)
                    elif event.key == pygame.K_o:
                        test_b = min(DEBUG_BOARD_SIZE - 1, test_b + 1)
                    elif event.key == HOLD_KEY:
                        if holding is None:
                            player_suffix = "2" if (event.mod & pygame.KMOD_RSHIFT) else "1"
                            source.write_line(f"HOLDCELL{player_suffix} {test_a} {test_b}")
                            holding = player_suffix
                            debug_target = (player_suffix, ("cell", test_a, test_b))
                    elif event.key in DEBUG_FIRE_KEYS:
                        player_suffix = "2" if (event.mod & pygame.KMOD_RSHIFT) else "1"
                        result = fire_debug(source, event.key, player_suffix, test_a, test_b)
                        if result is not None:
                            debug_target = result
                elif event.type == pygame.KEYUP:
                    if event.key == pygame.K_SPACE:
                        source.write_line("BTN1 0")
                    elif event.key == pygame.K_LSHIFT:
                        source.write_line("BTN2 0")
                    elif event.key == HOLD_KEY and holding is not None:
                        source.write_line(f"UNHOLDCELL{holding}")
                        holding = None

            try:
                frame = state_queue.get_nowait()
            except queue.Empty:
                pass

            while True:
                try:
                    log_lines.append(log_queue.get_nowait())
                except queue.Empty:
                    break

            screen.fill((15, 15, 15))
            draw_grid(screen, p1_origin, frame["p1"], "p1")
            draw_grid(screen, p2_origin, frame["p2"], "p2")
            draw_bar(screen, bar_origin, grid_w * 2 + GAP, frame["bar"])
            draw_log_panel(screen, log_origin, log_size, log_font, log_lines)

            p2_active = bool(pygame.key.get_mods() & pygame.KMOD_RSHIFT)
            draw_debug_selector(screen, p1_origin, "p1", test_a, test_b, not p2_active)
            draw_debug_selector(screen, p2_origin, "p2", test_a, test_b, p2_active)

            if debug_target is not None and debug_target[1] is not None:
                target_suffix, target = debug_target
                target_origin = p1_origin if target_suffix == "1" else p2_origin
                target_player = "p1" if target_suffix == "1" else "p2"
                draw_debug_target(screen, target_origin, target_player, target)

            legend_lines = [
                "Arrows/Space: P1    WASD/LShift: P2    Esc: quit",
                f"Debug test cell: A={test_a} B={test_b}   K/; = A-1/+1   L/O = B-1/+1",
                "1 SCANCELL  2 SELROWXY  3 SELCOLXY  4 SELCELLXY",
                "5 HOLDCELL (hold key)  6 SELROW  7 SELCOL  8 SELCELL  (+RShift = P2)",
                "Dim yellow outline = current (A, B) selector; bright yellow = last fired target",
            ]
            for i, line in enumerate(legend_lines):
                text = font.render(line, True, (200, 200, 200))
                screen.blit(text, (MARGIN, legend_y + i * legend_line_h))

            pygame.display.flip()
            clock.tick(60)
    finally:
        stop_event.set()
        source.close()
        pygame.quit()

    return 0


if __name__ == "__main__":
    sys.exit(main())
