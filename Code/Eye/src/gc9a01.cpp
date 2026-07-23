#include "gc9a01.hpp"

#include "hardware/pwm.h"
#include "hardware/spi.h"
#include "pico/stdlib.h"

namespace eye_display {
namespace {

void write_command(uint8_t command) {
    gpio_put(DC_PIN, 0);
    gpio_put(CS_PIN, 0);
    spi_write_blocking(spi0, &command, 1);
    gpio_put(CS_PIN, 1);
}

void write_command(uint8_t command, const uint8_t *data, size_t length) {
    write_command(command);
    if (length > 0) write_data(data, length);
}

void run_init_sequence() {
    // GC9A01A manufacturer sequence, as used by the Adafruit GC9A01A driver.
    static constexpr uint8_t commands[] = {
        0xEF, 0,
        0xEB, 1, 0x14,
        0xFE, 0,
        0xEF, 0,
        0xEB, 1, 0x14,
        0x84, 1, 0x40,
        0x85, 1, 0xFF,
        0x86, 1, 0xFF,
        0x87, 1, 0xFF,
        0x88, 1, 0x0A,
        0x89, 1, 0x21,
        0x8A, 1, 0x00,
        0x8B, 1, 0x80,
        0x8C, 1, 0x01,
        0x8D, 1, 0x01,
        0x8E, 1, 0xFF,
        0x8F, 1, 0xFF,
        0xB6, 2, 0x00, 0x00,
        0x90, 4, 0x08, 0x08, 0x08, 0x08,
        0xBD, 1, 0x06,
        0xBC, 1, 0x00,
        0xFF, 3, 0x60, 0x01, 0x04,
        0xC3, 1, 0x13,
        0xC4, 1, 0x13,
        0xC9, 1, 0x22,
        0xBE, 1, 0x11,
        0xE1, 2, 0x10, 0x0E,
        0xDF, 3, 0x21, 0x0C, 0x02,
        0xF0, 6, 0x45, 0x09, 0x08, 0x08, 0x26, 0x2A,
        0xF1, 6, 0x43, 0x70, 0x72, 0x36, 0x37, 0x6F,
        0xF2, 6, 0x45, 0x09, 0x08, 0x08, 0x26, 0x2A,
        0xF3, 6, 0x43, 0x70, 0x72, 0x36, 0x37, 0x6F,
        0xED, 2, 0x1B, 0x0B,
        0xAE, 1, 0x77,
        0xCD, 1, 0x63,
        0xE8, 1, 0x34,
        0x62, 12, 0x18, 0x0D, 0x71, 0xED, 0x70, 0x70,
                  0x18, 0x0F, 0x71, 0xEF, 0x70, 0x70,
        0x63, 12, 0x18, 0x11, 0x71, 0xF1, 0x70, 0x70,
                  0x18, 0x13, 0x71, 0xF3, 0x70, 0x70,
        0x64, 7, 0x28, 0x29, 0xF1, 0x01, 0xF1, 0x00, 0x07,
        0x66, 10, 0x3C, 0x00, 0xCD, 0x67, 0x45,
                  0x45, 0x10, 0x00, 0x00, 0x00,
        0x67, 10, 0x00, 0x3C, 0x00, 0x00, 0x00,
                  0x01, 0x54, 0x10, 0x32, 0x98,
        0x74, 7, 0x10, 0x85, 0x80, 0x00, 0x00, 0x4E, 0x00,
        0x98, 2, 0x3E, 0x07,
        0x00,
    };

    const uint8_t *entry = commands;
    while (*entry != 0x00) {
        const uint8_t command = *entry++;
        const uint8_t length = *entry++;
        write_command(command, entry, length);
        entry += length;
    }
}

}  // namespace

void write_data(const uint8_t *data, size_t length) {
    gpio_put(DC_PIN, 1);
    gpio_put(CS_PIN, 0);
    spi_write_blocking(spi0, data, length);
    gpio_put(CS_PIN, 1);
}

void set_brightness(uint8_t brightness) {
    pwm_set_gpio_level(BL_PIN, brightness);
}

void init(uint8_t brightness, uint8_t madctl) {
    spi_init(spi0, 40000000);
    gpio_set_function(SCK_PIN, GPIO_FUNC_SPI);
    gpio_set_function(MOSI_PIN, GPIO_FUNC_SPI);

    gpio_init(CS_PIN);
    gpio_set_dir(CS_PIN, GPIO_OUT);
    gpio_put(CS_PIN, 1);
    gpio_init(DC_PIN);
    gpio_set_dir(DC_PIN, GPIO_OUT);
    gpio_init(RST_PIN);
    gpio_set_dir(RST_PIN, GPIO_OUT);

    gpio_set_function(BL_PIN, GPIO_FUNC_PWM);
    const uint slice = pwm_gpio_to_slice_num(BL_PIN);
    pwm_set_wrap(slice, 255);
    set_brightness(brightness);
    pwm_set_enabled(slice, true);

    gpio_put(RST_PIN, 0);
    sleep_ms(30);
    gpio_put(RST_PIN, 1);
    sleep_ms(120);
    write_command(0x01);
    sleep_ms(150);

    run_init_sequence();
    write_command(0x36, &madctl, 1);
    const uint8_t color_mode = 0x05;
    write_command(0x3A, &color_mode, 1);
    write_command(0x35);
    write_command(0x21);
    write_command(0x11);
    sleep_ms(150);
    write_command(0x29);
    sleep_ms(20);
}

void set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    uint8_t data[] = {
        static_cast<uint8_t>(x0 >> 8),
        static_cast<uint8_t>(x0),
        static_cast<uint8_t>(x1 >> 8),
        static_cast<uint8_t>(x1),
    };
    write_command(0x2A, data, sizeof(data));
    data[0] = y0 >> 8;
    data[1] = y0;
    data[2] = y1 >> 8;
    data[3] = y1;
    write_command(0x2B, data, sizeof(data));
    write_command(0x2C);
}

uint16_t rgb565(uint8_t red, uint8_t green, uint8_t blue) {
    return static_cast<uint16_t>(((red & 0xf8) << 8) |
                                 ((green & 0xfc) << 3) |
                                 (blue >> 3));
}

}  // namespace eye_display
