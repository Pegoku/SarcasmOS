# Mapping to the production animation protocol

`eye_assets.json` has exactly the production pack's 31 IDs, names, and order.
The `mapped_from` field on every animation records which procedural ESP32 mode
provided its frames.

| ID | Production state | Procedural source | Mapping |
|---:|---|---|---|
| 0 | `idle` | `eye-idle` | Direct |
| 1 | `listening` | `eye-listening` | Direct |
| 2 | `thinking` | `eye-dizzy` | Orbiting pupils used as thinking motion |
| 3 | `thinking_audio` | `eye-listening` | Reactive pupil sizing used as audio motion |
| 4 | `thinking_long` | `eye-look` | Slow scanning gaze |
| 5 | `speaking` | `eye-happy` | Closest confident squint; source has no speaking eyes |
| 6 | `happy_fake` | `eye-happy` | Direct |
| 7 | `angry` | `eye-angry` | Direct |
| 8 | `error` | `system-error` | Explicit error screen rather than X-shaped eyes |
| 9 | `asleep` | `eye-sleep` | Direct |
| 10 | `tool` | `eye-angry` | Closest concentrated inward squint |
| 11 | `left` | `eye-look`, frame 7 | Static leftward point in the scan |
| 12 | `right` | `eye-look`, frame 3 | Static rightward point in the scan |
| 13 | `up` | `eye-look`, frame 8 | Closest upward point in the scan |
| 14 | `down` | `eye-look`, frame 3 | Closest downward point in the scan |
| 15 | `center` | `eye-idle`, frame 1 | Static near-center gaze |
| 16 | `neutral` | `eye-idle`, frame 1 | Static near-center gaze |
| 17 | `sarcastic` | `eye-confused` | Asymmetric lids and pupils |
| 18 | `suspicious` | `eye-suspicious` | Direct |
| 19 | `tired` | `eye-sleep` | Closed-eye fallback; no separate tired mode upstream |
| 20 | `surprised` | `eye-surprised` | Direct |
| 21 | `bored` | `eye-suspicious`, frame 1 | Static narrow-eye fallback |
| 22 | `dramatic` | `eye-alert` | Pulsing red alert expression |
| 23 | `watch` | `eye-look` | Scanning gaze |
| 24 | `party` | `eye-glitch` | Animated glitch fallback |
| 25 | `battery_low` | `system-battery` | Explicit battery status screen |
| 26 | `sunny` | `weather-sun` | Direct |
| 27 | `rainy` | `weather-rain` | Named source mode; upstream uses its cloud placeholder |
| 28 | `cloudy` | `weather-clouds` | Direct |
| 29 | `stormy` | `weather-storm` | Named source mode; upstream uses its cloud placeholder |
| 30 | `snowy` | `weather-snow` | Named source mode; upstream uses its cloud placeholder |

The directional selections use one-based frame numbers in this table; the
generator stores zero-based indices internally. A mapping marked as a fallback
does not claim that the external sketch implemented the production behavior.

The 13 unselected source modes remain in the procedural C++ and can be exported
for inspection with:

```sh
python "new animations/build_bitmap_pack.py" \
  --all --output /tmp/all-procedural-eye-assets.json
python emulator.py \
  --assets /tmp/all-procedural-eye-assets.json --view both
```
