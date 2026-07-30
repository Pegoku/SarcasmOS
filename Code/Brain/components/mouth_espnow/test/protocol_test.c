#include <assert.h>
#include <string.h>

#include "display_protocol.h"
#include "eye_protocol.h"

static void round_trip(uint8_t command, const uint8_t *payload, uint8_t length)
{
    uint8_t encoded[DISPLAY_PROTOCOL_MAX_PACKET_SIZE];
    size_t encoded_length = display_protocol_encode(
        encoded, sizeof(encoded), DISPLAY_PROTOCOL_TYPE_COMMAND,
        DISPLAY_PROTOCOL_ROLE_MOUTH, 17, command, payload, length);
    assert(encoded_length ==
           (size_t)DISPLAY_PROTOCOL_HEADER_SIZE + length + 1);

    display_protocol_packet_t decoded;
    assert(display_protocol_decode(
        encoded, encoded_length, DISPLAY_PROTOCOL_TYPE_COMMAND,
        DISPLAY_PROTOCOL_ROLE_MOUTH, &decoded));
    assert(decoded.sequence == 17);
    assert(decoded.command == command);
    assert(decoded.payload_length == length);
    assert(length == 0 || memcmp(decoded.payload, payload, length) == 0);
}

int main(void)
{
    static const uint8_t commands[] = {
        DISPLAY_CMD_PING, DISPLAY_CMD_GET_INFO, DISPLAY_CMD_SET_BRIGHTNESS,
        DISPLAY_CMD_SET_ANIMATION, DISPLAY_CMD_SET_EXPRESSION, DISPLAY_CMD_SYNC,
        DISPLAY_CMD_STOP, DISPLAY_CMD_SET_PARAM, DISPLAY_CMD_DEBUG_FRAME,
        DISPLAY_CMD_RESET,
    };
    uint8_t payload[DISPLAY_PROTOCOL_MAX_PAYLOAD_SIZE];
    for (size_t i = 0; i < sizeof(payload); ++i) {
        payload[i] = (uint8_t)i;
    }
    for (size_t i = 0; i < sizeof(commands); ++i) {
        round_trip(commands[i], payload, (uint8_t)i);
    }
    round_trip(DISPLAY_CMD_SET_PARAM, payload, sizeof(payload));

    assert(display_protocol_crc8((const uint8_t *)"123456789", 9) == 0xF4);
    uint8_t packet[DISPLAY_PROTOCOL_MAX_PACKET_SIZE];
    size_t length = display_protocol_encode(
        packet, sizeof(packet), DISPLAY_PROTOCOL_TYPE_STATUS,
        DISPLAY_PROTOCOL_ROLE_MOUTH, 1, DISPLAY_CMD_PING, payload, 1);
    assert(length > 0);

    display_protocol_packet_t decoded;
#define REJECT_CORRUPTION(index, value) do { \
        uint8_t saved = packet[index]; \
        packet[index] = (value); \
        assert(!display_protocol_decode(packet, length, \
            DISPLAY_PROTOCOL_TYPE_STATUS, DISPLAY_PROTOCOL_ROLE_MOUTH, &decoded)); \
        packet[index] = saved; \
    } while (0)
    REJECT_CORRUPTION(0, 'X');
    REJECT_CORRUPTION(2, 2);
    REJECT_CORRUPTION(3, DISPLAY_PROTOCOL_TYPE_COMMAND);
    REJECT_CORRUPTION(4, DISPLAY_PROTOCOL_ROLE_ANY);
    REJECT_CORRUPTION(7, 2);
    REJECT_CORRUPTION(length - 1, packet[length - 1] ^ 0xFF);
#undef REJECT_CORRUPTION

    assert(!display_protocol_decode(
        packet, length - 1, DISPLAY_PROTOCOL_TYPE_STATUS,
        DISPLAY_PROTOCOL_ROLE_MOUTH, &decoded));
    assert(display_protocol_encode(
        packet, sizeof(packet), DISPLAY_PROTOCOL_TYPE_COMMAND,
        DISPLAY_PROTOCOL_ROLE_MOUTH, 1, DISPLAY_CMD_PING, payload, 65) == 0);

    uint8_t eye_status[EYE_PROTOCOL_STATUS_SIZE] = {
        EYE_PROTOCOL_VERSION_TRANSITIONS,
        0,
        EYE_PROTOCOL_SUPPORTED_FIRMWARE_MAJOR,
        0,
        DISPLAY_ANIM_SPEAKING,
        19,
        0,
        160,
        EYE_PROTOCOL_NO_PENDING_ANIMATION,
        42,
        0,
        EYE_PLAYBACK_TARGET_ACTIVATED,
        3,
    };
    eye_protocol_status_t eye;
    assert(eye_protocol_decode_status(
        eye_status, sizeof(eye_status), 0, &eye));
    assert(eye_protocol_transition_complete(
        &eye, DISPLAY_ANIM_SPEAKING, 42));

    eye.pending_animation = DISPLAY_ANIM_HAPPY;
    eye.pending_transition_token = 43;
    eye.playback_flags = EYE_PLAYBACK_PENDING | EYE_PLAYBACK_EXITING |
                         EYE_PLAYBACK_READY;
    assert(eye_protocol_transition_ready(
        &eye, DISPLAY_ANIM_HAPPY, 43));
    assert(!eye_protocol_transition_complete(
        &eye, DISPLAY_ANIM_HAPPY, 43));
    eye.playback_flags &= (uint8_t)~EYE_PLAYBACK_READY;
    assert(!eye_protocol_transition_ready(
        &eye, DISPLAY_ANIM_HAPPY, 43));

    uint8_t sync[EYE_SYNC_PAYLOAD_SIZE];
    eye_protocol_encode_sync(sync, 43, EYE_SYNC_START_DELAY_MS);
    assert(sync[0] == 43);
    assert(sync[1] == 50);
    assert(sync[2] == 0);
    assert(sync[3] == 0);

    eye.pending_animation = EYE_PROTOCOL_NO_PENDING_ANIMATION;
    eye.pending_transition_token = 0;
    eye.playback_flags = EYE_PLAYBACK_TARGET_ACTIVATED;

    eye.active_transition_token = 41;
    assert(!eye_protocol_transition_complete(
        &eye, DISPLAY_ANIM_SPEAKING, 42));
    eye.active_transition_token = 42;
    eye.pending_animation = DISPLAY_ANIM_IDLE;
    eye.playback_flags = EYE_PLAYBACK_PENDING | EYE_PLAYBACK_EXITING;
    assert(!eye_protocol_transition_complete(
        &eye, DISPLAY_ANIM_SPEAKING, 42));
    eye.pending_animation = EYE_PROTOCOL_NO_PENDING_ANIMATION;
    eye.playback_flags = 0;
    eye.last_error = 1;
    assert(!eye_protocol_transition_complete(
        &eye, DISPLAY_ANIM_SPEAKING, 42));

    eye_status[0] = EYE_PROTOCOL_VERSION_LEGACY;
    assert(!eye_protocol_decode_status(
        eye_status, sizeof(eye_status), 0, &eye));
    eye_status[0] = EYE_PROTOCOL_VERSION_TRANSITIONS;
    eye_status[1] = 1;
    assert(!eye_protocol_decode_status(
        eye_status, sizeof(eye_status), 0, &eye));
    assert(!eye_protocol_decode_status(
        eye_status, EYE_PROTOCOL_STATUS_LEGACY_SIZE, 1, &eye));
    return 0;
}
