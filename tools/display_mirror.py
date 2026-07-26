#!/usr/bin/env python3
"""Mirrors the Teensy's LED display and simulates each player's two rotary
encoders (left/right, up/down) and button over its real USB serial port --
for when the physical display and/or controls aren't attached to the board.

Talks the same line protocol the Teensy speaks over Serial:
  in:  BTN1/BTN2 <0|1>, ENC1X/ENC1Y/ENC2X/ENC2Y <delta>
  out: S <144x 6-hex P1 pixels> <144x 6-hex P2 pixels> <12x 6-hex bar pixels>

Usage:
  python3 tools/display_mirror.py --port /dev/ttyACM0 --baud 115200

Controls:
  Arrow keys -- P1 encoders (left/right, up/down)    Space     -- P1 button
  WASD       -- P2 encoders (left/right, up/down)    Left Shift -- P2 button
"""

import argparse
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


def reader_thread(source, state_queue, stop_event):
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


def blank_frame():
    return {
        "p1": [(0, 0, 0)] * (BOARD_W * BOARD_H),
        "p2": [(0, 0, 0)] * (BOARD_W * BOARD_H),
        "bar": [(0, 0, 0)] * BOARD_W,
    }


def draw_grid(surface, origin, pixels):
    ox, oy = origin
    for y in range(BOARD_H):
        for x in range(BOARD_W):
            color = pixels[y * BOARD_W + x]
            # y=0 is the bottom of the screen (see Display::show()), so flip
            # the row when drawing to match the physical board.
            row = BOARD_H - 1 - y
            rect = (ox + x * CELL, oy + row * CELL, CELL - 1, CELL - 1)
            pygame.draw.rect(surface, color, rect)


def draw_bar(surface, origin, width, pixels):
    ox, oy = origin
    seg_w = width / BOARD_W
    for i, color in enumerate(pixels):
        rect = (ox + i * seg_w, oy, seg_w - 1, BAR_H - 1)
        pygame.draw.rect(surface, color, rect)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--port", required=True, help="serial port of the Teensy (e.g. /dev/ttyACM0)")
    parser.add_argument("--baud", type=int, default=115200)
    args = parser.parse_args()

    source = SerialSource(args.port, args.baud)

    state_queue = queue.Queue(maxsize=1)
    stop_event = threading.Event()
    thread = threading.Thread(target=reader_thread, args=(source, state_queue, stop_event), daemon=True)
    thread.start()

    grid_w = BOARD_W * CELL
    grid_h = BOARD_H * CELL
    legend_h = 22
    row_gap = 16
    width = MARGIN * 2 + grid_w * 2 + GAP
    height = MARGIN + grid_h + row_gap + BAR_H + row_gap + legend_h + MARGIN

    pygame.init()
    pygame.key.set_repeat(250, 60)
    screen = pygame.display.set_mode((width, height))
    pygame.display.set_caption("Battleship rig mirror")
    font = pygame.font.SysFont(None, 22)
    clock = pygame.time.Clock()

    p1_origin = (MARGIN, MARGIN)
    p2_origin = (MARGIN + grid_w + GAP, MARGIN)
    bar_origin = (MARGIN, MARGIN + grid_h + row_gap)
    legend_y = MARGIN + grid_h + row_gap + BAR_H + row_gap

    frame = blank_frame()
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
                elif event.type == pygame.KEYUP:
                    if event.key == pygame.K_SPACE:
                        source.write_line("BTN1 0")
                    elif event.key == pygame.K_LSHIFT:
                        source.write_line("BTN2 0")

            try:
                frame = state_queue.get_nowait()
            except queue.Empty:
                pass

            screen.fill((15, 15, 15))
            draw_grid(screen, p1_origin, frame["p1"])
            draw_grid(screen, p2_origin, frame["p2"])
            draw_bar(screen, bar_origin, grid_w * 2 + GAP, frame["bar"])

            legend = font.render(
                "Arrows/Space: P1    WASD/LShift: P2    Esc: quit",
                True, (200, 200, 200),
            )
            screen.blit(legend, (MARGIN, legend_y))

            pygame.display.flip()
            clock.tick(60)
    finally:
        stop_event.set()
        source.close()
        pygame.quit()

    return 0


if __name__ == "__main__":
    sys.exit(main())
