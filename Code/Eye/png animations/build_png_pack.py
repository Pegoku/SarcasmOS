#!/usr/bin/env python3
"""Convert the ready-made PNG animations into a SarcasmOS eye asset pack."""

from __future__ import annotations

import argparse
import hashlib
import json
import pathlib
import re
import sys

from PIL import Image


HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parent
DEFAULT_SOURCE = HERE / "source"
DEFAULT_OUTPUT = HERE / "eye_assets.json"
sys.path.insert(0, str(ROOT))

import asset_tool  # noqa: E402


# Standard state, source folder, selected zero-based frames or None, frame time,
# playback. Folder typos are retained because they are part of the supplied set.
ANIMATIONS = (
    ("idle", "idle", None, 120, "loop"),
    ("listening", "thinking", None, 192, "loop"),
    ("thinking", "thinking", None, 160, "loop"),
    ("thinking_audio", "thinking", None, 110, "loop"),
    ("thinking_long", "thinking", None, 320, "loop"),
    ("speaking", "speaking", None, 110, "loop"),
    ("happy_fake", "happy_fake", None, 150, "ping_pong"),
    ("angry", "angry", None, 180, "loop"),
    ("error", "error", None, 150, "loop"),
    ("asleep", "blink", (3,), 1000, "loop"),
    ("tool", "tool", None, 192, "loop"),
    ("left", "left", None, 180, "ping_pong"),
    ("right", "right", None, 180, "ping_pong"),
    ("up", "up", None, 180, "ping_pong"),
    ("down", "down", None, 180, "ping_pong"),
    ("center", "neutral-central", None, 1000, "loop"),
    ("neutral", "neutral-central", None, 1000, "loop"),
    ("sarcastic", "sarcastic", None, 320, "ping_pong"),
    ("suspicious", "suspicious", None, 300, "loop"),
    ("tired", "tired", None, 160, "loop"),
    ("surprised", "surprisde", None, 350, "ping_pong"),
    ("bored", "boared", None, 300, "loop"),
    ("dramatic", "dramatic", None, 192, "ping_pong"),
    ("watch", "watch", None, 180, "ping_pong"),
    ("party", "party", None, 144, "loop"),
    ("battery_low", "battery_low", None, 500, "loop"),
    ("sunny", "sunny", None, 800, "ping_pong"),
    ("rainy", "rainny", None, 180, "loop"),
    ("cloudy", "cloudy", None, 300, "loop"),
    ("stormy", "stormy", None, 160, "loop"),
    ("snowy", "snowy", None, 288, "loop"),
)

COLOR_NAMES = {
    (0, 0, 0): "black",
    (21, 23, 39): "weather_background",
    (67, 69, 33): "eye_shadow_1",
    (99, 155, 255): "weather_blue",
    (109, 112, 54): "eye_shadow_2",
    (134, 138, 62): "eye_shadow_3",
    (172, 50, 50): "error_red",
    (180, 186, 80): "eye_shadow_4",
    (203, 203, 203): "cloud_gray",
    (214, 220, 87): "eye_shadow_5",
    (247, 255, 98): "eye_yellow",
    (249, 255, 128): "eye_highlight_1",
    (250, 255, 149): "eye_highlight_2",
    (251, 242, 54): "sun_yellow",
    (251, 255, 167): "eye_highlight_3",
    (255, 255, 255): "white",
}


def natural_key(path: pathlib.Path) -> tuple:
    return tuple(
        int(part) if part.isdigit() else part.lower()
        for part in re.split(r"(\d+)", path.name)
    )


def png_files(directory: pathlib.Path) -> list[pathlib.Path]:
    files = sorted(directory.glob("*.png"), key=natural_key)
    if not files:
        raise ValueError(f"no PNG frames in {directory}")
    return files


def source_digest(source: pathlib.Path) -> str:
    digest = hashlib.sha256()
    for path in sorted(source.rglob("*.png")):
        digest.update(path.relative_to(source).as_posix().encode())
        digest.update(b"\0")
        digest.update(path.read_bytes())
    return digest.hexdigest()


