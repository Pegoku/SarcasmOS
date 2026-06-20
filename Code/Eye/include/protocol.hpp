#pragma once

#include <cstdint>

constexpr uint8_t kProtocolVersion = 0x01;
constexpr uint8_t kCmdPing = 0x01;
constexpr uint8_t kCmdGetInfo = 0x02;
constexpr uint8_t kCmdSetBrightness = 0x10;
constexpr uint8_t kCmdSetAnimation = 0x20;
constexpr uint8_t kCmdSetExpression = 0x21;
constexpr uint8_t kCmdSync = 0x22;
constexpr uint8_t kCmdStop = 0x23;
constexpr uint8_t kCmdSetParam = 0x30;
constexpr uint8_t kCmdDebugFrame = 0x7E;
constexpr uint8_t kCmdReset = 0x7F;

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

constexpr uint8_t kRoleLeftEye = 0;
constexpr uint8_t kRoleRightEye = 1;

inline uint8_t crc8(const uint8_t *data, uint8_t len) {
    uint8_t crc = 0;
    for (uint8_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc & 0x80) ? static_cast<uint8_t>((crc << 1) ^ 0x07) : static_cast<uint8_t>(crc << 1);
        }
    }
    return crc;
}
