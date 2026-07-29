#pragma once

#include <cstdint>

namespace eye_playback {

constexpr uint8_t kLoop = 0;
constexpr uint8_t kPingPong = 1;

struct Spec {
    uint8_t frame_count;
    uint8_t playback;
    uint8_t loop_start;
    uint8_t loop_end;
    uint8_t loop_playback;
};

struct State {
    uint8_t frame = 0;
    int8_t direction = 1;
    bool exiting = false;
    bool loop_return_required = false;
};

inline bool has_loop_range(const Spec &spec) {
    return spec.loop_end > spec.loop_start &&
           spec.loop_end <= spec.frame_count;
}

inline void start(State &state) {
    state = {};
}

inline void request_exit(const Spec &spec, State &state) {
    if (state.exiting) return;
    state.exiting = true;
    const bool ranged = has_loop_range(spec);
    const bool return_pong = ranged &&
        spec.loop_end == spec.frame_count &&
        spec.playback == kPingPong;
    state.loop_return_required = ranged && (
        return_pong || (
            spec.loop_playback == kPingPong &&
            spec.loop_end - spec.loop_start > 1 &&
            state.frame >= spec.loop_start &&
            state.frame < spec.loop_end
        )
    );
}

// Advance after the current frame has been visible for one frame interval.
// Returns true when the caller should activate its queued animation.
inline bool advance(const Spec &spec, State &state) {
    if (spec.frame_count <= 1) return state.exiting;

    const uint8_t last = spec.frame_count - 1;
    const bool ranged = has_loop_range(spec);
    const uint8_t range_start = spec.loop_start;
    const uint8_t range_last = spec.loop_end - 1;
    const bool return_pong = ranged &&
        spec.loop_end == spec.frame_count &&
        spec.playback == kPingPong;

    if (state.exiting) {
        if (ranged && state.loop_return_required) {
            if (state.frame < range_start) {
                ++state.frame;
            } else if (spec.loop_playback != kPingPong) {
                if (state.frame < range_last) {
                    ++state.frame;
                } else {
                    state.loop_return_required = false;
                    state.direction = -1;
                    if (state.frame > 0) --state.frame;
                    else return true;
                }
            } else if (state.direction > 0) {
                if (state.frame < range_last) {
                    ++state.frame;
                } else {
                    state.direction = -1;
                    --state.frame;
                }
            } else if (state.frame > range_start) {
                --state.frame;
            } else {
                state.loop_return_required = false;
                if (return_pong) {
                    if (state.frame > 0) --state.frame;
                    else return true;
                } else {
                    state.direction = 1;
                    if (state.frame < last) ++state.frame;
                    else return true;
                }
            }
            return false;
        }

        if (return_pong) {
            if (state.frame > 0) {
                --state.frame;
                return false;
            }
            return true;
        }

        if (!ranged && spec.playback == kPingPong) {
            if (state.direction > 0) {
                if (state.frame < last) {
                    ++state.frame;
                } else {
                    state.direction = -1;
                    --state.frame;
                }
            } else if (state.frame > 0) {
                --state.frame;
            } else {
                return true;
            }
            return false;
        }

        if (state.frame < last) {
            ++state.frame;
            return false;
        }
        return true;
    }

    if (ranged) {
        if (state.frame < range_start) {
            ++state.frame;
        } else if (spec.loop_playback == kPingPong &&
                   range_last > range_start) {
            if (state.direction > 0) {
                if (state.frame < range_last) {
                    ++state.frame;
                } else {
                    state.direction = -1;
                    --state.frame;
                }
            } else if (state.frame > range_start) {
                --state.frame;
            } else {
                state.direction = 1;
                ++state.frame;
            }
        } else if (state.frame < range_last) {
            ++state.frame;
        } else {
            state.frame = range_start;
        }
        return false;
    }

    if (spec.playback == kPingPong) {
        if (state.direction > 0) {
            if (state.frame < last) {
                ++state.frame;
            } else {
                state.direction = -1;
                --state.frame;
            }
        } else if (state.frame > 0) {
            --state.frame;
        } else {
            state.direction = 1;
            ++state.frame;
        }
    } else if (state.frame < last) {
        ++state.frame;
    } else {
        state.frame = 0;
    }
    return false;
}

}  // namespace eye_playback
