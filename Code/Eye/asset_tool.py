#!/usr/bin/env python3
"""Validate, compile, export, and import SarcasmOS eye animation assets."""

from __future__ import annotations

import argparse
import json
import math
import pathlib
import re
import time
from typing import Any

ROOT = pathlib.Path(__file__).resolve().parent
DEFAULT_ASSETS = ROOT / "assets" / "eye_assets.json"
DEFAULT_HEADER = ROOT / "generated" / "eye_assets.hpp"

EXPECTED_STATES = (
    "idle", "listening", "thinking", "thinking_audio", "thinking_long",
    "speaking", "happy_fake", "angry", "error", "asleep", "tool", "left",
    "right", "up", "down", "center", "neutral", "sarcastic", "suspicious",
    "tired", "surprised", "bored", "dramatic", "watch", "party",
    "battery_low", "sunny", "rainy", "cloudy", "stormy", "snowy",
)
ROLES = ("left", "right")
PLAYBACK_IDS = {"loop": 0, "ping_pong": 1}


def is_default_asset_path(path: pathlib.Path) -> bool:
    return path.resolve() == DEFAULT_ASSETS.resolve()


def load_assets(
    path: pathlib.Path = DEFAULT_ASSETS,
    *,
    strict_protocol: bool | None = None,
) -> dict[str, Any]:
    data = json.loads(path.read_text())
    if strict_protocol is None:
        strict_protocol = is_default_asset_path(path)
    validate_assets(data, strict_protocol=strict_protocol)
    return data


def palette(data: dict[str, Any]) -> list[tuple[int, int, int]]:
    return [tuple(entry["rgb"]) for entry in data["palette"]]


def decode_sprite(data: dict[str, Any], name: str) -> list[int]:
    rows = data["sprites"][name]["rows"]
    return [int(character, 16) for row in rows for character in row]


def mirror_rows(rows: list[str]) -> list[str]:
    """Return a horizontal mirror of a 240x240 indexed sprite."""
    return [row[::-1] for row in rows]


def animation_for(data: dict[str, Any], state: str) -> dict[str, Any]:
    try:
        return next(
            animation for animation in data["animations"]
            if animation["name"] == state
        )
    except StopIteration as error:
        raise ValueError(f"unknown animation state {state!r}") from error


def validate_assets(
    data: dict[str, Any], *, strict_protocol: bool = True,
) -> None:
    if data.get("format") != "sarcasmos-eye-assets":
        raise ValueError("unsupported eye asset format")
    if data.get("version") != 1:
        raise ValueError("unsupported eye asset version")
    width, height = data.get("width"), data.get("height")
    if (width, height) != (240, 240):
        raise ValueError(f"expected 240x240 assets, got {width}x{height}")

    colors = data.get("palette", [])
    if not 1 <= len(colors) <= 16:
        raise ValueError("the palette must contain 1..16 colors")
    for entry in colors:
        if (not isinstance(entry.get("name"), str) or
                len(entry.get("rgb", [])) != 3 or
                any(not isinstance(value, int) or not 0 <= value <= 255
                    for value in entry["rgb"])):
            raise ValueError(f"invalid palette entry: {entry!r}")

    animations = data.get("animations", [])
    names = tuple(animation.get("name") for animation in animations)
    if strict_protocol and names != EXPECTED_STATES:
        raise ValueError("animation order/names do not match protocol.hpp")
    if not animations or any(not isinstance(name, str) or not name for name in names):
        raise ValueError("the asset pack must contain named animations")
    if len(set(names)) != len(names):
        raise ValueError("animation names must be unique")
    for state_id, animation in enumerate(animations):
        if animation.get("id") != state_id:
            raise ValueError(f"{animation['name']}: ID must be {state_id}")
        if animation.get("playback") not in PLAYBACK_IDS:
            raise ValueError(f"{animation['name']}: invalid playback mode")
        for role in ROLES:
            if not isinstance(animation.get(f"flip_{role}"), bool):
                raise ValueError(
                    f"{animation['name']}: flip_{role} must be true or false"
                )
        if (not isinstance(animation.get("frame_ms"), int) or
                not 1 <= animation["frame_ms"] <= 65535):
            raise ValueError(f"{animation['name']}: invalid frame_ms")
        frames = animation.get("frames", [])
        if not frames or len(frames) > 255:
            raise ValueError(
                f"{animation['name']}: frame count must be 1..255"
            )
        for frame in frames:
            if not isinstance(frame, dict) or set(frame) != set(ROLES):
                raise ValueError(
                    f"{animation['name']}: every frame needs left/right art"
                )
            for role in ROLES:
                if frame[role] not in data.get("sprites", {}):
                    raise ValueError(
                        f"{animation['name']}: missing {role} sprite "
                        f"{frame[role]!r}"
                    )
            left = data["sprites"][frame["left"]]["rows"]
            right = data["sprites"][frame["right"]]["rows"]
            if strict_protocol and right != mirror_rows(left):
                raise ValueError(
                    f"{animation['name']}: right source must mirror left source"
                )

    valid_characters = set("0123456789ABCDEF"[:len(colors)])
    for name, sprite in data.get("sprites", {}).items():
        rows = sprite.get("rows", [])
        if len(rows) != height or any(len(row) != width for row in rows):
            raise ValueError(f"{name}: sprite must contain 240 rows of 240 pixels")
        if not set("".join(rows)) <= valid_characters:
            raise ValueError(f"{name}: sprite uses invalid palette indices")

    if strict_protocol:
        header = (ROOT / "include" / "protocol.hpp").read_text()
        if "kAnimCount = 0x1F;" not in header:
            raise ValueError("protocol.hpp animation count does not match assets")


