# Procedural bot animations

This folder extracts the procedural renderer from `BotAnimator_ESP32.ino` into
portable C++17 and includes a generated bitmap pack that the repository's
existing `emulator.py` can display. The renderer itself contains no hardcoded
animation bitmaps; `eye_assets.json` is a sampled, regenerable preview/export
of its RGB565 drawing primitives.

The renderer targets one 240x240 round display at a time. Call it with the same
timestamp for the left and right boards to keep the pair synchronized.

## Extracted catalog

The source sketch exposes 44 selectable variants:

- Eyes (13): idle, look, alert, angry, surprised, happy, suspicious, confused,
  dizzy, listening, glitch, sleep, off.
- Music (7): play, pause, stop, previous/next pair, volume pair, equalizer,
  wave.
- Clock (7): digital, analog, seconds, minimal, stopwatch, timer, alarm.
- Weather (8): sun, clouds, rain, storm, snow, wind, fog, night.
- System (9): power on, power off, update, install, Wi-Fi, battery, scan, OK,
  error.

This is a faithful extraction, including the sketch's shared fallback artwork:
digital/analog/seconds/minimal currently render the same clock layout;
clouds/rain/storm/snow share one cloud shape; and update/install/scan share the
same scan/progress screen. The incoming README mentions blink, but the supplied
`.ino` has no `Blink` enum or blink renderer, so no nonexistent animation was
invented here.

## Files

- `procedural_animations.hpp` is the stable public API and complete state list.
- `procedural_animations.cpp` contains all extracted animation behavior.
- `eye_assets.json` contains the rendered, palette-indexed 240x240 frames in
  the same `sarcasmos-eye-assets` format used by the current emulator. It uses
  the production pack's exact 31 IDs, names, and order.
- `MAPPING.md` records how each production state maps to a procedural source.
- `bitmap_exporter.cpp` rasterizes the real extracted C++ renderer.
- `build_bitmap_pack.py` rebuilds `eye_assets.json` from that rasterizer.
- `adapters/tft_espi_canvas.hpp` connects it to the original Arduino library.
- `tests/smoke_test.cpp` renders both sides of every selectable variant through
  a fake drawing backend.

## View the animations

Run this from the `Eye` directory:

```sh
python emulator.py --assets "new animations/eye_assets.json" --view both
```

Use Left/Right to move through all 31 mapped animations, Up/Down to step frames,
and Space to play or pause. Frame editing, GIMP auto-import, add/remove, timing,
folder synchronization, and orientation controls write directly to this
alternate JSON without changing the production pack or firmware header.

Edits to one eye preserve the other eye's independent artwork. Running
`build_bitmap_pack.py` again deliberately regenerates this file and replaces
manual edits, so copy an edited pack first if the procedural source is also
being changed.

Headless preview and validation work too:

```sh
python emulator.py \
  --assets "new animations/eye_assets.json" \
  --state eye-angry --view both --dump /tmp/eye-angry.ppm
python emulator.py --assets "new animations/eye_assets.json" --self-test
```

Regenerate every bitmap after changing the procedural C++:

```sh
python "new animations/build_bitmap_pack.py"
```

The procedural source still contains all 44 original variants. Generate a
temporary unmapped pack containing all of them with `--all`; see `MAPPING.md`.

## Use later in firmware

Copy this folder into the firmware project and implement `Canvas` using the
display driver's primitive calls. The included adapter already does this for
TFT_eSPI:

```cpp
#include "new animations/adapters/tft_espi_canvas.hpp"

TFT_eSPI tft;
bot_animations::TftEsPiCanvas canvas(tft);
bot_animations::Renderer renderer(canvas);
bot_animations::State animation;

void loop() {
  static uint32_t lastFrameMs = 0;
  const uint32_t nowMs = millis();
  if (nowMs - lastFrameMs < bot_animations::kFrameIntervalMs) return;
  lastFrameMs = nowMs;

  animation.screen = bot_animations::ScreenMode::Eyes;
  animation.eye = bot_animations::EyeState::Angry;

  selectLeftDisplay();
  renderer.render(animation, bot_animations::Side::Left, nowMs);
  selectRightDisplay();
  renderer.render(animation, bot_animations::Side::Right, nowMs);
}
```

When changing a time-relative mode such as power on/off, save its start time:

```cpp
animation.screen = bot_animations::ScreenMode::System;
animation.system = bot_animations::SystemMode::PowerOn;
animation.modeStartedMs = millis();
```

For this repository's RP2040/GC9A01 firmware, keep the public renderer intact
and add a `Canvas` adapter beside `gc9a01.cpp`. That is the only hardware-specific
piece needed. Text rendering and arcs are deliberately part of the adapter
contract because the current low-level driver only streams pixels and does not
yet provide graphics primitives.

## Host validation

From this directory:

```sh
c++ -std=c++17 -Wall -Wextra -Werror \
  procedural_animations.cpp tests/smoke_test.cpp -o /tmp/bot-animation-smoke
/tmp/bot-animation-smoke
```

The original memory rule still applies to firmware: use the procedural renderer
to store zero full-screen frames when possible. The JSON bitmap pack exists for
the desktop emulator and asset interchange. It can also be compiled by the
existing RLE compiler. Its state count and ordering already match the current
firmware protocol; replacing production artwork remains a separate deliberate
integration step.
