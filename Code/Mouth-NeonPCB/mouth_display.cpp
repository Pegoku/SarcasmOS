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

uint16_t teethColor() {
    return rgb(255, 246, 166);
}

uint16_t dimTeethColor() {
    return rgb(116, 112, 76);
}

void drawShell(uint16_t fill, int x = 1, int y = 1, int width = 62,
               int height = 30, int radius = 14) {
    const uint16_t seam = rgb(0, 0, 0);
    matrix->fillRoundRect(x, y, width, height, radius, seam);
    matrix->fillRoundRect(x + 2, y + 2, width - 4, height - 4,
                          max(radius - 2, 1), fill);
}

void drawColumnSeams(int y = 3, int height = 26) {
    const uint16_t seam = rgb(0, 0, 0);
    // Five columns, matching the grille in the reference mouth.
    // The 58-pixel interior contains five 10-pixel teeth and four 2-pixel
    // seams. Starting at x=3, these positions keep every column equal and
    // center the grille on the 64-pixel panel.
    constexpr int kColumnSeams[] = {13, 25, 37, 49};
    for (const int x : kColumnSeams) {
        matrix->drawFastVLine(x, y, height, seam);
        matrix->drawFastVLine(x + 1, y, height, seam);
    }
}

void drawResting(uint16_t fill = 0) {
    const uint16_t seam = rgb(0, 0, 0);
    if (fill == 0) fill = teethColor();
    drawShell(fill);
    drawColumnSeams();
    matrix->drawFastHLine(3, 11, 58, seam);
    matrix->drawFastHLine(3, 12, 58, seam);
    matrix->drawFastHLine(3, 20, 58, seam);
    matrix->drawFastHLine(3, 21, 58, seam);
}

void drawSpeaking(uint32_t tick) {
    const uint16_t seam = rgb(0, 0, 0);
    drawShell(teethColor());
    drawColumnSeams();

    const int phase = tick % 24;
    const int triangle = phase <= 12 ? phase : 24 - phase;
    int amplitude = 2 + (triangle * 6) / 12;
    amplitude = constrain(
        (amplitude * static_cast<int>(currentMouthIntensity)) / 120, 1, 9);

    constexpr int kCenterY = 16;
    // Keep the waveform centered: it opens at 1/4 of the panel, reaches its
    // peak near the middle, and closes at roughly 3/4.
    drawThickLine(3, kCenterY - 1, 15, kCenterY - 1, seam);
    drawThickLine(3, kCenterY + 1, 15, kCenterY + 1, seam);

    drawThickLine(15, kCenterY - 1, 32, kCenterY - amplitude, seam);
    drawThickLine(32, kCenterY - amplitude, 49, kCenterY - 1, seam);
    drawThickLine(49, kCenterY - 1, 60, kCenterY - 1, seam);

    drawThickLine(15, kCenterY + 1, 32, kCenterY + amplitude, seam);
    drawThickLine(32, kCenterY + amplitude, 49, kCenterY + 1, seam);
    drawThickLine(49, kCenterY + 1, 60, kCenterY + 1, seam);
}

void drawThinking(uint32_t tick, uint8_t variant) {
    const uint16_t seam = rgb(0, 0, 0);
    drawShell(teethColor());
    drawColumnSeams();
    matrix->drawFastHLine(3, 10, 58, seam);
    matrix->drawFastHLine(3, 22, 58, seam);

    const int cadence = variant == 2 ? 28 : (variant == 1 ? 12 : 18);
    const int step = (tick / cadence) % 3;
    constexpr int kDots[] = {24, 32, 40};
    for (int i = 0; i < 3; ++i) {
        const int radius = i == step ? (variant == 1 ? 3 : 2) : 1;
        matrix->fillCircle(kDots[i], 16, radius, seam);
    }
}

void drawTool(uint32_t tick) {
    const uint16_t seam = rgb(0, 0, 0);
    drawShell(teethColor());
    drawColumnSeams();
    drawThickLine(5, 13, 59, 13, seam);
    drawThickLine(5, 18, 59, 18, seam);
    const int x = 8 + ((tick / 2) % 48);
    matrix->fillRect(x, 15, 3, 2, seam);
}

void drawListening(uint32_t tick) {
    drawResting();
    const uint16_t accent = rgb(110, 88, 25);
    const int x = 8 + ((tick / 2) % 48);
    matrix->fillCircle(x, 16, 1, accent);
}

