#!/usr/bin/env python3
"""Rasterize the extracted C++ animations into an emulator asset pack."""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
import pathlib
import subprocess
import tempfile


HERE = pathlib.Path(__file__).resolve().parent
DEFAULT_OUTPUT = HERE / "eye_assets.json"


def rgb565(value: int) -> list[int]:
    red = (value >> 11) & 0x1F
    green = (value >> 5) & 0x3F
    blue = value & 0x1F
    return [
        (red << 3) | (red >> 2),
        (green << 2) | (green >> 4),
        (blue << 3) | (blue >> 2),
    ]


PALETTE = [
    {"name": "black", "rgb": rgb565(0x0000)},
    {"name": "white", "rgb": rgb565(0xFFFF)},
    {"name": "eye_yellow", "rgb": rgb565(0xEF13)},
    {"name": "eye_yellow_dark", "rgb": rgb565(0xCE6E)},
    {"name": "red", "rgb": rgb565(0xD800)},
    {"name": "blue", "rgb": rgb565(0x2D7F)},
    {"name": "cyan", "rgb": rgb565(0x57FF)},
    {"name": "green", "rgb": rgb565(0x77E9)},
    {"name": "amber", "rgb": rgb565(0xFEA0)},
    {"name": "gray", "rgb": rgb565(0x8410)},
    {"name": "dark", "rgb": rgb565(0x1082)},
    {"name": "weather_blue", "rgb": rgb565(0x1A8B)},
    {"name": "night_blue", "rgb": rgb565(0x0841)},
    {"name": "moon", "rgb": rgb565(0xFF76)},
    {"name": "bezel_outer", "rgb": rgb565(0x39E7)},
    {"name": "bezel_inner", "rgb": rgb565(0x2104)},
]

# Target protocol state, closest source animation, optional selected frames.
# None keeps the complete source animation. Selections make static gaze/status
# poses from a particular point in an otherwise continuous source animation.
PROTOCOL_MAPPING = (
    ("idle", "eye-idle", None),
    ("listening", "eye-listening", None),
    ("thinking", "eye-dizzy", None),
    ("thinking_audio", "eye-listening", None),
    ("thinking_long", "eye-look", None),
    ("speaking", "eye-happy", None),
    ("happy_fake", "eye-happy", None),
    ("angry", "eye-angry", None),
    ("error", "system-error", None),
    ("asleep", "eye-sleep", None),
    ("tool", "eye-angry", None),
    ("left", "eye-look", (6,)),
    ("right", "eye-look", (2,)),
    ("up", "eye-look", (7,)),
    ("down", "eye-look", (2,)),
    ("center", "eye-idle", (0,)),
    ("neutral", "eye-idle", (0,)),
    ("sarcastic", "eye-confused", None),
    ("suspicious", "eye-suspicious", None),
    ("tired", "eye-sleep", None),
    ("surprised", "eye-surprised", None),
    ("bored", "eye-suspicious", (0,)),
    ("dramatic", "eye-alert", None),
    ("watch", "eye-look", None),
    ("party", "eye-glitch", None),
    ("battery_low", "system-battery", None),
    ("sunny", "weather-sun", None),
    ("rainy", "weather-rain", None),
    ("cloudy", "weather-clouds", None),
    ("stormy", "weather-storm", None),
    ("snowy", "weather-snow", None),
)


def read_ppm(path: pathlib.Path) -> list[tuple[int, int, int]]:
    header, dimensions, maximum, body = path.read_bytes().split(b"\n", 3)
    if header != b"P6" or dimensions != b"240 240" or maximum != b"255":
        raise ValueError(f"unexpected PPM header in {path}")
    if len(body) != 240 * 240 * 3:
        raise ValueError(f"unexpected pixel count in {path}")
    return [
        tuple(body[index:index + 3])
        for index in range(0, len(body), 3)
    ]


