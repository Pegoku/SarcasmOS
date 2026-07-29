#!/usr/bin/env python3
"""Regression coverage for path-aware alternate asset-pack editing."""

from __future__ import annotations

import pathlib
import sys
import tempfile
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))

import asset_tool  # noqa: E402


def sprite_rows(color: str) -> list[str]:
    return [color * 240 for _ in range(240)]


def sample_pack() -> dict:
    return {
        "format": "sarcasmos-eye-assets",
        "version": 1,
        "width": 240,
        "height": 240,
        "palette": [
            {"name": "black", "rgb": [0, 0, 0]},
            {"name": "white", "rgb": [255, 255, 255]},
        ],
        "animations": [{
            "id": 0,
            "name": "sample",
            "flip_left": False,
            "flip_right": False,
            "playback": "loop",
            "frame_ms": 100,
            "frames": [
                {"left": "left_0", "right": "right_0"},
                {"left": "left_1", "right": "right_1"},
            ],
        }],
        "sprites": {
            "left_0": {"rows": sprite_rows("0")},
            "right_0": {"rows": sprite_rows("1")},
            "left_1": {"rows": sprite_rows("1")},
            "right_1": {"rows": sprite_rows("0")},
        },
    }


class AlternateAssetEditingTest(unittest.TestCase):
    def test_all_edit_operations_stay_in_alternate_pack(self) -> None:
        production_assets = asset_tool.DEFAULT_ASSETS.read_bytes()
        production_header = asset_tool.DEFAULT_HEADER.read_bytes()
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            assets = root / "alternate.json"
            asset_tool.save_asset_source(sample_pack(), assets)

            data = asset_tool.load_assets(assets)
            data["animations"][0]["flip_left"] = True
            asset_tool.save_asset_source(data, assets)
            self.assertTrue(
                asset_tool.load_assets(assets)["animations"][0]["flip_left"]
            )
            data = asset_tool.load_assets(assets)
            data["animations"][0]["flip_left"] = False
            asset_tool.save_asset_source(data, assets)

            data = asset_tool.load_assets(assets)
            pixels = asset_tool.sprite_pixels(data, "sample", "left", 0)
            pixels[120 * 240 + 120] = (255, 255, 255)
            edited = root / "edited.ppm"
            asset_tool.write_ppm(edited, 240, 240, pixels)
            original_right = data["animations"][0]["frames"][0]["right"]
            asset_tool.import_sprite(
                "sample", "left", 0, edited, assets_path=assets,
            )
            frame = asset_tool.load_assets(assets)["animations"][0]["frames"][0]
            self.assertTrue(frame["left"].endswith("_edited"))
            self.assertEqual(frame["right"], original_right)

            inserted = asset_tool.insert_animation_frame(
                "sample", 0, assets_path=assets,
            )
            self.assertEqual(inserted, 1)
            asset_tool.set_animation_frame_ms(
                "sample", 321, assets_path=assets,
            )
            data = asset_tool.load_assets(assets)
            self.assertEqual(len(data["animations"][0]["frames"]), 3)
            self.assertEqual(data["animations"][0]["frame_ms"], 321)
            asset_tool.remove_animation_frame(
                "sample", inserted, assets_path=assets,
            )

            before = asset_tool.load_assets(assets)["animations"][0]["frames"]
            asset_tool.sync_animation_frames(
                "sample", "left", [edited, edited], assets_path=assets,
            )
            frames = asset_tool.load_assets(assets)["animations"][0]["frames"]
            self.assertEqual(len(frames), 2)
            self.assertEqual(frames[0]["right"], before[0]["right"])
            self.assertEqual(frames[1]["right"], before[1]["right"])
            self.assertTrue(all(
                frame["left"].endswith("_edited") for frame in frames
            ))
            data = asset_tool.load_assets(assets)
            sprite_count = len(data["sprites"])
            asset_tool.set_animation_timeline(
                "sample", [frames[1], frames[0], frames[0]], "ping_pong",
                loop_range={"start": 1, "end": 2, "mode": "loop"},
                assets_path=assets,
            )
            data = asset_tool.load_assets(assets)
            timeline = data["animations"][0]
            self.assertEqual(timeline["playback"], "ping_pong")
            self.assertEqual(
                timeline["loop_range"],
                {"start": 1, "end": 2, "mode": "loop"},
            )
            self.assertEqual(len(timeline["frames"]), 3)
            self.assertEqual(timeline["frames"][1], timeline["frames"][2])
            self.assertLessEqual(len(data["sprites"]), sprite_count)
            compiled = root / "compiled.hpp"
            asset_tool.compile_assets(assets, compiled)
            header = compiled.read_text()
            self.assertIn("EYE_ASSETS_HAS_LOOP_RANGES", header)
            self.assertIn("uint8_t loopStart;", header)
            self.assertFalse((root / "eye_assets.hpp").exists())

        self.assertEqual(asset_tool.DEFAULT_ASSETS.read_bytes(), production_assets)
        self.assertEqual(asset_tool.DEFAULT_HEADER.read_bytes(), production_header)

    def test_loop_range_frame_resolution(self) -> None:
        animation = {
            "frames": list(range(5)),
            "frame_ms": 10,
            "playback": "loop",
            "loop_range": {"start": 1, "end": 3, "mode": "loop"},
        }
        self.assertEqual(
            [asset_tool.animation_frame_index(animation, step * 10)
             for step in range(9)],
            [0, 1, 2, 3, 1, 2, 3, 1, 2],
        )
        animation["loop_range"]["mode"] = "ping_pong"
        self.assertEqual(
            [asset_tool.animation_frame_index(animation, step * 10)
             for step in range(9)],
            [0, 1, 2, 3, 2, 1, 2, 3, 2],
        )


if __name__ == "__main__":
    unittest.main()
