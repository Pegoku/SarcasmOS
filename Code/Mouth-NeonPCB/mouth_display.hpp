#pragma once

#include <cstdint>

namespace mouth_display {

constexpr uint8_t kDefaultBrightness = 64;
constexpr uint8_t kDefaultMouthIntensity = 120;

bool begin();
void update();
void showNow();

void setAnimation(uint8_t animation);
uint8_t animation();

void setBrightness(uint8_t brightness);
uint8_t brightness();

void setMouthIntensity(uint8_t intensity);
uint8_t mouthIntensity();

void setSyncPhase(uint32_t phaseMs);

void showSolid(uint8_t red, uint8_t green, uint8_t blue);
void showColorBars();
void showRgbRows();
void showGeometryTest();

}  // namespace mouth_display
