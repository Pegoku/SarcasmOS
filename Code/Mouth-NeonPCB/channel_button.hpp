#pragma once

#include <cstdint>

namespace mouth_channel {

constexpr uint8_t kMinimumChannel = 1;
constexpr uint8_t kMaximumChannel = 13;
constexpr uint32_t kDebounceMs = 30;
constexpr uint32_t kLongPressMs = 1200;

enum class ButtonAction : uint8_t {
    None,
    NextChannel,
    ResetChannel,
};

struct ButtonState {
    bool rawPressed = false;
    bool stablePressed = false;
    bool longPressHandled = false;
    uint32_t rawChangedMs = 0;
    uint32_t pressedMs = 0;
};

inline constexpr bool isValidChannel(uint8_t channel) {
    return channel >= kMinimumChannel && channel <= kMaximumChannel;
}

inline constexpr uint8_t nextChannel(uint8_t channel) {
    return !isValidChannel(channel) || channel >= kMaximumChannel
               ? kMinimumChannel
               : static_cast<uint8_t>(channel + 1);
}

inline ButtonAction updateButton(
    ButtonState &state, bool pressed, uint32_t nowMs,
    uint32_t debounceMs = kDebounceMs,
    uint32_t longPressMs = kLongPressMs
) {
    if (pressed != state.rawPressed) {
        state.rawPressed = pressed;
        state.rawChangedMs = nowMs;
    }

    if (state.rawPressed != state.stablePressed &&
        nowMs - state.rawChangedMs >= debounceMs) {
        state.stablePressed = state.rawPressed;
        if (state.stablePressed) {
            state.pressedMs = nowMs;
            state.longPressHandled = false;
        } else if (!state.longPressHandled) {
            return ButtonAction::NextChannel;
        }
    }

    if (state.stablePressed && !state.longPressHandled &&
        nowMs - state.pressedMs >= longPressMs) {
        state.longPressHandled = true;
        return ButtonAction::ResetChannel;
    }
    return ButtonAction::None;
}

}  // namespace mouth_channel
