#!/usr/bin/env python3
"""Create the initial editable 240x240 Bender-style eye animation pack."""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import pathlib

import asset_tool

WIDTH = 240
HEIGHT = 240
BLACK = 0
YELLOW = 1
DIM_YELLOW = 2
WHITE = 3
CYAN = 4
BLUE = 5
GREEN = 6
RED = 7
DARK_RED = 8
PINK = 9
ORANGE = 10
RAIN = 11
CLOUD = 12
SNOW = 13
PURPLE = 14
AMBER = 15

PALETTE = [
    {"name": "black", "rgb": [0, 0, 0]},
    {"name": "eye_yellow", "rgb": [255, 246, 166]},
    {"name": "dim_yellow", "rgb": [140, 134, 90]},
    {"name": "white", "rgb": [255, 255, 245]},
    {"name": "listening_cyan", "rgb": [60, 210, 255]},
    {"name": "thinking_blue", "rgb": [55, 115, 255]},
    {"name": "happy_green", "rgb": [80, 255, 110]},
    {"name": "error_red", "rgb": [255, 70, 45]},
    {"name": "dark_red", "rgb": [125, 12, 10]},
    {"name": "party_pink", "rgb": [255, 55, 145]},
    {"name": "watch_orange", "rgb": [255, 175, 30]},
    {"name": "rain_blue", "rgb": [80, 155, 255]},
    {"name": "cloud_gray", "rgb": [185, 195, 195]},
    {"name": "snow_white", "rgb": [220, 240, 255]},
    {"name": "dramatic_purple", "rgb": [185, 90, 255]},
    {"name": "battery_amber", "rgb": [255, 190, 40]},
]


class Canvas:
    def __init__(self) -> None:
        self.pixels = [BLACK] * (WIDTH * HEIGHT)

    def put(self, x: int, y: int, color: int) -> None:
        if 0 <= x < WIDTH and 0 <= y < HEIGHT:
            self.pixels[y * WIDTH + x] = color

    def ellipse(
        self, cx: float, cy: float, rx: float, ry: float, color: int,
    ) -> None:
        x0, x1 = max(0, int(cx - rx)), min(WIDTH - 1, int(cx + rx))
        y0, y1 = max(0, int(cy - ry)), min(HEIGHT - 1, int(cy + ry))
        for y in range(y0, y1 + 1):
            dy = (y - cy) / ry
            span = rx * math.sqrt(max(0.0, 1.0 - dy * dy))
            for x in range(max(x0, math.ceil(cx - span)),
                           min(x1, math.floor(cx + span)) + 1):
                self.put(x, y, color)

    def circle(self, cx: float, cy: float, radius: float, color: int) -> None:
        self.ellipse(cx, cy, radius, radius, color)

    def polygon(self, points: list[tuple[float, float]], color: int) -> None:
        minimum = max(0, math.floor(min(y for _, y in points)))
        maximum = min(HEIGHT - 1, math.ceil(max(y for _, y in points)))
        for y in range(minimum, maximum + 1):
            intersections = []
            for index, (x1, y1) in enumerate(points):
                x2, y2 = points[(index + 1) % len(points)]
                if (y1 <= y < y2) or (y2 <= y < y1):
                    intersections.append(
                        x1 + (y - y1) * (x2 - x1) / (y2 - y1)
                    )
            intersections.sort()
            for start in range(0, len(intersections) - 1, 2):
                for x in range(
                    max(0, math.ceil(intersections[start])),
                    min(WIDTH - 1, math.floor(intersections[start + 1])) + 1,
                ):
                    self.put(x, y, color)

    def line(
        self, x1: float, y1: float, x2: float, y2: float,
        thickness: float, color: int,
    ) -> None:
        steps = max(1, int(max(abs(x2 - x1), abs(y2 - y1))))
        for step in range(steps + 1):
            amount = step / steps
            self.circle(
                x1 + (x2 - x1) * amount,
                y1 + (y2 - y1) * amount,
                thickness / 2,
                color,
            )

    def rect(
        self, x: int, y: int, width: int, height: int, color: int,
    ) -> None:
        for row in range(max(0, y), min(HEIGHT, y + height)):
            start = row * WIDTH + max(0, x)
            end = row * WIDTH + min(WIDTH, x + width)
            self.pixels[start:end] = [color] * max(0, end - start)

    def star(
        self, cx: float, cy: float, outer: float, inner: float,
        color: int, points: int = 5, rotation: float = -math.pi / 2,
    ) -> None:
        vertices = []
        for index in range(points * 2):
            radius = outer if index % 2 == 0 else inner
            angle = rotation + index * math.pi / points
            vertices.append(
                (cx + math.cos(angle) * radius, cy + math.sin(angle) * radius)
            )
        self.polygon(vertices, color)

    def mask_circle(self) -> None:
        radius_squared = 119 * 119
        for y in range(HEIGHT):
            for x in range(WIDTH):
                if (x - 119.5) ** 2 + (y - 119.5) ** 2 > radius_squared:
                    self.put(x, y, BLACK)

    def rows(self) -> list[str]:
        self.mask_circle()
        return [
            "".join(
                format(value, "X")
                for value in self.pixels[y * WIDTH:(y + 1) * WIDTH]
            )
            for y in range(HEIGHT)
        ]


