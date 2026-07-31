#pragma once

#include <cstdint>

namespace mouth_display {

constexpr uint8_t kDefaultBrightness = 64;
constexpr uint8_t kDefaultMouthIntensity = 120;
constexpr uint16_t kDefaultTransitionDurationMs = 200;
constexpr uint16_t kRendererFrameIntervalMs = 40;

bool begin();
void update();
void showNow();

bool requestAnimation(
    uint8_t animation, uint8_t transitionToken = 0,
    uint16_t transitionDurationMs = kDefaultTransitionDurationMs
);
// Immediate state selection for diagnostics and initialization.
void setAnimation(uint8_t animation);
uint8_t animation();
bool transitionActive();
uint8_t transitionToken();
uint8_t transitionProgress();

void setBrightness(uint8_t brightness);
uint8_t brightness();

void setMouthIntensity(uint8_t intensity);
uint8_t mouthIntensity();

void setTemperatureCelsius(int8_t temperature);
int8_t temperatureCelsius();

void setSyncPhase(uint32_t phaseMs);

void showChannel(uint8_t channel, uint16_t durationMs = 1200);

void showSolid(uint8_t red, uint8_t green, uint8_t blue);
void showColorBars();
void showRgbRows();
void showGeometryTest();

}  // namespace mouth_display
