# Mouth animation assets

`mouth_assets.json` is the single source of truth for both the desktop
emulator and ESP32 firmware. Artwork and animation timing must be changed here,
not in `mouth_display.cpp`, `emulator.py`, or the generated C++ header.

The pack contains:

- a shared palette of up to 16 RGB colors;
- 64x32 sprites stored as 32 rows of hexadecimal palette indices;
- one animation entry for every protocol state;
- ordered frame references, frame duration, and a playback mode.

Playback modes are:

- `loop`: advance through frames and wrap;
- `ping_pong`: advance to the last frame and reverse;
- `intensity`: use speaking intensity to scale a triangular frame sequence.

## Visual editing

Run `../emulator.py`, pause on a state with Left or Right, and use:

1. **Open frame in GIMP** (`e`) to export and open the current native frame.
2. Edit without resizing the 64x32 canvas. Disable interpolation when scaling
   the view; individual pixels are intentional.
3. Overwrite the opened PPM from GIMP.
4. **Import saved edit** (`i`) to quantize it to the palette, update this pack,
   regenerate the firmware header, and reload the preview.

**Copy frame** (`c`) copies a 64x32 PNG for sharing or attaching to a request.

## Command-line editing

```sh
../asset_tool.py export speaking speaking-frame-0.ppm --frame 0
../asset_tool.py import speaking speaking-frame-0.ppm --frame 0
../asset_tool.py validate
../asset_tool.py compile
```

Import accepts binary P6 PPM images at exactly 64x32. Colors are mapped to the
nearest shared palette entry. Both full asset colors and the emulator's
default `64/255` brightness colors map back to the same palette.

`generated/mouth_assets.hpp` is a deterministic RLE build artifact. PlatformIO
regenerates it when either the JSON pack or compiler is newer. Never edit it
by hand.
