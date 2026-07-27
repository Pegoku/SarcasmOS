#include <assert.h>
#include <string.h>

#include "display_protocol.h"

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
    return 0;
}
