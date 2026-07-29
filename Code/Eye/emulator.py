#!/usr/bin/env python3
"""Desktop emulator and sprite-editing UI for the SarcasmOS round eyes."""

from __future__ import annotations

import argparse
from collections import Counter
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


@dataclass
class EditTarget:
    path: pathlib.Path
    state: str
    role: str
    frame: int
    observed_signature: tuple[int, int]
    changed_at_ms: int | None = None


@dataclass
class AnimationEditSession:
    directory: pathlib.Path
    state: str
    role: str
    observed_signatures: dict[pathlib.Path, tuple[int, int]]
    changed_at_ms: int | None = None


def numbered_animation_files(
    directory: pathlib.Path, state: str, role: str,
) -> list[pathlib.Path]:
    pattern = re.compile(
        rf"^{re.escape(state)}-{re.escape(role)}-frame-(\d+)\.ppm$"
    )
    numbered = []
    for path in directory.iterdir():
        match = pattern.fullmatch(path.name)
        if path.is_file() and match and int(match.group(1)) > 0:
            numbered.append((int(match.group(1)), path.name, path))
    return [entry[2] for entry in sorted(numbered)]


def compact_animation_files(
    files: list[pathlib.Path], state: str, role: str,
) -> list[pathlib.Path]:
    if not files:
        return []
    directory = files[0].parent
    token = time.monotonic_ns()
    temporary = []
    for index, source in enumerate(files, 1):
        target = directory / f".{state}-{role}-renumber-{token}-{index}.ppm"
        source.replace(target)
        temporary.append(target)
    compacted = []
    for index, source in enumerate(temporary, 1):
        target = directory / f"{state}-{role}-frame-{index:02d}.ppm"
        source.replace(target)
        compacted.append(target)
    return compacted


class AssetPack:
    def __init__(self, path: pathlib.Path = asset_tool.DEFAULT_ASSETS) -> None:
        self.path = path.resolve()
        self.is_production = asset_tool.is_default_asset_path(self.path)
        self.reload()

    def reload(self) -> None:
        self.data = asset_tool.load_assets(self.path)
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

    def sprite_name(self, state_id: int, role: str, frame: int) -> str:
        animation = self.animations[state_id]
        source_role = (
            "right" if role == "left" else "left"
        ) if animation[f"flip_{role}"] else role
        return animation["frames"][frame][source_role]

    def frame_pixels(
        self, state_id: int, role: str, frame: int,
    ) -> list[tuple[int, int, int]]:
        sprite = self.sprite_name(state_id, role, frame)
        return [
            self.palette[index] for index in self.sprite_cache[sprite]
        ]