def encode_rle(indices: list[int]) -> list[int]:
    encoded: list[int] = []
    index = 0
    while index < len(indices):
        color = indices[index]
        run = 1
        while (index + run < len(indices) and
               indices[index + run] == color and run < 255):
            run += 1
        encoded.extend((run, color))
        index += run
    return encoded


def format_bytes(values: list[int], indent: str = "    ") -> str:
    return "\n".join(
        f"{indent}{', '.join(f'0x{value:02x}' for value in values[start:start + 16])},"
        for start in range(0, len(values), 16)
    )


def compile_assets(
    source: pathlib.Path = DEFAULT_ASSETS,
    target: pathlib.Path = DEFAULT_HEADER,
) -> None:
    data = load_assets(source)
    sprite_names = sorted(data["sprites"])
    sprite_ids = {name: index for index, name in enumerate(sprite_names)}
    rle_data: list[int] = []
    frame_records = []
    for name in sprite_names:
        encoded = encode_rle(decode_sprite(data, name))
        frame_records.append((len(rle_data), len(encoded)))
        rle_data.extend(encoded)

    frame_pairs: list[tuple[int, int]] = []
    animation_records = []
    for animation in data["animations"]:
        first_pair = len(frame_pairs)
        for frame in animation["frames"]:
            left = "right" if animation["flip_left"] else "left"
            right = "left" if animation["flip_right"] else "right"
            frame_pairs.append((
                sprite_ids[frame[left]],
                sprite_ids[frame[right]],
            ))
        animation_records.append((
            first_pair, len(animation["frames"]), animation["frame_ms"],
            PLAYBACK_IDS[animation["playback"]],
        ))

    palette_rows = "\n".join(
        f"    {{{red}, {green}, {blue}}},"
        for red, green, blue in palette(data)
    )
    frames = "\n".join(
        f"    {{{offset}u, {length}u}}," for offset, length in frame_records
    )
    pairs = "\n".join(
        f"    {{{left}u, {right}u}}," for left, right in frame_pairs
    )
    animations = "\n".join(
        f"    {{{first}u, {count}u, {frame_ms}u, {playback}u}},"
        for first, count, frame_ms, playback in animation_records
    )
    header = f"""// Generated by asset_tool.py from assets/eye_assets.json.
// Do not edit this file; edit/import the native 240x240 sprites instead.
#pragma once

#include <cstddef>
#include <cstdint>

namespace eye_assets {{

constexpr uint16_t kWidth = {data["width"]};
constexpr uint16_t kHeight = {data["height"]};
constexpr uint8_t kAnimationCount = {len(animation_records)};
constexpr uint8_t kPlaybackLoop = 0;
constexpr uint8_t kPlaybackPingPong = 1;

struct Frame {{
    uint32_t offset;
    uint32_t length;
}};

struct FramePair {{
    uint16_t left;
    uint16_t right;
}};

struct Animation {{
    uint16_t firstFramePair;
    uint8_t frameCount;
    uint16_t frameMs;
    uint8_t playback;
}};

constexpr uint8_t kPalette[][3] = {{
{palette_rows}
}};

constexpr Frame kFrames[] = {{
{frames}
}};

constexpr FramePair kFramePairs[] = {{
{pairs}
}};

constexpr Animation kAnimations[] = {{
{animations}
}};

constexpr uint8_t kRleData[] = {{
{format_bytes(rle_data)}
}};

}}  // namespace eye_assets
"""
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(header)
    print(
        f"Compiled {len(animation_records)} animations, "
        f"{len(frame_records)} sprites, {len(rle_data)} RLE bytes -> {target}"
    )


