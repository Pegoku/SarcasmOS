# Procedural bot animations

This folder extracts the procedural renderer from `BotAnimator_ESP32.ino` into
portable C++17. It contains no PNGs, GIFs, frame arrays, generated sprites, or
other hardcoded bitmaps. Every frame is rebuilt from RGB565 drawing primitives,
math, the selected state, and a caller-supplied timestamp.

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
- `adapters/tft_espi_canvas.hpp` connects it to the original Arduino library.
- `tests/smoke_test.cpp` renders both sides of every selectable variant through
  a fake drawing backend.

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

The original memory rule still applies: render directly to the display when
possible. The package stores zero full-screen frames and does not require a
framebuffer.
