#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DISPLAY_PROTOCOL_MAGIC_0 ((uint8_t)'S')
#define DISPLAY_PROTOCOL_MAGIC_1 ((uint8_t)'M')
#define DISPLAY_PROTOCOL_TRANSPORT_VERSION 0x01
#define DISPLAY_PROTOCOL_APPLICATION_VERSION_LEGACY 0x02
#define DISPLAY_PROTOCOL_APPLICATION_VERSION 0x03
#define DISPLAY_PROTOCOL_ROLE_MOUTH 0x02
#define DISPLAY_PROTOCOL_ROLE_ANY 0xFF
#define DISPLAY_PROTOCOL_TYPE_COMMAND 0x01
#define DISPLAY_PROTOCOL_TYPE_STATUS 0x02
#define DISPLAY_PROTOCOL_HEADER_SIZE 8
#define DISPLAY_PROTOCOL_MAX_PAYLOAD_SIZE 64
#define DISPLAY_PROTOCOL_MAX_PACKET_SIZE \
    (DISPLAY_PROTOCOL_HEADER_SIZE + DISPLAY_PROTOCOL_MAX_PAYLOAD_SIZE + 1)
#define DISPLAY_PROTOCOL_STATUS_PAYLOAD_LEGACY_SIZE 10
#define DISPLAY_PROTOCOL_STATUS_PAYLOAD_SIZE 13

#define DISPLAY_CMD_PING 0x01
#define DISPLAY_CMD_GET_INFO 0x02
#define DISPLAY_CMD_SET_BRIGHTNESS 0x10
#define DISPLAY_CMD_SET_ANIMATION 0x20
#define DISPLAY_CMD_SET_EXPRESSION 0x21
#define DISPLAY_CMD_SYNC 0x22
#define DISPLAY_CMD_STOP 0x23
#define DISPLAY_CMD_SET_PARAM 0x30
#define DISPLAY_CMD_DEBUG_FRAME 0x7E
#define DISPLAY_CMD_RESET 0x7F

#define DISPLAY_ANIM_IDLE 0x00
#define DISPLAY_ANIM_LISTENING 0x01
#define DISPLAY_ANIM_THINKING 0x02
#define DISPLAY_ANIM_THINKING_AUDIO 0x03
#define DISPLAY_ANIM_THINKING_LONG 0x04
#define DISPLAY_ANIM_SPEAKING 0x05
#define DISPLAY_ANIM_HAPPY 0x06
#define DISPLAY_ANIM_ANGRY 0x07
#define DISPLAY_ANIM_ERROR 0x08
#define DISPLAY_ANIM_SLEEP 0x09
#define DISPLAY_ANIM_TOOL 0x0A
#define DISPLAY_ANIM_LEFT 0x0B
#define DISPLAY_ANIM_RIGHT 0x0C
#define DISPLAY_ANIM_UP 0x0D
#define DISPLAY_ANIM_DOWN 0x0E
#define DISPLAY_ANIM_CENTER 0x0F
#define DISPLAY_ANIM_NEUTRAL 0x10
#define DISPLAY_ANIM_SARCASTIC 0x11
#define DISPLAY_ANIM_SUSPICIOUS 0x12
#define DISPLAY_ANIM_TIRED 0x13
#define DISPLAY_ANIM_SURPRISED 0x14
#define DISPLAY_ANIM_BORED 0x15
#define DISPLAY_ANIM_DRAMATIC 0x16
#define DISPLAY_ANIM_WATCH 0x17
#define DISPLAY_ANIM_PARTY 0x18
#define DISPLAY_ANIM_BATTERY_LOW 0x19
#define DISPLAY_ANIM_SUNNY 0x1A
#define DISPLAY_ANIM_RAINY 0x1B
#define DISPLAY_ANIM_CLOUDY 0x1C
#define DISPLAY_ANIM_STORMY 0x1D
#define DISPLAY_ANIM_SNOWY 0x1E
#define DISPLAY_ANIM_COUNT 0x1F

#define DISPLAY_PARAM_MOUTH_INTENSITY 0x01
#define DISPLAY_ERROR_NONE 0x00
#define DISPLAY_TRANSITION_DEFAULT_TICKS 0
#define DISPLAY_TRANSITION_TICK_MS 40

typedef struct {
    uint8_t type;
    uint8_t role;
    uint8_t sequence;
    uint8_t command;
    const uint8_t *payload;
    uint8_t payload_length;
} display_protocol_packet_t;

static inline uint8_t display_protocol_crc8(const uint8_t *data, size_t length)
{
    uint8_t crc = 0;
    for (size_t i = 0; i < length; ++i) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x07)
                               : (uint8_t)(crc << 1);
        }
    }
    return crc;
}

static inline size_t display_protocol_encode(
    uint8_t *output, size_t capacity, uint8_t type, uint8_t role,
    uint8_t sequence, uint8_t command, const uint8_t *payload,
    uint8_t payload_length)
{
    size_t packet_length = DISPLAY_PROTOCOL_HEADER_SIZE + payload_length + 1;
    if (output == NULL || payload_length > DISPLAY_PROTOCOL_MAX_PAYLOAD_SIZE ||
        capacity < packet_length || (payload_length > 0 && payload == NULL) ||
        (type != DISPLAY_PROTOCOL_TYPE_COMMAND &&
         type != DISPLAY_PROTOCOL_TYPE_STATUS) ||
        (role != DISPLAY_PROTOCOL_ROLE_MOUTH &&
         role != DISPLAY_PROTOCOL_ROLE_ANY)) {
        return 0;
    }

    output[0] = DISPLAY_PROTOCOL_MAGIC_0;
    output[1] = DISPLAY_PROTOCOL_MAGIC_1;
    output[2] = DISPLAY_PROTOCOL_TRANSPORT_VERSION;
    output[3] = type;
    output[4] = role;
    output[5] = sequence;
    output[6] = command;
    output[7] = payload_length;
    for (uint8_t i = 0; i < payload_length; ++i) {
        output[DISPLAY_PROTOCOL_HEADER_SIZE + i] = payload[i];
    }
    output[packet_length - 1] =
        display_protocol_crc8(output, packet_length - 1);
    return packet_length;
}

static inline bool display_protocol_decode(
    const uint8_t *data, size_t length, uint8_t expected_type,
    uint8_t expected_role, display_protocol_packet_t *packet)
{
    if (data == NULL || packet == NULL ||
        length < DISPLAY_PROTOCOL_HEADER_SIZE + 1 ||
        data[0] != DISPLAY_PROTOCOL_MAGIC_0 ||
        data[1] != DISPLAY_PROTOCOL_MAGIC_1 ||
        data[2] != DISPLAY_PROTOCOL_TRANSPORT_VERSION ||
        data[3] != expected_type || data[4] != expected_role ||
        data[7] > DISPLAY_PROTOCOL_MAX_PAYLOAD_SIZE ||
        length != (size_t)DISPLAY_PROTOCOL_HEADER_SIZE + data[7] + 1 ||
        display_protocol_crc8(data, length - 1) != data[length - 1]) {
        return false;
    }

    packet->type = data[3];
    packet->role = data[4];
    packet->sequence = data[5];
    packet->command = data[6];
    packet->payload = &data[DISPLAY_PROTOCOL_HEADER_SIZE];
    packet->payload_length = data[7];
    return true;
}

#ifdef __cplusplus
}
#endif
