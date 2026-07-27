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

1. Pause playback with Space. Use Up/Down to select an exact frame.
2. **Open frame in GIMP** (`e`) to export and open the current native frame,
   or **Open animation in GIMP** (`o`) to open every frame for the state.
3. Edit without resizing the 64x32 canvas. Disable interpolation when scaling
   the view; individual pixels are intentional.
4. Overwrite the opened PPM from GIMP. The emulator detects the overwrite,
   quantizes it to the palette, updates this pack, regenerates the firmware
   header, and reloads the preview automatically. **Import saved edit** (`i`)
   remains available as a manual refresh.

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
