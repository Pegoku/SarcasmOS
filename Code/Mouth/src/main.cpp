#include <cstdio>
#include <cstring>

#include "hardware/i2c.h"
#include "hardware/irq.h"
#include "hardware/regs/i2c.h"
#include "hardware/structs/i2c.h"
#include "hardware/watchdog.h"
#include "pico/stdlib.h"

#include "protocol.hpp"

#ifndef I2C_ADDRESS
#define I2C_ADDRESS 0x32
#endif

constexpr uint LED_PIN = 2;
constexpr uint I2C_SDA_PIN = 4;
constexpr uint I2C_SCL_PIN = 5;
constexpr uint PIN_R1 = 6;
constexpr uint PIN_G1 = 7;
constexpr uint PIN_B1 = 8;
constexpr uint PIN_R2 = 9;
constexpr uint PIN_G2 = 10;
constexpr uint PIN_B2 = 11;
constexpr uint PIN_E = 12;
constexpr uint PIN_A = 13;
constexpr uint PIN_B = 14;
constexpr uint PIN_C = 15;
constexpr uint PIN_D = 16;
constexpr uint PIN_CLK = 17;
constexpr uint PIN_LAT = 18;
constexpr uint PIN_OE = 19;
constexpr int WIDTH = 128;
constexpr int HEIGHT = 64;
constexpr int SCAN_ROWS = 32;

static uint16_t framebuffer[HEIGHT][WIDTH];
static volatile uint8_t rx_buf[80];
static volatile uint8_t rx_len;
static volatile uint8_t tx_buf[32];
static volatile uint8_t tx_len = 8;
static volatile uint8_t tx_pos;
static volatile bool command_ready;
static uint8_t current_animation = kAnimIdle;
static uint8_t brightness = 120;
static uint8_t last_sequence;
static uint8_t last_error;
static uint8_t mouth_intensity = 120;
static uint32_t sync_phase_ms;

static void prepare_status_response() {
    tx_buf[0] = kProtocolVersion;
    tx_buf[1] = kRoleMouth;
    tx_buf[2] = 1;
    tx_buf[3] = 0;
    tx_buf[4] = current_animation;
    tx_buf[5] = last_sequence;
    tx_buf[6] = last_error;
    tx_buf[7] = brightness;
    tx_len = 8;
    tx_pos = 0;
}

static void i2c_irq_handler() {
    auto *hw = i2c_get_hw(i2c0);
    uint32_t status = hw->intr_stat;
    if (status & I2C_IC_INTR_STAT_R_RX_FULL_BITS) {
        while (hw->status & I2C_IC_STATUS_RFNE_BITS) {
            uint8_t value = static_cast<uint8_t>(hw->data_cmd & 0xff);
            if (rx_len < sizeof(rx_buf)) rx_buf[rx_len++] = value;
        }
    }
    if (status & I2C_IC_INTR_STAT_R_RD_REQ_BITS) {
        hw->clr_rd_req;
        hw->data_cmd = tx_pos < tx_len ? tx_buf[tx_pos++] : 0;
    }
    if (status & I2C_IC_INTR_STAT_R_STOP_DET_BITS) {
        hw->clr_stop_det;
        if (rx_len > 0) command_ready = true;
    }
}

static void i2c_slave_init() {
    i2c_init(i2c0, 400000);
    gpio_set_function(I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA_PIN);
    gpio_pull_up(I2C_SCL_PIN);
    i2c_set_slave_mode(i2c0, true, I2C_ADDRESS);
    prepare_status_response();
    irq_set_exclusive_handler(I2C0_IRQ, i2c_irq_handler);
    irq_set_enabled(I2C0_IRQ, true);
    i2c_get_hw(i2c0)->intr_mask = I2C_IC_INTR_MASK_M_RX_FULL_BITS |
                                  I2C_IC_INTR_MASK_M_RD_REQ_BITS |
                                  I2C_IC_INTR_MASK_M_STOP_DET_BITS;
}

static void matrix_gpio_init() {
    const uint pins[] = {PIN_R1, PIN_G1, PIN_B1, PIN_R2, PIN_G2, PIN_B2, PIN_E, PIN_A, PIN_B, PIN_C, PIN_D, PIN_CLK, PIN_LAT, PIN_OE};
    for (uint pin : pins) {
        gpio_init(pin);
        gpio_set_dir(pin, GPIO_OUT);
        gpio_put(pin, 0);
    }
    gpio_put(PIN_OE, 1);
}

static uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
    return static_cast<uint16_t>(((r & 0xf8) << 8) | ((g & 0xfc) << 3) | (b >> 3));
}

static void clear(uint16_t color = 0) {
    for (int y = 0; y < HEIGHT; ++y) {
        for (int x = 0; x < WIDTH; ++x) framebuffer[y][x] = color;
    }
}

static void rect(int x0, int y0, int x1, int y1, uint16_t color) {
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > WIDTH) x1 = WIDTH;
    if (y1 > HEIGHT) y1 = HEIGHT;
    for (int y = y0; y < y1; ++y) {
        for (int x = x0; x < x1; ++x) framebuffer[y][x] = color;
    }
}

