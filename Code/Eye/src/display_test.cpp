#include <cstdio>

#include "hardware/pwm.h"
#include "hardware/spi.h"
#include "hardware/watchdog.h"
#include "pico/stdlib.h"

constexpr uint LED_PIN = 2;
constexpr uint DISP_CS_PIN = 17;
constexpr uint DISP_SCK_PIN = 18;
constexpr uint DISP_MOSI_PIN = 19;
constexpr uint DISP_DC_PIN = 20;
constexpr uint DISP_RST_PIN = 21;
constexpr uint DISP_BL_PIN = 22;
constexpr int WIDTH = 240;
constexpr int HEIGHT = 240;
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

static void spi_write_cmd(uint8_t command) {
    gpio_put(DISP_DC_PIN, 0);
    gpio_put(DISP_CS_PIN, 0);
    spi_write_blocking(spi0, &command, 1);
    gpio_put(DISP_CS_PIN, 1);
}

static void spi_write_data(const uint8_t *data, size_t length) {
    gpio_put(DISP_DC_PIN, 1);
    gpio_put(DISP_CS_PIN, 0);
    spi_write_blocking(spi0, data, length);
    gpio_put(DISP_CS_PIN, 1);
}

static void display_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    uint8_t data[4];
    spi_write_cmd(0x2A);
    data[0] = x0 >> 8;
    data[1] = x0 & 0xff;
    data[2] = x1 >> 8;
    data[3] = x1 & 0xff;
    spi_write_data(data, sizeof(data));

    spi_write_cmd(0x2B);
    data[0] = y0 >> 8;
    data[1] = y0 & 0xff;
    data[2] = y1 >> 8;
    data[3] = y1 & 0xff;
    spi_write_data(data, sizeof(data));
    spi_write_cmd(0x2C);
}

static void display_init() {
    spi_init(spi0, 40000000);
    gpio_set_function(DISP_SCK_PIN, GPIO_FUNC_SPI);
    gpio_set_function(DISP_MOSI_PIN, GPIO_FUNC_SPI);

    gpio_init(DISP_CS_PIN);
    gpio_set_dir(DISP_CS_PIN, GPIO_OUT);
    gpio_put(DISP_CS_PIN, 1);
    gpio_init(DISP_DC_PIN);
    gpio_set_dir(DISP_DC_PIN, GPIO_OUT);
    gpio_init(DISP_RST_PIN);
    gpio_set_dir(DISP_RST_PIN, GPIO_OUT);

    gpio_set_function(DISP_BL_PIN, GPIO_FUNC_PWM);
    const uint slice = pwm_gpio_to_slice_num(DISP_BL_PIN);
    pwm_set_wrap(slice, 255);
    pwm_set_gpio_level(DISP_BL_PIN, 255);
    pwm_set_enabled(slice, true);

    gpio_put(DISP_RST_PIN, 0);
    sleep_ms(30);
    gpio_put(DISP_RST_PIN, 1);
    sleep_ms(120);
    spi_write_cmd(0x01);
    sleep_ms(120);

    const uint8_t colmod = 0x55;
    spi_write_cmd(0x3A);
    spi_write_data(&colmod, 1);
    const uint8_t madctl = 0x00;
    spi_write_cmd(0x36);
    spi_write_data(&madctl, 1);
    spi_write_cmd(0x11);
    sleep_ms(120);
    spi_write_cmd(0x29);
}

static uint16_t rgb565(uint8_t red, uint8_t green, uint8_t blue) {
    return static_cast<uint16_t>(((red & 0xf8) << 8) |
                                 ((green & 0xfc) << 3) |
                                 (blue >> 3));
}

static uint16_t color_for_pattern(Pattern pattern, int x, int y) {
    constexpr uint8_t levels[] = {0, 51, 102, 153, 204, 255};

    switch (pattern) {
    case kBlack:
        return rgb565(0, 0, 0);
    case kWhite:
        return rgb565(255, 255, 255);
    case kRed:
        return rgb565(255, 0, 0);
    case kGreen:
        return rgb565(0, 255, 0);
    case kBlue:
        return rgb565(0, 0, 255);
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
        if (x < 40 && y < 40) return rgb565(255, 0, 0);
        if (x >= WIDTH - 40 && y < 40) return rgb565(0, 255, 0);
        if (x < 40 && y >= HEIGHT - 40) return rgb565(0, 0, 255);
        if (x >= WIDTH - 40 && y >= HEIGHT - 40) return rgb565(255, 255, 0);
        if (x >= WIDTH / 2 - 2 && x <= WIDTH / 2 + 2) return rgb565(255, 0, 255);
        if (y >= HEIGHT / 2 - 2 && y <= HEIGHT / 2 + 2) return rgb565(255, 0, 255);
        return 0x0000;
    case kGrayGradient: {
        const uint8_t level = static_cast<uint8_t>(x * 255 / (WIDTH - 1));
        return rgb565(level, level, level);
    }
    case kRgbGradient:
        return rgb565(static_cast<uint8_t>(x * 255 / (WIDTH - 1)),
                      static_cast<uint8_t>(y * 255 / (HEIGHT - 1)),
                      levels[((x / 40) + (y / 40)) % 6]);
    default:
        return 0x0000;
    }
}

static void draw_pattern(Pattern pattern) {
    display_set_window(0, 0, WIDTH - 1, HEIGHT - 1);
    uint8_t line[WIDTH * 2];
    for (int y = 0; y < HEIGHT; ++y) {
        for (int x = 0; x < WIDTH; ++x) {
            const uint16_t color = color_for_pattern(pattern, x, y);
            line[x * 2] = color >> 8;
            line[x * 2 + 1] = color & 0xff;
        }
        spi_write_data(line, sizeof(line));
        watchdog_update();
    }
}

int main() {
    stdio_init_all();
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    watchdog_enable(2000, true);
    display_init();

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
            draw_pattern(static_cast<Pattern>(pattern));
        }
        tight_loop_contents();
    }
}