def write_ppm(
    path: pathlib.Path, width: int, height: int,
    pixels: list[tuple[int, int, int]],
) -> None:
    body = bytearray(channel for pixel in pixels for channel in pixel)
    path.write_bytes(f"P6\n{width} {height}\n255\n".encode() + body)


def read_ppm(path: pathlib.Path) -> tuple[int, int, list[tuple[int, int, int]]]:
    raw = path.read_bytes()
    tokens: list[bytes] = []
    position = 0
    while len(tokens) < 4:
        while position < len(raw) and raw[position] in b" \t\r\n":
            position += 1
        if position < len(raw) and raw[position] == ord("#"):
            position = raw.find(b"\n", position)
            if position < 0:
                raise ValueError("truncated PPM comment")
            continue
        match = re.match(rb"\S+", raw[position:])
        if match is None:
            raise ValueError("truncated PPM header")
        tokens.append(match.group())
        position += len(match.group())
    magic, width_token, height_token, maximum_token = tokens
    if magic != b"P6" or maximum_token != b"255":
        raise ValueError("only binary 8-bit P6 PPM files are supported")
    width, height = int(width_token), int(height_token)
    while position < len(raw) and raw[position] in b" \t\r\n":
        position += 1
    body = raw[position:]
    if len(body) != width * height * 3:
        raise ValueError("incorrect PPM pixel-data length")
    return width, height, [
        tuple(body[index:index + 3]) for index in range(0, len(body), 3)
    ]


def nearest_palette_index(
    pixel: tuple[int, int, int],
    colors: list[tuple[int, int, int]],
) -> int:
    return min(
        range(len(colors)),
        key=lambda index: sum(
            (pixel[channel] - colors[index][channel]) ** 2
            for channel in range(3)
        ),
    )


def pixels_to_rows(
    data: dict[str, Any], pixels: list[tuple[int, int, int]],
) -> list[str]:
    width, height = data["width"], data["height"]
    if len(pixels) != width * height:
        raise ValueError(f"expected {width * height} pixels")
    colors = palette(data)
    exact = {color: index for index, color in enumerate(colors)}
    existing_names = {entry["name"] for entry in data["palette"]}
    indices = []
    for pixel in pixels:
        index = exact.get(pixel)
        if index is None and len(colors) < 16:
            index = len(colors)
            name = f"imported_{index:02X}"
            while name in existing_names:
                name += "_new"
            data["palette"].append({"name": name, "rgb": list(pixel)})
            existing_names.add(name)
            colors.append(pixel)
            exact[pixel] = index
        if index is None:
            index = nearest_palette_index(pixel, colors)
        indices.append(index)
    return [
        "".join(
            format(index, "X")
            for index in indices[y * width:(y + 1) * width]
        )
        for y in range(height)
    ]


def sprite_pixels(
    data: dict[str, Any], state: str, role: str, frame_index: int,
    brightness: int = 255,
) -> list[tuple[int, int, int]]:
    animation = animation_for(data, state)
    if role not in ROLES:
        raise ValueError(f"role must be one of {ROLES}")
    try:
        sprite = animation["frames"][frame_index][role]
    except IndexError as error:
        raise ValueError(f"{state} has no frame {frame_index + 1}") from error
    colors = palette(data)
    factor = math.sqrt(brightness / 255)
    return [
        tuple(round(channel * factor) for channel in colors[index])
        for index in decode_sprite(data, sprite)
    ]