void drawHappy(uint32_t tick, bool party) {
    const uint16_t seam = rgb(0, 0, 0);
    drawShell(teethColor());
    drawColumnSeams();
    drawThickLine(4, 11, 16, 14, seam);
    drawThickLine(16, 14, 32, 18, seam);
    drawThickLine(32, 18, 48, 14, seam);
    drawThickLine(48, 14, 60, 11, seam);
    if (party) {
        constexpr uint16_t kPeriod = 18;
        const int phase = (tick / 3) % kPeriod;
        matrix->fillRect(9 + phase, 6, 2, 2, rgb(255, 45, 90));
        matrix->fillRect(53 - phase, 23, 2, 2, rgb(30, 170, 255));
    }
}

void drawAngry() {
    const uint16_t seam = rgb(0, 0, 0);
    drawShell(teethColor());
    drawColumnSeams();
    drawThickLine(3, 8, 32, 14, seam);
    drawThickLine(32, 14, 60, 8, seam);
    drawThickLine(3, 23, 32, 18, seam);
    drawThickLine(32, 18, 60, 23, seam);
}

void drawSarcastic() {
    const uint16_t seam = rgb(0, 0, 0);
    drawShell(teethColor());
    drawColumnSeams();
    drawThickLine(3, 19, 19, 18, seam);
    drawThickLine(19, 18, 35, 15, seam);
    drawThickLine(35, 15, 60, 11, seam);
}

void drawSuspicious() {
    const uint16_t seam = rgb(0, 0, 0);
    drawShell(teethColor());
    drawColumnSeams();
    drawThickLine(3, 14, 25, 14, seam);
    drawThickLine(25, 14, 32, 18, seam);
    drawThickLine(32, 18, 39, 14, seam);
    drawThickLine(39, 14, 60, 14, seam);
}

void drawTired(uint32_t tick) {
    const uint16_t seam = rgb(0, 0, 0);
    drawShell(dimTeethColor(), 3, 7, 58, 20, 9);
    const int droop = 2 + ((tick / 40) % 2);
    drawThickLine(8, 15, 24, 15 + droop, seam);
    drawThickLine(24, 15 + droop, 40, 15 + droop, seam);
    drawThickLine(40, 15 + droop, 56, 15, seam);
}

void drawSurprised(uint32_t tick, bool dramatic) {
    const uint16_t fill = teethColor();
    const uint16_t seam = rgb(0, 0, 0);
    const int pulse = dramatic ? 2 + ((tick / 12) % 3) : 0;
    const int width = (dramatic ? 24 : 18) + pulse;
    const int height = (dramatic ? 30 : 22) + pulse / 2;
    const int x = (kPanelWidth - width) / 2;
    const int y = (kPanelHeight - height) / 2;
    drawShell(fill, x, y, width, height, min(width, height) / 2);
    matrix->fillRoundRect(x + 6, y + 6, width - 12, height - 12,
                          max((width - 12) / 2, 1), seam);
}

void drawBored() {
    const uint16_t seam = rgb(0, 0, 0);
    drawShell(dimTeethColor(), 4, 10, 56, 13, 6);
    matrix->drawFastHLine(6, 16, 52, seam);
    matrix->drawFastHLine(6, 17, 52, seam);
    constexpr int kShortSeams[] = {16, 27, 38, 49};
    for (const int x : kShortSeams) {
        matrix->drawFastVLine(x, 12, 9, seam);
    }
}

void drawWatch(uint32_t tick) {
    drawResting();
    const uint16_t accent = rgb(245, 178, 25);
    const int x = 5 + ((tick / 2) % 54);
    matrix->drawFastVLine(x, 5, 5, accent);
    matrix->drawFastVLine(x, 23, 4, accent);
}

void drawError(uint32_t tick) {
    const uint16_t seam = rgb(0, 0, 0);
    const uint16_t fill = ((tick / 64) % 2) ? rgb(255, 90, 60)
                                            : rgb(170, 25, 20);
    drawShell(fill);
    drawColumnSeams();
    constexpr int kX[] = {3, 10, 17, 24, 31, 38, 45, 52, 60};
    for (int i = 0; i < 8; ++i) {
        drawThickLine(kX[i], (i % 2) ? 22 : 9, kX[i + 1],
                      (i % 2) ? 9 : 22, seam);
    }
}

