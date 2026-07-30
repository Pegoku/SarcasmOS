#include <cstdio>
#include <cstring>

#include "hardware/i2c.h"
#include "hardware/irq.h"
#include "hardware/regs/i2c.h"
#include "hardware/structs/i2c.h"
#include "hardware/sync.h"
#include "hardware/watchdog.h"
#include "pico/stdlib.h"

#include "gc9a01.hpp"
#include "animation_playback.hpp"
#include "animation_transition.hpp"
#include "eye_status.hpp"
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
constexpr uint32_t READY_TIMEOUT_MS = 5000;
constexpr uint32_t DRAW_DEADLINE_MARGIN_MS = 2;
constexpr uint32_t ACTIVATION_PULSE_MS = 10;

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
static volatile uint8_t tx_len = kEyeStatusSize;
static volatile uint8_t tx_pos;
static volatile bool command_ready;
static volatile bool status_refresh_pending;
static volatile uint32_t command_stop_ms;
static eye_transition::State transition_state;
static uint8_t brightness = 180;
static uint8_t last_sequence;
static uint8_t last_error;
static eye_playback::State playback_state;
static uint32_t frame_started_ms;
static uint32_t animation_started_ms;
static uint32_t worst_draw_ms = FRAME_INTERVAL_MS;
static bool frame_render_pending;
static uint32_t activation_pulse_until_ms;

static void request_animation(
    uint8_t animation, uint8_t token, uint32_t now_ms);

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
           static_cast<unsigned>(transition_state.active_animation + 1),
           static_cast<unsigned>(kAnimCount),
           kAnimationNames[transition_state.active_animation],
           static_cast<unsigned>(transition_state.active_animation));
    swd_rtt_printf("%s animation %u/%u: %s (id=0x%02x)\n",
                   label,
                   static_cast<unsigned>(transition_state.active_animation + 1),
                   static_cast<unsigned>(kAnimCount),
                   kAnimationNames[transition_state.active_animation],
                   static_cast<unsigned>(transition_state.active_animation));
}

#ifdef ANIMATION_AUTOPLAY
static void update_animation_autoplay(uint32_t now_ms) {
    constexpr uint32_t kStateDurationMs = 3000;
    if (!eye_transition::has_pending(transition_state) &&
        now_ms - animation_started_ms >= kStateDurationMs) {
        request_animation(
            (transition_state.active_animation + 1) % kAnimCount, 0, now_ms);
    }
}
#endif

static void prepare_status_response() {
    uint8_t status[kEyeStatusSize];
    eye_status::encode(
        status, static_cast<uint8_t>(DEVICE_ROLE), 1, 1,
        transition_state, last_sequence, last_error, brightness,
        playback_state.exiting, playback_state.frame);

    // Publish one complete snapshot. If a read is already in progress, leave
    // its bytes untouched and publish the newer state after that transaction.
    const uint32_t interrupt_state = save_and_disable_interrupts();
    if (tx_pos != 0) {
        status_refresh_pending = true;
        restore_interrupts(interrupt_state);
        return;
    }
    for (uint8_t index = 0; index < kEyeStatusSize; ++index) {
        tx_buf[index] = status[index];
    }
    tx_len = kEyeStatusSize;
    status_refresh_pending = false;
    restore_interrupts(interrupt_state);
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
        if (rx_len > 0) {
            command_stop_ms = to_ms_since_boot(get_absolute_time());
            command_ready = true;
        }
        tx_pos = 0;
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
    if (eye_transition::has_pending(transition_state)) {
        eye_transition::activate_pending(transition_state);
    } else {
        transition_state.active_animation = animation;
    }
    eye_playback::start(playback_state);
    frame_started_ms = now_ms;
    animation_started_ms = now_ms;
    frame_render_pending = true;
    prepare_status_response();
    report_animation("Current");
}

static void request_animation(
    uint8_t animation, uint8_t token, uint32_t now_ms
) {
    if (!isValidAnimation(animation)) return;
    const eye_transition::RequestResult result =
        eye_transition::request(transition_state, animation, token, now_ms);
    if (result == eye_transition::RequestResult::Queued) {
        const eye_assets::Animation &current =
            eye_assets::kAnimations[transition_state.active_animation];
        eye_playback::request_exit(playback_spec(current), playback_state);
    }
    prepare_status_response();
}

