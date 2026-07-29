#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define EYE_PROTOCOL_VERSION_LEGACY 0x01
#define EYE_PROTOCOL_VERSION_TRANSITIONS 0x02
#define EYE_PROTOCOL_STATUS_LEGACY_SIZE 8
#define EYE_PROTOCOL_STATUS_SIZE 13
#define EYE_PROTOCOL_NO_PENDING_ANIMATION 0xFF
#define EYE_PROTOCOL_SUPPORTED_FIRMWARE_MAJOR 1

#define EYE_PLAYBACK_PENDING (1U << 0)
#define EYE_PLAYBACK_EXITING (1U << 1)
#define EYE_PLAYBACK_TARGET_ACTIVATED (1U << 2)

typedef struct {
    uint8_t protocol_version;
    uint8_t role;
    uint8_t firmware_major;
    uint8_t firmware_minor;
    uint8_t active_animation;
    uint8_t last_sequence;
    uint8_t last_error;
    uint8_t brightness;
    uint8_t pending_animation;
    uint8_t active_transition_token;
    uint8_t pending_transition_token;
    uint8_t playback_flags;
    uint8_t current_frame;
} eye_protocol_status_t;

static inline bool eye_protocol_decode_status(
    const uint8_t *data, size_t length, uint8_t expected_role,
    eye_protocol_status_t *status)
{
    if (data == NULL || status == NULL || length != EYE_PROTOCOL_STATUS_SIZE ||
        data[0] != EYE_PROTOCOL_VERSION_TRANSITIONS ||
        data[1] != expected_role ||
        data[2] != EYE_PROTOCOL_SUPPORTED_FIRMWARE_MAJOR) {
        return false;
    }

    status->protocol_version = data[0];
    status->role = data[1];
    status->firmware_major = data[2];
    status->firmware_minor = data[3];
    status->active_animation = data[4];
    status->last_sequence = data[5];
    status->last_error = data[6];
    status->brightness = data[7];
    status->pending_animation = data[8];
    status->active_transition_token = data[9];
    status->pending_transition_token = data[10];
    status->playback_flags = data[11];
    status->current_frame = data[12];
    return true;
}

static inline bool eye_protocol_transition_complete(
    const eye_protocol_status_t *status, uint8_t animation, uint8_t token)
{
    return status != NULL &&
           status->active_animation == animation &&
           status->active_transition_token == token &&
           status->pending_animation == EYE_PROTOCOL_NO_PENDING_ANIMATION &&
           (status->playback_flags & EYE_PLAYBACK_PENDING) == 0 &&
           status->last_error == 0;
}

#ifdef __cplusplus
}
#endif
