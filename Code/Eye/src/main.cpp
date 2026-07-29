#include <cstdio>
#include <cstring>

#include "hardware/i2c.h"
#include "hardware/irq.h"
#include "hardware/regs/i2c.h"
#include "hardware/structs/i2c.h"
#include "hardware/watchdog.h"
#include "pico/stdlib.h"

#include "gc9a01.hpp"
#include "animation_playback.hpp"
#include "generated/eye_assets.hpp"
#include "protocol.hpp"
#include "swd_rtt.hpp"

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
constexpr uint32_t FRAME_INTERVAL_MS = 40;

static_assert(eye_assets::kWidth == WIDTH, "asset width mismatch");
static_assert(eye_assets::kHeight == HEIGHT, "asset height mismatch");
static_assert(eye_assets::kAnimationCount == kAnimCount,
              "asset animation count mismatch");
static_assert(eye_playback::kLoop == eye_assets::kPlaybackLoop,
              "loop playback IDs must match");
static_assert(eye_playback::kPingPong == eye_assets::kPlaybackPingPong,
              "ping-pong playback IDs must match");

static volatile uint8_t rx_buf[80];
static volatile uint8_t rx_len;
static volatile uint8_t tx_buf[32];
static volatile uint8_t tx_len = 8;
static volatile uint8_t tx_pos;
static volatile bool command_ready;
static uint8_t current_animation = kAnimIdle;
static uint8_t pending_animation = kAnimCount;
static uint8_t brightness = 180;
static uint8_t last_sequence;
static uint8_t last_error;
static eye_playback::State playback_state;
static uint32_t frame_started_ms;
static uint32_t animation_started_ms;

static void request_animation(uint8_t animation);

static const char *const kAnimationNames[] = {
    "idle", "listening", "thinking", "thinking_audio",
    "thinking_long", "speaking", "happy_fake", "angry",
    "error", "asleep", "tool", "left",
    "right", "up", "down", "center",
    "neutral", "sarcastic", "suspicious", "tired",
    "surprised", "bored", "dramatic", "watch",
    "party", "battery_low", "sunny", "rainy",
    "cloudy", "stormy", "snowy",
};
static_assert(sizeof(kAnimationNames) / sizeof(kAnimationNames[0]) == kAnimCount,
              "animation name count mismatch");

static void report_animation(const char *label) {
    printf("%s animation %u/%u: %s (id=0x%02x)\n",
           label,
           static_cast<unsigned>(current_animation + 1),
           static_cast<unsigned>(kAnimCount),
           kAnimationNames[current_animation],
           static_cast<unsigned>(current_animation));
    swd_rtt_printf("%s animation %u/%u: %s (id=0x%02x)\n",
                   label,
                   static_cast<unsigned>(current_animation + 1),
                   static_cast<unsigned>(kAnimCount),
                   kAnimationNames[current_animation],
                   static_cast<unsigned>(current_animation));
}

#ifdef ANIMATION_AUTOPLAY
static void update_animation_autoplay(uint32_t now_ms) {
    constexpr uint32_t kStateDurationMs = 3000;
    if (pending_animation == kAnimCount &&
        now_ms - animation_started_ms >= kStateDurationMs) {
        request_animation((current_animation + 1) % kAnimCount);
    }
}
#endif

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

static void display_init() {
    const uint8_t madctl = (DEVICE_ROLE == kRoleRightEye) ? 0x48 : 0x88;
    eye_display::init(brightness, madctl);
}

static eye_playback::Spec playback_spec(
    const eye_assets::Animation &animation
) {
    eye_playback::Spec spec{
        animation.frameCount, animation.playback, 0, 0, 0,
    };
#ifdef EYE_ASSETS_HAS_LOOP_RANGES
    spec.loop_start = animation.loopStart;
    spec.loop_end = animation.loopEnd;
    spec.loop_playback = animation.loopPlayback;
#endif
    return spec;
}

static void activate_animation(uint8_t animation, uint32_t now_ms) {
    current_animation = animation;
    pending_animation = kAnimCount;
    eye_playback::start(playback_state);
    frame_started_ms = now_ms;
    animation_started_ms = now_ms;
    prepare_status_response();
    report_animation("Current");
}

