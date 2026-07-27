# Mouth animation assets

`mouth_assets.json` is the single source of truth for both the desktop
emulator and ESP32 firmware. Artwork and animation timing must be changed here,
not in `mouth_display.cpp`, `emulator.py`, or the generated C++ header.

Live weather temperature text is a dynamic black overlay and is intentionally
not baked into these sprites. Editing a weather frame in GIMP therefore edits
only the underlying animation; firmware and emulator add the current `°C`
value after rendering the selected frame. Its small pixel glyphs live in
`temperature_font.json`, which is also shared by both renderers.

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
   preserves new colors in available palette slots, updates this pack,
   regenerates the firmware header, and reloads the preview automatically.
   **Import saved edit** (`i`) remains available as a manual refresh.

When an animation is opened with `o`, its files form an editable, one-based
sequence:

- `watch-frame-01.ppm` is the first frame and `watch-frame-09.ppm` is the
  ninth.
- Saving a new `watch-frame-10.ppm` adds a tenth frame.
- A number beyond the next position is accepted and compacted. For example,
  adding `watch-frame-12.ppm` to a nine-frame sequence renames it to
  `watch-frame-10.ppm`.
- Deleting frame 7 removes it; old frames 8 and 9 are renamed to 7 and 8.
- At least one frame must remain. An empty animation is rejected.

The animation JSON, sprite records, generated header, displayed frame count,
and temporary PPM filenames update together after the files remain stable for
250 ms. Sequence editing is available in the folder opened by **Open
animation**, not the separate single-frame edit folder.

The emulator also provides direct **Add frame after current** and **Remove
current frame** buttons:

- adding while viewing frame 2 creates a separate copy as frame 3;
- the old frame 3 becomes frame 4, and every later frame shifts likewise;
- the new copy is selected immediately and can be edited independently;
- removing a frame shifts every later frame backward and selects the frame
  that moved into its position, or the new last frame;
- the final remaining frame cannot be removed.

`Insert` and `Delete` are keyboard shortcuts for these buttons. Structural
changes invalidate any older GIMP edit session for that animation; reopen it
with `o` if continued multi-frame editing is needed.

Use **Set frame time** (`T`) to change the current animation's delay between
frames. The dialog accepts `1..65535` milliseconds and updates the
animation's `frame_ms` value, emulator timing, and generated firmware header
without changing any frame artwork.

Use **Sync folder** (`S`) after restarting the emulator, or whenever an
existing `/tmp/sarcasmos-mouth-edit/animations/<state>` folder is not attached
to the current session. It imports all correctly numbered PPMs without first
exporting over them, reloads the preview, and resumes automatic watching of
that folder. By contrast, **Open animation in GIMP** exports the current asset
frames before opening them, so it should not be used to recover newer
unimported files.

**Copy frame** (`c`) copies a 64x32 PNG for sharing or attaching to a request.

## Command-line editing

```sh
../asset_tool.py export speaking speaking-frame-0.ppm --frame 0
../asset_tool.py import speaking speaking-frame-0.ppm --frame 0
../asset_tool.py validate
../asset_tool.py compile
```

Import accepts binary P6 PPM images at exactly 64x32. Existing full-brightness
and emulator `64/255` colors map back to their original palette entries. A new
exact RGB color is added to the shared palette while one of its 16 slots is
free; only additional colors after the palette is full are mapped to the
nearest entry.

`generated/mouth_assets.hpp` and `generated/temperature_font.hpp` are
deterministic build artifacts. PlatformIO regenerates them when their source
asset or compiler is newer. Never edit either generated header by hand.