static void draw_mouth(uint32_t tick) {
    clear();
    if (current_animation == kAnimSleep) return;
    uint16_t color = rgb565(255, 110, 24);
    if (current_animation == kAnimHappy) color = rgb565(40, 255, 70);
    if (current_animation == kAnimAngry || current_animation == kAnimError) color = rgb565(255, 0, 0);
    if (current_animation == kAnimListening) color = rgb565(0, 120, 255);
    if (current_animation == kAnimThinking || current_animation == kAnimThinkingAudio || current_animation == kAnimThinkingLong) color = rgb565(150, 0, 255);

    if (current_animation == kAnimSpeaking) {
        int open = 8 + ((tick / 3) % 20);
        if ((tick / 23) & 1) open = 30 - open;
        open = (open * mouth_intensity) / 160;
        rect(16, 32 - open / 2, 112, 32 + open / 2, color);
        rect(20, 30 - open / 2, 108, 34 + open / 2, color);
        return;
    }

    if (current_animation == kAnimError) {
        for (int i = 0; i < 9; ++i) rect(i * 15, (tick + i * 7) % 64, i * 15 + 9, ((tick + i * 7) % 64) + 5, color);
        return;
    }

    if (current_animation == kAnimHappy) {
        for (int x = 18; x < 110; ++x) {
            int dx = x - 64;
            int y = 27 + (dx * dx) / 170;
            rect(x, y, x + 2, y + 4, color);
        }
        return;
    }

    int y = 32;
    if (current_animation == kAnimThinking) y += ((tick / 8) % 9) - 4;
    rect(18, y - 3, 110, y + 3, color);
}

static inline bool bit_on(uint16_t color, int bit, int channel_shift) {
    uint8_t value;
    if (channel_shift == 11) value = static_cast<uint8_t>(((color >> 11) & 0x1f) << 3);
    else if (channel_shift == 5) value = static_cast<uint8_t>(((color >> 5) & 0x3f) << 2);
    else value = static_cast<uint8_t>((color & 0x1f) << 3);
    return value & (1 << bit);
}

static void scan_matrix_once() {
    for (int bit = 3; bit >= 0; --bit) {
        for (int row = 0; row < SCAN_ROWS; ++row) {
            gpio_put(PIN_OE, 1);
            gpio_put(PIN_A, row & 1);
            gpio_put(PIN_B, (row >> 1) & 1);
            gpio_put(PIN_C, (row >> 2) & 1);
            gpio_put(PIN_D, (row >> 3) & 1);
            gpio_put(PIN_E, (row >> 4) & 1);

            for (int x = 0; x < WIDTH; ++x) {
                uint16_t top = framebuffer[row][x];
                uint16_t bottom = framebuffer[row + SCAN_ROWS][x];
                gpio_put(PIN_R1, bit_on(top, bit + 4, 11));
                gpio_put(PIN_G1, bit_on(top, bit + 4, 5));
                gpio_put(PIN_B1, bit_on(top, bit + 4, 0));
                gpio_put(PIN_R2, bit_on(bottom, bit + 4, 11));
                gpio_put(PIN_G2, bit_on(bottom, bit + 4, 5));
                gpio_put(PIN_B2, bit_on(bottom, bit + 4, 0));
                gpio_put(PIN_CLK, 1);
                gpio_put(PIN_CLK, 0);
            }

            gpio_put(PIN_LAT, 1);
            gpio_put(PIN_LAT, 0);
            gpio_put(PIN_OE, 0);
            sleep_us((1u << bit) * brightness / 16 + 20);
        }
    }
    gpio_put(PIN_OE, 1);
}

static void parse_command() {
    uint8_t local[80];
    uint8_t len;
    irq_set_enabled(I2C0_IRQ, false);
    len = rx_len;
    memcpy(local, (const void *)rx_buf, len);
    rx_len = 0;
    command_ready = false;
    irq_set_enabled(I2C0_IRQ, true);

    if (len < 5 || local[0] != kProtocolVersion || local[3] > 64 || len != local[3] + 5 || crc8(local, len - 1) != local[len - 1]) {
        last_error = 1;
        prepare_status_response();
        return;
    }
    uint8_t command = local[1];
    last_sequence = local[2];
    const uint8_t *payload = &local[4];
    last_error = 0;
    switch (command) {
    case kCmdPing:
    case kCmdGetInfo:
        break;
    case kCmdSetBrightness:
        if (local[3] >= 1) brightness = payload[0];
        break;
    case kCmdSetAnimation:
    case kCmdSetExpression:
        if (local[3] >= 1) current_animation = payload[0];
        break;
    case kCmdSetParam:
        if (local[3] >= 2 && payload[0] == 1) mouth_intensity = payload[1];
        break;
    case kCmdSync:
        if (local[3] >= 4) sync_phase_ms = payload[0] | (payload[1] << 8) | (payload[2] << 16) | (payload[3] << 24);
        break;
    case kCmdStop:
        current_animation = kAnimSleep;
        break;
    case kCmdReset:
        watchdog_reboot(0, 0, 10);
        break;
    default:
        last_error = 2;
        break;
    }
    prepare_status_response();
}

int main() {
    stdio_init_all();
    gpio_init(LED_PIN); gpio_set_dir(LED_PIN, GPIO_OUT);
    watchdog_enable(2000, true);
    matrix_gpio_init();
    clear();
    i2c_slave_init();
    printf("SarcasmOS mouth addr=0x%02x\n", I2C_ADDRESS);

    uint32_t last_draw = 0;
    while (true) {
        watchdog_update();
        if (command_ready) parse_command();
        uint32_t now = to_ms_since_boot(get_absolute_time());
        gpio_put(LED_PIN, (now / 500) & 1);
        if (now - last_draw > 40) {
            last_draw = now;
            draw_mouth((now + sync_phase_ms) / 16);
        }
        scan_matrix_once();
    }
}
