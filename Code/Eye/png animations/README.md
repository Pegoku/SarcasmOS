# Ready-made PNG animation pack

This directory contains the 240x240 PNG animations imported from
`/home/pegoku/Downloads/eyes animations` and converted into the same
palette-indexed `sarcasmos-eye-assets` JSON format used by the desktop emulator
and firmware asset compiler.

The source images are already fully opaque, round-screen-safe, and share exactly
16 colors. Conversion is lossless: no resizing, color quantization, or generated
in-between frames are applied. Each PNG is treated as a native left-eye frame;
the paired right-eye frame is stored as its horizontal mirror, matching the
production asset contract.

## View and edit

From the `Eye` directory:

```sh
python emulator.py --assets "png animations/eye_assets.json" --view both
```

The alternate pack supports the emulator's normal GIMP auto-import, frame
insertion/removal, timing, folder synchronization, and orientation controls.

## Rebuild

The original PNGs are preserved under `source/`. Rebuild the JSON with:

```sh
python "png animations/build_png_pack.py"
```

This deliberately replaces manual edits made to `eye_assets.json`.

## Standard-name mapping

The resulting pack uses the exact 31 production animation names, IDs, and
ordering. Supplied folder/file typos are normalized automatically:
`angy` → `angry`, `boared` → `bored`, `rainny` → `rainy`, `surprisde` →
`surprised`, and `claudy` → `cloudy`.

Five standard states have no dedicated source folder and use these fallbacks:

| Standard state | PNG source | Reason |
|---|---|---|
| `listening` | `thinking` | Closest available attentive motion |
| `thinking_audio` | `thinking` | No audio-specific PNG sequence |
| `thinking_long` | `thinking` | Same artwork with a slower frame delay |
| `asleep` | `blink/blink4.png` | Last visible closed-eye frame; `blink5` is blank |
| `center` | `neutral-central/neutral.png` | Centered neutral pose |

The supplied `blink` sequence is not a standard protocol animation ID, so only
its closed frame is used for `asleep`.
