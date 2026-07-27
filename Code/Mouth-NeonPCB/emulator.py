#!/usr/bin/env python3
"""Desktop emulator and sprite-editing UI for the SarcasmOS mouth."""

from __future__ import annotations

import argparse
from dataclasses import dataclass
import hashlib
import math
import pathlib
import re
import shutil
import subprocess
import sys
import tempfile
import time
from typing import Any

import asset_tool

FRAME_MS = 40
BLACK = (0, 0, 0)
EDIT_SETTLE_MS = 250
WEATHER_STATES = {"sunny", "rainy", "cloudy", "stormy", "snowy"}
TEMPERATURE_MIN = -127
TEMPERATURE_MAX = 127
TEMPERATURE_FONT = asset_tool.load_temperature_font()
TEMPERATURE_GLYPHS = asset_tool.temperature_glyphs(TEMPERATURE_FONT)
GLYPH_WIDTH = TEMPERATURE_FONT["width"]
GLYPH_HEIGHT = TEMPERATURE_FONT["height"]
GLYPH_SPACING = TEMPERATURE_FONT["spacing"]


def temperature_text(temperature: int) -> str:
    return f"{temperature}°C"


def temperature_origin(
    width: int, height: int, temperature: int,
) -> tuple[int, int, int]:
    glyph_count = len(temperature_text(temperature))
    text_width = (
        glyph_count * GLYPH_WIDTH +
        (glyph_count - 1) * GLYPH_SPACING
    )
    return (width - text_width) // 2, (height - GLYPH_HEIGHT) // 2, text_width


def apply_temperature_overlay(
    pixels: list[tuple[int, int, int]], width: int, height: int,
    temperature: int,
) -> list[tuple[int, int, int]]:
    rendered = pixels.copy()
    x, y, _ = temperature_origin(width, height, temperature)
    for character in temperature_text(temperature):
        rows = TEMPERATURE_GLYPHS[character]
        for row, bits in enumerate(rows):
            for column in range(GLYPH_WIDTH):
                if bits & (1 << (GLYPH_WIDTH - column - 1)):
                    rendered[(y + row) * width + x + column] = BLACK
        x += GLYPH_WIDTH + GLYPH_SPACING
    return rendered


@dataclass
class EditTarget:
    path: pathlib.Path
    state: str
    frame: int
    observed_signature: tuple[int, int]
    changed_at_ms: int | None = None


@dataclass
class AnimationEditSession:
    directory: pathlib.Path
    state: str
    observed_signatures: dict[pathlib.Path, tuple[int, int]]
    changed_at_ms: int | None = None


def numbered_animation_files(
    directory: pathlib.Path, state: str,
) -> list[pathlib.Path]:
    pattern = re.compile(rf"^{re.escape(state)}-frame-(\d+)\.ppm$")
    numbered = []
    for path in directory.iterdir():
        match = pattern.fullmatch(path.name)
        if path.is_file() and match and int(match.group(1)) > 0:
            numbered.append((int(match.group(1)), path.name, path))
    return [entry[2] for entry in sorted(numbered)]


def compact_animation_files(
    files: list[pathlib.Path], state: str,
) -> list[pathlib.Path]:
    """Rename an ordered PPM sequence to contiguous one-based filenames."""
    if not files:
        return []
    directory = files[0].parent
    token = time.monotonic_ns()
    temporary = []
    for index, source in enumerate(files, 1):
        target = directory / f".{state}-renumber-{token}-{index:03d}.ppm"
        source.replace(target)
        temporary.append(target)
    compacted = []
    for index, source in enumerate(temporary, 1):
        target = directory / f"{state}-frame-{index:02d}.ppm"
        source.replace(target)
        compacted.append(target)
    return compacted