class EmulatorWindow:
    def __init__(self, args: argparse.Namespace) -> None:
        import tkinter as tk
        from tkinter import simpledialog

        self.tk = tk
        self.simpledialog = simpledialog
        self.assets = AssetPack(args.assets)
        self.state_id = self.assets.state_ids[args.state]
        self.view = args.view
        self.scale = args.scale
        self.gap = args.gap
        self.interval_ms = round(args.interval * 1000)
        self.auto_play = not args.paused
        self.auto_scroll = False
        self.brightness = args.brightness
        now_ms = time.monotonic_ns() // 1_000_000
        self.last_step_ms = now_ms
        self.last_update_ms = now_ms
        self.animation_elapsed_ms = 0
        self.current_local_frame = 0
        self.edit_targets: dict[pathlib.Path, EditTarget] = {}
        self.current_edit_path: pathlib.Path | None = None
        self.animation_edit_sessions: dict[
            tuple[str, str], AnimationEditSession
        ] = {}
        self.current_animation_edit: tuple[str, str] | None = None
        self.notice = ""
        self.notice_until_ms = 0
        self.last_render_key: tuple[Any, ...] | None = None
        self.photo_native: list[Any] = []
        self.photo_scaled: list[Any] = []
        self.timeline_window: Any | None = None

        self.root = tk.Tk()
        self.root.title(
            "SarcasmOS 240x240 round-eye asset emulator — "
            f"{self.assets.path.name}"
        )
        self.root.configure(background="#111111")
        self.root.resizable(False, False)

        self.status = tk.StringVar()
        tk.Label(
            self.root, textvariable=self.status, anchor="w",
            background="#111111", foreground="#eeeeee",
            font=("monospace", 11),
        ).pack(fill="x", padx=8, pady=(7, 4))

        size = self.assets.width * self.scale
        canvas_width = size * (2 if self.view == "both" else 1)
        if self.view == "both":
            canvas_width += self.gap * self.scale
        self.canvas = tk.Canvas(
            self.root, width=canvas_width + 16, height=size + 16,
            background="#20252a", highlightthickness=0,
        )
        self.canvas.pack(padx=8, pady=(0, 5))

        buttons = tk.Frame(self.root, background="#111111")
        buttons.pack(fill="x", padx=8, pady=(0, 5))
        for label, view in (
            ("Left eye", "left"),
            ("Both eyes", "both"),
            ("Right eye", "right"),
        ):
            tk.Button(
                buttons, text=label,
                command=lambda selected=view: self.set_view(selected),
            ).pack(side="left", padx=(0, 5))
        for label, action in (
            ("Open frame in GIMP (E)", self.open_in_gimp),
            ("Open animation in GIMP (O)", self.open_animation_in_gimp),
            ("Copy frame (C)", self.copy_frame),
            ("Reload assets (R)", self.reload_assets),
        ):
            tk.Button(buttons, text=label, command=action).pack(
                side="left", padx=(0, 5),
            )

        frame_buttons = tk.Frame(self.root, background="#111111")
        frame_buttons.pack(fill="x", padx=8, pady=(0, 5))
        for label, action in (
            ("Frame timeline (F)", self.open_timeline_editor),
            ("Add frame (Insert)", self.add_frame),
            ("Remove frame (Delete)", self.remove_frame),
            ("Set frame time (T)", self.change_frame_time),
            ("Sync folder (S)", self.sync_current_animation_folder),
        ):
            tk.Button(frame_buttons, text=label, command=action).pack(
                side="left", padx=(0, 5),
            )

        flip_buttons = tk.Frame(self.root, background="#111111")
        flip_buttons.pack(fill="x", padx=8, pady=(0, 5))
        tk.Label(
            flip_buttons, text="Preview orientation:",
            background="#111111", foreground="#aaaaaa",
        ).pack(side="left", padx=(0, 5))
        for label, role in (
            ("Flip left eye", "left"),
            ("Flip right eye", "right"),
        ):
            tk.Button(
                flip_buttons, text=label,
                command=lambda selected=role: self.toggle_flip(selected),
            ).pack(side="left", padx=(0, 5))

        tk.Label(
            self.root,
            text=(
                "←/→ state   ↑/↓ frame   Tab left/both/right view   "
                "Space frames only   A full auto-scroll\n"
                "F frame timeline   Insert/Delete frame   T frame time   "
                "S sync folder   "
                "+/- brightness   Q quit"
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
            self.auto_scroll = False
        self.current_local_frame = 0
        self.animation_elapsed_ms = 0
        self.last_step_ms = time.monotonic_ns() // 1_000_000
        self.last_render_key = None

    @property
    def edit_role(self) -> str:
        return self.view if self.view in asset_tool.ROLES else "left"

    def set_view(self, view: str) -> None:
        self.view = view
        self.current_edit_path = None
        self.current_animation_edit = None
        self.last_render_key = None
        size = self.assets.width * self.scale
        width = size * (2 if view == "both" else 1)
        if view == "both":
            width += self.gap * self.scale
        self.canvas.configure(width=width + 16, height=size + 16)

    def switch_view(self) -> None:
        views = ("left", "both", "right")
        self.set_view(views[(views.index(self.view) + 1) % len(views)])

    def toggle_flip(self, role: str) -> None:
        animation = self.assets.animations[self.state_id]
        key = f"flip_{role}"
        animation[key] = not animation[key]
        try:
            asset_tool.save_asset_source(
                self.assets.data, self.assets.path,
            )
            self.last_render_key = None
            state = "flipped" if animation[key] else "normal"
            self.show_notice(f"{role.title()} eye orientation: {state}")
        except (OSError, ValueError, KeyError) as error:
            animation[key] = not animation[key]
            self.show_notice(f"Could not save eye orientation: {error}")

    def native_ppm(
        self, state_id: int | None = None, frame: int | None = None,
        role: str | None = None,
    ) -> bytes:
        state_id = self.state_id if state_id is None else state_id
        frame = self.current_local_frame if frame is None else frame
        role = self.edit_role if role is None else role
        pixels = self.assets.frame_pixels(state_id, role, frame)
        body = bytearray(channel for pixel in pixels for channel in pixel)
        return (
            f"P6\n{self.assets.width} {self.assets.height}\n255\n".encode() +
            body
        )

    @staticmethod
    def file_signature(path: pathlib.Path) -> tuple[int, int]:
        stat = path.stat()
        return stat.st_mtime_ns, stat.st_size

    def edit_root(self) -> pathlib.Path:
        pack_token = hashlib.sha256(
            str(self.assets.path).encode()
        ).hexdigest()[:10]
        return (
            pathlib.Path(tempfile.gettempdir()) / "sarcasmos-eye-edit" /
            f"{self.assets.path.stem}-{pack_token}"
        )

    def export_edit_target(self) -> pathlib.Path:
        edit_dir = self.edit_root() / "frames"
        edit_dir.mkdir(parents=True, exist_ok=True)
        path = edit_dir / (
            f"{self.state_name}-{self.edit_role}-"
            f"frame-{self.current_local_frame + 1:02d}.ppm"
        )
        path.write_bytes(self.native_ppm())
        self.edit_targets[path] = EditTarget(
            path=path, state=self.state_name, role=self.edit_role,
            frame=self.current_local_frame,
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
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
        )
        return True

    def open_in_gimp(self) -> None:
        path = self.export_edit_target()
        if self.launch_gimp([path]):
            self.show_notice(
                f"Opened {path.name}; saved overwrites import automatically"
            )

    def open_animation_in_gimp(self) -> None:
        state, role = self.state_name, self.edit_role
        animation = self.assets.animations[self.state_id]
        edit_dir = self.edit_root() / "animations" / state / role
        edit_dir.mkdir(parents=True, exist_ok=True)
        for old in edit_dir.glob(f"{state}-{role}-frame-*.ppm"):
            old.unlink()
        paths = []
        for frame in range(len(animation["frames"])):
            path = edit_dir / f"{state}-{role}-frame-{frame + 1:02d}.ppm"
            path.write_bytes(self.native_ppm(frame=frame))
            paths.append(path)
        key = (state, role)
        self.animation_edit_sessions[key] = AnimationEditSession(
            directory=edit_dir, state=state, role=role,
            observed_signatures={
                path: self.file_signature(path) for path in paths
            },
        )
        self.current_animation_edit = key
        self.current_edit_path = None
        if self.launch_gimp(paths):
            self.show_notice(
                f"Opened {len(paths)} {state} {role}-eye frames"
            )

    def copy_frame(self) -> None:
        if shutil.which("wl-copy") is None or shutil.which("magick") is None:
            self.show_notice("Clipboard requires wl-copy and ImageMagick")
            return
        converted = subprocess.run(
            ["magick", "ppm:-", "png:-"], input=self.native_ppm(),
            capture_output=True, check=False,
        )
        if converted.returncode != 0:
            self.show_notice("ImageMagick could not encode the frame")
            return
        try:
            result = subprocess.run(
                ["wl-copy", "--type", "image/png"],
                input=converted.stdout, timeout=2, check=False,
            )
        except subprocess.TimeoutExpired:
            self.show_notice("wl-copy timed out")
            return
        self.show_notice(
            "Copied native 240x240 eye frame"
            if result.returncode == 0 else "wl-copy failed"
        )

    def import_target(self, target: EditTarget, automatic: bool) -> bool:
        try:
            asset_tool.import_sprite(
                target.state, target.role, target.frame, target.path,
                assets_path=self.assets.path,
            )
            current_state = self.state_name
            self.assets.reload()
            self.state_id = self.assets.state_ids[current_state]
            target.observed_signature = self.file_signature(target.path)
            target.changed_at_ms = None
            self.last_render_key = None
            prefix = "Auto-imported" if automatic else "Imported"
            self.show_notice(
                f"{prefix} {target.state} {target.role} "
                f"frame {target.frame + 1}"
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
                session.directory, session.state, session.role,
            )
            count = asset_tool.sync_animation_frames(
                session.state, session.role, paths,
                assets_path=self.assets.path,
            )
            compacted = compact_animation_files(
                paths, session.state, session.role,
            )
            session.observed_signatures = {
                path: self.file_signature(path) for path in compacted
            }
            session.changed_at_ms = None
            current_state = self.state_name
            self.assets.reload()
            self.state_id = self.assets.state_ids[current_state]
            self.current_local_frame = min(
                self.current_local_frame,
                len(self.assets.animations[self.state_id]["frames"]) - 1,
            )
            self.last_render_key = None
            prefix = "Auto-synchronized" if automatic else "Synchronized"
            self.show_notice(
                f"{prefix} {session.state} {session.role}: {count} frames"
            )
            return True
        except (OSError, ValueError, KeyError) as error:
            session.changed_at_ms = time.monotonic_ns() // 1_000_000
            self.show_notice(f"Animation sync failed: {error}")
            return False

    def watch_edits(self, now_ms: int) -> None:
        for target in self.edit_targets.values():
            try:
                signature = self.file_signature(target.path)
            except OSError:
                continue
            if signature != target.observed_signature:
                target.observed_signature = signature
                target.changed_at_ms = now_ms
            elif (target.changed_at_ms is not None and
                  now_ms - target.changed_at_ms >= EDIT_SETTLE_MS):
                self.import_target(target, automatic=True)

        for session in self.animation_edit_sessions.values():
            try:
                paths = numbered_animation_files(
                    session.directory, session.state, session.role,
                )
                signatures = {
                    path: self.file_signature(path) for path in paths
                }
            except OSError:
                continue
            if signatures != session.observed_signatures:
                session.observed_signatures = signatures
                session.changed_at_ms = now_ms
            elif (session.changed_at_ms is not None and
                  now_ms - session.changed_at_ms >= EDIT_SETTLE_MS):
                self.sync_animation_session(session, automatic=True)

    def reload_assets(self) -> None:
        state = self.state_name
        try:
            self.assets.reload()
            self.state_id = self.assets.state_ids[state]
            self.last_render_key = None
            self.show_notice(f"Reloaded {self.assets.path}")
        except (OSError, ValueError, KeyError) as error:
            self.show_notice(f"Reload failed: {error}")

    def invalidate_edits(self, state: str) -> None:
        for key in [key for key in self.animation_edit_sessions if key[0] == state]:
            del self.animation_edit_sessions[key]
        self.current_animation_edit = None
        for path in [
            path for path, target in self.edit_targets.items()
            if target.state == state
        ]:
            del self.edit_targets[path]
        self.current_edit_path = None

    def reload_after_frame_change(self, state: str, selected: int) -> None:
        self.invalidate_edits(state)
        self.assets.reload()
        self.state_id = self.assets.state_ids[state]
        self.auto_play = False
        self.auto_scroll = False
        self.current_local_frame = selected
        self.animation_elapsed_ms = (
            selected * self.assets.animations[self.state_id]["frame_ms"]
        )
        self.last_render_key = None

    def add_frame(self) -> None:
        state = self.state_name
        try:
            inserted = asset_tool.insert_animation_frame(
                state, self.current_local_frame,
                assets_path=self.assets.path,
            )
            self.reload_after_frame_change(state, inserted)
            self.show_notice(f"Added paired frame {inserted + 1} to {state}")
        except (OSError, ValueError, KeyError) as error:
            self.show_notice(f"Could not add frame: {error}")

    def remove_frame(self) -> None:
        state = self.state_name
        try:
            selected = asset_tool.remove_animation_frame(
                state, self.current_local_frame,
                assets_path=self.assets.path,
            )
            self.reload_after_frame_change(state, selected)
            self.show_notice(f"Removed paired frame from {state}")
        except (OSError, ValueError, KeyError) as error:
            self.show_notice(f"Could not remove frame: {error}")

    def change_frame_time(self) -> None:
        state = self.state_name
        animation = self.assets.animations[self.state_id]
        value = self.simpledialog.askinteger(
            "Set animation frame time",
            f"Delay between {state} frames in milliseconds:",
            parent=self.root, initialvalue=animation["frame_ms"],
            minvalue=1, maxvalue=65535,
        )
        if value is None:
            return
        try:
            asset_tool.set_animation_frame_ms(
                state, value, assets_path=self.assets.path,
            )
            self.reload_assets()
            self.show_notice(f"{state} frame time: {value} ms")
        except (OSError, ValueError, KeyError) as error:
            self.show_notice(f"Could not set frame time: {error}")

    def open_timeline_editor(self) -> None:
        if (self.timeline_window is not None and
                self.timeline_window.winfo_exists()):
            self.timeline_window.lift()
            self.timeline_window.focus_force()
            return

        state = self.state_name
        animation = self.assets.animations[self.state_id]
        frames = [dict(frame) for frame in animation["frames"]]
        tk = self.tk
        window = tk.Toplevel(self.root)
        self.timeline_window = window
        window.title(f"Frame timeline — {state}")
        window.configure(background="#111111")
        window.geometry("1280x560")
        window.minsize(1080, 440)
        window.transient(self.root)

        tk.Label(
            window,
            text=(
                "Each row is a lightweight reference to stored left/right "
                "art. Select one or more rows to reorder or repeat them."
            ),
            anchor="w", background="#111111", foreground="#eeeeee",
        ).pack(fill="x", padx=10, pady=(10, 5))

        body = tk.Frame(window, background="#111111")
        body.pack(fill="both", expand=True, padx=10)
        list_frame = tk.Frame(body, background="#111111")
        list_frame.pack(side="left", fill="both", expand=True)
        scrollbar = tk.Scrollbar(list_frame, orient="vertical")
        timeline = tk.Listbox(
            list_frame, selectmode=tk.EXTENDED, exportselection=False,
            yscrollcommand=scrollbar.set, font=("monospace", 10),
            background="#20252a", foreground="#eeeeee",
            selectbackground="#315b7d", activestyle="dotbox",
        )
        scrollbar.configure(command=timeline.yview)
        scrollbar.pack(side="right", fill="y")
        timeline.pack(side="left", fill="both", expand=True)

        controls = tk.Frame(body, background="#111111")
        controls.pack(side="left", fill="y", padx=(10, 0))
        status = tk.StringVar()
        playback = tk.StringVar(value=animation["playback"])

        preview_panel = tk.Frame(body, background="#171b1f")
        preview_panel.pack(side="left", fill="y", padx=(10, 0))
        tk.Label(
            preview_panel, text="Animation preview", anchor="w",
            background="#171b1f", foreground="#eeeeee",
            font=("sans", 11, "bold"),
        ).pack(fill="x", padx=8, pady=(8, 4))
        preview_canvas = tk.Canvas(
            preview_panel, width=264, height=136,
            background="#20252a", highlightthickness=0,
        )
        preview_canvas.pack(padx=8)
        preview_status = tk.StringVar()
        tk.Label(
            preview_panel, textvariable=preview_status, anchor="center",
            background="#171b1f", foreground="#aaaaaa",
            font=("monospace", 9),
        ).pack(fill="x", padx=8, pady=(4, 2))
        preview_buttons = tk.Frame(preview_panel, background="#171b1f")
        preview_buttons.pack(fill="x", padx=8, pady=(2, 8))
        preview_button_text = tk.StringVar(value="Play")
        preview_index = min(self.current_local_frame, len(frames) - 1)
        preview_direction = 1
        preview_playing = False
        preview_last_ms = time.monotonic_ns() // 1_000_000
        selecting_from_preview = False
        preview_photos: list[Any] = []

        def render_preview() -> None:
            preview_canvas.delete("preview")
            preview_photos.clear()
            frame = frames[preview_index]
            factor = math.sqrt(self.brightness / 255)
            for eye_index, role in enumerate(asset_tool.ROLES):
                source_role = (
                    "right" if role == "left" else "left"
                ) if animation[f"flip_{role}"] else role
                sprite = frame[source_role]
                pixels = [
                    tuple(round(channel * factor) for channel in color)
                    for color in (
                        self.assets.palette[value]
                        for value in self.assets.sprite_cache[sprite]
                    )
                ]
                body = bytearray(
                    channel for color in pixels for channel in color
                )
                ppm = (
                    f"P6\n{self.assets.width} {self.assets.height}\n255\n"
                    .encode() + body
                )
                native = tk.PhotoImage(data=ppm, format="PPM")
                photo = native.subsample(2, 2)
                x = 8 + eye_index * 128
                preview_canvas.create_image(
                    x, 8, anchor="nw", image=photo, tags="preview",
                )
                preview_canvas.create_oval(
                    x, 8, x + 120, 128, outline="#8b949e",
                    tags="preview",
                )
                preview_photos.extend((native, photo))
            preview_status.set(
                f"frame {preview_index + 1}/{len(frames)} · "
                f"{animation['frame_ms']} ms"
            )

        def show_preview(index: int, select_row: bool = False) -> None:
            nonlocal preview_index, selecting_from_preview
            preview_index = index % len(frames)
            timeline.activate(preview_index)
            timeline.see(preview_index)
            if select_row:
                selecting_from_preview = True
                timeline.selection_clear(0, tk.END)
                timeline.selection_set(preview_index)
                selecting_from_preview = False
            render_preview()

        def stop_preview() -> None:
            nonlocal preview_playing
            preview_playing = False
            preview_button_text.set("Play")

        def step_preview(delta: int) -> str:
            nonlocal preview_direction
            stop_preview()
            preview_direction = 1 if delta > 0 else -1
            show_preview(preview_index + delta, select_row=True)
            return "break"

        def toggle_preview() -> str:
            nonlocal preview_playing, preview_last_ms
            preview_playing = not preview_playing
            preview_button_text.set("Pause" if preview_playing else "Play")
            preview_last_ms = time.monotonic_ns() // 1_000_000
            return "break"

        def preview_tick() -> None:
            nonlocal preview_direction, preview_last_ms
            if not window.winfo_exists():
                return
            now_ms = time.monotonic_ns() // 1_000_000
            elapsed_ms = now_ms - preview_last_ms
            if preview_playing and elapsed_ms >= animation["frame_ms"]:
                steps = elapsed_ms // animation["frame_ms"]
                preview_last_ms += steps * animation["frame_ms"]
                for _ in range(steps):
                    if len(frames) <= 1:
                        break
                    if playback.get() == "ping_pong":
                        next_index = preview_index + preview_direction
                        if next_index >= len(frames):
                            preview_direction = -1
                            next_index = len(frames) - 2
                        elif next_index < 0:
                            preview_direction = 1
                            next_index = 1
                    else:
                        next_index = (preview_index + 1) % len(frames)
                    show_preview(next_index)
            window.after(20, preview_tick)

        def selection() -> list[int]:
            return [int(index) for index in timeline.curselection()]

        def refresh(selected: list[int] | None = None) -> None:
            selected = selection() if selected is None else selected
            pair_uses = Counter(
                (frame["left"], frame["right"]) for frame in frames
            )
            timeline.delete(0, tk.END)
            for index, frame in enumerate(frames):
                uses = pair_uses[(frame["left"], frame["right"])]
                timeline.insert(
                    tk.END,
                    f"{index + 1:03d}  L:{frame['left']}  "
                    f"R:{frame['right']}  refs:{uses}",
                )
            valid = [index for index in selected if index < len(frames)]
            for index in valid:
                timeline.selection_set(index)
            if valid:
                timeline.see(valid[0])
                show_preview(valid[0])
            else:
                show_preview(min(preview_index, len(frames) - 1))
            status.set(
                f"{len(frames)}/255 entries · {len(pair_uses)} unique "
                f"art pairs · {playback.get()} playback"
            )

        def require_selection() -> list[int]:
            selected = selection()
            if not selected:
                self.show_notice("Select one or more timeline rows first")
            return selected

        def move(direction: int) -> None:
            selected = set(require_selection())
            if not selected:
                return
            order = sorted(selected, reverse=direction > 0)
            for index in order:
                neighbor = index + direction
                if (0 <= neighbor < len(frames) and
                        neighbor not in selected):
                    frames[index], frames[neighbor] = (
                        frames[neighbor], frames[index]
                    )
                    selected.remove(index)
                    selected.add(neighbor)
            refresh(sorted(selected))

        def copy_references(repeats: int = 1) -> None:
            selected = require_selection()
            if not selected:
                return
            block = [dict(frames[index]) for index in selected]
            additions = block * repeats
            if len(frames) + len(additions) > 255:
                self.show_notice("A timeline cannot exceed 255 entries")
                return
            insert_at = selected[-1] + 1
            frames[insert_at:insert_at] = additions
            refresh(list(range(insert_at, insert_at + len(additions))))

        def repeat_selection() -> None:
            selected = require_selection()
            if not selected:
                return
            maximum = (255 - len(frames)) // len(selected)
            if maximum < 1:
                self.show_notice("A timeline cannot exceed 255 entries")
                return
            repeats = self.simpledialog.askinteger(
                "Repeat effect range",
                "How many additional times should this selection play?",
                parent=window, initialvalue=min(2, maximum),
                minvalue=1, maxvalue=maximum,
            )
            if repeats is not None:
                copy_references(repeats)

        def reverse_selection() -> None:
            selected = require_selection()
            if not selected:
                return
            values = [frames[index] for index in selected][::-1]
            for index, value in zip(selected, values):
                frames[index] = value
            refresh(selected)

        def append_reverse_exit() -> None:
            exit_frames = [dict(frame) for frame in frames[-2::-1]]
            if not exit_frames:
                self.show_notice("A one-frame animation has no reverse exit")
                return
            if len(frames) + len(exit_frames) > 255:
                self.show_notice("The reverse exit would exceed 255 entries")
                return
            start = len(frames)
            frames.extend(exit_frames)
            refresh(list(range(start, len(frames))))

        def delete_selection() -> None:
            selected = require_selection()
            if not selected:
                return
            if len(selected) >= len(frames):
                self.show_notice("An animation must keep at least one entry")
                return
            next_index = min(selected[0], len(frames) - len(selected) - 1)
            for index in reversed(selected):
                del frames[index]
            refresh([next_index])

        def save() -> None:
            selected = selection()
            selected_index = selected[0] if selected else 0
            try:
                asset_tool.set_animation_timeline(
                    state, frames, playback.get(),
                    assets_path=self.assets.path,
                )
                self.reload_after_frame_change(
                    state, min(selected_index, len(frames) - 1),
                )
                self.show_notice(
                    f"Saved {len(frames)} timeline entries for {state}"
                )
                close()
            except (OSError, ValueError, KeyError) as error:
                self.show_notice(f"Could not save timeline: {error}")

        def close() -> None:
            stop_preview()
            self.timeline_window = None
            window.destroy()

        for label, command in (
            ("Move earlier", lambda: move(-1)),
            ("Move later", lambda: move(1)),
            ("Copy references", copy_references),
            ("Repeat selection…", repeat_selection),
            ("Reverse selection", reverse_selection),
            ("Append reverse exit", append_reverse_exit),
            ("Delete selected", delete_selection),
        ):
            tk.Button(
                controls, text=label, command=command, width=21,
            ).pack(fill="x", pady=(0, 5))

        tk.Label(
            controls, text="Playback after save:", anchor="w",
            background="#111111", foreground="#aaaaaa",
        ).pack(fill="x", pady=(10, 2))
        for label, value in (("Loop", "loop"), ("Ping-pong", "ping_pong")):
            tk.Radiobutton(
                controls, text=label, variable=playback, value=value,
                command=refresh, anchor="w", background="#111111",
                foreground="#eeeeee", selectcolor="#20252a",
                activebackground="#111111", activeforeground="#eeeeee",
            ).pack(fill="x")

        tk.Button(
            preview_buttons, text="◀", width=4,
            command=lambda: step_preview(-1),
        ).pack(side="left")
        tk.Button(
            preview_buttons, textvariable=preview_button_text,
            command=toggle_preview,
        ).pack(side="left", fill="x", expand=True, padx=5)
        tk.Button(
            preview_buttons, text="▶", width=4,
            command=lambda: step_preview(1),
        ).pack(side="left")

        footer = tk.Frame(window, background="#111111")
        footer.pack(fill="x", padx=10, pady=10)
        tk.Label(
            footer, textvariable=status, anchor="w",
            background="#111111", foreground="#aaaaaa",
        ).pack(side="left", fill="x", expand=True)
        tk.Button(footer, text="Cancel", command=close).pack(
            side="right", padx=(5, 0),
        )
        tk.Button(footer, text="Save timeline", command=save).pack(side="right")
        window.protocol("WM_DELETE_WINDOW", close)
        window.bind("<Escape>", lambda _event: close())
        window.bind("<Left>", lambda _event: step_preview(-1))
        window.bind("<Right>", lambda _event: step_preview(1))
        timeline.bind("<Delete>", lambda _event: delete_selection())
        timeline.bind("<Control-d>", lambda _event: copy_references())
        timeline.bind("<Left>", lambda _event: step_preview(-1))
        timeline.bind("<Right>", lambda _event: step_preview(1))

        def preview_selected(_event: Any) -> None:
            if selecting_from_preview:
                return
            selected = selection()
            if selected:
                stop_preview()
                active = int(timeline.index(tk.ACTIVE))
                show_preview(active if active in selected else selected[0])

        timeline.bind("<<ListboxSelect>>", preview_selected)
        refresh([self.current_local_frame])
        window.after(20, preview_tick)
        timeline.focus_set()

    def sync_current_animation_folder(self) -> None:
        state, role = self.state_name, self.edit_role
        directory = self.edit_root() / "animations" / state / role
        if not directory.is_dir():
            self.show_notice(
                f"No {state}/{role} edit folder; open the animation first"
            )
            return
        session = AnimationEditSession(
            directory=directory, state=state, role=role,
            observed_signatures={},
        )
        self.animation_edit_sessions[(state, role)] = session
        self.current_animation_edit = (state, role)
        self.sync_animation_session(session, automatic=False)

    def on_key(self, event: Any) -> None:
        if (self.timeline_window is not None and
                self.timeline_window.winfo_exists() and
                event.widget.winfo_toplevel() == self.timeline_window):
            return
        key = event.keysym
        if key == "Left":
            self.select(self.state_id - 1)
        elif key == "Right":
            self.select(self.state_id + 1)
        elif key in ("Up", "Down"):
            self.auto_play = False
            self.auto_scroll = False
            frames = self.assets.animations[self.state_id]["frames"]
            direction = 1 if key == "Up" else -1
            self.current_local_frame = (
                self.current_local_frame + direction
            ) % len(frames)
            self.last_render_key = None
        elif key in ("Tab", "ISO_Left_Tab"):
            self.switch_view()
        elif key == "space":
            self.auto_scroll = False
            self.auto_play = not self.auto_play
        elif key in ("a", "A"):
            enable = not (self.auto_play and self.auto_scroll)
            self.auto_play = enable
            self.auto_scroll = enable
            self.last_step_ms = time.monotonic_ns() // 1_000_000
        elif key in ("plus", "equal", "KP_Add"):
            self.brightness = min(255, self.brightness + 16)
            self.last_render_key = None
        elif key in ("minus", "underscore", "KP_Subtract"):
            self.brightness = max(0, self.brightness - 16)
            self.last_render_key = None
        elif key == "Insert":
            self.add_frame()
        elif key == "Delete":
            self.remove_frame()
        elif key in ("t", "T"):
            self.change_frame_time()
        elif key in ("f", "F"):
            self.open_timeline_editor()
        elif key in ("s", "S"):
            self.sync_current_animation_folder()
        elif key in ("e", "E"):
            self.open_in_gimp()
        elif key in ("o", "O"):
            self.open_animation_in_gimp()
        elif key in ("c", "C"):
            self.copy_frame()
        elif key in ("r", "R"):
            self.reload_assets()
        elif key in ("q", "Q", "Escape"):
            self.root.destroy()

    def render(self) -> None:
        factor = math.sqrt(self.brightness / 255)
        roles = asset_tool.ROLES if self.view == "both" else (self.view,)
        size = self.assets.width * self.scale
        self.canvas.delete("preview")
        self.photo_native = []
        self.photo_scaled = []
        for index, role in enumerate(roles):
            pixels = self.assets.frame_pixels(
                self.state_id, role, self.current_local_frame,
            )
            scaled = [
                tuple(round(channel * factor) for channel in pixel)
                for pixel in pixels
            ]
            body = bytearray(channel for pixel in scaled for channel in pixel)
            ppm = (
                f"P6\n{self.assets.width} {self.assets.height}\n255\n".encode() +
                body
            )
            native = self.tk.PhotoImage(data=ppm, format="PPM")
            photo = native.zoom(self.scale, self.scale)
            x = 8 + index * (size + self.gap * self.scale)
            self.canvas.create_image(
                x, 8, anchor="nw", image=photo, tags="preview",
            )
            self.canvas.create_oval(
                x, 8, x + size, 8 + size,
                outline="#8b949e", width=max(1, self.scale),
                tags="preview",
            )
            self.photo_native.append(native)
            self.photo_scaled.append(photo)

    def update(self) -> None:
        now_ms = time.monotonic_ns() // 1_000_000
        delta_ms = max(0, now_ms - self.last_update_ms)
        self.last_update_ms = now_ms
        self.watch_edits(now_ms)
        if self.auto_play:
            self.animation_elapsed_ms += delta_ms
        if self.auto_scroll and now_ms - self.last_step_ms >= self.interval_ms:
            self.select(self.state_id + 1, pause=False)
        if self.auto_play:
            self.current_local_frame = self.assets.local_frame(
                self.state_id, self.animation_elapsed_ms,
            )
        render_key = (
            self.state_id, self.view, self.current_local_frame, self.brightness,
        )
        if render_key != self.last_render_key:
            self.render()
            self.last_render_key = render_key

        animation = self.assets.animations[self.state_id]
        frame_mode = "PLAY" if self.auto_play else "PAUSED"
        scroll_mode = "SCROLL" if self.auto_scroll else "MANUAL"
        status = (
            f"0x{self.state_id:02x}  {self.state_name:<16} "
            f"{self.view.upper():<5} view  "
            f"L:{'flip' if animation['flip_left'] else 'normal'} "
            f"R:{'flip' if animation['flip_right'] else 'normal'}  "
            f"frame {self.current_local_frame + 1}/"
            f"{len(animation['frames'])}   "
            f"delay {animation['frame_ms']} ms   {frame_mode}   "
            f"{scroll_mode}   "
            f"brightness {self.brightness:3}/255"
        )
        if now_ms < self.notice_until_ms:
            status = self.notice
        self.status.set(status)
        self.root.after(FRAME_MS, self.update)

    def run(self) -> None:
        self.root.mainloop()


def self_test(assets: AssetPack) -> None:
    hashes = set()
    for state_id, animation in enumerate(assets.animations):
        for frame in range(len(animation["frames"])):
            left = assets.sprite_cache[
                animation["frames"][frame]["left"]
            ]
            right = assets.sprite_cache[
                animation["frames"][frame]["right"]
            ]
            expected = [
                value
                for y in range(assets.height)
                for value in reversed(
                    left[y * assets.width:(y + 1) * assets.width]
                )
            ]
            if assets.is_production and right != expected:
                raise AssertionError(
                    f"{animation['name']} frame {frame + 1} is not mirrored"
                )
        for role in asset_tool.ROLES:
            lit_frames = 0
            for frame in range(len(animation["frames"])):
                pixels = assets.frame_pixels(state_id, role, frame)
                if len(pixels) != assets.width * assets.height:
                    raise AssertionError(
                        f"{animation['name']} {role} frame {frame}: bad size"
                    )
                lit = sum(pixel != BLACK for pixel in pixels)
                lit_frames += lit > 0
                if animation["name"] == "asleep" and lit == 0:
                    raise AssertionError("asleep must show a closed eye line")
                for x, y in ((0, 0), (239, 0), (0, 239), (239, 239)):
                    if pixels[y * assets.width + x] != BLACK:
                        raise AssertionError(
                            f"{animation['name']} leaks outside round screen"
                        )
                raw = bytes(
                    channel for pixel in pixels for channel in pixel
                )
                hashes.add(hashlib.sha256(raw).hexdigest())
            if lit_frames == 0:
                raise AssertionError(
                    f"{animation['name']} {role}: every frame is blank"
                )
    if assets.is_production:
        asset_tool.compile_assets()
    else:
        with tempfile.TemporaryDirectory() as directory:
            asset_tool.compile_assets(
                assets.path, pathlib.Path(directory) / "eye_assets.hpp",
            )
    print(
        f"OK: rendered {len(assets.animations)} paired animations, "
        f"{len(assets.data['sprites'])} native 240x240 sprites"
    )
    print(f"OK: {len(hashes)} distinct role/frame images")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--assets", type=pathlib.Path, default=asset_tool.DEFAULT_ASSETS,
        help="alternate sarcasmos-eye-assets JSON pack to preview",
    )
    parser.add_argument("--state", default=None)
    parser.add_argument(
        "--view", choices=("left", "right", "both"), default="left",
    )
    parser.add_argument(
        "--role", dest="view", choices=asset_tool.ROLES,
        help=argparse.SUPPRESS,
    )
    parser.add_argument("--scale", type=int, default=2)
    parser.add_argument(
        "--gap", type=int, default=24,
        help="native-pixel gap between eyes in the combined view",
    )
    parser.add_argument("--interval", type=float, default=3.0)
    parser.add_argument("--brightness", type=int, default=180)
    parser.add_argument("--paused", action="store_true")
    parser.add_argument(
        "--self-test", action="store_true",
        help="validate every asset without opening a window",
    )
    parser.add_argument(
        "--dump", type=pathlib.Path,
        help="write the selected first frame as a native PPM and exit",
    )
    args = parser.parse_args()
    try:
        assets = AssetPack(args.assets)
    except (OSError, ValueError, KeyError) as error:
        parser.error(f"cannot load --assets: {error}")
    states = tuple(animation["name"] for animation in assets.animations)
    if args.state is None:
        args.state = states[0]
    elif args.state not in states:
        parser.error(
            f"--state must be one of: {', '.join(states)}"
        )
    if args.scale < 1:
        parser.error("--scale must be at least 1")
    if args.gap < 0:
        parser.error("--gap cannot be negative")
    if args.interval <= 0:
        parser.error("--interval must be greater than zero")
    if not 0 <= args.brightness <= 255:
        parser.error("--brightness must be in the range 0..255")
    return args


def main() -> int:
    args = parse_args()
    assets = AssetPack(args.assets)
    if args.self_test:
        self_test(assets)
        return 0
    if args.dump is not None:
        roles = asset_tool.ROLES if args.view == "both" else (args.view,)
        role_pixels = [
            assets.frame_pixels(assets.state_ids[args.state], role, 0)
            for role in roles
        ]
        factor = math.sqrt(args.brightness / 255)
        if len(role_pixels) == 1:
            pixels = role_pixels[0]
            width = assets.width
        else:
            width = assets.width * 2 + args.gap
            pixels = []
            for row in range(assets.height):
                start = row * assets.width
                end = start + assets.width
                pixels.extend(role_pixels[0][start:end])
                pixels.extend([BLACK] * args.gap)
                pixels.extend(role_pixels[1][start:end])
        pixels = [
            tuple(round(channel * factor) for channel in pixel)
            for pixel in pixels
        ]
        asset_tool.write_ppm(
            args.dump, width, assets.height, pixels,
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