def eye(
    role: str,
    *,
    gaze_x: float = 0,
    gaze_y: float = 0,
    openness: float = 1.0,
    tilt: float = 0,
    curve: float = 0,
    eye_color: int = YELLOW,
    pupil_scale: float = 1.0,
    no_pupil: bool = False,
    star_pupil: bool = False,
    highlight: bool = False,
) -> Canvas:
    canvas = Canvas()
    cx, cy, rx, ry = 120.0, 120.0, 91.0, 80.0
    canvas.ellipse(cx, cy, rx + 7, ry + 7, BLACK)
    canvas.ellipse(cx, cy, rx, ry, eye_color)

    visible = [False] * (WIDTH * HEIGHT)
    for y in range(HEIGHT):
        for x in range(WIDTH):
            normalized_x = (x - cx) / rx
            if abs(normalized_x) > 1:
                continue
            ellipse_half = ry * math.sqrt(max(0.0, 1 - normalized_x ** 2))
            natural_top = cy - ellipse_half
            natural_bottom = cy + ellipse_half
            half_open = ellipse_half * max(0.02, min(1.0, openness))
            center_shift = tilt * normalized_x + curve * (
                normalized_x * normalized_x - 0.35
            )
            top = max(natural_top, cy - half_open + center_shift)
            bottom = min(natural_bottom, cy + half_open - center_shift)
            if top <= y <= bottom:
                visible[y * WIDTH + x] = True
            elif natural_top <= y <= natural_bottom:
                canvas.put(x, y, BLACK)

    pupil_x = cx + max(-34, min(34, gaze_x))
    pupil_y = cy + max(-26, min(26, gaze_y))
    if not no_pupil:
        if star_pupil:
            canvas.star(
                pupil_x, pupil_y, 30 * pupil_scale, 12 * pupil_scale, BLACK,
            )
        else:
            half = 16 * pupil_scale
            canvas.polygon(
                [
                    (pupil_x - half, pupil_y - half),
                    (pupil_x + half, pupil_y - half + 2),
                    (pupil_x + half - 2, pupil_y + half),
                    (pupil_x - half + 1, pupil_y + half - 1),
                ],
                BLACK,
            )
        if highlight and pupil_scale >= 0.55:
            canvas.circle(
                pupil_x - 6 * pupil_scale,
                pupil_y - 10 * pupil_scale,
                max(2, 4 * pupil_scale),
                WHITE,
            )
        for index, is_visible in enumerate(visible):
            if not is_visible:
                x, y = index % WIDTH, index // WIDTH
                normalized_x = (x - cx) / rx
                if abs(normalized_x) <= 1:
                    ellipse_half = ry * math.sqrt(
                        max(0.0, 1 - normalized_x ** 2)
                    )
                    if cy - ellipse_half <= y <= cy + ellipse_half:
                        canvas.pixels[index] = BLACK
    return canvas


def closed_eye(color: int = YELLOW, curve: float = 0) -> Canvas:
    canvas = Canvas()
    points = []
    for x in range(43, 198, 4):
        normalized = (x - 120) / 77
        y = 121 + curve * (normalized * normalized - 0.5)
        points.append((x, y))
    for first, second in zip(points, points[1:]):
        canvas.line(*first, *second, 10, color)
    return canvas