def write_asset_pack(
    data: dict[str, Any],
    assets_path: pathlib.Path = DEFAULT_ASSETS,
    header_path: pathlib.Path | None = None,
) -> None:
    production = is_default_asset_path(assets_path)
    validate_assets(data, strict_protocol=production)
    if header_path is None and production:
        header_path = DEFAULT_HEADER
    token = time.monotonic_ns()
    temporary_assets = assets_path.with_name(
        f".{assets_path.name}.{token}.tmp"
    )
    temporary_header = (
        header_path.with_name(f".{header_path.name}.{token}.tmp")
        if header_path is not None else None
    )
    try:
        temporary_assets.parent.mkdir(parents=True, exist_ok=True)
        temporary_assets.write_text(json.dumps(data, indent=2) + "\n")
        if temporary_header is not None:
            temporary_header.parent.mkdir(parents=True, exist_ok=True)
            compile_assets(temporary_assets, temporary_header)
        temporary_assets.replace(assets_path)
        if temporary_header is not None and header_path is not None:
            temporary_header.replace(header_path)
    finally:
        temporary_assets.unlink(missing_ok=True)
        if temporary_header is not None:
            temporary_header.unlink(missing_ok=True)


def import_sprite(
    state: str, role: str, frame_index: int, source: pathlib.Path,
    *,
    assets_path: pathlib.Path = DEFAULT_ASSETS,
    header_path: pathlib.Path | None = None,
) -> None:
    data = load_assets(assets_path)
    width, height, pixels = read_ppm(source)
    if (width, height) != (240, 240):
        raise ValueError(f"edited sprite must be 240x240, got {width}x{height}")
    animation = animation_for(data, state)
    try:
        frame = animation["frames"][frame_index]
    except IndexError as error:
        raise ValueError(f"{state} has no frame {frame_index + 1}") from error
    edited_rows = pixels_to_rows(data, pixels)
    source_role = (
        "right" if role == "left" else "left"
    ) if animation[f"flip_{role}"] else role
    if is_default_asset_path(assets_path):
        left_rows = (
            edited_rows
            if source_role == "left" else mirror_rows(edited_rows)
        )
        right_rows = mirror_rows(left_rows)
        left_sprite = f"{state}_left_{frame_index:02d}_edited"
        right_sprite = f"{state}_right_{frame_index:02d}_mirrored"
        data["sprites"][left_sprite] = {"rows": left_rows}
        data["sprites"][right_sprite] = {"rows": right_rows}
        frame["left"] = left_sprite
        frame["right"] = right_sprite
    else:
        sprite = f"{state}_{source_role}_{frame_index:02d}_edited"
        data["sprites"][sprite] = {"rows": edited_rows}
        frame[source_role] = sprite
    prune_unreferenced_sprites(data)
    write_asset_pack(data, assets_path, header_path)
    print(f"Imported {source} -> {state} {role} frame {frame_index + 1}")


def prune_unreferenced_sprites(data: dict[str, Any]) -> None:
    referenced = {
        frame[role]
        for animation in data["animations"]
        for frame in animation["frames"]
        for role in ROLES
    }
    for sprite in set(data["sprites"]) - referenced:
        del data["sprites"][sprite]


def insert_animation_frame(
    state: str, after_index: int, *,
    assets_path: pathlib.Path = DEFAULT_ASSETS,
    header_path: pathlib.Path | None = None,
) -> int:
    data = load_assets(assets_path)
    animation = animation_for(data, state)
    frames = animation["frames"]
    if not 0 <= after_index < len(frames):
        raise ValueError(f"{state} has no frame {after_index + 1}")
    if len(frames) >= 255:
        raise ValueError(f"{state} cannot contain more than 255 frames")
    inserted = after_index + 1
    frames.insert(inserted, dict(frames[after_index]))
    write_asset_pack(data, assets_path, header_path)
    return inserted


def remove_animation_frame(
    state: str, frame_index: int, *,
    assets_path: pathlib.Path = DEFAULT_ASSETS,
    header_path: pathlib.Path | None = None,
) -> int:
    data = load_assets(assets_path)
    animation = animation_for(data, state)
    frames = animation["frames"]
    if len(frames) <= 1:
        raise ValueError(f"{state} must keep at least one frame")
    if not 0 <= frame_index < len(frames):
        raise ValueError(f"{state} has no frame {frame_index + 1}")
    del frames[frame_index]
    prune_unreferenced_sprites(data)
    write_asset_pack(data, assets_path, header_path)
    return min(frame_index, len(frames) - 1)


