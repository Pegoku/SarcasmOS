#pragma once

#include <cstdint>

namespace eye_transition {

constexpr uint8_t kNoAnimation = 0xff;
constexpr uint8_t kFlagPending = 1u << 0;
constexpr uint8_t kFlagExiting = 1u << 1;
constexpr uint8_t kFlagActivated = 1u << 2;

struct State {
    uint8_t active_animation = 0;
    uint8_t pending_animation = kNoAnimation;
    uint8_t active_token = 0;
    uint8_t pending_token = 0;
};

enum class RequestResult : uint8_t {
    Duplicate,
    Activated,
    Queued,
};

inline bool has_pending(const State &state) {
    return state.pending_animation != kNoAnimation;
}

inline RequestResult request(
    State &state, uint8_t animation, uint8_t token
) {
    if (state.pending_animation == animation &&
        state.pending_token == token) {
        return RequestResult::Duplicate;
    }
    if (!has_pending(state) && state.active_animation == animation) {
        state.active_token = token;
        return RequestResult::Activated;
    }
    state.pending_animation = animation;
    state.pending_token = token;
    return RequestResult::Queued;
}

inline void activate_pending(State &state) {
    if (!has_pending(state)) return;
    state.active_animation = state.pending_animation;
    state.active_token = state.pending_token;
    state.pending_animation = kNoAnimation;
    state.pending_token = 0;
}

inline uint8_t playback_flags(const State &state, bool exiting) {
    uint8_t flags = 0;
    if (has_pending(state)) flags |= kFlagPending;
    if (exiting) flags |= kFlagExiting;
    if (!has_pending(state)) flags |= kFlagActivated;
    return flags;
}

}  // namespace eye_transition
