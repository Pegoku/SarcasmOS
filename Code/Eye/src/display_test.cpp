#include <cstdio>

#include "hardware/watchdog.h"
#include "pico/stdlib.h"

#include "gc9a01.hpp"
#include "protocol.hpp"
#include "swd_rtt.hpp"

#ifndef DEVICE_ROLE
#define DEVICE_ROLE kRoleLeftEye
#endif

constexpr uint LED_PIN = 2;
constexpr int WIDTH = eye_display::WIDTH;
constexpr int HEIGHT = eye_display::HEIGHT;
constexpr uint32_t PATTERN_TIME_MS = 3000;

enum Pattern {
    kBlack,
    kWhite,
    kRed,
    kGreen,
    kBlue,
    kVerticalBars,
    kHorizontalBars,
    kCheckerboard,
    kAlignment,
    kGrayGradient,
    kRgbGradient,
    kPatternCount,
};

static const char *const kPatternNames[] = {
    "black",
    "white",
    "red",
    "green",
    "blue",
    "vertical RGB bars",
    "horizontal RGB bars",
    "checkerboard",
    "alignment border and corners",
    "horizontal grayscale gradient",
    "two-axis RGB gradient",
};

static uint16_t color_for_pattern(Pattern pattern, int x, int y) {
    constexpr uint8_t levels[] = {0, 51, 102, 153, 204, 255};

    switch (pattern) {
    case kBlack:
        return eye_display::rgb565(0, 0, 0);
    case kWhite:
        return eye_display::rgb565(255, 255, 255);
    case kRed:
        return eye_display::rgb565(255, 0, 0);
    case kGreen:
        return eye_display::rgb565(0, 255, 0);
    case kBlue:
        return eye_display::rgb565(0, 0, 255);
    case kVerticalBars: {
        constexpr uint16_t colors[] = {
            0xffff, 0xffe0, 0x07ff, 0x07e0,
            0xf81f, 0xf800, 0x001f, 0x0000,
        };
        return colors[x * 8 / WIDTH];
    }
    case kHorizontalBars: {
        constexpr uint16_t colors[] = {
            0xffff, 0xffe0, 0x07ff, 0x07e0,
            0xf81f, 0xf800, 0x001f, 0x0000,
        };
        return colors[y * 8 / HEIGHT];
    }
    case kCheckerboard:
        return ((x / 16) + (y / 16)) & 1 ? 0xffff : 0x0000;
    case kAlignment:
        if (x < 4 || x >= WIDTH - 4 || y < 4 || y >= HEIGHT - 4) return 0xffff;
        if (x < 40 && y < 40) return eye_display::rgb565(255, 0, 0);
        if (x >= WIDTH - 40 && y < 40) return eye_display::rgb565(0, 255, 0);
        if (x < 40 && y >= HEIGHT - 40) return eye_display::rgb565(0, 0, 255);
        if (x >= WIDTH - 40 && y >= HEIGHT - 40) return eye_display::rgb565(255, 255, 0);
        if (x >= WIDTH / 2 - 2 && x <= WIDTH / 2 + 2) return eye_display::rgb565(255, 0, 255);
        if (y >= HEIGHT / 2 - 2 && y <= HEIGHT / 2 + 2) return eye_display::rgb565(255, 0, 255);
        return 0x0000;
    case kGrayGradient: {
        const uint8_t level = static_cast<uint8_t>(x * 255 / (WIDTH - 1));
        return eye_display::rgb565(level, level, level);
    }
    case kRgbGradient:
        return eye_display::rgb565(static_cast<uint8_t>(x * 255 / (WIDTH - 1)),
                                   static_cast<uint8_t>(y * 255 / (HEIGHT - 1)),
                                   levels[((x / 40) + (y / 40)) % 6]);
    default:
        return 0x0000;
    }
}

static void draw_pattern(Pattern pattern) {
    eye_display::set_window(0, 0, WIDTH - 1, HEIGHT - 1);
    uint8_t line[WIDTH * 2];
    for (int y = 0; y < HEIGHT; ++y) {
        for (int x = 0; x < WIDTH; ++x) {
            const uint16_t color = color_for_pattern(pattern, x, y);
            line[x * 2] = color >> 8;
            line[x * 2 + 1] = color & 0xff;
        }
        eye_display::write_data(line, sizeof(line));
        watchdog_update();
    }
}

int main() {
    stdio_init_all();
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    watchdog_enable(2000, true);
    const uint8_t madctl = (DEVICE_ROLE == kRoleRightEye) ? 0x48 : 0x88;
    eye_display::init(255, madctl);

    int last_pattern = -1;
    while (true) {
        watchdog_update();
        const uint32_t now = to_ms_since_boot(get_absolute_time());
        const int pattern = (now / PATTERN_TIME_MS) % kPatternCount;
        gpio_put(LED_PIN, (now / 250) & 1);
        if (pattern != last_pattern) {
            last_pattern = pattern;
            printf("Display test %d/%d: %s\n",
                   pattern + 1, kPatternCount, kPatternNames[pattern]);
            swd_rtt_printf("Display test %d/%d: %s\n",
                           pattern + 1, kPatternCount,
                           kPatternNames[pattern]);
            draw_pattern(static_cast<Pattern>(pattern));
        }
        tight_loop_contents();
    }
}
