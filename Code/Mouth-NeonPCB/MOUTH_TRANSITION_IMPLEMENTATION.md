# Mouth-NeonPCB animation transition implementation

## Scope

This document describes the changes required in `Code/Mouth-NeonPCB`. It is an
implementation plan only; none of the changes below are implemented by this
document.

The mouth must not decide when the eyes have finished their outgoing
animation. The Brain owns that synchronization barrier. The mouth receives its
new animation only after the Brain has confirmed that both eyes activated the
same target state. When that command arrives, the mouth blends from its
currently displayed image into the first frame of the requested animation
instead of switching instantly.

See also:

- [`EYE_TRANSITION_IMPLEMENTATION.md`](EYE_TRANSITION_IMPLEMENTATION.md)
- [`BRAIN_TRANSITION_IMPLEMENTATION.md`](BRAIN_TRANSITION_IMPLEMENTATION.md)

## Required visible behavior

1. Keep rendering the current mouth animation while the Brain waits for both
   eyes.
2. On a new `SET_ANIMATION`, capture the exact currently visible mouth image.
3. Start the destination animation at frame zero.
4. Blend from the captured image to the live destination image over 200 ms.
5. Continue advancing the destination animation during the blend.
6. At the end of the blend, render the destination normally.

Use a 200 ms default because the renderer updates every 40 ms. This produces
five intermediate steps plus the final destination without making state
changes feel sluggish. Keep the duration as one named constant so it can be
tuned after testing on the physical panel.

Brightness must remain controlled by the HUB75 driver. Do not implement the
transition by changing global panel brightness or by fading through black.

## Renderer changes

The current `drawMouth()` in `mouth_display.cpp` clears the panel, decodes one
RLE sprite directly into the DMA drawing buffer, overlays temperature text,
and flips the buffer. Refactor rendering into these stages:

```text
select animation frame
        |
decode sprite into a 64x32 RGB565 canvas
        |
apply temperature overlay to that canvas
        |
optionally blend outgoing canvas with destination canvas
        |
copy result to HUB75 back buffer and present
```

Add a small canvas abstraction:

```cpp
constexpr size_t kPixelCount = kPanelWidth * kPanelHeight;
using Canvas = std::array<uint16_t, kPixelCount>;
```

Two 64x32 RGB565 canvases cost 8192 bytes. Prefer two canvases:

- `outgoingCanvas`: frozen image captured when the transition begins.
- `destinationCanvas`: the current destination frame rendered each update.

If internal RAM is tight, retain only `outgoingCanvas` and render/blend the
destination one scan line at a time. Do not allocate these buffers on the
task stack.

Replace the direct-draw helpers with canvas equivalents:

- `decodeAssetFrame(uint8_t spriteId, Canvas &canvas)`
- `drawTemperatureOverlay(uint8_t animationId, Canvas &canvas)`
- `blendCanvas(const Canvas &from, const Canvas &to, uint8_t amount, Canvas &out)`
- `presentCanvas(const Canvas &canvas)`

Blend the RGB565 channels separately. Convert the packed components to at
least 8-bit intermediates, interpolate using integer arithmetic, then repack:

```text
output = from + ((to - from) * amount + 127) / 255
```

Do not interpolate the packed 16-bit value directly; that mixes color
channels and produces incorrect colors.

## Playback and transition state

Add explicit state instead of overloading `currentAnimation`:

```cpp
struct TransitionState {
    bool active;
    uint8_t sourceAnimation;
    uint8_t destinationAnimation;
    uint8_t token;
    uint32_t startedMs;
    uint32_t durationMs;
};
```

Also track `animationStartedMs`. The current renderer derives its frame from
absolute `millis() + syncPhaseMs`, so changing animations can enter a new
sequence in the middle. After this change, frame selection should use:

```text
elapsed = now - animationStartedMs + syncPhaseMs
```

On an accepted destination:

1. Render and capture the currently visible image into `outgoingCanvas`.
2. Set `currentAnimation` to the destination.
3. Set `animationStartedMs = now`.
4. Set transition progress to zero.
5. Render destination frame zero into `destinationCanvas`.

If the same animation is requested again with a new token, treat it as a
timeline restart only when the Brain explicitly requests it. If both the
animation and token are duplicates, acknowledge without restarting.

If another destination arrives during a blend, capture the currently blended
result as the new outgoing canvas and begin a fresh transition. This prevents
a flash back to the original source frame.

`showSolid()`, RGB diagnostics, and geometry tests should cancel any active
transition because they bypass normal animation rendering.

## Protocol changes

Keep the existing command ID. Extend `SET_ANIMATION` and `SET_EXPRESSION`
payloads from one meaningful byte to:

| Offset | Meaning |
| ---: | --- |
| 0 | Destination animation ID |
| 1 | Transition token assigned by the Brain |
| 2 | Transition duration in 40 ms ticks; `0` selects the firmware default |

For backward compatibility:

- A one-byte payload remains valid and uses the default duration.
- The currently sent two-byte payload `{animation, 0}` remains valid.
- A three-byte payload enables the complete synchronized contract.

Increment the application protocol version when the extended status is
introduced across Mouth-NeonPCB and Brain. Add these fields to the mouth
status payload:

- last activated transition token;
- transition active flag;
- transition progress `0..255`.

An acknowledgement means the command was accepted and the blend was started.
It does not need to wait 200 ms for the blend to finish.

Update the matching definitions in:

- `protocol.hpp`;
- `README.md`;
- the Brain's `components/mouth_espnow/include/display_protocol.h`;
- the Brain's mouth status decoder.

## Suggested API changes

In `mouth_display.hpp` expose:

```cpp
bool requestAnimation(
    uint8_t animation,
    uint8_t transitionToken,
    uint16_t transitionDurationMs
);
bool transitionActive();
uint8_t transitionToken();
uint8_t transitionProgress();
```

Replace direct calls to `setAnimation()` in `main.cpp` with
`requestAnimation()`. Keep `setAnimation()` only for local diagnostics if it
is explicitly documented as an immediate, no-transition operation.

The local animation-test firmware and desktop emulator must use the same
transition algorithm and duration. Add an option to disable transitions for
individual-frame editing so an editor can still inspect exact source sprites.

## Verification

Add host-testable transition math where possible and verify:

1. Progress zero equals the captured outgoing canvas exactly.
2. Progress 255 equals the destination canvas exactly.
3. RGB channels interpolate independently.
4. A mid-transition command begins from the currently visible blended image.
5. The destination animation starts at frame zero.
6. Weather temperature text participates in the blend and appears only once.
7. Repeated ESP-NOW packets with the same sequence and token do not restart
   the blend.
8. The mouth remains on its old state while Brain is still waiting for an
   eye.
9. The physical panel changes only after both eyes report the matching token.

Run at minimum:

```sh
pio run -e custom_esp32s3_mini_n8
pio run -e local_animation_test
./asset_tool.py validate
```

Do not upload automatically as part of implementation or testing.
