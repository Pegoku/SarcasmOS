// SPDX-License-Identifier: MIT

#include "mouth_display.hpp"

#include <Arduino.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>

#include "generated/mouth_assets.hpp"
#include "protocol.hpp"

namespace mouth_display {
namespace {

using namespace mouth_protocol;

constexpr int kPanelWidth = 64;
constexpr int kPanelHeight = 32;
constexpr int kPanelChain = 1;
constexpr uint32_t kFrameIntervalMs = 40;

// Custom driver PCB pin mapping, traced through U5/U1 to HUB75.
constexpr int kR1Pin = 1;
constexpr int kG1Pin = 2;
constexpr int kB1Pin = 3;
constexpr int kR2Pin = 5;
constexpr int kG2Pin = 4;
constexpr int kB2Pin = 6;
constexpr int kAPin = 8;
constexpr int kBPin = 7;
constexpr int kCPin = 10;
constexpr int kDPin = 9;
constexpr int kEPin = -1;
constexpr int kLatchPin = 11;
constexpr int kOePin = 13;
constexpr int kClockPin = 12;

static_assert(mouth_assets::kWidth == kPanelWidth, "asset width mismatch");
static_assert(mouth_assets::kHeight == kPanelHeight, "asset height mismatch");
static_assert(mouth_assets::kAnimationCount == kAnimCount,
              "asset animation count mismatch");

MatrixPanel_I2S_DMA *matrix = nullptr;
uint8_t currentAnimation = kAnimIdle;
uint8_t currentBrightness = kDefaultBrightness;
uint8_t currentMouthIntensity = kDefaultMouthIntensity;
uint32_t syncPhaseMs = 0;
uint32_t lastFrameMs = 0;

uint16_t rgb(uint8_t red, uint8_t green, uint8_t blue) {
    return matrix->color565(red, green, blue);
}

void present() {
    matrix->flipDMABuffer();
}

uint8_t animationFrame(const mouth_assets::Animation &animation,
                       uint32_t elapsedMs) {
    if (animation.frameCount <= 1) return 0;

    const uint32_t step = elapsedMs / animation.frameMs;
    if (animation.playback == mouth_assets::kPlaybackPingPong) {
        const uint8_t finalFrame = animation.frameCount - 1;
        const uint16_t phase = step % (finalFrame * 2);
        return phase <= finalFrame ? phase : finalFrame * 2 - phase;
    }
    if (animation.playback == mouth_assets::kPlaybackIntensity) {
        const uint8_t finalFrame = animation.frameCount - 1;
        const uint16_t phase = step % (finalFrame * 2);
        const uint16_t level =
            phase <= finalFrame ? phase : finalFrame * 2 - phase;
        return min(static_cast<int>(finalFrame),
                   static_cast<int>(level) * currentMouthIntensity / 120);
    }
    return step % animation.frameCount;
}

void drawAssetFrame(uint8_t spriteId) {
    const mouth_assets::Frame &frame = mouth_assets::kFrames[spriteId];
    uint32_t pixel = 0;
    for (uint32_t offset = frame.offset;
         offset < frame.offset + frame.length; offset += 2) {
        uint16_t run = mouth_assets::kRleData[offset];
        const uint8_t paletteIndex = mouth_assets::kRleData[offset + 1];
        const uint8_t *color = mouth_assets::kPalette[paletteIndex];
        const uint16_t packed = rgb(color[0], color[1], color[2]);
        while (run > 0 && pixel < kPanelWidth * kPanelHeight) {
            const uint8_t x = pixel % kPanelWidth;
            const uint8_t y = pixel / kPanelWidth;
            const uint8_t chunk =
                min(static_cast<int>(run), kPanelWidth - x);
            matrix->drawFastHLine(x, y, chunk, packed);
            pixel += chunk;
            run -= chunk;
        }
    }
}

void drawMouth(uint32_t elapsedMs) {
    matrix->clearScreen();
    const uint8_t animationId =
        isValidAnimation(currentAnimation) ? currentAnimation : kAnimNeutral;
    const mouth_assets::Animation &animation =
        mouth_assets::kAnimations[animationId];
    const uint8_t localFrame = animationFrame(animation, elapsedMs);
    const uint8_t spriteId = mouth_assets::kFrameReferences[
        animation.firstFrameReference + localFrame];
    drawAssetFrame(spriteId);
    present();
}

}  // namespace

bool begin() {
    HUB75_I2S_CFG::i2s_pins pins = {
        kR1Pin, kG1Pin, kB1Pin, kR2Pin, kG2Pin, kB2Pin, kAPin,
        kBPin,  kCPin,  kDPin,  kEPin,  kLatchPin, kOePin, kClockPin,
    };
    HUB75_I2S_CFG config(kPanelWidth, kPanelHeight, kPanelChain, pins);
    config.double_buff = true;
    // This panel samples RGB on the opposite clock edge. The default phase
    // rotates the scan chain by one pixel (1..63,0 instead of 0..63).
    config.clkphase = false;

    matrix = new MatrixPanel_I2S_DMA(config);
    if (matrix == nullptr || !matrix->begin()) return false;
    matrix->setBrightness8(currentBrightness);
    matrix->clearScreen();
    present();
    showNow();
    return true;
}

void update() {
    const uint32_t now = millis();
    if (now - lastFrameMs >= kFrameIntervalMs) {
        lastFrameMs = now;
        drawMouth(now + syncPhaseMs);
    }
}

void showNow() {
    if (matrix != nullptr) drawMouth(millis() + syncPhaseMs);
}

void setAnimation(uint8_t animationValue) {
    currentAnimation = animationValue;
}

uint8_t animation() {
    return currentAnimation;
}

void setBrightness(uint8_t brightnessValue) {
    currentBrightness = brightnessValue;
    if (matrix != nullptr) matrix->setBrightness8(currentBrightness);
}

uint8_t brightness() {
    return currentBrightness;
}

void setMouthIntensity(uint8_t intensity) {
    currentMouthIntensity = intensity;
}

uint8_t mouthIntensity() {
    return currentMouthIntensity;
}

void setSyncPhase(uint32_t phaseMs) {
    syncPhaseMs = phaseMs;
}

void showSolid(uint8_t red, uint8_t green, uint8_t blue) {
    if (matrix == nullptr) return;
    matrix->fillScreen(rgb(red, green, blue));
    present();
}

void showColorBars() {
    if (matrix == nullptr) return;
    constexpr uint8_t colors[][3] = {
        {255, 0, 0},   {0, 255, 0},     {0, 0, 255},   {255, 255, 0},
        {0, 255, 255}, {255, 0, 255},   {255, 255, 255}, {0, 0, 0},
    };
    matrix->clearScreen();
    constexpr int kBarWidth = kPanelWidth / 8;
    for (int bar = 0; bar < 8; ++bar) {
        matrix->fillRect(bar * kBarWidth, 0, kBarWidth, kPanelHeight,
                         rgb(colors[bar][0], colors[bar][1], colors[bar][2]));
    }
    present();
}

void showRgbRows() {
    if (matrix == nullptr) return;
    matrix->clearScreen();
    matrix->fillRect(0, 0, kPanelWidth, 10, rgb(255, 0, 0));
    matrix->fillRect(0, 10, kPanelWidth, 11, rgb(0, 255, 0));
    matrix->fillRect(0, 21, kPanelWidth, 11, rgb(0, 0, 255));
    present();
}

void showGeometryTest() {
    if (matrix == nullptr) return;
    matrix->clearScreen();
    matrix->drawRect(0, 0, kPanelWidth, kPanelHeight, rgb(255, 255, 255));
    matrix->drawLine(0, 0, kPanelWidth - 1, kPanelHeight - 1,
                     rgb(255, 0, 0));
    matrix->drawLine(kPanelWidth - 1, 0, 0, kPanelHeight - 1,
                     rgb(0, 255, 0));
    matrix->fillRect(1, 1, 5, 5, rgb(255, 0, 0));
    matrix->fillRect(kPanelWidth - 6, 1, 5, 5, rgb(0, 255, 0));
    matrix->fillRect(1, kPanelHeight - 6, 5, 5, rgb(0, 0, 255));
    matrix->fillRect(kPanelWidth - 6, kPanelHeight - 6, 5, 5,
                     rgb(255, 255, 255));
    present();
}

}  // namespace mouth_display
