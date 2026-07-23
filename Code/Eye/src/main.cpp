#include <cstdio>
#include <cstring>

#include "hardware/i2c.h"
#include "hardware/irq.h"
#include "hardware/pwm.h"
#include "hardware/regs/i2c.h"
#include "hardware/spi.h"
#include "hardware/structs/i2c.h"
#include "hardware/watchdog.h"
#include "pico/stdlib.h"

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
constexpr uint DISP_CS_PIN = 17;
constexpr uint DISP_SCK_PIN = 18;
constexpr uint DISP_MOSI_PIN = 19;
constexpr uint DISP_DC_PIN = 20;
constexpr uint DISP_RST_PIN = 21;
constexpr uint DISP_BL_PIN = 22;
constexpr int WIDTH = 240;
constexpr int HEIGHT = 240;

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

static void spi_write_cmd(uint8_t cmd) {
    gpio_put(DISP_DC_PIN, 0);
    gpio_put(DISP_CS_PIN, 0);
    spi_write_blocking(spi0, &cmd, 1);
    gpio_put(DISP_CS_PIN, 1);
}

static void spi_write_data(const uint8_t *data, size_t len) {
    gpio_put(DISP_DC_PIN, 1);
    gpio_put(DISP_CS_PIN, 0);
    spi_write_blocking(spi0, data, len);
    gpio_put(DISP_CS_PIN, 1);
}

static void display_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    uint8_t data[4];
    spi_write_cmd(0x2A);
    data[0] = x0 >> 8; data[1] = x0 & 0xff; data[2] = x1 >> 8; data[3] = x1 & 0xff;
    spi_write_data(data, 4);
    spi_write_cmd(0x2B);
    data[0] = y0 >> 8; data[1] = y0 & 0xff; data[2] = y1 >> 8; data[3] = y1 & 0xff;
    spi_write_data(data, 4);
    spi_write_cmd(0x2C);
}

static void display_init() {
    spi_init(spi0, 40000000);
    gpio_set_function(DISP_SCK_PIN, GPIO_FUNC_SPI);
    gpio_set_function(DISP_MOSI_PIN, GPIO_FUNC_SPI);
    gpio_init(DISP_CS_PIN); gpio_set_dir(DISP_CS_PIN, GPIO_OUT); gpio_put(DISP_CS_PIN, 1);
    gpio_init(DISP_DC_PIN); gpio_set_dir(DISP_DC_PIN, GPIO_OUT);
    gpio_init(DISP_RST_PIN); gpio_set_dir(DISP_RST_PIN, GPIO_OUT);
    gpio_set_function(DISP_BL_PIN, GPIO_FUNC_PWM);
    uint slice = pwm_gpio_to_slice_num(DISP_BL_PIN);
    pwm_set_wrap(slice, 255);
    pwm_set_gpio_level(DISP_BL_PIN, brightness);
    pwm_set_enabled(slice, true);

    gpio_put(DISP_RST_PIN, 0); sleep_ms(30);
    gpio_put(DISP_RST_PIN, 1); sleep_ms(120);
    spi_write_cmd(0x01); sleep_ms(120);
    uint8_t colmod = 0x55;
    spi_write_cmd(0x3A); spi_write_data(&colmod, 1);
    uint8_t madctl = (DEVICE_ROLE == kRoleRightEye) ? 0x60 : 0x00;
    spi_write_cmd(0x36); spi_write_data(&madctl, 1);
    spi_write_cmd(0x11); sleep_ms(120);
    spi_write_cmd(0x29);
}

static uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
    return static_cast<uint16_t>(((r & 0xf8) << 8) | ((g & 0xfc) << 3) | (b >> 3));
}

static void fill_screen(uint16_t color) {
    display_set_window(0, 0, WIDTH - 1, HEIGHT - 1);
    uint8_t line[WIDTH * 2];
    for (int x = 0; x < WIDTH; ++x) {
        line[x * 2] = color >> 8;
        line[x * 2 + 1] = color & 0xff;
    }
    for (int y = 0; y < HEIGHT; ++y) spi_write_data(line, sizeof(line));
}

static void draw_eye(uint32_t tick) {
    uint16_t bg = rgb565(4, 6, 10);
    uint16_t iris = rgb565(0, 190, 255);
    uint16_t pupil = rgb565(0, 0, 0);
    if (current_animation == kAnimSleep) { fill_screen(0); return; }
    if (current_animation == kAnimError) { fill_screen(rgb565(120, 0, 0)); return; }
    if (current_animation == kAnimAngry) iris = rgb565(255, 48, 0);
    if (current_animation == kAnimHappy) iris = rgb565(80, 255, 90);

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
            if ((dx * dx) / 85 + (dy * dy) / blink < 74) color = rgb565(220, 245, 255);
            if (dx * dx + dy * dy < 34 * 34) color = iris;
            if (dx * dx + dy * dy < 15 * 15) color = pupil;
            line[x * 2] = color >> 8;
            line[x * 2 + 1] = color & 0xff;
        }
        spi_write_data(line, sizeof(line));
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
        if (local[3] >= 1) { brightness = payload[0]; pwm_set_gpio_level(DISP_BL_PIN, brightness); }
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
