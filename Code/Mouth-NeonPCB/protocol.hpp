#pragma once

#include <cstddef>
#include <cstdint>

namespace mouth_protocol {

constexpr uint8_t kMagic0 = 'S';
constexpr uint8_t kMagic1 = 'M';
constexpr uint8_t kTransportVersion = 0x01;
constexpr uint8_t kApplicationVersion = 0x01;
constexpr uint8_t kRoleMouth = 0x02;
constexpr uint8_t kRoleAny = 0xff;

constexpr uint8_t kTypeCommand = 0x01;
constexpr uint8_t kTypeStatus = 0x02;

constexpr uint8_t kCmdPing = 0x01;
constexpr uint8_t kCmdGetInfo = 0x02;
constexpr uint8_t kCmdSetBrightness = 0x10;
constexpr uint8_t kCmdSetAnimation = 0x20;
constexpr uint8_t kCmdSetExpression = 0x21;
constexpr uint8_t kCmdSync = 0x22;
constexpr uint8_t kCmdStop = 0x23;
constexpr uint8_t kCmdSetParam = 0x30;
constexpr uint8_t kCmdDebugFrame = 0x7e;
constexpr uint8_t kCmdReset = 0x7f;

constexpr uint8_t kAnimIdle = 0x00;
constexpr uint8_t kAnimListening = 0x01;
constexpr uint8_t kAnimThinking = 0x02;
constexpr uint8_t kAnimThinkingAudio = 0x03;
constexpr uint8_t kAnimThinkingLong = 0x04;
constexpr uint8_t kAnimSpeaking = 0x05;
constexpr uint8_t kAnimHappy = 0x06;
constexpr uint8_t kAnimAngry = 0x07;
constexpr uint8_t kAnimError = 0x08;
constexpr uint8_t kAnimSleep = 0x09;

constexpr uint8_t kParamMouthIntensity = 0x01;

constexpr uint8_t kErrorNone = 0x00;
constexpr uint8_t kErrorMalformed = 0x01;
constexpr uint8_t kErrorUnknownCommand = 0x02;
constexpr uint8_t kErrorInvalidPayload = 0x03;

constexpr size_t kHeaderSize = 8;
constexpr size_t kMaxPayloadSize = 64;
constexpr size_t kMaxPacketSize = kHeaderSize + kMaxPayloadSize + 1;
constexpr size_t kStatusPayloadSize = 10;

struct PacketView {
    uint8_t type;
    uint8_t role;
    uint8_t sequence;
    uint8_t command;
    const uint8_t *payload;
    uint8_t payloadLength;
};

inline uint8_t crc8(const uint8_t *data, size_t length) {
    uint8_t crc = 0;
    for (size_t i = 0; i < length; ++i) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc & 0x80) ? static_cast<uint8_t>((crc << 1) ^ 0x07)
                               : static_cast<uint8_t>(crc << 1);
        }
    }
    return crc;
}

inline size_t encodePacket(uint8_t *output, size_t capacity, uint8_t type,
                           uint8_t role, uint8_t sequence, uint8_t command,
                           const uint8_t *payload, uint8_t payloadLength) {
    const size_t packetLength = kHeaderSize + payloadLength + 1;
    if (output == nullptr || payloadLength > kMaxPayloadSize ||
        capacity < packetLength || (payloadLength != 0 && payload == nullptr)) {
        return 0;
    }

    output[0] = kMagic0;
    output[1] = kMagic1;
    output[2] = kTransportVersion;
    output[3] = type;
    output[4] = role;
    output[5] = sequence;
    output[6] = command;
    output[7] = payloadLength;
    for (uint8_t i = 0; i < payloadLength; ++i) {
        output[kHeaderSize + i] = payload[i];
    }
    output[packetLength - 1] = crc8(output, packetLength - 1);
    return packetLength;
}

inline bool decodePacket(const uint8_t *data, size_t length, PacketView &packet) {
    if (data == nullptr || length < kHeaderSize + 1 ||
        data[0] != kMagic0 || data[1] != kMagic1 ||
        data[2] != kTransportVersion || data[7] > kMaxPayloadSize ||
        length != kHeaderSize + data[7] + 1 ||
        crc8(data, length - 1) != data[length - 1]) {
        return false;
    }

    packet.type = data[3];
    packet.role = data[4];
    packet.sequence = data[5];
    packet.command = data[6];
    packet.payload = &data[kHeaderSize];
    packet.payloadLength = data[7];
    return true;
}

}  // namespace mouth_protocol
