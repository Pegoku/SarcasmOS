#pragma once

#include <TFT_eSPI.h>

#include "../procedural_animations.hpp"

namespace bot_animations {

// Drop-in adapter for the library used by BotAnimator_ESP32.ino.
class TftEsPiCanvas final : public Canvas {
public:
    explicit TftEsPiCanvas(TFT_eSPI& display) : display_(display) {}

    void fillScreen(uint16_t color) override { display_.fillScreen(color); }
    void drawPixel(int16_t x, int16_t y, uint16_t color) override {
        display_.drawPixel(x, y, color);
    }
    void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                  uint16_t color) override {
        display_.drawLine(x0, y0, x1, y1, color);
    }
    void drawFastHLine(int16_t x, int16_t y, int16_t width,
                       uint16_t color) override {
        display_.drawFastHLine(x, y, width, color);
    }
    void fillRect(int16_t x, int16_t y, int16_t width, int16_t height,
                  uint16_t color) override {
        display_.fillRect(x, y, width, height, color);
    }
    void fillRoundRect(int16_t x, int16_t y, int16_t width, int16_t height,
                       int16_t radius, uint16_t color) override {
        display_.fillRoundRect(x, y, width, height, radius, color);
    }
    void drawCircle(int16_t x, int16_t y, int16_t radius,
                    uint16_t color) override {
        display_.drawCircle(x, y, radius, color);
    }
    void fillCircle(int16_t x, int16_t y, int16_t radius,
                    uint16_t color) override {
        display_.fillCircle(x, y, radius, color);
    }
    void fillTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                      int16_t x2, int16_t y2, uint16_t color) override {
        display_.fillTriangle(x0, y0, x1, y1, x2, y2, color);
    }
    void drawArc(int16_t x, int16_t y, int16_t outerRadius,
                 int16_t innerRadius, int16_t startAngle, int16_t endAngle,
                 uint16_t foreground, uint16_t background) override {
        display_.drawArc(x, y, outerRadius, innerRadius, startAngle, endAngle,
                         foreground, background);
    }
    void drawCenteredText(const char* text, int16_t x, int16_t y,
                          uint8_t font, uint16_t foreground,
                          uint16_t background) override {
        display_.setTextDatum(MC_DATUM);
        display_.setTextColor(foreground, background);
        display_.drawString(text, x, y, font);
    }

private:
    TFT_eSPI& display_;
};

}  // namespace bot_animations
