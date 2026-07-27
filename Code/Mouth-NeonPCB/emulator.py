#!/usr/bin/env python3
"""Desktop emulator for the SarcasmOS 64x32 HUB75 mouth."""

from __future__ import annotations

import argparse
import hashlib
import math
import pathlib
import sys
import time
from dataclasses import dataclass

WIDTH = 64
HEIGHT = 32
FRAME_MS = 40

BLACK = (0, 0, 0)
TEETH = (255, 246, 166)
DIM_TEETH = (116, 112, 76)

# This order intentionally matches protocol.hpp IDs 0x00 through 0x1e.
STATES = (
    "idle",
    "listening",
    "thinking",
    "thinking_audio",
    "thinking_long",
    "speaking",
    "happy_fake",
    "angry",
    "error",
    "asleep",
    "tool",
    "left",
    "right",
    "up",
    "down",
    "center",
    "neutral",
    "sarcastic",
    "suspicious",
    "tired",
    "surprised",
    "bored",
    "dramatic",
    "watch",
    "party",
    "battery_low",
    "sunny",
    "rainy",
    "cloudy",
    "stormy",
    "snowy",
)
STATE_IDS = {name: state_id for state_id, name in enumerate(STATES)}


@dataclass
class FrameBuffer:
    pixels: list[tuple[int, int, int]]

    @classmethod
    def create(cls) -> "FrameBuffer":
        return cls([BLACK] * (WIDTH * HEIGHT))

    def clear(self) -> None:
        self.pixels[:] = [BLACK] * len(self.pixels)

    def pixel(self, x: int, y: int, color: tuple[int, int, int]) -> None:
        if 0 <= x < WIDTH and 0 <= y < HEIGHT:
            self.pixels[y * WIDTH + x] = color

    def fill_rect(
        self, x: int, y: int, width: int, height: int,
        color: tuple[int, int, int],
    ) -> None:
        for py in range(max(y, 0), min(y + height, HEIGHT)):
            for px in range(max(x, 0), min(x + width, WIDTH)):
                self.pixel(px, py, color)

    def hline(
        self, x: int, y: int, width: int, color: tuple[int, int, int],
    ) -> None:
        self.fill_rect(x, y, width, 1, color)

    def vline(
        self, x: int, y: int, height: int, color: tuple[int, int, int],
    ) -> None:
        self.fill_rect(x, y, 1, height, color)

    def line(
        self, x0: int, y0: int, x1: int, y1: int,
        color: tuple[int, int, int],
    ) -> None:
        dx = abs(x1 - x0)
        sx = 1 if x0 < x1 else -1
        dy = -abs(y1 - y0)
        sy = 1 if y0 < y1 else -1
        error = dx + dy
        while True:
            self.pixel(x0, y0, color)
            if x0 == x1 and y0 == y1:
                return
            twice_error = 2 * error
            if twice_error >= dy:
                error += dy
                x0 += sx
            if twice_error <= dx:
                error += dx
                y0 += sy

    def thick_line(
        self, x0: int, y0: int, x1: int, y1: int,
        color: tuple[int, int, int],
    ) -> None:
        self.line(x0, y0, x1, y1, color)
        self.line(x0, y0 + 1, x1, y1 + 1, color)

    def fill_circle(
        self, cx: int, cy: int, radius: int,
        color: tuple[int, int, int],
    ) -> None:
        radius_squared = radius * radius
        for y in range(cy - radius, cy + radius + 1):
            for x in range(cx - radius, cx + radius + 1):
                if (x - cx) ** 2 + (y - cy) ** 2 <= radius_squared:
                    self.pixel(x, y, color)

    def fill_round_rect(
        self, x: int, y: int, width: int, height: int, radius: int,
        color: tuple[int, int, int],
    ) -> None:
        radius = max(0, min(radius, width // 2, height // 2))
        if radius == 0:
            self.fill_rect(x, y, width, height, color)
            return
        left_center = x + radius
        right_center = x + width - radius - 1
        top_center = y + radius
        bottom_center = y + height - radius - 1
        radius_squared = radius * radius
        for py in range(y, y + height):
            for px in range(x, x + width):
                nearest_x = min(max(px, left_center), right_center)
                nearest_y = min(max(py, top_center), bottom_center)
                if ((px - nearest_x) ** 2 +
                        (py - nearest_y) ** 2 <= radius_squared):
                    self.pixel(px, py, color)

    def ppm(self, brightness: int = 255) -> bytes:
        factor = math.sqrt(max(0, min(brightness, 255)) / 255)
        data = bytearray()
        for red, green, blue in self.pixels:
            data.extend((
                round(red * factor),
                round(green * factor),
                round(blue * factor),
            ))
        return f"P6\n{WIDTH} {HEIGHT}\n255\n".encode() + bytes(data)


class MouthRenderer:
    def __init__(self) -> None:
        self.frame = FrameBuffer.create()

    def shell(
        self, fill: tuple[int, int, int], x: int = 1, y: int = 1,
        width: int = 62, height: int = 30, radius: int = 14,
    ) -> None:
        self.frame.fill_round_rect(x, y, width, height, radius, BLACK)
        self.frame.fill_round_rect(
            x + 2, y + 2, width - 4, height - 4, max(radius - 2, 1), fill,
        )

    def column_seams(self, y: int = 3, height: int = 26) -> None:
        for x in (15, 27, 39, 51):
            self.frame.vline(x, y, height, BLACK)
            self.frame.vline(x + 1, y, height, BLACK)

    def resting(self, fill: tuple[int, int, int] = TEETH) -> None:
        self.shell(fill)
        self.column_seams()
        for y in (11, 12, 20, 21):
            self.frame.hline(3, y, 58, BLACK)

    def speaking(self, tick: int, intensity: int) -> None:
        self.shell(TEETH)
        self.column_seams()
        phase = tick % 24
        triangle = phase if phase <= 12 else 24 - phase
        amplitude = 2 + triangle * 6 // 12
        amplitude = max(1, min(amplitude * intensity // 120, 9))
        center = 16
        self.frame.thick_line(3, center - 1, 15, center - 1, BLACK)
        self.frame.thick_line(3, center + 1, 15, center + 1, BLACK)
        self.frame.thick_line(15, center - 1, 32, center - amplitude, BLACK)
        self.frame.thick_line(32, center - amplitude, 49, center - 1, BLACK)
        self.frame.thick_line(49, center - 1, 60, center - 1, BLACK)
        self.frame.thick_line(15, center + 1, 32, center + amplitude, BLACK)
        self.frame.thick_line(32, center + amplitude, 49, center + 1, BLACK)
        self.frame.thick_line(49, center + 1, 60, center + 1, BLACK)

    def thinking(self, tick: int, variant: int) -> None:
        self.shell(TEETH)
        self.column_seams()
        self.frame.hline(3, 10, 58, BLACK)
        self.frame.hline(3, 22, 58, BLACK)
        cadence = 28 if variant == 2 else (12 if variant == 1 else 18)
        step = tick // cadence % 3
        for index, x in enumerate((24, 32, 40)):
            radius = (3 if variant == 1 else 2) if index == step else 1
            self.frame.fill_circle(x, 16, radius, BLACK)

    def tool(self, tick: int) -> None:
        self.shell(TEETH)
        self.column_seams()
        self.frame.thick_line(5, 13, 59, 13, BLACK)
        self.frame.thick_line(5, 18, 59, 18, BLACK)
        self.frame.fill_rect(8 + tick // 2 % 48, 15, 3, 2, BLACK)

    def listening(self, tick: int) -> None:
        self.resting()
        self.frame.fill_circle(8 + tick // 2 % 48, 16, 1, (110, 88, 25))

    def happy(self, tick: int, party: bool) -> None:
        self.shell(TEETH)
        self.column_seams()
        for segment in (
            (4, 11, 16, 14), (16, 14, 32, 18),
            (32, 18, 48, 14), (48, 14, 60, 11),
        ):
            self.frame.thick_line(*segment, BLACK)
        if party:
            phase = tick // 3 % 18
            self.frame.fill_rect(9 + phase, 6, 2, 2, (255, 45, 90))
            self.frame.fill_rect(53 - phase, 23, 2, 2, (30, 170, 255))

    def angry(self) -> None:
        self.shell(TEETH)
        self.column_seams()
        for segment in (
            (3, 8, 32, 14), (32, 14, 60, 8),
            (3, 23, 32, 18), (32, 18, 60, 23),
        ):
            self.frame.thick_line(*segment, BLACK)

    def sarcastic(self) -> None:
        self.shell(TEETH)
        self.column_seams()
        for segment in ((3, 19, 19, 18), (19, 18, 35, 15),
                        (35, 15, 60, 11)):
            self.frame.thick_line(*segment, BLACK)

    def suspicious(self) -> None:
        self.shell(TEETH)
        self.column_seams()
        for segment in ((3, 14, 25, 14), (25, 14, 32, 18),
                        (32, 18, 39, 14), (39, 14, 60, 14)):
            self.frame.thick_line(*segment, BLACK)

    def tired(self, tick: int) -> None:
        self.shell(DIM_TEETH, 3, 7, 58, 20, 9)
        droop = 2 + tick // 40 % 2
        for segment in ((8, 15, 24, 15 + droop),
                        (24, 15 + droop, 40, 15 + droop),
                        (40, 15 + droop, 56, 15)):
            self.frame.thick_line(*segment, BLACK)

    def surprised(self, tick: int, dramatic: bool) -> None:
        pulse = 2 + tick // 12 % 3 if dramatic else 0
        width = (24 if dramatic else 18) + pulse
        height = (30 if dramatic else 22) + pulse // 2
        x = (WIDTH - width) // 2
        y = (HEIGHT - height) // 2
        self.shell(TEETH, x, y, width, height, min(width, height) // 2)
        self.frame.fill_round_rect(
            x + 6, y + 6, width - 12, height - 12,
            max((width - 12) // 2, 1), BLACK,
        )

    def bored(self) -> None:
        self.shell(DIM_TEETH, 4, 10, 56, 13, 6)
        self.frame.hline(6, 16, 52, BLACK)
        self.frame.hline(6, 17, 52, BLACK)
        for x in (16, 27, 38, 49):
            self.frame.vline(x, 12, 9, BLACK)

    def watch(self, tick: int) -> None:
        self.resting()
        x = 5 + tick // 2 % 54
        self.frame.vline(x, 5, 5, (245, 178, 25))
        self.frame.vline(x, 23, 4, (245, 178, 25))

    def error(self, tick: int) -> None:
        fill = (255, 90, 60) if tick // 64 % 2 else (170, 25, 20)
        self.shell(fill)
        self.column_seams()
        points = (3, 10, 17, 24, 31, 38, 45, 52, 60)
        for index in range(8):
            self.frame.thick_line(
                points[index], 22 if index % 2 else 9,
                points[index + 1], 9 if index % 2 else 22, BLACK,
            )

    def battery_low(self, tick: int) -> None:
        if tick % 128 >= 112:
            return
        self.shell(DIM_TEETH, 5, 8, 54, 18, 8)
        for segment in ((10, 15, 24, 15), (24, 15, 32, 19),
                        (32, 19, 40, 15), (40, 15, 54, 15)):
            self.frame.thick_line(*segment, BLACK)
        self.frame.fill_rect(57, 13, 3, 7, DIM_TEETH)

    def rainy(self, tick: int) -> None:
        self.shell((185, 198, 172))
        self.column_seams()
        offset = tick // 8 % 8
        for x in range(-5 + offset, 64, 8):
            self.frame.thick_line(x, 14, x + 4, 18, BLACK)
            self.frame.thick_line(x + 4, 18, x + 8, 14, BLACK)

    def cloudy(self, tick: int) -> None:
        self.shell((205, 205, 160), 5, 6, 54, 22, 10)
        drift = tick // 30 % 3
        for segment in ((10, 15, 22, 13), (22, 13, 34, 16),
                        (34, 16, 46, 13), (46, 13, 54, 15)):
            self.frame.thick_line(
                segment[0], segment[1] + drift,
                segment[2], segment[3] + drift, BLACK,
            )

    def stormy(self, tick: int) -> None:
        self.shell((235, 215, 110))
        self.column_seams()
        shift = tick // 10 % 6
        for x in range(-8 + shift, 64, 12):
            self.frame.thick_line(x, 9, x + 6, 16, BLACK)
            self.frame.thick_line(x + 6, 16, x, 23, BLACK)

    def snowy(self, tick: int) -> None:
        self.shell((230, 232, 200))
        self.column_seams()
        dots = ((10, 9), (20, 18), (31, 12),
                (42, 21), (53, 8), (57, 18))
        phase = tick // 18 % 6
        for index, (x, source_y) in enumerate(dots):
            y = 5 + (source_y + phase + index) % 21
            self.frame.fill_circle(x, y, 2 if index == phase else 1, BLACK)

    def render(self, state_id: int, tick: int, intensity: int = 120) -> FrameBuffer:
        self.frame.clear()
        state = STATES[state_id]
        if state == "listening":
            self.listening(tick)
        elif state in ("thinking", "thinking_audio", "thinking_long"):
            self.thinking(
                tick, {"thinking": 0, "thinking_audio": 1,
                       "thinking_long": 2}[state],
            )
        elif state == "speaking":
            self.speaking(tick, intensity)
        elif state in ("happy_fake", "sunny"):
            self.happy(tick, False)
        elif state == "angry":
            self.angry()
        elif state == "error":
            self.error(tick)
        elif state == "asleep":
            pass
        elif state == "tool":
            self.tool(tick)
        elif state in ("idle", "left", "right", "up", "down",
                       "center", "neutral"):
            self.resting()
        elif state == "sarcastic":
            self.sarcastic()
        elif state == "suspicious":
            self.suspicious()
        elif state == "tired":
            self.tired(tick)
        elif state == "surprised":
            self.surprised(tick, False)
        elif state == "bored":
            self.bored()
        elif state == "dramatic":
            self.surprised(tick, True)
        elif state == "watch":
            self.watch(tick)
        elif state == "party":
            self.happy(tick, True)
        elif state == "battery_low":
            self.battery_low(tick)
        elif state == "rainy":
            self.rainy(tick)
        elif state == "cloudy":
            self.cloudy(tick)
        elif state == "stormy":
            self.stormy(tick)
        elif state == "snowy":
            self.snowy(tick)
        else:
            raise ValueError(f"unhandled mouth state: {state}")
        return self.frame


class EmulatorWindow:
    def __init__(self, args: argparse.Namespace) -> None:
        import tkinter as tk

        self.tk = tk
        self.renderer = MouthRenderer()
        self.state_id = STATE_IDS[args.state]
        self.scale = args.scale
        self.interval_ms = round(args.interval * 1000)
        self.auto_play = not args.paused
        self.brightness = args.brightness
        self.intensity = args.intensity
        self.last_step_ms = time.monotonic_ns() // 1_000_000
        self.last_pixels: list[tuple[int, int, int] | None] = (
            [None] * (WIDTH * HEIGHT)
        )

        self.root = tk.Tk()
        self.root.title("SarcasmOS mouth emulator")
        self.root.configure(background="#111111")
        self.root.resizable(False, False)

        self.status = tk.StringVar()
        tk.Label(
            self.root, textvariable=self.status, anchor="w",
            background="#111111", foreground="#eeeeee",
            font=("monospace", 11),
        ).pack(fill="x", padx=8, pady=(7, 4))

        self.canvas = tk.Canvas(
            self.root, width=WIDTH * self.scale,
            height=HEIGHT * self.scale, background="black",
            highlightthickness=0,
        )
        self.canvas.pack(padx=8, pady=(0, 5))
        self.pixel_items = [
            self.canvas.create_rectangle(
                (index % WIDTH) * self.scale,
                (index // WIDTH) * self.scale,
                (index % WIDTH + 1) * self.scale,
                (index // WIDTH + 1) * self.scale,
                outline="", fill="#000000",
            )
            for index in range(WIDTH * HEIGHT)
        ]

        tk.Label(
            self.root,
            text="←/→ state   Space/A autoplay   +/- brightness   [/] intensity   Q quit",
            background="#111111", foreground="#aaaaaa",
            font=("monospace", 9),
        ).pack(padx=8, pady=(0, 7))
        self.root.bind_all("<KeyPress>", self.on_key)
        self.root.after(0, self.update)

    def select(self, state_id: int, pause: bool = True) -> None:
        self.state_id = state_id % len(STATES)
        if pause:
            self.auto_play = False
        self.last_step_ms = time.monotonic_ns() // 1_000_000

    def on_key(self, event: object) -> None:
        key = event.keysym
        if key == "Left":
            self.select(self.state_id - 1)
        elif key == "Right":
            self.select(self.state_id + 1)
        elif key in ("space", "a", "A"):
            self.auto_play = not self.auto_play
            self.last_step_ms = time.monotonic_ns() // 1_000_000
        elif key in ("plus", "equal", "KP_Add"):
            self.brightness = min(255, self.brightness + 16)
        elif key in ("minus", "underscore", "KP_Subtract"):
            self.brightness = max(0, self.brightness - 16)
        elif key == "bracketright":
            self.intensity = min(255, self.intensity + 16)
        elif key == "bracketleft":
            self.intensity = max(0, self.intensity - 16)
        elif key in ("q", "Q", "Escape"):
            self.root.destroy()

    def update(self) -> None:
        now_ms = time.monotonic_ns() // 1_000_000
        if self.auto_play and now_ms - self.last_step_ms >= self.interval_ms:
            self.select(self.state_id + 1, pause=False)

        tick = now_ms // 16
        frame = self.renderer.render(self.state_id, tick, self.intensity)
        factor = math.sqrt(self.brightness / 255)
        for index, color in enumerate(frame.pixels):
            scaled = tuple(round(channel * factor) for channel in color)
            if scaled != self.last_pixels[index]:
                self.canvas.itemconfigure(
                    self.pixel_items[index],
                    fill=f"#{scaled[0]:02x}{scaled[1]:02x}{scaled[2]:02x}",
                )
                self.last_pixels[index] = scaled

        mode = "AUTO" if self.auto_play else "PAUSED"
        self.status.set(
            f"0x{self.state_id:02x}  {STATES[self.state_id]:<16} "
            f"{mode}   brightness {self.brightness:3}/255   "
            f"intensity {self.intensity:3}/255"
        )
        self.root.after(FRAME_MS, self.update)

    def run(self) -> None:
        self.root.mainloop()


def validate_protocol_ids() -> None:
    header = pathlib.Path(__file__).with_name("protocol.hpp").read_text()
    expected = {
        "kAnimIdle": 0x00, "kAnimListening": 0x01,
        "kAnimThinking": 0x02, "kAnimThinkingAudio": 0x03,
        "kAnimThinkingLong": 0x04, "kAnimSpeaking": 0x05,
        "kAnimHappy": 0x06, "kAnimAngry": 0x07,
        "kAnimError": 0x08, "kAnimSleep": 0x09,
        "kAnimTool": 0x0A, "kAnimLeft": 0x0B,
        "kAnimRight": 0x0C, "kAnimUp": 0x0D,
        "kAnimDown": 0x0E, "kAnimCenter": 0x0F,
        "kAnimNeutral": 0x10, "kAnimSarcastic": 0x11,
        "kAnimSuspicious": 0x12, "kAnimTired": 0x13,
        "kAnimSurprised": 0x14, "kAnimBored": 0x15,
        "kAnimDramatic": 0x16, "kAnimWatch": 0x17,
        "kAnimParty": 0x18, "kAnimBatteryLow": 0x19,
        "kAnimSunny": 0x1A, "kAnimRainy": 0x1B,
        "kAnimCloudy": 0x1C, "kAnimStormy": 0x1D,
        "kAnimSnowy": 0x1E,
    }
    for symbol, value in expected.items():
        declaration = f"{symbol} = 0x{value:02x};"
        if declaration not in header:
            raise AssertionError(f"protocol.hpp is missing {declaration}")


def self_test(renderer: MouthRenderer) -> None:
    validate_protocol_ids()
    hashes = set()
    for state_id, state in enumerate(STATES):
        frame = renderer.render(state_id, 1234, 120)
        if len(frame.pixels) != WIDTH * HEIGHT:
            raise AssertionError(f"{state}: incorrect framebuffer size")
        lit = sum(pixel != BLACK for pixel in frame.pixels)
        if state == "asleep":
            if lit != 0:
                raise AssertionError("asleep must be blank")
        elif lit == 0:
            raise AssertionError(f"{state}: unexpectedly blank")
        hashes.add(hashlib.sha256(frame.ppm()).hexdigest())
    if len(hashes) < 20:
        raise AssertionError("too many states render identically")
    print(f"OK: rendered {len(STATES)} protocol states at {WIDTH}x{HEIGHT}")
    print(f"OK: {len(hashes)} distinct validation frames")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--state", choices=STATES, default="idle")
    parser.add_argument("--scale", type=int, default=12)
    parser.add_argument("--interval", type=float, default=2.5)
    parser.add_argument("--brightness", type=int, default=64)
    parser.add_argument("--intensity", type=int, default=120)
    parser.add_argument("--paused", action="store_true")
    parser.add_argument(
        "--self-test", action="store_true",
        help="render and validate every state without opening a window",
    )
    parser.add_argument(
        "--dump", type=pathlib.Path,
        help="write the selected state as a binary PPM image and exit",
    )
    args = parser.parse_args()
    if args.scale < 2:
        parser.error("--scale must be at least 2")
    if args.interval <= 0:
        parser.error("--interval must be greater than zero")
    for option in ("brightness", "intensity"):
        if not 0 <= getattr(args, option) <= 255:
            parser.error(f"--{option} must be in the range 0..255")
    return args


def main() -> int:
    args = parse_args()
    renderer = MouthRenderer()
    if args.self_test:
        self_test(renderer)
        return 0
    if args.dump is not None:
        frame = renderer.render(STATE_IDS[args.state], 1234, args.intensity)
        args.dump.write_bytes(frame.ppm(args.brightness))
        print(f"Wrote {args.dump} ({WIDTH}x{HEIGHT}, {args.state})")
        return 0
    try:
        EmulatorWindow(args).run()
    except Exception as error:
        print(f"Unable to open emulator window: {error}", file=sys.stderr)
        print("Run with --self-test for headless validation.", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