def error_eye(phase: int) -> Canvas:
    canvas = Canvas()
    color = RED if phase == 0 else DARK_RED
    canvas.circle(120, 120, 91, DARK_RED)
    canvas.circle(120, 120, 82, BLACK)
    canvas.line(72, 72, 168, 168, 19, color)
    canvas.line(168, 72, 72, 168, 19, color)
    return canvas


def add_ring_dot(canvas: Canvas, phase: int, count: int, color: int) -> None:
    angle = phase * math.tau / count - math.pi / 2
    canvas.circle(
        120 + math.cos(angle) * 100,
        120 + math.sin(angle) * 100,
        7,
        color,
    )


def add_tear(canvas: Canvas, phase: int) -> None:
    y = 137 + phase * 9
    canvas.circle(176, y, 7, RAIN)
    canvas.polygon([(169, y), (183, y), (176, y - 18)], RAIN)


def add_snow(canvas: Canvas, phase: int) -> None:
    points = ((55, 58), (184, 66), (56, 175), (178, 184), (120, 41))
    for index, (x, y) in enumerate(points):
        offset = (phase * 7 + index * 13) % 35
        canvas.line(x - 6, y + offset, x + 6, y + offset, 3, SNOW)
        canvas.line(x, y + offset - 6, x, y + offset + 6, 3, SNOW)


def draw_cloud(
    canvas: Canvas, x: float, y: float, color: int = CLOUD,
    scale: float = 1.0,
) -> None:
    canvas.rect(
        round(x - 72 * scale), round(y - 2 * scale),
        round(144 * scale), round(52 * scale), color,
    )
    canvas.circle(x - 48 * scale, y, 35 * scale, color)
    canvas.circle(x - 10 * scale, y - 26 * scale, 49 * scale, color)
    canvas.circle(x + 39 * scale, y - 12 * scale, 42 * scale, color)
    canvas.circle(x + 66 * scale, y + 7 * scale, 28 * scale, color)


