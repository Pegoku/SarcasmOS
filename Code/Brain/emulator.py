#!/usr/bin/env python3
"""View-only desktop emulator for the complete SarcasmOS face."""

from __future__ import annotations

import argparse
import importlib.util
import math
import pathlib
import sys
import time
from types import ModuleType
from typing import Any


FRAME_MS = 40
BLACK = (0, 0, 0)
WEATHER_STATES = {"sunny", "rainy", "cloudy", "stormy", "snowy"}
TEMPERATURE_MIN = -127
TEMPERATURE_MAX = 127
ROOT = pathlib.Path(__file__).resolve().parent
EYE_ROOT = ROOT.parent / "Eye"
MOUTH_ROOT = ROOT.parent / "Mouth-NeonPCB"


def load_asset_tool(name: str, path: pathlib.Path) -> ModuleType:
    """Load sibling asset tools without their shared module name colliding."""
    spec = importlib.util.spec_from_file_location(name, path)
    if spec is None or spec.loader is None:
        raise ImportError(f"could not load {path}")
    module = importlib.util.module_from_spec(spec)
    sys.modules[name] = module
    spec.loader.exec_module(module)
    return module


EYE_TOOL = load_asset_tool("sarcasmos_eye_asset_tool", EYE_ROOT / "asset_tool.py")
MOUTH_TOOL = load_asset_tool(
    "sarcasmos_mouth_asset_tool", MOUTH_ROOT / "asset_tool.py",
)


def scaled_pixels(
    pixels: list[tuple[int, int, int]], brightness: int,
) -> list[tuple[int, int, int]]:
    factor = math.sqrt(brightness / 255)
    return [
        tuple(round(channel * factor) for channel in pixel)
        for pixel in pixels
    ]


def ppm_bytes(
    width: int, height: int, pixels: list[tuple[int, int, int]],
) -> bytes:
    body = bytearray(channel for pixel in pixels for channel in pixel)
    return f"P6\n{width} {height}\n255\n".encode() + body


class EyeAssets:
    def __init__(self, data: dict[str, Any]) -> None:
        self.data = data
        self.width = data["width"]
        self.height = data["height"]
        self.palette = EYE_TOOL.palette(data)
        self.animations = data["animations"]
        self.sprite_cache = {
            name: EYE_TOOL.decode_sprite(data, name)
            for name in data["sprites"]
        }

    def local_frame(self, state_id: int, elapsed_ms: int) -> int:
        animation = self.animations[state_id]
        count = len(animation["frames"])
        if count <= 1:
            return 0
        step = elapsed_ms // animation["frame_ms"]
        if animation["playback"] == "ping_pong":
            final_frame = count - 1
            phase = step % (final_frame * 2)
            return phase if phase <= final_frame else final_frame * 2 - phase
        return step % count

    def pixels(
        self, state_id: int, role: str, elapsed_ms: int,
    ) -> tuple[int, list[tuple[int, int, int]]]:
        frame = self.local_frame(state_id, elapsed_ms)
        return frame, self.frame_pixels(state_id, role, frame)

    def frame_pixels(
        self, state_id: int, role: str, frame: int,
    ) -> list[tuple[int, int, int]]:
        animation = self.animations[state_id]
        source_role = (
            "right" if role == "left" else "left"
        ) if animation[f"flip_{role}"] else role
        sprite = animation["frames"][frame][source_role]
        return [
            self.palette[index] for index in self.sprite_cache[sprite]
        ]