void drawBatteryLow(uint32_t tick) {
    const uint16_t seam = rgb(0, 0, 0);
    const bool visible = (tick % 128) < 112;
    if (!visible) return;
    drawShell(dimTeethColor(), 5, 8, 54, 18, 8);
    drawThickLine(10, 15, 24, 15, seam);
    drawThickLine(24, 15, 32, 19, seam);
    drawThickLine(32, 19, 40, 15, seam);
    drawThickLine(40, 15, 54, 15, seam);
    matrix->fillRect(57, 13, 3, 7, dimTeethColor());
}

void drawRainy(uint32_t tick) {
    const uint16_t seam = rgb(0, 0, 0);
    drawShell(rgb(185, 198, 172));
    drawColumnSeams();
    const int offset = (tick / 8) % 8;
    for (int x = -5 + offset; x < 64; x += 8) {
        drawThickLine(x, 14, x + 4, 18, seam);
        drawThickLine(x + 4, 18, x + 8, 14, seam);
    }
}

void drawCloudy(uint32_t tick) {
    const uint16_t seam = rgb(0, 0, 0);
    drawShell(rgb(205, 205, 160), 5, 6, 54, 22, 10);
    const int drift = (tick / 30) % 3;
    drawThickLine(10, 15 + drift, 22, 13 + drift, seam);
    drawThickLine(22, 13 + drift, 34, 16 + drift, seam);
    drawThickLine(34, 16 + drift, 46, 13 + drift, seam);
    drawThickLine(46, 13 + drift, 54, 15 + drift, seam);
}

void drawStormy(uint32_t tick) {
    const uint16_t seam = rgb(0, 0, 0);
    drawShell(rgb(235, 215, 110));
    drawColumnSeams();
    const int shift = (tick / 10) % 6;
    for (int x = -8 + shift; x < 64; x += 12) {
        drawThickLine(x, 9, x + 6, 16, seam);
        drawThickLine(x + 6, 16, x, 23, seam);
    }
}

void drawSnowy(uint32_t tick) {
    const uint16_t seam = rgb(0, 0, 0);
    drawShell(rgb(230, 232, 200));
    drawColumnSeams();
    constexpr int kDots[][2] = {
        {10, 9}, {20, 18}, {31, 12}, {42, 21}, {53, 8}, {57, 18},
    };
    const int phase = (tick / 18) % 6;
    for (int i = 0; i < 6; ++i) {
        const int y = 5 + ((kDots[i][1] + phase + i) % 21);
        matrix->fillCircle(kDots[i][0], y, i == phase ? 2 : 1, seam);
    }
}

void drawMouth(uint32_t tick) {
    matrix->clearScreen();
    switch (currentAnimation) {
    case kAnimListening:
        drawListening(tick);
        break;
    case kAnimThinking:
        drawThinking(tick, 0);
        break;
    case kAnimThinkingAudio:
        drawThinking(tick, 1);
        break;
    case kAnimThinkingLong:
        drawThinking(tick, 2);
        break;
    case kAnimSpeaking:
        drawSpeaking(tick);
        break;
    case kAnimHappy:
    case kAnimSunny:
        drawHappy(tick, false);
        break;
    case kAnimAngry:
        drawAngry();
        break;
    case kAnimError:
        drawError(tick);
        break;
    case kAnimSleep:
        break;
    case kAnimTool:
        drawTool(tick);
        break;
    case kAnimLeft:
    case kAnimRight:
    case kAnimUp:
    case kAnimDown:
    case kAnimCenter:
    case kAnimNeutral:
    case kAnimIdle:
        drawResting();
        break;
    case kAnimSarcastic:
        drawSarcastic();
        break;
    case kAnimSuspicious:
        drawSuspicious();
        break;
    case kAnimTired:
        drawTired(tick);
        break;
    case kAnimSurprised:
        drawSurprised(tick, false);
        break;
    case kAnimBored:
        drawBored();
        break;
    case kAnimDramatic:
        drawSurprised(tick, true);
        break;
    case kAnimWatch:
        drawWatch(tick);
        break;
    case kAnimParty:
        drawHappy(tick, true);
        break;
    case kAnimBatteryLow:
        drawBatteryLow(tick);
        break;
    case kAnimRainy:
        drawRainy(tick);
        break;
    case kAnimCloudy:
        drawCloudy(tick);
        break;
    case kAnimStormy:
        drawStormy(tick);
        break;
    case kAnimSnowy:
        drawSnowy(tick);
        break;
    default:
        drawResting();
        break;
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