def render_state(name: str, frame: int, count: int, role: str) -> Canvas:
    phase = frame / max(1, count - 1)
    wave = math.sin(frame * math.tau / max(1, count))
    inward = 13 if role == "left" else -13

    if name == "idle":
        openness = (1.0, 1.0, 1.0, 1.0, 0.8, 0.35, 0.08, 0.5)[frame]
        return eye(role, gaze_x=wave * 3, openness=openness)
    if name == "listening":
        canvas = eye(role, gaze_x=wave * 25, openness=0.9)
        add_ring_dot(canvas, frame, count, CYAN)
        return canvas
    if name in ("thinking", "thinking_audio", "thinking_long"):
        speed_phase = frame * math.tau / count
        radius_x = 24 if name != "thinking_long" else 30
        radius_y = 15 if name != "thinking_audio" else 22
        canvas = eye(
            role,
            gaze_x=math.cos(speed_phase) * radius_x,
            gaze_y=math.sin(speed_phase) * radius_y,
            openness=0.78,
        )
        canvas.circle(120, 25, 4 + (frame % 3) * 2, BLUE)
        return canvas
    if name == "speaking":
        return eye(
            role, gaze_y=wave * 7, openness=0.58 + 0.12 * wave,
            curve=-10, pupil_scale=0.88,
        )
    if name == "happy_fake":
        openness = (0.42, 0.32, 0.25, 0.32, 0.42)[frame]
        return eye(
            role, openness=openness, curve=-10,
            tilt=20 if role == "left" else -20, no_pupil=True,
            eye_color=YELLOW,
        )
    if name == "angry":
        twitch = (-2, 1, 0, 2)[frame]
        return eye(
            role, gaze_x=inward + twitch, openness=0.62,
            tilt=28 if role == "left" else -28,
            curve=7, pupil_scale=0.8, highlight=False,
        )
    if name == "error":
        return error_eye(frame)
    if name == "asleep":
        return closed_eye(DIM_YELLOW, curve=12)
    if name == "tool":
        canvas = eye(
            role, gaze_x=inward, openness=0.48,
            tilt=14 if role == "left" else -14, pupil_scale=0.75,
        )
        add_ring_dot(canvas, frame, count, ORANGE)
        return canvas
    if name == "left":
        return eye(role, gaze_x=-31)
    if name == "right":
        return eye(role, gaze_x=31)
    if name == "up":
        return eye(role, gaze_y=-25)
    if name == "down":
        return eye(role, gaze_y=25)
    if name in ("center", "neutral"):
        return eye(role)
    if name == "sarcastic":
        left_open = 0.34 + frame * 0.03
        return eye(
            role, gaze_x=14, openness=left_open if role == "left" else 0.8,
            tilt=-8 if role == "left" else 5, pupil_scale=0.9,
        )
    if name == "suspicious":
        return eye(
            role, gaze_x=10 + wave * 7, openness=0.38,
            tilt=-6 if role == "left" else 6, pupil_scale=0.72,
            highlight=False,
        )
    if name == "tired":
        return eye(
            role, gaze_y=18, openness=0.30 + frame * 0.04,
            eye_color=DIM_YELLOW, pupil_scale=0.85, highlight=False,
        )
    if name == "surprised":
        return eye(
            role, openness=1.0, pupil_scale=0.55 + frame * 0.08,
        )
    if name == "bored":
        return eye(
            role, gaze_x=-18, gaze_y=12, openness=0.32,
            eye_color=DIM_YELLOW, highlight=False,
        )
    if name == "dramatic":
        return eye(
            role, openness=1.0, pupil_scale=(0.65, 0.9, 1.15)[frame],
            star_pupil=True,
        )
    if name == "watch":
        return eye(role, gaze_x=-32 + frame * 64 / (count - 1), openness=0.92)
    if name == "party":
        if (frame // 2 + (role == "right")) % 2 == 0:
            canvas = eye(
                role, openness=0.15, curve=-20, no_pupil=True,
                eye_color=YELLOW,
            )
        else:
            canvas = eye(
                role, pupil_scale=0.95, star_pupil=True, eye_color=YELLOW,
            )
        add_ring_dot(canvas, frame, count, PINK if frame % 2 else PURPLE)
        return canvas
    if name == "battery_low":
        canvas = eye(
            role, gaze_y=20, openness=0.22 + frame * 0.04,
            eye_color=DIM_YELLOW, no_pupil=True,
        )
        canvas.rect(91, 108, 58, 28, AMBER)
        canvas.rect(149, 116, 7, 12, AMBER)
        canvas.rect(97, 114, 12 if frame else 7, 16, BLACK)
        return canvas
    if name == "sunny":
        canvas = Canvas()
        pulse = 2 if frame else 0
        for angle_index in range(8):
            angle = angle_index * math.tau / 8
            canvas.line(
                120 + math.cos(angle) * (74 + pulse),
                120 + math.sin(angle) * (74 + pulse),
                120 + math.cos(angle) * (103 + pulse),
                120 + math.sin(angle) * (103 + pulse),
                14, ORANGE,
            )
        canvas.circle(120, 120, 61 + pulse, AMBER)
        return canvas
    if name == "rainy":
        canvas = Canvas()
        draw_cloud(canvas, 120 + wave * 4, 77, CLOUD, 0.82)
        for index, x in enumerate((58, 92, 126, 160, 194)):
            y = 139 + ((frame * 19 + index * 27) % 70)
            canvas.line(x + 8, y - 10, x - 8, y + 17, 8, RAIN)
        return canvas
    if name == "cloudy":
        canvas = Canvas()
        draw_cloud(canvas, 120 + wave * 7, 112 + wave * 2, CLOUD, 1.0)
        return canvas
    if name == "stormy":
        canvas = Canvas()
        draw_cloud(canvas, 120 - wave * 4, 76, CLOUD, 0.86)
        shift = ((frame % 4) - 2) * 3
        canvas.polygon(
            [(111 + shift, 119), (143 + shift, 119),
             (126 + shift, 157), (151 + shift, 157),
             (100 + shift, 224), (116 + shift, 174),
             (91 + shift, 174)],
            ORANGE,
        )
        return canvas
    if name == "snowy":
        canvas = Canvas()
        draw_cloud(canvas, 120, 69, CLOUD, 0.78)
        for index, x in enumerate((49, 84, 119, 154, 189)):
            y = 137 + ((frame * 13 + index * 29) % 76)
            canvas.line(x - 9, y, x + 9, y, 4, SNOW)
            canvas.line(x, y - 9, x, y + 9, 4, SNOW)
            canvas.line(x - 6, y - 6, x + 6, y + 6, 3, SNOW)
            canvas.line(x + 6, y - 6, x - 6, y + 6, 3, SNOW)
        return canvas
    raise ValueError(f"no renderer for {name}")


ANIMATIONS = (
    ("idle", 8, 120, "loop"),
    ("listening", 8, 192, "loop"),
    ("thinking", 8, 160, "loop"),
    ("thinking_audio", 6, 110, "loop"),
    ("thinking_long", 8, 320, "loop"),
    ("speaking", 6, 110, "loop"),
    ("happy_fake", 5, 150, "ping_pong"),
    ("angry", 4, 180, "loop"),
    ("error", 2, 1024, "loop"),
    ("asleep", 1, 1000, "loop"),
    ("tool", 8, 192, "loop"),
    ("left", 1, 1000, "loop"),
    ("right", 1, 1000, "loop"),
    ("up", 1, 1000, "loop"),
    ("down", 1, 1000, "loop"),
    ("center", 1, 1000, "loop"),
    ("neutral", 1, 1000, "loop"),
    ("sarcastic", 4, 320, "ping_pong"),
    ("suspicious", 4, 300, "loop"),
    ("tired", 2, 640, "loop"),
    ("surprised", 2, 350, "ping_pong"),
    ("bored", 1, 1000, "loop"),
    ("dramatic", 3, 192, "ping_pong"),
    ("watch", 10, 180, "ping_pong"),
    ("party", 6, 144, "loop"),
    ("battery_low", 2, 500, "loop"),
    ("sunny", 2, 800, "ping_pong"),
    ("rainy", 8, 180, "loop"),
    ("cloudy", 5, 300, "loop"),
    ("stormy", 8, 160, "loop"),
    ("snowy", 6, 288, "loop"),
)


def create_pack() -> dict:
    sprites: dict[str, dict[str, list[str]]] = {}
    rows_to_name: dict[str, str] = {}
    animations = []
    for state_id, (name, count, frame_ms, playback) in enumerate(ANIMATIONS):
        frames = []
        for frame in range(count):
            pair = {}
            left_rows = render_state(name, frame, count, "left").rows()
            for role, rows in (
                ("left", left_rows),
                ("right", asset_tool.mirror_rows(left_rows)),
            ):
                digest = hashlib.sha256("".join(rows).encode()).hexdigest()
                sprite_name = rows_to_name.get(digest)
                if sprite_name is None:
                    sprite_name = f"{name}_{role}_{frame:02d}"
                    rows_to_name[digest] = sprite_name
                    sprites[sprite_name] = {"rows": rows}
                pair[role] = sprite_name
            frames.append(pair)
        animations.append({
            "id": state_id,
            "name": name,
            "flip_left": False,
            "flip_right": False,
            "playback": playback,
            "frame_ms": frame_ms,
            "frames": frames,
        })
    return {
        "format": "sarcasmos-eye-assets",
        "version": 1,
        "width": WIDTH,
        "height": HEIGHT,
        "palette": PALETTE,
        "animations": animations,
        "sprites": sprites,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--force", action="store_true",
        help="overwrite the editable asset pack (destroys manual artwork edits)",
    )
    args = parser.parse_args()
    if asset_tool.DEFAULT_ASSETS.exists() and not args.force:
        parser.error(
            f"{asset_tool.DEFAULT_ASSETS} already exists; use --force to replace it"
        )
    pack = create_pack()
    asset_tool.DEFAULT_ASSETS.parent.mkdir(parents=True, exist_ok=True)
    asset_tool.DEFAULT_ASSETS.write_text(json.dumps(pack, indent=2) + "\n")
    asset_tool.compile_assets()
    print(
        f"Created {len(pack['animations'])} animations and "
        f"{len(pack['sprites'])} deduplicated paired-eye sprites"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