class MouthAssets:
    def __init__(self, data: dict[str, Any]) -> None:
        self.data = data
        self.width = data["width"]
        self.height = data["height"]
        self.palette = MOUTH_TOOL.palette(data)
        self.animations = data["animations"]
        self.sprite_cache = {
            name: MOUTH_TOOL.decode_sprite(data, name)
            for name in data["sprites"]
        }
        font = MOUTH_TOOL.load_temperature_font()
        self.temperature_glyphs = MOUTH_TOOL.temperature_glyphs(font)
        self.glyph_width = font["width"]
        self.glyph_height = font["height"]
        self.glyph_spacing = font["spacing"]

    def local_frame(
        self, state_id: int, elapsed_ms: int, intensity: int,
    ) -> int:
        animation = self.animations[state_id]
        count = len(animation["frames"])
        if count <= 1:
            return 0
        step = elapsed_ms // animation["frame_ms"]
        final_frame = count - 1
        if animation["playback"] in ("ping_pong", "intensity"):
            phase = step % (final_frame * 2)
            frame = phase if phase <= final_frame else final_frame * 2 - phase
            if animation["playback"] == "intensity":
                return min(final_frame, frame * intensity // 120)
            return frame
        return step % count

    def pixels(
        self, state_id: int, elapsed_ms: int, intensity: int,
        temperature: int,
    ) -> tuple[int, list[tuple[int, int, int]]]:
        frame = self.local_frame(state_id, elapsed_ms, intensity)
        return frame, self.frame_pixels(state_id, frame, temperature)

    def frame_pixels(
        self, state_id: int, frame: int, temperature: int,
    ) -> list[tuple[int, int, int]]:
        sprite = self.animations[state_id]["frames"][frame]
        pixels = [
            self.palette[index] for index in self.sprite_cache[sprite]
        ]
        if self.animations[state_id]["name"] in WEATHER_STATES:
            pixels = self.apply_temperature(pixels, temperature)
        return pixels

    def apply_temperature(
        self, pixels: list[tuple[int, int, int]], temperature: int,
    ) -> list[tuple[int, int, int]]:
        rendered = pixels.copy()
        text = f"{temperature}°C"
        text_width = (
            len(text) * self.glyph_width +
            (len(text) - 1) * self.glyph_spacing
        )
        x = (self.width - text_width) // 2
        y = (self.height - self.glyph_height) // 2
        for character in text:
            for row, bits in enumerate(self.temperature_glyphs[character]):
                for column in range(self.glyph_width):
                    if bits & (1 << (self.glyph_width - column - 1)):
                        rendered[(y + row) * self.width + x + column] = BLACK
            x += self.glyph_width + self.glyph_spacing
        return rendered


class FaceAssets:
    def __init__(self) -> None:
        self.reload()

    def reload(self) -> None:
        eye = EyeAssets(EYE_TOOL.load_assets())
        mouth = MouthAssets(MOUTH_TOOL.load_assets())
        eye_states = tuple(animation["name"] for animation in eye.animations)
        mouth_states = tuple(
            animation["name"] for animation in mouth.animations
        )
        if eye_states != mouth_states:
            raise ValueError("eye and mouth animation states do not match")
        self.eye = eye
        self.mouth = mouth
        self.states = eye_states
        self.state_ids = {name: index for index, name in enumerate(self.states)}


class EmulatorWindow:
    def __init__(self, args: argparse.Namespace) -> None:
        import tkinter as tk

        self.tk = tk
        self.assets = FaceAssets()
        self.state_id = self.assets.state_ids[args.state]
        self.eye_scale = args.eye_scale
        self.mouth_scale = args.mouth_scale
        self.eye_gap = args.eye_gap * self.eye_scale
        self.face_gap = args.face_gap
        self.interval_ms = round(args.interval * 1000)
        self.auto_play = not args.paused
        self.brightness = args.brightness
        self.intensity = args.intensity
        self.temperature = args.temperature
        self.elapsed_ms = 0
        self.manual_eye_frame = 0 if args.paused else None
        self.manual_mouth_frame = 0 if args.paused else None
        now_ms = time.monotonic_ns() // 1_000_000
        self.last_update_ms = now_ms
        self.last_step_ms = now_ms
        self.last_render_key: tuple[Any, ...] | None = None
        self.notice = ""
        self.notice_until_ms = 0
        self.photos: list[Any] = []

        eye_size = self.assets.eye.width * self.eye_scale
        eyes_width = eye_size * 2 + self.eye_gap
        mouth_width = self.assets.mouth.width * self.mouth_scale
        mouth_height = self.assets.mouth.height * self.mouth_scale
        self.canvas_width = max(eyes_width, mouth_width)
        self.canvas_height = eye_size + self.face_gap + mouth_height

        self.root = tk.Tk()
        self.root.title("SarcasmOS full-face emulator (view only)")
        self.root.configure(background="#111111")
        self.root.resizable(False, False)

        self.status = tk.StringVar()
        tk.Label(
            self.root, textvariable=self.status, anchor="w",
            background="#111111", foreground="#eeeeee",
            font=("monospace", 11),
        ).pack(fill="x", padx=8, pady=(7, 4))

        self.canvas = tk.Canvas(
            self.root, width=self.canvas_width + 16,
            height=self.canvas_height + 16, background="#20252a",
            highlightthickness=0,
        )
        self.canvas.pack(padx=8, pady=(0, 5))

        controls = tk.Frame(self.root, background="#111111")
        controls.pack(fill="x", padx=8, pady=(0, 5))
        tk.Button(controls, text="Previous", command=self.previous_state).pack(
            side="left", padx=(0, 5),
        )
        self.state_var = tk.StringVar(value=self.state_name)
        tk.OptionMenu(
            controls, self.state_var, *self.assets.states,
            command=self.choose_state,
        ).pack(side="left", padx=(0, 5))
        self.play_button = tk.Button(
            controls, command=self.toggle_playback,
        )
        self.play_button.pack(side="left", padx=(0, 5))
        tk.Button(controls, text="Next", command=self.next_state).pack(
            side="left", padx=(0, 5),
        )
        tk.Button(
            controls, text="Reload assets (R)", command=self.reload_assets,
        ).pack(side="right")

        frame_controls = tk.Frame(self.root, background="#111111")
        frame_controls.pack(fill="x", padx=8, pady=(0, 5))
        tk.Button(
            frame_controls, text="Previous frame (↓)",
            command=lambda: self.step_frames(-1),
        ).pack(side="left", padx=(0, 5))
        tk.Button(
            frame_controls, text="Next frame (↑)",
            command=lambda: self.step_frames(1),
        ).pack(side="left")

        tk.Label(
            self.root,
            text=(
                "←/→ state   ↑/↓ frame   Space play/pause   "
                "+/- brightness   [/] intensity   ,/. temperature   "
                "R reload   Q quit"
            ),
            background="#111111", foreground="#aaaaaa",
            font=("monospace", 9),
        ).pack(padx=8, pady=(0, 7))
        self.root.bind_all("<KeyPress>", self.on_key)
        self.root.after(0, self.update)

    @property
    def state_name(self) -> str:
        return self.assets.states[self.state_id]

    def show_notice(self, message: str) -> None:
        self.notice = message
        self.notice_until_ms = time.monotonic_ns() // 1_000_000 + 4000

    def select(self, state_id: int, pause: bool = True) -> None:
        self.state_id = state_id % len(self.assets.states)
        self.state_var.set(self.state_name)
        if pause:
            self.auto_play = False
            self.manual_eye_frame = 0
            self.manual_mouth_frame = 0
        else:
            self.manual_eye_frame = None
            self.manual_mouth_frame = None
        self.elapsed_ms = 0
        now_ms = time.monotonic_ns() // 1_000_000
        self.last_step_ms = now_ms
        self.last_update_ms = now_ms
        self.last_render_key = None

    def choose_state(self, state: str) -> None:
        self.select(self.assets.state_ids[state])

    def previous_state(self) -> None:
        self.select(self.state_id - 1)

    def next_state(self) -> None:
        self.select(self.state_id + 1)

    def toggle_playback(self) -> None:
        if self.auto_play:
            self.manual_eye_frame = self.assets.eye.local_frame(
                self.state_id, self.elapsed_ms,
            )
            self.manual_mouth_frame = self.assets.mouth.local_frame(
                self.state_id, self.elapsed_ms, self.intensity,
            )
            self.auto_play = False
        else:
            self.manual_eye_frame = None
            self.manual_mouth_frame = None
            self.auto_play = True
        now_ms = time.monotonic_ns() // 1_000_000
        self.last_step_ms = now_ms
        self.last_update_ms = now_ms

    def step_frames(self, direction: int) -> None:
        eye_animation = self.assets.eye.animations[self.state_id]
        mouth_animation = self.assets.mouth.animations[self.state_id]
        if self.manual_eye_frame is None:
            self.manual_eye_frame = self.assets.eye.local_frame(
                self.state_id, self.elapsed_ms,
            )
        if self.manual_mouth_frame is None:
            self.manual_mouth_frame = self.assets.mouth.local_frame(
                self.state_id, self.elapsed_ms, self.intensity,
            )
        self.auto_play = False
        self.manual_eye_frame = (
            self.manual_eye_frame + direction
        ) % len(eye_animation["frames"])
        self.manual_mouth_frame = (
            self.manual_mouth_frame + direction
        ) % len(mouth_animation["frames"])
        self.last_render_key = None

    def reload_assets(self) -> None:
        state = self.state_name
        try:
            self.assets.reload()
            self.state_id = self.assets.state_ids.get(state, 0)
            self.state_var.set(self.state_name)
            if self.manual_eye_frame is not None:
                self.manual_eye_frame %= len(
                    self.assets.eye.animations[self.state_id]["frames"]
                )
            if self.manual_mouth_frame is not None:
                self.manual_mouth_frame %= len(
                    self.assets.mouth.animations[self.state_id]["frames"]
                )
            self.last_render_key = None
            self.show_notice("Reloaded eye and mouth assets")
        except (OSError, ValueError, KeyError) as error:
            self.show_notice(f"Reload failed: {error}")

    def make_photo(
        self, pixels: list[tuple[int, int, int]], width: int,
        height: int, scale: int,
    ) -> Any:
        native = self.tk.PhotoImage(
            data=ppm_bytes(width, height, scaled_pixels(pixels, self.brightness)),
            format="PPM",
        )
        scaled = native.zoom(scale, scale)
        self.photos.extend((native, scaled))
        return scaled

    def render(self) -> tuple[int, int]:
        if self.manual_eye_frame is None:
            eye_frame, left = self.assets.eye.pixels(
                self.state_id, "left", self.elapsed_ms,
            )
            _, right = self.assets.eye.pixels(
                self.state_id, "right", self.elapsed_ms,
            )
        else:
            eye_frame = self.manual_eye_frame
            left = self.assets.eye.frame_pixels(
                self.state_id, "left", eye_frame,
            )
            right = self.assets.eye.frame_pixels(
                self.state_id, "right", eye_frame,
            )
        if self.manual_mouth_frame is None:
            mouth_frame, mouth = self.assets.mouth.pixels(
                self.state_id, self.elapsed_ms, self.intensity,
                self.temperature,
            )
        else:
            mouth_frame = self.manual_mouth_frame
            mouth = self.assets.mouth.frame_pixels(
                self.state_id, mouth_frame, self.temperature,
            )

        self.canvas.delete("face")
        self.photos = []
        eye_size = self.assets.eye.width * self.eye_scale
        eyes_width = eye_size * 2 + self.eye_gap
        eye_x = 8 + (self.canvas_width - eyes_width) // 2
        for index, pixels in enumerate((left, right)):
            photo = self.make_photo(
                pixels, self.assets.eye.width, self.assets.eye.height,
                self.eye_scale,
            )
            x = eye_x + index * (eye_size + self.eye_gap)
            self.canvas.create_image(
                x, 8, anchor="nw", image=photo, tags="face",
            )
            self.canvas.create_oval(
                x, 8, x + eye_size, 8 + eye_size,
                outline="#8b949e", width=max(1, self.eye_scale), tags="face",
            )

        mouth_photo = self.make_photo(
            mouth, self.assets.mouth.width, self.assets.mouth.height,
            self.mouth_scale,
        )
        mouth_width = self.assets.mouth.width * self.mouth_scale
        mouth_x = 8 + (self.canvas_width - mouth_width) // 2
        mouth_y = 8 + eye_size + self.face_gap
        self.canvas.create_image(
            mouth_x, mouth_y, anchor="nw", image=mouth_photo, tags="face",
        )
        self.canvas.create_rectangle(
            mouth_x, mouth_y, mouth_x + mouth_width,
            mouth_y + self.assets.mouth.height * self.mouth_scale,
            outline="#8b949e", width=max(1, self.eye_scale), tags="face",
        )
        return eye_frame, mouth_frame

    def on_key(self, event: Any) -> None:
        key = event.keysym
        if key == "Left":
            self.previous_state()
        elif key == "Right":
            self.next_state()
        elif key == "Up":
            self.step_frames(1)
        elif key == "Down":
            self.step_frames(-1)
        elif key in ("space", "a", "A"):
            self.toggle_playback()
        elif key in ("plus", "equal", "KP_Add"):
            self.brightness = min(255, self.brightness + 16)
            self.last_render_key = None
        elif key in ("minus", "underscore", "KP_Subtract"):
            self.brightness = max(0, self.brightness - 16)
            self.last_render_key = None
        elif key == "bracketright":
            self.intensity = min(255, self.intensity + 16)
            self.last_render_key = None
        elif key == "bracketleft":
            self.intensity = max(0, self.intensity - 16)
            self.last_render_key = None
        elif key in ("period", "greater"):
            self.temperature = min(TEMPERATURE_MAX, self.temperature + 1)
            self.last_render_key = None
        elif key in ("comma", "less"):
            self.temperature = max(TEMPERATURE_MIN, self.temperature - 1)
            self.last_render_key = None
        elif key in ("r", "R"):
            self.reload_assets()
        elif key in ("q", "Q", "Escape"):
            self.root.destroy()

    def update(self) -> None:
        now_ms = time.monotonic_ns() // 1_000_000
        delta_ms = max(0, now_ms - self.last_update_ms)
        self.last_update_ms = now_ms
        if self.auto_play:
            self.elapsed_ms += delta_ms
            if now_ms - self.last_step_ms >= self.interval_ms:
                self.select(self.state_id + 1, pause=False)

        render_key = (
            self.state_id, self.elapsed_ms // FRAME_MS, self.brightness,
            self.intensity, self.temperature, self.manual_eye_frame,
            self.manual_mouth_frame,
        )
        if render_key != self.last_render_key:
            eye_frame, mouth_frame = self.render()
            self.last_render_key = render_key
        else:
            eye_frame = (
                self.assets.eye.local_frame(self.state_id, self.elapsed_ms)
                if self.manual_eye_frame is None
                else self.manual_eye_frame
            )
            mouth_frame = (
                self.assets.mouth.local_frame(
                    self.state_id, self.elapsed_ms, self.intensity,
                )
                if self.manual_mouth_frame is None
                else self.manual_mouth_frame
            )

        eye_animation = self.assets.eye.animations[self.state_id]
        mouth_animation = self.assets.mouth.animations[self.state_id]
        mode = "AUTO" if self.auto_play else "PAUSED"
        status = (
            f"0x{self.state_id:02x}  {self.state_name:<16} "
            f"eyes {eye_frame + 1}/{len(eye_animation['frames'])}  "
            f"mouth {mouth_frame + 1}/{len(mouth_animation['frames'])}  "
            f"{mode}  brightness {self.brightness:3}/255  "
            f"intensity {self.intensity:3}/255"
        )
        if self.state_name in WEATHER_STATES:
            status += f"  temperature {self.temperature}°C"
        if now_ms < self.notice_until_ms:
            status = self.notice
        self.status.set(status)
        self.play_button.configure(
            text="Pause" if self.auto_play else "Play",
        )
        self.root.after(FRAME_MS, self.update)

    def run(self) -> None:
        self.root.mainloop()


def self_test(assets: FaceAssets) -> None:
    rendered = 0
    for state_id, state in enumerate(assets.states):
        for role in ("left", "right"):
            _, pixels = assets.eye.pixels(state_id, role, 0)
            if len(pixels) != assets.eye.width * assets.eye.height:
                raise AssertionError(f"{state} {role} eye has invalid size")
            rendered += 1
        _, pixels = assets.mouth.pixels(state_id, 0, 120, 30)
        if len(pixels) != assets.mouth.width * assets.mouth.height:
            raise AssertionError(f"{state} mouth has invalid size")
        rendered += 1
    print(
        f"OK: rendered {rendered} views for {len(assets.states)} "
        "full-face animations"
    )


def parse_args() -> argparse.Namespace:
    assets = FaceAssets()
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--state", choices=assets.states, default="idle")
    parser.add_argument("--eye-scale", type=int, default=2)
    parser.add_argument("--mouth-scale", type=int, default=12)
    parser.add_argument(
        "--eye-gap", type=int, default=24,
        help="native eye pixels between the round displays (default: 24)",
    )
    parser.add_argument(
        "--face-gap", type=int, default=24,
        help="window pixels between the eyes and mouth (default: 24)",
    )
    parser.add_argument("--interval", type=float, default=3.0)
    parser.add_argument("--brightness", type=int, default=180)
    parser.add_argument("--intensity", type=int, default=120)
    parser.add_argument("--temperature", type=int, default=30)
    parser.add_argument("--paused", action="store_true")
    parser.add_argument(
        "--self-test", action="store_true",
        help="validate combined rendering without opening a window",
    )
    args = parser.parse_args()
    for option in ("eye_scale", "mouth_scale"):
        if getattr(args, option) < 1:
            parser.error(f"--{option.replace('_', '-')} must be at least 1")
    if args.eye_gap < 0 or args.face_gap < 0:
        parser.error("display gaps cannot be negative")
    if args.interval <= 0:
        parser.error("--interval must be greater than zero")
    for option in ("brightness", "intensity"):
        if not 0 <= getattr(args, option) <= 255:
            parser.error(f"--{option} must be in the range 0..255")
    if not TEMPERATURE_MIN <= args.temperature <= TEMPERATURE_MAX:
        parser.error(
            f"--temperature must be in the range "
            f"{TEMPERATURE_MIN}..{TEMPERATURE_MAX}"
        )
    return args


def main() -> int:
    try:
        args = parse_args()
        assets = FaceAssets()
        if args.self_test:
            self_test(assets)
            return 0
        EmulatorWindow(args).run()
    except Exception as error:
        print(f"Unable to run full-face emulator: {error}", file=sys.stderr)
        print("Run with --self-test for headless validation.", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