def set_animation_frame_ms(
    state: str, frame_ms: int, *,
    assets_path: pathlib.Path = DEFAULT_ASSETS,
    header_path: pathlib.Path | None = None,
) -> None:
    if not 1 <= frame_ms <= 65535:
        raise ValueError("frame time must be in the range 1..65535 ms")
    data = load_assets(assets_path)
    animation_for(data, state)["frame_ms"] = frame_ms
    write_asset_pack(data, assets_path, header_path)


def save_asset_source(
    data: dict[str, Any], assets_path: pathlib.Path = DEFAULT_ASSETS,
) -> None:
    """Atomically save loaded assets without compiling the firmware header."""
    validate_assets(
        data, strict_protocol=is_default_asset_path(assets_path),
    )
    token = time.monotonic_ns()
    temporary = assets_path.with_name(
        f".{assets_path.name}.{token}.tmp"
    )
    try:
        temporary.parent.mkdir(parents=True, exist_ok=True)
        temporary.write_text(json.dumps(data, indent=2) + "\n")
        temporary.replace(assets_path)
    finally:
        temporary.unlink(missing_ok=True)


def sync_animation_frames(
    state: str, role: str, sources: list[pathlib.Path],
    *,
    assets_path: pathlib.Path = DEFAULT_ASSETS,
    header_path: pathlib.Path | None = None,
) -> int:
    if not sources or len(sources) > 255:
        raise ValueError(f"{state} must keep 1..255 frames")
    data = load_assets(assets_path)
    animation = animation_for(data, state)
    existing_frames = animation["frames"]
    production = is_default_asset_path(assets_path)
    source_role = (
        "right" if role == "left" else "left"
    ) if animation[f"flip_{role}"] else role
    new_frames = []
    for index, source in enumerate(sources):
        width, height, pixels = read_ppm(source)
        if (width, height) != (240, 240):
            raise ValueError(f"{source.name} must be 240x240")
        edited_rows = pixels_to_rows(data, pixels)
        if production:
            left_rows = (
                edited_rows
                if source_role == "left" else mirror_rows(edited_rows)
            )
            right_rows = mirror_rows(left_rows)
            left_sprite = f"{state}_left_{index:02d}_edited"
            right_sprite = f"{state}_right_{index:02d}_mirrored"
            data["sprites"][left_sprite] = {"rows": left_rows}
            data["sprites"][right_sprite] = {"rows": right_rows}
            frame = {"left": left_sprite, "right": right_sprite}
        else:
            sprite = f"{state}_{source_role}_{index:02d}_edited"
            data["sprites"][sprite] = {"rows": edited_rows}
            frame = dict(existing_frames[min(index, len(existing_frames) - 1)])
            frame[source_role] = sprite
        new_frames.append(frame)
    animation["frames"] = new_frames
    prune_unreferenced_sprites(data)
    write_asset_pack(data, assets_path, header_path)
    return len(new_frames)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    commands.add_parser("validate")
    commands.add_parser("compile")
    export = commands.add_parser("export")
    export.add_argument("state", choices=EXPECTED_STATES)
    export.add_argument("output", type=pathlib.Path)
    export.add_argument("--role", choices=ROLES, default="left")
    export.add_argument("--frame", type=int, default=0)
    export.add_argument("--brightness", type=int, default=255)
    import_command = commands.add_parser("import")
    import_command.add_argument("state", choices=EXPECTED_STATES)
    import_command.add_argument("source", type=pathlib.Path)
    import_command.add_argument("--role", choices=ROLES, default="left")
    import_command.add_argument("--frame", type=int, default=0)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    try:
        if args.command == "validate":
            data = load_assets()
            print(
                f"OK: {len(data['animations'])} animations, "
                f"{len(data['sprites'])} sprites, 240x240 paired eyes"
            )
        elif args.command == "compile":
            compile_assets()
        elif args.command == "export":
            if not 0 <= args.brightness <= 255:
                raise ValueError("brightness must be in the range 0..255")
            data = load_assets()
            write_ppm(
                args.output, data["width"], data["height"],
                sprite_pixels(
                    data, args.state, args.role, args.frame, args.brightness,
                ),
            )
        elif args.command == "import":
            import_sprite(args.state, args.role, args.frame, args.source)
    except (OSError, ValueError, KeyError, json.JSONDecodeError) as error:
        print(f"asset error: {error}", file=__import__("sys").stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
