#include <cstdio>
#include <cstring>

#include "hardware/i2c.h"
#include "hardware/irq.h"
#include "hardware/regs/i2c.h"
#include "hardware/structs/i2c.h"
#include "hardware/watchdog.h"
#include "pico/stdlib.h"

#include "gc9a01.hpp"
#include "protocol.hpp"

#ifndef DEVICE_ROLE
#define DEVICE_ROLE kRoleLeftEye
#endif
#ifndef I2C_ADDRESS
#define I2C_ADDRESS 0x30
#endif

constexpr uint LED_PIN = 2;
constexpr uint I2C_SDA_PIN = 4;
constexpr uint I2C_SCL_PIN = 5;
constexpr int WIDTH = eye_display::WIDTH;
constexpr int HEIGHT = eye_display::HEIGHT;

static volatile uint8_t rx_buf[80];
static volatile uint8_t rx_len;
static volatile uint8_t tx_buf[32];
static volatile uint8_t tx_len = 8;
static volatile uint8_t tx_pos;
static volatile bool command_ready;
static uint8_t current_animation = kAnimIdle;
static uint8_t brightness = 180;
static uint8_t last_sequence;
static uint8_t last_error;
static uint32_t sync_phase_ms;

static void prepare_status_response() {
    tx_buf[0] = kProtocolVersion;
    tx_buf[1] = static_cast<uint8_t>(DEVICE_ROLE);
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
        uint8_t value = tx_pos < tx_len ? tx_buf[tx_pos++] : 0;
        hw->data_cmd = value;
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
    i2c_get_hw(i2c0)->intr_mask = I2C_IC_INTR_MASK_M_RX_FULL_BITS |
                                  I2C_IC_INTR_MASK_M_RD_REQ_BITS |
                                  I2C_IC_INTR_MASK_M_STOP_DET_BITS;
    irq_set_enabled(I2C0_IRQ, true);
}

static void display_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    eye_display::set_window(x0, y0, x1, y1);
}

static void display_init() {
    const uint8_t madctl = (DEVICE_ROLE == kRoleRightEye) ? 0x88 : 0x48;
    eye_display::init(brightness, madctl);
}

static void fill_screen(uint16_t color) {
    display_set_window(0, 0, WIDTH - 1, HEIGHT - 1);
    uint8_t line[WIDTH * 2];
    for (int x = 0; x < WIDTH; ++x) {
        line[x * 2] = color >> 8;
        line[x * 2 + 1] = color & 0xff;
    }
    for (int y = 0; y < HEIGHT; ++y) eye_display::write_data(line, sizeof(line));
}

static void draw_eye(uint32_t tick) {
    uint16_t bg = eye_display::rgb565(4, 6, 10);
    uint16_t iris = eye_display::rgb565(0, 190, 255);
    uint16_t pupil = eye_display::rgb565(0, 0, 0);
    if (current_animation == kAnimSleep) { fill_screen(0); return; }
    if (current_animation == kAnimError) { fill_screen(eye_display::rgb565(120, 0, 0)); return; }
    if (current_animation == kAnimAngry) iris = eye_display::rgb565(255, 48, 0);
    if (current_animation == kAnimHappy) iris = eye_display::rgb565(80, 255, 90);

    display_set_window(0, 0, WIDTH - 1, HEIGHT - 1);
    uint8_t line[WIDTH * 2];
    int cx = WIDTH / 2;
    int cy = HEIGHT / 2;
    if (current_animation == kAnimThinking) cx += ((tick / 20) % 41) - 20;
    if (current_animation == kAnimSpeaking) cy += ((tick / 9) % 17) - 8;
    int blink = (current_animation == kAnimIdle && (tick % 180) > 170) ? 10 : 78;
    for (int y = 0; y < HEIGHT; ++y) {
        for (int x = 0; x < WIDTH; ++x) {
            int dx = x - cx;
            int dy = y - cy;
            uint16_t color = bg;
            if ((dx * dx) / 85 + (dy * dy) / blink < 74) color = eye_display::rgb565(220, 245, 255);
            if (dx * dx + dy * dy < 34 * 34) color = iris;
            if (dx * dx + dy * dy < 15 * 15) color = pupil;
            line[x * 2] = color >> 8;
            line[x * 2 + 1] = color & 0xff;
        }
        eye_display::write_data(line, sizeof(line));
    }
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
        if (local[3] >= 1) { brightness = payload[0]; eye_display::set_brightness(brightness); }
        break;
    case kCmdSetAnimation:
    case kCmdSetExpression:
        if (local[3] >= 1) current_animation = payload[0];
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
    display_init();
    i2c_slave_init();
    printf("SarcasmOS eye role=%d addr=0x%02x\n", DEVICE_ROLE, I2C_ADDRESS);

    uint32_t last_draw = 0;
    while (true) {
        watchdog_update();
        if (command_ready) parse_command();
        uint32_t now = to_ms_since_boot(get_absolute_time());
        gpio_put(LED_PIN, (now / 500) & 1);
        if (now - last_draw > 80) {
            last_draw = now;
            draw_eye((now + sync_phase_ms) / 16);
        }
        tight_loop_contents();
    }
}
