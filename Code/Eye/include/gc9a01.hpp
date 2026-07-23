#pragma once

#include <cstddef>
#include <cstdint>

namespace eye_display {

constexpr unsigned int CS_PIN = 17;
constexpr unsigned int SCK_PIN = 18;
constexpr unsigned int MOSI_PIN = 19;
constexpr unsigned int DC_PIN = 20;
constexpr unsigned int RST_PIN = 21;
constexpr unsigned int BL_PIN = 22;
constexpr int WIDTH = 240;
constexpr int HEIGHT = 240;

void init(uint8_t brightness, uint8_t madctl);
void set_brightness(uint8_t brightness);
void set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
void write_data(const uint8_t *data, size_t length);
uint16_t rgb565(uint8_t red, uint8_t green, uint8_t blue);

}  // namespace eye_display