static void update_animation_playback(uint32_t now_ms) {
    while (true) {
        if (transition_state.ready) {
            if (eye_transition::commit_due(transition_state, now_ms)) {
                const uint32_t deadline = transition_state.commit_deadline_ms;
                activate_animation(
                    transition_state.pending_animation, deadline);
                activation_pulse_until_ms = now_ms + ACTIVATION_PULSE_MS;
                continue;
            }
            if (last_error != kErrorReadyTimeout &&
                eye_transition::ready_timed_out(
                    transition_state, now_ms, READY_TIMEOUT_MS)) {
                last_error = kErrorReadyTimeout;
                prepare_status_response();
            }
            return;
        }
        const eye_assets::Animation &animation =
            eye_assets::kAnimations[transition_state.active_animation];
        if (now_ms - frame_started_ms < animation.frameMs) return;
        frame_started_ms += animation.frameMs;
        const bool was_exiting = playback_state.exiting;
        if (eye_playback::advance(
                playback_spec(animation), playback_state)) {
            if (!eye_transition::has_pending(transition_state)) return;
            eye_transition::mark_ready(transition_state, now_ms);
#ifdef ANIMATION_AUTOPLAY
            eye_transition::arm_commit(
                transition_state, transition_state.pending_token,
                eye_transition::kMinimumCommitDelayMs, now_ms);
#endif
            prepare_status_response();
            return;
        } else if (was_exiting) {
            prepare_status_response();
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
        isValidAnimation(transition_state.active_animation)
            ? transition_state.active_animation : kAnimNeutral;
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
    uint32_t stop_ms;
    irq_set_enabled(I2C0_IRQ, false);
    len = rx_len;
    stop_ms = command_stop_ms;
    memcpy(local, (const void *)rx_buf, len);
    rx_len = 0;
    command_ready = false;
    irq_set_enabled(I2C0_IRQ, true);

    if (len < 5 || local[0] != kProtocolVersion || local[3] > 64 || len != local[3] + 5 || crc8(local, len - 1) != local[len - 1]) {
        last_error = kErrorMalformedCommand;
        prepare_status_response();
        return;
    }
    uint8_t command = local[1];
    last_sequence = local[2];
    const uint8_t *payload = &local[4];
    last_error = kErrorNone;
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
            const uint8_t token = local[3] >= 2 ? payload[1] : 0;
            request_animation(payload[0], token, stop_ms);
        } else {
            last_error = kErrorMalformedCommand;
        }
        break;
    case kCmdSync: {
        if (local[3] != 4 || payload[3] != 0) {
            last_error = kErrorMalformedCommand;
            break;
        }
        const uint16_t delay_ms = static_cast<uint16_t>(payload[1]) |
                                  (static_cast<uint16_t>(payload[2]) << 8);
        const eye_transition::CommitResult result =
            eye_transition::arm_commit(
                transition_state, payload[0], delay_ms, stop_ms);
        if (result == eye_transition::CommitResult::WrongToken) {
            last_error = kErrorSyncToken;
        } else if (result == eye_transition::CommitResult::InvalidDelay) {
            last_error = kErrorSyncDelay;
        } else if (result == eye_transition::CommitResult::NoPending ||
                   result == eye_transition::CommitResult::NotReady) {
            last_error = kErrorSyncState;
        }
        break;
    }
    case kCmdStop:
        request_animation(kAnimSleep, 0, stop_ms);
        break;
    case kCmdReset:
        watchdog_reboot(0, 0, 10);
        break;
    default:
        last_error = kErrorUnknownCommand;
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
        if (status_refresh_pending && tx_pos == 0) {
            prepare_status_response();
        }
#ifdef ANIMATION_AUTOPLAY
        update_animation_autoplay(now);
#endif
        update_animation_playback(now);
        const bool activation_pulse =
            activation_pulse_until_ms != 0 &&
            !eye_transition::time_reached(now, activation_pulse_until_ms);
        const bool heartbeat = (now / 500) & 1;
        gpio_put(LED_PIN, activation_pulse ? !heartbeat : heartbeat);
        const bool draw_due = frame_render_pending ||
                              now - last_draw >= FRAME_INTERVAL_MS;
        bool deadline_safe = true;
        if (transition_state.commit_armed) {
            const int32_t until_deadline = static_cast<int32_t>(
                transition_state.commit_deadline_ms - now);
            deadline_safe = until_deadline <= 0 ||
                            static_cast<uint32_t>(until_deadline) >
                                worst_draw_ms + DRAW_DEADLINE_MARGIN_MS;
        }
        if (draw_due && deadline_safe) {
            last_draw = now;
            frame_render_pending = false;
            const uint32_t draw_started_ms =
                to_ms_since_boot(get_absolute_time());
            draw_eye();
            const uint32_t draw_duration_ms =
                to_ms_since_boot(get_absolute_time()) - draw_started_ms;
            if (draw_duration_ms > worst_draw_ms) {
                worst_draw_ms = draw_duration_ms;
            }
        }
        tight_loop_contents();
    }
}
