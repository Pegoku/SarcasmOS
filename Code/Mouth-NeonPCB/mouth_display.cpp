// SPDX-License-Identifier: MIT

#include "mouth_display.hpp"

#include <Arduino.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <array>
#include <cstdio>
#include <cstring>

#include "color_transition.hpp"
#include "generated/mouth_assets.hpp"
#include "generated/temperature_font.hpp"
#include "protocol.hpp"

namespace mouth_display {
namespace {

using namespace mouth_protocol;
using namespace mouth_temperature_font;

constexpr int kPanelWidth = 64;
constexpr int kPanelHeight = 32;
constexpr int kPanelChain = 1;
constexpr size_t kPixelCount = kPanelWidth * kPanelHeight;
using Canvas = std::array<uint16_t, kPixelCount>;

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
int8_t currentTemperatureCelsius = kTemperatureUnavailable;
uint32_t syncPhaseMs = 0;
uint32_t lastFrameMs = 0;
uint32_t animationStartedMs = 0;
uint8_t channelNotice = 0;
uint32_t channelNoticeUntilMs = 0;
Canvas transitionSource = {};
Canvas destinationCanvas = {};

constexpr uint8_t kChannelGlyphH[kGlyphHeight] = {
    0b10001, 0b10001, 0b10001, 0b11111,
    0b10001, 0b10001, 0b10001,
};

struct TransitionState {
    bool active = false;
    uint8_t destinationAnimation = kAnimIdle;
    uint8_t token = 0;
    uint32_t startedMs = 0;
    uint16_t durationMs = kDefaultTransitionDurationMs;
};

TransitionState transition = {};

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

void decodeAssetFrame(uint8_t spriteId, Canvas &canvas) {
    const mouth_assets::Frame &frame = mouth_assets::kFrames[spriteId];
    uint32_t pixel = 0;
    for (uint32_t offset = frame.offset;
         offset < frame.offset + frame.length; offset += 2) {
        uint16_t run = mouth_assets::kRleData[offset];
        const uint8_t paletteIndex = mouth_assets::kRleData[offset + 1];
        const uint8_t *color = mouth_assets::kPalette[paletteIndex];
        const uint16_t packed =
            mouth_transition::packRgb565(color[0], color[1], color[2]);
        while (run > 0 && pixel < kPixelCount) {
            canvas[pixel++] = packed;
            --run;
        }
    }
    while (pixel < kPixelCount) canvas[pixel++] = 0;
}

bool isWeatherAnimation(uint8_t animationId) {
    return animationId >= kAnimSunny && animationId <= kAnimSnowy;
}

const uint8_t *glyphFor(char character) {
    if (character >= '0' && character <= '9') {
        return kDigitGlyphs[character - '0'];
    }
    if (character == '-') return kMinusGlyph;
    if (character == 'C') return kCelsiusGlyph;
    return kDegreeGlyph;
}

void drawGlyph(Canvas &canvas, int x, int y, const uint8_t *rows) {
    for (uint8_t row = 0; row < kGlyphHeight; ++row) {
        for (uint8_t column = 0; column < kGlyphWidth; ++column) {
            if ((rows[row] & (1u << (kGlyphWidth - column - 1))) != 0) {
                canvas[(y + row) * kPanelWidth + x + column] = 0;
            }
        }
    }
}

void drawTemperatureOverlay(uint8_t animationId, Canvas &canvas) {
    if (!isWeatherAnimation(animationId) ||
        currentTemperatureCelsius == kTemperatureUnavailable) {
        return;
    }

    char digits[5] = {};
    std::snprintf(digits, sizeof(digits), "%d",
                  static_cast<int>(currentTemperatureCelsius));
    const size_t digitCount = strlen(digits);
    const size_t glyphCount = digitCount + 2;  // degree symbol and C
    const int textWidth =
        glyphCount * kGlyphWidth + (glyphCount - 1) * kGlyphSpacing;
    int x = (kPanelWidth - textWidth) / 2;
    constexpr int y = (kPanelHeight - kGlyphHeight) / 2;

    for (size_t index = 0; index < digitCount; ++index) {
        drawGlyph(canvas, x, y, glyphFor(digits[index]));
        x += kGlyphWidth + kGlyphSpacing;
    }
    drawGlyph(canvas, x, y, kDegreeGlyph);
    x += kGlyphWidth + kGlyphSpacing;
    drawGlyph(canvas, x, y, kCelsiusGlyph);
}

void drawScaledGlyph(
    Canvas &canvas, int x, int y, const uint8_t *rows,
    uint16_t color, uint8_t scale
) {
    for (uint8_t row = 0; row < kGlyphHeight; ++row) {
        for (uint8_t column = 0; column < kGlyphWidth; ++column) {
            if ((rows[row] & (1u << (kGlyphWidth - column - 1))) == 0) {
                continue;
            }
            for (uint8_t dy = 0; dy < scale; ++dy) {
                for (uint8_t dx = 0; dx < scale; ++dx) {
                    canvas[(y + row * scale + dy) * kPanelWidth +
                           x + column * scale + dx] = color;
                }
            }
        }
    }
}

void renderChannelNotice(Canvas &canvas) {
    canvas.fill(0);
    char digits[3] = {};
    std::snprintf(digits, sizeof(digits), "%u", channelNotice);
    const size_t digitCount = strlen(digits);
    constexpr uint8_t scale = 2;
    constexpr int glyphWidth = kGlyphWidth * scale;
    constexpr int glyphSpacing = kGlyphSpacing * scale;
    constexpr int glyphHeight = kGlyphHeight * scale;
    const size_t glyphCount = digitCount + 2;
    const int textWidth =
        glyphCount * glyphWidth + (glyphCount - 1) * glyphSpacing;
    int x = (kPanelWidth - textWidth) / 2;
    constexpr int y = (kPanelHeight - glyphHeight) / 2;
    const uint16_t yellow = mouth_transition::packRgb565(255, 220, 32);

    drawScaledGlyph(canvas, x, y, kCelsiusGlyph, yellow, scale);
    x += glyphWidth + glyphSpacing;
    drawScaledGlyph(canvas, x, y, kChannelGlyphH, yellow, scale);
    x += glyphWidth + glyphSpacing;
    for (size_t index = 0; index < digitCount; ++index) {
        drawScaledGlyph(
            canvas, x, y, glyphFor(digits[index]), yellow, scale
        );
        x += glyphWidth + glyphSpacing;
    }
}

void renderAnimation(
    uint8_t animationId, uint32_t now, Canvas &canvas
) {
    if (!isValidAnimation(animationId)) animationId = kAnimNeutral;
    const mouth_assets::Animation &animation =
        mouth_assets::kAnimations[animationId];
    const uint32_t elapsedMs =
        now - animationStartedMs + syncPhaseMs;
    const uint8_t localFrame = animationFrame(animation, elapsedMs);
    const uint8_t spriteId = mouth_assets::kFrameReferences[
        animation.firstFrameReference + localFrame];
    decodeAssetFrame(spriteId, canvas);
    drawTemperatureOverlay(animationId, canvas);
}

uint8_t transitionProgressAt(uint32_t now) {
    if (!transition.active) return 255;
    const uint32_t elapsed = now - transition.startedMs;
    if (elapsed >= transition.durationMs) return 255;
    return static_cast<uint8_t>(
        (elapsed * 255U) / transition.durationMs
    );
}

void presentCanvas(const Canvas &canvas) {
    for (int y = 0; y < kPanelHeight; ++y) {
        int x = 0;
        while (x < kPanelWidth) {
            const uint16_t color = canvas[y * kPanelWidth + x];
            int end = x + 1;
            while (end < kPanelWidth &&
                   canvas[y * kPanelWidth + end] == color) {
                ++end;
            }
            matrix->drawFastHLine(x, y, end - x, color);
            x = end;
        }
    }
    present();
}

void presentBlend(
    const Canvas &from, const Canvas &to, uint8_t amount
) {
    for (int y = 0; y < kPanelHeight; ++y) {
        for (int x = 0; x < kPanelWidth; ++x) {
            const size_t pixel = y * kPanelWidth + x;
            matrix->drawPixel(
                x, y,
                mouth_transition::blendRgb565(
                    from[pixel], to[pixel], amount
                )
            );
        }
    }
    present();
}

void captureVisible(uint32_t now) {
    renderAnimation(currentAnimation, now, destinationCanvas);
    if (!transition.active) {
        transitionSource = destinationCanvas;
        return;
    }
    const uint8_t amount = transitionProgressAt(now);
    for (size_t pixel = 0; pixel < kPixelCount; ++pixel) {
        transitionSource[pixel] = mouth_transition::blendRgb565(
            transitionSource[pixel], destinationCanvas[pixel], amount
        );
    }
}

void drawMouth(uint32_t now) {
    renderAnimation(currentAnimation, now, destinationCanvas);
    if (channelNotice != 0 &&
        static_cast<int32_t>(channelNoticeUntilMs - now) > 0) {
        renderChannelNotice(destinationCanvas);
        presentCanvas(destinationCanvas);
        return;
    }
    channelNotice = 0;
    if (transition.active) {
        const uint8_t amount = transitionProgressAt(now);
        if (amount < 255) {
            presentBlend(transitionSource, destinationCanvas, amount);
            return;
        }
        transition.active = false;
    }
    presentCanvas(destinationCanvas);
}

void cancelTransition() {
    transition.active = false;
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
    animationStartedMs = millis();
    showNow();
    return true;
}

void update() {
    const uint32_t now = millis();
    if (now - lastFrameMs >= kRendererFrameIntervalMs) {
        lastFrameMs = now;
        drawMouth(now);
    }
}

void showNow() {
    if (matrix != nullptr) drawMouth(millis());
}

bool requestAnimation(
    uint8_t animationValue, uint8_t token, uint16_t durationMs
) {
    if (!isValidAnimation(animationValue)) return false;
    if (animationValue == currentAnimation && token == transition.token) {
        return true;
    }

    const uint32_t now = millis();
    captureVisible(now);
    currentAnimation = animationValue;
    animationStartedMs = now;
    syncPhaseMs = 0;
    transition.destinationAnimation = animationValue;
    transition.token = token;
    transition.startedMs = now;
    transition.durationMs =
        durationMs == 0 ? kDefaultTransitionDurationMs : durationMs;
    transition.active = transition.durationMs > 0;
    return true;
}

void setAnimation(uint8_t animationValue) {
    currentAnimation =
        isValidAnimation(animationValue) ? animationValue : kAnimNeutral;
    animationStartedMs = millis();
    syncPhaseMs = 0;
    cancelTransition();
}

uint8_t animation() {
    return currentAnimation;
}

bool transitionActive() {
    return transition.active && transitionProgressAt(millis()) < 255;
}

uint8_t transitionToken() {
    return transition.token;
}

uint8_t transitionProgress() {
    return transitionProgressAt(millis());
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

void setTemperatureCelsius(int8_t temperature) {
    currentTemperatureCelsius = temperature;
}

int8_t temperatureCelsius() {
    return currentTemperatureCelsius;
}

void setSyncPhase(uint32_t phaseMs) {
    syncPhaseMs = phaseMs;
}

void showChannel(uint8_t channel, uint16_t durationMs) {
    channelNotice = channel;
    channelNoticeUntilMs = millis() + durationMs;
    showNow();
}

void showSolid(uint8_t red, uint8_t green, uint8_t blue) {
    if (matrix == nullptr) return;
    cancelTransition();
    matrix->fillScreen(rgb(red, green, blue));
    present();
}

void showColorBars() {
    if (matrix == nullptr) return;
    cancelTransition();
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
    cancelTransition();
    matrix->clearScreen();
    matrix->fillRect(0, 0, kPanelWidth, 10, rgb(255, 0, 0));
    matrix->fillRect(0, 10, kPanelWidth, 11, rgb(0, 255, 0));
    matrix->fillRect(0, 21, kPanelWidth, 11, rgb(0, 0, 255));
    present();
}

void showGeometryTest() {
    if (matrix == nullptr) return;
    cancelTransition();
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
