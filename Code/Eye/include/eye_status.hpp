#pragma once

#include <cstdint>

#include "animation_transition.hpp"
#include "protocol.hpp"

namespace eye_status {

inline void encode(
    uint8_t (&output)[kEyeStatusSize], uint8_t role,
    uint8_t firmware_major, uint8_t firmware_minor,
    const eye_transition::State &transition, uint8_t sequence,
    uint8_t error, uint8_t brightness, bool exiting, uint8_t frame
) {
    output[0] = kProtocolVersion;
    output[1] = role;
    output[2] = firmware_major;
    output[3] = firmware_minor;
    output[4] = transition.active_animation;
    output[5] = sequence;
    output[6] = error;
    output[7] = brightness;
    output[8] = transition.pending_animation;
    output[9] = transition.active_token;
    output[10] = transition.pending_token;
    output[11] = eye_transition::playback_flags(transition, exiting);
    output[12] = frame;
}

}  // namespace eye_status