def indexed_rows(path: pathlib.Path) -> list[str]:
    indices = {tuple(entry["rgb"]): index for index, entry in enumerate(PALETTE)}
    try:
        pixels = [indices[pixel] for pixel in read_ppm(path)]
    except KeyError as error:
        raise ValueError(f"{path} contains a color outside the 16-color palette") from error
    return [
        "".join(format(value, "X") for value in pixels[y * 240:(y + 1) * 240])
        for y in range(240)
    ]


def compile_exporter(target: pathlib.Path) -> None:
    subprocess.run(
        [
            "c++", "-std=c++17", "-O2", "-Wall", "-Wextra", "-Werror",
            str(HERE / "procedural_animations.cpp"),
            str(HERE / "bitmap_exporter.cpp"),
            "-o", str(target),
        ],
        check=True,
    )


def build_pack(frames_directory: pathlib.Path) -> dict:
    sprites: dict[str, dict[str, list[str]]] = {}
    digest_to_name: dict[str, str] = {}
    animations = []
    with (frames_directory / "manifest.tsv").open(newline="") as source:
        manifest = csv.DictReader(source, delimiter="\t")
        for animation_id, entry in enumerate(manifest):
            name = entry["name"]
            frame_count = int(entry["frames"])
            frames = []
            for frame_index in range(frame_count):
                pair = {}
                for role in ("left", "right"):
                    ppm = frames_directory / (
                        f"{name}-{role}-{frame_index:02d}.ppm"
                    )
                    rows = indexed_rows(ppm)
                    digest = hashlib.sha256("".join(rows).encode()).hexdigest()
                    sprite_name = digest_to_name.get(digest)
                    if sprite_name is None:
                        sprite_name = f"{name}_{role}_{frame_index:02d}"
                        digest_to_name[digest] = sprite_name
                        sprites[sprite_name] = {"rows": rows}
                    pair[role] = sprite_name
                frames.append(pair)
            animations.append({
                "id": animation_id,
                "name": name,
                "flip_left": False,
                "flip_right": False,
                "playback": entry["playback"],
                "frame_ms": int(entry["frame_ms"]),
                "frames": frames,
            })
    return {
        "format": "sarcasmos-eye-assets",
        "version": 1,
        "width": 240,
        "height": 240,
        "palette": PALETTE,
        "animations": animations,
        "sprites": sprites,
    }


def map_to_protocol(source: dict) -> dict:
    by_name = {
        animation["name"]: animation
        for animation in source["animations"]
    }
    animations = []
    referenced_sprites: set[str] = set()
    for animation_id, (target_name, source_name, selection) in enumerate(
        PROTOCOL_MAPPING
    ):
        original = by_name[source_name]
        indices = (
            range(len(original["frames"]))
            if selection is None else selection
        )
        frames = [original["frames"][index] for index in indices]
        for frame in frames:
            referenced_sprites.update(frame.values())
        animations.append({
            "id": animation_id,
            "name": target_name,
            "mapped_from": source_name,
            "flip_left": False,
            "flip_right": False,
            "playback": original["playback"],
            "frame_ms": 1000 if selection is not None else original["frame_ms"],
            "frames": frames,
        })
    return {
        **source,
        "source": "BotAnimator_ESP32.ino procedural mapping",
        "animations": animations,
        "sprites": {
            name: sprite
            for name, sprite in source["sprites"].items()
            if name in referenced_sprites
        },
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=pathlib.Path, default=DEFAULT_OUTPUT)
    parser.add_argument(
        "--all", action="store_true",
        help="export all 44 source modes instead of the 31-state protocol map",
    )
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="bot-animation-export-") as temporary:
        temporary_path = pathlib.Path(temporary)
        exporter = temporary_path / "bitmap-exporter"
        frames = temporary_path / "frames"
        compile_exporter(exporter)
        subprocess.run([str(exporter), str(frames)], check=True)
        source_pack = build_pack(frames)
        pack = source_pack if args.all else map_to_protocol(source_pack)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(pack, indent=2) + "\n")
    print(
        f"Created {len(pack['animations'])} animations and "
        f"{len(pack['sprites'])} deduplicated 240x240 sprites -> {args.output}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