static void request_animation(uint8_t animation) {
    if (!isValidAnimation(animation)) return;
    if (animation == current_animation && pending_animation == kAnimCount) {
        return;
    }
    pending_animation = animation;
    const eye_assets::Animation &current =
        eye_assets::kAnimations[current_animation];
    eye_playback::request_exit(playback_spec(current), playback_state);
}

static void update_animation_playback(uint32_t now_ms) {
    while (true) {
        const eye_assets::Animation &animation =
            eye_assets::kAnimations[current_animation];
        if (now_ms - frame_started_ms < animation.frameMs) return;
        frame_started_ms += animation.frameMs;
        if (eye_playback::advance(
                playback_spec(animation), playback_state)) {
            if (!isValidAnimation(pending_animation)) return;
            activate_animation(pending_animation, frame_started_ms);
        }
    }
}

static void draw_asset_frame(uint16_t sprite_id) {
    const eye_assets::Frame &frame = eye_assets::kFrames[sprite_id];
    eye_display::set_window(0, 0, WIDTH - 1, HEIGHT - 1);
    uint8_t line[WIDTH * 2];
    uint16_t x = 0;
    uint16_t rows = 0;
    for (uint32_t offset = frame.offset;
         offset < frame.offset + frame.length; offset += 2) {
        uint16_t run = eye_assets::kRleData[offset];
        const uint8_t palette_index = eye_assets::kRleData[offset + 1];
        const uint8_t *color = eye_assets::kPalette[palette_index];
        const uint16_t packed =
            eye_display::rgb565(color[0], color[1], color[2]);
        while (run > 0) {
            const uint16_t chunk =
                run < WIDTH - x ? run : WIDTH - x;
            for (uint16_t index = 0; index < chunk; ++index) {
                line[(x + index) * 2] = packed >> 8;
                line[(x + index) * 2 + 1] = packed & 0xff;
            }
            x += chunk;
            run -= chunk;
            if (x == WIDTH) {
                eye_display::write_data(line, sizeof(line));
                x = 0;
                ++rows;
                watchdog_update();
            }
        }
    }
    while (rows < HEIGHT) {
        memset(line, 0, sizeof(line));
        eye_display::write_data(line, sizeof(line));
        ++rows;
    }
}

static void draw_eye() {
    const uint8_t animation_id =
        isValidAnimation(current_animation) ? current_animation : kAnimNeutral;
    const eye_assets::Animation &animation =
        eye_assets::kAnimations[animation_id];
    const uint8_t local_frame = playback_state.frame;
    const eye_assets::FramePair &pair = eye_assets::kFramePairs[
        animation.firstFramePair + local_frame];
    const uint16_t sprite_id =
        DEVICE_ROLE == kRoleRightEye ? pair.right : pair.left;
    draw_asset_frame(sprite_id);
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
        if (local[3] >= 1 && isValidAnimation(payload[0])) {
            request_animation(payload[0]);
        } else {
            last_error = 1;
        }
        break;
    case kCmdSync:
        // Animation commands reset their own local timeline. Both eye boards
        // receive the same command sequence, so an absolute remote uptime must
        // not be added to the new animation's frame index.
        break;
    case kCmdStop:
        request_animation(kAnimSleep);
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
    swd_rtt_printf("SarcasmOS eye role=%d addr=0x%02x\n",
                   DEVICE_ROLE, I2C_ADDRESS);
    uint32_t now = to_ms_since_boot(get_absolute_time());
    activate_animation(kAnimIdle, now);

    uint32_t last_draw = 0;
    while (true) {
        watchdog_update();
        now = to_ms_since_boot(get_absolute_time());
        if (command_ready) parse_command();
#ifdef ANIMATION_AUTOPLAY
        update_animation_autoplay(now);
#endif
        update_animation_playback(now);
        gpio_put(LED_PIN, (now / 500) & 1);
        if (now - last_draw >= FRAME_INTERVAL_MS) {
            last_draw = now;
            draw_eye();
        }
        tight_loop_contents();
    }
}