def discover_palette(source: pathlib.Path) -> list[tuple[int, int, int]]:
    colors: set[tuple[int, int, int]] = set()
    for path in source.rglob("*.png"):
        image = Image.open(path).convert("RGBA")
        if image.size != (240, 240):
            raise ValueError(f"{path}: expected 240x240, got {image.size}")
        rgba = set(image.get_flattened_data())
        if any(alpha != 255 for *_, alpha in rgba):
            raise ValueError(f"{path}: transparent pixels are not supported")
        colors.update((red, green, blue) for red, green, blue, _ in rgba)
    if not 1 <= len(colors) <= 16:
        raise ValueError(f"expected at most 16 shared colors, found {len(colors)}")
    ordered = sorted(colors)
    missing_names = set(ordered) - set(COLOR_NAMES)
    if missing_names:
        raise ValueError(f"unrecognized source colors: {sorted(missing_names)}")
    return ordered


def image_rows(
    path: pathlib.Path,
    color_indices: dict[tuple[int, int, int], int],
) -> list[str]:
    image = Image.open(path).convert("RGB")
    pixels = list(image.get_flattened_data())
    if any(pixels[y * 240 + x] != (0, 0, 0)
           for x, y in ((0, 0), (239, 0), (0, 239), (239, 239))):
        raise ValueError(f"{path}: artwork leaks outside the round screen")
    return [
        "".join(
            format(color_indices[pixel], "X")
            for pixel in pixels[y * 240:(y + 1) * 240]
        )
        for y in range(240)
    ]


def build_pack(source: pathlib.Path) -> dict:
    colors = discover_palette(source)
    color_indices = {color: index for index, color in enumerate(colors)}
    palette = [
        {"name": COLOR_NAMES[color], "rgb": list(color)}
        for color in colors
    ]
    sprites: dict[str, dict[str, list[str]]] = {}
    digest_to_name: dict[str, str] = {}
    animations = []

    for animation_id, entry in enumerate(ANIMATIONS):
        name, folder, selection, frame_ms, playback = entry
        files = png_files(source / folder)
        if selection is not None:
            files = [files[index] for index in selection]
        frames = []
        for frame_index, path in enumerate(files):
            left_rows = image_rows(path, color_indices)
            pair = {}
            for role, rows in (
                ("left", left_rows),
                ("right", asset_tool.mirror_rows(left_rows)),
            ):
                digest = hashlib.sha256("".join(rows).encode()).hexdigest()
                sprite = digest_to_name.get(digest)
                if sprite is None:
                    sprite = f"{name}_{role}_{frame_index:02d}"
                    digest_to_name[digest] = sprite
                    sprites[sprite] = {"rows": rows}
                pair[role] = sprite
            frames.append(pair)
        animations.append({
            "id": animation_id,
            "name": name,
            "mapped_from": folder,
            "flip_left": False,
            "flip_right": name in ("left", "right"),
            "playback": playback,
            "frame_ms": frame_ms,
            "frames": frames,
        })

    pack = {
        "format": "sarcasmos-eye-assets",
        "version": 1,
        "source": "ready-made PNG animations",
        "source_manifest_sha256": source_digest(source),
        "width": 240,
        "height": 240,
        "palette": palette,
        "animations": animations,
        "sprites": sprites,
    }
    asset_tool.validate_assets(pack, strict_protocol=True)
    return pack


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=pathlib.Path, default=DEFAULT_SOURCE)
    parser.add_argument("--output", type=pathlib.Path, default=DEFAULT_OUTPUT)
    args = parser.parse_args()
    pack = build_pack(args.source.resolve())
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(pack, indent=2) + "\n")
    print(
        f"Created {len(pack['animations'])} animations, "
        f"{sum(len(animation['frames']) for animation in pack['animations'])} "
        f"paired frames, and {len(pack['sprites'])} sprites -> {args.output}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