class AssetPack:
    def __init__(self) -> None:
        self.reload()

    def reload(self) -> None:
        self.data = asset_tool.load_assets()
        self.width = self.data["width"]
        self.height = self.data["height"]
        self.palette = asset_tool.palette(self.data)
        self.animations = self.data["animations"]
        self.state_ids = {
            animation["name"]: animation["id"]
            for animation in self.animations
        }
        self.sprite_cache = {
            name: asset_tool.decode_sprite(self.data, name)
            for name in self.data["sprites"]
        }

    def local_frame(
        self, state_id: int, elapsed_ms: int, intensity: int,
    ) -> int:
        animation = self.animations[state_id]
        count = len(animation["frames"])
        if count <= 1:
            return 0
        step = elapsed_ms // animation["frame_ms"]
        if animation["playback"] == "ping_pong":
            final_frame = count - 1
            phase = step % (final_frame * 2)
            return phase if phase <= final_frame else final_frame * 2 - phase
        if animation["playback"] == "intensity":
            final_frame = count - 1
            phase = step % (final_frame * 2)
            level = phase if phase <= final_frame else final_frame * 2 - phase
            return min(final_frame, level * intensity // 120)
        return step % count

    def sprite_name(self, state_id: int, local_frame: int) -> str:
        return self.animations[state_id]["frames"][local_frame]

    def pixels(
        self, state_id: int, elapsed_ms: int, intensity: int,
    ) -> tuple[int, list[tuple[int, int, int]]]:
        local_frame = self.local_frame(state_id, elapsed_ms, intensity)
        return local_frame, self.frame_pixels(state_id, local_frame)

    def frame_pixels(
        self, state_id: int, local_frame: int,
    ) -> list[tuple[int, int, int]]:
        sprite = self.sprite_name(state_id, local_frame)
        return [
            self.palette[index] for index in self.sprite_cache[sprite]
        ]


class EmulatorWindow:
    def __init__(self, args: argparse.Namespace) -> None:
        import tkinter as tk

        self.tk = tk
        self.assets = AssetPack()
        self.state_id = self.assets.state_ids[args.state]
        self.scale = args.scale
        self.interval_ms = round(args.interval * 1000)
        self.auto_play = not args.paused
        self.brightness = args.brightness
        self.intensity = args.intensity
        self.temperature = args.temperature
        now_ms = time.monotonic_ns() // 1_000_000
        self.last_step_ms = now_ms
        self.last_update_ms = now_ms
        self.animation_elapsed_ms = 0
        self.last_pixels: list[tuple[int, int, int] | None] = (
            [None] * (self.assets.width * self.assets.height)
        )
        self.current_local_frame = 0
        self.edit_targets: dict[pathlib.Path, EditTarget] = {}
        self.current_edit_path: pathlib.Path | None = None
        self.animation_edit_sessions: dict[str, AnimationEditSession] = {}
        self.current_animation_edit: str | None = None
        self.notice = ""
        self.notice_until_ms = 0

        self.root = tk.Tk()
        self.root.title("SarcasmOS mouth asset emulator")
        self.root.configure(background="#111111")
        self.root.resizable(False, False)

        self.status = tk.StringVar()
        tk.Label(
            self.root, textvariable=self.status, anchor="w",
            background="#111111", foreground="#eeeeee",
            font=("monospace", 11),
        ).pack(fill="x", padx=8, pady=(7, 4))

        self.canvas = tk.Canvas(
            self.root, width=self.assets.width * self.scale,
            height=self.assets.height * self.scale, background="black",
            highlightthickness=0,
        )
        self.canvas.pack(padx=8, pady=(0, 5))
        self.pixel_items = [
            self.canvas.create_rectangle(
                (index % self.assets.width) * self.scale,
                (index // self.assets.width) * self.scale,
                (index % self.assets.width + 1) * self.scale,
                (index // self.assets.width + 1) * self.scale,
                outline="", fill="#000000",
            )
            for index in range(self.assets.width * self.assets.height)
        ]

        buttons = tk.Frame(self.root, background="#111111")
        buttons.pack(fill="x", padx=8, pady=(0, 5))
        for label, action in (
            ("Open frame in GIMP (E)", self.open_in_gimp),
            ("Open animation in GIMP (O)", self.open_animation_in_gimp),
            ("Copy frame (C)", self.copy_frame),
            ("Import saved edit (I)", self.import_edit),
            ("Reload assets (R)", self.reload_assets),
        ):
            tk.Button(buttons, text=label, command=action).pack(
                side="left", padx=(0, 5),
            )

        tk.Label(
            self.root,
            text=(
                "←/→ state   ↑/↓ frame   Space/A play/pause   "
                "+/- brightness   [/] intensity   ,/. temperature   Q quit"
            ),
            background="#111111", foreground="#aaaaaa",
            font=("monospace", 9),
        ).pack(padx=8, pady=(0, 7))
        self.root.bind_all("<KeyPress>", self.on_key)
        self.root.after(0, self.update)

    @property
    def state_name(self) -> str:
        return self.assets.animations[self.state_id]["name"]

    def show_notice(self, message: str) -> None:
        self.notice = message
        self.notice_until_ms = time.monotonic_ns() // 1_000_000 + 4000

    def select(self, state_id: int, pause: bool = True) -> None:
        self.state_id = state_id % len(self.assets.animations)
        if pause:
            self.auto_play = False
        self.current_local_frame = 0
        self.animation_elapsed_ms = 0
        self.last_step_ms = time.monotonic_ns() // 1_000_000

    def current_source_pixels(self) -> list[tuple[int, int, int]]:
        sprite = self.assets.sprite_name(
            self.state_id, self.current_local_frame,
        )
        return [
            self.assets.palette[index]
            for index in self.assets.sprite_cache[sprite]
        ]

    def native_ppm(
        self, state_id: int | None = None, local_frame: int | None = None,
    ) -> bytes:
        if state_id is None:
            state_id = self.state_id
        if local_frame is None:
            local_frame = self.current_local_frame
        pixels = self.assets.frame_pixels(state_id, local_frame)
        body = bytearray(channel for pixel in pixels for channel in pixel)
        return (
            f"P6\n{self.assets.width} {self.assets.height}\n255\n".encode() +
            body
        )

    @staticmethod
    def file_signature(path: pathlib.Path) -> tuple[int, int]:
        stat = path.stat()
        return stat.st_mtime_ns, stat.st_size

    @staticmethod
    def edit_root() -> pathlib.Path:
        return pathlib.Path(tempfile.gettempdir()) / "sarcasmos-mouth-edit"

    def export_edit_target(
        self, state_id: int, local_frame: int,
    ) -> pathlib.Path:
        edit_dir = self.edit_root() / "frames"
        edit_dir.mkdir(parents=True, exist_ok=True)
        state = self.assets.animations[state_id]["name"]
        path = edit_dir / f"{state}-frame-{local_frame + 1:02d}.ppm"
        path.write_bytes(self.native_ppm(state_id, local_frame))
        self.edit_targets[path] = EditTarget(
            path=path,
            state=state,
            frame=local_frame,
            observed_signature=self.file_signature(path),
        )
        self.current_edit_path = path
        self.current_animation_edit = None
        return path

    def launch_gimp(self, paths: list[pathlib.Path]) -> bool:
        if shutil.which("gimp") is None:
            self.show_notice("GIMP is not installed")
            return False
        subprocess.Popen(
            ["gimp", *(str(path) for path in paths)],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
        return True

    def open_in_gimp(self) -> None:
        path = self.export_edit_target(
            self.state_id, self.current_local_frame,
        )
        if not self.launch_gimp([path]):
            return
        self.show_notice(
            f"Opened {path.name}; overwrites import automatically"
        )

    def open_animation_in_gimp(self) -> None:
        animation = self.assets.animations[self.state_id]
        state = self.state_name
        edit_dir = self.edit_root() / "animations" / state
        edit_dir.mkdir(parents=True, exist_ok=True)
        for old_path in edit_dir.glob(f"{state}-frame-*.ppm"):
            if old_path.is_file():
                old_path.unlink()
        paths = []
        for local_frame in range(len(animation["frames"])):
            path = edit_dir / f"{state}-frame-{local_frame + 1:02d}.ppm"
            path.write_bytes(self.native_ppm(self.state_id, local_frame))
            paths.append(path)
        self.animation_edit_sessions[state] = AnimationEditSession(
            directory=edit_dir,
            state=state,
            observed_signatures={
                path: self.file_signature(path) for path in paths
            },
        )
        self.current_edit_path = None
        self.current_animation_edit = state
        if not self.launch_gimp(paths):
            return
        self.show_notice(
            f"Opened {len(paths)} {self.state_name} frames; "
            "add/delete numbered PPMs to change the sequence"
        )

    def copy_frame(self) -> None:
        if shutil.which("wl-copy") is None or shutil.which("magick") is None:
            self.show_notice("Image clipboard requires wl-copy and ImageMagick")
            return
        converted = subprocess.run(
            ["magick", "ppm:-", "png:-"], input=self.native_ppm(),
            capture_output=True, check=False,
        )
        if converted.returncode != 0:
            self.show_notice("ImageMagick could not encode the clipboard image")
            return
        try:
            copied = subprocess.run(
                ["wl-copy", "--type", "image/png"], input=converted.stdout,
                stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                check=False, timeout=2,
            )
        except subprocess.TimeoutExpired:
            self.show_notice("wl-copy timed out while setting the clipboard")
            return
        if copied.returncode == 0:
            self.show_notice(
                f"Copied native 64x32 {self.state_name} frame to clipboard"
            )
        else:
            self.show_notice("wl-copy could not access the image clipboard")

    def import_edit(self) -> None:
        if self.current_animation_edit is not None:
            session = self.animation_edit_sessions.get(
                self.current_animation_edit,
            )
            if session is None:
                self.show_notice("The current animation edit is not tracked")
                return
            self.sync_animation_session(session, automatic=False)
            return
        if self.current_edit_path is None:
            self.show_notice("Open a frame in GIMP before importing")
            return
        target = self.edit_targets.get(self.current_edit_path)
        if target is None:
            self.show_notice("The current GIMP edit is no longer tracked")
            return
        self.import_target(target, automatic=False)

    def import_target(self, target: EditTarget, automatic: bool) -> bool:
        try:
            asset_tool.import_sprite(
                target.state, target.frame, target.path,
            )
            current_name = self.state_name
            self.assets.reload()
            self.state_id = self.assets.state_ids[current_name]
            self.last_pixels[:] = [None] * len(self.last_pixels)
            target.observed_signature = self.file_signature(target.path)
            target.changed_at_ms = None
            prefix = "Auto-imported" if automatic else "Imported"
            self.show_notice(
                f"{prefix} {target.state} frame {target.frame + 1}"
            )
            return True
        except (OSError, ValueError, KeyError) as error:
            self.show_notice(f"Import failed: {error}")
            return False

    def sync_animation_session(
        self, session: AnimationEditSession, automatic: bool,
    ) -> bool:
        try:
            paths = numbered_animation_files(
                session.directory, session.state,
            )
            count = asset_tool.sync_animation_frames(
                session.state, paths,
            )
            compacted = compact_animation_files(paths, session.state)
            session.observed_signatures = {
                path: self.file_signature(path) for path in compacted
            }
            session.changed_at_ms = None

            current_name = self.state_name
            self.assets.reload()
            self.state_id = self.assets.state_ids[current_name]
            frame_count = len(
                self.assets.animations[self.state_id]["frames"]
            )
            self.current_local_frame = min(
                self.current_local_frame, frame_count - 1,
            )
            self.last_pixels[:] = [None] * len(self.last_pixels)
            prefix = "Auto-synchronized" if automatic else "Synchronized"
            self.show_notice(
                f"{prefix} {session.state}: {count} frame(s)"
            )
            return True
        except (OSError, ValueError, KeyError) as error:
            session.changed_at_ms = (
                time.monotonic_ns() // 1_000_000
            )
            self.show_notice(f"Animation sync failed: {error}")
            return False

    def watch_animation_sessions(self, now_ms: int) -> None:
        for session in self.animation_edit_sessions.values():
            try:
                paths = numbered_animation_files(
                    session.directory, session.state,
                )
                signatures = {
                    path: self.file_signature(path) for path in paths
                }
            except OSError:
                continue
            if signatures != session.observed_signatures:
                session.observed_signatures = signatures
                session.changed_at_ms = now_ms
                continue
            if (session.changed_at_ms is not None and
                    now_ms - session.changed_at_ms >= EDIT_SETTLE_MS):
                self.sync_animation_session(session, automatic=True)

    def watch_gimp_edits(self, now_ms: int) -> None:
        for target in self.edit_targets.values():
            try:
                signature = self.file_signature(target.path)
            except OSError:
                continue
            if signature != target.observed_signature:
                target.observed_signature = signature
                target.changed_at_ms = now_ms
                continue
            if (target.changed_at_ms is not None and
                    now_ms - target.changed_at_ms >= EDIT_SETTLE_MS):
                self.import_target(target, automatic=True)
        self.watch_animation_sessions(now_ms)

    def reload_assets(self) -> None:
        current_name = self.state_name
        try:
            self.assets.reload()
            self.state_id = self.assets.state_ids[current_name]
            self.last_pixels[:] = [None] * len(self.last_pixels)
            self.show_notice("Reloaded assets/mouth_assets.json")
        except (OSError, ValueError, KeyError) as error:
            self.show_notice(f"Reload failed: {error}")

    def on_key(self, event: Any) -> None:
        key = event.keysym
        if key == "Left":
            self.select(self.state_id - 1)
        elif key == "Right":
            self.select(self.state_id + 1)
        elif key in ("Up", "Down"):
            self.auto_play = False
            animation = self.assets.animations[self.state_id]
            direction = 1 if key == "Up" else -1
            self.current_local_frame = (
                self.current_local_frame + direction
            ) % len(animation["frames"])
            self.animation_elapsed_ms = (
                self.current_local_frame * animation["frame_ms"]
            )
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
        elif key in ("period", "greater"):
            self.temperature = min(
                TEMPERATURE_MAX, self.temperature + 1,
            )
        elif key in ("comma", "less"):
            self.temperature = max(
                TEMPERATURE_MIN, self.temperature - 1,
            )
        elif key in ("e", "E"):
            self.open_in_gimp()
        elif key in ("o", "O"):
            self.open_animation_in_gimp()
        elif key in ("c", "C"):
            self.copy_frame()
        elif key in ("i", "I"):
            self.import_edit()
        elif key in ("r", "R"):
            self.reload_assets()
        elif key in ("q", "Q", "Escape"):
            self.root.destroy()

    def update(self) -> None:
        now_ms = time.monotonic_ns() // 1_000_000
        delta_ms = max(0, now_ms - self.last_update_ms)
        self.last_update_ms = now_ms
        self.watch_gimp_edits(now_ms)

        if self.auto_play:
            self.animation_elapsed_ms += delta_ms
        if self.auto_play and now_ms - self.last_step_ms >= self.interval_ms:
            self.select(self.state_id + 1, pause=False)

        if self.auto_play:
            self.current_local_frame, pixels = self.assets.pixels(
                self.state_id, self.animation_elapsed_ms, self.intensity,
            )
        else:
            pixels = self.assets.frame_pixels(
                self.state_id, self.current_local_frame,
            )
        if self.state_name in WEATHER_STATES:
            pixels = apply_temperature_overlay(
                pixels, self.assets.width, self.assets.height,
                self.temperature,
            )
        factor = math.sqrt(self.brightness / 255)
        for index, color in enumerate(pixels):
            scaled = tuple(round(channel * factor) for channel in color)
            if scaled != self.last_pixels[index]:
                self.canvas.itemconfigure(
                    self.pixel_items[index],
                    fill=f"#{scaled[0]:02x}{scaled[1]:02x}{scaled[2]:02x}",
                )
                self.last_pixels[index] = scaled

        animation = self.assets.animations[self.state_id]
        mode = "AUTO" if self.auto_play else "PAUSED"
        status = (
            f"0x{self.state_id:02x}  {self.state_name:<16} "
            f"frame {self.current_local_frame + 1}/{len(animation['frames'])} "
            f"{mode}   brightness {self.brightness:3}/255   "
            f"intensity {self.intensity:3}/255"
        )
        if self.state_name in WEATHER_STATES:
            status += f"   temperature {temperature_text(self.temperature)}"
        if now_ms < self.notice_until_ms:
            status = self.notice
        self.status.set(status)
        self.root.after(FRAME_MS, self.update)

    def run(self) -> None:
        self.root.mainloop()


def self_test(assets: AssetPack) -> None:
    hashes = set()
    for state_id, animation in enumerate(assets.animations):
        lit_frames = 0
        for local_frame, sprite in enumerate(animation["frames"]):
            pixels = [
                assets.palette[index]
                for index in assets.sprite_cache[sprite]
            ]
            if len(pixels) != assets.width * assets.height:
                raise AssertionError(
                    f"{animation['name']} frame {local_frame}: bad size"
                )
            lit = sum(pixel != BLACK for pixel in pixels)
            lit_frames += lit > 0
            if animation["name"] == "asleep" and lit != 0:
                raise AssertionError("asleep must be blank")
            raw = bytes(channel for pixel in pixels for channel in pixel)
            hashes.add(hashlib.sha256(raw).hexdigest())
        if animation["name"] != "asleep" and lit_frames == 0:
            raise AssertionError(f"{animation['name']}: every frame is blank")
    common_origins = {
        temperature_origin(assets.width, assets.height, value)[0]
        for value in (30, -5)
    }
    if len(common_origins) != 1:
        raise AssertionError("two-character temperatures must share a center")
    for value in (30, -5, -10, TEMPERATURE_MIN, TEMPERATURE_MAX):
        x, y, text_width = temperature_origin(
            assets.width, assets.height, value,
        )
        if x < 0 or y < 0 or x + text_width > assets.width:
            raise AssertionError(f"{value}°C does not fit the display")
    asset_tool.compile_assets()
    asset_tool.compile_temperature_font()
    print(
        f"OK: rendered {len(assets.animations)} animations and "
        f"{len(assets.data['sprites'])} native 64x32 sprites"
    )
    print(f"OK: {len(hashes)} distinct sprite frames")


def parse_args() -> argparse.Namespace:
    assets = AssetPack()
    states = tuple(animation["name"] for animation in assets.animations)
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--state", choices=states, default="idle")
    parser.add_argument("--scale", type=int, default=12)
    parser.add_argument("--interval", type=float, default=2.5)
    parser.add_argument("--brightness", type=int, default=64)
    parser.add_argument("--intensity", type=int, default=120)
    parser.add_argument(
        "--temperature", type=int, default=30,
        help="test temperature shown over weather states (default: 30)",
    )
    parser.add_argument("--paused", action="store_true")
    parser.add_argument(
        "--self-test", action="store_true",
        help="validate every asset without opening a window",
    )
    parser.add_argument(
        "--dump", type=pathlib.Path,
        help="write the selected state's first native sprite as PPM and exit",
    )
    args = parser.parse_args()
    if args.scale < 2:
        parser.error("--scale must be at least 2")
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
    args = parse_args()
    assets = AssetPack()
    if args.self_test:
        self_test(assets)
        return 0
    if args.dump is not None:
        pixels = assets.frame_pixels(assets.state_ids[args.state], 0)
        if args.state in WEATHER_STATES:
            pixels = apply_temperature_overlay(
                pixels, assets.width, assets.height, args.temperature,
            )
        factor = math.sqrt(args.brightness / 255)
        pixels = [
            tuple(round(channel * factor) for channel in color)
            for color in pixels
        ]
        asset_tool.write_ppm(
            args.dump, assets.width, assets.height, pixels,
        )
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
