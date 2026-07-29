#pragma once

#include <cstdint>

constexpr uint8_t kProtocolVersion = 0x02;
constexpr uint8_t kEyeStatusSize = 13;
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
constexpr uint8_t kAnimHappyFake = 0x06;
constexpr uint8_t kAnimHappy = kAnimHappyFake;
constexpr uint8_t kAnimAngry = 0x07;
constexpr uint8_t kAnimError = 0x08;
constexpr uint8_t kAnimAsleep = 0x09;
constexpr uint8_t kAnimSleep = kAnimAsleep;
constexpr uint8_t kAnimTool = 0x0A;
constexpr uint8_t kAnimLeft = 0x0B;
constexpr uint8_t kAnimRight = 0x0C;
constexpr uint8_t kAnimUp = 0x0D;
constexpr uint8_t kAnimDown = 0x0E;
constexpr uint8_t kAnimCenter = 0x0F;
constexpr uint8_t kAnimNeutral = 0x10;
constexpr uint8_t kAnimSarcastic = 0x11;
constexpr uint8_t kAnimSuspicious = 0x12;
constexpr uint8_t kAnimTired = 0x13;
constexpr uint8_t kAnimSurprised = 0x14;
constexpr uint8_t kAnimBored = 0x15;
constexpr uint8_t kAnimDramatic = 0x16;
constexpr uint8_t kAnimWatch = 0x17;
constexpr uint8_t kAnimParty = 0x18;
constexpr uint8_t kAnimBatteryLow = 0x19;
constexpr uint8_t kAnimSunny = 0x1A;
constexpr uint8_t kAnimRainy = 0x1B;
constexpr uint8_t kAnimCloudy = 0x1C;
constexpr uint8_t kAnimStormy = 0x1D;
constexpr uint8_t kAnimSnowy = 0x1E;
constexpr uint8_t kAnimCount = 0x1F;

inline bool isValidAnimation(uint8_t animation) {
    return animation < kAnimCount;
}

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
