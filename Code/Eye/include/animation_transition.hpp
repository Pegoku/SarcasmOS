#pragma once

#include <cstdint>

namespace eye_transition {

constexpr uint8_t kNoAnimation = 0xff;
constexpr uint8_t kFlagPending = 1u << 0;
constexpr uint8_t kFlagExiting = 1u << 1;
constexpr uint8_t kFlagActivated = 1u << 2;
constexpr uint8_t kFlagReady = 1u << 3;
constexpr uint16_t kMinimumCommitDelayMs = 10;
constexpr uint16_t kMaximumCommitDelayMs = 250;

struct State {
    uint8_t active_animation = 0;
    uint8_t pending_animation = kNoAnimation;
    uint8_t active_token = 0;
    uint8_t pending_token = 0;
    bool ready = false;
    bool commit_armed = false;
    uint32_t ready_since_ms = 0;
    uint32_t commit_deadline_ms = 0;
};

enum class RequestResult : uint8_t {
    Duplicate,
    Queued,
    Ready,
};

enum class CommitResult : uint8_t {
    Armed,
    Duplicate,
    NoPending,
    NotReady,
    WrongToken,
    InvalidDelay,
};

inline bool has_pending(const State &state) {
    return state.pending_animation != kNoAnimation;
}

inline RequestResult request(
    State &state, uint8_t animation, uint8_t token, uint32_t now_ms
) {
    if (state.pending_animation == animation &&
        state.pending_token == token) {
        return RequestResult::Duplicate;
    }
    if (!has_pending(state) && state.active_animation == animation &&
        state.active_token == token) {
        return RequestResult::Duplicate;
    }
    const bool immediately_ready = state.active_animation == animation;
    state.pending_animation = animation;
    state.pending_token = token;
    state.ready = immediately_ready;
    state.commit_armed = false;
    state.ready_since_ms = immediately_ready ? now_ms : 0;
    state.commit_deadline_ms = 0;
    return immediately_ready ? RequestResult::Ready : RequestResult::Queued;
}

inline void mark_ready(State &state, uint32_t now_ms) {
    if (!has_pending(state)) return;
    state.ready = true;
    state.commit_armed = false;
    state.ready_since_ms = now_ms;
    state.commit_deadline_ms = 0;
}

inline CommitResult arm_commit(
    State &state, uint8_t token, uint16_t delay_ms, uint32_t received_ms
) {
    if (!has_pending(state)) {
        return state.active_token == token
                   ? CommitResult::Duplicate : CommitResult::NoPending;
    }
    if (state.pending_token != token) return CommitResult::WrongToken;
    if (!state.ready) return CommitResult::NotReady;
    if (state.commit_armed) return CommitResult::Duplicate;
    if (delay_ms < kMinimumCommitDelayMs ||
        delay_ms > kMaximumCommitDelayMs) {
        return CommitResult::InvalidDelay;
    }
    state.commit_armed = true;
    state.commit_deadline_ms = received_ms + delay_ms;
    return CommitResult::Armed;
}

inline bool time_reached(uint32_t now_ms, uint32_t deadline_ms) {
    return static_cast<int32_t>(now_ms - deadline_ms) >= 0;
}

inline bool commit_due(const State &state, uint32_t now_ms) {
    return state.ready && state.commit_armed &&
           time_reached(now_ms, state.commit_deadline_ms);
}

inline bool ready_timed_out(
    const State &state, uint32_t now_ms, uint32_t timeout_ms
) {
    return state.ready && !state.commit_armed &&
           time_reached(now_ms, state.ready_since_ms + timeout_ms);
}

inline void activate_pending(State &state) {
    if (!has_pending(state)) return;
    state.active_animation = state.pending_animation;
    state.active_token = state.pending_token;
    state.pending_animation = kNoAnimation;
    state.pending_token = 0;
    state.ready = false;
    state.commit_armed = false;
    state.ready_since_ms = 0;
    state.commit_deadline_ms = 0;
}

inline uint8_t playback_flags(const State &state, bool exiting) {
    uint8_t flags = 0;
    if (has_pending(state)) flags |= kFlagPending;
    if (exiting) flags |= kFlagExiting;
    if (!has_pending(state)) flags |= kFlagActivated;
    if (state.ready) flags |= kFlagReady;
    return flags;
}

}  // namespace eye_transition
