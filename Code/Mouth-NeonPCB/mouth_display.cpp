// SPDX-License-Identifier: MIT

#include "mouth_display.hpp"

#include <Arduino.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>

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

void drawThickLine(int x0, int y0, int x1, int y1, uint16_t color) {
    matrix->drawLine(x0, y0, x1, y1, color);
    matrix->drawLine(x0, y0 + 1, x1, y1 + 1, color);
}

void drawBenderTeeth(bool speaking, uint32_t tick) {
    const uint16_t teeth = rgb(255, 246, 166);
    const uint16_t seam = rgb(0, 0, 0);

    // Bender's mouth is a rounded, pale tooth panel with a heavy dark edge.
    matrix->fillRoundRect(1, 1, 62, 30, 14, seam);
    matrix->fillRoundRect(3, 3, 58, 26, 12, teeth);

    // Five columns, matching the grille in the reference mouth.
    constexpr int kColumnSeams[] = {15, 27, 39, 51};
    for (const int x : kColumnSeams) {
        matrix->drawFastVLine(x, 3, 26, seam);
        matrix->drawFastVLine(x + 1, 3, 26, seam);
    }

    if (!speaking) {
        // Three rows for the resting tooth grille.
        matrix->drawFastHLine(3, 11, 58, seam);
        matrix->drawFastHLine(3, 12, 58, seam);
        matrix->drawFastHLine(3, 20, 58, seam);
        matrix->drawFastHLine(3, 21, 58, seam);
        return;
    }

    // Talking replaces the horizontal grille seams with two lines that split
    // into a changing waveform and close again at the right edge.
    const int phase = tick % 24;
    const int triangle = phase <= 12 ? phase : 24 - phase;
    int amplitude = 2 + (triangle * 6) / 12;
    amplitude = constrain(
        (amplitude * static_cast<int>(currentMouthIntensity)) / 120, 1, 9);

    constexpr int kCenterY = 16;
    drawThickLine(3, kCenterY - 1, 27, kCenterY - 1, seam);
    drawThickLine(3, kCenterY + 1, 27, kCenterY + 1, seam);

    drawThickLine(27, kCenterY - 1, 43, kCenterY - amplitude, seam);
    drawThickLine(43, kCenterY - amplitude, 52, kCenterY - amplitude / 2,
                  seam);
    drawThickLine(52, kCenterY - amplitude / 2, 60, kCenterY - 1, seam);

    drawThickLine(27, kCenterY + 1, 43, kCenterY + amplitude, seam);
    drawThickLine(43, kCenterY + amplitude, 52, kCenterY + amplitude / 2,
                  seam);
    drawThickLine(52, kCenterY + amplitude / 2, 60, kCenterY + 1, seam);
}

void drawMouth(uint32_t tick) {
    matrix->clearScreen();
    if (currentAnimation == kAnimSleep) {
        present();
        return;
    }

    if (currentAnimation == kAnimIdle) {
        drawBenderTeeth(false, tick);
        present();
        return;
    }

    if (currentAnimation == kAnimSpeaking) {
        drawBenderTeeth(true, tick);
        present();
        return;
    }

    uint16_t color = rgb(255, 110, 24);
    if (currentAnimation == kAnimHappy) color = rgb(40, 255, 70);
    if (currentAnimation == kAnimAngry || currentAnimation == kAnimError) {
        color = rgb(255, 0, 0);
    }
    if (currentAnimation == kAnimListening) color = rgb(0, 120, 255);
    if (currentAnimation == kAnimThinking ||
        currentAnimation == kAnimThinkingAudio ||
        currentAnimation == kAnimThinkingLong) {
        color = rgb(150, 0, 255);
    }

    if (currentAnimation == kAnimError) {
        for (int i = 0; i < 9; ++i) {
            const int y = (tick + i * 7) % kPanelHeight;
            matrix->fillRect(i * 8, y, 5, 3, color);
        }
    } else if (currentAnimation == kAnimHappy) {
        for (int x = 9; x < 55; ++x) {
            const int dx = x - 32;
            const int y = 13 + (dx * dx) / 85;
            matrix->fillRect(x, y, 2, 2, color);
        }
    } else {
        int y = 16;
        if (currentAnimation == kAnimThinking ||
            currentAnimation == kAnimThinkingAudio ||
            currentAnimation == kAnimThinkingLong) {
            y += ((tick / 8) % 7) - 3;
        }
        matrix->fillRect(9, y - 2, 46, 4, color);
    }
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
        drawMouth((now + syncPhaseMs) / 16);
    }
}

void showNow() {
    if (matrix != nullptr) drawMouth((millis() + syncPhaseMs) / 16);
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
